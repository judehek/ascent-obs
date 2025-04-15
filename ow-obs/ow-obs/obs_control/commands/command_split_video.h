/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2019 Overwolf Ltd.
*******************************************************************************/
#ifndef OWOBS_OBS_CONTROL_COMMANDS_COMMAND_SPLIT_VIDEO_H_
#define OWOBS_OBS_CONTROL_COMMANDS_COMMAND_SPLIT_VIDEO_H_

#include <obs.hpp>

#include "obs_control/commands/command.h"

namespace obs_control {
class OBS;

class CommandSplitVideo : public Command {
public:
  CommandSplitVideo(OBS* obs, OBSControlCommunications* communications);
  virtual ~CommandSplitVideo();

public:
  virtual void Perform(int identifier, OBSData& data);
};

};

#endif // OWOBS_OBS_CONTROL_COMMANDS_COMMAND_SPLIT_VIDEO_H_#pragma once
