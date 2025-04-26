
#ifndef LIBASCENTOBS_COMMUNICATIONS_PROTOCOL_H_
#define LIBASCENTOBS_COMMUNICATIONS_PROTOCOL_H_

namespace libascentobs {
namespace protocol {

extern const char kCommandField[];
extern const char kCommandIdentifier[];
extern const char kTypeField[];
extern const char kEventField[];
extern const char kErrorCodeField[];
extern const char kErrorDescField[];
extern const char kFilenameField[];
extern const char kMaxFileSizeField[];
extern const char kEnableOnDemandSplitField[];
extern const char kIncludeFullVideoField[];
extern const char kStatsDataField[];

extern const char kAudioInputDevices[];
extern const char kAudioOutputDevices[];
extern const char kVideoEncoders[];
extern const char kWinrtCaptureSupported[];
extern const char KIsWindowCapture[];

// known video encoders
extern const char kVideoEncoderId_x264[];
extern const char kVideoEncoderId_QuickSync[];
extern const char kVideoEncoderId_QuickSync_HEVC[];
extern const char kVideoEncoderId_QuickSync_AV1[];
extern const char kVideoEncoderId_AMF[];
extern const char kVideoEncoderId_AMF_HEVC[];
extern const char kVideoEncoderId_AMF_AV1[];
extern const char kVideoEncoderId_NVENC[];
extern const char kVideoEncoderId_NVENC_NEW[];
extern const char kVideoEncoderId_NVENC_HEVC[];
extern const char kVideoEncoderId_NVENC_AV1[];

namespace commands {
enum Commands {
  SHUTDOWN = 1,
  QUERY_MACHINE_INFO = 2,
  START = 3,
  STOP = 4,
  SET_VOLUME = 5,
  GAME_FOCUS_CHANGED = 6,
  ADD_GAME_SOURCE,
  START_REPLAY_CAPTURE,
  STOP_REPLAY_CAPTURE,
  TOBII_GAZE,
  SET_BRB,
  SPLIT_VIDEO
};

namespace recorderType {
enum RecorderType {
  VIDEO = 1,
  REPLAY = 2,
  STREAMING = 3,
};
};
};

namespace events {

enum InitErrors {
  INIT_ERROR_CURRENTLY_ACTIVE = -1,
  INIT_ERROR_FAILED_TO_INIT = -2,
  INIT_ERROR_FAILED_TO_CREATE_SCENE = -3,
  INIT_ERROR_FAILED_TO_CREATE_SOURCES = -4,
  INIT_ERROR_MISSING_PARAM = -5,
  INIT_ERROR_UNSUPPORTED_VIDEO_ENCODER = -6,
  INIT_ERROR_FAILED_CREATING_OUTPUT_FILE = -7,
  INIT_ERROR_FAILED_CREATING_VID_ENCODER = -8,
  INIT_ERROR_FAILED_CREATING_AUD_ENCODER = -9,
  INIT_ERROR_FAILED_STARTING_UPDATE_DRIVER_ERROR = -10,
  INIT_ERROR_FAILED_CREATING_OUTPUT_ALREADY_CREATED = -11,
  INIT_ERROR_FAILED_CREATING_OUTPUT_SIGNALS = -12,
  INIT_ERROR_FAILED_STARTING_OUTPUT_ALREADY_RUNNING = -13,
  INIT_ERROR_FAILED_UNSUPPORTED_RECORDING_TYPE = -14,
  INIT_ERROR_REPLAY_START_ERROR = -15,
  INIT_ERROR_STREAM_START_NO_SERVICE_ERROR = -16,
  INIT_ERROR_FAILED_STARTING_OUTPUT_WITH_OBS_ERROR = -17,
  INIT_ERROR_GAME_INJECTION_ERROR = -18,
};

enum ReplayError {
  REPLAY_ERROR_OFFLINE = -1,
  REPLAY_ERROR_START_CAPTURE_OBS_ERROR = -2,
  REPLAY_ERROR_START_CAPTURE_ALREADY_CAPTURING = -3,
  REPLAY_ERROR_STOP_CAPTURE_NO_CAPTURE  = -4,
  REPLAY_ERROR_STOP_CAPTURE_OBS_ERROR = -5,
  REPLAY_ERROR_REPLAY_OBS_ERROR = -6,
  REPLAY_ERROR_REPLAY_OFFLINE_DELAY = -7,
};

// taken from obs-defs.h
enum OutputErrors {
  OUTPUT_SUCCESS = 0,
  OUTPUT_BAD_PATH = -1,
  OUTPUT_CONNECT_FAILED = -2,
  OUTPUT_INVALID_STREAM = -3,
  OUTPUT_ERROR = -4,
  OUTPUT_DISCONNECTED = -5,
  OUTPUT_UNSUPPORTED = -6,
  OUTPUT_NO_SPACE = -7,
  OUTPUT_ENCODE_ERROR = -8
};

enum VideoErrors {
  VIDEO_SUCCESS = 0,
  VIDEO_FAIL = -1,
  VIDEO_NOT_SUPPORTED = -2,
  VIDEO_INVALID_PARAM = -3,
  VIDEO_CURRENTLY_ACTIVE = -4,
  VIDEO_MODULE_NOT_FOUND = -5
};

enum Events {
  QUERY_MACHINE_INFO = 1,
  ERR = 2,
  READY = 3,
  RECORDING_STARTED = 4,
  RECORDING_STOPPING = 5,
  RECORDING_STOPPED = 6,
  DISPLAY_SOURCE_CHANGED,
  VIDEO_FILE_SPLIT,
  REPLAY_STARTED,
  REPLAY_STOPPING,
  REPLAY_STOPPED,
  REPLAY_ARMED,
  REPLAY_CAPTURE_VIDEO_STARTED,
  REPLAY_CAPTURE_VIDEO_READY,
  REPLAY_ERROR,
  STREAMING_STARTING,
  STREAMING_STARTED,
  STREAMING_STOPPING,
  STREAMING_STOPPED,
  SWITCHABLE_DEVICE_DETECTED,
  OBS_WARNING
};

};

};
};

#endif // LIBASCENTOBS_COMMUNICATIONS_PROTOCOL_H_