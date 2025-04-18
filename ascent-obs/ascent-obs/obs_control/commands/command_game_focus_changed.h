/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2017 Overwolf Ltd.
*******************************************************************************/
#ifndef ASCENTOBS_OBS_CONTROL_COMMAND_GAME_FOCUS_CHANGED_H_
#define ASCENTOBS_OBS_CONTROL_COMMAND_GAME_FOCUS_CHANGED_H_

#include <obs.hpp>

#include "obs_control/commands/command.h"

namespace obs_control {
class OBS;

class CommandGameFocusChanged : public Command {
public:
  CommandGameFocusChanged(OBS* obs, OBSControlCommunications* communications);
  virtual ~CommandGameFocusChanged();

public:
  virtual void Perform(int identifier, OBSData& data);
};

}; // namespace obs_control

#endif // ASCENTOBS_OBS_CONTROL_COMMAND_GAME_FOCUS_CHANGED_H_