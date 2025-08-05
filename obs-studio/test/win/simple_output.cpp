#include "simple_output.h"
#include <algorithm>
#include <util/platform.h>
#include <util/dstr.h>
#include <util/util.hpp>
#include "audio-encoders.hpp"
#include <windows.h>
#include <chrono>
#include <ctime>

#define SIMPLE_ENCODER_X264                    "x264"
#define SIMPLE_ENCODER_X264_LOWCPU             "x264_lowcpu"
#define SIMPLE_ENCODER_QSV                     "qsv"
#define SIMPLE_ENCODER_NVENC                   "nvenc"
#define SIMPLE_ENCODER_AMD                     "amd"


static bool CreateAACEncoder(OBSEncoder &res, string &id, int bitrate,
  const char *name, size_t idx) {
  const char *id_ = GetAACEncoderForBitrate(bitrate);

  if (!id_) {
    id.clear();
    res = nullptr;
    return false;
  }

  if (id == id_)
    return true;

  id = id_;
  res = obs_audio_encoder_create(id_, name, nullptr, idx, nullptr);

  if (res) {
    obs_encoder_release(res);
    return true;
  }

  return false;
}

string GenerateSpecifiedFilename(const char *extension, bool noSpace,
  const char *format) {
  BPtr<char> filename = os_generate_formatted_filename(extension,
    !noSpace, format);
  return string(filename);
}


static void FindBestFilename(string &strPath, bool noSpace) {
  int num = 2;

  if (!os_file_exists(strPath.c_str()))
    return;

  const char *ext = strrchr(strPath.c_str(), '.');
  if (!ext)
    return;

  int extStart = int(ext - strPath.c_str());
  for (;;) {
    string testPath = strPath;
    string numStr;

    numStr = noSpace ? "_" : " (";
    numStr += to_string(num++);
    if (!noSpace)
      numStr += ")";

    testPath.insert(extStart, numStr);

    if (!os_file_exists(testPath.c_str())) {
      strPath = testPath;
      break;
    }
  }
}

static void OBSStartRecording(void *data, calldata_t *params) {
  SimpleOutput *output = static_cast<SimpleOutput*>(data);
  output->recordingActive = true;

  UNUSED_PARAMETER(params);
}

static void OBSStopRecording(void *data, calldata_t *params) {

  SimpleOutput *output = static_cast<SimpleOutput*>(data);
  obs_output_t *obj = (obs_output_t*)calldata_ptr(params, "output");
  int code = (int)calldata_int(params, "code");
  auto last_error = calldata_string(params, "last_error");
  output->_didStopped = true;
  output->recordingActive = false;;

}

static void OBSRecordStopping(void *data, calldata_t *params) {
  SimpleOutput *output = static_cast<SimpleOutput*>(data);
  //obs_output_t *obj = (obs_output_t*)calldata_ptr(params, "output");
  //int sec = (int)obs_output_get_active_delay(obj);
  UNUSED_PARAMETER(params);
}

static void OBSVideoSplit(void *data, calldata_t *params) {
  SimpleOutput *output = static_cast<SimpleOutput*>(data);

  const char* path;
  calldata_get_string(params, "path", &path);

  int64_t duration = 0;
  calldata_get_int(params, "duration", &duration);

  //obs_output_t *obj = (obs_output_t*)calldata_ptr(params, "output");
  //int sec = (int)obs_output_get_active_delay(obj);
  UNUSED_PARAMETER(params);
}

static void OBSStreamStopping(void *data, calldata_t *params) {
  SimpleOutput *output = static_cast<SimpleOutput*>(data);
  obs_output_t *obj = (obs_output_t*)calldata_ptr(params, "output");
  int sec = (int)obs_output_get_active_delay(obj);
}

static void OBSStartStreaming(void *data, calldata_t *params) {
  SimpleOutput *output = static_cast<SimpleOutput*>(data);
  output->streamingActive = true;
  UNUSED_PARAMETER(params);
}

static void OBSStopStreaming(void *data, calldata_t *params) {
  SimpleOutput *output = static_cast<SimpleOutput*>(data);
  int code = (int)calldata_int(params, "code");
  const char *last_error = calldata_string(params, "last_error");
  output->streamingActive = false;
  output->delayActive = false;
}

static void OBSStartReplayBuffer(void *data, calldata_t *params) {
  UNUSED_PARAMETER(params);
}

static void OBSStopReplayBuffer(void *data, calldata_t *params) {
}

static void OBSReplayBufferStopping(void *data, calldata_t *params) {

}
using Clock = std::chrono::high_resolution_clock;
template<class Duration>
using TimePoint = std::chrono::time_point<Clock, Duration>;

static void OBSReplayVideoReady(void *data, calldata_t *params) {
  auto time = std::chrono::system_clock::now();
  const char* path;
  calldata_get_string(params, "path", &path);
#define NANO_SECS_INTERVAL_UNITS_BETWEEN_1601_AND_1970   116444736000000000
  static const __int64 kMillisecondsPerSecond = 1000;
  static const __int64 kMicrosecondsPerMillisecond = 1000;
  static const __int64 kMicrosecondsPerSecond = kMicrosecondsPerMillisecond * kMillisecondsPerSecond;
  int64_t system_start_time = calldata_int(params, "system_start_time");

  int64_t origin_start_time = system_start_time;
  system_start_time -= NANO_SECS_INTERVAL_UNITS_BETWEEN_1601_AND_1970; // we start at 1601 and above
  system_start_time /= 10; // 100-nanosecs to microsecs
  system_start_time /= kMicrosecondsPerMillisecond;

  //1519031483

  //std::time_t epoch_time = std::chrono::system_clock::to_time_t(time_point_sec);
  //std::cout << "epoch: " << std::ctime(&epoch_time);
  //std::time_t today_time = std::chrono::system_clock::to_time_t(p2);
  //std::cout << "today: " << std::ctime(&today_time);


  //std::chrono::time_point<std::chrono::system_clock> pa = (std::chrono::system_clock)system_start_time;
 /* auto a= std::chrono::nanoseconds(system_start_time);
  
  std::chrono::system_clock::from_time_t(a);
  auto time_t = std::chrono::high_resolution_clock::from_time_t(a);*/

 /* #define NANO_SECS_INTERVAL_UNITS_BETWEEN_1601_AND_1970   116444736000000000
  system_start_time += NANO_SECS_INTERVAL_UNITS_BETWEEN_1601_AND_1970;*/
//
//  __int64 temp;
//  memcpy((void*)&temp, (const void*)&system_start_time, sizeof(FILETIME));
//
//  static const __int64 kMillisecondsPerSecond = 1000;
//  static const __int64 kMicrosecondsPerMillisecond = 1000;
//  static const __int64 kMicrosecondsPerSecond = kMicrosecondsPerMillisecond * kMillisecondsPerSecond;
//#define NANO_SECS_INTERVAL_UNITS_BETWEEN_1601_AND_1970   116444736000000000
//  temp -= NANO_SECS_INTERVAL_UNITS_BETWEEN_1601_AND_1970; // we start at 1601 and above
//  temp /= 10; // 100-nanosecs to microsecs
//  temp /= kMicrosecondsPerMillisecond;
//
//  FILETIME replay_start_time;
//  // Copy the result back into the FILETIME structure.
//  replay_start_time.dwLowDateTime = (DWORD)(system_start_time & 0xFFFFFFFF);
//  replay_start_time.dwHighDateTime = (DWORD)(system_start_time >> 32);
//
//  SYSTEMTIME st = { 0 };
//  FileTimeToSystemTime(&replay_start_time, &st);
//  wchar_t buffer_st[1024];
//  wsprintf(buffer_st,
//    L"%d-%02d-%02d %02d:%02d:%02d.%03d",
//    st.wYear,
//    st.wMonth,
//    st.wDay,
//    st.wHour,
//    st.wMinute,
//    st.wSecond,
//    st.wMilliseconds
//    );
//


  calldata_get_string(params, "path", &path);
  int64_t duration_usec = (int64_t)calldata_int(params, "duration");
  int64_t duration_ms = duration_usec / 1000;
}

static void OBSReplayVideoError(void *data, calldata_t *params) {
  const char* path;
  calldata_get_string(params, "path",  &path);
  const char* error = NULL;
  calldata_get_string(params, "error", &error);
}
static void OBSReplayArmed(void *data, calldata_t *params) {
  obs_output* replay_output;
  calldata_get_ptr(params, "output", &replay_output);
}

SimpleOutput::~SimpleOutput() {
  startRecording.Disconnect();
  stopRecording.Disconnect();
  recordStopping.Disconnect();
  startReplayBuffer.Disconnect();
  stopReplayBuffer.Disconnect();
  replayBufferStopping.Disconnect();
  replayReady.Disconnect();
  replayError.Disconnect();
  replayArmed.Disconnect();
  VideoSpillted.Disconnect();
}

SimpleOutput::SimpleOutput(const char *encoder):
  _didStopped(false),
  _encoder(encoder){

  if (strcmp(encoder, SIMPLE_ENCODER_QSV) == 0)
    LoadStreamingPreset_h264("obs_qsv11");
  else if (strcmp(encoder, SIMPLE_ENCODER_AMD) == 0)
    LoadStreamingPreset_h264("amd_amf_h264");
  else if (strcmp(encoder, SIMPLE_ENCODER_NVENC) == 0)
    LoadStreamingPreset_h264("ffmpeg_nvenc");
  else
    LoadStreamingPreset_h264("obs_x264");

  if (!CreateAACEncoder(aacStreaming, aacStreamEncID, GetAudioBitrate(),
    "simple_aac", 0))
    throw "Failed to create aac streaming encoder (simple output)";

  LoadRecordingPreset();

  if (!ffmpegOutput) {
    bool useReplayBuffer = true;//
    if (useReplayBuffer) {
      //const char *str = config_get_string(main->Config(),
      //  "Hotkeys", "ReplayBuffer");
      //obs_data_t *hotkey = obs_data_create_from_json(str);
      replayBuffer = obs_output_create("replay_buffer", "ReplayBuffer", nullptr, nullptr);

//      obs_data_release(hotkey);
      if (!replayBuffer)
        throw "Failed to create replay buffer output "
        "(simple output)";
      obs_output_release(replayBuffer);

      signal_handler_t *signal =
        obs_output_get_signal_handler(replayBuffer);

      startReplayBuffer.Connect(signal, "start",
        OBSStartReplayBuffer, this);
      stopReplayBuffer.Connect(signal, "stop",
        OBSStopReplayBuffer, this);
      replayBufferStopping.Connect(signal, "stopping",
        OBSReplayBufferStopping, this);

      replayReady.Connect(signal, "replay_ready",
        OBSReplayVideoReady, this);
      replayError.Connect(signal, "replay_error",
        OBSReplayVideoError, this);
      replayArmed.Connect(signal, "replay_fully_armed",
        OBSReplayArmed, this);

    }

    /*fileOutput = obs_output_create("ffmpeg_muxer",
      "simple_file_output", nullptr, nullptr);  */
    fileOutput = obs_output_create("ffmpeg_muxer_splitter",
      "simple_file_output", nullptr, nullptr);
   
    if (!fileOutput)
      throw "Failed to create recording output "
      "(simple output)";

    obs_output_release(fileOutput);
  }

  startRecording.Connect(obs_output_get_signal_handler(fileOutput),
    "start", OBSStartRecording, this);
  stopRecording.Connect(obs_output_get_signal_handler(fileOutput),
    "stop", OBSStopRecording, this);
  recordStopping.Connect(obs_output_get_signal_handler(fileOutput),
    "stopping", OBSRecordStopping, this);
  VideoSpillted.Connect(obs_output_get_signal_handler(fileOutput), 
    "video_split", OBSVideoSplit, this);
}

int SimpleOutput::GetAudioBitrate() const {
  int bitrate = 160;

  return 160;// FindClosestAvailableAACBitrate(bitrate);
}

void SimpleOutput::Update() {
  obs_data_t *h264Settings = obs_data_create();
  obs_data_t *aacSettings = obs_data_create();

  int videoBitrate = 2500;//
  int audioBitrate = GetAudioBitrate();
  bool advanced = true;
  bool enforceBitrate = true;
  const char *custom = NULL;
  const char *encoder = _encoder.c_str();

  const char *presetType;
  const char *preset;

  if (strcmp(encoder, SIMPLE_ENCODER_QSV) == 0) {
    presetType = "QSVPreset";
    preset = "balanced";

  } else if (strcmp(encoder, SIMPLE_ENCODER_AMD) == 0) {
    presetType = "AMDPreset";
    UpdateStreamingSettings_amd(h264Settings, videoBitrate);
    preset = "balanced";

  } else if (strcmp(encoder, SIMPLE_ENCODER_NVENC) == 0) {
    presetType = "NVENCPreset";

  } else {
    presetType = "Preset";
    preset = "ultrafast";
  }

  //preset = config_get_string(main->Config(), "SimpleOutput", presetType);

  obs_data_set_string(h264Settings, "rate_control", "CBR");
  obs_data_set_int(h264Settings, "bitrate", videoBitrate);

  if (advanced) {
    obs_data_set_string(h264Settings, "preset", preset);
    obs_data_set_string(h264Settings, "x264opts", custom);
  }

  obs_data_set_string(aacSettings, "rate_control", "CBR");
  obs_data_set_int(aacSettings, "bitrate", audioBitrate);

  auto service = GetService();
  if (service) {
    obs_service_apply_encoder_settings(service,
      h264Settings, aacSettings);
  }

  if (advanced && !enforceBitrate) {
    obs_data_set_int(h264Settings, "bitrate", videoBitrate);
    obs_data_set_int(aacSettings, "bitrate", audioBitrate);
  }

  video_t *video = obs_get_video();
  enum video_format format = video_output_get_format(video);

  if (format != VIDEO_FORMAT_NV12 && format != VIDEO_FORMAT_I420) {
    obs_encoder_set_preferred_video_format(h264Streaming,
      VIDEO_FORMAT_NV12);
  }

  obs_encoder_update(h264Streaming, h264Settings);
  obs_encoder_update(aacStreaming, aacSettings);

  obs_data_release(h264Settings);
  obs_data_release(aacSettings);
}

void SimpleOutput::UpdateRecordingAudioSettings() {
  obs_data_t *settings = obs_data_create();
  obs_data_set_int(settings, "bitrate", 192);
  obs_data_set_string(settings, "rate_control", "CBR");

  obs_encoder_update(aacRecording, settings);

  obs_data_release(settings);
}

#define CROSS_DIST_CUTOFF 2000.0

int SimpleOutput::CalcCRF(int crf) {
  int cx = 1920;
    //config_get_uint(main->Config(), "Video", "OutputCX");
  int cy = 1080;
    //config_get_uint(main->Config(), "Video", "OutputCY");

  double fCX = double(cx);
  double fCY = double(cy);

  if (lowCPUx264)
    crf -= 2;

  double crossDist = sqrt(fCX * fCX + fCY * fCY);
  double crfResReduction =
    fmin(CROSS_DIST_CUTOFF, crossDist) / CROSS_DIST_CUTOFF;
  crfResReduction = (1.0 - crfResReduction) * 10.0;

  return crf - int(crfResReduction);
}

void SimpleOutput::UpdateRecordingSettings_x264_crf(int crf) {
  obs_data_t *settings = obs_data_create();
  obs_data_set_int(settings, "crf", crf);
  obs_data_set_bool(settings, "use_bufsize", true);
  obs_data_set_string(settings, "rate_control", "CRF");
  obs_data_set_string(settings, "profile", "high");
  obs_data_set_string(settings, "preset",
    lowCPUx264 ? "ultrafast" : "veryfast");

  obs_encoder_update(h264Recording, settings);

  obs_data_release(settings);
}

static bool icq_available(obs_encoder_t *encoder) {
  obs_properties_t *props = obs_encoder_properties(encoder);
  obs_property_t *p = obs_properties_get(props, "rate_control");
  bool icq_found = false;

  size_t num = obs_property_list_item_count(p);
  for (size_t i = 0; i < num; i++) {
    const char *val = obs_property_list_item_string(p, i);
    if (strcmp(val, "ICQ") == 0) {
      icq_found = true;
      break;
    }
  }

  obs_properties_destroy(props);
  return icq_found;
}

void SimpleOutput::UpdateRecordingSettings_qsv11(int crf) {
  bool icq = icq_available(h264Recording);

  obs_data_t *settings = obs_data_create();
  obs_data_set_string(settings, "profile", "high");

  if (icq) {
    obs_data_set_string(settings, "rate_control", "ICQ");
    obs_data_set_int(settings, "icq_quality", crf);
  } else {
    obs_data_set_string(settings, "rate_control", "CQP");
    obs_data_set_int(settings, "qpi", crf);
    obs_data_set_int(settings, "qpp", crf);
    obs_data_set_int(settings, "qpb", crf);
  }

  obs_encoder_update(h264Recording, settings);

  obs_data_release(settings);
}

void SimpleOutput::UpdateRecordingSettings_nvenc(int cqp) {
  obs_data_t *settings = obs_data_create();
  obs_data_set_string(settings, "rate_control", "CQP");
  obs_data_set_string(settings, "profile", "high");
  obs_data_set_string(settings, "preset", "hq");
  obs_data_set_int(settings, "cqp", cqp);

  obs_encoder_update(h264Recording, settings);

  obs_data_release(settings);
}

void SimpleOutput::UpdateStreamingSettings_amd(obs_data_t *settings,
  int bitrate) {
  // Static Properties
  obs_data_set_int(settings, "Usage", 0);
  obs_data_set_int(settings, "Profile", 100); // High

                                              // Rate Control Properties
  obs_data_set_int(settings, "RateControlMethod", 3);
  obs_data_set_int(settings, "Bitrate.Target", bitrate);
  obs_data_set_int(settings, "FillerData", 1);
  obs_data_set_int(settings, "VBVBuffer", 1);
  obs_data_set_int(settings, "VBVBuffer.Size", bitrate);

  // Picture Control Properties
  obs_data_set_double(settings, "KeyframeInterval", 1.0);
  obs_data_set_int(settings, "BFrame.Pattern", 0);
}

void SimpleOutput::UpdateRecordingSettings_amd_cqp(int cqp) {
  obs_data_t *settings = obs_data_create();

  // Static Properties
  obs_data_set_int(settings, "Usage", 0);
  obs_data_set_int(settings, "Profile", 100); // High

                                              // Rate Control Properties
  obs_data_set_int(settings, "RateControlMethod", 0);
  obs_data_set_int(settings, "QP.IFrame", cqp);
  obs_data_set_int(settings, "QP.PFrame", cqp);
  obs_data_set_int(settings, "QP.BFrame", cqp);
  obs_data_set_int(settings, "VBVBuffer", 1);
  obs_data_set_int(settings, "VBVBuffer.Size", 100000);

  // Picture Control Properties
  obs_data_set_double(settings, "KeyframeInterval", 1.0);
  obs_data_set_int(settings, "BFrame.Pattern", 0);

  // Update and release
  obs_encoder_update(h264Recording, settings);
  obs_data_release(settings);
}

void SimpleOutput::UpdateRecordingSettings() {
  bool ultra_hq = (videoQuality == "HQ");
  int crf = CalcCRF(ultra_hq ? 16 : 23);

  if (astrcmp_n(videoEncoder.c_str(), "x264", 4) == 0) {
    UpdateRecordingSettings_x264_crf(crf);

  } else if (videoEncoder == SIMPLE_ENCODER_QSV) {
    UpdateRecordingSettings_qsv11(crf);

  } else if (videoEncoder == SIMPLE_ENCODER_AMD) {
    UpdateRecordingSettings_amd_cqp(crf);

  } else if (videoEncoder == SIMPLE_ENCODER_NVENC) {
    UpdateRecordingSettings_nvenc(crf);
  }
}

inline void SimpleOutput::SetupOutputs() {
  SimpleOutput::Update();
  obs_encoder_set_video(h264Streaming, obs_get_video());
  obs_encoder_set_audio(aacStreaming, obs_get_audio());

  if (usingRecordingPreset) {
    if (ffmpegOutput) {
      obs_output_set_media(fileOutput, obs_get_video(),
        obs_get_audio());
    } else {
      obs_encoder_set_video(h264Recording, obs_get_video());
      obs_encoder_set_audio(aacRecording, obs_get_audio());
    }
  }
}

const char *FindAudioEncoderFromCodec(const char *type) {
  const char *alt_enc_id = nullptr;
  size_t i = 0;

  while (obs_enum_encoder_types(i++, &alt_enc_id)) {
    const char *codec = obs_get_encoder_codec(alt_enc_id);
    if (strcmp(type, codec) == 0) {
      return alt_enc_id;
    }
  }

  return nullptr;
}

bool SimpleOutput::StartStreaming(obs_service_t *service) {
  //if (!Active())
  //  SetupOutputs();

  ///* --------------------- */

  //const char *type = obs_service_get_output_type(service);
  //if (!type)
  //  type = "rtmp_output";

  ///* XXX: this is messy and disgusting and should be refactored */
  //if (outputType != type) {
  //  streamDelayStarting.Disconnect();
  //  streamStopping.Disconnect();
  //  startStreaming.Disconnect();
  //  stopStreaming.Disconnect();

  //  streamOutput = obs_output_create(type, "simple_stream",
  //    nullptr, nullptr);
  //  if (!streamOutput) {
  //    blog(LOG_WARNING, "Creation of stream output type '%s' "
  //      "failed!", type);
  //    return false;
  //  }
  //  obs_output_release(streamOutput);

  ///*  streamDelayStarting.Connect(
  //    obs_output_get_signal_handler(streamOutput),
  //    "starting", OBSStreamStarting, this);
  //  streamStopping.Connect(
  //    obs_output_get_signal_handler(streamOutput),
  //    "stopping", OBSStreamStopping, this);

  //  startStreaming.Connect(
  //    obs_output_get_signal_handler(streamOutput),
  //    "start", OBSStartStreaming, this);
  //  stopStreaming.Connect(
  //    obs_output_get_signal_handler(streamOutput),
  //    "stop", OBSStopStreaming, this);*/

  //  const char *codec =
  //    obs_output_get_supported_audio_codecs(streamOutput);
  //  if (!codec) {
  //    return false;
  //  }

  //  if (strcmp(codec, "aac") != 0) {
  //    const char *id = FindAudioEncoderFromCodec(codec);
  //    int audioBitrate = GetAudioBitrate();
  //    obs_data_t *settings = obs_data_create();
  //    obs_data_set_int(settings, "bitrate", audioBitrate);

  //    aacStreaming = obs_audio_encoder_create(id,
  //      "alt_audio_enc", nullptr, 0, nullptr);
  //    obs_encoder_release(aacStreaming);
  //    if (!aacStreaming)
  //      return false;

  //    obs_encoder_update(aacStreaming, settings);
  //    obs_encoder_set_audio(aacStreaming, obs_get_audio());

  //    obs_data_release(settings);
  //  }

  //  outputType = type;
  //}

  //obs_output_set_video_encoder(streamOutput, h264Streaming);
  //obs_output_set_audio_encoder(streamOutput, aacStreaming, 0);
  //obs_output_set_service(streamOutput, service);

  ///* --------------------- */

  //bool reconnect = config_get_bool(main->Config(), "Output",
  //  "Reconnect");
  //int retryDelay = config_get_uint(main->Config(), "Output",
  //  "RetryDelay");
  //int maxRetries = config_get_uint(main->Config(), "Output",
  //  "MaxRetries");
  //bool useDelay = config_get_bool(main->Config(), "Output",
  //  "DelayEnable");
  //int delaySec = config_get_int(main->Config(), "Output",
  //  "DelaySec");
  //bool preserveDelay = config_get_bool(main->Config(), "Output",
  //  "DelayPreserve");
  //const char *bindIP = config_get_string(main->Config(), "Output",
  //  "BindIP");
  //bool enableNewSocketLoop = config_get_bool(main->Config(), "Output",
  //  "NewSocketLoopEnable");
  //bool enableLowLatencyMode = config_get_bool(main->Config(), "Output",
  //  "LowLatencyEnable");

  //obs_data_t *settings = obs_data_create();
  //obs_data_set_string(settings, "bind_ip", bindIP);
  //obs_data_set_bool(settings, "new_socket_loop_enabled",
  //  enableNewSocketLoop);
  //obs_data_set_bool(settings, "low_latency_mode_enabled",
  //  enableLowLatencyMode);
  //obs_output_update(streamOutput, settings);
  //obs_data_release(settings);

  //if (!reconnect)
  //  maxRetries = 0;

  //obs_output_set_delay(streamOutput, useDelay ? delaySec : 0,
  //  preserveDelay ? OBS_OUTPUT_DELAY_PRESERVE : 0);

  //obs_output_set_reconnect_settings(streamOutput, maxRetries,
  //  retryDelay);

  //if (obs_output_start(streamOutput)) {
  //  return true;
  //}

  //const char *error = obs_output_get_last_error(streamOutput);
  //bool has_last_error = error && *error;

  //blog(LOG_WARNING, "Stream output type '%s' failed to start!%s%s",
  //  type,
  //  has_last_error ? "  Last Error: " : "",
  //  has_last_error ? error : "");
  return false;
}

static void remove_reserved_file_characters(string &s) {
  replace(s.begin(), s.end(), '/', '_');
  replace(s.begin(), s.end(), '\\', '_');
  replace(s.begin(), s.end(), '*', '_');
  replace(s.begin(), s.end(), '?', '_');
  replace(s.begin(), s.end(), '"', '_');
  replace(s.begin(), s.end(), '|', '_');
  replace(s.begin(), s.end(), ':', '_');
  replace(s.begin(), s.end(), '>', '_');
  replace(s.begin(), s.end(), '<', '_');
}

static void ensure_directory_exists(string &path) {
  replace(path.begin(), path.end(), '\\', '/');

  size_t last = path.rfind('/');
  if (last == string::npos)
    return;

  string directory = path.substr(0, last);
  os_mkdirs(directory.c_str());
}

void SimpleOutput::UpdateRecording(bool has_audio) {
  if (replayBufferActive || recordingActive)
    return;

  if (usingRecordingPreset) {
    if (!ffmpegOutput)
      UpdateRecordingSettings();
  } else if (!obs_output_active(streamOutput)) {
    Update();
  }

  if (!Active())
    SetupOutputs();

  if (!ffmpegOutput) {
    obs_output_set_video_encoder(fileOutput, h264Recording);
    if (has_audio) {
      obs_output_set_audio_encoder(fileOutput, aacRecording, 0);
    }
  }
  if (replayBuffer) {
    obs_output_set_video_encoder(replayBuffer, h264Recording);
    if (has_audio) {
      obs_output_set_audio_encoder(replayBuffer, aacRecording, 0);
    }
  }

  recordingConfigured = true;
}

bool SimpleOutput::ConfigureRecording(bool updateReplayBuffer) {
  const char *path = "C://Users//elad.bahar//Videos";
  const char *format = "mp4";
  const char *mux = nullptr;
  bool noSpace = false;
  const char *filenameFormat = "%CCYY-%MM-%DD %hh-%mm-%ss";
  bool overwriteIfExists = false;
  const char *rbPrefix = "Replay";
  const char *rbSuffix = nullptr;
  int rbTime = 20;
  int rbSize = 500;

  os_dir_t *dir = path && path[0] ? os_opendir(path) : nullptr;

  if (!dir) {
    return false;
  }

  os_closedir(dir);

  string strPath;
  strPath += path;

  char lastChar = strPath.back();
  if (lastChar != '/' && lastChar != '\\')
    strPath += "/";

  strPath += GenerateSpecifiedFilename(ffmpegOutput ? "avi" : format,
    noSpace, filenameFormat);
  ensure_directory_exists(strPath);
  if (!overwriteIfExists)
    FindBestFilename(strPath, noSpace);

  obs_data_t *settings = obs_data_create();
  if (updateReplayBuffer) {
    string f;

    if (rbPrefix && *rbPrefix) {
      f += rbPrefix;
      if (f.back() != ' ')
        f += " ";
    }

    f += filenameFormat;

    if (rbSuffix && *rbSuffix) {
      if (*rbSuffix != ' ')
        f += " ";
      f += rbSuffix;
    }

    remove_reserved_file_characters(f);

    obs_data_set_string(settings, "directory", path);
    obs_data_set_string(settings, "format", f.c_str());
    obs_data_set_string(settings, "extension", format);
    obs_data_set_int(settings, "max_time_sec", rbTime);
    obs_data_set_int(settings, "max_size_mb",
      usingRecordingPreset ? rbSize : 0);
  } else {
    obs_data_set_string(settings, ffmpegOutput ? "url" : "path",
      strPath.c_str());
  }

  obs_data_set_string(settings, "muxer_settings", mux);

  obs_data_set_int(settings, "split_size_bytes", 1000* (1024 * 1024)); // 5 MB

  if (updateReplayBuffer)
    obs_output_update(replayBuffer, settings);
  else
    obs_output_update(fileOutput, settings);

  obs_data_release(settings);
  return true;
}

bool SimpleOutput::StartRecording(bool has_audio) {
  UpdateRecording(has_audio);
  if (!ConfigureRecording(false))
    return false;
  if (!obs_output_start(fileOutput)) {
    //QString error_reason;
    //const char *error = obs_output_get_last_error(fileOutput);
    //if (error) {
    //  error_reason = QT_UTF8(error);
    //} else {
    //  error_reason = QTStr("Output.StartFailedGeneric");
    //}
    return false;
  }

  return true;
}

bool SimpleOutput::StartReplayBuffer() {
  UpdateRecording();
  if (!ConfigureRecording(true))
    return false;
  if (!obs_output_start(replayBuffer)) {
    return false;
  }

  return true;
}

void SimpleOutput::StopStreaming(bool force) {
  if (force)
    obs_output_force_stop(streamOutput);
  else
    obs_output_stop(streamOutput);
}

void SimpleOutput::StopRecording(bool force) {
  if (force)
    obs_output_force_stop(fileOutput);
  else
    obs_output_stop(fileOutput);
}

void SimpleOutput::StopReplayBuffer(bool force) {
  if (force)
    obs_output_force_stop(replayBuffer);
  else
    obs_output_stop(replayBuffer);
}

bool SimpleOutput::StreamingActive() const {
  return obs_output_active(streamOutput);
}

bool SimpleOutput::RecordingActive() const {
  return obs_output_active(fileOutput);
}

bool SimpleOutput::ReplayBufferActive() const {
  return obs_output_active(replayBuffer);
}

OBSService SimpleOutput::GetService() {
  if (service != nullptr)
    return service;
  service = obs_service_create("rtmp_common", "default_service", nullptr,
    nullptr);
  if (!service)
    return nullptr;
  obs_service_release(service);

  return service;
}

void SimpleOutput::LoadRecordingPreset_Lossless() {
  fileOutput = obs_output_create("ffmpeg_output",
    "simple_ffmpeg_output", nullptr, nullptr);
  if (!fileOutput)
    throw "Failed to create recording FFmpeg output "
    "(simple output)";
  obs_output_release(fileOutput);

  obs_data_t *settings = obs_data_create();
  obs_data_set_string(settings, "format_name", "avi");
  obs_data_set_string(settings, "video_encoder", "utvideo");
  obs_data_set_string(settings, "audio_encoder", "pcm_s16le");

  obs_output_update(fileOutput, settings);
  obs_data_release(settings);
}

void SimpleOutput::LoadStreamingPreset_h264(const char *encoderId) {
  h264Streaming = obs_video_encoder_create(encoderId,
    "simple_h264_stream", nullptr, nullptr);
  if (!h264Streaming)
    throw "Failed to create h264 streaming encoder (simple output)";
  obs_encoder_release(h264Streaming);
}

void SimpleOutput::LoadRecordingPreset_h264(const char *encoderId) {
  h264Recording = obs_video_encoder_create(encoderId,
    "simple_h264_recording", nullptr, nullptr);
  if (!h264Recording)
    throw "Failed to create h264 recording encoder (simple output)";
  obs_encoder_release(h264Recording);
}


void SimpleOutput::LoadRecordingPreset() {
  const char *quality = "Stream";
  const char *encoder = "x264";

  videoEncoder = encoder;
  videoQuality = quality;
  ffmpegOutput = false;

  if (strcmp(quality, "Stream") == 0) {
    h264Recording = h264Streaming;
    aacRecording = aacStreaming;
    usingRecordingPreset = false;
    return;

  } else if (strcmp(quality, "Lossless") == 0) {
    LoadRecordingPreset_Lossless();
    usingRecordingPreset = true;
    ffmpegOutput = true;
    return;

  } else {
    lowCPUx264 = false;

    if (strcmp(encoder, SIMPLE_ENCODER_X264) == 0) {
      LoadRecordingPreset_h264("obs_x264");
    } else if (strcmp(encoder, SIMPLE_ENCODER_X264_LOWCPU) == 0) {
      LoadRecordingPreset_h264("obs_x264");
      lowCPUx264 = true;
    } else if (strcmp(encoder, SIMPLE_ENCODER_QSV) == 0) {
      LoadRecordingPreset_h264("obs_qsv11");
    } else if (strcmp(encoder, SIMPLE_ENCODER_AMD) == 0) {
      LoadRecordingPreset_h264("amd_amf_h264");
    } else if (strcmp(encoder, SIMPLE_ENCODER_NVENC) == 0) {
      LoadRecordingPreset_h264("ffmpeg_nvenc");
    }
    usingRecordingPreset = true;

    if (!CreateAACEncoder(aacRecording, aacRecEncID, 192,
      "simple_aac_recording", 0))
      throw "Failed to create aac recording encoder "
      "(simple output)";
  }
}

