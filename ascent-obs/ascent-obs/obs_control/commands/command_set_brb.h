
#ifndef ASCENTOBS_OBS_CONTROL_COMMANDS_COMMAND_SET_BRB_H_
#define ASCENTOBS_OBS_CONTROL_COMMANDS_COMMAND_SET_BRB_H_

#include <obs.hpp>

#include "obs_control/commands/command.h"

namespace obs_control {
class OBS;

class CommandSetBRB : public Command {
public:
  CommandSetBRB(OBS* obs, OBSControlCommunications* communications);
  virtual ~CommandSetBRB();

public:
  virtual void Perform(int identifier, OBSData& data);
};

};

#endif // ASCENTOBS_OBS_CONTROL_COMMANDS_COMMAND_SET_BRB_H_