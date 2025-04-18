/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2017 Overwolf Ltd.
*******************************************************************************/
#ifndef ASCENTOBS_OBS_CONTROL_OBS_CONTROL_COMMUNICATIONS_H_
#define ASCENTOBS_OBS_CONTROL_OBS_CONTROL_COMMUNICATIONS_H_

#include <obs.hpp>
namespace obs_control {

class OBSControlCommunications {
public:
  virtual void Send(int command_id, OBSData& data) = 0;
  virtual void Send(int command_id) = 0;

  virtual void Shutdown() = 0;
};

}; // namespace obs_control

#endif // ASCENTOBS_OBS_CONTROL_OBS_CONTROL_COMMUNICATIONS_H_