/*******************************************************************************
* Overwolf OBS Window Source
*
* Copyright (c) 2021 Overwolf Ltd.
*******************************************************************************/

#ifndef ASCENTOBS_OBS_CONTROL_WINDOW_SOURCE_H_
#define ASCENTOBS_OBS_CONTROL_WINDOW_SOURCE_H_

//-----------------------------------------------------------------------------

#include "obs_control/scene/source.h"

//-----------------------------------------------------------------------------

class WindowSource : public Source {
public:
  static bool IsEnabled(OBSData& window_settings);

public:
  WindowSource(Delegate* delegate) :
    Source(delegate),
    window_handle_(0) {
  }

  virtual ~WindowSource() {};

  const uint32_t& window_handle() { return window_handle_; }

  virtual bool Create(OBSData& data, obs_scene_t* scene, bool visible = true);

  virtual const char* name() override {
    return "window";
  };

protected:

  virtual void OnVisiblityChanged(bool visible);
  virtual void OnSetVisibilty();

private:

  bool CreateWindowSource(obs_scene_t* scene);

private:
  bool cursor_ = false;
  uint32_t window_handle_ = 0;

};

#endif // ASCENTOBS_OBS_CONTROL_WINDOW_SOURCE_H_