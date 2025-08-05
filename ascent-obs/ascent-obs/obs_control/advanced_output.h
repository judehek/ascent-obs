#ifndef ASCENTOBS_OBS_OBS_CONTROL_ADVANCED_OUTPUT_H_
#define ASCENTOBS_OBS_OBS_CONTROL_ADVANCED_OUTPUT_H_

#include <string>
#include <memory>
#include <base/macros.h>
#include "replay_output.h"
#include "stream_output.h"
#include "record_output.h"
#include "obs_control/settings.h"


#include <obs.h>
#include <obs.hpp>

namespace obs_control {

extern const int kAudioMixes;

struct AdvancedOutputDelegate : public BaseOutputDelegate,
                                public ReplayOutputDelegate,
                                public RecordOutputDelegate,
                                public StreamOutputDelegate {

public:
  virtual bool HasDelayGameSource() = 0;
  virtual bool DelayedGameCaptureFailure() = 0;

};

class AdvancedOutput {
public:
  static AdvancedOutput* Create(AdvancedOutputDelegate* delegate,
                                OBSData& video_encoder_settings,
                                OBSData& error_result);
  virtual ~AdvancedOutput();

public:
  bool ResetOutputSetting(OBSData& output_settings,
                          OBSData& audio_setting,
                          OBSData& error_result);

  void StartDelayRecording(int identifier) {
    //waiting for game frame
    if (record_output_.get()) {
      record_output_->StartDelayRecording(identifier);
    }
  }

  bool StartRecording(int identifier,
                      OBSData& error_result);
  bool StartReplay(int identifier,
                   OBSData& settings,
                   OBSData& replay_settings,
                   OBSData& error_result,
                   bool force_start); // do not wait for game
  bool StartReplay(OBSData& error_result);

  bool StartStreaming(int identifier,
                      OBSData& streaming_settings,
                      OBSData& error_result);

  void StopRecording(bool force);
  void StopReplay(bool force);
  void StopStreaming(bool force);

  void SplitVideo();

  bool StartCaptureReplay(OBSData& data, OBSData& error_result);

  bool StopCaptureReplay(OBSData& data, OBSData& error_result);

  inline void UpdateAudioSettings();
  void Update();

  inline void SetupRecording();
  void SetupOutputs();
  int GetAudioBitrate(size_t i) const;

  bool RecordingActive() const;
  bool ReplayActive() const;
  bool StreamActive() const;

  inline bool Active() const {
    return RecorderActive() ||
           ReplayActive()   ||
           StreamActive();
  }

  inline bool DelayRecorderActive() {
    if (!record_output_.get())
      return false;

    return record_output_->DelayActive();
  }

  inline bool DelayReplayActive() {
    if (!replay_output_.get())
      return false;

    return replay_output_->DelayActive();
  }

  inline bool DelayActive() {
    return DelayRecorderActive() || DelayReplayActive();
  }


  inline bool RecorderActive() const {
    if (!record_output_.get())
      return false;

    return record_output_->Active();
  }

  inline void TestStats() {
    if (record_output_) {
      record_output_->TestStats();
    }

    if (stream_output_) {
      stream_output_->TestStats();
    }

    if (replay_output_) {
      replay_output_->TestStats();
    }
  }

  const int identifier() const {
    if (!record_output_.get())
      return -1;

    return record_output_->identifier();
  }

  const int replay_identifier() const {
    if (!replay_output_.get())
      return -1;

    return replay_output_->identifier();
  }

  const int streaming_identifier() const {
    if (!stream_output_.get())
      return 0;

    return stream_output_->identifier();
  }

  const obs_output_t* streaming_output() {
    if (!stream_output_.get())
      return nullptr;

    return stream_output_->output_;
  }

  void set_supported_tracks(uint32_t audio_tracks);

  uint32_t supported_tracks() {
    return supported_tracks_;
  }

  void set_fragmented_file(bool enable) {
    set_fragmented_file_ = enable;
  }

  int GetOutputTracks(const char* output_type, bool separate_tracks);

private:
  AdvancedOutput(AdvancedOutputDelegate* delegate);
  bool Initialize(OBSData& video_encoder_settings,
                  OBSData& error_result);
  static bool IsValidVideoEncoder(const char* encoder_id);
  //void ConnectSignals();

  DISALLOW_IMPLICIT_CONSTRUCTORS(AdvancedOutput);

public:
  AdvancedOutputDelegate* delegate_;

private:
  friend class BaseOutput;
  friend class ReplayOutput;
  friend class StreamOutput;
  friend class RecordOutput;

  bool uses_bitrate_;
  OBSOutput file_output_;

  void applay_fragmented_file(obs_data_t* settings);

  bool set_fragmented_file_ = false;

public:
  OBSEncoder recording_video_encoder_;
  std::unique_ptr<RecordOutput> record_output_;
  std::unique_ptr<ReplayOutput> replay_output_;
  std::unique_ptr<StreamOutput> stream_output_;

  OBSEncoder aacTrack[MAX_AUDIO_MIXES];
  std::string aacEncoderID[MAX_AUDIO_MIXES];

  OBSData system_game_info_;

  uint32_t supported_tracks_ = settings::AudioTracksFlags::AudioTrack1;
};

}; // namespace obs_control

#endif // ASCENTOBS_OBS_CONTROL_OBS_CONTROL_H_