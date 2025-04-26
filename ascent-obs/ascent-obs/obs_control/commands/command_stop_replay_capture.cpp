
#include "obs_control/commands/command_stop_replay_capture.h"
#include "obs_control/obs_utils.h"
#include "obs_control/obs.h"
#include "obs_control/settings.h"

using namespace obs_control;
using namespace libowobs;
using namespace settings;

//------------------------------------------------------------------------------
CommandStopReplayCapture::CommandStopReplayCapture(OBS* obs,
                           OBSControlCommunications* communications) : 
  Command(obs, communications) {
}

//------------------------------------------------------------------------------
CommandStopReplayCapture::~CommandStopReplayCapture() {

}

//------------------------------------------------------------------------------
// virtual 
void CommandStopReplayCapture::Perform(int identifier, OBSData& data) {
  // initialize the error result (just in case we get an error)
  CREATE_OBS_DATA(error_result);
  obs_data_set_int(error_result, protocol::kCommandIdentifier, identifier);
  if (!__super::obs_->StopCaptureReplay(data, error_result)) {
    obs_data_set_default_obj(error_result, "data", data);
    __super::communications_->Send(protocol::events::REPLAY_ERROR, error_result);
  }
}