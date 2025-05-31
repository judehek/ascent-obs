use std::path::PathBuf;
use serde::{Serialize, Deserialize};
use crate::types::{RateControlMode, VIDEO_ENCODER_ID_AMF, VIDEO_ENCODER_ID_X264};

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

    pub encoder_preset: Option<String>,
    
    /// System audio volume percentage (default: 100)
    pub system_audio_volume: u32,
    
    /// Microphone audio volume percentage (default: 100)
    pub microphone_volume: u32,
    
    /// Rate control mode (default: CBR)
    pub rate_control_mode: RateControlMode,
    
    /// CRF value for quality-based rate control (default: 23)
    pub crf_value: u32,
    
    /// CQP value for constant quality (default: 23)
    pub cqp_value: u32,
    
    /// Target bitrate in bits per second (default: 5,000,000)
    pub target_bitrate: u32,
    
    /// Maximum bitrate in bits per second (default: 10,000,000)
    pub max_bitrate: u32,
    
    /// Whether rate control preanalysis is enabled (default: false)
    pub rate_control_preanalysis_enabled: bool,
}

impl RecordingConfig {
    /// Creates a new recording configuration with the required parameters
    /// and default values for optional parameters.
    pub fn new(output_file: impl Into<PathBuf>) -> Self {
        Self {
            output_file: output_file.into(),
            encoder_id: VIDEO_ENCODER_ID_AMF.to_string(),
            fps: 30,
            output_resolution: (1920, 1080),
            show_cursor: true,
            sample_rate: 48000, 
            bitrate: 5000000, // bits per second
            replay_buffer_seconds: None,
            capture_microphone: false,
            microphone_device: None,
            window_audio_only: None,
            encoder_preset: None,
            system_audio_volume: 100,
            microphone_volume: 100,
            rate_control_mode: RateControlMode::Cbr,
            crf_value: 23,
            cqp_value: 23,
            target_bitrate: 5000000,
            max_bitrate: 10000000,
            rate_control_preanalysis_enabled: false,
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

    pub fn with_encoder_preset(mut self, encoder_preset: Option<String>) -> Self {
        self.encoder_preset = encoder_preset;
        self
    }
    
    /// Sets the system audio volume percentage.
    pub fn with_system_audio_volume(mut self, volume: u32) -> Self {
        self.system_audio_volume = volume;
        self
    }
    
    /// Sets the microphone audio volume percentage.
    pub fn with_microphone_volume(mut self, volume: u32) -> Self {
        self.microphone_volume = volume;
        self
    }
    
    /// Sets the rate control mode
    pub fn with_rate_control_mode(mut self, mode: RateControlMode) -> Self {
        self.rate_control_mode = mode;
        self
    }
    
    /// Sets the CRF (Constant Rate Factor) value (lower = better quality, 16-23 is a good range)
    pub fn with_crf_value(mut self, value: u32) -> Self {
        self.crf_value = value;
        self
    }
    
    /// Sets the CQP (Constant Quantizer Parameter) value (lower = better quality, 16-23 is a good range)
    pub fn with_cqp_value(mut self, value: u32) -> Self {
        self.cqp_value = value;
        self
    }
    
    /// Sets the target bitrate in bits per second
    pub fn with_target_bitrate(mut self, bitrate: u32) -> Self {
        self.target_bitrate = bitrate;
        self
    }
    
    /// Sets the maximum bitrate in bits per second
    pub fn with_max_bitrate(mut self, bitrate: u32) -> Self {
        self.max_bitrate = bitrate;
        self
    }
    
    /// Enables or disables rate control preanalysis
    pub fn with_rate_control_preanalysis(mut self, enabled: bool) -> Self {
        self.rate_control_preanalysis_enabled = enabled;
        self
    }
}