#ifndef ASCENTOBS_OBS_CONTROL_OBS_H_
#define ASCENTOBS_OBS_CONTROL_OBS_H_

#include <memory>
#include <obs.hpp>
#include <base/macros.h>
#include <base/thread.h>
#include <base/critical_section_lock.h>

#include "obs_control/advanced_output.h"
#include "obs_control/scene/game_capture_source_delegate.h"
#include "obs_control/obs_control_communications.h"
#include "obs_control/obs_display_tester.h"
#include <base/timer_queue_timer.h>
#include <mutex>
#include "obs_control/obs_audio.h"


class Source;
class SceneContext;
class MonitorSource;
class WindowSource;
class GenericObsSource;
class BRBSource;
class GameCaptureSource;
class GazeOverlaySource;
class DisplayContext;

namespace obs_control {

class OBSAudioSourceControl;

class OBS : public AdvancedOutputDelegate,
            public GameCaptureSourceDelegate,
            public OBSDisplayTester::Delegate,
            public libowobs::TimerQueueTimerDelegate {
public:
  OBS();
  virtual ~OBS();

public:
  bool Startup(OBSControlCommunications* communications,
               libowobs::SharedThreadPtr command_thread);

  bool Recording() const;

  void InitAudioSources(OBSData& audio_settings);

  bool InitVideo(OBSData& video_settings, 
                 OBSData& extra_video_settings,
                 OBSData& error_result);

  void RegisterDisplay();

  bool InitVideoEncoder(OBSData& video_encoder_settings,
                        OBSData& video_extra_options,
                        OBSData& error_result,
                        const char* type = nullptr);

  bool InitScene(OBSData& scene_settings,
                 OBSData& error_result);

  bool AddGameSource(OBSData& game_settings);

  bool LoadModules();

  bool IsWinrtCaptureSupported();

  void RetreiveSupportedVideoEncoders(OBSDataArray& encoders);

  void RetreiveAudioDevices(const char* source_id, OBSDataArray& devices);

  bool ResetOutputSetting(OBSData& output_settings,
                          OBSData& audio_setting,
                          OBSData& error_result);

  bool StartDelayRecording(int identifier);

  bool StartRecording(int identifier,
                      OBSData& error_result);

  bool StartReplay(int identifier,
                   OBSData& settings,
                   OBSData& replay_settings,
                   OBSData& error_result);

  bool StartStreaming(int identifier,
                      OBSData& stream_setting,
                      OBSData& error_result);

  bool Stop(int identifier, int recorder_type, bool force = false);

  bool StopRecording(bool force = false);
  bool StopReplay(bool force = false);
  bool StopStreaming(bool force = false);
  void SplitVideo();

  bool StartCaptureReplay(OBSData& data, OBSData& error_result);
  bool StopCaptureReplay(OBSData& data, OBSData& error_result);

  bool UpdateTobiiGazaSource(OBSData& data);
  bool UpdateBRB(OBSData& data);

  void UpdateSourcesVisiblity(bool game_in_foreground, bool is_minimized);

  inline bool has_window_source()  { return window_source_.get() != nullptr; }
  inline bool has_monitor_source() { return monitor_source_.get() != nullptr; }

  virtual bool UsingGameSource();

  virtual bool HasDelayGameSource();

  virtual bool DelayedGameCaptureFailure() {
    return game_source_capture_failure_;
  }

  const uint32_t& output_width() {
    return output_width_;
  }

  const uint32_t& output_height() {
    return output_height_;
  }


  bool IsActive();

  void Shutdown();

  OBSAudioControl* audio_control() {
    return obs_audio_controller_.get();
  }

  //--------------------------GameCaptureSourceDelegate-------------------------
  virtual void OnGameCaptureStateChanged(bool capturing,
                                         bool is_process_alive,
                                         bool compatibility_mode,
                                         const char* error);

  //-------------------------- Source::Delegate -------------------------------
  virtual void GetCanvasDimensions(uint32_t& output_width,
                                   uint32_t& output_height) {
    output_width = output_width_;
    output_height = output_height_;
  }

// AdvancedOutputDelegate
private:

  void HandleGameCaptureStateChanged(bool capturing,
                                      bool is_process_alive,
                                      bool compatibility_mode,
                                      std::string error);
  // delayed replay
  bool StartCaptureReplay(OBSData& error_result);
  virtual void OnStartedRecording(int identifier);
  virtual void OnStoppingRecording(int identifier);
  virtual void OnStoppedRecording(int identifier,
                                  int code,
                                  const char* last_error,
                                  int64_t duration_ms,
                                  obs_data_t* stats_data = nullptr);
  virtual void OnVideoSplit(int identifier,
                            std::string path,
                            int64_t duration,
                            int64_t split_file_duration,
                            int64_t last_frame_pts,
                            std::string next_video_path);

  //ReplayOutputDelegate
  virtual void OnStartedReplay(int identifier);
  virtual void OnStoppingReplay(int identifier);
  virtual void OnStoppedReplay(int identifier,
                               int code,
                               const char* last_error,
                               obs_data_t* stats_data = nullptr);
  virtual void OnReplayVideoReady(int identifier,
                                  std::string path,
                                  int64_t duration,
                                  int64_t video_start_time,
                                  std::string thumbnail_folder,
                                  bool stop_stream);
  virtual void OnReplayVideoError(int identifier,
                                  std::string path, std::string error);
  virtual void OnReplayArmed(int identifier);

  //StreamOutputDelegate
  virtual void OnStartingStreaming(int identifier);
  virtual void OnStartedStreaming(int identifier);
  virtual void OnStoppingStreaming(int identifier);
  virtual void OnStoppedStreaming(int identifier,
                                  int code,
                                  const char* last_error,
                                  obs_data_t* stats_data = nullptr);

  //BaseOutputDelegate
  virtual void OnCaptureWarning(int identifier,
                                 const char* message,
                                 obs_data_t* stats_data = nullptr);

  // OBSDisplayTester::Delegate,
  virtual void OnBlackTextureDetected(OBSDisplayTester::TestSouceType type);
  virtual void OnColoredTextedDetected(OBSDisplayTester::TestSouceType type);
  virtual Source* GetSource(OBSDisplayTester::TestSouceType source_type);

  //libowobs::TimerQueueTimerDelegate
  virtual void OnTimer(libowobs::TimerQueueTimer* timer);

  void OnGameQuite(bool force);

private:
  bool InitMonitorSource(OBSData& monitor_handle, OBSData& error_result);
  bool InitWindowSource(OBSData& window_setting, OBSData& error_result);
  bool InitBRBSource(OBSData& brb_setting, OBSData& error_result);
  bool InitTobiiGazeSource(OBSData& gaze_setting);
  bool InitGameSource(OBSData& game_setting,
                      OBSData& error_result,
                      bool&    foreground,
                      bool     capture_window=false);

  bool InitGenericOBSSource(OBSData& generic_source_handle,
        OBSData& error_result, int index);

  bool HasAudioDevices(const char* source_id);

  void NotifyGameSourceChangedSafe();
  void NotifyGameSourceChanged();
  void StopRecordingOnGameSourceExit();
  void StopDisplayTest();

  void NotifyPossibleSwitchableDevices();

  void RemoveGameSource();


  // Check if the encoder is Valid (on startup for each encoder)
  bool IsEncoderValid(const char* type,
                      std::string& status,
                      std::string& code,
                      const char* codec);

  bool IsEncoderValidSafe(const char* type,
    std::string& status,
    std::string& code,
    const char* codec);


private:
  bool DoInitVideo(OBSData& video_settings,
                  OBSData& extra_video_settings,
                  OBSData& error_result);

  void OnOutputStopped();

  inline bool has_game_source() { return game_source_.get() != nullptr; }
  bool is_replay_capture_in_progress();

  static void RenderWindow(void *data, uint32_t cx, uint32_t cy);

  void SetVisibleSource(Source* new_visible_source);
  bool SetVisibleSourceName(OBSData& data);
  std::string GetVisibleSource();

  void OnStatTimer();
  void OnStopReplayTimer();

private:
  static bool gs_enum_adapters_callback(void *param, const char *name, uint32_t id);
  void StartPendingDelayRecording();

  void ApplyCustomParamters(OBSData& video_custom_parameters);

  void CreateGenericSourcesFromCustomParam(obs_data_array_t* sources);

private:
  std::unique_ptr<OBSAudioControl> obs_audio_controller_;
  // NOTE(twolf): do not call obs_shutdown explicitly, as it is called
  // implicitly by ~OBSContext
  OBSControlCommunications* communications_;
  libowobs::SharedThreadPtr command_thread_;
  std::unique_ptr<AdvancedOutput> advanced_output_;

  std::unique_ptr<SceneContext> scene_;

  Source* current_visible_source_ = nullptr;
  std::unique_ptr<MonitorSource> monitor_source_;
  std::unique_ptr<WindowSource> window_source_;
  std::unique_ptr<BRBSource> brb_source_;
  std::unique_ptr<GameCaptureSource> game_source_;
  std::unique_ptr<GazeOverlaySource> tobii_source_;
  std::vector<std::unique_ptr<Source>> generic_obs_source_;

  // from CustomParamters
  OBSDataArray custom_source_setting_;

  int split_video_counter_;
  uint32_t  output_width_;
  uint32_t  output_height_;

  bool compatibility_mode_;

  bool capture_game_cursor_;

  bool did_notify_switchable_devices_;

  gs_texture_t*  black_texture_tester_;
  bool test_black_texture_;
  uint32_t next_black_test_time_stamp;
  int  black_texture_detection_counter;

  std::unique_ptr<DisplayContext> display_context_;

  std::unique_ptr<OBSDisplayTester> display_tester_;

  std::unique_ptr<libowobs::TimerQueueTimer> stats_time_;
  std::unique_ptr<libowobs::TimerQueueTimer> stop_replay_timer_;

  libowobs::CriticalSection sync_;

  libowobs::CriticalSection visible_source_sync_;

  bool shoutdown_on_stop_;

  std::string adapter_name_;

  OBSData pending_tobii_;

  std::mutex access_mutex_;

  // game source wasn't frame wasn't capture...
  bool game_source_capture_failure_ = false;

  bool keep_recording_on_lost_focus_ = false;

  bool disable_shutdown_on_game_exit_ = false;

  DISALLOW_COPY_AND_ASSIGN(OBS);
};

}; // namespace obs_control

#endif // ASCENTOBS_OBS_CONTROL_OBS_H_