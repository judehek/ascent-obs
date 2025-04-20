use crate::communication::ObsClient;
use crate::errors::ObsError;
use crate::types::{
    // Keep necessary types for internal use and builder
    AudioDeviceSettings, AudioSettings, EventNotification, FileOutputSettings,
    GameSourceSettings, RecorderType, SceneSettings, StartCommandPayload, StopCommandPayload,
    VideoEncoderSettings, VideoSettings,
    // Command codes needed internally
    CMD_QUERY_MACHINE_INFO, CMD_SHUTDOWN, CMD_START, CMD_STOP,
    // For default encoder settings
    VIDEO_ENCODER_ID_NVENC_NEW,
};
use log::{debug, error, info};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicI32, Ordering};
use tokio::sync::mpsc;

// Counter for generating simple identifiers when needed
static NEXT_IDENTIFIER: AtomicI32 = AtomicI32::new(1);

/// Generates a unique identifier for commands/requests.
fn generate_identifier() -> i32 {
    NEXT_IDENTIFIER.fetch_add(1, Ordering::Relaxed)
}

// Type alias for the event receiver channel
pub type EventReceiver = mpsc::Receiver<Result<EventNotification, ObsError>>;

// --- RecorderBuilder ---

/// Builder for configuring and starting the ascent-obs process.
#[derive(Debug, Clone)]
pub struct RecorderBuilder {
    ascent_obs_path: PathBuf,
    buffer_size: usize,
    // Add other OBS process-level config here if needed later
}

impl RecorderBuilder {
    /// Creates a new builder for the recorder.
    ///
    /// # Arguments
    /// * `ascent_obs_path` - Path to the ascent-obs.exe executable.
    pub fn new(ascent_obs_path: impl Into<PathBuf>) -> Self {
        Self {
            ascent_obs_path: ascent_obs_path.into(),
            buffer_size: 128, // Default buffer size
        }
    }

    /// Sets the size of the internal channel buffer for incoming events.
    /// Defaults to 128.
    pub fn event_buffer_size(mut self, size: usize) -> Self {
        self.buffer_size = size;
        self
    }

    /// Starts the ascent-obs.exe process and returns a `Recorder` instance
    /// along with a channel receiver for events.
    pub async fn build(self) -> Result<(Recorder, EventReceiver), ObsError> {
        info!("Starting ascent-obs process from: {:?}", self.ascent_obs_path);
        // For stdio communication, channel_id is typically None.
        let (client, event_receiver) =
            ObsClient::start(self.ascent_obs_path, None, self.buffer_size).await?;

        Ok((
            Recorder {
                client,
                // Internal state for tracking started recordings could go here if needed
            },
            event_receiver,
        ))
    }
}

// --- StartRecordingBuilder ---

/// Builder for configuring and starting a specific video recording session.
#[derive(Debug)]
pub struct StartRecordingBuilder<'a> {
    recorder: &'a Recorder,
    output_file: Option<PathBuf>,
    game_pid: Option<i32>,
    encoder_id: Option<String>,
    fps: Option<u32>,
    resolution: Option<(u32, u32)>,
    show_cursor: Option<bool>,
    sample_rate: Option<u32>,
    // Add more configuration methods as needed
}

impl<'a> StartRecordingBuilder<'a> {
    /// Creates a new builder associated with a Recorder instance.
    /// (Internal constructor, accessed via `recorder.start_video_recording()`)
    fn new(recorder: &'a Recorder) -> Self {
        Self {
            recorder,
            output_file: None,
            game_pid: None,
            encoder_id: None,
            fps: None,
            resolution: None,
            show_cursor: None,
            sample_rate: None,
        }
    }

    /// Sets the output file path for the recording (Required).
    pub fn output_file(mut self, path: impl Into<PathBuf>) -> Self {
        self.output_file = Some(path.into());
        self
    }

    /// Sets the process ID of the game to capture (Required).
    pub fn capture_game(mut self, pid: i32) -> Self {
        self.game_pid = Some(pid);
        self
    }

    /// Sets the video encoder ID (e.g., "jim_nvenc").
    /// Defaults to "jim_nvenc" if not set.
    pub fn video_encoder(mut self, id: impl Into<String>) -> Self {
        self.encoder_id = Some(id.into());
        self
    }

    /// Sets the recording frame rate (FPS). Defaults to 60.
    pub fn fps(mut self, fps: u32) -> Self {
        self.fps = Some(fps);
        self
    }

    /// Sets the recording output resolution. Defaults to 1920x1080.
    pub fn resolution(mut self, width: u32, height: u32) -> Self {
        self.resolution = Some((width, height));
        self
    }

    /// Sets whether to capture the game cursor. Defaults to true.
    pub fn show_game_cursor(mut self, show: bool) -> Self {
        self.show_cursor = Some(show);
        self
    }

    /// Sets the audio sample rate. Defaults to 48000.
    pub fn audio_sample_rate(mut self, rate: u32) -> Self {
        self.sample_rate = Some(rate);
        self
    }

    /// Validates settings, constructs the StartCommandPayload, sends the command,
    /// and returns the unique identifier for this recording session.
    pub async fn start(self) -> Result<i32, ObsError> {
        let output_file = self
            .output_file
            .ok_or_else(|| ObsError::Configuration("Output file path is required".to_string()))?;
        let game_pid = self
            .game_pid
            .ok_or_else(|| ObsError::Configuration("Game process ID is required".to_string()))?;

        let identifier = generate_identifier();
        let (output_width, output_height) = self.resolution.unwrap_or((1920, 1080));
        let fps = self.fps.unwrap_or(60);
        let encoder_id = self
            .encoder_id
            .unwrap_or_else(|| VIDEO_ENCODER_ID_NVENC_NEW.to_string()); // Default Encoder
        let game_cursor = self.show_cursor.unwrap_or(true);
        let sample_rate = self.sample_rate.unwrap_or(48000);

        let video_settings = VideoSettings {
            video_encoder: VideoEncoderSettings {
                encoder_id,
                ..Default::default() // Keep default encoder settings for now
            },
            game_cursor: Some(game_cursor),
            fps: Some(fps),
            base_width: Some(output_width), // Base = Output for simplicity here
            base_height: Some(output_height),
            output_width: Some(output_width),
            output_height: Some(output_height),
            ..Default::default()
        };

        let audio_settings = AudioSettings {
            sample_rate: Some(sample_rate),
            // Use default audio devices unless more specific config is added
            output_device: Some(AudioDeviceSettings { device_id: Some("default".to_string()), ..Default::default() }),
            input_device: Some(AudioDeviceSettings { device_id: Some("default".to_string()), ..Default::default() }),
            ..Default::default()
        };

        let scene_settings = SceneSettings {
            game: Some(GameSourceSettings {
                process_id: Some(game_pid),
                foreground: Some(true), // Assume foreground for basic recording
                allow_transparency: Some(false),
                ..Default::default()
            }),
            ..Default::default()
        };

        let file_settings = FileOutputSettings {
            filename: Some(output_file.to_string_lossy().into_owned()),
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
            output_file,
            game_pid
        );

        self.recorder
            .client
            .send_command(CMD_START, Some(identifier), start_payload)
            .await?;

        Ok(identifier)
    }
}

// --- Recorder ---

/// A high-level client for controlling an ascent-obs recording process,
/// focusing on simplified video recording operations.
#[derive(Debug)]
pub struct Recorder {
    client: ObsClient,
    // Add internal state tracking if needed later (e.g., map of active recording IDs)
}

impl Recorder {
    /// Creates a builder to configure and start the ascent-obs process.
    /// Use `RecorderBuilder::new(path).build().await` to get a Recorder instance.
    pub fn builder(ascent_obs_path: impl Into<PathBuf>) -> RecorderBuilder {
        RecorderBuilder::new(ascent_obs_path)
    }

    // Private start method used by the builder
    // async fn start(
    //     ascent_obs_path: impl AsRef<Path>,
    //     buffer_size: usize,
    // ) -> Result<(Self, EventReceiver), ObsError> {
    //     info!("Starting ascent-obs process from: {:?}", ascent_obs_path.as_ref());
    //     let (client, event_receiver) = ObsClient::start(ascent_obs_path, None, buffer_size).await?;
    //     Ok((
    //         Self {
    //             client,
    //         },
    //         event_receiver,
    //     ))
    // }

    /// Creates a builder to configure and initiate a video recording.
    ///
    /// Call `.start().await` on the returned builder to begin recording.
    /// The `start()` method returns a unique identifier for this recording session.
    ///
    /// # Example
    /// ```ignore
    /// let recording_id = recorder.start_video_recording()
    ///     .output_file("my_recording.mp4")
    ///     .capture_game(12345) // PID
    ///     .fps(60)
    ///     .start()
    ///     .await?;
    /// ```
    pub fn start_video_recording(&self) -> StartRecordingBuilder<'_> {
        StartRecordingBuilder::new(self)
    }

    /// Sends a command to stop a specific recording.
    ///
    /// # Arguments
    /// * `identifier` - The identifier returned when the recording was started.
    pub async fn stop_recording(&self, identifier: i32) -> Result<(), ObsError> {
        info!(
            "Sending STOP command (id: {}, type: {:?})",
            identifier,
            RecorderType::Video
        );
        let payload = StopCommandPayload {
            recorder_type: RecorderType::Video,
        };
        // Use internal client method
        self.client
            .send_command(CMD_STOP, Some(identifier), payload)
            .await
    }

    /// Sends a request to query machine information (encoders, audio devices).
    /// Returns the identifier sent with the command, which can be used to
    /// match the corresponding `EVT_QUERY_MACHINE_INFO` event.
    pub async fn query_machine_info(&self) -> Result<i32, ObsError> {
        let identifier = generate_identifier();
        info!("Sending QUERY_MACHINE_INFO command (id: {})", identifier);
        self.client
            .send_simple_command(CMD_QUERY_MACHINE_INFO, Some(identifier))
            .await?;
        Ok(identifier)
    }

    /// Sends the shutdown command to ascent-obs and waits for the process and tasks to terminate.
    /// Consumes the `Recorder` instance.
    pub async fn shutdown(self) -> Result<(), ObsError> {
        info!("Sending SHUTDOWN command");
        // Send the command first, don't strictly wait for confirmation
        // as the shutdown method will kill the process anyway.
        let _ = self
            .client
            .send_simple_command(CMD_SHUTDOWN, None) // Shutdown doesn't need ID
            .await;
        // Now call the client's shutdown which handles process termination etc.
        self.client.shutdown().await
    }
}