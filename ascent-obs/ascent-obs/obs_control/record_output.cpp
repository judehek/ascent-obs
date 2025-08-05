#include "obs_control/record_output.h"
#include "obs_control/advanced_output.h"
#include "obs_control/obs_utils.h"
#include <communications/protocol.h>
#include <util/platform.h>

namespace obs_control {

  const char kErrorStartCaptureGenericEncoderError[] = "failed to open encoder?";
  const char kErrorFailedToStart[] = "failed to start recording";

  //------------------------------------------------------------------------------
  void OBSStartRecording(void* data, calldata_t *params) {
    RecordOutput* output = static_cast<RecordOutput*>(data);
    output->active_ = true;
    output->delay_active_ = false;
    output->OnStarted();
    blog(LOG_INFO, "recording started [id:%d]", output->identifier());
    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }

    output->advanced_output_->delegate_->OnStartedRecording(output->identifier());

    UNUSED_PARAMETER(params);
  }

  //------------------------------------------------------------------------------
  void OBSStopRecording(void *data, calldata_t *params) {
    UNUSED_PARAMETER(params);
    RecordOutput* output = static_cast<RecordOutput*>(data);
    blog(LOG_INFO, "recording stopped [id:%d]", output->identifier());

    output->active_ = false;
    output->delay_active_ = false;

    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }

    /*
    const char* - "last_error"
    int - "code" (OBS_OUTPUT_SUCCESS)
    int64_t - "duration"
    */
    const char* last_error = (const char*)calldata_string(params, "last_error");
    int64_t duration_usec = (int64_t)calldata_int(params, "duration");
    int64_t duration_ms = duration_usec / 1000;
    int code = (int)calldata_int(params, "code");

    blog(LOG_INFO,
      "recording stopped [id:%d code:%d error:%s]",
      output->identifier(), code, last_error);

    CREATE_OBS_DATA(extra_data);
    output->FillRecordingStat(extra_data);
    output->advanced_output_->delegate_->OnStoppedRecording(output->identifier(),
      code,
      last_error,
      duration_ms,
      extra_data);

    output->identifier_ = -1;
  }

  //------------------------------------------------------------------------------
  void OBSRecordStopping(void *data, calldata_t *params) {
    UNUSED_PARAMETER(params);
    RecordOutput* output = static_cast<RecordOutput*>(data);

    blog(LOG_INFO, "record stopping [id:%d]", output->identifier());
    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }

    output->advanced_output_->delegate_->OnStoppingRecording(output->identifier());

    if (output->delay_active_) {
      blog(LOG_INFO, "stop delay recording [id:%d]", output->identifier());
      // replay not really started.. so raise the stop even
      OBSStopRecording(data, params);
    }
  }

  //------------------------------------------------------------------------------
  void OBSVideoSplit(void *data, calldata_t *params) {
    UNUSED_PARAMETER(params);
    RecordOutput*output = static_cast<RecordOutput*>(data);

    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }

    const char* path = nullptr;
    calldata_get_string(params, "path", &path);
    const char* next_file_path = nullptr;
    calldata_get_string(params, "next_file_path", &next_file_path);

    // duration from start
    int64_t duration_ms = (int64_t)calldata_int(params, "duration");

    // split file duration
    int64_t split_duration = (int64_t)calldata_int(params, "split_video_duration");
    
    // last presentation epoch frame timestamp
    int64_t last_frame_pts = (int64_t)calldata_int(params, "last_frame_ts");

    blog(LOG_INFO,
      "On video split [id:%d]. path: %s duration: %d",
      output->identifier(), path, duration_ms);

    output->advanced_output_->delegate_->OnVideoSplit(
      output->identifier(),
      path ? path : "",
      duration_ms,
      split_duration,
      last_frame_pts,
      next_file_path ? next_file_path : "");
  }

};

using namespace libascentobs;
using namespace obs_control;

RecordOutput::~RecordOutput() {
  DisconnectSignals();
}

RecordOutput::RecordOutput(AdvancedOutput* advanced_output)
 :BaseOutput(advanced_output) {

}

bool RecordOutput::Initialize(obs_output_t* file_output, OBSData& error_result) {
  UNUSED_PARAMETER(error_result);
  output_ = file_output;
  ConnectSignals();
  return true;
}

bool RecordOutput::Start(int identifier, OBSData& error_result) {
  if (identifier_ != -1 && identifier != identifier) {
    blog(LOG_ERROR, "other recorder already running");
    return false;
  }

  if (Running()) {
    blog(LOG_WARNING, "same recorder already running: %d", identifier);
    return true;
  }

  identifier_ = identifier;
  //MessageBox(NULL, L"RecordOutput::Start", L"RecordOutput::Start", 0);
  if (!obs_output_start(output_)) {
    const char *error = obs_output_get_last_error(output_);
    bool driver_update_error = IsUpdateDriverError(error);
    if (!driver_update_error) {
      blog(LOG_ERROR, "failed to start recording [err: %s]", error);
      obs_data_set_int(error_result,
        protocol::kErrorCodeField,
        protocol::events::INIT_ERROR_FAILED_STARTING_OUTPUT_WITH_OBS_ERROR);

    } else {
      blog(LOG_ERROR, kErrorFailedToStart);
      obs_data_set_int(error_result,
        protocol::kErrorCodeField,
        protocol::events::INIT_ERROR_FAILED_STARTING_UPDATE_DRIVER_ERROR);
      obs_data_set_string(error_result,
        protocol::kErrorDescField,
        kErrorStartCaptureGenericEncoderError);
    }

    if (error) {
      obs_data_set_string(error_result,
        protocol::kErrorDescField,
        error);
    }
    identifier_ = -1;
    return false;
  }
  return true;
}

void RecordOutput::Stop(bool force) {
  if (!active_) {
    // in case we still not capture game frame (delay start)
    if (identifier_ != -1) {
      blog(LOG_WARNING, "Stop inactive recording [id:%d force: %d]...", identifier_, force);
      OnDelayOutputStopped();
    }
    identifier_ = -1;
    return;
  }

  __super::Stop(force);
}

//------------------------------------------------------------------------------
void RecordOutput::SplitVideo() {
  if (!this->active_) {
    blog(LOG_WARNING, "Can't split inactive video recording [id:%d]...", identifier_);
    return;
  }

  calldata_t cd = { 0 };
  proc_handler_t *ph = obs_output_get_proc_handler(output_);

  //int64_t start_time_usec = start_time_ms * 1000;
  int64_t now_split_time_usec = (os_gettime_ns() / 1000LL);
  int64_t now_split_time_epoch = os_get_epoch_time();
  utils::EpochSystemTimeToUnixEpochTime(now_split_time_epoch);

  calldata_set_int(&cd, "pts_split_time", now_split_time_usec);
  calldata_set_int(&cd, "pts_split_time_epoch", now_split_time_epoch);

  blog(LOG_INFO, "Split video [%u]", now_split_time_usec);

  if (!proc_handler_call(ph, "split_file", &cd)) {
    blog(LOG_ERROR, "fail to send split video command");
    return;
  }

  // return value
  bool success;
  calldata_get_bool(&cd, "success", &success);

  if (success) {
    return;
  }

  const char* error = nullptr;
  if (!calldata_get_string(&cd, "error", &error)) {
    error = nullptr;
  }

  blog(LOG_ERROR, "Split video command error: %s",
    error ? error : "unknown");
}

//------------------------------------------------------------------------------
void RecordOutput::ConnectSignals() {
  DisconnectSignals();

  startRecording.Connect(obs_output_get_signal_handler(output_),
    "start",
    OBSStartRecording, this);
  stopRecording.Connect(obs_output_get_signal_handler(output_),
    "stop",
    OBSStopRecording, this);
  recordStopping.Connect(obs_output_get_signal_handler(output_),
    "stopping",
    OBSRecordStopping, this);
  videoSplit.Connect(obs_output_get_signal_handler(output_),
    "video_split",
    OBSVideoSplit, this);
  diskWarning.Connect(obs_output_get_signal_handler(output_),
    "disk_space_warning",
    OBSDiskWarning, this);
}

//------------------------------------------------------------------------------
void RecordOutput::DisconnectSignals() {
  startRecording.Disconnect();
  stopRecording.Disconnect();
  recordStopping.Disconnect();
  videoSplit.Disconnect();
  diskWarning.Disconnect();
}

//------------------------------------------------------------------------------
void RecordOutput::ReportOutputStopped(
  int code, const char* last_error /*= nullptr*/) {
  advanced_output_->delegate_->OnStoppedRecording(
    identifier_, code, last_error, 0);
}

