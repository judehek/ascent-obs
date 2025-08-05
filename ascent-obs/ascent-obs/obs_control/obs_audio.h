#pragma once
#include <memory>
#include <map>
#include <list>
#include <unordered_map>
#include <obs.hpp>
#include <base/macros.h>
#include <base/thread.h>
#include <base/critical_section_lock.h>


#include "settings.h"


namespace obs_control {
extern const char kInputAudioSource[];
extern const char kOutputAudioSource[];

class OBSAudioSourceControl;
class AdvancedOutput;
class OBSAudioProcess;

typedef std::shared_ptr<OBSAudioSourceControl> OBSAudioSourceControlPtr;

class OBSAudioControl {
public:
  virtual ~OBSAudioControl();

  static bool IsGameAudioCaptureSuppored();

  bool ResetAudio(OBSData& audio_settings);

  void InitAudioSources(OBSData& audio_settings,
                        AdvancedOutput* advanced_output);

  void InitScene(obs_scene_t* scene, OBSData& audio_settings);

  void SetVolume(OBSData& volume_settings);

  const unsigned int& active_tracks() {
    return active_tracks_;
  }

  void CreateAudioCaptureSourceItem(obs_data_t* item);

  void Shutdown();

private:
  void InitDefaultAudioSources(OBSData& audio_settings,
                               OBSData& audio_settings_extra);

  void InitExtraAudioSources(OBSData& audio_settings,
                             OBSData& audio_settings_extra);

  void ResetAudioDefaultDevice(const char* source_id,
                               bool is_input_device,
                               int channel,
                               const char* device_desc,
                               OBSData& audio_settings,
                               OBSAudioSourceControlPtr& control);

  void SetSampleRate(uint32_t sample_rate);

  //void ResetAudioDefaultDevice(const char* source_id,
  //                             const char* device_id,
  //                             const char* device_desc,
  //                             bool is_input_device,
  //                             int volume,
  //                             int channel,
  //                             bool use_device_timing,
  //                             OBSAudioSourceControlPtr& control);


  void SetAudioMixerTrack(OBSData& audio_settings_extra);

  bool StopGameCapture();

  void UpdateOuputDevices(bool is_game_audio_capture);

  bool CreateAudioCaptureSource(char** process_list);

  void CreateAudioCaptureSourceV2(obs_data_array_t* audio_capture_proces);  

  bool SetAudioMixerTrackV2(OBSData& audio_settings_extra);

  bool InitDefaultAudioSourcesV2(OBSData& audio_settings_extra);

  void AddAudioSource(OBSData& audio_source,
                      bool is_input_device,
                      const char* device_type);

  bool IsAudioProcessAlreadyCaptured(const char* process_name);

private:
  unsigned int active_tracks_ = settings::AudioTracksFlags::AudioTrack1;

  std::unique_ptr<OBSAudioProcess> audio_process_capture_control_;

  //additional sources
  std::unordered_map<std::string, OBSAudioSourceControlPtr> audio_sources_;

  // backwards compatible
  OBSAudioSourceControlPtr dekstop_volume_control_;
  OBSAudioSourceControlPtr mic_volume_control_;

  // fore old audio process plugin (before 29.0.1)
  OBSAudioSourceControlPtr audio_capture_volume_control_;

  std::list<OBSAudioSourceControlPtr> process_audio_sources_v2_;

private:
  friend class OBS;
  OBSAudioControl();

}; // OBSAudioControl
}; // obs_control