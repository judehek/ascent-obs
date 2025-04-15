/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#ifndef OWOBS_OBS_CONTROL_OBS_AUDIO_SOURCE_CONTROL_H_
#define OWOBS_OBS_CONTROL_OBS_AUDIO_SOURCE_CONTROL_H_

#include <string>
#include <memory>

#include <obs.hpp>
#include "obs_control/settings.h"

namespace obs_control {
 
extern std::string GetAudioTracksStr(uint32_t enabled_mixers);

class OBSAudioSourceControl {


public:
  OBSAudioSourceControl(obs_source_t* source,
                   const char* name,
                   bool is_input_device = false);
  virtual ~OBSAudioSourceControl();

  // set one track mixer id
  /*settings::AudioTracksFlags*/
  void set_mixer_track(uint32_t mixer_id);

  // add or remove mixer id
  void set_mixer_track(uint32_t mixer_id,
                       const bool checked, bool log = true);

  int volume() { return volume_; }

  bool has_source() {
    return obs_source_;
  }

  bool is_mono() { return mono_; }
  void SetMono(bool val);

  obs_source_t* audio_source() { return obs_source_; }

  void AddToScene(obs_scene_t* scene);

public:
  void SetVolume(int volume);
  void SetMute(bool mute);

private:
  std::string name_;
  bool is_muted_ = false;
  bool mono_ = false;
  bool is_input_device_;
  int volume_ = 100;

  OBSSource obs_source_;
  obs_fader_t* obs_fader_ = nullptr;

  bool added_to_scene_ = false;
};

typedef std::shared_ptr<OBSAudioSourceControl> OBSAudioSourceControlPtr;

}; // namespace obs_control

#endif // OWOBS_OBS_CONTROL_OBS_AUDIO_SOURCE_CONTROL_H_