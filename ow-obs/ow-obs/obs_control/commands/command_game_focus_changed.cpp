/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2017 Overwolf Ltd.
*******************************************************************************/
#include "obs_control/commands/command_game_focus_changed.h"
#include "obs_control/obs_utils.h"
#include "obs_control/obs.h"

using namespace obs_control;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
CommandGameFocusChanged::CommandGameFocusChanged(OBS* obs,
  OBSControlCommunications* communications) :
  Command(obs, communications) {
}

//------------------------------------------------------------------------------
CommandGameFocusChanged::~CommandGameFocusChanged() {

}

//------------------------------------------------------------------------------
// virtual 
void CommandGameFocusChanged::Perform(int identifier, OBSData& data) {
  UNUSED_PARAMETER(identifier);
  
  bool game_in_foreground = obs_data_get_bool(data, "game_foreground");
  bool is_minimized = obs_data_get_bool(data, "is_minimized");
  
  blog(LOG_INFO, 
       "game focus changed: %s", 
        game_in_foreground ? "true" :"false");

  __super::obs_->UpdateSourcesVisiblity(game_in_foreground, is_minimized);
}
