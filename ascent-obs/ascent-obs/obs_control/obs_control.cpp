#include "obs_control/obs_control.h"
#include <communications/protocol.h>
#include "obs_control/obs_utils.h"
#include "base/thread.h"

#include "obs_control/commands/command_query_machine_info.h"
#include "obs_control/commands/command_start.h"
#include "obs_control/commands/command_stop.h"
#include "obs_control/commands/command_set_volume.h"
#include "obs_control/commands/command_game_focus_changed.h"
#include "obs_control/commands/command_add_game_source.h"
#include "obs_control/commands/command_start_replay_capture.h"
#include "obs_control/commands/command_stop_replay_capture.h"
#include "obs_control/commands/command_update_tobii_gaze.h"
#include "obs_control/commands/command_set_brb.h"
#include "obs_control/commands/command_split_video.h"

#include <windows.h>

namespace obs_control {
#define INIT_COMMAND(id, class) commands_map_[id] = new class(obs_.get(), communications_);
};

using namespace obs_control;
using namespace libowobs;

//------------------------------------------------------------------------------
OBSControl::OBSControl() {
  obs_.reset(new OBS());
}

//------------------------------------------------------------------------------
OBSControl::~OBSControl() {
  if (command_thread_.get()) {
    command_thread_->Stop(false, 2000);
  }

  obs_.reset();

  if (initialized_) {
    blog(LOG_INFO, "obs shutdown");
    obs_shutdown();
  }
}

//------------------------------------------------------------------------------
bool OBSControl::Init(OBSControlCommunications* communications) {
  if (nullptr == communications) {
    return false;
  }

  communications_ = communications;
  command_thread_.reset(new libowobs::Thread());

  if (!command_thread_->Start("obs_command_thread", true)) {
    blog(LOG_WARNING, "fail to start obs command thread");
    //return false;
  }

  // NOTE(twolf): it is important we set the working directory before calling
  // |LoadModules| as the OBS code looks at a relative path when trying to find
  // plugins
  SetWorkingDirectory();

  if (!obs_->Startup(communications, command_thread_)) {
    return false;
  }

  CREATE_OBS_DATA(audio_settings);
  obs_->audio_control()->ResetAudio(audio_settings);

  // NOTE(twolf): not calling |ResetVideo| before |LoadModules| might mean we
  // won't get encoders to work
  CREATE_OBS_DATA(video_settings);
  CREATE_OBS_DATA(error_result);
  CREATE_OBS_DATA(extra_video_setting);
  if (obs_->InitVideo(video_settings, extra_video_setting, error_result)) {
    obs_->RegisterDisplay();
  }

  obs_->LoadModules();

  INIT_COMMAND(protocol::commands::QUERY_MACHINE_INFO, CommandQueryMachineInfo);
  INIT_COMMAND(protocol::commands::START, CommandStart);
  INIT_COMMAND(protocol::commands::STOP, CommandStop);
  INIT_COMMAND(protocol::commands::SET_VOLUME, CommandSetVolume);
  INIT_COMMAND(protocol::commands::GAME_FOCUS_CHANGED, CommandGameFocusChanged);
  INIT_COMMAND(protocol::commands::ADD_GAME_SOURCE, CommandAddGameSource);
  INIT_COMMAND(protocol::commands::START_REPLAY_CAPTURE, CommandStartReplayCapture);
  INIT_COMMAND(protocol::commands::STOP_REPLAY_CAPTURE, CommandStopReplayCapture);
  INIT_COMMAND(protocol::commands::TOBII_GAZE, CommandTobiiGaze);
  INIT_COMMAND(protocol::commands::SET_BRB, CommandSetBRB);
  INIT_COMMAND(protocol::commands::SPLIT_VIDEO, CommandSplitVideo);

  initialized_ = true;

  return true;
}

//------------------------------------------------------------------------------
bool OBSControl::HandleCommand(int command_id, int identifier, OBSData& data) {
  auto command = commands_map_.find(command_id);
  if (command == commands_map_.end()) {
    return false;
  }

  if (nullptr == command->second) {
    return false;
  }

  blog(LOG_INFO, "Handle command %d (%d)", command_id, identifier);

  try {
    command->second->Perform(identifier, data);
  } catch (...) {
    blog(LOG_ERROR, "Handle command %d (%d) ERROR!", command_id, identifier);
  }
  return true;
}

//------------------------------------------------------------------------------
void OBSControl::SetWorkingDirectory() {
  WCHAR path[MAX_PATH] = { 0 };
  GetModuleFileNameW(NULL, path, MAX_PATH);
  std::wstring full_path = path;

  std::size_t found = full_path.rfind(L"\\");
  if (found != std::string::npos) {
    full_path[found] = NULL;
  }

  if (!SetCurrentDirectoryW(full_path.c_str())) {
    // TODO(twolf): add error log
  }
}

//------------------------------------------------------------------------------
void OBSControl::Shutdown() {
  obs_->Shutdown();
}

