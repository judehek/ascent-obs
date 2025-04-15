/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#ifndef OWOBS_OBS_CONTROL_COMMANDS_COMMAND_TOBII_GAZE_H_
#define OWOBS_OBS_CONTROL_COMMANDS_COMMAND_TOBII_GAZE_H_

#include <obs.hpp>

#include "obs_control/commands/command.h"

namespace obs_control {
class OBS;

class CommandTobiiGaze : public Command {
public:
  CommandTobiiGaze(OBS* obs, OBSControlCommunications* communications);
  virtual ~CommandTobiiGaze();

public:
  virtual void Perform(int identifier, OBSData& data);
};

};

#endif // OWOBS_OBS_CONTROL_COMMANDS_COMMAND_TOBII_GAZE_H_