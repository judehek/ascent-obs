#pragma once
#include <string>
#include <base/macros.h>

#include <obs.h>
#include <obs.hpp>
#include <base/critical_section.h>

namespace obs_control {

const char kErrorStartDelayRecordingError[] = "failed to start game recording";
const int kReportFailToStartGamedDelay = 30 * 1000; // 30 second

class AdvancedOutput;

struct BaseOutputDelegate {
  virtual void OnCaptureWarning(int identifier, 
                                 const char* message,
                                 obs_data_t* extra = nullptr) = 0;
};

class BaseOutput {
public:
  virtual ~BaseOutput() { DisconnectSignals(); };
  
  virtual void Stop(bool force);
  virtual void TestStats();  

  virtual void DisconnectSignals() {}

  const int& identifier() {
    return identifier_;
  }

  bool Active() const {
    return active_ || delay_active_;
  }

  bool Running() {
    return active_;
  }

  bool DelayActive() {
    if (identifier_ == -1)
      return false;

    return delay_active_ && !active_;
  }

  virtual const char* Type() = 0;

  virtual void FillRecordingStat(obs_data_t* data);

protected:
  friend class AdvancedOutput;
  DISALLOW_IMPLICIT_CONSTRUCTORS(BaseOutput);

  BaseOutput(AdvancedOutput* advanced_output);
  void OnStarted() {
    skipped_frame_counter_ = 0;
    last_drop_frame_ratio_ = 0.0;
    notify_high_cpu_ = false;
  }

  virtual void OnDelayOutputStopped();

  virtual void ReportOutputStopped(int code,
                                   const char* last_error = nullptr) = 0;

  virtual void StartAsDelay() {
    delay_active_ = true;
    delay_start_time_ = GetTickCount64();
  }

  double get_lagged_frames_percentage(uint32_t* out_total_drawn_frames,
                                      uint32_t* out_total_lagged_frames);

  static bool IsUpdateDriverError(const char* error);

protected:
  libascentobs::CriticalSection sync_;
  OBSOutput output_;
  AdvancedOutput* advanced_output_; // owner
  int identifier_;


  int  skipped_frame_counter_ = 0;
  double  last_drop_frame_ratio_ = 0.0;
  bool notify_high_cpu_ = false;

  int skip_delay_frames_lagged_ = -1;
  int skip_delay_frames_drawn_ = -1;

  bool active_;
  bool delay_active_;

  ULONGLONG delay_start_time_ = 0;

};
} // obs_control