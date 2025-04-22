// src/wrapper.rs (or wherever your Recorder struct is defined)

// ---- IMPORTS ----
use crate::communication::ObsClient; // Assumes ObsClient is now the synchronous version
use crate::errors::ObsError;
use crate::types::{
    AudioDeviceSettings, AudioSettings, ErrorEventPayload, EventNotification, FileOutputSettings, GameSourceSettings, QueryMachineInfoEventPayload, RecorderType, ReplaySettings, SceneSettings, StartCommandPayload, StartReplayCaptureCommandPayload, StopCommandPayload, VideoEncoderSettings, VideoSettings, CMD_QUERY_MACHINE_INFO, CMD_SHUTDOWN, CMD_START, CMD_START_REPLAY_CAPTURE, CMD_STOP, EVT_ERR, EVT_QUERY_MACHINE_INFO, VIDEO_ENCODER_ID_NVENC_NEW // Assuming types remain mostly the same
};
use crate::RecordingConfig;
use log::{debug, error, info, warn};
use serde_json::json;
// --- Use standard library equivalents ---
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicI32, Ordering};
use std::sync::mpsc::{RecvTimeoutError, Receiver}; // For query_machine_info
use std::sync::Mutex; // Use std::sync::Mutex
use std::thread::{self, JoinHandle}; // Use std::thread
use std::time::Duration; // For timeouts

// Counter for generating simple identifiers when needed (remains the same)
static NEXT_IDENTIFIER: AtomicI32 = AtomicI32::new(1);

/// Generates a unique identifier for commands/requests. (remains the same)
fn generate_identifier() -> i32 {
    NEXT_IDENTIFIER.fetch_add(1, Ordering::Relaxed)
}

/// A high-level client for controlling an ascent-obs recording process (Synchronous Version).
///
/// WARNING: Methods on this client (new, start*, save*, stop*, shutdown) are blocking.
/// Ensure this client is managed appropriately, potentially in its own thread,
/// to avoid blocking critical parts of your application (like a UI thread).
#[derive(Debug)]
pub struct Recorder {
    client: ObsClient, // Now the synchronous client
    config: RecordingConfig,
    // --- Use std::sync::Mutex ---
    active_recording_id: Mutex<Option<i32>>,
    active_replay_buffer_id: Mutex<Option<i32>>,
    game_pid: i32,
    // --- Use std::thread::JoinHandle ---
    _event_drain_task: JoinHandle<()>,
}

impl Recorder {
    /// Creates a new recorder instance by starting the ascent-obs process (Synchronous).
    ///
    /// This function blocks until the process is started and threads are spawned.
    ///
    /// # Arguments
    /// * `ascent_obs_path` - Path to the ascent-obs.exe executable.
    /// * `config` - Recording configuration.
    /// * `game_pid` - Process ID of the game to capture.
    /// * `buffer_size` - Optional size of the internal command/event channels. Default is 128.
    pub fn new(
        ascent_obs_path: impl Into<PathBuf>,
        config: RecordingConfig,
        game_pid: i32,
        buffer_size: Option<usize>,
    ) -> Result<Self, ObsError> {
        let path = ascent_obs_path.into();
        info!("(Sync) Starting ascent-obs process from: {:?}", path);

        let buffer_size = buffer_size.unwrap_or(128);
        // --- Call the synchronous ObsClient::start ---
        let (client, event_receiver) = ObsClient::start(path, None, buffer_size)?;

        // --- Spawn a standard OS thread to drain the event channel ---
        let drain_task = thread::spawn(move || {
            info!("(Sync) Event drain thread started.");
            // event_receiver is now std::sync::mpsc::Receiver
            // Loop while the channel is open. recv() blocks.
            while let Ok(event_result) = event_receiver.recv() {
                // Optionally log events
                match &event_result {
                    Ok(notification) => info!("Received event: {:?}", notification), // Use trace to avoid spamming logs
                    Err(e) => warn!("(Sync) Drain Thread Received error event: {:?}", e),
                }
                // Just drop the event
            }
            // recv() returned Err, meaning the channel is closed (likely during shutdown)
            info!("(Sync) Event drain thread finished (channel closed).");
        });

        Ok(Self {
            client,
            config,
            // --- Initialize std::sync::Mutex ---
            active_recording_id: Mutex::new(None),
            active_replay_buffer_id: Mutex::new(None),
            game_pid,
            _event_drain_task: drain_task, // Store the standard JoinHandle
        })
    }

    /// Starts a video recording session with the given configuration (Synchronous).
    /// Blocks while sending the command.
    ///
    /// Returns a unique identifier for this recording session.
    pub fn start_recording(&self) -> Result<i32, ObsError> {
        // --- Use std::sync::Mutex::lock ---
        // Use unwrap() for simplicity, assuming PoisonError is fatal. Handle if needed.
        let mut active_id_guard = self.active_recording_id.lock().map_err(|_| ObsError::InternalError("Mutex poisoned".to_string()))?;

        if active_id_guard.is_some() {
            error!(
                "(Sync) Start recording failed: another recording (id: {:?}) is already active.",
                active_id_guard
            );
            return Err(ObsError::AlreadyRecording);
        }

        let identifier = generate_identifier();
        let start_payload = self.create_start_payload(RecorderType::Video, true, self.game_pid); // create_start_payload is already sync

        info!(
            "(Sync) Sending START command (id: {}, type: {:?}, path: {:?}, pid: {})",
            identifier,
            start_payload.recorder_type,
            self.config.output_file,
            self.game_pid
        );

        // Send command synchronously (blocks)
        self.client
            .send_command(CMD_START, Some(identifier), start_payload)?;

        // Store the ID *after* successfully sending the command
        *active_id_guard = Some(identifier);

        // Start the replay buffer if enabled (will block)
        if self.config.replay_buffer_seconds.is_some() {
            // Call the now synchronous version
            self.start_replay_buffer()?;
        }

        Ok(identifier)
        // Mutex guard is dropped here, releasing the lock
    }

    /// Starts the replay buffer if enabled in configuration (Synchronous).
    /// Blocks while sending the command.
    fn start_replay_buffer(&self) -> Result<i32, ObsError> {
        let mut active_replay_id_guard = self.active_replay_buffer_id.lock().map_err(|_| ObsError::InternalError("Mutex poisoned".to_string()))?;

        if active_replay_id_guard.is_some() {
            warn!("(Sync) Replay buffer is already active.");
            // Return the existing ID
            return Ok(*active_replay_id_guard.as_ref().unwrap());
        }

        if self.config.replay_buffer_seconds.is_none() {
            error!("(Sync) Cannot start replay buffer: missing replay buffer configuration");
            return Err(ObsError::ShouldNotHappen("Missing replay buffer configuration".to_string()));
        }

        let identifier = generate_identifier();
        let replay_start_payload = self.create_start_payload(RecorderType::Replay, false, self.game_pid);

        info!(
            "(Sync) Sending START command for replay buffer (id: {}, type: {:?}, buffer length: {} seconds)",
            identifier,
            replay_start_payload.recorder_type,
            self.config.replay_buffer_seconds.unwrap()
        );

        // Send command synchronously
        self.client
            .send_command(CMD_START, Some(identifier), replay_start_payload)?;

        *active_replay_id_guard = Some(identifier);
        Ok(identifier)
    }

    // create_start_payload was already synchronous, no changes needed here
    fn create_start_payload(&self, recorder_type: RecorderType, include_file_output: bool, game_pid: i32) -> StartCommandPayload {
         let config = &self.config;

        // Create encoder settings
        let mut encoder_settings = HashMap::new();
        encoder_settings.insert("bitrate".to_string(), json!(config.bitrate));

        // Create video settings
        let video_settings = VideoSettings {
            video_encoder: VideoEncoderSettings {
                encoder_id: config.encoder_id.clone(),
                settings: encoder_settings,
                ..Default::default()
            },
            game_cursor: Some(config.show_cursor),
            fps: Some(config.fps),
            base_width: Some(config.resolution.0),
            base_height: Some(config.resolution.1),
            output_width: Some(config.resolution.0),
            output_height: Some(config.resolution.1),
            ..Default::default()
        };

        // Create audio settings
        let audio_settings = AudioSettings {
            sample_rate: Some(config.sample_rate),
            output_device: Some(AudioDeviceSettings { device_id: Some("default".to_string()), ..Default::default() }),
            input_device: Some(AudioDeviceSettings { device_id: Some("default".to_string()), ..Default::default() }),
            ..Default::default()
        };

        // Create scene settings
        let sources = SceneSettings {
            game: Some(GameSourceSettings {
                process_id: game_pid,
                foreground: Some(true),
                allow_transparency: Some(false),
                ..Default::default()
            }),
            ..Default::default()
        };

        // Create replay settings if configured
        let replay_settings = if config.replay_buffer_seconds.is_some() {
            Some(ReplaySettings {
                max_time_sec: config.replay_buffer_seconds,
                ..Default::default()
            })
        } else {
            None
        };

        // Create file output settings if requested
        let file_output = if include_file_output {
            Some(FileOutputSettings {
                filename: Some(config.output_file.to_string_lossy().into_owned()),
                ..Default::default()
            })
        } else {
            None
        };

        // Construct and return the payload
        StartCommandPayload {
            recorder_type,
            video_settings: Some(video_settings),
            audio_settings: Some(audio_settings),
            sources: Some(sources),
            file_output,
            replay: replay_settings,
            ..Default::default()
        }
    }

    /// Saves the current replay buffer to the specified file (Synchronous).
    /// Blocks while sending commands and potentially restarting the buffer.
    pub fn save_replay_buffer(&self, output_path: &str) -> Result<(), ObsError> {
        let (current_replay_id, _path, _buffer_duration) = { // Scope for the lock
            let mut active_replay_id_guard = self.active_replay_buffer_id.lock().map_err(|_| ObsError::InternalError("Mutex poisoned".to_string()))?;

            if active_replay_id_guard.is_none() {
                error!("(Sync) Cannot save replay buffer: replay buffer is not active");
                return Err(ObsError::NotRecording); // Or a more specific error
            }

            let id = *active_replay_id_guard.as_ref().unwrap();

            // Use the provided path directly
            let path = output_path.to_string();

            let duration_ms = self.config.replay_buffer_seconds
                .ok_or_else(|| {
                    error!("(Sync) Cannot save replay buffer: no buffer duration configured");
                    ObsError::ShouldNotHappen("No replay buffer duration configured".to_string())
                })? * 1000;

            // Step 1: Send command to save the replay buffer contents (synchronous)
            let save_id = generate_identifier();
            info!(
                "(Sync) Sending SAVE_REPLAY_BUFFER command (id: {}, duration: {}ms, path: {:?})",
                save_id, duration_ms, path
            );
            let save_payload = StartReplayCaptureCommandPayload {
                head_duration: duration_ms as i64,
                path: path.clone(),
                thumbnail_folder: None,
            };
            self.client
                .send_command(CMD_START_REPLAY_CAPTURE, Some(save_id), save_payload)?;

            // Step 2: Send STOP command for the *current* replay buffer (synchronous)
            info!(
                "(Sync) Sending STOP command for replay buffer (id: {}, type: {:?})",
                id, RecorderType::Replay
            );
            let stop_payload = StopCommandPayload {
                recorder_type: RecorderType::Replay,
            };
            self.client
                .send_command(CMD_STOP, Some(id), stop_payload)?;

            // Clear the active ID *after* successfully sending stop command
            *active_replay_id_guard = None;

            // Return values needed for step 3 outside the lock
            (id, path, duration_ms)
        }; // --- Lock Released Here ---

        // Step 3: Start a new replay buffer (synchronous)
        info!("(Sync) Attempting to start new replay buffer (after stopping id: {})...", current_replay_id);
        match self.start_replay_buffer() { // This call is now synchronous
            Ok(new_replay_id) => {
                info!("(Sync) Successfully started new replay buffer with id: {}", new_replay_id);
                Ok(())
            }
            Err(e) => {
                error!("(Sync) Failed to start new replay buffer: {}", e);
                // Consider if you need error recovery here.
                // Should we try again? Is the recorder in a bad state?
                Err(e)
            }
        }
    }

    /// Stops the active recording and replay buffer (Synchronous).
    /// Blocks while sending commands.
    pub fn stop_recording(&self) -> Result<(), ObsError> {
        let mut stop_error: Option<ObsError> = None; // Keep track of the first error

         // --- Lock replay buffer ID first ---
        let mut active_replay_id_guard = self.active_replay_buffer_id.lock().map_err(|_| ObsError::InternalError("Mutex poisoned".to_string()))?;
        if let Some(replay_identifier) = *active_replay_id_guard {
            info!(
                "(Sync) Sending STOP command for replay buffer (id: {}, type: {:?})",
                replay_identifier, RecorderType::Replay
            );
            let replay_stop_payload = StopCommandPayload { recorder_type: RecorderType::Replay };

            match self.client.send_command(CMD_STOP, Some(replay_identifier), replay_stop_payload) {
                Ok(_) => info!("(Sync) Replay buffer stop command sent successfully for id: {}", replay_identifier),
                Err(e) => {
                    error!("(Sync) Failed to send STOP command for replay buffer id {}: {}", replay_identifier, e);
                    stop_error = Some(e); // Store the error, but continue
                }
            }
            // Clear the ID regardless of command success, as we intended to stop it.
            *active_replay_id_guard = None;
        }
        drop(active_replay_id_guard); // Release replay lock before taking recording lock


        // --- Lock recording ID ---
        let mut active_id_guard = self.active_recording_id.lock().map_err(|_| ObsError::InternalError("Mutex poisoned".to_string()))?;
        if let Some(identifier) = *active_id_guard {
            info!(
                "(Sync) Sending STOP command for main recording (id: {}, type: {:?})",
                identifier, RecorderType::Video
            );
            let payload = StopCommandPayload { recorder_type: RecorderType::Video };

            match self.client.send_command(CMD_STOP, Some(identifier), payload) {
                Ok(_) => info!("(Sync) Recording stop command sent successfully for id: {}", identifier),
                Err(e) => {
                    error!("(Sync) Failed to send STOP command for main recording id {}: {}", identifier, e);
                    // Prioritize this error if we didn't have one from the replay buffer
                    if stop_error.is_none() {
                        stop_error = Some(e);
                    }
                }
            }
            // Clear the ID regardless of command success.
            *active_id_guard = None;

            // Return the stored error, if any
            if let Some(err) = stop_error {
                Err(err)
            } else {
                Ok(())
            }
        } else {
             warn!("(Sync) Stop recording called, but no main recording is active.");
             // If there was an error stopping the replay buffer, return that.
             // Otherwise, return NotRecording error.
             stop_error.map_or(Err(ObsError::NotRecording), Err)
        }
        // Locks are released when guards go out of scope
    }


    /// Shuts down the ascent-obs process and associated communication (Synchronous).
    /// Consumes the `Recorder` instance. Blocks until shutdown is complete.
    pub fn shutdown(self) -> Result<(), ObsError> {
        info!("(Sync) Shutting down Recorder...");

        // Send shutdown command (synchronous)
        info!("(Sync) Sending SHUTDOWN command to ascent-obs process");
        match self.client.send_simple_command(CMD_SHUTDOWN, None) {
            Ok(_) => info!("(Sync) Successfully sent shutdown command to ascent-obs process"),
            Err(e) => warn!("(Sync) Failed to send shutdown command to ascent-obs process: {}", e),
            // Continue shutdown even if sending command fails
        }

        // Shutdown the client (synchronous, waits for process exit and threads)
        let client_shutdown_result = self.client.shutdown(); // This now blocks

        // Note: The event drain thread should stop automatically when the
        // ObsClient::shutdown() call closes the underlying stdout pipe, which
        // causes the mpsc Receiver to disconnect, ending the recv() loop.
        // ObsClient::shutdown already waits for its threads.
        // We don't explicitly join _event_drain_task here as ObsClient handles cleanup.

        client_shutdown_result // Return the result from ObsClient::shutdown
    }
}

/// Queries machine information (encoders, audio devices) without creating a Recorder (Synchronous).
/// Blocks while starting/stopping the temporary client and waiting for the response.
///
/// # Arguments
/// * `ascent_obs_path` - Path to the ascent-obs.exe executable.
///
/// # Returns
/// The machine information payload or an error.
pub fn query_machine_info(
    ascent_obs_path: impl Into<PathBuf>,
) -> Result<QueryMachineInfoEventPayload, ObsError> {
    let path = ascent_obs_path.into();
    info!("(Sync) Querying machine info from: {:?}", path);

    // Start a temporary synchronous client
    let (client, event_receiver) = ObsClient::start(path, None, 32)?; // Synchronous start

    // Send the query command (synchronous)
    let identifier = generate_identifier();
    info!("(Sync) Sending QUERY_MACHINE_INFO command (id: {})", identifier);
    if let Err(e) = client.send_simple_command(CMD_QUERY_MACHINE_INFO, Some(identifier)) {
        error!("(Sync) Failed to send query command: {}", e);
        // Attempt shutdown even if send failed
        let _ = client.shutdown();
        return Err(e);
    }

    // Wait for the response with a timeout using mpsc::recv_timeout
    let timeout_duration = Duration::from_secs(5);
    let mut response_payload: Option<QueryMachineInfoEventPayload> = None;
    let mut response_error: Option<ObsError> = None;

    info!("(Sync) Waiting for response (max {} seconds)...", timeout_duration.as_secs());
    loop {
        match event_receiver.recv_timeout(timeout_duration) {
            Ok(Ok(notification)) => { // Successfully received an EventNotification
                debug!("(Sync) Query received event type: {}, identifier: {:?}", notification.event, notification.identifier);
                // TODO: Confirm if ascent-obs *does* return identifier for this event now.
                // Assuming it *might* not, we primarily check the event type.
                 if notification.event == EVT_QUERY_MACHINE_INFO {
                     match notification.deserialize_payload::<QueryMachineInfoEventPayload>() {
                         Ok(Some(payload)) => {
                             info!("(Sync) Received valid QUERY_MACHINE_INFO response.");
                             response_payload = Some(payload);
                             break; // Got our response
                         }
                         Ok(None) => {
                             warn!("(Sync) Received QUERY_MACHINE_INFO event with null/empty payload.");
                             // Continue waiting or treat as error? Let's treat as error.
                              response_error = Some(ObsError::Deserialization("Query response payload was empty".into()));
                              break;
                         }
                         Err(e) => {
                             error!("(Sync) Failed to deserialize QUERY_MACHINE_INFO payload: {}", e);
                             response_error = Some(ObsError::Deserialization(format!("Failed to deserialize query response: {}", e)));
                             break; // Deserialization error
                         }
                     }
                 } else if notification.event == EVT_ERR && notification.identifier == Some(identifier) {
                     // Received a specific error response for our command ID
                     match notification.deserialize_payload::<ErrorEventPayload>() {
                         Ok(Some(error_payload)) => {
                             error!("(Sync) Received error response for query (Code {}): {:?}", error_payload.code, error_payload.desc);
                             response_error = Some(ObsError::PipeError(format!(
                                 "Error response: Code {}, Description: {:?}",
                                 error_payload.code,
                                 error_payload.desc
                             )));
                         }
                         _ => {
                              error!("(Sync) Received error response with invalid payload for query.");
                              response_error = Some(ObsError::PipeError("Error response with invalid payload".into()));
                         }
                     }
                      break; // Got an error response
                 }
                 // Ignore other events
            }
            Ok(Err(e)) => { // Received an ObsError from the reader thread
                 error!("(Sync) Query received error from event channel: {}", e);
                 response_error = Some(e);
                 break; // Error from the underlying communication
            }
            Err(RecvTimeoutError::Timeout) => {
                error!("(Sync) Timed out waiting for query response after {} seconds.", timeout_duration.as_secs());
                response_error = Some(ObsError::PipeError("Timed out waiting for query response".into()));
                break; // Timeout
            }
            Err(RecvTimeoutError::Disconnected) => {
                 error!("(Sync) Event channel disconnected while waiting for query response.");
                 response_error = Some(ObsError::PipeError("Event channel closed before receiving response".into()));
                 break; // Channel closed prematurely
            }
        }
    }

    // Shutdown the client (synchronous)
    info!("(Sync) Shutting down temporary client used for query...");
    if let Err(e) = client.shutdown() {
         warn!("(Sync) Error during temporary client shutdown: {}", e);
         // Prioritize the response error over the shutdown error, unless no response error occurred.
         if response_error.is_none() {
             response_error = Some(e);
         }
    }

    // Return the result
    match (response_payload, response_error) {
        (Some(payload), None) => Ok(payload),
        (_, Some(err)) => Err(err),
        (None, None) => Err(ObsError::InternalError("Query finished without payload or error".into())), // Should not happen given the loop logic
    }
}