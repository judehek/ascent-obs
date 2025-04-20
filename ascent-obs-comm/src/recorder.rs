use crate::communication::ObsClient;
use crate::errors::ObsError;
use crate::types::{
    // Keep necessary types for internal use
    AudioDeviceSettings, AudioSettings, ErrorEventPayload, FileOutputSettings, GameSourceSettings, QueryMachineInfoEventPayload, RecorderType, SceneSettings, StartCommandPayload, StopCommandPayload, VideoEncoderSettings, VideoSettings, CMD_QUERY_MACHINE_INFO, CMD_SHUTDOWN, CMD_START, CMD_STOP, EVT_ERR, EVT_QUERY_MACHINE_INFO, VIDEO_ENCODER_ID_NVENC_NEW
};
use crate::RecordingConfig;
use log::{debug, error, info, warn};
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
    active_recording_id: tokio::sync::Mutex<Option<i32>>,
}

impl Recorder {
    /// Creates a new recorder instance by starting the ascent-obs process.
    ///
    /// # Arguments
    /// * `ascent_obs_path` - Path to the ascent-obs.exe executable.
    /// * `buffer_size` - Optional size of the internal buffer. Default is 128.
    pub async fn new(
        ascent_obs_path: impl Into<PathBuf>,
        buffer_size: Option<usize>,
    ) -> Result<Self, ObsError> {
        let path = ascent_obs_path.into();
        info!("Starting ascent-obs process from: {:?}", path);
        
        // Start the client but discard the event receiver
        let buffer_size = buffer_size.unwrap_or(128);
        let (client, _event_receiver) = ObsClient::start(path, None, buffer_size).await?;
        
        Ok(Self {
            client,
            active_recording_id: tokio::sync::Mutex::new(None),
        })
    }

    /// Starts a video recording session with the given configuration.
    ///
    /// Returns a unique identifier for this recording session that can be used
    /// to stop it later.
    ///
    /// # Example
    /// ```ignore
    /// let config = RecordingConfig::new("my_recording.mp4", 12345)
    ///     .with_fps(60)
    ///     .with_resolution(1920, 1080);
    /// let recording_id = recorder.start_recording(config).await?;
    /// ```
    pub async fn start_recording(&self, config: RecordingConfig) -> Result<i32, ObsError> {
        let mut active_id_guard = self.active_recording_id.lock().await;

        if active_id_guard.is_some() {
            error!(
                "Start recording failed: another recording (id: {:?}) is already active.",
                active_id_guard
            );
            return Err(ObsError::AlreadyRecording);
        }

        let identifier = generate_identifier();
        let (output_width, output_height) = config.resolution.unwrap_or((1920, 1080));
        let fps = config.fps.unwrap_or(60);
        let encoder_id = config.encoder_id
            .unwrap_or_else(|| VIDEO_ENCODER_ID_NVENC_NEW.to_string());
        let game_cursor = config.show_cursor.unwrap_or(true);
        let sample_rate = config.sample_rate.unwrap_or(48000);

        let video_settings = VideoSettings {
            video_encoder: VideoEncoderSettings {
                encoder_id,
                ..Default::default()
            },
            game_cursor: Some(game_cursor),
            fps: Some(fps),
            base_width: Some(output_width),
            base_height: Some(output_height),
            output_width: Some(output_width),
            output_height: Some(output_height),
            ..Default::default()
        };

        let audio_settings = AudioSettings {
            sample_rate: Some(sample_rate),
            output_device: Some(AudioDeviceSettings { device_id: Some("default".to_string()), ..Default::default() }),
            input_device: Some(AudioDeviceSettings { device_id: Some("default".to_string()), ..Default::default() }),
            ..Default::default()
        };

        let scene_settings = SceneSettings {
            game: Some(GameSourceSettings {
                process_id: Some(config.game_pid),
                foreground: Some(true),
                allow_transparency: Some(false),
                ..Default::default()
            }),
            ..Default::default()
        };

        let file_settings = FileOutputSettings {
            filename: Some(config.output_file.to_string_lossy().into_owned()),
            ..Default::default()
        };

        let start_payload = StartCommandPayload {
            recorder_type: RecorderType::Video,
            video_settings: Some(video_settings),
            audio_settings: Some(audio_settings),
            file_output: Some(file_settings),
            scene_settings: Some(scene_settings),
            ..Default::default()
        };

        info!(
            "Sending START command (id: {}, type: {:?}, path: {:?}, pid: {})",
            identifier,
            start_payload.recorder_type,
            config.output_file,
            config.game_pid
        );

        // TODO: we have no idea whether the recording actually started
        *active_id_guard = Some(identifier);
        self.client
            .send_command(CMD_START, Some(identifier), start_payload)
            .await?;

        Ok(identifier)
    }

    /// Stops a specific recording.
    ///
    /// # Arguments
    /// * `identifier` - The identifier returned when the recording was started.
    pub async fn stop_recording(&self) -> Result<(), ObsError> {
        let mut active_id_guard = self.active_recording_id.lock().await;

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
            // It's debatable whether this should be an error or not.
            // Let's make it an error for clarity.
            Err(ObsError::NotRecording)
        }
        // Lock is released when active_id_guard goes out of scope
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
                        // Check if this is our response
                        if notification.event == EVT_QUERY_MACHINE_INFO && 
                           notification.identifier == Some(identifier) {
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