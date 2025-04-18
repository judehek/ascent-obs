/*****  **************************************************************************
* Overwolf OBS Game Capture source
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#include "obs_control/scene/game_capture_source.h"
#include "obs_control/settings.h"
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

namespace {
  const char kSettingProcessId[] = "process_id";
  const char kSettingCompatibility[] = "compatibility"; // shared memory
  const char kSettingFlipType[] = "flip_type";

  bool IsProcessExists(DWORD pid) {
    PROCESSENTRY32 pe32;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    __try {
      if (hSnapshot == INVALID_HANDLE_VALUE) __leave;

      ZeroMemory(&pe32, sizeof(pe32));
      pe32.dwSize = sizeof(pe32);
      if (!Process32First(hSnapshot, &pe32)) __leave;

      do {
        if (pe32.th32ProcessID == pid) {
          return true;
          break;
        }
      } while (Process32Next(hSnapshot, &pe32));

    }
    __finally {
      if (hSnapshot != INVALID_HANDLE_VALUE) CloseHandle(hSnapshot);
    }

    return false;
  }
}


void UpdateGameSourceCapureState(void *data, calldata_t *params) {

  obs_source_t *source;
  calldata_get_ptr(params, "source", &source);

  bool capturing = false;
  calldata_get_bool(params, "capture", &capturing);

  bool compatibility_mode = false;
  calldata_get_bool(params, "sli_compatibility", &compatibility_mode);

  const char* error = nullptr;
  calldata_get_string(params, "error", &error);

  GameCaptureSource* game_source = static_cast<GameCaptureSource*>(data);
  if (game_source == nullptr)
    return;

  game_source->CaptureStateChanged(source, capturing, compatibility_mode, error);
}

GameCaptureSource::GameCaptureSource(GameCaptureSourceDelegate* delegate,
  bool compatibility_mode,
  bool capture_game_cursor,
  bool move_top)
  : Source(delegate),
  foreground_(false),
  compatibility_mode_(compatibility_mode),
  capture_game_cursor_(capture_game_cursor),
  move_top_(move_top),
  game_process_id_(0),
  did_start_capture_(false) {
}

GameCaptureSource::~GameCaptureSource() {
  capture_state_signal_.Disconnect();
}

bool GameCaptureSource::Create(OBSData& data,
  obs_scene_t* scene, bool visible/*= true*/) {

  if (data == nullptr) {
    return false;
  }

  foreground_ = obs_data_get_bool(data, settings::kSettingsForeground);
  game_process_id_ =
    (int)obs_data_get_int(data, kSettingProcessId);

  if (game_process_id_ <= 0) {
    blog(LOG_ERROR,
      "Game capture source: invalid process id: %d",
      game_process_id_);
    return false;
  }

  flip_type_ = (obs_flip_type)obs_data_get_int(data, kSettingFlipType);

  if (!IsProcessAlive()) {
    blog(LOG_ERROR,
      "Game capture source: invalid process %d (exit?)",
      game_process_id_);
    return false;
  }

  obs_data_t *game_settings = obs_data_create();
  obs_data_set_string(game_settings, "capture_mode", "process");
  obs_data_set_int(game_settings, "process_id", game_process_id_);
  obs_data_set_bool(game_settings, "anti_cheat_hook", false);
  obs_data_set_bool(game_settings, "sli_compatibility", compatibility_mode_);
  obs_data_set_bool(game_settings, "in_foreground", true);
  obs_data_set_bool(game_settings, "capture_cursor", capture_game_cursor_);
  obs_data_set_bool(game_settings, "anti_cheat_hook", true);

  bool allow_transparency = obs_data_get_bool(data, settings::kAllowTransparency);
  obs_data_set_bool(game_settings, settings::kAllowTransparency, allow_transparency);

  source_.reset(new SourceContext(
    obs_source_create("game_capture",
    "Ascent Game capture",
    game_settings,
    nullptr)));

  obs_data_release(game_settings);

  source_item_ = obs_scene_add(scene, source_->get_source());

  RefreshTransform();

  capture_state_signal_.Connect(obs_source_get_signal_handler(source_->get_source()),
    "update_capture_state",
    UpdateGameSourceCapureState,
    this);

  //always start as visible so we can hook the game
  // forground_ will be take care on OBS::HandleGameCaptureStateChanged
  SetVisible(/*foreground_ || visible*/ true);

  blog(LOG_INFO,
    "Add Game source [process:%d, sli_compatibility:%d cursor:%d foreground:%d visible:%d (true) flip:%d]",
    game_process_id_,
    compatibility_mode_,
    capture_game_cursor_,
    foreground_,
    visible,
    flip_type_);

  return true;
}

int GameCaptureSource::GetGameSourceId(OBSData& data) {
  return (int)obs_data_get_int(data, kSettingProcessId);
}

bool GameCaptureSource::IsProcessAlive() {
  auto process_handle = OpenProcess(
    SYNCHRONIZE,
    false, game_process_id_);

  DWORD error = GetLastError();
  bool alive = process_handle != NULL;

  if (alive) {
    alive = WaitForSingleObject(process_handle, 0) == WAIT_TIMEOUT;
  } if (error != 0) {
    return IsProcessExists(game_process_id_);
  }

  if (process_handle != nullptr) {
    CloseHandle(process_handle);
  }

  return alive;
}

void GameCaptureSource::SetForegroundState(bool in_foreground) {
  bool updated = foreground_ != in_foreground;
  if (foreground_ != in_foreground) {
    blog(LOG_INFO, "Game capture foreground changed: %d -> %d (exist: %d visible:%d)",
      foreground_ , in_foreground,
      (source_.get() != nullptr),
      IsVisible());
  }

  if (source_.get() == nullptr) {
    return;
  }

  foreground_ = in_foreground;

  if (updated) {
    obs_data_t *game_settings = obs_data_create();
    obs_data_set_string(game_settings, "capture_mode", "process");
    obs_data_set_int(game_settings, "process_id", game_process_id_);
    obs_data_set_bool(game_settings, "in_foreground", foreground_);
    obs_data_set_bool(game_settings, "capture_cursor", capture_game_cursor_);
    obs_data_set_bool(game_settings, "anti_cheat_hook", true);

    obs_source_update(source_->get_source(), game_settings);
    obs_data_release(game_settings);
  }

  if (foreground_) {
    SetVisible(true);
  } else if (!IsProcessAlive()) { // delay injection and game quite
    blog(LOG_WARNING, "game foreground off and game quite! (delayed?) ");
    CaptureStateChanged(source_->get_source(), false, false, nullptr);
  }
}

void GameCaptureSource::SwitchToCompatibilityMode() {
  blog(LOG_WARNING, "Switching to compatibility mode!!");

  obs_data_t *game_settings = obs_data_create();

  obs_data_set_string(game_settings, "capture_mode", "process");
  obs_data_set_int(game_settings, "process_id", game_process_id_);
  obs_data_set_bool(game_settings, "in_foreground", foreground_);
  obs_data_set_bool(game_settings, "sli_compatibility", true);
  obs_data_set_bool(game_settings, "capture_cursor", capture_game_cursor_);
  obs_data_set_bool(game_settings, "anti_cheat_hook", true);
  obs_source_update(source_->get_source(), game_settings);

  compatibility_mode_ = true;
  obs_data_release(game_settings);
}

void GameCaptureSource::RefreshTransform() {
  SetTransform(source_item_, OBS_BOUNDS_STRETCH, flip_type_);

  // if we capture the game with window source move the overlay on top
  if (move_top_) {
    obs_sceneitem_set_order(source_item_, OBS_ORDER_MOVE_TOP);
  }

  //if (!delegate_->CanvasExpended()) { // TBD
  //  SetTransform(source_item_, OBS_BOUNDS_STRETCH);
  //} else {
    //uint32_t width, hight;
    //delegate_->GetCanvasDimensions(width, hight);
    //vec2 pos{ 0, 0 };
    //obs_sceneitem_set_pos(source_item_, &pos);

    //vec2 bounds{ static_cast<float>(width /2), static_cast<float>(hight) };
    //obs_sceneitem_set_bounds(source_item_, &bounds);
    //obs_sceneitem_set_bounds_type(source_item_, OBS_BOUNDS_STRETCH);
 // }
}

void GameCaptureSource::CaptureStateChanged(obs_source_t* source,
                                            bool capture,
                                            bool compatibility_mode,
                                            const char* error) {
  bool game_process_quit = !capture && !IsProcessAlive();

  if (!did_start_capture_ && capture) {
    did_start_capture_ = true;
  }

  blog(LOG_INFO,
       "Game source capture state changed: %d [process alive: %s, sli: %s]",
       capture ? 1 : 0,
       game_process_quit ? "true" : "false",
       compatibility_mode ? "true" : "false");

  if (GameDelegate() == nullptr) {
    return;
  }

  if (capture || game_process_quit || error) {
    GameDelegate()->OnGameCaptureStateChanged(
      capture, !game_process_quit, compatibility_mode, error);
  }
  UNUSED_PARAMETER(source);
}

void GameCaptureSource::OnSetVisibility() {
  RefreshTransform();
}