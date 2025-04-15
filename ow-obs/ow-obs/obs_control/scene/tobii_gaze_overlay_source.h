/*******************************************************************************
* Overwolf OBS Game Capture source
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#ifndef OWOBS_OBS_CONTROL_GAZE_OVERLAY_SOURCE_H_
#define OWOBS_OBS_CONTROL_GAZE_OVERLAY_SOURCE_H_

#include "obs_control/scene/source.h"
#include "game_capture_source_delegate.h"

class GazeOverlaySource : public Source {
public:

public:
  GazeOverlaySource(Delegate* delegate,
                    bool compatibility_mode = false);
  virtual ~GazeOverlaySource();

public:
  virtual bool Create(OBSData& data, 
                      obs_scene_t* scene, 
                      bool visible = true);
  virtual const char* name() override {
    return "tobii_gaze_";
  };

protected:
  virtual void OnSetVisibility();

private:
  friend void UpdateTobiiGazeSourceCaptureState(void *data, calldata_t *params);

  void CaptureStateChanged(obs_source_t* source, bool capture);
private:
  OBSSignal capture_state_signal_;
  bool compatibility_mode_;
};

#endif // OWOBS_OBS_CONTROL_GAZE_OVERLAY_SOURCE_H_