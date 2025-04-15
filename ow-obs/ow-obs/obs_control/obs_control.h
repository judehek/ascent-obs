/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2017 Overwolf Ltd.
*******************************************************************************/
#ifndef OWOBS_OBS_CONTROL_OBS_CONTROL_H_
#define OWOBS_OBS_CONTROL_OBS_CONTROL_H_

#include <memory>
#include <map>

#include "obs_control/commands/command.h"
#include "obs_control/obs.h"

namespace libowobs {
  class Thread;
}

namespace obs_control {

class OBSControlCommunications;

class OBSControl {
public:
  OBSControl();
  virtual ~OBSControl();

public:
  bool Init(OBSControlCommunications* communications);
  bool HandleCommand(int command_id, int identifier, OBSData& data);

  void Shutdown();

private:
  void OBSControl::SetWorkingDirectory();
  
private:
  std::unique_ptr<OBS> obs_;
  OBSControlCommunications* communications_;

  std::shared_ptr<libowobs::Thread> command_thread_;

  typedef std::map<int, Command*> CommandsMap;
  CommandsMap commands_map_;

  bool initialized_ = false;
};

}; // namespace obs_control

#endif // OWOBS_OBS_CONTROL_OBS_CONTROL_H_