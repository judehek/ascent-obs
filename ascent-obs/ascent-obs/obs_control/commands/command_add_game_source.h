#ifndef ASCENTOBS_OBS_CONTROL_COMMAND_ADD_GAME_SOURCE_H_
#define ASCENTOBS_OBS_CONTROL_COMMAND_ADD_GAME_SOURCE_H_

#include <obs.hpp>

#include "obs_control/commands/command.h"

namespace obs_control {
class OBS;

class CommandAddGameSource : public Command {
public:
  CommandAddGameSource(OBS* obs, OBSControlCommunications* communications);
  virtual ~CommandAddGameSource();

public:
  virtual void Perform(int identifier, OBSData& data);
};

}; // namespace obs_control

#endif // ASCENTOBS_OBS_CONTROL_COMMAND_ADD_GAME_SOURCE_H_