#pragma once
#include <obs.hpp>

#include <memory>
#include <unordered_map>

#define OPT_PROCESS "process"


namespace obs_control {
class OBSAudioSourceControl;
typedef std::shared_ptr<OBSAudioSourceControl> OBSAudioSourceControlPtr;

class OBSAudioProcess {
 public:
  virtual ~OBSAudioProcess();

  static OBSAudioProcess* Create(char** process_list);

  void InitScene(obs_scene_t* scene);

  void set_mixer_track(uint32_t mixer_id);

  // add or remove mixer id
  void set_mixer_track(uint32_t mixer_id, const bool checked, bool log = true);

  void SetVolume(int volume);

  void SetMute(bool mute);

  void SetMono(bool mono);

  static OBSAudioSourceControlPtr CreateAudioSource(const char* executable);

 private:
  OBSAudioProcess();
  
  bool Init(char** process_list);

  bool AddProcess(const char* executable);

 private:
  std::list<OBSAudioSourceControlPtr> audio_sources_;
};

}  // namespace obs_control