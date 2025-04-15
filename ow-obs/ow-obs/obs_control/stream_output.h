/*****  **************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#ifndef OWOBS_OBS_OBS_CONTROL_STREAM_OUTPUT_H_
#define OWOBS_OBS_OBS_CONTROL_STREAM_OUTPUT_H_
#include "base_output.h"

namespace obs_control {
struct StreamOutputDelegate {
  virtual void OnStartingStreaming(int identifier) = 0;
  virtual void OnStartedStreaming(int identifier) = 0;
  virtual void OnStoppingStreaming(int identifier) = 0;
  virtual void OnStoppedStreaming(int identifier,
                                  int code,
                                  const char* last_error,
                                  obs_data_t* stats_data = nullptr) = 0;
};

class AdvancedOutput;

class StreamOutput : public BaseOutput {
public:
  virtual ~StreamOutput();

  bool Initialize(OBSData& error_result, const char* type);

  bool Start(int identifier, 
             OBSData& replay_settings,
             OBSData& error_result);
  
  void Stop(bool force);

  bool Active() const;
  virtual void DisconnectSignals();

  virtual const char* Type() { return "stream"; }

protected:
  virtual void ReportOutputStopped(int code,
                                   const char* last_error = nullptr);

private:
  OBSService GetService(const char* type);
  // delay start
  bool Start(OBSData& error_result);

  bool ConnectSignals();
private:
  friend class AdvancedOutput;

  friend void OBSStreamStarting(void *data, calldata_t *params);
  friend void OBSStreamStopping(void *data, calldata_t *params);
  friend void OBSStartStreaming(void *data, calldata_t *params);
  friend void OBSStopStreaming(void *data, calldata_t *params);

  StreamOutput(AdvancedOutput* advanced_output);

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamOutput);

private:
  OBSService service_;
  bool is_custom_server_;

  OBSSignal startStreaming;
  OBSSignal stopStreaming;
  OBSSignal streamStopping;
  OBSSignal streamDelayStarting;
  OBSSignal diskWarning;

  bool capturing_replay_;
  std::string last_video_thumbnail_folder_;

}; // class ReplayOutput
}; // namespace obs_control
#endif //OWOBS_OBS_OBS_CONTROL_STREAM_OUTPUT_H_
