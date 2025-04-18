// src/types/events.rs
use serde::{Deserialize, Serialize};
use serde_json::value::RawValue; // For delayed payload deserialization
use std::collections::HashMap;

// --- Event Specific Payloads ---

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
#[serde(untagged)] // Allows deserializing into either a map or potentially a specific struct later
pub enum AudioDevice {
    // Handles {"Device Name": "device_id"} structure from C++
    Map(HashMap<String, String>),
    // Add specific device struct if needed later
}

impl Default for AudioDevice {
    fn default() -> Self {
        AudioDevice::Map(HashMap::new())
    }
}


#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct VideoEncoderInfo {
    #[serde(rename = "type")]
    pub encoder_type: String,
    pub description: String,
    pub valid: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub status: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub code: Option<String>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct QueryMachineInfoEventPayload {
    #[serde(rename = "adio_in_devs")] // Match C++ typo
    pub audio_input_devices: Vec<AudioDevice>,
    #[serde(rename = "adio_out_devs")] // Match C++ typo
    pub audio_output_devices: Vec<AudioDevice>,
    #[serde(rename = "vid_encs")]
    pub video_encoders: Vec<VideoEncoderInfo>,
    pub winrt_capture_supported: bool,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct ErrorEventPayload {
    pub code: i32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub desc: Option<String>,
    // May sometimes contain the original data that caused the error
    #[serde(skip_serializing_if = "Option::is_none")]
    pub data: Option<serde_json::Value>,
}

// Ready event typically has no specific payload beyond identifier

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct RecordingStartedEventPayload {
    pub source: String, // Name of the visible source
    #[serde(skip_serializing_if = "Option::is_none", rename = "is_window_capture")]
    pub is_window_capture: Option<bool>
}

// RecordingStopping event typically has no specific payload beyond identifier

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct StatsData {
    // system_info seems opaque in C++, using Value for flexibility
    #[serde(skip_serializing_if = "Option::is_none")]
    pub system_info: Option<serde_json::Value>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub percentage_lagged: Option<i32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub drawn: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub lagged: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub dropped: Option<i32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub total_frames: Option<i32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub percentage_dropped: Option<i32>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct RecordingStoppedEventPayload {
    pub code: i32, // 0 for success, see codes::OUTPUT_*
    #[serde(skip_serializing_if = "Option::is_none")]
    pub last_error: Option<String>,
    pub duration: i64, // Milliseconds
    #[serde(skip_serializing_if = "Option::is_none")]
    pub output_width: Option<u32>, // Added Option<> as it might not always be present
    #[serde(skip_serializing_if = "Option::is_none")]
    pub output_height: Option<u32>,// Added Option<>
    #[serde(skip_serializing_if = "Option::is_none")]
    pub stats_data: Option<StatsData>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct DisplaySourceChangedEventPayload {
    pub source: String, // Name of the new visible source
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct VideoFileSplitEventPayload {
    pub duration: i64, // Milliseconds since recording start
    pub split_file_duration: i64, // Milliseconds duration of the split file itself
    pub frame_pts: i64, // Timestamp of the last frame in the split file
    #[serde(rename = "count")]
    pub split_count: i32,
    pub path: String, // Path to the completed split file
    pub next_video_path: String, // Path to the *new* file being recorded
    #[serde(skip_serializing_if = "Option::is_none")]
    pub output_width: Option<u32>,// Added Option<>
    #[serde(skip_serializing_if = "Option::is_none")]
    pub output_height: Option<u32>,// Added Option<>
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct ReplayStartedEventPayload {
    pub source: String, // Name of the visible source
     #[serde(skip_serializing_if = "Option::is_none", rename = "is_window_capture")]
     pub is_window_capture: Option<bool>
}

// ReplayStopping event typically has no specific payload beyond identifier
// ReplayArmed event typically has no specific payload beyond identifier
// ReplayCaptureVideoStarted event typically has no specific payload beyond identifier

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct ReplayStoppedEventPayload {
     pub code: i32, // See codes::REPLAY_ERROR_* or codes::OUTPUT_*
     #[serde(skip_serializing_if = "Option::is_none")]
     pub last_error: Option<String>,
     #[serde(skip_serializing_if = "Option::is_none")]
     pub stats_data: Option<StatsData>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct ReplayCaptureVideoReadyEventPayload {
    pub duration: i64, // Milliseconds
    pub video_start_time: i64, // Unix epoch milliseconds
    pub path: String,
    pub thumbnail_folder: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub output_width: Option<u32>,// Added Option<>
    #[serde(skip_serializing_if = "Option::is_none")]
    pub output_height: Option<u32>,// Added Option<>
    pub disconnection: bool, // Indicates if replay output was stopped because of this
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct ReplayErrorEventPayload {
    pub code: i32, // See codes::REPLAY_ERROR_*
    #[serde(skip_serializing_if = "Option::is_none")]
    pub desc: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub path: Option<String>, // Path related to the error, if applicable
}

// StreamingStarting event typically has no specific payload beyond identifier

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct StreamingStartedEventPayload {
     pub source: String, // Name of the visible source
}

// StreamingStopping event typically has no specific payload beyond identifier

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct StreamingStoppedEventPayload {
    pub code: i32, // See codes::OUTPUT_*
    #[serde(skip_serializing_if = "Option::is_none")]
    pub last_error: Option<String>,
     #[serde(skip_serializing_if = "Option::is_none")]
    pub stats_data: Option<StatsData>,
}

// SwitchableDeviceDetected event typically has no specific payload beyond identifier

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct ObsWarningEventPayload {
    pub message: String,
    // Extra seems to be an object, using Value for flexibility
    #[serde(skip_serializing_if = "Option::is_none")]
    pub extra: Option<serde_json::Value>,
}

// --- Generic Event Notification Wrapper ---

/// Helper structure for receiving an event.
/// Deserialize into this first, then match on `event` to deserialize
/// the `payload` RawValue into the appropriate specific payload struct.
#[derive(Deserialize, Debug, Clone)]
pub struct EventNotification {
    pub event: i32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub identifier: Option<i32>,
    #[serde(flatten)]
    pub payload: Option<serde_json::Value>, // Store payload as generic Value
}

// Method to deserialize the payload based on the event type
impl EventNotification {
    pub fn deserialize_payload<T>(&self) -> Result<Option<T>, serde_json::Error>
    where
        T: serde::de::DeserializeOwned, // Changed from Deserialize<'a> to DeserializeOwned
    {
        match &self.payload {
            Some(value) => {
                // Convert the Value directly rather than getting a string
                serde_json::from_value(value.clone()).map(Some)
            }
            None => Ok(None), // No payload present
        }
    }
}

// --- Type Aliases for Convenience (Optional) ---
pub type QueryMachineInfoEvent = QueryMachineInfoEventPayload;
pub type ErrorEvent = ErrorEventPayload;
pub type RecordingStartedEvent = RecordingStartedEventPayload;
pub type RecordingStoppedEvent = RecordingStoppedEventPayload;
pub type DisplaySourceChangedEvent = DisplaySourceChangedEventPayload;
pub type VideoFileSplitEvent = VideoFileSplitEventPayload;
pub type ReplayStartedEvent = ReplayStartedEventPayload;
pub type ReplayStoppedEvent = ReplayStoppedEventPayload;
pub type ReplayCaptureVideoReadyEvent = ReplayCaptureVideoReadyEventPayload;
pub type ReplayErrorEvent = ReplayErrorEventPayload;
pub type StreamingStartedEvent = StreamingStartedEventPayload;
pub type StreamingStoppedEvent = StreamingStoppedEventPayload;
pub type ObsWarningEvent = ObsWarningEventPayload;
// ... add others for events without specific payloads if needed, though less useful