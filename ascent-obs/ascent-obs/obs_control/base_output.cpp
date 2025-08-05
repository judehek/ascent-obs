#include "obs_control/base_output.h"
#include "obs_control/advanced_output.h"
#include "obs_control/obs_utils.h"
#include "base/critical_section_lock.h"

namespace obs_control {
  const char kLaggedFramesWarnning[]  = "perforamnce_lagged_frames";
  const char kHighCpuUsagesWarnning[] = "perforamnce_high_cpu_usages";
  const int KDelayFrameCounter = 1000;

  //---------------------------------------------------------------------------

  void OBSDiskWarning(void *data, calldata_t *params) {
    ReplayOutput *output = static_cast<ReplayOutput*>(data);

    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }

    const char* path;
    calldata_get_string(params, "path", &path);
    const char* warning = NULL;
    calldata_get_string(params, "warning", &warning);

    blog(LOG_INFO, "On disk space warning [id: %d path: %s]: '%s'",
                    output->identifier(), path, warning);

    CREATE_OBS_DATA(extra_data);
    obs_data_set_string(extra_data, "path", path);

    output->advanced_output_->delegate_->OnCaptureWarning(
      output->identifier(),
      warning,
      extra_data);
  }
}


using namespace libascentobs;
using namespace obs_control;

BaseOutput::BaseOutput(AdvancedOutput* advanced_output)
 : advanced_output_(advanced_output),
   identifier_(-1),
   active_(false) ,
   delay_active_(false){
}

void BaseOutput::TestStats() {
  if (!active_ || identifier_ == -1) {
    return;
  }

  if (!output_) {
    return;
  }

  //drop frames
  obs_output_t* obs_output = output_;
  if (obs_output == nullptr) {
    return;
  }

  uint32_t drawn = 0;
  uint32_t lagged = 0;
  double percentage_lagged =
    get_lagged_frames_percentage(&drawn, &lagged);

  if (drawn) {
    percentage_lagged = (double)lagged / (double)drawn * 100.0;
  }

  if (drawn && lagged && percentage_lagged) {
    libascentobs::CriticalSectionLock locker(sync_);
    if (percentage_lagged - last_drop_frame_ratio_ >= 5.0) {
      last_drop_frame_ratio_ = percentage_lagged;

      CREATE_OBS_DATA(extra_data);
      blog(LOG_WARNING, "Output '%s (id:%d)': Number of lagged frames due "
            "to rendering lag/stalls: %""u"" (%0.1f%%)",
            Type(),
            identifier_,
            lagged, percentage_lagged);

      obs_data_set_obj(extra_data, "system_info", advanced_output_->system_game_info_);
      obs_data_set_int(extra_data, "percentage_lagged", (int)(percentage_lagged));

      advanced_output_->delegate_->OnCaptureWarning(
        identifier_,
        kLaggedFramesWarnning,
        extra_data);
    }
  }

  int total_frames = obs_output_get_total_frames(obs_output);
  int skipped = video_output_get_skipped_frames(obs_get_video());
  int diff = skipped - skipped_frame_counter_;
  double percentage = double(skipped) / double(total_frames) * 100.0;

  if (diff > 10 && percentage >= 0.1f) {
    libascentobs::CriticalSectionLock locker(sync_);
    if (!notify_high_cpu_) {
      blog(LOG_WARNING, "HighResourceUsage id:%d (%s): skipped %d (%f)",
        identifier_, Type(), diff, percentage);
      advanced_output_->delegate_->OnCaptureWarning(
        identifier_,
        kHighCpuUsagesWarnning);
      notify_high_cpu_ = true;
    }
  } else {
    notify_high_cpu_ = false;
  }

  skipped_frame_counter_ = skipped;
}

bool BaseOutput::IsUpdateDriverError(const char* error) {
  if (!error || strlen(error) == 0) {
    return false;
  }
  
  const char* outdated_drivers = "NVENC.OutdatedDriver";
  const char* check_drivers = "NVENC.CheckDrivers";

  return strstr(error, outdated_drivers) || strstr(error, check_drivers);
}

void BaseOutput::FillRecordingStat(obs_data_t* data) {
  if (data == nullptr)
    return;

  uint32_t drawn = 0;
  uint32_t lagged = 0;
  double percentage_lagged =
    get_lagged_frames_percentage(&drawn, &lagged);

  int dropped = obs_output_get_frames_dropped(output_);
  int total_frames = obs_output_get_total_frames(output_);

  double percentage_dropped = 0.0f;
  if (dropped && total_frames) {
    percentage_dropped = (double)dropped / (double)total_frames * 100.0;
  }

  obs_data_set_obj(data, "system_info", advanced_output_->system_game_info_);
  obs_data_set_int(data, "percentage_lagged", (int)(percentage_lagged));
  obs_data_set_int(data, "drawn", drawn);
  obs_data_set_int(data, "lagged", lagged);
  obs_data_set_int(data, "dropped", dropped);
  obs_data_set_int(data, "total_frames", total_frames);
  obs_data_set_int(data, "percentage_dropped", (int)(percentage_dropped));
}

void BaseOutput::Stop(bool force) {
  if (!output_) {
    return;
  }

  if (!Active()) {
    return;
  }

  if (delay_active_) { // no active yet..
    blog(LOG_INFO, "skip obs stop id:%d (%s),due to delay activation",
                   identifier_, Type());
    delay_active_ = false;
    OnDelayOutputStopped();
    return;
  }

  if (force) {
    obs_output_force_stop(output_);
  } else {
    obs_output_stop(output_);
  }
}

double BaseOutput::get_lagged_frames_percentage(
  uint32_t* out_total_drawn_frames, uint32_t* out_total_lagged_frames) {
  if (!output_) {
    return 0.0;
  }

  obs_output_t* obs_output = output_;
  if (obs_output == nullptr)
    return 0.0;

  uint32_t drawn = obs_output_get_info_drawn_frame(obs_output);
  if (drawn < KDelayFrameCounter) {
    return 0.0;
  }

  uint32_t lagged = obs_output_get_info_lagged_frame(obs_output);

  if (skip_delay_frames_lagged_ < 0) {
    skip_delay_frames_drawn_ = drawn;
    skip_delay_frames_lagged_ = lagged;
  }

  drawn  -= skip_delay_frames_drawn_;
  lagged -= skip_delay_frames_lagged_;
  double percentage_lagged = 0.0f;
  if (drawn) {
    percentage_lagged = (double)lagged / (double)drawn * 100.0;
  }

  if (out_total_drawn_frames) {
    *out_total_drawn_frames = drawn;
  }

  if (out_total_lagged_frames) {
    *out_total_lagged_frames = lagged;
  }

  return percentage_lagged;
}


void BaseOutput::OnDelayOutputStopped() {
  if (!advanced_output_ || !advanced_output_->delegate_) {
    return;
  }

  auto delegate = advanced_output_->delegate_;

  // in case the was quite before we even has the chance to
  // inject ascent-graphics-dll, do not report as error
  bool report_as_error =
    (delegate->HasDelayGameSource() || delegate->DelayedGameCaptureFailure()) &&
    GetTickCount64() - delay_start_time_ > kReportFailToStartGamedDelay;

  blog(LOG_WARNING,
      "Stop inactive output [id:%d] (as error: %d).",
       identifier_, report_as_error);

  // -999 is just a dummy number
   // and will use the error string (kErrorStartDelayRecordingError)
  ReportOutputStopped(
    report_as_error ? -999 : 0,
    report_as_error ? kErrorStartDelayRecordingError : nullptr);
}
