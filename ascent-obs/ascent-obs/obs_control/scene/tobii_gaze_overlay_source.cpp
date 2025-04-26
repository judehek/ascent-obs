#include "obs_control/scene/tobii_gaze_overlay_source.h"
#include "obs_control/settings.h"
#include <windows.h>

namespace {

}

void UpdateTobiiGazeSourceCaptureState(void *data, calldata_t *params) {

  obs_source_t *source;
  calldata_get_ptr(params, "source", &source);

  bool capturing = false;
  calldata_get_bool(params, "capture", &capturing);

  GazeOverlaySource* game_source = static_cast<GazeOverlaySource*>(data);
  if (game_source == nullptr)
    return;

  game_source->CaptureStateChanged(source, capturing);
}

GazeOverlaySource::GazeOverlaySource(Delegate* delegate,
  bool compatibility_mode /*= false*/)
 : Source(delegate),
   compatibility_mode_(compatibility_mode) {
}

GazeOverlaySource::~GazeOverlaySource() {
  capture_state_signal_.Disconnect();
}

bool GazeOverlaySource::Create(OBSData& data,
                               obs_scene_t* scene,
                               bool visible /*= true*/) {
  if (data == nullptr) {
    return false;
  }
  //MessageBox(NULL, L"GazeOverlaySource", L"GazeOverlaySource",  0);

  auto window = obs_data_get_string(data, "window");
  visible = obs_data_get_bool(data, "visible");
  bool shared_mem = compatibility_mode_;


  obs_data_t *game_settings = obs_data_create();
  obs_data_set_string(game_settings, "capture_mode", "window");
  obs_data_set_string(game_settings, "window", window);
  obs_data_set_bool(game_settings, "sli_compatibility", shared_mem);
  obs_data_set_int(game_settings, "priority", 1);
  obs_data_set_bool(game_settings, "anti_cheat_hook", false);
  obs_data_set_bool(game_settings, "allow_transparency", true);

  source_.reset(new SourceContext(
      obs_source_create("game_capture",
                        "Ascent Tobii gaze capture",
                        game_settings,
                        nullptr)));

  obs_data_release(game_settings);

  source_item_ = obs_scene_add(scene, source_->get_source());

  SetTransform(source_item_, OBS_BOUNDS_STRETCH);

  capture_state_signal_.Connect(obs_source_get_signal_handler(source_->get_source()),
    "update_capture_state",
    UpdateTobiiGazeSourceCaptureState,
    this);

  SetVisible(visible);

  blog(LOG_INFO,
       "Add Tobii Gaze overlay source [window:%s visible:%d sli_compatibility:%d]",
       window,
       visible,
       shared_mem);

  return true;
}

void GazeOverlaySource::OnSetVisibility() {
  SetTransform(source_item_, OBS_BOUNDS_STRETCH);
  MoveTop();
}

void GazeOverlaySource::CaptureStateChanged(obs_source_t* source, bool capture) {
  blog(LOG_INFO,
       "Tobii gaze source capture state changed: %d",
       capture ? 1 : 0);

  UNUSED_PARAMETER(source);
}
