#ifndef ASCENTOBS_OBS_CONTROL_COMMAND_QUERY_MACHINE_INFO_H_
#define ASCENTOBS_OBS_CONTROL_COMMAND_QUERY_MACHINE_INFO_H_

#include <obs.hpp>

#include "obs_control/commands/command.h"

namespace obs_control {
class OBS;

class CommandQueryMachineInfo : public Command {
public:
  CommandQueryMachineInfo(OBS* obs, OBSControlCommunications* communications);
  virtual ~CommandQueryMachineInfo();

public:
  virtual void Perform(int identifier, OBSData& data);
};

}; // namespace obs_control

#endif // ASCENTOBS_OBS_CONTROL_COMMAND_QUERY_MACHINE_INFO_H_