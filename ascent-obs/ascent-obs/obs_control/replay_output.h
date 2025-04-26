#ifndef ASCENTOBS_OBS_OBS_CONTROL_REPLAY_OUTPUT_H_
#define ASCENTOBS_OBS_OBS_CONTROL_REPLAY_OUTPUT_H_
#include "base_output.h"

namespace obs_control {

struct ReplayOutputDelegate {
  virtual void OnStartedReplay(int identifier) = 0;
  virtual void OnStoppingReplay(int identifier) = 0;
  virtual void OnStoppedReplay(int identifier,
                               int code,
                               const char* last_error,
                               obs_data_t* stats_data = nullptr) = 0;
  virtual void OnReplayVideoReady(int identifier,
                                  std::string path,
                                  int64_t duration,
                                  int64_t video_start_time,
                                  std::string thumbnail_folder,
                                  bool stop_stream) = 0;
  virtual void OnReplayVideoError(int identifier,
                                  std::string path,
                                  std::string error) = 0;
  virtual void OnReplayArmed(int identifier) = 0;
};


class ReplayOutput : public BaseOutput {
public:
  virtual ~ReplayOutput();

  bool Initialize(OBSData& error_result);

  bool Start(int identifier,
             OBSData& settings,
             OBSData& replay_settings,
             OBSData& error_result,
             bool force_start /*= false*/);

  virtual void Stop(bool force);

  bool StartCaptureReplay(OBSData& data, OBSData& error_result);

  bool StopCaptureReplay(OBSData& data, OBSData& error_result);

  virtual void DisconnectSignals();
  virtual const char* Type() { return "replay"; }

  bool capture_in_progress() {
    return capturing_replay_;
  }

private:
  // delay start
  bool Start(OBSData& error_result);

  bool DoStart(OBSData& error_result);

  bool DoStopActiveReplay(OBSData& error_result, bool force = false);

  bool ConnectSignals();

protected:
  virtual void ReportOutputStopped(int code,
    const char* last_error = nullptr);

  friend class AdvancedOutput;
  friend void OBSStartReplayBuffer(void *data, calldata_t *params);
  friend void OBSStopReplayBuffer(void *data, calldata_t *params);
  friend void OBSReplayBufferStopping(void *data, calldata_t *params);
  friend void OBSReplayVideoReady(void *data, calldata_t *params);
  friend void OBSReplayVideoError(void *data, calldata_t *params);
  friend void OBSReplayVideoWarning(void *data, calldata_t *params);
  friend void OBSReplayArmed(void *data, calldata_t *params);
  friend void OBSDiskWarning(void *data, calldata_t *params);

  ReplayOutput(AdvancedOutput* advanced_output);

  DISALLOW_IMPLICIT_CONSTRUCTORS(ReplayOutput);

private:
  OBSSignal startReplayBuffer;
  OBSSignal stopReplayBuffer;
  OBSSignal replayBufferStopping;
  OBSSignal replayReady;
  OBSSignal replayError;
  OBSSignal replayWarning;
  OBSSignal replayArmed;
  OBSSignal diskWarning;

  bool capturing_replay_;
  bool stop_replay_on_replay_ready_;
  std::string last_video_thumbnail_folder_;
  libascentobs::CriticalSection sync_;
}; // class ReplayOutput
}; // namespace obs_control
#endif //ASCENTOBS_OBS_OBS_CONTROL_REPLAY_OUTPUT_H_
