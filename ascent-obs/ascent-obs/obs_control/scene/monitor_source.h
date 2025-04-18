/*******************************************************************************
* Overwolf OBS Monitor Source
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#ifndef ASCENTOBS_OBS_CONTROL_MONITOR_SOURCE_H_
#define ASCENTOBS_OBS_CONTROL_MONITOR_SOURCE_H_

#include "obs_control/scene/source.h"

class MonitorSource : public Source {
public:
  static bool IsEnabled(OBSData& monitor_settings);

public:
  MonitorSource(Delegate* delegate) :
    Source(delegate),
    force_(false),
    monitor_index_(0),
    monitor_handle_(0),
    compatible_mode_(false){
  }

  virtual ~MonitorSource() {};

  const bool& force() { return force_; }
  const bool& compatible_mode() { return compatible_mode_; }
  const int& monitor_id() { return monitor_index_; }
  const uint32_t monitor_handle() { return monitor_handle_; }

public:
  virtual bool Create(OBSData& data, obs_scene_t* scene,
    bool visible = true);

  virtual const char* name() override {
    return "monitor";
  };

  bool CreateCompatibility(int monitor_id,
                           uint32_t monitor_handle,
                           obs_scene_t* scene,
                           bool force);

protected:
  virtual void OnVisiblityChanged(bool visible);
  virtual void OnSetVisibility();

private:
  bool CreateMonitorsSource(obs_scene_t* scene,
                           bool compatible = false);

private:
  bool cursor_ = false;
  bool force_ = false;
  int monitor_index_ = 0;
  uint32_t monitor_handle_ = 0;
  bool compatible_mode_;
};

#endif // ASCENTOBS_OBS_CONTROL_MONITOR_SOURCE_H_