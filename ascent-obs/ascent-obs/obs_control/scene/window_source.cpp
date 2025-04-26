#include "obs_control/scene/window_source.h"
#include <map>
#include <windows.h>

//-----------------------------------------------------------------------------
namespace {
  const char kSettingEnabled[] = "enable";
  const char kSettingForce[] = "force";
  const char kSettingWindowHandle[] = "window_handle";
  const char kSettingCursor[] = "cursor";
}

//-----------------------------------------------------------------------------
bool WindowSource::IsEnabled(OBSData& window_settings) {
  if (window_settings == nullptr) {
    return false;
  }

  bool enabled = obs_data_get_bool(window_settings, kSettingEnabled);

  return enabled;
}

//-----------------------------------------------------------------------------
bool WindowSource::Create(OBSData& data, obs_scene_t* scene,
  bool visible /*= true*/) {

  if (data == nullptr) {
    return false;
  }

  if (scene == nullptr) {
    blog(LOG_ERROR, "Failed to create window source : scene undefined ");
    return false;
  }

  if (!IsEnabled(data)) {
    blog(LOG_INFO, "window source disabled");
    return false;
  }

  cursor_ = obs_data_get_bool(data, kSettingCursor);
  window_handle_ = (int)obs_data_get_int(data, kSettingWindowHandle);

  blog(LOG_INFO, "capture window handle %d.", window_handle_);

  if (!CreateWindowSource(scene)) {
    return false;
  }

  SetVisible(visible);

  return true;
}

//-----------------------------------------------------------------------------
bool WindowSource::CreateWindowSource(obs_scene_t* scene) {
  //MessageBox(NULL, L"WindowSource", L"WindowSource", 0);

  obs_data_t *window_settings = obs_data_create();

  obs_data_set_int(window_settings, "window_handle", window_handle_);
  obs_data_set_bool(window_settings, "cursor", cursor_);

  source_.reset(new SourceContext(obs_source_create("window_capture",
                                 "window capture", window_settings, nullptr)));

  source_item_ = obs_scene_add(scene, source_->get_source());
  obs_data_release(window_settings);

  //SetTransform(source_item_, OBS_BOUNDS_SCALE_INNER);
  SetTransform(source_item_, OBS_BOUNDS_STRETCH);

  blog(LOG_INFO,
       "window source [handle:%d cursor:%d] added",
       window_handle_,
       cursor_);

  return true;
}

//-----------------------------------------------------------------------------
void WindowSource::OnVisiblityChanged(bool visible) {
  if (!source_.get() || !source_->get_source()) {
    __super::OnVisiblityChanged(visible);
    return;
  }

  auto deskop_cursor_visible = cursor_ && visible;

  obs_data_t* window_settings = obs_data_create();

  obs_data_set_int(window_settings, "window_handle", window_handle_);
  obs_data_set_bool(window_settings, "capture_cursor", deskop_cursor_visible);
  obs_source_update(source_->get_source(), window_settings);

  blog(LOG_INFO, "update desktop cursor visibility: %d", deskop_cursor_visible);

  __super::OnVisiblityChanged(visible);
}

//-----------------------------------------------------------------------------
void WindowSource::OnSetVisibilty() {
  SetTransform(source_item_, OBS_BOUNDS_STRETCH);
  obs_sceneitem_set_order(source_item_, OBS_ORDER_MOVE_TOP);
}
