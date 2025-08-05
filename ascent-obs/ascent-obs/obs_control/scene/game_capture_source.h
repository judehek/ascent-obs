#ifndef ASCENTOBS_OBS_CONTROL_GAME_CAPTURE_SOURCE_H_
#define ASCENTOBS_OBS_CONTROL_GAME_CAPTURE_SOURCE_H_

#include "obs_control/scene/source.h"
#include "game_capture_source_delegate.h"

class GameCaptureSource : public Source {
public:

public:
  GameCaptureSource(GameCaptureSourceDelegate* delegate,
                    bool compatibility_mode,
                    bool capture_game_cursor,
                    bool capture_window=false);
  virtual ~GameCaptureSource();

public:
  virtual bool Create(OBSData& data,
                      obs_scene_t* scene,
                      bool visible = true);

  static int GetGameSourceId(OBSData& data);

  virtual const char* name() override {
    return "game";
  };

  bool IsProcessAlive();

  void SetForegroundState(bool in_foreground);

  void SwitchToCompatibilityMode();

  const bool foreground() { return foreground_; }

  const bool compatibility_mode() { return compatibility_mode_; }

  const bool did_start_capture() { return did_start_capture_; }

  int game_process_id() { return game_process_id_; }

  void RefreshTransform();

protected:
  virtual void OnSetVisibility();

private:
  friend void UpdateGameSourceCapureState(void *data, calldata_t *params);

  void CaptureStateChanged(obs_source_t* source,
                           bool capture,
                           bool compatibility_mode,
                           const char* error);
private:
  GameCaptureSourceDelegate* GameDelegate() {
    return static_cast<GameCaptureSourceDelegate*>(delegate_);
  }

  OBSSignal capture_state_signal_;
  int game_process_id_;
  bool foreground_;
  bool compatibility_mode_;
  bool capture_game_cursor_;
  bool did_start_capture_;
  bool move_top_ = false;
  obs_flip_type flip_type_ = obs_flip_type::OBS_FLIP_NONE;
};

#endif // ASCENTOBS_OBS_CONTROL_GAME_CAPTURE_SOURCE_H_