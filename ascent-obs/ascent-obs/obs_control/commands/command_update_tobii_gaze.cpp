
#include "obs_control/commands/command_update_tobii_gaze.h"
#include "obs_control/obs_utils.h"
#include "obs_control/obs.h"
#include "obs_control/settings.h"

using namespace obs_control;
using namespace libascentobs;
using namespace settings;

//------------------------------------------------------------------------------
CommandTobiiGaze::CommandTobiiGaze(OBS* obs,
                                   OBSControlCommunications* communications) : 
  Command(obs, communications) {
}

//------------------------------------------------------------------------------
CommandTobiiGaze::~CommandTobiiGaze() {

}

//------------------------------------------------------------------------------
// virtual 
void CommandTobiiGaze::Perform(int identifier, OBSData& data) {
  UNUSED_PARAMETER(identifier);
  __super::obs_->UpdateTobiiGazaSource(data);
}
