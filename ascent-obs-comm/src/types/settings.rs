// src/types/settings.rs
use serde::{Serialize, Deserialize};
use std::collections::HashMap;
use super::enums::ObsFlipType; // Import if needed

// Type alias for encoder-specific settings, which can vary wildly.
// Using serde_json::Value allows flexibility.
pub type EncoderSettings = HashMap<String, serde_json::Value>;

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
#[serde(rename_all = "camelCase")] // Use camelCase for JS compatibility if needed
pub struct AudioDeviceSettings {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub device_id: Option<String>, // "default", "disabled", or specific ID
    #[serde(skip_serializing_if = "Option::is_none")]
    pub volume: Option<i32>, // Typically 0-100 or 0-2000; -1 might mean disabled/ignore
    #[serde(skip_serializing_if = "Option::is_none")]
    pub mono: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none", rename = "use_device_timing")]
    pub use_device_timing: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<String>, // For V2 audio sources list
    #[serde(skip_serializing_if = "Option::is_none", rename = "type")]
    pub device_type: Option<i32>, // For V2 audio sources list (0=out, 1=in for 'default')
    #[serde(skip_serializing_if = "Option::is_none")]
    pub enable: Option<bool>, // For V2 audio sources list
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tracks: Option<u32>, // For V2 audio sources list (bitmask)
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
#[serde(rename_all = "camelCase")]
pub struct AudioProcessCaptureSettings {
    pub process_name: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub enable: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub mono: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub volume: Option<i32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tracks: Option<u32>, // Bitmask
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
#[serde(rename_all = "camelCase")]
pub struct AudioExtraOptions {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub separate_tracks: Option<bool>,
    // Legacy process capture list (semicolon separated)
    #[serde(skip_serializing_if = "Option::is_none")]
    pub audio_capture_process: Option<String>,
    // New process capture list (array of objects)
    #[serde(skip_serializing_if = "Option::is_none")]
    pub audio_capture_process2: Option<Vec<AudioProcessCaptureSettings>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub sample_rate: Option<u32>, // Can also be top-level in AudioSettings
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tracks: Option<u32>, // Master track assignment? Bitmask.
     // New in V2 for specifying default devices via list instead of input/output objects
    #[serde(skip_serializing_if = "Option::is_none")]
    pub audio_sources: Option<Vec<AudioDeviceSettings>>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct AudioSettings {
    #[serde(skip_serializing_if = "Option::is_none", rename = "output")]
    pub output_device: Option<AudioDeviceSettings>, // Legacy V1 style
    #[serde(skip_serializing_if = "Option::is_none", rename = "input")]
    pub input_device: Option<AudioDeviceSettings>, // Legacy V1 style
    #[serde(skip_serializing_if = "Option::is_none", rename = "extra_options")]
    pub extra_options: Option<AudioExtraOptions>,
    // Some settings might be duplicated from extra_options for direct access
    #[serde(skip_serializing_if = "Option::is_none")]
    pub sample_rate: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub mono: Option<bool>, // Global mono setting? Usually per-device.
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct VideoEncoderSettings {
    #[serde(rename = "id")]
    pub encoder_id: String, // e.g., "jim_nvenc", "obs_x264" must match one returned by QUERY_MACHINE_INFO
    #[serde(flatten)] // Include other encoder-specific settings directly
    pub settings: EncoderSettings,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
#[serde(rename_all = "camelCase")]
pub struct VideoExtraOptions {
     #[serde(skip_serializing_if = "Option::is_none")]
     pub encoder_custom_parameters: Option<EncoderSettings>,
     #[serde(skip_serializing_if = "Option::is_none")]
     pub custom_parameters: Option<EncoderSettings>, // Opaque custom params for OBS core?
     #[serde(skip_serializing_if = "Option::is_none")]
     pub color_space: Option<String>, // e.g., "Rec709"
     #[serde(skip_serializing_if = "Option::is_none")]
     pub color_format: Option<String>, // e.g., "NV12"
     #[serde(skip_serializing_if = "Option::is_none")]
     pub fragmented_video_file: Option<bool>,
     // Add other potential extra video options here
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct VideoSettings {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub fps: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub base_width: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub base_height: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub output_width: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub output_height: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub compatibility_mode: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub game_cursor: Option<bool>,
    pub video_encoder: VideoEncoderSettings,
    #[serde(skip_serializing_if = "Option::is_none", rename = "extra_options")]
    pub extra_options: Option<VideoExtraOptions>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
#[serde(rename_all = "camelCase")]
pub struct FileOutputSettings {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub filename: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub max_file_size_bytes: Option<i64>, // Use i64 for large sizes
    #[serde(skip_serializing_if = "Option::is_none")]
    pub max_time_sec: Option<i64>, // Use i64
    #[serde(skip_serializing_if = "Option::is_none", rename="enable_on_demand_spilt_video")]
    pub enable_on_demand_split_video: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub include_full_video: Option<bool>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
#[serde(rename_all = "camelCase")]
pub struct MonitorSourceSettings {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub enable: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub force: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub monitor_handle: Option<i64>, // Monitor handle (HWND essentially), use i64 for safety
    #[serde(skip_serializing_if = "Option::is_none")]
    pub cursor: Option<bool>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
#[serde(rename_all = "camelCase")]
pub struct WindowCaptureSourceSettings {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub enable: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub window_handle: Option<i64>, // Window handle (HWND), use i64 for safety
    #[serde(skip_serializing_if = "Option::is_none")]
    pub cursor: Option<bool>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
#[serde(rename_all = "camelCase")]
pub struct GameSourceSettings {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub process_id: Option<i32>, // Process ID to capture
    #[serde(skip_serializing_if = "Option::is_none")]
    pub foreground: Option<bool>, // Hint if the game window is expected to be foreground
    #[serde(skip_serializing_if = "Option::is_none")]
    pub allow_transparency: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub flip_type: Option<ObsFlipType>, // Or i32 if using raw values
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
#[serde(rename_all = "camelCase")]
pub struct BrbSourceSettings {
     #[serde(skip_serializing_if = "Option::is_none")]
     pub path: Option<String>, // Path to the BRB image
     #[serde(skip_serializing_if = "Option::is_none")]
     pub color: Option<i32>, // Background color (format ABGR?)
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
#[serde(rename_all = "camelCase")]
pub struct AuxSourceParameter {
    pub name: String,
    #[serde(rename = "type")]
    pub param_type: i32, // 0=int, 1=bool, 2=string, 3=double
    pub value: serde_json::Value, // Use Value to handle different types easily
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
#[serde(rename_all = "camelCase")]
pub struct AuxSourceSettings {
    #[serde(rename = "sourceId")]
    pub source_id: String, // OBS source type ID (e.g., "image_source")
    pub name: String, // Unique name for this source instance
    #[serde(skip_serializing_if = "Option::is_none")]
    pub parameters: Option<Vec<AuxSourceParameter>>, // Source-specific settings
    #[serde(skip_serializing_if = "Option::is_none")]
    pub transform: Option<String>, // Docking identifier (e.g., "dockTopLeft")
    #[serde(skip_serializing_if = "Option::is_none")]
    pub posx: Option<f32>, // Relative X position (0.0-1.0)
    #[serde(skip_serializing_if = "Option::is_none")]
    pub posy: Option<f32>, // Relative Y position (0.0-1.0)
    #[serde(skip_serializing_if = "Option::is_none")]
    pub scalex: Option<f32>, // Relative width scale (0.0-1.0)
    #[serde(skip_serializing_if = "Option::is_none")]
    pub scaley: Option<f32>, // Relative height scale (0.0-1.0)
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "secondaryFile")] // Match C++ field name
    pub secondary_file: Option<bool> // Should this source only appear in secondary ascent-obs process?
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
#[serde(rename_all = "camelCase")]
pub struct TobiiSourceSettings {
     #[serde(skip_serializing_if = "Option::is_none")]
     pub window: Option<String>, // Window title or class to capture gaze overlay from
     #[serde(skip_serializing_if = "Option::is_none")]
     pub visible: Option<bool>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
#[serde(rename_all = "camelCase")]
pub struct SceneSettings { // Represents the C++ "sources" object
    #[serde(skip_serializing_if = "Option::is_none")]
    pub monitor: Option<MonitorSourceSettings>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub window_capture: Option<WindowCaptureSourceSettings>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub game: Option<GameSourceSettings>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub brb: Option<BrbSourceSettings>,
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "auxSources")]
    pub aux_sources: Option<Vec<AuxSourceSettings>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tobii: Option<TobiiSourceSettings>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub keep_game_recording: Option<bool>, // keep_recording_on_lost_focus
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
#[serde(rename_all = "camelCase")]
pub struct ReplaySettings {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub max_time_sec: Option<i64>,
    // Other replay-specific settings like buffer size might go here if controllable
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
#[serde(rename_all = "camelCase")]
pub struct StreamingSettings {
    #[serde(skip_serializing_if = "Option::is_none", rename = "type")]
    pub service_type: Option<String>, // e.g., "Custom", "Twitch"
    #[serde(skip_serializing_if = "Option::is_none")]
    pub server_url: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub stream_key: Option<String>,
    // Fields for custom RTMP auth
    #[serde(skip_serializing_if = "Option::is_none")]
    pub use_auth: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub username: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub password: Option<String>,
}