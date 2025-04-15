#include <string>
#include <algorithm>
//#include "audio-encoders.hpp"
#include "basic-output.hpp"
#include <util/config-file.h>
#include <util/dstr.h>
#include <util/threading.h>

using namespace std;

volatile bool streaming_active = false;
volatile bool recording_active = false;
volatile bool recording_paused = false;
volatile bool replaybuf_active = false;
volatile bool virtualcam_active = false;

#define FTL_PROTOCOL "ftl"
#define RTMP_PROTOCOL "rtmp"
#define SRT_PROTOCOL "srt"
#define RIST_PROTOCOL "rist"

string GetFormatExt(const char *container)
{
	string ext = container;
	if (ext == "fragmented_mp4")
		ext = "mp4";
	else if (ext == "fragmented_mov")
		ext = "mov";
	else if (ext == "hls")
		ext = "m3u8";
	else if (ext == "mpegts")
		ext = "ts";

	return ext;
}

static void OBSStreamStarting(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	obs_output_t *obj = (obs_output_t *)calldata_ptr(params, "output");

	int sec = (int)obs_output_get_active_delay(obj);
	if (sec == 0)
		return;

	output->delayActive = true;
}

static void OBSStreamStopping(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	obs_output_t *obj = (obs_output_t *)calldata_ptr(params, "output");

	int sec = (int)obs_output_get_active_delay(obj);
	// if (sec == 0)
	// 	QMetaObject::invokeMethod(output->main, "StreamStopping");
	// else
	// 	QMetaObject::invokeMethod(output->main, "StreamDelayStopping",
	// 				  Q_ARG(int, sec));
}

static void OBSStartStreaming(void *data, calldata_t * /* params */)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	output->streamingActive = true;
	os_atomic_set_bool(&streaming_active, true);
	//QMetaObject::invokeMethod(output->main, "StreamingStart");
}

static void OBSStopStreaming(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	int code = (int)calldata_int(params, "code");
	const char *last_error = calldata_string(params, "last_error");

	//QString arg_last_error = QString::fromUtf8(last_error);

	output->streamingActive = false;
	output->delayActive = false;
	os_atomic_set_bool(&streaming_active, false);
	// QMetaObject::invokeMethod(output->main, "StreamingStop",
	// 			  Q_ARG(int, code),
	// 			  Q_ARG(QString, arg_last_error));
}

static void OBSStartRecording(void *data, calldata_t * /* params */)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);

	output->recordingActive = true;
	os_atomic_set_bool(&recording_active, true);
	//QMetaObject::invokeMethod(output->main, "RecordingStart");
}

static void OBSStopRecording(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	int code = (int)calldata_int(params, "code");
	const char *last_error = calldata_string(params, "last_error");

	//QString arg_last_error = QString::fromUtf8(last_error);

	output->recordingActive = false;
	os_atomic_set_bool(&recording_active, false);
	os_atomic_set_bool(&recording_paused, false);
	// QMetaObject::invokeMethod(output->main, "RecordingStop",
	// 			  Q_ARG(int, code),
	// 			  Q_ARG(QString, arg_last_error));
}

static void OBSRecordStopping(void *data, calldata_t * /* params */)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	//QMetaObject::invokeMethod(output->main, "RecordStopping");
}

static void OBSRecordFileChanged(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	const char *next_file = calldata_string(params, "next_file");

	// QString arg_last_file =
	// 	QString::fromUtf8(output->lastRecordingPath.c_str());

	// QMetaObject::invokeMethod(output->main, "RecordingFileChanged",
	// 			  Q_ARG(QString, arg_last_file));

	output->lastRecordingPath = next_file;
}

static void OBSStartReplayBuffer(void *data, calldata_t * /* params */)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);

	output->replayBufferActive = true;
	os_atomic_set_bool(&replaybuf_active, true);
	//QMetaObject::invokeMethod(output->main, "ReplayBufferStart");
}

static void OBSStopReplayBuffer(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	int code = (int)calldata_int(params, "code");

	output->replayBufferActive = false;
	os_atomic_set_bool(&replaybuf_active, false);
	// QMetaObject::invokeMethod(output->main, "ReplayBufferStop",
	// 			  Q_ARG(int, code));
}

static void OBSReplayBufferStopping(void *data, calldata_t * /* params */)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	//QMetaObject::invokeMethod(output->main, "ReplayBufferStopping");
}

static void OBSReplayBufferSaved(void *data, calldata_t * /* params */)
{
	// BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	// QMetaObject::invokeMethod(output->main, "ReplayBufferSaved",
	// 			  Qt::QueuedConnection);
}

static void OBSStartVirtualCam(void *data, calldata_t * /* params */)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);

	output->virtualCamActive = true;
	os_atomic_set_bool(&virtualcam_active, true);
	//QMetaObject::invokeMethod(output->main, "OnVirtualCamStart");
}

static void OBSStopVirtualCam(void *data, calldata_t *params)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	int code = (int)calldata_int(params, "code");

	output->virtualCamActive = false;
	os_atomic_set_bool(&virtualcam_active, false);
	// QMetaObject::invokeMethod(output->main, "OnVirtualCamStop",
	// 			  Q_ARG(int, code));
}

static void OBSDeactivateVirtualCam(void *data, calldata_t * /* params */)
{
	BasicOutputHandler *output = static_cast<BasicOutputHandler *>(data);
	output->DestroyVirtualCamView();
}

/* ------------------------------------------------------------------------ */

static inline bool can_use_output(const char *prot, const char *output,
				  const char *prot_test1,
				  const char *prot_test2 = nullptr)
{
	return (strcmp(prot, prot_test1) == 0 ||
		(prot_test2 && strcmp(prot, prot_test2) == 0)) &&
	       (obs_get_output_flags(output) & OBS_OUTPUT_SERVICE) != 0;
}

static bool return_first_id(void *data, const char *id)
{
	const char **output = (const char **)data;

	*output = id;
	return false;
}

static const char *GetStreamOutputType(const obs_service_t *service)
{
	const char *protocol = obs_service_get_protocol(service);
	const char *output = nullptr;

	if (!protocol) {
		blog(LOG_WARNING, "The service '%s' has no protocol set",
		     obs_service_get_id(service));
		return nullptr;
	}

	if (!obs_is_output_protocol_registered(protocol)) {
		blog(LOG_WARNING, "The protocol '%s' is not registered",
		     protocol);
		return nullptr;
	}

	/* Check if the service has a preferred output type */
	output = obs_service_get_preferred_output_type(service);
	if (output) {
		if ((obs_get_output_flags(output) & OBS_OUTPUT_SERVICE) != 0)
			return output;

		blog(LOG_WARNING,
		     "The output '%s' is not registered, fallback to another one",
		     output);
	}

	/* Otherwise, prefer first-party output types */
	if (can_use_output(protocol, "rtmp_output", "RTMP", "RTMPS")) {
		return "rtmp_output";
	} else if (can_use_output(protocol, "ffmpeg_hls_muxer", "HLS")) {
		return "ffmpeg_hls_muxer";
	} else if (can_use_output(protocol, "ffmpeg_mpegts_muxer", "SRT",
				  "RIST")) {
		return "ffmpeg_mpegts_muxer";
	}

	/* If third-party protocol, use the first enumerated type */
	obs_enum_output_types_with_protocol(protocol, &output, return_first_id);
	if (output)
		return output;

	blog(LOG_WARNING,
	     "No output compatible with the service '%s' is registered",
	     obs_service_get_id(service));

	return nullptr;
}

/* ------------------------------------------------------------------------ */

inline BasicOutputHandler::BasicOutputHandler(config_t *config_t)
	: config_t_(config_t)
{
}

//extern void log_vcam_changed(const VCamConfig &config, bool starting);

bool BasicOutputHandler::StartVirtualCam()
{
	return false;
}

void BasicOutputHandler::StopVirtualCam()
{
	// if (main->vcamEnabled) {
	// 	obs_output_stop(virtualCam);
	// }
}

bool BasicOutputHandler::VirtualCamActive() const
{
	// if (main->vcamEnabled) {
	// 	return obs_output_active(virtualCam);
	// }
	return false;
}

void BasicOutputHandler::UpdateVirtualCamOutputSource()
{
}

void BasicOutputHandler::DestroyVirtualCamView()
{
}

void BasicOutputHandler::DestroyVirtualCameraScene()
{
	if (!vCamSourceScene)
		return;

	obs_scene_release(vCamSourceScene);
	vCamSourceScene = nullptr;
	vCamSourceSceneItem = nullptr;
}

/* ------------------------------------------------------------------------ */

struct AdvancedOutput : BasicOutputHandler {
	OBSEncoder streamAudioEnc;
	OBSEncoder streamArchiveEnc;
	OBSEncoder streamTrack[MAX_AUDIO_MIXES];
	OBSEncoder recordTrack[MAX_AUDIO_MIXES];
	OBSEncoder videoStreaming;
	OBSEncoder videoRecording;

	bool ffmpegOutput;
	bool ffmpegRecording;
	bool useStreamEncoder;
	bool useStreamAudioEncoder;
	bool usesBitrate = false;

	AdvancedOutput(config_t *config_t);

	inline void UpdateStreamSettings();
	inline void UpdateRecordingSettings();
	inline void UpdateAudioSettings();
	virtual void Update() override;

	inline void SetupVodTrack(obs_service_t *service);

	inline void SetupStreaming();
	inline void SetupRecording();
	inline void SetupFFmpeg();
	void SetupOutputs() override;
	int GetAudioBitrate(size_t i, const char *id) const;

	virtual bool SetupStreaming(obs_service_t *service) override;
	virtual bool StartStreaming(obs_service_t *service) override;
	virtual bool StartRecording() override;
	virtual bool StartReplayBuffer() override;
	virtual void StopStreaming(bool force) override;
	virtual void StopRecording(bool force) override;
	virtual void StopReplayBuffer(bool force) override;
	virtual bool StreamingActive() const override;
	virtual bool RecordingActive() const override;
	virtual bool ReplayBufferActive() const override;
	bool allowsMultiTrack();
};

static OBSData GetDataFromJsonFile(const char *jsonFile)
{
	//char fullPath[512];
	//OBSDataAutoRelease data = nullptr;

	//int ret = GetProfilePath(fullPath, sizeof(fullPath), jsonFile);
	//if (ret > 0) {
	//	BPtr<char> jsonData = os_quick_read_utf8_file(fullPath);
	//	if (!!jsonData) {
	//		data = obs_data_create_from_json(jsonData);
	//	}
	//}

	//if (!data)
	//	data = obs_data_create();

	//return data.Get();
	OBSDataAutoRelease data = nullptr;
	return data.Get();
}

static void ApplyEncoderDefaults(OBSData &settings,
				 const obs_encoder_t *encoder)
{
	OBSData dataRet = obs_encoder_get_defaults(encoder);
	obs_data_release(dataRet);

	if (!!settings)
		obs_data_apply(dataRet, settings);
	settings = std::move(dataRet);
}

#define ADV_ARCHIVE_NAME "adv_archive_audio"

#ifdef __APPLE__
static void translate_macvth264_encoder(const char *&encoder)
{
	if (strcmp(encoder, "vt_h264_hw") == 0) {
		encoder = "com.apple.videotoolbox.videoencoder.h264.gva";
	} else if (strcmp(encoder, "vt_h264_sw") == 0) {
		encoder = "com.apple.videotoolbox.videoencoder.h264";
	}
}
#endif

AdvancedOutput::AdvancedOutput(config_t *config_t)
	: BasicOutputHandler(config_t)
{
	const char *recType =
		config_get_string(config_t_, "AdvOut", "RecType");
	const char *streamEncoder =
		config_get_string(config_t_, "AdvOut", "Encoder");
	const char *streamAudioEncoder =
		config_get_string(config_t_, "AdvOut", "AudioEncoder");
	const char *recordEncoder =
		config_get_string(config_t_, "AdvOut", "RecEncoder");
	const char *recAudioEncoder =
		config_get_string(config_t_, "AdvOut", "RecAudioEncoder");
#ifdef __APPLE__
	translate_macvth264_encoder(streamEncoder);
	translate_macvth264_encoder(recordEncoder);
#endif

	ffmpegOutput = astrcmpi(recType, "FFmpeg") == 0;
	ffmpegRecording =
		ffmpegOutput &&
		config_get_bool(config_t_, "AdvOut", "FFOutputToFile");
	useStreamEncoder = false; //astrcmpi(recordEncoder, "none") == 0;
	useStreamAudioEncoder = astrcmpi(recAudioEncoder, "none") == 0;

	OBSData streamEncSettings;// = GetDataFromJsonFile("streamEncoder.json");
	OBSData recordEncSettings;// = GetDataFromJsonFile("recordEncoder.json");

	if (ffmpegOutput) {
		fileOutput = obs_output_create(
			"ffmpeg_output", "adv_ffmpeg_output", nullptr, nullptr);
		if (!fileOutput)
			throw "Failed to create recording FFmpeg output "
			      "(advanced output)";
	} else {
		bool useReplayBuffer =
			config_get_bool(config_t_, "AdvOut", "RecRB");
		if (useReplayBuffer) {
			replayBuffer = obs_output_create("replay_buffer",
							 "ReplayBuffer",
							 nullptr, nullptr);

			if (!replayBuffer)
				throw "Failed to create replay buffer output "
				      "(simple output)";

			signal_handler_t *signal =
				obs_output_get_signal_handler(replayBuffer);

			startReplayBuffer.Connect(signal, "start",
						  OBSStartReplayBuffer, this);
			stopReplayBuffer.Connect(signal, "stop",
						 OBSStopReplayBuffer, this);
			replayBufferStopping.Connect(signal, "stopping",
						     OBSReplayBufferStopping,
						     this);
			replayBufferSaved.Connect(signal, "saved",
						  OBSReplayBufferSaved, this);
		}

		fileOutput = obs_output_create(
			"ffmpeg_muxer", "adv_file_output", nullptr, nullptr);
		if (!fileOutput)
			throw "Failed to create recording output "
			      "(advanced output)";

		if (!useStreamEncoder) {
			videoRecording = obs_video_encoder_create(
				recordEncoder, "advanced_video_recording",
				recordEncSettings, nullptr);
			if (!videoRecording)
				throw "Failed to create recording video "
				      "encoder (advanced output)";
			obs_encoder_release(videoRecording);
		}
	}

	//videoStreaming = obs_video_encoder_create(streamEncoder,
	//					  "advanced_video_stream",
	//					  streamEncSettings, nullptr);
	//if (!videoStreaming)
	//	throw "Failed to create streaming video encoder "
	//	      "(advanced output)";
	obs_encoder_release(videoStreaming);

	const char *rate_control = obs_data_get_string(
		useStreamEncoder ? streamEncSettings : recordEncSettings,
		"rate_control");
	if (!rate_control)
		rate_control = "";
	usesBitrate = astrcmpi(rate_control, "CBR") == 0 ||
		      astrcmpi(rate_control, "VBR") == 0 ||
		      astrcmpi(rate_control, "ABR") == 0;

	for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
		char name[19];
		snprintf(name, sizeof(
			name), "adv_record_audio_%d", i);

		recordTrack[i] = obs_audio_encoder_create(
			useStreamAudioEncoder ? streamAudioEncoder
					      : recAudioEncoder,
			name, nullptr, i, nullptr);

		if (!recordTrack[i]) {
			throw "Failed to create audio encoder "
			      "(advanced output)";
		}

		obs_encoder_release(recordTrack[i]);

		snprintf(name, sizeof(name), "adv_stream_audio_%d", i);
		streamTrack[i] = obs_audio_encoder_create(
			streamAudioEncoder, name, nullptr, i, nullptr);

		if (!streamTrack[i]) {
			throw "Failed to create streaming audio encoders "
			      "(advanced output)";
		}

		obs_encoder_release(streamTrack[i]);
	}

	std::string id;
	auto streamTrackIndex =
		config_get_int(config_t_, "AdvOut", "TrackIndex") - 1;
	streamAudioEnc = obs_audio_encoder_create(streamAudioEncoder,
						  "adv_stream_audio", nullptr,
						  streamTrackIndex, nullptr);
	if (!streamAudioEnc)
		throw "Failed to create streaming audio encoder "
		      "(advanced output)";
	obs_encoder_release(streamAudioEnc);

	id = "";
	auto vodTrack =
		config_get_int(config_t_, "AdvOut", "VodTrackIndex") - 1;
	streamArchiveEnc = obs_audio_encoder_create(streamAudioEncoder,
						    ADV_ARCHIVE_NAME, nullptr,
						    vodTrack, nullptr);
	if (!streamArchiveEnc)
		throw "Failed to create archive audio encoder "
		      "(advanced output)";
	obs_encoder_release(streamArchiveEnc);

	startRecording.Connect(obs_output_get_signal_handler(fileOutput),
			       "start", OBSStartRecording, this);
	stopRecording.Connect(obs_output_get_signal_handler(fileOutput), "stop",
			      OBSStopRecording, this);
	recordStopping.Connect(obs_output_get_signal_handler(fileOutput),
			       "stopping", OBSRecordStopping, this);
	recordFileChanged.Connect(obs_output_get_signal_handler(fileOutput),
				  "file_changed", OBSRecordFileChanged, this);
}

void AdvancedOutput::UpdateStreamSettings()
{
	/*bool applyServiceSettings = config_get_bool(config_t_, "AdvOut",
						    "ApplyServiceSettings");
	bool enforceBitrate = !config_get_bool(config_t_, "Stream1",
					       "IgnoreRecommended");
	bool dynBitrate =
		config_get_bool(config_t_, "Output", "DynamicBitrate");
	const char *streamEncoder =
		config_get_string(config_t_, "AdvOut", "Encoder");

	OBSData settings = GetDataFromJsonFile("streamEncoder.json");
	ApplyEncoderDefaults(settings, videoStreaming);

	if (applyServiceSettings) {
		int bitrate = (int)obs_data_get_int(settings, "bitrate");
		int keyint_sec = (int)obs_data_get_int(settings, "keyint_sec");
		obs_service_apply_encoder_settings(main->GetService(), settings,
						   nullptr);
		if (!enforceBitrate) {
			blog(LOG_INFO,
			     "User is ignoring service bitrate limits.");
			obs_data_set_int(settings, "bitrate", bitrate);
		}

		int enforced_keyint_sec =
			(int)obs_data_get_int(settings, "keyint_sec");
		if (keyint_sec != 0 && keyint_sec < enforced_keyint_sec)
			obs_data_set_int(settings, "keyint_sec", keyint_sec);
	} else {
		blog(LOG_WARNING, "User is ignoring service settings.");
	}

	if (dynBitrate && astrcmpi(streamEncoder, "jim_nvenc") == 0)
		obs_data_set_bool(settings, "lookahead", false);

	video_t *video = obs_get_video();
	enum video_format format = video_output_get_format(video);

	switch (format) {
	case VIDEO_FORMAT_I420:
	case VIDEO_FORMAT_NV12:
	case VIDEO_FORMAT_I010:
	case VIDEO_FORMAT_P010:
		break;
	default:
		obs_encoder_set_preferred_video_format(videoStreaming,
						       VIDEO_FORMAT_NV12);
	}

	obs_encoder_update(videoStreaming, settings);*/
}

inline void AdvancedOutput::UpdateRecordingSettings()
{
	OBSData settings = GetDataFromJsonFile("recordEncoder.json");
	obs_encoder_update(videoRecording, settings);
}

void AdvancedOutput::Update()
{
	UpdateStreamSettings();
	if (!useStreamEncoder && !ffmpegOutput)
		UpdateRecordingSettings();
	UpdateAudioSettings();
}

static inline bool ServiceSupportsVodTrack(const char *service)
{
	static const char *vodTrackServices[] = {"Twitch"};

	for (const char *vodTrackService : vodTrackServices) {
		if (astrcmpi(vodTrackService, service) == 0)
			return true;
	}

	return false;
}

inline bool AdvancedOutput::allowsMultiTrack()
{
	//const char *protocol = nullptr;
	//obs_service_t *service_obj = main->GetService();
	//protocol = obs_service_get_protocol(service_obj);
	//if (!protocol)
	//	return false;
	//return astrcmpi_n(protocol, SRT_PROTOCOL, strlen(SRT_PROTOCOL)) == 0 ||
	//       astrcmpi_n(protocol, RIST_PROTOCOL, strlen(RIST_PROTOCOL)) == 0;
	return true;
}

inline void AdvancedOutput::SetupStreaming()
{
	const char *rescaleRes =
		config_get_string(config_t_, "AdvOut", "RescaleRes");
	auto rescaleFilter =
		config_get_int(config_t_, "AdvOut", "RescaleFilter");
	auto multiTrackAudioMixes = (int)config_get_int(config_t_, "AdvOut",
						  "StreamMultiTrackAudioMixes");
	unsigned int cx = 0;
	unsigned int cy = 0;
	int idx = 0;
	bool is_multitrack_output = allowsMultiTrack();

	if (rescaleFilter != OBS_SCALE_DISABLE && rescaleRes && *rescaleRes) {
		if (sscanf(rescaleRes, "%ux%u", &cx, &cy) != 2) {
			cx = 0;
			cy = 0;
		}
	}

	if (!is_multitrack_output) {
		obs_output_set_audio_encoder(streamOutput, streamAudioEnc, 0);
	} else {
		for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
			if ((multiTrackAudioMixes & (1 << i)) != 0) {
				obs_output_set_audio_encoder(
					streamOutput, streamTrack[i], idx);
				idx++;
			}
		}
	}

	obs_encoder_set_scaled_size(videoStreaming, cx, cy);
	obs_encoder_set_gpu_scale_type(videoStreaming,
				       (obs_scale_type)rescaleFilter);

	/*const char *id = obs_service_get_id(main->GetService());
	if (strcmp(id, "rtmp_custom") == 0) {
		OBSDataAutoRelease settings = obs_data_create();
		obs_service_apply_encoder_settings(main->GetService(), settings,
						   nullptr);
		obs_encoder_update(videoStreaming, settings);
	}*/
}

inline void AdvancedOutput::SetupRecording()
{
	const char *path =
		config_get_string(config_t_, "AdvOut", "RecFilePath");
	const char *mux =
		config_get_string(config_t_, "AdvOut", "RecMuxerCustom");
	const char *rescaleRes =
		config_get_string(config_t_, "AdvOut", "RecRescaleRes");
	int rescaleFilter =
		(int)config_get_int(config_t_, "AdvOut", "RecRescaleFilter");
	int tracks;

	const char *recFormat =
		config_get_string(config_t_, "AdvOut", "RecFormat2");

	bool is_fragmented = strncmp(recFormat, "fragmented", 10) == 0;
	bool flv = strcmp(recFormat, "flv") == 0;

	if (flv)
		tracks = (int)config_get_int(config_t_, "AdvOut", "FLVTrack");
	else
		tracks = (int)config_get_int(config_t_, "AdvOut", "RecTracks");

	OBSDataAutoRelease settings = obs_data_create();
	unsigned int cx = 0;
	unsigned int cy = 0;
	int idx = 0;

	/* Hack to allow recordings without any audio tracks selected. It is no
	 * longer possible to select such a configuration in settings, but legacy
	 * configurations might still have this configured and we don't want to
	 * just break them. */
	if (tracks == 0)
		tracks = (int)config_get_int(config_t_, "AdvOut", "TrackIndex");

	if (useStreamEncoder) {
		obs_output_set_video_encoder(fileOutput, videoStreaming);
		if (replayBuffer)
			obs_output_set_video_encoder(replayBuffer,
						     videoStreaming);
	} else {
		if (rescaleFilter != OBS_SCALE_DISABLE && rescaleRes &&
		    *rescaleRes) {
			if (sscanf(rescaleRes, "%ux%u", &cx, &cy) != 2) {
				cx = 0;
				cy = 0;
			}
		}

		obs_encoder_set_scaled_size(videoRecording, cx, cy);
		obs_encoder_set_gpu_scale_type(videoRecording,
					       (obs_scale_type)rescaleFilter);
		obs_output_set_video_encoder(fileOutput, videoRecording);
		if (replayBuffer)
			obs_output_set_video_encoder(replayBuffer,
						     videoRecording);
	}

	if (!flv) {
		for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
			if ((tracks & (1 << i)) != 0) {
				obs_output_set_audio_encoder(
					fileOutput, recordTrack[i], idx);
				if (replayBuffer)
					obs_output_set_audio_encoder(
						replayBuffer, recordTrack[i],
						idx);
				idx++;
			}
		}
	} else if (flv && tracks != 0) {
		obs_output_set_audio_encoder(fileOutput,
					     recordTrack[tracks - 1], idx);

		if (replayBuffer)
			obs_output_set_audio_encoder(
				replayBuffer, recordTrack[tracks - 1], idx);
	}

	// Use fragmented MOV/MP4 if user has not already specified custom movflags
	if (is_fragmented && (!mux || strstr(mux, "movflags") == NULL)) {
		string mux_frag =
			"movflags=frag_keyframe+empty_moov+delay_moov";
		if (mux) {
			mux_frag += " ";
			mux_frag += mux;
		}
		obs_data_set_string(settings, "muxer_settings",
				    mux_frag.c_str());
	} else {
		if (is_fragmented)
			blog(LOG_WARNING,
			     "User enabled fragmented recording, "
			     "but custom muxer settings contained movflags.");
		obs_data_set_string(settings, "muxer_settings", mux);
	}

	obs_data_set_string(settings, "path", path);
	obs_output_update(fileOutput, settings);
	if (replayBuffer)
		obs_output_update(replayBuffer, settings);
}

inline void AdvancedOutput::SetupFFmpeg()
{
	/*const char *url = config_get_string(config_t_, "AdvOut", "FFURL");
	int vBitrate = config_get_int(config_t_, "AdvOut", "FFVBitrate");
	int gopSize = config_get_int(config_t_, "AdvOut", "FFVGOPSize");
	bool rescale = config_get_bool(config_t_, "AdvOut", "FFRescale");
	const char *rescaleRes =
		config_get_string(config_t_, "AdvOut", "FFRescaleRes");
	const char *formatName =
		config_get_string(config_t_, "AdvOut", "FFFormat");
	const char *mimeType =
		config_get_string(config_t_, "AdvOut", "FFFormatMimeType");
	const char *muxCustom =
		config_get_string(config_t_, "AdvOut", "FFMCustom");
	const char *vEncoder =
		config_get_string(config_t_, "AdvOut", "FFVEncoder");
	int vEncoderId =
		config_get_int(config_t_, "AdvOut", "FFVEncoderId");
	const char *vEncCustom =
		config_get_string(config_t_, "AdvOut", "FFVCustom");
	int aBitrate = config_get_int(config_t_, "AdvOut", "FFABitrate");
	int aMixes = config_get_int(config_t_, "AdvOut", "FFAudioMixes");
	const char *aEncoder =
		config_get_string(config_t_, "AdvOut", "FFAEncoder");
	int aEncoderId =
		config_get_int(config_t_, "AdvOut", "FFAEncoderId");
	const char *aEncCustom =
		config_get_string(config_t_, "AdvOut", "FFACustom");

	OBSDataArrayAutoRelease audio_names = obs_data_array_create();

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		string cfg_name = "Track";
		cfg_name += to_string((int)i + 1);
		cfg_name += "Name";

		const char *audioName = config_get_string(
			config_t_, "AdvOut", cfg_name.c_str());

		OBSDataAutoRelease item = obs_data_create();
		obs_data_set_string(item, "name", audioName);
		obs_data_array_push_back(audio_names, item);
	}

	OBSDataAutoRelease settings = obs_data_create();

	obs_data_set_array(settings, "audio_names", audio_names);
	obs_data_set_string(settings, "url", url);
	obs_data_set_string(settings, "format_name", formatName);
	obs_data_set_string(settings, "format_mime_type", mimeType);
	obs_data_set_string(settings, "muxer_settings", muxCustom);
	obs_data_set_int(settings, "gop_size", gopSize);
	obs_data_set_int(settings, "video_bitrate", vBitrate);
	obs_data_set_string(settings, "video_encoder", vEncoder);
	obs_data_set_int(settings, "video_encoder_id", vEncoderId);
	obs_data_set_string(settings, "video_settings", vEncCustom);
	obs_data_set_int(settings, "audio_bitrate", aBitrate);
	obs_data_set_string(settings, "audio_encoder", aEncoder);
	obs_data_set_int(settings, "audio_encoder_id", aEncoderId);
	obs_data_set_string(settings, "audio_settings", aEncCustom);

	if (rescale && rescaleRes && *rescaleRes) {
		int width;
		int height;
		int val = sscanf(rescaleRes, "%dx%d", &width, &height);

		if (val == 2 && width && height) {
			obs_data_set_int(settings, "scale_width", width);
			obs_data_set_int(settings, "scale_height", height);
		}
	}

	obs_output_set_mixers(fileOutput, aMixes);
	obs_output_set_media(fileOutput, obs_get_video(), obs_get_audio());
	obs_output_update(fileOutput, settings);*/
}

static inline void SetEncoderName(obs_encoder_t *encoder, const char *name,
				  const char *defaultName)
{
	obs_encoder_set_name(encoder, (name && *name) ? name : defaultName);
}

inline void AdvancedOutput::UpdateAudioSettings()
{
	bool applyServiceSettings = config_get_bool(config_t_, "AdvOut",
						    "ApplyServiceSettings");
	bool enforceBitrate = !config_get_bool(config_t_, "Stream1",
					       "IgnoreRecommended");
	auto streamTrackIndex =
		config_get_int(config_t_, "AdvOut", "TrackIndex");
	auto vodTrackIndex =
		config_get_int(config_t_, "AdvOut", "VodTrackIndex");
	const char *audioEncoder =
		config_get_string(config_t_, "AdvOut", "AudioEncoder");
	const char *recAudioEncoder =
		config_get_string(config_t_, "AdvOut", "RecAudioEncoder");

	bool is_multitrack_output = allowsMultiTrack();

	OBSDataAutoRelease settings[MAX_AUDIO_MIXES];

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		string cfg_name = "Track";
		cfg_name += to_string((int)i + 1);
		cfg_name += "Name";
		const char *name = config_get_string(config_t_, "AdvOut",
						     cfg_name.c_str());

		string def_name = "Track";
		def_name += to_string((int)i + 1);
		SetEncoderName(recordTrack[i], name, def_name.c_str());
		SetEncoderName(streamTrack[i], name, def_name.c_str());
	}

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		int track = (int)(i + 1);
		settings[i] = obs_data_create();
		obs_data_set_int(settings[i], "bitrate",
				 GetAudioBitrate(i, recAudioEncoder));

		obs_encoder_update(recordTrack[i], settings[i]);

		obs_data_set_int(settings[i], "bitrate",
				 GetAudioBitrate(i, audioEncoder));

		if (!is_multitrack_output) {
			if (track == streamTrackIndex ||
			    track == vodTrackIndex) {
				if (applyServiceSettings) {
					int bitrate = (int)obs_data_get_int(
						settings[i], "bitrate");
					obs_service_apply_encoder_settings(
						/*main->GetService()*/ nullptr,
						nullptr,
						settings[i]);

					if (!enforceBitrate)
						obs_data_set_int(settings[i],
								 "bitrate",
								 bitrate);
				}
			}

			if (track == streamTrackIndex)
				obs_encoder_update(streamAudioEnc, settings[i]);
			if (track == vodTrackIndex)
				obs_encoder_update(streamArchiveEnc,
						   settings[i]);
		} else {
			obs_encoder_update(streamTrack[i], settings[i]);
		}
	}
}

void AdvancedOutput::SetupOutputs()
{
	obs_encoder_set_video(videoStreaming, obs_get_video());
	if (videoRecording)
		obs_encoder_set_video(videoRecording, obs_get_video());
	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		obs_encoder_set_audio(streamTrack[i], obs_get_audio());
		obs_encoder_set_audio(recordTrack[i], obs_get_audio());
	}
	obs_encoder_set_audio(streamAudioEnc, obs_get_audio());
	obs_encoder_set_audio(streamArchiveEnc, obs_get_audio());

	SetupStreaming();

	if (ffmpegOutput)
		SetupFFmpeg();
	else
		SetupRecording();
}

int AdvancedOutput::GetAudioBitrate(size_t i, const char *id) const
{
	static const char *names[] = {
		"Track1Bitrate", "Track2Bitrate", "Track3Bitrate",
		"Track4Bitrate", "Track5Bitrate", "Track6Bitrate",
	};
	int bitrate = (int)config_get_uint(config_t_, "AdvOut", names[i]);
	return 196;// FindClosestAvailableAudioBitrate(id, bitrate);
}

inline void AdvancedOutput::SetupVodTrack(obs_service_t *service)
{
	// int streamTrackIndex =
	// 	config_get_int(config_t_, "AdvOut", "TrackIndex");
	// bool vodTrackEnabled =
	// 	config_get_bool(config_t_, "AdvOut", "VodTrackEnabled");
	// int vodTrackIndex =
	// 	config_get_int(config_t_, "AdvOut", "VodTrackIndex");
	// bool enableForCustomServer = config_get_bool(
	// 	GetGlobalConfig(), "General", "EnableCustomServerVodTrack");

	// const char *id = obs_service_get_id(service);
	// if (strcmp(id, "rtmp_custom") == 0) {
	// 	vodTrackEnabled = enableForCustomServer ? vodTrackEnabled
	// 						: false;
	// } else {
	// 	OBSDataAutoRelease settings = obs_service_get_settings(service);
	// 	const char *service = obs_data_get_string(settings, "service");
	// 	if (!ServiceSupportsVodTrack(service))
	// 		vodTrackEnabled = false;
	// }
	// if (vodTrackEnabled && streamTrackIndex != vodTrackIndex)
	// 	obs_output_set_audio_encoder(streamOutput, streamArchiveEnc, 1);
	// else
	// 	clear_archive_encoder(streamOutput, ADV_ARCHIVE_NAME);
}

bool AdvancedOutput::SetupStreaming(obs_service_t *service)
{
	return false;
	
}

bool AdvancedOutput::StartStreaming(obs_service_t *service)
{
	return false;
}

bool AdvancedOutput::StartRecording()
{
	const char *path;
	const char *recFormat;
	const char *filenameFormat;
	bool noSpace = false;
	bool overwriteIfExists = false;
	bool splitFile;
	const char *splitFileType;
	int64_t splitFileTime;
	int64_t splitFileSize;

	if (!useStreamEncoder) {
		if (!ffmpegOutput) {
			UpdateRecordingSettings();
		}
	} else if (!obs_output_active(streamOutput)) {
		UpdateStreamSettings();
	}

	UpdateAudioSettings();

	if (!Active())
		SetupOutputs();

	if (!ffmpegOutput || ffmpegRecording) {
		path = config_get_string(config_t_, "AdvOut",
					 ffmpegRecording ? "FFFilePath"
							 : "RecFilePath");
		recFormat = config_get_string(config_t_, "AdvOut",
					      ffmpegRecording ? "FFExtension"
							      : "RecFormat2");
		filenameFormat = config_get_string(config_t_, "Output",
						   "FilenameFormatting");
		overwriteIfExists = config_get_bool(config_t_, "Output",
						    "OverwriteIfExists");
		noSpace = config_get_bool(config_t_, "AdvOut",
					  ffmpegRecording
						  ? "FFFileNameWithoutSpace"
						  : "RecFileNameWithoutSpace");
		splitFile = config_get_bool(config_t_, "AdvOut",
					    "RecSplitFile");

		string strPath = GetRecordingFilename(path, recFormat, noSpace,
						      overwriteIfExists,
						      filenameFormat,
						      ffmpegRecording);

		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, ffmpegRecording ? "url" : "path",
				    strPath.c_str());

		if (splitFile) {
			splitFileType = config_get_string(
				config_t_, "AdvOut", "RecSplitFileType");
			splitFileTime =
				(astrcmpi(splitFileType, "Time") == 0)
					? config_get_int(config_t_,
							 "AdvOut",
							 "RecSplitFileTime")
					: 0;
			splitFileSize =
				(astrcmpi(splitFileType, "Size") == 0)
					? config_get_int(config_t_,
							 "AdvOut",
							 "RecSplitFileSize")
					: 0;
			string ext = GetFormatExt(recFormat);
			obs_data_set_string(settings, "directory", path);
			obs_data_set_string(settings, "format", filenameFormat);
			obs_data_set_string(settings, "extension", ext.c_str());
			obs_data_set_bool(settings, "allow_spaces", !noSpace);
			obs_data_set_bool(settings, "allow_overwrite",
					  overwriteIfExists);
			obs_data_set_bool(settings, "split_file", true);
			obs_data_set_int(settings, "max_time_sec",
					 splitFileTime * 60);
			obs_data_set_int(settings, "max_size_mb",
					 splitFileSize);
		}

		obs_output_update(fileOutput, settings);
	}

	if (!obs_output_start(fileOutput)) {
		// QString error_reason;
		// const char *error = obs_output_get_last_error(fileOutput);
		// if (error)
		// 	error_reason = QT_UTF8(error);
		// else
		// 	error_reason = QTStr("Output.StartFailedGeneric");
		// QMessageBox::critical(main,
		// 		      QTStr("Output.StartRecordingFailed"),
		// 		      error_reason);
		return false;
	}

	return true;
}

bool AdvancedOutput::StartReplayBuffer()
{
	//const char *path;
	//const char *recFormat;
	//const char *filenameFormat;
	//bool noSpace = false;
	//bool overwriteIfExists = false;
	//const char *rbPrefix;
	//const char *rbSuffix;
	//int rbTime;
	//int rbSize;

	//if (!useStreamEncoder) {
	//	if (!ffmpegOutput)
	//		UpdateRecordingSettings();
	//} else if (!obs_output_active(streamOutput)) {
	//	UpdateStreamSettings();
	//}

	//UpdateAudioSettings();

	//if (!Active())
	//	SetupOutputs();

	//if (!ffmpegOutput || ffmpegRecording) {
	//	path = config_get_string(config_t_, "AdvOut",
	//				 ffmpegRecording ? "FFFilePath"
	//						 : "RecFilePath");
	//	recFormat = config_get_string(config_t_, "AdvOut",
	//				      ffmpegRecording ? "FFExtension"
	//						      : "RecFormat2");
	//	filenameFormat = config_get_string(config_t_, "Output",
	//					   "FilenameFormatting");
	//	overwriteIfExists = config_get_bool(config_t_, "Output",
	//					    "OverwriteIfExists");
	//	noSpace = config_get_bool(config_t_, "AdvOut",
	//				  ffmpegRecording
	//					  ? "FFFileNameWithoutSpace"
	//					  : "RecFileNameWithoutSpace");
	//	rbPrefix = config_get_string(config_t_, "SimpleOutput",
	//				     "RecRBPrefix");
	//	rbSuffix = config_get_string(config_t_, "SimpleOutput",
	//				     "RecRBSuffix");
	//	rbTime = config_get_int(config_t_, "AdvOut", "RecRBTime");
	//	rbSize = config_get_int(config_t_, "AdvOut", "RecRBSize");

	//	string f = GetFormatString(filenameFormat, rbPrefix, rbSuffix);
	//	string ext = GetFormatExt(recFormat);

	//	OBSDataAutoRelease settings = obs_data_create();

	//	obs_data_set_string(settings, "directory", path);
	//	obs_data_set_string(settings, "format", f.c_str());
	//	obs_data_set_string(settings, "extension", ext.c_str());
	//	obs_data_set_bool(settings, "allow_spaces", !noSpace);
	//	obs_data_set_int(settings, "max_time_sec", rbTime);
	//	obs_data_set_int(settings, "max_size_mb",
	//			 usesBitrate ? 0 : rbSize);

	//	obs_output_update(replayBuffer, settings);
	//}

	//if (!obs_output_start(replayBuffer)) {
	//	const char *error = obs_output_get_last_error(replayBuffer);
	//	// if (error)
	//	// 	error_reason = QT_UTF8(error);
	//	// else
	//	// 	error_reason = QTStr("Output.StartFailedGeneric");
	//	// QMessageBox::critical(main, QTStr("Output.StartReplayFailed"),
	//	// 		      error_reason);
	//	return false;
	//}

	//return true;
	return false;
}

void AdvancedOutput::StopStreaming(bool force)
{
	if (force)
		obs_output_force_stop(streamOutput);
	else
		obs_output_stop(streamOutput);
}

void AdvancedOutput::StopRecording(bool force)
{
	if (force)
		obs_output_force_stop(fileOutput);
	else
		obs_output_stop(fileOutput);
}

void AdvancedOutput::StopReplayBuffer(bool force)
{
	if (force)
		obs_output_force_stop(replayBuffer);
	else
		obs_output_stop(replayBuffer);
}

bool AdvancedOutput::StreamingActive() const
{
	return obs_output_active(streamOutput);
}

bool AdvancedOutput::RecordingActive() const
{
	return obs_output_active(fileOutput);
}

bool AdvancedOutput::ReplayBufferActive() const
{
	return obs_output_active(replayBuffer);
}


std::string BasicOutputHandler::GetRecordingFilename(
	const char *path, const char *container, bool noSpace, bool overwrite,
	const char *format, bool ffmpeg)
{
	//if (!ffmpeg)
	//	SetupAutoRemux(container);

	string dst = "";
		//GetOutputFilename(path, container, noSpace, overwrite, format);
	lastRecordingPath = dst;
	return dst;
}
