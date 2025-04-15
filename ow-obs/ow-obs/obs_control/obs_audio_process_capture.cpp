#include "obs_control/obs_audio_process_capture.h"

#include "advanced_output.h"
#include "obs_audio.h"
#include "obs_audio_source_control.h"
#include "obs_utils.h"

using namespace obs_control;

const char kAudioCaptureSourceNew[] = "wasapi_process_output_capture";

//-----------------------------------------------------------------------------
OBSAudioProcess::~OBSAudioProcess() {}

//-----------------------------------------------------------------------------
OBSAudioProcess::OBSAudioProcess() {}

//-----------------------------------------------------------------------------
OBSAudioProcess* OBSAudioProcess::Create(char** process_list) {
  if (!process_list || !(*process_list)) {
    return nullptr;
  }

  OBSAudioProcess* process_audio_capture = new OBSAudioProcess();
  if (!process_audio_capture->Init(process_list)) {
    delete process_audio_capture;
    return nullptr;
  }

  return process_audio_capture;
}

//-----------------------------------------------------------------------------
bool OBSAudioProcess::Init(char** process_list) {
  while (*process_list) {
    if (!AddProcess(*process_list)) {
      return false;
    }
    process_list++;
  }

  return true;
}

//-----------------------------------------------------------------------------
bool OBSAudioProcess::AddProcess(const char* executable) {
  auto source = CreateAudioSource(executable);
  if (!source || !source.get()) {    
    return false;
  }
  audio_sources_.push_back(std::move(source));      
  return true;
}

//-----------------------------------------------------------------------------
void OBSAudioProcess::InitScene(obs_scene_t* scene) {
  for (auto& audio_source : audio_sources_) {
    audio_source->AddToScene(scene);
  }
}

//-----------------------------------------------------------------------------
void OBSAudioProcess::set_mixer_track(uint32_t mixer_id) {
  for (auto& audio_source : audio_sources_) {
    audio_source->set_mixer_track(mixer_id);
  }
}

//-----------------------------------------------------------------------------
void OBSAudioProcess::set_mixer_track(uint32_t mixer_id,
                                      const bool checked,
                                      bool log /*= true*/) {
  for (auto& audio_source : audio_sources_) {
    audio_source->set_mixer_track(mixer_id, checked, log);
  }
}

//-----------------------------------------------------------------------------
void OBSAudioProcess::SetVolume(int volume) {
  for (auto& audio_source : audio_sources_) {
    audio_source->SetVolume(volume);
  }
}

//-----------------------------------------------------------------------------
void OBSAudioProcess::SetMute(bool mute) {
  for (auto& audio_source : audio_sources_) {
    audio_source->SetMute(mute);
  }
}

//-----------------------------------------------------------------------------
void OBSAudioProcess::SetMono(bool mono) {
  for (auto& audio_source : audio_sources_) {
    audio_source->SetMono(mono);
  }
}

//-----------------------------------------------------------------------------
obs_control::OBSAudioSourceControlPtr OBSAudioProcess::CreateAudioSource(
    const char* executable) {
  std::string name = "Process audio capture ";
  name += executable;
      
  OBSData settings = obs_data_create();
  obs_data_set_string(settings, OPT_PROCESS, executable);
  obs_data_set_int(settings, "priority", 2);

  OBSSource audio_capture_source = obs_source_create(
      kAudioCaptureSourceNew, name.c_str(), settings, nullptr);

  obs_source_release(audio_capture_source);
  
  if (!audio_capture_source) {
    blog(LOG_ERROR, "fail to create audio capture (new) %s", executable);
    return nullptr;
  }
   
  return std::make_unique<OBSAudioSourceControl>(audio_capture_source,
                                                 name.c_str());
}
