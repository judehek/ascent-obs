// src/types/commands.rs
use serde::{Serialize, Deserialize};
use super::enums::RecorderType;
use super::settings::{
    VideoSettings, AudioSettings, FileOutputSettings, SceneSettings, ReplaySettings,
    StreamingSettings, GameSourceSettings, TobiiSourceSettings, BrbSourceSettings
};

// --- Command Specific Payloads ---

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct StartCommandPayload {
    pub recorder_type: RecorderType,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub video_settings: Option<VideoSettings>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub audio_settings: Option<AudioSettings>,
     #[serde(skip_serializing_if = "Option::is_none")]
    pub file_output: Option<FileOutputSettings>,
     #[serde(skip_serializing_if = "Option::is_none")]
     #[serde(rename = "sources")] // Match C++ field name kSettingsScene
    pub scene_settings: Option<SceneSettings>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub replay: Option<ReplaySettings>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub streaming: Option<StreamingSettings>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct StopCommandPayload {
     pub recorder_type: RecorderType,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct SetVolumeCommandPayload {
    // C++ CommandSetVolume takes the 'audio_settings' object containing input/output volumes
     #[serde(skip_serializing_if = "Option::is_none")]
     pub audio_settings: Option<AudioSettings>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct GameFocusChangedCommandPayload {
     pub game_foreground: bool,
     #[serde(skip_serializing_if = "Option::is_none")]
     pub is_minimized: Option<bool>,
}

// AddGameSource uses GameSourceSettings directly as its payload
// We embed GameSourceSettings directly when creating the request
// pub type AddGameSourceCommandPayload = GameSourceSettings;

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct StartReplayCaptureCommandPayload {
    pub head_duration: i64, // Milliseconds from the "head" of the buffer to save
    pub path: String, // Output file path
    #[serde(skip_serializing_if = "Option::is_none")]
    pub thumbnail_folder: Option<String>,
}

// StopReplayCapture doesn't seem to have a specific payload other than the identifier

// UpdateTobiiGaze uses TobiiSourceSettings directly as its payload
// pub type UpdateTobiiGazeCommandPayload = TobiiSourceSettings;

// SetBrb uses BrbSourceSettings directly as its payload
// pub type SetBrbCommandPayload = BrbSourceSettings;

// Commands like Shutdown, QueryMachineInfo, SplitVideo don't have specific payloads

// --- Generic Command Request Wrappers ---

/// Generic structure for sending a command with a payload.
#[derive(Serialize, Debug, Clone, PartialEq)]
pub struct CommandRequest<T> {
    pub cmd: i32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub identifier: Option<i32>,
    #[serde(flatten)] // Embed the payload fields directly into the JSON object
    pub payload: T,
}

/// Structure for sending commands that don't require a payload.
#[derive(Serialize, Debug, Clone, PartialEq)]
pub struct SimpleCommandRequest {
    pub cmd: i32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub identifier: Option<i32>,
}

// --- Type Aliases for Convenience ---
pub type StartRequest = CommandRequest<StartCommandPayload>;
pub type StopRequest = CommandRequest<StopCommandPayload>;
pub type SetVolumeRequest = CommandRequest<SetVolumeCommandPayload>;
pub type GameFocusChangedRequest = CommandRequest<GameFocusChangedCommandPayload>;
pub type AddGameSourceRequest = CommandRequest<GameSourceSettings>; // Payload is GameSourceSettings
pub type StartReplayCaptureRequest = CommandRequest<StartReplayCaptureCommandPayload>;
pub type UpdateTobiiGazeRequest = CommandRequest<TobiiSourceSettings>; // Payload is TobiiSourceSettings
pub type SetBrbRequest = CommandRequest<BrbSourceSettings>; // Payload is BrbSourceSettings
// Add others as needed