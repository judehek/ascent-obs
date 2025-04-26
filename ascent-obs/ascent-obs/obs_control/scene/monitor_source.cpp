#include "obs_control/scene/monitor_source.h"
#include <map>
#include <windows.h>

namespace {
  const char kSettingEnabled[] = "enable";
  const char kSettingForce[] = "force";
  const char kSettingMonitorHandle[] = "monitor_handle";
  const char kSettingCursor[] = "cursor";
}


HMONITOR GetMainDisplay() {
  // Get the handle to the desktop window
  HWND hwnd = GetDesktopWindow();

  // Get the HMONITOR for the primary display
  return MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
}

std::map<uint32_t, int> monitors;
static BOOL CALLBACK enum_monitor_props(HMONITOR handle, 
                                        HDC hdc, 
                                        LPRECT rect,
                                        LPARAM param) {
  UNUSED_PARAMETER(hdc);
  UNUSED_PARAMETER(rect);
  UNUSED_PARAMETER(param);
  uint32_t handle_uint32_t = (uint32_t)handle;
  monitors[handle_uint32_t] = (int)monitors.size();
  blog(LOG_INFO, "detect monitor [%d]: handle 0x%x", monitors.size() - 1,
       handle_uint32_t);
  return TRUE;
}

bool MonitorSource::IsEnabled(OBSData& monitor_settings) {
  if (monitor_settings == nullptr) {
    return false;
  }
  
  bool enabled = obs_data_get_bool(monitor_settings, kSettingEnabled);
  return enabled;
}

bool MonitorSource::Create(OBSData& data, obs_scene_t* scene, 
  bool visible /*= true*/) {

  //MessageBox(NULL, L"MonitorSource", L"MonitorSource", 0);

  if (data == nullptr) {
    return false;
  }

  if (scene == nullptr) {
    blog(LOG_ERROR, "Failed create monitor source : scene undefined ");
    return false;
  }

  if (!IsEnabled(data)) {
    blog(LOG_INFO, "monitor source disabled");
    return false;
  }
  
  force_ = obs_data_get_bool(data, kSettingForce);
  cursor_ = obs_data_get_bool(data, kSettingCursor);

  monitor_index_ = 0;
  monitor_handle_ = (uint32_t)obs_data_get_int(data, kSettingMonitorHandle);

  if (monitors.empty()) {
    EnumDisplayMonitors(NULL, NULL, enum_monitor_props, NULL);
  }
 
  auto monitor_iter = monitor_handle_ ? 
    monitors.find(monitor_handle_) : monitors.end();

  if (monitor_iter != monitors.end()) {
    monitor_index_ = monitor_iter->second;
  } else {
    if (monitor_handle_ == NULL) {
      monitor_handle_ = (uint32_t)GetMainDisplay();
    }
    blog(LOG_WARNING,
         "invalid monitor handle 0x%x (int: 0x%x), using main screen.",
         (HMONITOR)monitor_handle_, monitor_handle_);
  }

  blog(LOG_INFO, "capture monitor index %d.", monitor_index_);
  if (!CreateMonitorsSource(scene)) {
    return false;
  }

  SetVisible(visible);
  return true;
}

bool MonitorSource::CreateCompatibility(int monitor_id,
                                        uint32_t monitor_handle,
                                        obs_scene_t* scene,
                                        bool force) {
  force_ = force;
  monitor_handle_ = monitor_handle;
  monitor_index_ = monitor_id;
  return CreateMonitorsSource(scene, true);
}

bool MonitorSource::CreateMonitorsSource(obs_scene_t* scene,
                                        bool compatible /*= false*/) {

  //MessageBox(NULL, L"MonitorSource", L"MonitorSource", 0);

  obs_data_t *moitor_settings = obs_data_create();
  obs_data_set_int(moitor_settings, "monitor_handle", monitor_handle_);
  obs_data_set_int(moitor_settings, "monitor_index", monitor_index_);
  obs_data_set_bool(moitor_settings, "capture_cursor", cursor_); 
  // METHOD_AUTO = 0, METHOD_DXGI = 1, METHOD_WGC =2
  obs_data_set_int(moitor_settings, "method", 0); 

  source_.reset(new SourceContext(obs_source_create(
      !compatible ?"monitor_capture" :"monitor_capture_low",
      "monitor capture", moitor_settings, nullptr)));

  source_item_ = obs_scene_add(scene, source_->get_source());
  obs_data_release(moitor_settings);

  SetTransform(source_item_, OBS_BOUNDS_SCALE_INNER);

  blog(LOG_INFO, 
       "monitor source [index: %d handle:0x%x (%d) cursor:%d] added", 
       monitor_index_, monitor_handle_, monitor_handle_, cursor_);

  return true;
}

void MonitorSource::OnVisiblityChanged(bool visible) {
  if (!source_.get() || !source_->get_source()) {
    __super::OnVisiblityChanged(visible);
    return;
  }

  auto deskop_cursor_visible = cursor_ && visible;
  obs_data_t *moitor_settings = obs_data_create();
  obs_data_set_int(moitor_settings, "monitor_handle", monitor_handle_);
  obs_data_set_int(moitor_settings, "monitor_index", monitor_index_);
  obs_data_set_bool(moitor_settings, "capture_cursor", deskop_cursor_visible);
  obs_source_update(source_->get_source(), moitor_settings);
  blog(LOG_INFO, "update desktop cursor visibility: %d", deskop_cursor_visible);
  __super::OnVisiblityChanged(visible);
}

void MonitorSource::OnSetVisibility() {

  SetTransform(source_item_, OBS_BOUNDS_SCALE_INNER);
  obs_sceneitem_set_order(source_item_, OBS_ORDER_MOVE_TOP);
}

