
#ifndef ASCENTOBS_OBS_CONTROL_COMMANDS_COMMAND_STOP_H_
#define ASCENTOBS_OBS_CONTROL_COMMANDS_COMMAND_STOP_H_

#include <obs.hpp>

#include "obs_control/commands/command.h"

namespace obs_control {
class OBS;

/*
*/
class CommandStop : public Command {
public:
  CommandStop(OBS* obs, OBSControlCommunications* communications);
  virtual ~CommandStop();

public:
  virtual void Perform(int identifier, OBSData& data);
};

}; // namespace obs_control

#endif // ASCENTOBS_OBS_CONTROL_COMMANDS_COMMAND_STOP_H_