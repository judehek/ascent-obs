#pragma once
#include <string>
#include <obs.h>
#include <obs.hpp>
using namespace std;

struct SimpleOutput  {
public:
  OBSSignal              startRecording;
  OBSSignal              stopRecording;
  OBSSignal              startReplayBuffer;
  OBSSignal              stopReplayBuffer;
  OBSSignal              startStreaming;
  OBSSignal              stopStreaming;
  OBSSignal              streamDelayStarting;
  OBSSignal              streamStopping;
  OBSSignal              recordStopping;
  OBSSignal              replayBufferStopping; 
  OBSSignal              replayReady;
  OBSSignal              replayError;
  OBSSignal              replayArmed;

  OBSSignal              VideoSpillted;


  OBSOutput              fileOutput;
  OBSOutput              streamOutput;
  OBSOutput              replayBuffer;
  bool                   streamingActive = false;
  bool                   recordingActive = false;
  bool                   delayActive = false;
  bool                   replayBufferActive = false;
  OBSEncoder             aacStreaming;
  OBSEncoder             h264Streaming;
  OBSEncoder             aacRecording;
  OBSEncoder             h264Recording;

  string                 aacRecEncID;
  string                 aacStreamEncID;

  string                 videoEncoder;
  string                 videoQuality;
  bool                   usingRecordingPreset = false;
  bool                   recordingConfigured = false;
  bool                   ffmpegOutput = false;
  bool                   lowCPUx264 = false;

  SimpleOutput(const char *encoder);
  virtual ~SimpleOutput();

  int CalcCRF(int crf);

  void UpdateStreamingSettings_amd(obs_data_t *settings, int bitrate);
  void UpdateRecordingSettings_x264_crf(int crf);
  void UpdateRecordingSettings_qsv11(int crf);
  void UpdateRecordingSettings_nvenc(int cqp);
  void UpdateRecordingSettings_amd_cqp(int cqp);
  void UpdateRecordingSettings();
  void UpdateRecordingAudioSettings();
  virtual void Update();

  void SetupOutputs();
  int GetAudioBitrate() const;

  void LoadRecordingPreset_h264(const char *encoder);
  void LoadRecordingPreset_Lossless();
  void LoadRecordingPreset();

  void LoadStreamingPreset_h264(const char *encoder);

  void UpdateRecording(bool has_audio = true);
  bool ConfigureRecording(bool useReplayBuffer);

  virtual bool StartStreaming(obs_service_t *service);
  virtual bool StartRecording(bool has_audio = true);
  virtual bool StartReplayBuffer();
  virtual void StopStreaming(bool force);
  virtual void StopRecording(bool force);
  virtual void StopReplayBuffer(bool force);
  virtual bool StreamingActive() const ;
  virtual bool RecordingActive() const ;
  virtual bool ReplayBufferActive() const ;

  OBSService GetService();

  inline bool Active() const {
    return streamingActive || recordingActive || delayActive ||
      replayBufferActive;
  }

  OBSService service;
  bool _didStopped;
  std::string _encoder;
};

