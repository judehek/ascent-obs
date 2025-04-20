use std::path::PathBuf;

/// Configuration for a video recording session.
#[derive(Debug, Clone)]
pub struct RecordingConfig {
    /// Path to save the output file (required)
    pub output_file: PathBuf,
    
    /// Process ID of the game to capture (required)
    pub game_pid: i32,
    
    /// Video encoder ID (default: "jim_nvenc")
    pub encoder_id: Option<String>,
    
    /// Frame rate (FPS) (default: 60)
    pub fps: Option<u32>,
    
    /// Output resolution (width, height) (default: 1920x1080)
    pub resolution: Option<(u32, u32)>,
    
    /// Whether to show the cursor (default: true)
    pub show_cursor: Option<bool>,
    
    /// Audio sample rate (default: 48000)
    pub sample_rate: Option<u32>,
}

impl RecordingConfig {
    /// Creates a new recording configuration with the required parameters.
    pub fn new(output_file: impl Into<PathBuf>, game_pid: i32) -> Self {
        Self {
            output_file: output_file.into(),
            game_pid,
            encoder_id: None,
            fps: None,
            resolution: None,
            show_cursor: None,
            sample_rate: None,
        }
    }

    /// Sets the video encoder ID.
    pub fn with_encoder(mut self, id: impl Into<String>) -> Self {
        self.encoder_id = Some(id.into());
        self
    }

    /// Sets the recording frame rate.
    pub fn with_fps(mut self, fps: u32) -> Self {
        self.fps = Some(fps);
        self
    }

    /// Sets the recording resolution.
    pub fn with_resolution(mut self, width: u32, height: u32) -> Self {
        self.resolution = Some((width, height));
        self
    }

    /// Sets whether to show the game cursor.
    pub fn with_cursor(mut self, show: bool) -> Self {
        self.show_cursor = Some(show);
        self
    }

    /// Sets the audio sample rate.
    pub fn with_sample_rate(mut self, rate: u32) -> Self {
        self.sample_rate = Some(rate);
        self
    }
}