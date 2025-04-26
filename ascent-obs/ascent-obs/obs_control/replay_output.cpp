#include "obs_control/replay_output.h"
#include "obs_control/advanced_output.h"
#include "obs_utils.h"
#include <base/critical_section_lock.h>
#include <communications/protocol.h>
#include <util/platform.h>
#include "obs_control/settings.h"

namespace obs_control {
  const char kErrorCreateReplayOutput[] = "failed to create replay output";
  const char kErrorReplayOutputExist[]  = "replay out already created";
  const char kErrorReplayOutputSignal[] = "can't connect replay signals";
  const char kErrorReplayStart[] = "failed to start replay";
  const char kErrorReplayAlreadyStart [] = "replay out already started";
  const char kErrorAlreadyCapturing[] = " replay already capturing";
  const char kErrorStartCaptureReplayOffline[] = "replays capture is offline";
  const char kErrorStartCaptureReplayOfflineDelay[] = "replays capture is offline (delay - waiting for game)";
  const char kErrorStartCaptureReplayOBSError[] = "start capture replay obs error";
  const char kErrorStartCaptureGenericEncoderError[] = "failed to open encoder?";

  void OBSStartReplayBuffer(void *data, calldata_t *params) {
    ReplayOutput *output = static_cast<ReplayOutput*>(data);

    blog(LOG_INFO, "Replay buffer started [id:%d]", output->identifier());

    UNUSED_PARAMETER(params);
    output->OnStarted();
    output->active_ = true;
    output->delay_active_ = false;
    output->last_video_thumbnail_folder_ = "";
    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }

    output->advanced_output_->delegate_->OnStartedReplay(output->identifier());
  }

  void OBSStopReplayBuffer(void *data, calldata_t *params) {
    UNUSED_PARAMETER(params);
    ReplayOutput *output = static_cast<ReplayOutput*>(data);
    output->active_ = false;
    output->delay_active_ = false;
    output->capturing_replay_ = false;

    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }

    const char* last_error = (const char*)calldata_string(params, "last_error");
    int code = (int)calldata_int(params, "code");

    blog(LOG_INFO, "Replay buffer stopped [id:%d]. code:%d",
      output->identifier(), code);

		if (output->identifier() == -1) {
			return;
		}

    CREATE_OBS_DATA(extra_data);
    output->FillRecordingStat(extra_data);
    output->advanced_output_->delegate_->OnStoppedReplay(
      output->identifier(),
      code,
      last_error,
      extra_data);

    output->identifier_ = -1;
  }

  void OBSReplayBufferStopping(void *data, calldata_t *params) {
    UNUSED_PARAMETER(params);
    ReplayOutput *output = static_cast<ReplayOutput*>(data);

    blog(LOG_INFO, "Replay buffer stopping [id:%d]", output->identifier());
    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }

    output->advanced_output_->delegate_->OnStoppingReplay(output->identifier());

    if (output->delay_active_) {
      // replay not really started.. so raise the stop even
      OBSStopReplayBuffer(data, params);
    }
  }

  ///////////////////////////////////////////////////////////////////////////

  void RunProcess(char* cmd)
  {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Start the child process.
    if (!CreateProcessA(NULL,   // No module name (use command line)
      cmd,            // Command line
      NULL,           // Process handle not inheritable
      NULL,           // Thread handle not inheritable
      FALSE,          // Set handle inheritance to FALSE
      CREATE_NO_WINDOW,              // No creation flags
      NULL,           // Use parent's environment block
      NULL,           // Use parent's starting directory
      &si,            // Pointer to STARTUPINFO structure
      &pi)           // Pointer to PROCESS_INFORMATION structure
      )
    {
      printf("CreateProcess failed (%d).\n", GetLastError());
      return;
    }

    SetPriorityClass(pi.hProcess, IDLE_PRIORITY_CLASS);

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }

  ///////////////////////////////////////////////////////////////////////////

  void OBSReplayVideoReady(void *data, calldata_t *params) {
    ReplayOutput *output = static_cast<ReplayOutput*>(data);

    output->capturing_replay_ = false;
    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }

    int64_t system_start_time = calldata_int(params, "system_start_time");
    int64_t origin_start_time = system_start_time;
    utils::EpochSystemTimeToUnixEpochTime(origin_start_time);

    const char* path;
    calldata_get_string(params, "path", &path);

    int64_t duration_ms = (int64_t)calldata_int(params, "duration");  

    blog(LOG_INFO,
         "On replay video ready [id:%d]: path:%s duration:%d start-time: %lld (%lld)",
          output->identifier(),
          path,
          duration_ms,
          system_start_time,
          origin_start_time);

    output->advanced_output_
           ->delegate_
           ->OnReplayVideoReady(output->identifier(),
                                path,
                                duration_ms,
                                origin_start_time,
                                output->last_video_thumbnail_folder_,
                                output->stop_replay_on_replay_ready_);

  }

  void OBSReplayVideoError(void *data, calldata_t *params) {
    ReplayOutput *output = static_cast<ReplayOutput*>(data);

    output->capturing_replay_ = false;
    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }

    const char* path;
    calldata_get_string(params, "path", &path);
    const char* error = NULL;
    calldata_get_string(params, "error", &error);


    blog(LOG_INFO,
         "On replay video error [id:%d]: path:%s error:%s]",
         output->identifier(), path, error);

    output->advanced_output_->delegate_->OnReplayVideoError(
      output->identifier(),
      path,
      error);
  }

  void OBSReplayVideoWarning(void *data, calldata_t *params) {
    ReplayOutput *output = static_cast<ReplayOutput*>(data);

    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }

    const char* path;
    calldata_get_string(params, "path", &path);
    const char* warning = NULL;
    calldata_get_string(params, "warning", &warning);

    blog(LOG_INFO,
      "On replay video warning [id: %d path: %s]: '%s'",
      output->identifier(), path, warning);

    CREATE_OBS_DATA(extra_data);
    obs_data_set_string(extra_data, "path", path);

    output->advanced_output_->delegate_->OnCaptureWarning(
      output->identifier(),
      warning,
      extra_data);
  }

  void OBSReplayArmed(void *data, calldata_t *params) {
    ReplayOutput *output = static_cast<ReplayOutput*>(data);

    blog(LOG_INFO, "Replay buffer fully armed [id:%d]", output->identifier());

    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }

    obs_output_t* replay_output = nullptr;
    calldata_get_ptr(params, "output", &replay_output);

    output->advanced_output_->delegate_->OnReplayArmed(output->identifier());
  }
};

using namespace libascentobs;
using namespace obs_control;

ReplayOutput::ReplayOutput(AdvancedOutput* advanced_output)
 : BaseOutput(advanced_output),
   capturing_replay_(false),
   stop_replay_on_replay_ready_(false) {
}

ReplayOutput::~ReplayOutput() {
}

bool ReplayOutput::Initialize(OBSData& error_result) {
  if (output_ == nullptr) {
    output_ = obs_output_create(
      "replay_buffer", "ReplayBuffer", nullptr, nullptr);

    if (output_ == nullptr) {
      blog(LOG_ERROR, kErrorCreateReplayOutput);
      obs_data_set_int(error_result,
        protocol::kErrorCodeField,
        protocol::events::INIT_ERROR_FAILED_CREATING_OUTPUT_FILE);
      return false;
    }
    obs_output_release(output_);
  }

  if (!ConnectSignals()) {
    obs_data_set_int(error_result,
      protocol::kErrorCodeField,
      protocol::events::INIT_ERROR_FAILED_CREATING_OUTPUT_SIGNALS);
    return false;
  }

  return true;
}


bool ReplayOutput::Start(int identifier,
                         OBSData& settings,
                         OBSData& replay_settings,
                         OBSData& error_result,
                         bool force_start /*= false*/) {

  if (Running()) {
    if (identifier == identifier_) {
      blog(LOG_WARNING, "same replay already running: %d", identifier);
      return true;
    }

    blog(LOG_ERROR, kErrorReplayAlreadyStart);
    obs_data_set_int(error_result,
      protocol::kErrorCodeField,
      protocol::events::INIT_ERROR_FAILED_STARTING_OUTPUT_ALREADY_RUNNING);
    return false;
  }

  obs_output_set_video_encoder(output_,
                               advanced_output_->recording_video_encoder_);

  SET_OBS_DATA(audio_setting, obs_data_get_obj(
    settings, settings::kSettingsAudio));

  bool separate_tracks = settings::GetAudioExtraParam(
    audio_setting, "separate_tracks");

  auto tracks = advanced_output_->GetOutputTracks("Replay", separate_tracks);
  int idx = 0;
  for (int i = 0; i < kAudioMixes; i++) {
    if ((tracks & (1 << i)) != 0) {
      obs_output_set_audio_encoder(output_,
        advanced_output_->aacTrack[i],
        idx);
      idx++;
    }
  }

  int max_size_mb = 1000;
  auto max_time_sec = obs_data_get_int(replay_settings,  "max_time_sec");
  obs_data_t* settings_data = obs_data_create();
  obs_data_set_int(settings_data, "max_time_sec", max_time_sec ? max_time_sec : 60);
  obs_data_set_int(settings_data, "max_size_mb", max_size_mb);
  obs_data_set_default_bool(settings_data, "allow_spaces", false);
  advanced_output_->applay_fragmented_file(settings_data);
  
  obs_output_update(output_, settings_data);
  obs_data_release(settings_data);

  identifier_ = identifier;

  // no need to wait for game
  if (!force_start &&
      (!advanced_output_->Active() || advanced_output_->DelayRecorderActive())) {
    // wait for game capture
    blog(LOG_INFO,
      "Starting replay buffer [id:%d replay-max-time:%d max-size:%d] is delayed. waiting for game capture to start",
      identifier_, max_time_sec, max_size_mb);
    StartAsDelay();
    return true;
  }

  if (!DoStart(error_result)) {
    return false;
  }

  blog(LOG_INFO, "Starting replay buffer [id:%d replay-max-time:%d tracks: %d] [force:%d]",
    identifier_, max_time_sec, idx, force_start);

  return true;
}

bool ReplayOutput::Start(OBSData& error_result) {
  libascentobs::CriticalSectionLock locker(sync_);
  if (identifier_ <= 0) {
    blog(LOG_ERROR, "replay start: no pending replay");
    obs_data_set_int(error_result,
      protocol::kErrorCodeField,
      protocol::events::INIT_ERROR_REPLAY_START_ERROR);
    return false;
  }

  if (Running()) {
    blog(LOG_ERROR, "replay start: already active %d", identifier_);
    obs_data_set_int(error_result,
      protocol::kErrorCodeField,
      protocol::events::INIT_ERROR_CURRENTLY_ACTIVE);
    return true;
  }

  return DoStart(error_result);
}

bool ReplayOutput::DoStart(OBSData& error_result) {
  libascentobs::CriticalSectionLock locker(sync_);
  //MessageBox(NULL, L"AAAAA", L"AAAAA", 0);

  if (identifier_ == -1) {
    blog(LOG_ERROR, "cancel start replay (stopped!)");
    obs_data_set_int(error_result,
      protocol::kErrorCodeField,
      protocol::events::INIT_ERROR_FAILED_STARTING_OUTPUT_WITH_OBS_ERROR);

    obs_data_set_string(error_result,
      protocol::kErrorDescField,
      "start replay after was stopped");

    return false;
  }

  if (!obs_output_start(output_)) {
    const char *error = obs_output_get_last_error(output_);

    blog(LOG_ERROR, kErrorReplayStart);
    blog(LOG_ERROR, "Error message: %s", error ? error : "unknown");

    bool driver_error = IsUpdateDriverError(error);
    obs_data_set_int(error_result, protocol::kErrorCodeField, !driver_error ?
        protocol::events::INIT_ERROR_FAILED_STARTING_OUTPUT_WITH_OBS_ERROR :
        protocol::events::INIT_ERROR_FAILED_STARTING_UPDATE_DRIVER_ERROR);

    obs_data_set_string(error_result,
      protocol::kErrorDescField,
      error ? error : kErrorStartCaptureGenericEncoderError);

    identifier_ = -1;
    delay_active_ = false;

    return false;
  }

  blog(LOG_INFO, "Starting replay buffer (delayed :%d) [id:%d]",
       delay_active_, identifier_);

  return true;
}

void ReplayOutput::Stop(bool force) {
  libascentobs::CriticalSectionLock locker(sync_);

  // in case we still not capture game frame (delay start)
  if (!Active()) {
    if (identifier_ != -1) {
      blog(LOG_WARNING,
           "Stop inactive replay [id:%d force: %d]...",
           identifier_, force);

      advanced_output_->delegate_->OnStoppedReplay(
        identifier_, 0, "");
    }

    identifier_ = -1;
    delay_active_ = false;
    return;
  }

  blog(LOG_INFO, "Stop replay buffer [id:%d force: %d]", identifier_, force);

  if (capturing_replay_ && !force) {
    blog(LOG_INFO, "Stop replay while active buffer");
    CREATE_OBS_DATA(error_result);
    stop_replay_on_replay_ready_ = true;
    if (DoStopActiveReplay(error_result, true)) {
      return;
    }

    blog(LOG_WARNING, "Fail to stop active replay, terminating output...");
  }

  __super::Stop(force);
}

bool ReplayOutput::StartCaptureReplay(OBSData& data, OBSData& error_result) {
  if (!this->active_) {
    if (this->delay_active_) {
      blog(LOG_ERROR, kErrorStartCaptureReplayOfflineDelay);
      obs_data_set_int(error_result,
        protocol::kErrorCodeField,
        protocol::events::REPLAY_ERROR_OFFLINE);

    } else {
      blog(LOG_ERROR, kErrorStartCaptureReplayOffline);
      obs_data_set_int(error_result,
        protocol::kErrorCodeField,
        protocol::events::REPLAY_ERROR_REPLAY_OFFLINE_DELAY);
    }
    return false;
  }

  bool success = false;
  auto start_time_ms = obs_data_get_int(data,
    "head_duration");

  auto output_file_path = obs_data_get_string(data,
    "path");

  auto output_thumbnail_folder = obs_data_get_string(data,
    "thumbnail_folder");

  blog(LOG_INFO,
      "Start capture replay [id: %d]: path:%s start time:%d",
      identifier_, output_file_path, start_time_ms);

  calldata_t cd = { 0 };
  proc_handler_t *ph = obs_output_get_proc_handler(output_);

  //int64_t start_time_usec = start_time_ms * 1000;
  //int64_t start_time = (os_gettime_ns() / 1000LL) - start_time_ms * 1000;
  calldata_set_int(&cd, "start_time", start_time_ms);
  calldata_set_string(&cd, "file_path", output_file_path);

  if (!proc_handler_call(ph, "start_capute_replay", &cd)) {
    blog(LOG_ERROR,
         "start capture replay error: can't find handler [file: %s head:%d]",
         output_file_path, start_time_ms);

    return false;
  }

  // return value
  calldata_get_bool(&cd, "success", &success);

  if (!success) {
    const char* error = nullptr;
    if (!calldata_get_string(&cd, "error", &error)) {
      error = nullptr;
    }

    blog(LOG_ERROR, "start capture replay error: %s [file: %s head:%d]",
      error ? error : "unknown", output_file_path, start_time_ms);

    obs_data_set_int(error_result,
      protocol::kErrorCodeField,
      protocol::events::REPLAY_ERROR_START_CAPTURE_OBS_ERROR);
    obs_data_set_string(error_result,
      protocol::kErrorDescField,
      error);

  } else {
    // do not set to false on error (due to already capturing error)
    capturing_replay_ = true;
    last_video_thumbnail_folder_ = output_thumbnail_folder;
  }

  calldata_free(&cd);
  return success;
}

bool ReplayOutput::StopCaptureReplay(OBSData& data, OBSData& error_result) {
  UNUSED_PARAMETER(data);
  if (!this->active_) {
    blog(LOG_ERROR, kErrorStartCaptureReplayOffline);
    obs_data_set_int(error_result,
      protocol::kErrorCodeField,
      protocol::events::REPLAY_ERROR_OFFLINE);
    return false;
  }

  if (!capturing_replay_) {
    blog(LOG_ERROR, "no active capture replay" );
    obs_data_set_int(error_result,
      protocol::kErrorCodeField,
      protocol::events::REPLAY_ERROR_STOP_CAPTURE_NO_CAPTURE);
    return false;
  }

  return DoStopActiveReplay(error_result);
}

bool ReplayOutput::DoStopActiveReplay(OBSData& error_result, bool force) {
  calldata_t cd = { 0 };
  proc_handler_t *ph = obs_output_get_proc_handler(output_);
  calldata_set_bool(&cd, "force", force);
  if (!proc_handler_call(ph, "stop_capute_replay", &cd)) {
    blog(LOG_ERROR, "start capture replay error: can't stop handler");
    obs_data_set_int(error_result,
      protocol::kErrorCodeField,
      protocol::events::REPLAY_ERROR_REPLAY_OBS_ERROR);
    return false;
  }

  blog(LOG_INFO, "Stop capture replay [id: %d force:%d]", identifier_, force);

  bool success = false;
  const char* error = { 0 };

  calldata_get_bool(&cd, "success", &success);
  if (!success) {
    calldata_get_string(&cd, "error", &error);
    blog(LOG_ERROR, "stop capture replay error: %s", error);

    obs_data_set_int(error_result,
      protocol::kErrorCodeField,
      protocol::events::REPLAY_ERROR_STOP_CAPTURE_OBS_ERROR);

    obs_data_set_string(error_result,
      protocol::kErrorDescField,
      error);
  }
  calldata_free(&cd);
  return success;
}

bool ReplayOutput::ConnectSignals() {
  signal_handler_t *signal =
    obs_output_get_signal_handler(output_);

  if (!signal) {
    blog(LOG_ERROR, kErrorReplayOutputSignal);
    return false;
  }

  DisconnectSignals();

  startReplayBuffer.Connect(signal, "start",
    OBSStartReplayBuffer, this);
  stopReplayBuffer.Connect(signal, "stop",
    OBSStopReplayBuffer, this);
  replayBufferStopping.Connect(signal, "stopping",
    OBSReplayBufferStopping, this);
  replayReady.Connect(signal, "replay_ready",
    OBSReplayVideoReady, this);
  replayError.Connect(signal, "replay_error",
    OBSReplayVideoError, this);
  replayWarning.Connect(signal, "replay_warning",
    OBSReplayVideoWarning, this);
  replayArmed.Connect(signal, "replay_fully_armed",
    OBSReplayArmed, this);
  diskWarning.Connect(signal, "disk_space_warning",
    OBSDiskWarning, this);

  return true;
}

void ReplayOutput::DisconnectSignals() {
  startReplayBuffer.Disconnect();
  stopReplayBuffer.Disconnect();
  replayBufferStopping.Disconnect();
  replayReady.Disconnect();
  replayError.Disconnect();
  replayWarning.Disconnect();
  replayArmed.Disconnect();
  diskWarning.Disconnect();
}

void ReplayOutput::ReportOutputStopped(
  int code, const char* last_error /*= nullptr*/) {

  advanced_output_->delegate_->OnStoppedReplay(
    identifier_, code, last_error);
}

