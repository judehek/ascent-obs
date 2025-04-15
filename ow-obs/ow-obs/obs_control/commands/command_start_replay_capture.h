/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#ifndef OWOBS_OBS_CONTROL_COMMANDS_COMMAND_START_REPLAY_CAPTURE_H_
#define OWOBS_OBS_CONTROL_COMMANDS_COMMAND_START_REPLAY_CAPTURE_H_

#include <obs.hpp>

#include "obs_control/commands/command.h"

namespace obs_control {
class OBS;

class CommandStartReplayCapture: public Command {
public:
  CommandStartReplayCapture(OBS* obs, OBSControlCommunications* communications);
  virtual ~CommandStartReplayCapture();

public:
  virtual void Perform(int identifier, OBSData& data);

private:
};

};

#endif // OWOBS_OBS_CONTROL_COMMANDS_COMMAND_START_REPLAY_CAPTURE_H_