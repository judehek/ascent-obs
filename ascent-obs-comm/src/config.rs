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
    
    pub output_resolution: (u32, u32),
    
    /// Whether to show the cursor (default: true)
    pub show_cursor: bool,
    
    /// Audio sample rate (default: 48000)
    pub sample_rate: u32,
    
    /// Bitrate in kbps (default: 6000)
    pub bitrate: u32,
    
    pub replay_buffer_seconds: Option<u32>,
    
    pub capture_microphone: bool,

    pub microphone_device: Option<String>,

    /// If set will only capture audio from this window, example: "RiotClientServices.exe;League of Legends.exe"
    pub window_audio_only: Option<String>,
}

impl RecordingConfig {
    /// Creates a new recording configuration with the required parameters
    /// and default values for optional parameters.
    pub fn new(output_file: impl Into<PathBuf>) -> Self {
        Self {
            output_file: output_file.into(),
            encoder_id: VIDEO_ENCODER_ID_X264.to_string(),
            fps: 30,
            output_resolution: (1920, 1080),
            show_cursor: true,
            sample_rate: 48000,
            bitrate: 6000,
            replay_buffer_seconds: None,
            capture_microphone: false,
            microphone_device: None,
            window_audio_only: None,
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

    pub fn with_microphone_device(mut self, microphone_device: Option<String>) -> Self {
        self.microphone_device = microphone_device;
        self
    }

    pub fn with_window_audio_only(mut self, window_audio_only: Option<String>) -> Self {
        self.window_audio_only = window_audio_only;
        self
    }
}