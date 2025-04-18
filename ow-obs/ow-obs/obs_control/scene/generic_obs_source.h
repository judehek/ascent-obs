/*******************************************************************************
* Overwolf OBS GenericObsSource
*
* Copyright (c) 2020 Overwolf Ltd.
*******************************************************************************/

#ifndef ASCENTOBS_OBS_CONTROL_GENERIC_OBS_SOURCE_H_
#define ASCENTOBS_OBS_CONTROL_GENERIC_OBS_SOURCE_H_

///////////////////////////////////////////////////////////////////////////////

#include "obs_control/scene/source.h"
#include "game_capture_source_delegate.h"
#include <string>

#define VIDEO_DEVICE_ID   "video_device_id"

///////////////////////////////////////////////////////////////////////////////

class GenericObsSource : public Source {
public:
// factory
static Source* CreateOBSSource(Delegate* delegate,
                               OBSData& data,
                               obs_scene_t* scene,
                               bool visible = true);

public:
  GenericObsSource(Delegate* delegate = nullptr): Source(delegate){}
  ~GenericObsSource() {}

public:
  virtual bool Create(OBSData& data, obs_scene_t* scene,
                      bool visible = true);

  virtual const char* name() override {
    return name_.c_str();
  };

protected:
  void ParseParamters(obs_data_t* settings, OBSData& data);

protected:
  std::string name_;
};

#endif // ASCENTOBS_OBS_CONTROL_GENERIC_OBS_SOURCE_H_