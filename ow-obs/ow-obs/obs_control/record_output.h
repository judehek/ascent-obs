/*****  **************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#ifndef c
#define OWOBS_OBS_OBS_CONTROL_RECORD_OUTPUT_H_
#include "base_output.h"

namespace obs_control {
struct RecordOutputDelegate {
  virtual void OnStartedRecording(int identifier) = 0;
  virtual void OnStoppingRecording(int identifier) = 0;
  virtual void OnStoppedRecording(int identifier,
                                  int code,
                                  const char* last_error,
                                  int64_t duration_ms,
                                  obs_data_t* stats_data = nullptr) = 0;
  virtual void OnVideoSplit(int identifier,
                            std::string path,
                            int64_t duration,
                            int64_t split_file_duration,
                            int64_t last_frame_pts,
                            std::string next_video_path) = 0;
};

class AdvancedOutput;

class RecordOutput : public BaseOutput {
public:
  virtual ~RecordOutput();

  bool Initialize(obs_output_t* file_output, OBSData& error_result);

  bool Start(int identifier, OBSData& error_result);

  virtual void Stop(bool force);

  void StartDelayRecording(int identifier) {
    //waiting for game frame
    identifier_ = identifier;
    StartAsDelay();
  }

  void SplitVideo();
  
  virtual const char* Type() { return "video_recorder"; }
  virtual void DisconnectSignals();

protected:
  virtual void ReportOutputStopped(int code,
    const char* last_error = nullptr);

private:
  friend class AdvancedOutput;
  friend void OBSStartRecording(void *data, calldata_t *params);
  friend void OBSStopRecording(void *data, calldata_t *params);
  friend void OBSRecordStopping(void *data, calldata_t *params);
  friend void OBSVideoSplit(void *data, calldata_t *params);
  RecordOutput(AdvancedOutput* advanced_output);

  DISALLOW_IMPLICIT_CONSTRUCTORS(RecordOutput);

  void ConnectSignals();

private:
  OBSSignal startRecording;
  OBSSignal stopRecording;
  OBSSignal recordStopping;
  OBSSignal videoSplit;
  OBSSignal diskWarning;

}; // class RecordOutput
}; // namespace obs_control
#endif //OWOBS_OBS_OBS_CONTROL_STREAM_OUTPUT_H_
#pragma once
