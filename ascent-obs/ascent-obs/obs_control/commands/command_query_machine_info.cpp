#include "obs_control/commands/command_query_machine_info.h"
#include "obs_control/obs_utils.h"
#include "obs_control/obs.h"
#include "obs_control/obs_audio.h"

using namespace obs_control;

//------------------------------------------------------------------------------
CommandQueryMachineInfo::CommandQueryMachineInfo(
  OBS* obs, 
  OBSControlCommunications* communications) : Command(obs, communications) {

}

//------------------------------------------------------------------------------
CommandQueryMachineInfo::~CommandQueryMachineInfo() {

}

//------------------------------------------------------------------------------
// virtual 
void CommandQueryMachineInfo::Perform(int identifier, OBSData& data) {
  UNUSED_PARAMETER(identifier);
  UNUSED_PARAMETER(data);

  CREATE_OBS_DATA(result);

  // get input audio devices
  CREATE_OBS_DATA_ARRAY(audio_input_devices);
  blog(LOG_INFO, "QueryMachine: retrieve audio input devices");

  __super::obs_->RetreiveAudioDevices(kInputAudioSource, 
                                      audio_input_devices);
  obs_data_set_array(result, 
                     libascentobs::protocol::kAudioInputDevices, 
                     audio_input_devices);
  
  // get output audio devices
  CREATE_OBS_DATA_ARRAY(audio_output_devices);
  blog(LOG_INFO, "QueryMachine: retrieve audio output devices");
  __super::obs_->RetreiveAudioDevices(kOutputAudioSource,
                                      audio_output_devices);
  obs_data_set_array(result, 
                     libascentobs::protocol::kAudioOutputDevices, 
                     audio_output_devices);

  // get video encoders
  //MessageBoxA(NULL, "", "get video encoders", 0);

  CREATE_OBS_DATA_ARRAY(video_encoders);
  blog(LOG_INFO, "QueryMachine: retrieve supported video encoders");
  __super::obs_->RetreiveSupportedVideoEncoders(video_encoders);
  obs_data_set_array(result, 
                     libascentobs::protocol::kVideoEncoders, 
                     video_encoders);

  obs_data_set_bool(result, libascentobs::protocol::kWinrtCaptureSupported,
                    __super::obs_->IsWinrtCaptureSupported());

  blog(LOG_INFO, "QueryMachine: sending result");

  __super::communications_->Send(libascentobs::protocol::events::QUERY_MACHINE_INFO, 
                                 result);
}
