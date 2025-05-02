// src/wrapper.rs (or wherever your Recorder struct is defined)

// ---- IMPORTS ----
use crate::communication::ObsClient; // Assumes ObsClient is now the synchronous version
use crate::errors::ObsError;
use crate::types::{
    AudioDeviceSettings, AudioExtraOptions, AudioSettings, ErrorEventPayload, EventNotification, FileOutputSettings, GameSourceSettings, QueryMachineInfoEventPayload, RecorderType, ReplaySettings, SceneSettings, StartCommandPayload, StartReplayCaptureCommandPayload, StopCommandPayload, VideoEncoderSettings, VideoSettings, CMD_QUERY_MACHINE_INFO, CMD_SHUTDOWN, CMD_START, CMD_START_REPLAY_CAPTURE, CMD_STOP, CMD_STOP_REPLAY_CAPTURE, EVT_ERR, EVT_QUERY_MACHINE_INFO, EVT_RECORDING_STARTED, EVT_RECORDING_STOPPED, EVT_REPLAY_CAPTURE_VIDEO_STARTED, EVT_REPLAY_STARTED, VIDEO_ENCODER_ID_NVENC_NEW // Assuming types remain mostly the same
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
pub fn generate_identifier() -> i32 {
    NEXT_IDENTIFIER.fetch_add(1, Ordering::Relaxed)
}

/// A high-level client for controlling an ascent-obs recording process (Synchronous Version).
///
/// WARNING: Methods on this client (new, start*, save*, stop*, shutdown) are blocking.
/// Ensure this client is managed appropriately, potentially in its own thread,
/// to avoid blocking critical parts of your application (like a UI thread).
pub struct Recorder {
    client: ObsClient, // Now the synchronous client
    config: RecordingConfig,
    // --- Use std::sync::Mutex ---
    active_recording_id: Mutex<Option<i32>>,
    active_replay_buffer_id: Mutex<Option<i32>>,
    game_pid: i32,
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
        let client = ObsClient::start(path, None, buffer_size)?;

        Ok(Self {
            client,
            config,
            // --- Initialize std::sync::Mutex ---
            active_recording_id: Mutex::new(None),
            active_replay_buffer_id: Mutex::new(None),
            game_pid,
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

        fn deserialize_start_response(
            event: &EventNotification,
        ) -> Result<Option<()>, ObsError> {
            // If the event is an error type, extract the error code and return a meaningful error
            if event.event == EVT_ERR {
                if let Some(payload) = &event.payload {
                    // Try to extract error code as a number
                    if let Some(code) = payload.get("code").and_then(|v| v.as_i64()) {
                        return Err(ObsError::ProcessStart(format!(
                            "Recording failed to start with error code: {}", 
                            code
                        )));
                    }
                }
                // Generic error if we can't extract the payload
                return Err(ObsError::ProcessStart("Recording failed to start".to_string()));
            }
            
            // For non-error events, return success
            Ok(Some(()))
        }

        // Send command synchronously (blocks)
        self.client
        .send_command_and_wait(
            CMD_START,
            identifier,
            start_payload,
            Duration::from_secs(20), // Adjust timeout as needed
            Some(EVT_RECORDING_STARTED), // No specific expected event type
            vec![EVT_ERR], // Blacklist the error event type
            deserialize_start_response,
        )?;

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
            base_width: Some(config.output_resolution.0),
            base_height: Some(config.output_resolution.1),
            output_width: Some(config.output_resolution.0),
            output_height: Some(config.output_resolution.1),
            ..Default::default()
        };
    
        let audio_settings = {
            let settings = AudioSettings {
                sample_rate: Some(config.sample_rate),
                output_device: Some(AudioDeviceSettings { 
                    device_id: Some("default".to_string()), 
                    ..Default::default() 
                }),
                // Only include input_device if capture_microphone is true
                input_device: if config.capture_microphone {
                    Some(AudioDeviceSettings { 
                        device_id: Some(match &config.microphone_device {
                            Some(device) => device.clone(),
                            None => "default".to_string()
                        }), 
                        ..Default::default() 
                    })
                } else {
                    None
                },
                ..Default::default()
            };
            
            settings
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
    pub fn save_replay_buffer(&self, output_path: impl Into<PathBuf>) -> Result<(), ObsError> {
        let output_path = output_path.into();

        let (current_replay_id, _path, _buffer_duration) = { // Scope for the lock
            let mut active_replay_id_guard = self.active_replay_buffer_id.lock().map_err(|_| ObsError::InternalError("Mutex poisoned".to_string()))?;
    
            if active_replay_id_guard.is_none() {
                error!("(Sync) Cannot save replay buffer: replay buffer is not active");
                return Err(ObsError::NotRecording); // Or a more specific error
            }
    
            let replay_id = *active_replay_id_guard.as_ref().unwrap();
    
            // Use the provided path
            let path = output_path.to_string_lossy().into_owned();
    
            let duration_ms = self.config.replay_buffer_seconds
                .ok_or_else(|| {
                    error!("(Sync) Cannot save replay buffer: no buffer duration configured");
                    ObsError::ShouldNotHappen("No replay buffer duration configured".to_string())
                })? * 1000;
    
            // Define the deserializer function for the start replay capture response
            fn deserialize_start_replay_response(
                event: &EventNotification,
            ) -> Result<Option<()>, ObsError> {
                if event.event == EVT_REPLAY_CAPTURE_VIDEO_STARTED {
                    Ok(Some(()))
                } else if event.event == EVT_ERR {
                    // Handle error event
                    let error_payload = event.deserialize_payload::<ErrorEventPayload>()
                        .map_err(|e| ObsError::Deserialization(format!("Failed to deserialize error payload: {}", e)))?;
                    if let Some(payload) = error_payload {
                        Err(ObsError::EventManagerError(format!("Replay capture error: {:?}", payload.data)))
                    } else {
                        Err(ObsError::EventManagerError("Unknown replay capture error".to_string()))
                    }
                } else {
                    Ok(None)
                }
            }
    
            // Step 1: Send command to save the replay buffer contents and wait for the response
            // Use the SAME identifier as the active replay buffer
            info!(
                "(Sync) Sending SAVE_REPLAY_BUFFER command (id: {}, duration: {}ms, path: {:?})",
                replay_id, duration_ms, path
            );
            let save_payload = StartReplayCaptureCommandPayload {
                head_duration: duration_ms as i64,
                path: path.clone(),
                thumbnail_folder: None,
            };
            
            // Use send_command_and_wait with the same replay_id
            self.client
                .send_command_and_wait(
                    CMD_START_REPLAY_CAPTURE,
                    replay_id,
                    save_payload,
                    Duration::from_secs(3), // Adjust timeout as needed
                    Some(EVT_REPLAY_CAPTURE_VIDEO_STARTED), // Replace with actual event code
                    vec![],
                    deserialize_start_replay_response,
                )?;
    
            // Step 2: Send STOP command for the *current* replay buffer (synchronous)
            // Again using the SAME identifier
            info!(
                "(Sync) Sending STOP command for replay buffer (id: {}, type: {:?})",
                replay_id, RecorderType::Replay
            );
            let stop_payload = StopCommandPayload {
                recorder_type: RecorderType::Replay,
            };
            self.client
                .send_command(CMD_STOP_REPLAY_CAPTURE, Some(replay_id), stop_payload)?;
    
            // Clear the active ID *after* successfully sending stop command
            *active_replay_id_guard = None;
    
            // Return values needed for step 3 outside the lock
            (replay_id, path, duration_ms)
        }; // --- Lock Released Here ---
        Ok(())
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
    
            // Define the deserializer function for the stop response
            fn deserialize_stop_response(
                event: &EventNotification,
            ) -> Result<Option<()>, ObsError> {
                info!("Deserializing stop response: event type {}, expected {}", 
                       event.event, EVT_RECORDING_STOPPED);
                
                if event.event == EVT_RECORDING_STOPPED {
                    info!("Successfully matched event type for recording stop");
                    Ok(Some(()))
                } else if event.event == EVT_ERR {
                    error!("error");
                    Ok(Some(()))
                } else {
                    info!("Event type {} didn't match expected {}, returning None", 
                           event.event, EVT_RECORDING_STOPPED);
                    Ok(None)
                }
            }
    
            // Send command and wait for response
            match self.client.send_command_and_wait(
                CMD_STOP,
                identifier,
                payload,
                Duration::from_secs(20), // Longer timeout for stopping recording
                Some(EVT_RECORDING_STOPPED),    // Expected event type
                vec![],
                deserialize_stop_response,
            ) {
                Ok(_) => {
                    info!("(Sync) Recording stop confirmed for id: {}", identifier);
                    // Clear the ID after successful stop confirmation
                    *active_id_guard = None;
                    
                    // Return the stored error from replay buffer stop, if any
                    if let Some(err) = stop_error {
                        Err(err)
                    } else {
                        Ok(())
                    }
                },
                Err(e) => {
                    error!("(Sync) Failed to stop main recording id {}: {}", identifier, e);
                    // Clear the ID anyway since we tried to stop it
                    *active_id_guard = None;
                    
                    // Prioritize this error over any replay buffer error
                    Err(e)
                }
            }
        } else {
            warn!("(Sync) Stop recording called, but no main recording is active.");
            // If there was an error stopping the replay buffer, return that.
            // Otherwise, return NotRecording error.
            stop_error.map_or(Err(ObsError::NotRecording), Err)
        }
        // Locks are released when guards go out of scope
    }

    pub fn request_shutdown(&self) -> Result<(), ObsError> {
        info!("(Sync) Requesting shutdown of ascent-obs process...");
    
        // Send shutdown command (synchronous)
        info!("(Sync) Sending SHUTDOWN command to ascent-obs process");
        match self.client.send_simple_command(CMD_SHUTDOWN, None) {
            Ok(_) => {
                info!("(Sync) Successfully sent shutdown command to ascent-obs process");
                Ok(())
            },
            Err(e) => {
                warn!("(Sync) Failed to send shutdown command to ascent-obs process: {}", e);
                Err(e)
            }
        }
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
/// Queries machine information (encoders, audio devices) without creating a Recorder.
/// Uses the new event handling system to wait for the response.
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
    info!("Querying machine info from: {:?}", path);

    // Start a temporary client
    let client = ObsClient::start(path, None, 32)?;
    
    // Create a channel to receive the info payload
    let (sender, receiver) = std::sync::mpsc::channel();
    // Clone sender before moving it into closures
    let main_sender = sender.clone();
    let err_sender = sender;
    
    // Register a callback for the expected event type
    client.register_event_callback(EVT_QUERY_MACHINE_INFO, move |event| {
        if let Ok(Some(payload)) = event.deserialize_payload::<QueryMachineInfoEventPayload>() {
            let _ = main_sender.send(Ok(payload));
        }
    });
    
    // Also register a callback for errors
    client.register_event_callback(EVT_ERR, move |event| {
        if let Ok(Some(error_payload)) = event.deserialize_payload::<ErrorEventPayload>() {
            let _ = err_sender.send(Err(
                ObsError::EventManagerError(format!("Query machine info error: {:?}", error_payload.data))
            ));
        }
    });
    
    // Send the command without waiting for response
    client.send_simple_command(CMD_QUERY_MACHINE_INFO, None)?;
    
    // Wait for response through our channel
    let result = match receiver.recv_timeout(Duration::from_secs(5)) {
        Ok(result) => result,
        Err(RecvTimeoutError::Timeout) => Err(ObsError::Timeout("Query machine info timed out".to_string())),
        Err(RecvTimeoutError::Disconnected) => Err(ObsError::EventManagerError("Channel disconnected".to_string())),
    };
    
    // Shutdown the client
    if let Err(e) = client.shutdown() {
        warn!("Error during temporary client shutdown: {}", e);
    }
    
    result
}