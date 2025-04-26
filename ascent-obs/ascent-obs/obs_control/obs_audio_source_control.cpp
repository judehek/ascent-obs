#include "obs_control/obs_audio_source_control.h"
#include "obs_control/obs_utils.h"

#include <windows.h>


namespace obs_control {
std::string GetAudioTracksStr(uint32_t enabled_mixers) {
  std::vector<uint32_t> enabled_tracks;
  for (uint32_t i = 0; i < MAX_AUDIO_MIXES; i++) {
    if ((enabled_mixers & (1 << i)) != 0) {      
      enabled_tracks.push_back(i + 1);
    }
  }

  return utils::join(enabled_tracks);
}

};  // namespace obs_control

using namespace obs_control;

//------------------------------------------------------------------------------
OBSAudioSourceControl::OBSAudioSourceControl(obs_source_t *source,
                                   const char* name,
                                   bool is_input_device) :
  is_input_device_(is_input_device),
  obs_source_(source),
  name_(name ? name :  "unknown" ){
  obs_fader_ = obs_fader_create(OBS_FADER_CUBIC);
  if (obs_source_ != nullptr) {
    obs_fader_attach_source(obs_fader_, obs_source_);
  }
  SetVolume(100);
}

//------------------------------------------------------------------------------
OBSAudioSourceControl::~OBSAudioSourceControl() {
  if (obs_fader_) {
    obs_fader_destroy(obs_fader_);
    obs_fader_ = nullptr;
  }
  obs_source_ = nullptr;
}

//------------------------------------------------------------------------------
void OBSAudioSourceControl::SetVolume(int volume) {
  //MessageBoxW(0, L"SetVolume", L"OBSAudioSourceControl", 0);
  if (volume < 0) {
    volume = 0;
  }

  if (volume > 2000) {
    volume = 2000;
  }

  if (volume_ != volume) {
    blog(LOG_INFO, "Set '%s' volume: %d", name_.c_str(), volume);
  }

  volume_= volume;
  if (volume_ <= 100 && obs_fader_) {
    obs_fader_set_deflection(obs_fader_, float(volume) * 0.01f);
  } else {
    obs_source_set_volume(obs_source_, float(volume) * 0.01f);
  } 
}

//------------------------------------------------------------------------------
void OBSAudioSourceControl::SetMute(bool mute) {
  if (mute == is_muted_) {
    return;
  }

  is_muted_ = mute;
  blog(LOG_INFO, "mute '%s': %d", name_.c_str(), mute);
  obs_source_set_muted(obs_source_, is_muted_);
}

//------------------------------------------------------------------------------
void OBSAudioSourceControl::set_mixer_track(uint32_t enabled_mixers) {
  if (obs_source_ == nullptr) {
    return;
  }

  /*blog(LOG_INFO, "Set %s mixer track: %d",
    is_input_device_ ? "input-device" : "output-device", mixer_id + 1);*/

  uint32_t mixers = obs_source_get_audio_mixers(obs_source_);
  uint32_t new_mixers = mixers;

  for (uint32_t i = 0; i < MAX_AUDIO_MIXES; i++) {
    if ((enabled_mixers & (1 << i)) != 0) {
      new_mixers |= (1 << i);
    } else {
      new_mixers &= ~(1 << i);
    }
  }

  blog(LOG_INFO, "Set %s mixer tracks: 0x%x (0x%x) %s",
       name_.c_str(), new_mixers, enabled_mixers, 
       GetAudioTracksStr(new_mixers).c_str());

  obs_source_set_audio_mixers(obs_source_, new_mixers);

}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void OBSAudioSourceControl::set_mixer_track(uint32_t mixer_id,
                                       const bool checked,
                                       bool log) {
  uint32_t mixers = obs_source_get_audio_mixers(obs_source_);
  uint32_t new_mixers = mixers;

  if (checked) {
    new_mixers |= (1 << mixer_id);
  } else {
    new_mixers &= ~(1 << mixer_id);
  }

  if (log) {
    blog(LOG_INFO, "set '%s' mixer audio track: %d (%s)",
         name_.c_str(), mixer_id + 1,checked ? "on" : "off");
  }

  obs_source_set_audio_mixers(obs_source_, new_mixers);
}


//------------------------------------------------------------------------------
void OBSAudioSourceControl::SetMono(bool val) {
  if (!obs_source_) {
    return;
  }

  uint32_t flags = obs_source_get_flags(obs_source_);
  bool forceMonoActive = (flags & OBS_SOURCE_FLAG_FORCE_MONO) != 0;

  if (forceMonoActive == val) {
    return;
  }

	if (val) {
    flags |= OBS_SOURCE_FLAG_FORCE_MONO;
  } else {
    flags &= ~OBS_SOURCE_FLAG_FORCE_MONO;
  }
  mono_ = val;
  obs_source_set_flags(obs_source_, flags);
  blog(LOG_INFO, "Update (%s) force mono:", name_.c_str(), val ? "on" : "off");
}

void OBSAudioSourceControl::AddToScene(obs_scene_t* scene) {
  if (added_to_scene_) {
    return;
  }

  obs_scene_add(scene, audio_source());
  added_to_scene_ = true;
}