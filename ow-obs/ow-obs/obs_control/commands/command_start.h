/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#ifndef OWOBS_OBS_CONTROL_COMMANDS_COMMAND_START_H_
#define OWOBS_OBS_CONTROL_COMMANDS_COMMAND_START_H_

#include <obs.hpp>

#include "obs_control/commands/command.h"

namespace obs_control {
class OBS;

/*
*/
class CommandStart : public Command {
public:
  CommandStart(OBS* obs, OBSControlCommunications* communications);
  virtual ~CommandStart();

public:
  virtual void Perform(int identifier, OBSData& data);

private:
  bool InitializeOBS(OBSData& data, OBSData& error_result);

  bool StartDelayRecording();

  void StartRecording(int identifier, 
                      OBSData& data,
                      OBSData& error_result);
  void StartStreaming(int identifier, 
                      OBSData& data,
                      OBSData& error_result);
  void StartReplay(int identifier,
                   OBSData& data,
                   OBSData& error_result);

  bool already_running_;
};

};

#endif // OWOBS_OBS_CONTROL_COMMANDS_COMMAND_START_H_