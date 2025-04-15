/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#include "obs_control/advanced_output.h"

#include <algorithm>
#include <util/platform.h>
#include <util/dstr.h>
#include <util/util.hpp>
#include "obs_control/settings.h"
#include "audio-encoders.hpp"

#include <communications/protocol.h>
#include "obs_control/obs_utils.h"
#include "obs_control/obs_control_communications.h"

#include "obs_control/obs_audio_source_control.h"

namespace obs_control {
const int kAudioMixes = 6;
const char kRecVideoEncoderName[] = "h264_recording";

const char kErrorMissingEncoderId[] = "missing encoder id";
const char kErrorUnsupportedEncoder[] = "unsupported encoder (%s) id passed";
const char kErrorCreateRecordingOutput[] = "failed to create recording output";
const char kErrorCreateRecordingEncoder[] = "failed creating h264 encoder";
const char kErrorCreateAudioEncoder[] = "failed to create audio encoder";
const char kErrorMissingFilename[] = "missing filename field";
const char kErrorFailedToInitReplay[] = "failed to init replay";
const char kErrorFailedToStartReplayAlreadyRunning[] = "failed to start replay. other replay already running";
const char kErrorAlreadyRecording[] = "recording is already active";


//------------------------------------------------------------------------------
static bool CreateAACEncoder(OBSEncoder &res,
                             std::string &id, int bitrate,
                             const char *name,
                             size_t idx) {
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


static void convert_nvenc_h264_presets(obs_data_t* data) {
  const char* preset = obs_data_get_string(data, "preset");
  const char* rc = obs_data_get_string(data, "rate_control");

  // If already using SDK10+ preset, return early.
  if (astrcmpi_n(preset, "p", 1) == 0) {
    obs_data_set_string(data, "preset2", preset);
    blog(LOG_INFO, "already using SDK10+ preset");
    return;
  }

  if (astrcmpi(rc, "lossless") == 0 && astrcmpi(preset, "mq")) {
    obs_data_set_string(data, "preset2", "p3");
    obs_data_set_string(data, "tune", "lossless");
    obs_data_set_string(data, "multipass", "disabled");

  } else if (astrcmpi(rc, "lossless") == 0 && astrcmpi(preset, "hp")) {
    obs_data_set_string(data, "preset2", "p2");
    obs_data_set_string(data, "tune", "lossless");
    obs_data_set_string(data, "multipass", "disabled");

  } else if (astrcmpi(preset, "mq") == 0) {
    obs_data_set_string(data, "preset2", "p5");
    obs_data_set_string(data, "tune", "hq");
    obs_data_set_string(data, "multipass", "qres");

  } else if (astrcmpi(preset, "hq") == 0) {
    obs_data_set_string(data, "preset2", "p5");
    obs_data_set_string(data, "tune", "hq");
    obs_data_set_string(data, "multipass", "disabled");

  } else if (astrcmpi(preset, "default") == 0) {
    obs_data_set_string(data, "preset2", "p3");
    obs_data_set_string(data, "tune", "hq");
    obs_data_set_string(data, "multipass", "disabled");

  } else if (astrcmpi(preset, "hp") == 0) {
    obs_data_set_string(data, "preset2", "p1");
    obs_data_set_string(data, "tune", "hq");
    obs_data_set_string(data, "multipass", "disabled");

  } else if (astrcmpi(preset, "ll") == 0 || astrcmpi(preset, "lossless") == 0) {
    obs_data_set_string(data, "preset2", "p3");
    obs_data_set_string(data, "tune", "ll");
    obs_data_set_string(data, "multipass", "disabled");

  } else if (astrcmpi(preset, "llhq") == 0) {
    obs_data_set_string(data, "preset2", "p4");
    obs_data_set_string(data, "tune", "ll");
    obs_data_set_string(data, "multipass", "disabled");

  } else if (astrcmpi(preset, "llhp") == 0) {
    obs_data_set_string(data, "preset2", "p2");
    obs_data_set_string(data, "tune", "ll");
    obs_data_set_string(data, "multipass", "disabled");
  }
}

static void convert_nvenc_hevc_presets(obs_data_t* data) {
  const char* preset = obs_data_get_string(data, "preset");
  const char* rc = obs_data_get_string(data, "rate_control");

  // If already using SDK10+ preset, return early.
  if (astrcmpi_n(preset, "p", 1) == 0) {
    obs_data_set_string(data, "preset2", preset);
    blog(LOG_INFO, "already using SDK10+ preset");
    return;
  }

  if (astrcmpi(rc, "lossless") == 0 && astrcmpi(preset, "mq")) {
    obs_data_set_string(data, "preset2", "p5");
    obs_data_set_string(data, "tune", "lossless");
    obs_data_set_string(data, "multipass", "disabled");

  } else if (astrcmpi(rc, "lossless") == 0 && astrcmpi(preset, "hp")) {
    obs_data_set_string(data, "preset2", "p3");
    obs_data_set_string(data, "tune", "lossless");
    obs_data_set_string(data, "multipass", "disabled");

  } else if (astrcmpi(preset, "mq") == 0) {
    obs_data_set_string(data, "preset2", "p6");
    obs_data_set_string(data, "tune", "hq");
    obs_data_set_string(data, "multipass", "qres");

  } else if (astrcmpi(preset, "hq") == 0) {
    obs_data_set_string(data, "preset2", "p6");
    obs_data_set_string(data, "tune", "hq");
    obs_data_set_string(data, "multipass", "disabled");

  } else if (astrcmpi(preset, "default") == 0) {
    obs_data_set_string(data, "preset2", "p5");
    obs_data_set_string(data, "tune", "hq");
    obs_data_set_string(data, "multipass", "disabled");

  } else if (astrcmpi(preset, "hp") == 0) {
    obs_data_set_string(data, "preset2", "p1");
    obs_data_set_string(data, "tune", "hq");
    obs_data_set_string(data, "multipass", "disabled");

  } else if (astrcmpi(preset, "ll") == 0 || astrcmpi(preset, "lossless") == 0) {
    obs_data_set_string(data, "preset2", "p3");
    obs_data_set_string(data, "tune", "ll");
    obs_data_set_string(data, "multipass", "disabled");

  } else if (astrcmpi(preset, "llhq") == 0) {
    obs_data_set_string(data, "preset2", "p4");
    obs_data_set_string(data, "tune", "ll");
    obs_data_set_string(data, "multipass", "disabled");

  } else if (astrcmpi(preset, "llhp") == 0) {
    obs_data_set_string(data, "preset2", "p2");
    obs_data_set_string(data, "tune", "ll");
    obs_data_set_string(data, "multipass", "disabled");
  }
}

static void convert_28_1_encoder_setting(const char* encoder,
                                         obs_data_t* data) {
  bool modify = false;
  if (astrcmpi(encoder, "jim_nvenc") == 0 ||
      astrcmpi(encoder, "ffmpeg_nvenc") == 0) {
    if (obs_data_has_user_value(data, "preset") &&
        !obs_data_has_user_value(data, "preset2")) {
      convert_nvenc_h264_presets(data);
      modify = true;
      blog(LOG_INFO, "convert nvenc encoder setting to new obs!");
    }
  } else if (astrcmpi(encoder, "jim_hevc_nvenc") == 0 ||
             astrcmpi(encoder, "ffmpeg_hevc_nvenc") == 0) {
    if (obs_data_has_user_value(data, "preset") &&
        !obs_data_has_user_value(data, "preset2")) {
      convert_nvenc_hevc_presets(data);
      modify = true;
    }
  }

  if (!modify) {
    return;
  }

  blog(LOG_INFO, 
      "convert nvenc encoder setting to new version:\n"
      "\tpreset:       %s\n"
      "\ttuning:       %s\n"
      "\tmultipass:    %s\n",
       obs_data_get_string(data, "preset2"),
       obs_data_get_string(data, "tune"),
       obs_data_get_string(data, "multipass"));

}
};  // namespace obs_control

using namespace obs_control;
using namespace libowobs;

//------------------------------------------------------------------------------
AdvancedOutput::AdvancedOutput(AdvancedOutputDelegate* delegate) :
  delegate_(delegate),
  uses_bitrate_(false){
}

//------------------------------------------------------------------------------
AdvancedOutput::~AdvancedOutput() {
  if (record_output_.get()) {
    record_output_->DisconnectSignals();
  }

  StopReplay(true);
  StopRecording(true);
  StopStreaming(true);
  record_output_.reset(nullptr);
  replay_output_.reset(nullptr);
  stream_output_.reset(nullptr);
}

//------------------------------------------------------------------------------
//static
AdvancedOutput* AdvancedOutput::Create(AdvancedOutputDelegate* delegate,
                                       OBSData& video_encoder_settings,
                                       OBSData& error_result) {
  AdvancedOutput* advanced_output = new AdvancedOutput(delegate);
  if (!advanced_output->Initialize(video_encoder_settings,
                                   error_result)) {
    delete advanced_output;
    return nullptr;
  }

  return advanced_output;
}

//------------------------------------------------------------------------------
bool AdvancedOutput::Initialize(OBSData& video_encoder_settings,
                                OBSData& error_result) {
  std::string encoder_id = obs_data_get_string(video_encoder_settings, "id");
  if (encoder_id.empty()) {
    blog(LOG_ERROR, kErrorMissingEncoderId);
    obs_data_set_int(error_result,
                     protocol::kErrorCodeField,
                     protocol::events::INIT_ERROR_MISSING_PARAM);
    return false;
  }

  if (!IsValidVideoEncoder(encoder_id.c_str())) {
    blog(LOG_ERROR, kErrorUnsupportedEncoder, encoder_id.c_str());
    obs_data_set_int(error_result,
                     protocol::kErrorCodeField,
                     protocol::events::INIT_ERROR_UNSUPPORTED_VIDEO_ENCODER);
    return false;
  }

  const char* rate_control =
    obs_data_get_string(video_encoder_settings, "rate_control");
  if (!rate_control) {
    rate_control = "";
  }

  uses_bitrate_ = astrcmpi(rate_control, "CBR") == 0 ||
                  astrcmpi(rate_control, "VBR") == 0 ||
                  astrcmpi(rate_control, "ABR") == 0;

  if (astrcmpi(obs_data_get_string(video_encoder_settings, "preset"),
               "lossless") == 0) {
    obs_data_set_string(video_encoder_settings, "preset", "ll");
    blog(LOG_INFO, "fix 'lossless' preset");
  }

  convert_28_1_encoder_setting(encoder_id.c_str(), video_encoder_settings);

  // create video encoder
  recording_video_encoder_ = obs_video_encoder_create(encoder_id.c_str(),
                                                      "recording_h264",
                                                      video_encoder_settings,
                                                      nullptr);
  if (!recording_video_encoder_) {
    blog(LOG_ERROR, kErrorCreateRecordingEncoder);
    obs_data_set_int(error_result,
                     protocol::kErrorCodeField,
                     protocol::events::INIT_ERROR_FAILED_CREATING_VID_ENCODER);
    return false;
  }
  obs_encoder_release(recording_video_encoder_);

  // create audio encoders
  for (int i = 0; i < kAudioMixes; i++) {
    char name[9];
    sprintf(name, "adv_aac%d", i);

    if (!CreateAACEncoder(aacTrack[i],
                          aacEncoderID[i],
                          GetAudioBitrate(i),
                          name,
                          i)) {
      blog(LOG_ERROR, kErrorCreateAudioEncoder);
      obs_data_set_int(error_result,
                      protocol::kErrorCodeField,
                      protocol::events::INIT_ERROR_FAILED_CREATING_AUD_ENCODER);
      return false;
    }
  }

  system_game_info_ = obs_data_create();
  get_system_game_info(system_game_info_);

  return true;
}

//------------------------------------------------------------------------------
void AdvancedOutput::Update() {
  UpdateAudioSettings();
}

//------------------------------------------------------------------------------
inline void AdvancedOutput::SetupRecording() {
  //obs_data_t *settings = obs_data_create();
  unsigned int cx = 0;
  unsigned int cy = 0;

  if (!recording_video_encoder_) {
    blog(LOG_ERROR, "setup encoder: no video encoder!");
    return;
  }

  size_t sleep_counter = 0;
  while (obs_encoder_active(recording_video_encoder_) &&
         sleep_counter++ < 200) {
    blog(LOG_WARNING, "setup encoder: video encoder still active");
    Sleep(10);
  }

  blog(LOG_INFO, "setup video encoder");
  obs_encoder_set_scaled_size(recording_video_encoder_, cx, cy);
  obs_encoder_set_video(recording_video_encoder_, obs_get_video());
  
}

//------------------------------------------------------------------------------
static inline void SetEncoderName(obs_encoder_t *encoder,
                                  const char *name,
                                  const char *defaultName) {
  obs_encoder_set_name(encoder, (name && *name) ? name : defaultName);
}

//------------------------------------------------------------------------------
inline void AdvancedOutput::UpdateAudioSettings() {
  obs_data_t* settings[kAudioMixes];

  for (size_t i = 0; i < kAudioMixes; i++) {
    settings[i] = obs_data_create();
    obs_data_set_int(settings[i], "bitrate", GetAudioBitrate(i));
  }

  for (size_t i = 0; i < kAudioMixes; i++) {
    std::string def_name = "Track";
    def_name += std::to_string((int)i + 1);
    SetEncoderName(aacTrack[i], nullptr, def_name.c_str());
  }

  for (size_t i = 0; i < kAudioMixes; i++) {
    obs_encoder_update(aacTrack[i], settings[i]);
    obs_data_release(settings[i]);
  }
}

//------------------------------------------------------------------------------
void AdvancedOutput::SetupOutputs() {
  for (size_t i = 0; i < kAudioMixes; i++) {
    bool log = false;
    size_t sleep_counter = 0;
    while (obs_encoder_active(aacTrack[i]) && sleep_counter++ < 200) {
      blog(LOG_WARNING, "setup outputs [%d]: audio encoder still active", i);
      Sleep(10);
      log = true;
    }

    obs_encoder_set_audio(aacTrack[i], obs_get_audio());

    if (log) {
      blog(LOG_INFO, "set audio encoder outputs [%d]", i);
    }
  }

  SetupRecording();
}

//------------------------------------------------------------------------------
int AdvancedOutput::GetAudioBitrate(size_t i) const {
  UNUSED_PARAMETER(i);

  static const char *names[] = {
    "Track1Bitrate", "Track2Bitrate",
    "Track3Bitrate", "Track4Bitrate",
    "Track5Bitrate", "Track6Bitrate",
  };
  int bitrate = 160;
  return FindClosestAvailableAACBitrate(bitrate);
}

//------------------------------------------------------------------------------
bool AdvancedOutput::ResetOutputSetting(OBSData& output_settings,
                                        OBSData& audio_setting,
                                        OBSData& error_result) {

  if (RecorderActive()) {
    blog(LOG_ERROR, kErrorAlreadyRecording);
    obs_data_set_int(error_result,
      protocol::kErrorCodeField,
      protocol::events::INIT_ERROR_CURRENTLY_ACTIVE);
    return false;
  }

  if (!Active()) {
    UpdateAudioSettings();
    SetupOutputs();
  }

  // create file output
    // splitter muxer ?
  auto file_size_bytes = obs_data_get_int(output_settings,
    protocol::kMaxFileSizeField);

  auto max_time_sec = obs_data_get_int(output_settings, "max_time_sec");

  bool on_demand_split_enabled = obs_data_get_bool(output_settings,
    protocol::kEnableOnDemandSplitField);

  bool support_split_muxer =
      (on_demand_split_enabled || file_size_bytes > 0 || max_time_sec > 0);

  const char* file_outout_id = "ffmpeg_muxer";

  bool separate_tracks = settings::GetAudioExtraParam(
    audio_setting, "separate_tracks");

  blog(LOG_INFO,
       "selected recording muxer: %s [max size: %d, max time:%d, manual split enabled: %d, separate tracks: %d]",
       file_outout_id, file_size_bytes, max_time_sec, on_demand_split_enabled,
       separate_tracks);

  obs_output_t* file_output = file_output_;
  if (file_output == nullptr) {
    file_output_ = obs_output_create(file_outout_id,
      "Overwolf Output",
      nullptr,
      nullptr);

    if (!file_output_) {
      blog(LOG_ERROR, kErrorCreateRecordingOutput);
      obs_data_set_int(error_result,
        protocol::kErrorCodeField,
        protocol::events::INIT_ERROR_FAILED_CREATING_OUTPUT_FILE);
      return false;
    }

    obs_output_release(file_output_);

    record_output_.reset(new RecordOutput(this));
    record_output_->Initialize(file_output_, error_result);
  }

  obs_output_set_video_encoder(file_output_, recording_video_encoder_);

  int idx = 0;
  auto tracks = GetOutputTracks("Recording", separate_tracks);
  for (int i = 0; i < kAudioMixes; i++) {
    if ((tracks & (1 << i)) != 0) {
      obs_output_set_audio_encoder(file_output_, aacTrack[i], idx);
      idx++;
    }
  }

  blog(LOG_INFO, "output active tracks %d (%d)", tracks, idx);

  const char* path = obs_data_get_string(output_settings,
                                         protocol::kFilenameField);
  if (path == nullptr) {
    blog(LOG_ERROR, kErrorMissingFilename);
    obs_data_set_int(error_result,
                     protocol::kErrorCodeField,
                     protocol::events::INIT_ERROR_MISSING_PARAM);
    return false;
  }

  obs_data_t* settings = obs_data_create();
  obs_data_set_string(settings, "path", path);

  if (support_split_muxer) {
    if (file_size_bytes >0 ) {
      obs_data_set_int(settings, "max_size_mb",
                       file_size_bytes / (long long)(1024 * 1024));
    }
    if (max_time_sec > 0) {
      obs_data_set_int(settings, "max_time_sec", max_time_sec);
    }
    obs_data_set_bool(settings, "manual_split_enabled", on_demand_split_enabled);
    obs_data_set_bool(settings, "split_file", true);

    auto include_full_size_video = obs_data_get_bool(output_settings,
      "include_full_video");

    obs_data_set_bool(settings, "include_full_video", include_full_size_video);
  }

  applay_fragmented_file(settings);

  obs_output_update(file_output_, settings);
  obs_data_release(settings);

  blog(LOG_INFO, "reset video options %s", path);
  return true;
}

//------------------------------------------------------------------------------
bool AdvancedOutput::StartRecording(int identifier, OBSData& error_result) {
  if (record_output_.get() && record_output_->Active() &&
      identifier != record_output_->identifier()) {
    blog(LOG_ERROR, kErrorFailedToStartReplayAlreadyRunning);
    return false;
  }

  if (!record_output_) {
    record_output_.reset(new RecordOutput(this));
    if (!record_output_->Initialize(file_output_, error_result)) {
      blog(LOG_ERROR, kErrorFailedToInitReplay);
      return false;
    }
  }

  return record_output_->Start(identifier, error_result);
}

//------------------------------------------------------------------------------
bool AdvancedOutput::StartReplay(int identifier,
                                 OBSData& settings,
                                 OBSData& replay_settings,
                                 OBSData& error_result,
                                 bool force_start) {
  if (replay_output_.get() && replay_output_->Running()) {
    blog(LOG_ERROR, kErrorFailedToStartReplayAlreadyRunning);
    obs_data_set_int(error_result,
                      protocol::kErrorCodeField,
                      protocol::events::INIT_ERROR_CURRENTLY_ACTIVE);
    return false;
  }

  if (!Active()) {
    UpdateAudioSettings();
    SetupOutputs();
  }

  if (!replay_output_) {
    replay_output_.reset(new ReplayOutput(this));
    if (!replay_output_->Initialize(error_result)) {
      blog(LOG_ERROR, kErrorFailedToInitReplay);
      return false;
    }
  }

  return replay_output_->Start(
    identifier, settings, replay_settings, error_result, force_start);
}

//------------------------------------------------------------------------------
bool AdvancedOutput::StartStreaming(int identifier,
                                    OBSData& streaming_settings,
                                    OBSData& error_result) {
  if (stream_output_.get() && stream_output_->Active()) {
    blog(LOG_ERROR, kErrorFailedToStartReplayAlreadyRunning);
    // TODO: set error
    return false;
  }

  if (!stream_output_) {
    auto stream_type = obs_data_get_string(streaming_settings, "type");
    stream_output_.reset(new StreamOutput(this));
    if (!stream_output_->Initialize(error_result, stream_type)) {
      blog(LOG_ERROR, kErrorFailedToInitReplay);
      return false;
    }
  }

  if (!Active()) {
    UpdateAudioSettings();
    SetupOutputs();
  }

  return stream_output_->Start(identifier, streaming_settings, error_result);
}

//------------------------------------------------------------------------------
bool AdvancedOutput::StartReplay(OBSData& error_result) {
  if (!replay_output_) {
    blog(LOG_ERROR, "replay output doesn't exists");
    return false;
  }
  return replay_output_->Start(error_result);
}

//------------------------------------------------------------------------------
void AdvancedOutput::StopRecording(bool force) {
  if (!record_output_) {
    return;
  }

  record_output_->Stop(force);
}

//------------------------------------------------------------------------------
void AdvancedOutput::StopReplay(bool force) {
 if (!replay_output_.get()) {
    //blog(LOG_ERROR, "replay output doesn't exists");
    return;
  }

  replay_output_->Stop(force);
}

//------------------------------------------------------------------------------
void AdvancedOutput::StopStreaming(bool force) {
 if (!stream_output_.get()) {
    //blog(LOG_ERROR, "streaming output doesn't exists");
    return;
  }

 stream_output_->Stop(force);
}

//------------------------------------------------------------------------------
void AdvancedOutput::SplitVideo() {
  if (!record_output_.get()) {
    //blog(LOG_ERROR, "streaming output doesn't exists");
    return;
  }
  record_output_->SplitVideo();
}

//------------------------------------------------------------------------------
bool AdvancedOutput::StartCaptureReplay(OBSData& data, OBSData& error_result) {
  if (!replay_output_) {
    blog(LOG_ERROR, "replay output doesn't exists");
    return false;
  }

  return replay_output_->StartCaptureReplay(data, error_result);
}

//------------------------------------------------------------------------------
bool AdvancedOutput::StopCaptureReplay(OBSData& data, OBSData& error_result) {
  if (!replay_output_) {
    blog(LOG_ERROR, "replay output doesn't exists");
    return false;
  }

  return replay_output_->StopCaptureReplay(data, error_result);
}

//------------------------------------------------------------------------------
bool AdvancedOutput::RecordingActive() const {
  if (!record_output_) {
    return false;
  }
  return record_output_->Active();
}

//------------------------------------------------------------------------------
bool AdvancedOutput::ReplayActive() const {
  if (!replay_output_)
    return false;

  return replay_output_->Active();
}

//------------------------------------------------------------------------------
bool AdvancedOutput::StreamActive() const {
  if (!stream_output_)
    return false;

  return stream_output_->Active();
}

//------------------------------------------------------------------------------
void AdvancedOutput::set_supported_tracks(uint32_t audio_tracks) {
  if (supported_tracks_ == audio_tracks) {
    return;
  }

  supported_tracks_ = audio_tracks;
  supported_tracks_ |= settings::AudioTracksFlags::AudioTrack1;

  blog(LOG_INFO, "supported audio tracks: 0x%x (%d) %s", supported_tracks_,
       settings::GetSupportedAudioTracksCount(supported_tracks_),
       obs_control::GetAudioTracksStr(supported_tracks_).c_str());
}


//------------------------------------------------------------------------------
int AdvancedOutput::GetOutputTracks(const char* output_type,
                                    bool separate_tracks) {

  if (separate_tracks) {
    uint32_t separate_tracks_ids = (settings::AudioTracksFlags::AudioTrack2 |
                                    settings::AudioTracksFlags::AudioTrack3);

    supported_tracks_ |= separate_tracks_ids;    
    blog(LOG_INFO, "('%s') apply separate tracks for output (2,3)", output_type);
  }

  
   blog(LOG_INFO, "output '%s' active tracks: %s [%d]", output_type,
       GetAudioTracksStr(supported_tracks_).c_str(), supported_tracks_);
  return supported_tracks_;
}

//------------------------------------------------------------------------------
bool AdvancedOutput::IsValidVideoEncoder(const char* encoder_id) {
  static const char* encoders[] = {
    protocol::kVideoEncoderId_x264,
    protocol::kVideoEncoderId_QuickSync,
    protocol::kVideoEncoderId_QuickSync_HEVC,
    protocol::kVideoEncoderId_QuickSync_AV1,
    protocol::kVideoEncoderId_AMF,
    protocol::kVideoEncoderId_AMF_HEVC,
    protocol::kVideoEncoderId_NVENC,
    protocol::kVideoEncoderId_NVENC_NEW,
    protocol::kVideoEncoderId_NVENC_HEVC,
    protocol::kVideoEncoderId_NVENC_AV1,
    protocol::kVideoEncoderId_AMF_AV1,
    nullptr
  };

  for (int i = 0; encoders[i] != nullptr; ++i) {
    if (strncmp(encoder_id, encoders[i], strlen(encoders[i])) == 0) {
      return true;
    }
  }

  return false;
}

//------------------------------------------------------------------------------
void AdvancedOutput::applay_fragmented_file(obs_data_t* settings) {
  if (!settings) {
    return;
  }

  if (!set_fragmented_file_) {
    blog(LOG_WARNING, "*** fragmented file is disabled ***");
    return;
  }

  std::string mux_frag = "movflags=frag_keyframe+empty_moov+delay_moov";
  obs_data_set_string(settings, "muxer_settings", mux_frag.c_str());
  blog(LOG_INFO, "enable fragmented video file");
}
