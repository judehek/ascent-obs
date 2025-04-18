/*******************************************************************************
* Overwolf OBS Game Capture source delegate
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#ifndef ASCENTOBS_OBS_CONTROL_GAME_CAPTURE_SOURCE_DELEGATE_H_
#define ASCENTOBS_OBS_CONTROL_GAME_CAPTURE_SOURCE_DELEGATE_H_
#include "source.h"

class GameCaptureSourceDelegate : public Source::Delegate {
public:
  virtual void OnGameCaptureStateChanged(bool capturing,
                                         bool is_process_alive,
                                         bool compatibility_mode,
                                         const char* error) = 0;
};

#endif // ASCENTOBS_OBS_CONTROL_GAME_CAPTURE_SOURCE_DELEGATE_H_