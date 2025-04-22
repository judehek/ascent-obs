use std::path::PathBuf;

use crate::types::VIDEO_ENCODER_ID_X264;

/// Configuration for a video recording session.
#[derive(Debug, Clone)]
pub struct RecordingConfig {
    /// Path to save the output file (required)
    pub output_file: PathBuf,
    
    /// Video encoder ID (default: "jim_nvenc")
    pub encoder_id: String,
    
    /// Frame rate (FPS) (default: 60)
    pub fps: u32,
    
    /// Output resolution (width, height) (default: 1920x1080)
    pub input_resolution: (u32, u32),

    pub output_resolution: (u32, u32),
    
    /// Whether to show the cursor (default: true)
    pub show_cursor: bool,
    
    /// Audio sample rate (default: 48000)
    pub sample_rate: u32,

    /// Bitrate in kbps (default: 6000)
    pub bitrate: u32,

    pub replay_buffer_seconds: Option<u32>,

    pub capture_microphone: bool,

}

impl RecordingConfig {
    /// Creates a new recording configuration with the required parameters 
    /// and default values for optional parameters.
    pub fn new(output_file: impl Into<PathBuf>) -> Self {
        Self {
            output_file: output_file.into(),
            encoder_id: VIDEO_ENCODER_ID_X264.to_string(),
            fps: 30,
            input_resolution: (1920, 1080),
            output_resolution: (1920, 1080),
            show_cursor: true,
            sample_rate: 48000,
            bitrate: 6000,
            replay_buffer_seconds: None,
            capture_microphone: false,
        }
    }

    /// Sets the video encoder ID.
    pub fn with_encoder(mut self, id: impl Into<String>) -> Self {
        self.encoder_id = id.into();
        self
    }

    /// Sets the recording frame rate.
    pub fn with_fps(mut self, fps: u32) -> Self {
        self.fps = fps;
        self
    }

    /// Sets the recording resolution.
    pub fn with_input_resolution(mut self, width: u32, height: u32) -> Self {
        self.input_resolution = (width, height);
        self
    }

    pub fn with_output_resolution(mut self, width: u32, height: u32) -> Self {
        self.output_resolution = (width, height);
        self
    }

    /// Sets whether to show the game cursor.
    pub fn with_cursor(mut self, show: bool) -> Self {
        self.show_cursor = show;
        self
    }

    /// Sets the audio sample rate.
    pub fn with_sample_rate(mut self, rate: u32) -> Self {
        self.sample_rate = rate;
        self
    }

    pub fn with_bitrate(mut self, bitrate: u32) -> Self {
        self.bitrate = bitrate;
        self
    }

    pub fn with_replay_buffer(mut self, replay_buffer_seconds: Option<u32>) -> Self {
        self.replay_buffer_seconds = replay_buffer_seconds;
        self
    }

    pub fn with_capture_microphone(mut self, capture_microphone: bool) -> Self {
        self.capture_microphone = capture_microphone;
        self
    }
}