
#ifndef ASCENTOBS_OBS_CONTROL_COMMANDS_COMMAND_START_REPLAY_CAPTURE_H_
#define ASCENTOBS_OBS_CONTROL_COMMANDS_COMMAND_START_REPLAY_CAPTURE_H_

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

#endif // ASCENTOBS_OBS_CONTROL_COMMANDS_COMMAND_START_REPLAY_CAPTURE_H_