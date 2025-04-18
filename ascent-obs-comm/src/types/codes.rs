// src/types/codes.rs

// --- Command Identifiers ---
pub const CMD_SHUTDOWN: i32 = 1;
pub const CMD_QUERY_MACHINE_INFO: i32 = 2;
pub const CMD_START: i32 = 3;
pub const CMD_STOP: i32 = 4;
pub const CMD_SET_VOLUME: i32 = 5;
pub const CMD_GAME_FOCUS_CHANGED: i32 = 6;
pub const CMD_ADD_GAME_SOURCE: i32 = 7;
pub const CMD_START_REPLAY_CAPTURE: i32 = 8;
pub const CMD_STOP_REPLAY_CAPTURE: i32 = 9;
pub const CMD_TOBII_GAZE: i32 = 10;
pub const CMD_SET_BRB: i32 = 11;
pub const CMD_SPLIT_VIDEO: i32 = 12;

// --- Event Identifiers ---
pub const EVT_QUERY_MACHINE_INFO: i32 = 1;
pub const EVT_ERR: i32 = 2;
pub const EVT_READY: i32 = 3;
pub const EVT_RECORDING_STARTED: i32 = 4;
pub const EVT_RECORDING_STOPPING: i32 = 5;
pub const EVT_RECORDING_STOPPED: i32 = 6;
pub const EVT_DISPLAY_SOURCE_CHANGED: i32 = 7;
pub const EVT_VIDEO_FILE_SPLIT: i32 = 8;
pub const EVT_REPLAY_STARTED: i32 = 9;
pub const EVT_REPLAY_STOPPING: i32 = 10;
pub const EVT_REPLAY_STOPPED: i32 = 11;
pub const EVT_REPLAY_ARMED: i32 = 12;
pub const EVT_REPLAY_CAPTURE_VIDEO_STARTED: i32 = 13;
pub const EVT_REPLAY_CAPTURE_VIDEO_READY: i32 = 14;
pub const EVT_REPLAY_ERROR: i32 = 15;
pub const EVT_STREAMING_STARTING: i32 = 16;
pub const EVT_STREAMING_STARTED: i32 = 17;
pub const EVT_STREAMING_STOPPING: i32 = 18;
pub const EVT_STREAMING_STOPPED: i32 = 19;
pub const EVT_SWITCHABLE_DEVICE_DETECTED: i32 = 20;
pub const EVT_OBS_WARNING: i32 = 21;

// --- Error Codes (Examples from protocol.h) ---
// Init Errors
pub const INIT_ERROR_CURRENTLY_ACTIVE: i32 = -1;
pub const INIT_ERROR_FAILED_TO_INIT: i32 = -2;
pub const INIT_ERROR_FAILED_TO_CREATE_SCENE: i32 = -3;
pub const INIT_ERROR_FAILED_TO_CREATE_SOURCES: i32 = -4;
pub const INIT_ERROR_MISSING_PARAM: i32 = -5;
pub const INIT_ERROR_UNSUPPORTED_VIDEO_ENCODER: i32 = -6;
pub const INIT_ERROR_FAILED_CREATING_OUTPUT_FILE: i32 = -7;
pub const INIT_ERROR_FAILED_CREATING_VID_ENCODER: i32 = -8;
pub const INIT_ERROR_FAILED_CREATING_AUD_ENCODER: i32 = -9;
pub const INIT_ERROR_FAILED_STARTING_UPDATE_DRIVER_ERROR: i32 = -10;
pub const INIT_ERROR_FAILED_CREATING_OUTPUT_ALREADY_CREATED: i32 = -11;
pub const INIT_ERROR_FAILED_CREATING_OUTPUT_SIGNALS: i32 = -12;
pub const INIT_ERROR_FAILED_STARTING_OUTPUT_ALREADY_RUNNING: i32 = -13;
pub const INIT_ERROR_FAILED_UNSUPPORTED_RECORDING_TYPE: i32 = -14;
pub const INIT_ERROR_REPLAY_START_ERROR: i32 = -15;
pub const INIT_ERROR_STREAM_START_NO_SERVICE_ERROR: i32 = -16;
pub const INIT_ERROR_FAILED_STARTING_OUTPUT_WITH_OBS_ERROR: i32 = -17;
pub const INIT_ERROR_GAME_INJECTION_ERROR: i32 = -18;

// Replay Errors
pub const REPLAY_ERROR_OFFLINE: i32 = -1;
pub const REPLAY_ERROR_START_CAPTURE_OBS_ERROR: i32 = -2;
pub const REPLAY_ERROR_START_CAPTURE_ALREADY_CAPTURING: i32 = -3;
pub const REPLAY_ERROR_STOP_CAPTURE_NO_CAPTURE: i32 = -4;
pub const REPLAY_ERROR_STOP_CAPTURE_OBS_ERROR: i32 = -5;
pub const REPLAY_ERROR_REPLAY_OBS_ERROR: i32 = -6;
pub const REPLAY_ERROR_REPLAY_OFFLINE_DELAY: i32 = -7;

// Output Errors (from obs-defs.h via protocol.h)
pub const OUTPUT_SUCCESS: i32 = 0;
pub const OUTPUT_BAD_PATH: i32 = -1;
pub const OUTPUT_CONNECT_FAILED: i32 = -2;
pub const OUTPUT_INVALID_STREAM: i32 = -3;
pub const OUTPUT_ERROR: i32 = -4;
pub const OUTPUT_DISCONNECTED: i32 = -5;
pub const OUTPUT_UNSUPPORTED: i32 = -6;
pub const OUTPUT_NO_SPACE: i32 = -7;
pub const OUTPUT_ENCODE_ERROR: i32 = -8;

// Video Errors (from protocol.h)
pub const VIDEO_SUCCESS: i32 = 0;
pub const VIDEO_FAIL: i32 = -1;
pub const VIDEO_NOT_SUPPORTED: i32 = -2;
pub const VIDEO_INVALID_PARAM: i32 = -3;
pub const VIDEO_CURRENTLY_ACTIVE: i32 = -4;
pub const VIDEO_MODULE_NOT_FOUND: i32 = -5;

// --- Audio Track Flags (bitmask) ---
pub const AUDIO_TRACK_1: u32 = 1 << 0;
pub const AUDIO_TRACK_2: u32 = 1 << 1;
pub const AUDIO_TRACK_3: u32 = 1 << 2;
pub const AUDIO_TRACK_4: u32 = 1 << 3;
pub const AUDIO_TRACK_5: u32 = 1 << 4;
pub const AUDIO_TRACK_6: u32 = 1 << 5;
pub const AUDIO_TRACK_ALL: u32 = 0xFF; // Or calculate based on MAX_AUDIO_MIXES if defined elsewhere