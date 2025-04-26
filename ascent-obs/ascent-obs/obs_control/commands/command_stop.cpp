
#include "obs_control/commands/command_stop.h"
#include "obs_control/obs_utils.h"
#include "obs_control/obs.h"
#include "obs_control/settings.h"

using namespace obs_control;
using namespace libascentobs;
using namespace settings;

//------------------------------------------------------------------------------
CommandStop::CommandStop(OBS* obs,
                         OBSControlCommunications* communications) :
  Command(obs, communications) {
}

//------------------------------------------------------------------------------
CommandStop::~CommandStop() {

}

//------------------------------------------------------------------------------
// virtual
void CommandStop::Perform(int identifier, OBSData& data) {
  UNUSED_PARAMETER(data);
  UNUSED_PARAMETER(identifier);
  int recording_type = (int)obs_data_get_int(data, protocol::kTypeField);
  __super::obs_->Stop(identifier, recording_type);
}
