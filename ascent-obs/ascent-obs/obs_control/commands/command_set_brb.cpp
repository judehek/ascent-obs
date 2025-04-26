#include "obs_control/commands/command_set_brb.h"
#include "obs_control/obs_utils.h"
#include "obs_control/obs.h"
#include "obs_control/settings.h"

using namespace obs_control;
using namespace libowobs;
using namespace settings;

//------------------------------------------------------------------------------
CommandSetBRB::CommandSetBRB(OBS* obs,
                                   OBSControlCommunications* communications) : 
  Command(obs, communications) {
}

//------------------------------------------------------------------------------
CommandSetBRB::~CommandSetBRB() {

}

//------------------------------------------------------------------------------
// virtual 
void CommandSetBRB::Perform(int identifier, OBSData& data) {
  UNUSED_PARAMETER(identifier);
  __super::obs_->UpdateBRB(data);
}
