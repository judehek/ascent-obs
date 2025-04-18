#pragma once
#include "obs_audio.h"
#include "advanced_output.h"
#include "obs_audio_source_control.h"
#include "obs_utils.h"

#include "util/dstr.h"
#include "util/windows/win-version.h"

#include "obs_audio_process_capture.h"

#include <unordered_set>
#include <sstream>

using namespace obs_control;

const char obs_control::kInputAudioSource[] = "wasapi_input_capture";
const char obs_control::kOutputAudioSource[] = "wasapi_output_capture";

const char kAudioCaptureSource[] = "audio_capture";
const char kAudioCaptureSourceNew[] = "wasapi_process_output_capture";

const int kOutputAudioChannelId = 1;
const int kInputAudioChannelId = 3;

const char* kDesktopDefaultAutioDeviceName = "ascentobs desktop device";
const char* kMicAutioDeviceName = "ascentobs mic";

const char* kAudioProccessPlugin = "audio_capture_process";
const char* kAudioSampleRate = "sample_rate";
const char* kAudioTracks = "tracks";
const char* kAudioSources = "audio_sources";
const char* kDefualtDesktopSourceName = "output_game";
const char* kDefualtMicSourceName = "input_mic";

const uint32_t kOutputTracks = settings::AudioTracksFlags::AudioTrack1 |
                               settings::AudioTracksFlags::AudioTrack2;
const uint32_t kInputTracks = settings::AudioTracksFlags::AudioTrack1 |
                              settings::AudioTracksFlags::AudioTrack3;

typedef std::unordered_set<std::string> DeviceList;

DeviceList obs_input_devices_ids;
DeviceList obs_output_devices_ids;

namespace {
// -----------------------------------------------------------------------------
void popluateDevicesId(DeviceList& list, const char* audio_source_name) {
  obs_properties_t* props = obs_get_source_properties(audio_source_name);
  if (!props) {
    return;
  }

  obs_property_t* device_ids = obs_properties_get(props, "device_id");
  size_t count = obs_property_list_item_count(device_ids);
  for (size_t i = 0; i < count; i++) {
    const char* val = obs_property_list_item_string(device_ids, i);
    list.insert(val);
  }

  obs_properties_destroy(props);
}

// -----------------------------------------------------------------------------
const char * getDeviceTypeFromDeviceId (const char* device_id, 
                                      bool* is_input_device = nullptr) {
  if (!device_id) {
    return nullptr;
  }

  if (obs_input_devices_ids.find(device_id) != obs_input_devices_ids.end()) {
    if (is_input_device) {
      *is_input_device = true;
    }
    return obs_control::kInputAudioSource;
  }

  if (obs_output_devices_ids.find(device_id) != obs_output_devices_ids.end()) {
    if (is_input_device) {
      *is_input_device = false;
    }
    return obs_control::kOutputAudioSource;
  }

  return nullptr;
}

// -----------------------------------------------------------------------------
void InitOBSAudioDevicesList() {
  if (obs_input_devices_ids.empty()) {
    popluateDevicesId(obs_input_devices_ids, kInputAudioSource);
  }

  if (obs_output_devices_ids.empty()) {
    popluateDevicesId(obs_output_devices_ids, kOutputAudioSource);
  }
}
} // namepsace

//------------------------------------------------------------------------------
OBSAudioControl::OBSAudioControl() {}

//------------------------------------------------------------------------------
OBSAudioControl::~OBSAudioControl() {}

//------------------------------------------------------------------------------
bool OBSAudioControl::IsGameAudioCaptureSuppored() {
  static bool is_applcation_audio_capture_supported = ([]() -> bool {
    struct win_version_info win19041 = {};
    win19041.major = 10;
    win19041.minor = 0;
    win19041.build = 19041;
    win19041.revis = 0;
    struct win_version_info ver;
    get_win_ver(&ver);
    return win_version_compare(&ver, &win19041) >= 0;
  }());

  return is_applcation_audio_capture_supported;
}

//------------------------------------------------------------------------------
bool OBSAudioControl::ResetAudio(OBSData& audio_settings) {
  settings::SetDefaultAudio(audio_settings);

  struct obs_audio_info ai;
  ai.samples_per_sec = (uint32_t)obs_data_get_int(
      audio_settings, settings::kSettingsAudioSampleRate);

  ai.speakers = SPEAKERS_STEREO;
  if (obs_data_get_bool(audio_settings, settings::kSettingsAudioMono)) {
    ai.speakers = SPEAKERS_MONO;
  }

  return obs_reset_audio(&ai);
}

//------------------------------------------------------------------------------
//"audio_settings": {
//  "output": {
//    "device_id": "default",
//    "volume" : 50
//  },
//  "input" : {
//    "device_id": "default",
//    "volume" : 50
//  },
//  "extra_options": {
//    "separate_tracks": true
//    "audio_capture_process: [] // for filter audio process
//  }
//}
void OBSAudioControl::InitAudioSources(OBSData& audio_settings,
                                       AdvancedOutput* advanced_output) {
  try {
    // MessageBoxA(NULL, "InitAudioSources", "ascent-obs", MB_OK);

    InitOBSAudioDevicesList();

    SET_OBS_DATA(
        audio_extra_options,
        obs_data_get_obj(audio_settings, settings::kSettingsExtraOptions));

    InitDefaultAudioSources(audio_settings, audio_extra_options);

    InitExtraAudioSources(audio_settings, audio_extra_options);

    SetAudioMixerTrack(audio_extra_options);

    if (advanced_output) {
      advanced_output->set_supported_tracks(active_tracks_);
    }

  } catch (...) {
    blog(LOG_ERROR, "Error init audio source!");
  }
}

//------------------------------------------------------------------------------
void OBSAudioControl::InitScene(obs_scene_t* scene, OBSData& audio_settings) {
  UNUSED_PARAMETER(audio_settings);

  if (audio_process_capture_control_) {
    audio_process_capture_control_->InitScene(scene);
  }

  for (auto& audio_source : process_audio_sources_v2_) {
    audio_source->AddToScene(scene);
  }

  for (auto& audio_source : audio_sources_) {
    audio_source.second->AddToScene(scene);    
  }

}

//------------------------------------------------------------------------------
//{
//  "cmd": 5,
//  "output" : {
//    "volume": 50
//  },
//  "input" : {
//    "volume": 50
//  }
//}
void OBSAudioControl::SetVolume(OBSData& volume_settings) {
  // settings::SetDefaultAudioSources(audio_settings);
  SET_OBS_DATA(
      output_settings,
      obs_data_get_obj(volume_settings, settings::kSettingsAudioOutput));
  SET_OBS_DATA(input_settings, obs_data_get_obj(volume_settings,
                                                settings::kSettingsAudioInput));

  if (obs_data_has_user_value(output_settings, "volume")) {
    int volume = (int)obs_data_get_int(output_settings, "volume");
    if (volume >= 0) { // -1 to ignore (client send it from 0.256.0.0) 
      if (dekstop_volume_control_.get()) {
        dekstop_volume_control_->SetVolume(volume);
      }

      if (audio_capture_volume_control_.get()) {
        audio_capture_volume_control_->SetVolume(volume);
      }

      if (audio_process_capture_control_.get()) {
        audio_process_capture_control_->SetVolume(volume);
      }
    }
  }

  if (obs_data_has_user_value(input_settings, "volume")) {
    int volume = (int)obs_data_get_int(input_settings, "volume");
    if (mic_volume_control_.get() && volume >= 0) {
      mic_volume_control_->SetVolume(volume);
    }
  }
}

//------------------------------------------------------------------------------
bool OBSAudioControl::InitDefaultAudioSourcesV2(OBSData& audio_extra_options) {
  if (!audio_extra_options.Get() ||
      !obs_data_has_user_value(audio_extra_options, kAudioSources)) {
    return false;
  }

  OBSDataArray audio_sources =
      obs_data_get_array(audio_extra_options, kAudioSources);
  size_t size = obs_data_array_count(audio_sources);

  blog(LOG_INFO, "init audio sources [%d] (V2) ", size);

  for (size_t i = 0; i < size; i++) {
    OBSData audio_source = obs_data_array_item(audio_sources, i);
    const char* device_id = obs_data_get_string(audio_source, "device_id");

    bool is_input_device = false;
    const char* device_type = nullptr;
    if (strcmp(device_id, "default") == 0) {
      is_input_device = obs_data_get_int(audio_source, "type") == 1;
      device_type = is_input_device ? obs_control::kInputAudioSource
                                    : obs_control::kOutputAudioSource;
    } else {
      device_type = getDeviceTypeFromDeviceId(device_id, &is_input_device);
    }

    if (!device_type) {
      blog(LOG_ERROR, "unknown device id '%s'", device_id);
      continue;      
    }

    if (obs_data_has_user_value(audio_source, "enable") &&
        !obs_data_get_bool(audio_source, "enable")) {      
        blog(LOG_INFO, "device id '%s' disabled", device_id);
        continue;      
    }

    AddAudioSource(audio_source, is_input_device, device_type);
  }

  return true;
}

//------------------------------------------------------------------------------
void OBSAudioControl::InitDefaultAudioSources(OBSData& audio_settings,
                                              OBSData& audio_settings_extra) {

  if (InitDefaultAudioSourcesV2(audio_settings_extra)) {
    return;
  }

  // settings::SetDefaultAudioSources(audio_settings);
  SET_OBS_DATA(
      output_settings,
      obs_data_get_obj(audio_settings, settings::kSettingsAudioOutput));

  SET_OBS_DATA(input_settings,
               obs_data_get_obj(audio_settings, settings::kSettingsAudioInput));


  //// NOTE(twolf): whether audio_settings or output_settings are null or not,
  /// we / still get a valid value for output_device (an empty string if one of
  /// them / is null) - so this saves a lot of checks (like if output_settings
  /// is null)
  ResetAudioDefaultDevice(kOutputAudioSource, false, kOutputAudioChannelId,
                          kDesktopDefaultAutioDeviceName,
                          output_settings, dekstop_volume_control_);


  
  ResetAudioDefaultDevice(kInputAudioSource, true, kInputAudioChannelId, kMicAutioDeviceName,
                          input_settings,
                          mic_volume_control_);

}

//------------------------------------------------------------------------------
void OBSAudioControl::ResetAudioDefaultDevice(const char* source_id,
                                              bool is_input_device,
                                              int channel,
                                              const char* device_desc,
                                              OBSData& audio_settings,
                                              OBSAudioSourceControlPtr& control) {


  if (!audio_settings.Get()) {
    blog(LOG_INFO, "No %s device, continue");
    return;
  }

  const char* device_id = obs_data_get_string(audio_settings, "device_id");
  int volume = 100;
  if (obs_data_has_user_value(audio_settings, "volume")) {
    volume = (int)obs_data_get_int(audio_settings, "volume");
  }

  bool mono = false;
  if (obs_data_has_user_value(audio_settings, "mono")) {
    mono = obs_data_get_bool(audio_settings, "mono");
  }

  bool disable = device_id && ((strcmp(device_id, "disabled") == 0) ||
                               (strlen(device_id) == 0));

  bool use_device_timing = is_input_device ? false : true;  
  if (volume == -1) {
    blog(LOG_INFO, "Skip disabled audio device '%s' [volume: %d]]", device_desc,
         volume);
    return;
  }

  obs_data_t* settings;
  OBSSource source = obs_get_output_source(channel);
  
  if (source) {  // existing
    if (disable) {
      blog(LOG_WARNING, "(update) Disable Audio device [%s]!",
           channel == kOutputAudioChannelId ? "Output" : "Input");

      obs_set_output_source(channel, nullptr);
    } else {
      settings = obs_source_get_settings(source);
      const char* oldId = obs_data_get_string(settings, "device_id");
      if ((!device_id && oldId) || (device_id && !oldId) ||
           strcmp(oldId, device_id) != 0) {
        obs_data_set_string(settings, "device_id", device_id);
        obs_data_set_bool(settings, "use_device_timing", use_device_timing);
        obs_source_update(source, settings);

        blog(LOG_INFO, "update Audio device [%s]: new device id - %s",
             !is_input_device ? "Output" : "Input", device_id);
      }
      obs_data_release(settings);
    }

  } else if (!disable) {  // new one
    settings = obs_data_create();
    obs_data_set_string(settings, "device_id", device_id);
    obs_data_set_bool(settings, "use_device_timing", use_device_timing);
    source = obs_source_create(source_id, device_desc, settings, nullptr);
    obs_data_release(settings);
    obs_set_output_source(channel, source);
    obs_source_release(source);

    blog(LOG_INFO, "Create Audio device [%s]: %s",
         !is_input_device ? "Output" : "Input", device_id);

  } else {  // disable
    blog(LOG_WARNING, "Disable Audio device [%s]!",
         !is_input_device ? "Output" : "Input");
  }

  if (!control.get()) {
    control.reset(new OBSAudioSourceControl(source, device_desc));
  }
  control->SetVolume(volume);
  control->SetMono(mono);
}

//------------------------------------------------------------------------------
void OBSAudioControl::InitExtraAudioSources(OBSData& audio_settings,
                                            OBSData& audio_extra_options) {
  UNREFERENCED_PARAMETER(audio_settings);
  // MessageBoxA(NULL, "WinMain", "ascent-obs", MB_OK);
  if (!audio_extra_options.Get()) {
    return;
  }

  auto sample_rate =
      (uint32_t)obs_data_get_int(audio_extra_options, kAudioSampleRate);
  SetSampleRate(sample_rate);

  OBSDataArray process_audio_capture_list =
      obs_data_get_array(audio_extra_options, "audio_capture_process2");
  
  CreateAudioCaptureSourceV2(process_audio_capture_list);

  auto* capture_audio_process_list =
      obs_data_get_string(audio_extra_options, kAudioProccessPlugin);

  if (!capture_audio_process_list || !strlen(capture_audio_process_list)) {
    StopGameCapture();
    return;
  }

  char** process_list = strlist_split(capture_audio_process_list, ';', false);
  if (process_list && CreateAudioCaptureSource(process_list)) {
    blog(LOG_INFO, "audio process to capture: %s", capture_audio_process_list);
  } else {
    StopGameCapture();
  }
  strlist_free(process_list);
}

//------------------------------------------------------------------------------
bool OBSAudioControl::StopGameCapture() {
  if (!audio_process_capture_control_.get()) {
    return true;
  }

  blog(LOG_INFO, "stop game audio capture");
  UpdateOuputDevices(false);
  return false;
}

//------------------------------------------------------------------------------
void OBSAudioControl::UpdateOuputDevices(bool is_game_audio_capture) {
  // when game_audio is on, we mute the desktop and an unmute
  if (audio_capture_volume_control_.get()) {
    audio_capture_volume_control_->SetMute(!is_game_audio_capture);
  }

  if (audio_process_capture_control_.get()) {
    audio_process_capture_control_->SetMute(!is_game_audio_capture);
  } 

  if (dekstop_volume_control_.get()) {
    dekstop_volume_control_->SetMute(is_game_audio_capture);
  }
}

// -----------------------------------------------------------------------------
bool OBSAudioControl::CreateAudioCaptureSource(char** process_list) {
  if (!IsGameAudioCaptureSuppored()) {
    blog(LOG_WARNING, "filter audio capture not supported!");
    return false;
  }

  auto* audio_capture_control = OBSAudioProcess::Create(process_list);
  if (!audio_capture_control) {
    return false;
  }
  audio_process_capture_control_.reset(audio_capture_control);

  if (dekstop_volume_control_.get()) {
    audio_process_capture_control_->SetMono(dekstop_volume_control_->is_mono());
    audio_process_capture_control_->SetVolume(
        dekstop_volume_control_->volume());
  }

  UpdateOuputDevices(true);
  return true;
}

// -----------------------------------------------------------------------------
void OBSAudioControl::CreateAudioCaptureSourceV2(
    obs_data_array_t* audio_capture_process) {
  if (!audio_capture_process) {
    return;
  }

  obs_data_array_enum(
      audio_capture_process,
      [](obs_data_t* item, void* __this) {
        OBSAudioControl* _this = (OBSAudioControl*)__this;
        _this->CreateAudioCaptureSourceItem(item);
      },
      (void*)this);
}

// -----------------------------------------------------------------------------
void OBSAudioControl::CreateAudioCaptureSourceItem(obs_data_t* item) {
  if (!item) {
    return;
  }

  auto* process_name = obs_data_get_string(item, "process_name");

  if (!process_name) {
    blog(LOG_WARNING, "Add game audio error. 'process_name' is missing");
    return;
  }

  auto enabled = obs_data_get_bool(item, "enable");
  if (!enabled) {
    blog(LOG_WARNING, "Skip adding game audio '%s' is disabled", process_name);
    return;
  }

  if (IsAudioProcessAlreadyCaptured(process_name)) {
    blog(LOG_INFO, "process audio '%s' already captured", process_name);
    return;
  }

  auto source = OBSAudioProcess::CreateAudioSource(process_name);
  if (!source || !source.get()) {
    return;
  }
  
  auto mono = obs_data_get_bool(item, "mono");
  
  uint32_t tracks = (uint32_t)obs_data_get_int(item, "tracks");
  int volume = (int)obs_data_get_int(item, "volume");
  
  uint32_t active_tracks = tracks != 0 ? tracks : kOutputTracks;
  source->SetMono(mono);
  source->SetVolume(volume);
  source->set_mixer_track(active_tracks);

  process_audio_sources_v2_.push_back(std::move(source));
  blog(LOG_WARNING, "Add audio process capture: '%s'", process_name);
}

// -----------------------------------------------------------------------------
void OBSAudioControl::SetAudioMixerTrack(OBSData& audio_settings_extra) {
  // from 254, we allow apps to control 'tracks' from the api
  if (SetAudioMixerTrackV2(audio_settings_extra)) {  
    return;
  }

  // no input device
   if (!mic_volume_control_.get()) {
    return;
  }

   bool separate_tracks =
          obs_data_get_bool(audio_settings_extra, "separate_tracks");

  if (!separate_tracks) {
    blog(LOG_INFO, "Separate audio tracks is disabled");
    return;
  }

  bool has_desktop_control =
      dekstop_volume_control_.get() && dekstop_volume_control_->has_source();
  
  bool has_output_device = 
    has_desktop_control ||
    audio_capture_volume_control_.get() ||
    audio_process_capture_control_.get();

  if (!has_output_device) {
    blog(LOG_INFO,
         "Separate audio tracks not supported (input or output is disabled)");
    return;
  }

  active_tracks_ = settings::AudioTracksFlags::AudioTrack1 |
                   settings::AudioTracksFlags::AudioTrack2 |
                   settings::AudioTracksFlags::AudioTrack3;


  if (dekstop_volume_control_) {
    dekstop_volume_control_->set_mixer_track(kOutputTracks);
  }

  if (audio_capture_volume_control_.get()) {
    audio_capture_volume_control_->set_mixer_track(kOutputTracks);
  }

  if (audio_process_capture_control_.get()) {
    audio_process_capture_control_->set_mixer_track(kOutputTracks);
  }

  if (mic_volume_control_.get()) {
    mic_volume_control_->set_mixer_track(kInputTracks);
  }

  blog(LOG_INFO, "Separate audio tracks is supported");
}

// -----------------------------------------------------------------------------
void OBSAudioControl::SetSampleRate(uint32_t sample_rate) {
  if (sample_rate == 0) {
    return;
  }

  if (sample_rate != 48000 && sample_rate != 44100) {
    blog(LOG_WARNING, "invalid audio sample rate: ", sample_rate);
    return;
  }

  struct obs_audio_info audio_info;
  if (!obs_get_audio_info(&audio_info)) {
    return;
  }

  if (audio_info.samples_per_sec == sample_rate) {
    return;
  }

  blog(LOG_INFO, "reset audio sample rate: %d", sample_rate);
  audio_info.samples_per_sec = sample_rate;
  if (!obs_reset_audio(&audio_info)) {
    blog(LOG_ERROR, "fail to reset audio sample rate: ", sample_rate);
  }
}

// -----------------------------------------------------------------------------
void OBSAudioControl::Shutdown() {

}

// -----------------------------------------------------------------------------
bool OBSAudioControl::SetAudioMixerTrackV2(OBSData& audio_extra_options) {
  if (!audio_extra_options.Get()) {
    return false;
  }
  
  if (!obs_data_has_user_value(audio_extra_options, kAudioTracks)) {
    return false;
  }

  auto tracks = (uint32_t)obs_data_get_int(
    audio_extra_options, kAudioTracks);
  if (tracks == 0) {
    return false;
  }

  active_tracks_ = tracks;
  std::vector<uint32_t> active_tracks;
  for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
    if ((active_tracks_ & (1 << i)) != 0) {
      active_tracks.push_back(1 + 1);
    }
  }

  blog(LOG_INFO, "set custom audio tracks %s",
       GetAudioTracksStr(active_tracks_).c_str());
  return true;
}

// -----------------------------------------------------------------------------
void OBSAudioControl::AddAudioSource(OBSData& audio_source,
                                     bool is_input_device,
                                     const char* device_type) {
  const char* device_id = obs_data_get_string(audio_source, "device_id");
  
  // set source name 
  std::string source_name;
  const char* name = obs_data_get_string(audio_source, "name");
  if (name && strlen(name) > 0) {
    source_name = name;
  } else {
    source_name = (is_input_device ? "input" : "output") +
                  std::to_string(audio_sources_.size() + 1);
  }

  int volume = 100;
  if (obs_data_has_user_value(audio_source, "volume")) {
    volume = (int)obs_data_get_int(audio_source, "volume");
  }

  bool mono = false;
  if (obs_data_has_user_value(audio_source, "mono")) {
    mono = obs_data_get_bool(audio_source, "mono");
  }

  bool use_device_timing = is_input_device ? false : true;
  if (obs_data_has_user_value(audio_source, "use_device_timing")) {
    use_device_timing = obs_data_get_bool(audio_source, "use_device_timing");

    blog(LOG_INFO, " device [%s] 'use_device_timing': %d", device_id,
         use_device_timing);
  }

  uint32_t tracks = settings::AudioTracksFlags::AudioTrackAll;
  if (obs_data_has_user_value(audio_source, "tracks")) {
    tracks = (uint32_t)obs_data_get_int(audio_source, "tracks");
  }

  auto existing = audio_sources_.find(source_name);
  OBSAudioSourceControlPtr control;

  if (existing == audio_sources_.end()) {
    CREATE_OBS_DATA(settings);
    obs_data_set_string(settings, "device_id", device_id);
    obs_data_set_bool(settings, "use_device_timing", use_device_timing);
    OBSSource source =
        obs_source_create(device_type, source_name.c_str(), settings, nullptr);

    obs_source_release(source);
    control.reset(new OBSAudioSourceControl(source, name));
    audio_sources_[source_name] = control;

    if (strcmp(source_name.c_str(), kDefualtDesktopSourceName) == 0) {
      dekstop_volume_control_ = control;
    } else if (strcmp(source_name.c_str(), kDefualtMicSourceName) == 0) {
      mic_volume_control_ = control;
    }
    blog(LOG_INFO,
         "Add new audio source id:%s name:%s type:%s mono:%d volume:%d "
         "use-device-timing:%d "
         "tracks:%d ",
         device_id, source_name.c_str(), is_input_device ? "input" : "output",
         mono, volume, use_device_timing, tracks);
  } else { // update e 
    blog(LOG_INFO,
         "Update audio source id:%s name:%s type:%s mono:%d volume:%d "
         "use-device-timing:%d "
         "tracks:%d ",
         device_id, source_name.c_str(), is_input_device ? "input" : "output",
         mono, volume, use_device_timing, tracks);   
    control = existing->second;
  }

  if (!control.get()) {
    blog(LOG_ERROR, "no audio control for %s", source_name.c_str());
    return;
  }

  control->SetVolume(volume);
  control->set_mixer_track(tracks);
  control->SetMono(mono);
}

// -----------------------------------------------------------------------------
bool OBSAudioControl::IsAudioProcessAlreadyCaptured(const char* process_name) {
  for (auto& iter : process_audio_sources_v2_) {
    obs_source_t* audio_source = iter->audio_source();
    if (!audio_source) {
      continue;
    }

    OBSDataAutoRelease outputSettings = obs_source_get_settings(audio_source);
    auto audio_process_name = obs_data_get_string(outputSettings, OPT_PROCESS);
    if (!audio_process_name) {
      continue;
    }
    
    if (_stricmp(process_name, audio_process_name) == 0) {
      return true;
    }
  }

  return false;
}
