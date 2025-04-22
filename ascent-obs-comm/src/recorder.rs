use crate::communication::ObsClient;
use crate::errors::ObsError;
use crate::types::{
    // Keep necessary types for internal use
    AudioDeviceSettings, AudioSettings, ErrorEventPayload, EventNotification, FileOutputSettings, GameSourceSettings, QueryMachineInfoEventPayload, RecorderType, ReplaySettings, SceneSettings, StartCommandPayload, StartReplayCaptureCommandPayload, StopCommandPayload, VideoEncoderSettings, VideoSettings, CMD_QUERY_MACHINE_INFO, CMD_SHUTDOWN, CMD_START, CMD_START_REPLAY_CAPTURE, CMD_STOP, EVT_ERR, EVT_QUERY_MACHINE_INFO, VIDEO_ENCODER_ID_NVENC_NEW
};
use crate::RecordingConfig;
use log::{debug, error, info, warn};
use serde_json::json;
use tokio::task::JoinHandle;
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicI32, Ordering};

// Counter for generating simple identifiers when needed
static NEXT_IDENTIFIER: AtomicI32 = AtomicI32::new(1);

/// Generates a unique identifier for commands/requests.
fn generate_identifier() -> i32 {
    NEXT_IDENTIFIER.fetch_add(1, Ordering::Relaxed)
}

/// A high-level client for controlling an ascent-obs recording process.
#[derive(Debug)]
pub struct Recorder {
    client: ObsClient,
    config: RecordingConfig,
    active_recording_id: tokio::sync::Mutex<Option<i32>>,
    active_replay_buffer_id: tokio::sync::Mutex<Option<i32>>,
    game_pid: tokio::sync::Mutex<Option<i32>>,
    _event_drain_task: JoinHandle<()>
}

impl Recorder {
    /// Creates a new recorder instance by starting the ascent-obs process.
    ///
    /// # Arguments
    /// * `ascent_obs_path` - Path to the ascent-obs.exe executable.
    /// * `buffer_size` - Optional size of the internal buffer. Default is 128.
    pub async fn new(
        ascent_obs_path: impl Into<PathBuf>,
        config: RecordingConfig,
        buffer_size: Option<usize>,
    ) -> Result<Self, ObsError> {
        let path = ascent_obs_path.into();
        info!("Starting ascent-obs process from: {:?}", path);
        
        // Start the client but discard the event receiver
        let buffer_size = buffer_size.unwrap_or(128);
        let (client, event_receiver) = ObsClient::start(path, None, buffer_size).await?;

        // Spawn a task that just drains the channel
        let drain_task = tokio::spawn(async move {
            let mut receiver = event_receiver;
            while let Some(event) = receiver.recv().await {
                // Optionally log events if you want to see them
                match &event {
                    Ok(notification) => info!("Received event: {:?}", notification),
                    Err(e) => warn!("Received error event: {:?}", e),
                }
                // Just drop the event - we're only draining the channel
            }
            info!("Event drain task finished");
        });
        
        Ok(Self {
            client,
            config,
            active_recording_id: tokio::sync::Mutex::new(None),
            active_replay_buffer_id: tokio::sync::Mutex::new(None),
            game_pid: tokio::sync::Mutex::new(None),
            _event_drain_task: drain_task,
        })
    }

    /// Starts a video recording session with the given configuration.
    ///
    /// Returns a unique identifier for this recording session that can be used
    /// to stop it later.
    ///
    /// # Example
    /// ```ignore
    /// let recording_id = recorder.start_recording(game_pid).await?;
    /// ```
    pub async fn start_recording(&self, game_pid: i32) -> Result<i32, ObsError> {
        let mut active_id_guard = self.active_recording_id.lock().await;
        let mut game_pid_guard = self.game_pid.lock().await;
    
        if active_id_guard.is_some() {
            error!(
                "Start recording failed: another recording (id: {:?}) is already active.",
                active_id_guard
            );
            return Err(ObsError::AlreadyRecording);
        }
    
        // Store the game PID
        *game_pid_guard = Some(game_pid);
        
        let identifier = generate_identifier();
        
        // Create payload with common settings for video recording
        let start_payload = self.create_start_payload(RecorderType::Video, true, game_pid);
    
        info!(
            "Sending START command (id: {}, type: {:?}, path: {:?}, pid: {})",
            identifier,
            start_payload.recorder_type,
            self.config.output_file,
            game_pid
        );
    
        // TODO: we have no idea whether the recording actually started
        *active_id_guard = Some(identifier);
        self.client
            .send_command(CMD_START, Some(identifier), start_payload)
            .await?;
    
        // Start the replay buffer if it's enabled
        if self.config.replay_buffer_seconds.is_some() && self.config.replay_buffer_output_file.is_some() {
            self.start_replay_buffer().await?;  // No need to pass game_pid anymore
        }
    
        Ok(identifier)
    }

    /// Starts the replay buffer if enabled in configuration
    async fn start_replay_buffer(&self) -> Result<i32, ObsError> {
        let mut active_replay_id_guard = self.active_replay_buffer_id.lock().await;
        let game_pid_guard = self.game_pid.lock().await;
        
        // Get the game PID
        let game_pid = match *game_pid_guard {
            Some(pid) => pid,
            None => {
                error!("Cannot start replay buffer: no game PID is set");
                return Err(ObsError::ShouldNotHappen("No game PID is set".to_string()));
            }
        };
        
        if active_replay_id_guard.is_some() {
            warn!("Replay buffer is already active.");
            return Ok(*active_replay_id_guard.as_ref().unwrap());
        }
        
        // Ensure we have the necessary replay buffer configuration
        if self.config.replay_buffer_seconds.is_none() || self.config.replay_buffer_output_file.is_none() {
            error!("Cannot start replay buffer: missing replay buffer configuration");
            return Err(ObsError::ShouldNotHappen("Missing replay buffer configuration".to_string()));
        }
        
        let identifier = generate_identifier();
        
        // Create payload with common settings but specific to replay buffer
        let replay_start_payload = self.create_start_payload(RecorderType::Replay, false, game_pid);
        
        info!(
            "Sending START command for replay buffer (id: {}, type: {:?}, buffer length: {} seconds)",
            identifier,
            replay_start_payload.recorder_type,
            self.config.replay_buffer_seconds.unwrap()
        );
        
        *active_replay_id_guard = Some(identifier);
        self.client
            .send_command(CMD_START, Some(identifier), replay_start_payload)
            .await?;
        
        Ok(identifier)
    }

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
    
    /// Saves the current replay buffer to the configured file
    pub async fn save_replay_buffer(&self) -> Result<(), ObsError> {
        let game_pid_guard = self.game_pid.lock().await;
        
        // Get the game PID
        let game_pid = match *game_pid_guard {
            Some(pid) => pid,
            None => {
                error!("Cannot save replay buffer: no game PID is set");
                return Err(ObsError::ShouldNotHappen("No game PID is set".to_string()));
            }
        };
        
        let current_replay_id = { // Create a smaller scope for the lock guard
            let mut active_replay_id_guard = self.active_replay_buffer_id.lock().await;
    
            if active_replay_id_guard.is_none() {
                error!("Cannot save replay buffer: replay buffer is not active");
                return Err(ObsError::NotRecording);
            }
    
            let id = *active_replay_id_guard.as_ref().unwrap();
    
            // Make sure we have an output path configured
            let output_path = match &self.config.replay_buffer_output_file {
                Some(path) => path.to_string_lossy().into_owned(),
                None => {
                    error!("Cannot save replay buffer: no output file configured");
                    return Err(ObsError::ShouldNotHappen("No replay buffer output file configured".to_string()));
                }
            };
    
            // Get the buffer duration in milliseconds
            let buffer_duration = match self.config.replay_buffer_seconds {
                Some(seconds) => seconds * 1000, // Convert to milliseconds
                None => {
                    error!("Cannot save replay buffer: no buffer duration configured");
                    return Err(ObsError::ShouldNotHappen("No replay buffer duration configured".to_string()));
                }
            };
    
            // Step 1: Send command to save the replay buffer contents
            let save_id = generate_identifier();
            info!(
                "Sending SAVE_REPLAY_BUFFER command (id: {}, duration: {}ms, path: {:?})",
                save_id, buffer_duration, output_path
            );
            let save_payload = StartReplayCaptureCommandPayload {
                head_duration: buffer_duration as i64,
                path: output_path,
                thumbnail_folder: None,
            };
            self.client
                .send_command(CMD_START_REPLAY_CAPTURE, Some(save_id), save_payload)
                .await?;
    
            // Step 2: Stop the current replay buffer
            info!(
                "Sending STOP command for replay buffer (id: {}, type: {:?})",
                id, RecorderType::Replay
            );
            let stop_payload = StopCommandPayload {
                recorder_type: RecorderType::Replay,
            };
            self.client
                .send_command(CMD_STOP, Some(id), stop_payload)
                .await?;
    
            // Clear the active replay buffer ID since we've stopped it
            *active_replay_id_guard = None;
    
            // Return the ID we just stopped
            id
            // The lock guard `active_replay_id_guard` goes out of scope here, releasing the lock
        }; // LOCK RELEASED
    
        // Step 3: Start a new replay buffer with a new identifier
        info!("Attempting to start new replay buffer (after stopping id: {})...", current_replay_id);
        match self.start_replay_buffer().await {  // No need to pass game_pid anymore
            Ok(new_replay_id) => {
                info!("Successfully started new replay buffer with id: {}", new_replay_id);
                Ok(())
            }
            Err(e) => {
                error!("Failed to start new replay buffer: {}", e);
                Err(e)
            }
        }
    }

    /// Stops a specific recording.
    pub async fn stop_recording(&self) -> Result<(), ObsError> {
        let mut active_id_guard = self.active_recording_id.lock().await;
        let mut active_replay_id_guard = self.active_replay_buffer_id.lock().await;
        let mut game_pid_guard = self.game_pid.lock().await;

        // First stop the replay buffer if it's active
        if let Some(replay_identifier) = *active_replay_id_guard {
            info!(
                "Sending STOP command for replay buffer (id: {}, type: {:?})",
                replay_identifier,
                RecorderType::Replay
            );
            
            let replay_stop_payload = StopCommandPayload {
                recorder_type: RecorderType::Replay,
            };

            // Attempt to send the stop command for replay buffer
            match self.client
                .send_command(CMD_STOP, Some(replay_identifier), replay_stop_payload)
                .await {
                Ok(_) => {
                    info!("Replay buffer stop command sent successfully for id: {}", replay_identifier);
                    *active_replay_id_guard = None;
                }
                Err(e) => {
                    error!("Failed to send STOP command for replay buffer id {}: {}", replay_identifier, e);
                    // We continue to stop the main recording even if stopping the replay buffer fails
                    *active_replay_id_guard = None;
                }
            }
        }

        // Then stop the main recording
        if let Some(identifier) = *active_id_guard {
            info!(
                "Sending STOP command (id: {}, type: {:?})",
                identifier,
                RecorderType::Video
            );
            let payload = StopCommandPayload {
                recorder_type: RecorderType::Video,
            };

            // Attempt to send the stop command
            let result = self
                .client
                .send_command(CMD_STOP, Some(identifier), payload)
                .await;

            // Regardless of whether the command succeeded (it might fail if OBS crashed),
            // we clear the state because we *intended* to stop.
            *active_id_guard = None;
            
            // Clear the game PID as well
            *game_pid_guard = None;

            match result {
                Ok(_) => {
                    info!("Recording stop command sent successfully for id: {}", identifier);
                    Ok(())
                }
                Err(e) => {
                    error!("Failed to send STOP command for id {}: {}", identifier, e);
                    // Propagate the error
                    Err(e)
                }
            }
        } else {
            warn!("Stop recording called, but no recording is active.");
            
            // Clear the game PID anyway since we're trying to stop
            *game_pid_guard = None;
            
            // It's debatable whether this should be an error or not.
            // Let's make it an error for clarity.
            Err(ObsError::NotRecording)
        }
        // Locks are released when guards go out of scope
    }

    /// Shuts down the ascent-obs process and associated communication.
    /// Consumes the `Recorder` instance.
    pub async fn shutdown(self) -> Result<(), ObsError> {
        info!("Shutting down Recorder...");
        
        // Send shutdown command immediately
        info!("Sending SHUTDOWN command to ascent-obs process");
        
        // Send the command and check the result, but continue with shutdown regardless
        match self.client.send_simple_command(CMD_SHUTDOWN, None).await {
            Ok(_) => info!("Successfully sent shutdown command to ascent-obs process"),
            Err(e) => warn!("Failed to send shutdown command to ascent-obs process: {}", e),
        }
        
        // Always shutdown the client communication channels and wait for process exit
        self.client.shutdown().await
    }
}

/// Queries machine information (encoders, audio devices) without creating a Recorder.
/// 
/// # Arguments
/// * `ascent_obs_path` - Path to the ascent-obs.exe executable.
///
/// # Returns
/// Returns the command identifier. The caller will need to implement their own
/// way of receiving events if they need the response.
pub async fn query_machine_info(
    ascent_obs_path: impl Into<PathBuf>,
) -> Result<QueryMachineInfoEventPayload, ObsError> {
    let path = ascent_obs_path.into();
    
    // Start a temporary client with a small buffer
    let (client, mut event_receiver) = ObsClient::start(path, None, 32).await?;
    
    // Send the query command
    let identifier = generate_identifier();
    info!("Sending QUERY_MACHINE_INFO command (id: {})", identifier);
    client.send_simple_command(CMD_QUERY_MACHINE_INFO, Some(identifier)).await?;
    
    // Wait for the response with the matching identifier
    let result = tokio::time::timeout(
        tokio::time::Duration::from_secs(5), // Set a reasonable timeout
        async {
            while let Some(event_result) = event_receiver.recv().await {
                match event_result {
                    Ok(notification) => {
                        debug!("Received event type: {}, identifier: {:?}", notification.event, notification.identifier);
                        // Check if this is our response
                        // TODO: ascent-obs.exe does NOT return identifier for query machine info
                        //if notification.event == EVT_QUERY_MACHINE_INFO && notification.identifier == Some(identifier) {
                        if notification.event == EVT_QUERY_MACHINE_INFO {
                            // Deserialize the payload
                            if let Ok(Some(payload)) = notification.deserialize_payload::<QueryMachineInfoEventPayload>() {
                                return Ok(payload);
                            } else {
                                return Err(ObsError::Deserialization("Failed to deserialize query response".into()));
                            }
                        } else if notification.event == EVT_ERR && 
                                  notification.identifier == Some(identifier) {
                            // Error response for our request
                            if let Ok(Some(error_payload)) = notification.deserialize_payload::<ErrorEventPayload>() {
                                return Err(ObsError::PipeError(format!(
                                    "Error response: Code {}, Description: {:?}", 
                                    error_payload.code, 
                                    error_payload.desc
                                )));
                            } else {
                                return Err(ObsError::PipeError("Error response with invalid payload".into()));
                            }
                        }
                        // Not our response, continue waiting
                    },
                    Err(e) => return Err(e),
                }
            }
            // Channel closed without receiving our response
            Err(ObsError::PipeError("Event channel closed before receiving response".into()))
        }
    ).await;
    
    // Shutdown the client regardless of the outcome
    let _ = client.shutdown().await;
    
    // Handle the timeout case and return the result
    match result {
        Ok(result) => result,
        Err(_) => Err(ObsError::PipeError("Timed out waiting for query response".into())),
    }
}