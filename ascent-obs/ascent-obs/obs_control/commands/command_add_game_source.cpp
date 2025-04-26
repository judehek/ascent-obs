#include "obs_control/commands/command_add_game_source.h"
#include "obs_control/obs_utils.h"
#include "obs_control/obs.h"

using namespace obs_control;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
CommandAddGameSource::CommandAddGameSource(OBS* obs,
  OBSControlCommunications* communications) :
  Command(obs, communications) {
}

//------------------------------------------------------------------------------
CommandAddGameSource::~CommandAddGameSource() {

}

//------------------------------------------------------------------------------
// virtual 
void CommandAddGameSource::Perform(int identifier, OBSData& data) {
  UNUSED_PARAMETER(identifier);

  __super::obs_->AddGameSource(data);
}
