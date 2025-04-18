/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#include "obs_control/commands/command_start.h"
#include "obs_control/obs_utils.h"
#include "obs_control/obs.h"
#include "obs_control/settings.h"

using namespace obs_control;
using namespace libowobs;
using namespace settings;

static bool was_ever_initialized = false; // first initialization

//------------------------------------------------------------------------------
CommandStart::CommandStart(OBS* obs,
                           OBSControlCommunications* communications) :
  Command(obs, communications),
  already_running_(false) {
}

//------------------------------------------------------------------------------
CommandStart::~CommandStart() {
}

//------------------------------------------------------------------------------
// virtual
void CommandStart::Perform(int identifier, OBSData& data) {
  //MessageBox(NULL, L"DASDSA", L"DASDSA", 0);

  // initialize the error result (just in case we get an error)
  CREATE_OBS_DATA(error_result);
  try {
    obs_data_set_int(error_result, protocol::kCommandIdentifier, identifier);
    //  MessageBoxA(0, "Perform Start", "Failed start replays", 0);

    already_running_ = __super::obs_->IsActive();

    if (!already_running_) {
      if (!this->InitializeOBS(data, error_result)) {
        __super::communications_->Send(protocol::events::ERR, error_result);
        return;
      }
    }

    SET_OBS_DATA(source_settings, obs_data_get_obj(data, kSettingsScene));
    // source (scene) setting
    if (!__super::obs_->InitScene(source_settings, error_result)) {
      __super::communications_->Send(protocol::events::ERR, error_result);
      return;
    }

    int recording_type = (int)obs_data_get_int(data, protocol::kTypeField);

    switch (recording_type) {
    case protocol::commands::recorderType::VIDEO:
      StartRecording(identifier, data, error_result);
      break;
    case protocol::commands::recorderType::REPLAY:
      StartReplay(identifier, data, error_result);
      break;
    case protocol::commands::recorderType::STREAMING:
      StartStreaming(identifier, data, error_result);
      break;
    default:
      obs_data_set_int(error_result,
                       protocol::kErrorCodeField,
                       protocol::events::INIT_ERROR_FAILED_UNSUPPORTED_RECORDING_TYPE);
      __super::communications_->Send(protocol::events::ERR, error_result);
      break;
    }
    was_ever_initialized = true;
  } catch (...) {
    blog(LOG_ERROR, "Start command error!", identifier);
    obs_data_set_int(error_result,
                     protocol::kErrorCodeField,
                     protocol::events::INIT_ERROR_FAILED_TO_INIT);
    __super::communications_->Send(protocol::events::ERR, error_result);
  }
}

bool obs_control::CommandStart::InitializeOBS(OBSData& data, OBSData& error_result) {
  // video_settings
  SET_OBS_DATA(video_settings, obs_data_get_obj(data, kSettingsVideo));

  // video_settings.video_encoder
  SET_OBS_DATA(video_encoder,
               obs_data_get_obj(video_settings, kSettingsVideoEncoder));

  // video_settings.video_encoder.extra_options
  SET_OBS_DATA(video_extra_options,
               obs_data_get_obj(video_settings, kSettingsExtraOptions));

  if (!__super::obs_->InitVideo(video_settings, video_extra_options,
                                error_result)) {
    __super::communications_->Send(protocol::events::ERR, error_result);
    return false;
  }


  if (!__super::obs_->InitVideoEncoder(video_encoder, 
                                       video_extra_options,
                                       error_result)) {
    __super::communications_->Send(protocol::events::ERR, error_result);
    return false;
  }

  // audio settings
  SET_OBS_DATA(audio_settings, obs_data_get_obj(data, kSettingsAudio));
  __super::obs_->InitAudioSources(audio_settings); // TBD

  return true;
}

bool CommandStart::StartDelayRecording() {
  if (__super::obs_->UsingGameSource() &&
    !already_running_ && !was_ever_initialized) {
    return true;
  }
  else {
    if (__super::obs_->has_monitor_source()) {
      return false;
    }
  }
  return __super::obs_->HasDelayGameSource();
}

void CommandStart::StartRecording(int identifier,
                                  OBSData& data,
                                  OBSData& error_result) {
  // out put file setting
  blog(LOG_INFO, "On Start recording command :%d", identifier);

  SET_OBS_DATA(file_output, obs_data_get_obj(data, kSettingsFileoutput));
  SET_OBS_DATA(audio_setting, obs_data_get_obj(data, kSettingsAudio));
  if (!__super::obs_->ResetOutputSetting(file_output, audio_setting, error_result)) {
    __super::communications_->Send(protocol::events::ERR, error_result);
    return;
  }

  // delay recording is valid only on first init
  if (StartDelayRecording()) {
    // start recording once obs start capturing game frames
    // to prevent black frames
    // we are ready for game capture
    blog(LOG_INFO, "Start delay recording: %d", identifier);
    __super::obs_->StartDelayRecording(identifier);
    __super::communications_->Send(protocol::events::READY, error_result);
    return;
  }

  // no game capture.. lets start
  if (!__super::obs_->StartRecording(identifier, error_result)) {
    blog(LOG_INFO, "error start recording  recording: %d", identifier);
    /// TODO: send different error when |already_running_|
    __super::communications_->Send(protocol::events::ERR, error_result);
    return;
  }

  __super::communications_->Send(protocol::events::READY, error_result);
}

void CommandStart::StartReplay(int identifier,
                               OBSData& data,
                               OBSData& error_result) {
  // video_settings.video_encoder
  SET_OBS_DATA(replay_setting, obs_data_get_obj(data, kSettingsReplay));
  if (!__super::obs_->StartReplay(identifier, data, replay_setting, error_result)) {
    /// TODO: send different error when |already_running_|
    obs_data_set_int(error_result, protocol::kCommandIdentifier, identifier);
    __super::communications_->Send(protocol::events::ERR, error_result);
    return;
  }

  if (!already_running_) {
    __super::communications_->Send(protocol::events::READY, error_result);
  }
}

void CommandStart::StartStreaming(int identifier,
                                  OBSData& data,
                                  OBSData& error_result) {
  UNUSED_PARAMETER(identifier);
  UNUSED_PARAMETER(data);

  // video_settings.video_encoder
  SET_OBS_DATA(streaming_setting, obs_data_get_obj(data, kSettingsStreaming));

  if (!__super::obs_->StartStreaming(identifier, streaming_setting, error_result)) {
    /// TODO: send different error when |already_running_|
    __super::communications_->Send(protocol::events::ERR, error_result);
    return;
  }

  __super::communications_->Send(protocol::events::READY, error_result);
}
