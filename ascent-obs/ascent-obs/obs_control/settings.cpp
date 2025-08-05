#include "obs_control/settings.h"
#include "obs_utils.h"

namespace settings {

const char kSettingsAudioSampleRate[] = "sample_rate";
const int kSettingsAudioSampleRateDefault = 44100;

const char kSettingsAudioMono[] = "mono";
const bool kSettingsAudioMonoDefault = false;


const char kSettingsVideo[] = "video_settings";
const char kSettingsVideoEncoder[] = "video_encoder";
const char kSettingsFileoutput[] = "file_output";
const char kSettingsScene[] = "sources";
const char kSettingsReplay[] = "replay";
const char kSettingsStreaming[] = "streaming";
const char kSettingsSourceMonitor[] = "monitor";
const char kSettingsSourceWindowCapture[] = "window_capture"; 
const char kSettingsSourceGame[] = "game";
const char kSettingsSourceBRB[] = "brb";
const char kSettingsSourceAux[] = "auxSources";
const char kSettingsSourceTobii[] = "tobii";
const char kSettingsForeground[] = "foreground";
const char kAllowTransparency[] = "allow_transparency";
const char kKeepRecordingOnLostForeground[] = "keep_game_recording";
const char kEncoderCustomParameters[] = "encoder_custom_parameters";
const char kCustomParameters[] = "custom_parameters";
const char kSettingsAudio[] = "audio_settings";
const char kSettingsAudioOutput[] = "output";
const char kSettingsAudioInput[] = "input";
const char kSettingsExtraOptions[] = "extra_options";

const char kSettingsVideoFPS[] = "fps";
const int kSettingsVideoFPSDefault = 30;

const char kSettingsVideoBaseWidth[] = "base_width";
const int kSettingsVideoBaseWidthDefault = 1920;

const char kSettingsVideoBaseHeight[] = "base_height";
const int kSettingsVideoBaseHeightDefault = 1080;

const char kSettingsVideoOutputWidth[] = "output_width";
const int kSettingsVideoOutputWidthDefault = 1920;

const char kSettingsVideoOutputHeight[] = "output_height";
const int kSettingsVideoOutputHeightDefault = 1080;

const char kSettingsVideoCompatibilityMode[] = "compatibility_mode";
const char kSettingsGameCursor[] = "game_cursor";

const char kSettingsSecondaryFile[] = "secondaryFile";

};

//------------------------------------------------------------------------------
void settings::SetDefaultAudio(OBSData& audio_settings) {
  if (!obs_data_has_default_value(audio_settings, kSettingsAudioSampleRate)) {
    obs_data_set_default_int(audio_settings,
                             kSettingsAudioSampleRate,
                             kSettingsAudioSampleRateDefault);
  }

  if (!obs_data_has_default_value(audio_settings, kSettingsAudioMono)) {
    obs_data_set_default_bool(audio_settings,
                              kSettingsAudioMono,
                              kSettingsAudioMonoDefault);
  }
}

//------------------------------------------------------------------------------
void settings::SetDefaultVideo(OBSData& video_settings) {
  if (!obs_data_has_default_value(video_settings, kSettingsVideoFPS)) {
    obs_data_set_default_int(video_settings,
                             kSettingsVideoFPS,
                             kSettingsVideoFPSDefault);
  }

  if (!obs_data_has_default_value(video_settings, kSettingsVideoBaseWidth)) {
    obs_data_set_default_int(video_settings,
                             kSettingsVideoBaseWidth,
                             kSettingsVideoBaseWidthDefault);
  }

  if (!obs_data_has_default_value(video_settings, kSettingsVideoBaseHeight)) {
    obs_data_set_default_int(video_settings,
                             kSettingsVideoBaseHeight,
                             kSettingsVideoBaseHeightDefault);
  }

  if (!obs_data_has_default_value(video_settings, kSettingsVideoOutputWidth)) {
    obs_data_set_default_int(video_settings,
                             kSettingsVideoOutputWidth,
                             kSettingsVideoOutputWidthDefault);
  }

  if (!obs_data_has_default_value(video_settings, kSettingsVideoOutputHeight)) {
    obs_data_set_default_int(video_settings,
                             kSettingsVideoOutputHeight,
                             kSettingsVideoOutputHeightDefault);
  }
}

//------------------------------------------------------------------------------
void settings::SetDefaultVideoEncoder(OBSData& video_encoder) {
  UNUSED_PARAMETER(video_encoder);
}

//------------------------------------------------------------------------------
void settings::SetCustomEncoderParameters(OBSData& video_settings,
                                          OBSData& custom_parameters) {
  obs_data_item_t* item = NULL;

  for (item = obs_data_first(custom_parameters); item;
       obs_data_item_next(&item)) {
    enum obs_data_type type = obs_data_item_gettype(item);
    const char* name = obs_data_item_get_name(item);

    if (type == OBS_DATA_STRING) {
      obs_data_set_string(video_settings, name, obs_data_item_get_string(item));
      blog(LOG_INFO, "custom encoder param: '%s':%s", name,
           obs_data_item_get_string(item));
    } else if (type == OBS_DATA_NUMBER) {
      obs_data_set_int(video_settings, name, obs_data_item_get_int(item));
      blog(LOG_INFO, "custom encoder param: '%s':%d", name,
           obs_data_item_get_int(item));
    } else if (type == OBS_DATA_BOOLEAN) {
      obs_data_set_bool(video_settings, name, obs_data_item_get_bool(item));
      blog(LOG_INFO, "custom encoder param: '%s':%s", name,
           obs_data_item_get_bool(item) ? "true" : "false");
    } else if (type == OBS_DATA_OBJECT) {
      obs_data_set_obj(video_settings, name, obs_data_item_get_obj(item));
      blog(LOG_INFO, "custom encoder param: '%s':'object", name);
    } else if (type == OBS_DATA_ARRAY) {
      obs_data_set_array(video_settings, name, obs_data_item_get_array(item));
      blog(LOG_INFO, "custom encoder param: '%s':'array", name);
    } else {
      continue;
    }
  }
}

//------------------------------------------------------------------------------
bool settings::GetAudioExtraParam(OBSData& audio_settings, const char *name) {
  if (!obs_data_has_user_value(audio_settings, settings::kSettingsExtraOptions)) {
    return false;
  }
  SET_OBS_DATA(audio_extra_options, obs_data_get_obj(
    audio_settings, settings::kSettingsExtraOptions));

  return obs_data_get_bool(audio_extra_options, name);
}

int settings::GetSupportedAudioTracksCount(const uint32_t& tracks) {
  int count = 0;
  for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
    if ((tracks & (1 << i)) != 0) {
      count++;
    }
  }

  return count;
}
