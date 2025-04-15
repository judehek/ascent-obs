/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2017 Overwolf Ltd.
*******************************************************************************/
#ifndef OWOBS_OBS_CONTROL_COMMAND_H_
#define OWOBS_OBS_CONTROL_COMMAND_H_

#include <communications/protocol.h>
#include "obs_control/obs_control_communications.h"

namespace obs_control {

class OBS;

class Command {
public:
  Command(OBS* obs, OBSControlCommunications* communications) : 
    obs_(obs),
    communications_(communications) {
  };

  virtual ~Command() {};

public:
  virtual void Perform(int identifier, OBSData& data) = 0;

protected:
  OBS* obs_;
  OBSControlCommunications* communications_;
};

}; // namespace obs_control

#endif // OWOBS_OBS_CONTROL_COMMAND_H_