/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/
#include "ffmpeg-mux/ffmpeg-mux.h"
#include "obs-ffmpeg-mux.h"
#include "obs-ffmpeg-formats.h"

#ifdef _WIN32
#include "util/windows/win-version.h"
#include "util/windows/win-registry.h"
#endif

#include <libavformat/avformat.h>

#define do_log(level, format, ...)                  \
	blog(level, "[ffmpeg muxer: '%s'] " format, \
	     obs_output_get_name(stream->output), ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define error(format, ...) do_log(LOG_ERROR, format, ##__VA_ARGS__)

#define MINIMUM_FREE_DISKSPACE_LIMIT L"SOFTWARE\\OverwolfQA\\OBS"
#define MAX_REPLAY_TIMEOUT_SECONDS 60
// arbitrarily decided on 50 MB
static int64_t kMinFreeDiskspaceBytes = 52428800;
static int64_t kFreeDiskSpaceBytesWarning = 1024 * 1024 * 200;
const int64_t k100MBInBytes = 1024 * 1024 * 100;

static inline bool
ffmpeg_mux_start_internal_full_video(struct ffmpeg_muxer *stream,
				     obs_data_t *settings);

static void signal_failure2(struct ffmpeg_muxer *stream, int code,
			    bool read_pipe_error);
static void report_video_split(struct ffmpeg_muxer *stream, const char *path,
			       const char *next_file_path);

static char *get_next_split_file_name(const char *path, int index)
{
	if (!path)
		return "";

	struct dstr split_path;
	dstr_init_copy(&split_path, "");

	if (index != 0) {
		size_t ext_index = last_indexof(path, '.');
		dstr_ncopy(&split_path, path, ext_index - 1);
		dstr_cat_ch(&split_path, '_');

		char buffer[10];
		itoa(index, buffer, 10);
		dstr_cat(&split_path, buffer);

		struct dstr file_path_extension = {0};
		dstr_ncopy(&file_path_extension, path + ext_index - 1,
			   strlen(path) - ext_index + 1);
		dstr_cat(&split_path, file_path_extension.array);
	} else {
		dstr_copy(&split_path, path);
	}

	return split_path.array;
}

static inline void set_free_disk_space(struct ffmpeg_muxer *stream)
{
	obs_data_t *settings;
	const char *path;
	settings = obs_output_get_settings(stream->output);
	path = obs_data_get_string(settings, "path");

	if (path == NULL || strlen(path) == 0) {
		info("set_free_disk_space: get path from stream");
		path = stream->path.array;
	}

	char *drive = path ? get_drive_from_path(path) : NULL;
	info("set_free_disk_space(Drive: |%s| path: %s)",
	     drive ? drive : "ERROR", path ? path : "");

	obs_data_release(settings);
	if (drive && strlen(drive) != 0) {

		int64_t free_space = os_get_free_space(drive);

		int64_t free_disk_space_delta_mb =
			(free_space - stream->free_disk_space) / 1024 / 1024;

		info("|%s| free disk space: %d MB", drive,
		     (free_space / 1024 / 1024));

		bfree(drive);
		stream->free_disk_space = free_space;
		stream->origin_free_disk_space = free_space;

	} else {
		if (stream->free_disk_space != 0) {
			warn("reset free disk space");
			stream->free_disk_space = 0;
		}
		stream->origin_free_disk_space = 0;
	}
}

static bool check_free_disk_space(struct ffmpeg_muxer *stream,
				  size_t written_bytes)
{
	if (stream->free_disk_space == 0) {
		// init the free disk space on the first check, if needed...
		set_free_disk_space(stream);
		return true;
	}

	stream->free_disk_space -= written_bytes;

	if (!stream->disk_space_warning_sent &&
	    stream->free_disk_space < kFreeDiskSpaceBytesWarning) {
		warn("low_disk_space_warning: %d", stream->free_disk_space);
		obs_output_signal_disk_space_warning(stream->output,
						     stream->path.array,
						     "low_disk_space_warning");
		stream->disk_space_warning_sent = true;
	}

	int64_t allowed_disk_space =
		stream->free_disk_space - kMinFreeDiskspaceBytes;

	if (allowed_disk_space <= 0) {
		signal_failure2(stream, OBS_OUTPUT_NO_SPACE, false);
		return false;
	}

	int64_t total_write =
		stream->origin_free_disk_space - stream->free_disk_space;

	// sync disk space size every 100 MB
	if (total_write > k100MBInBytes) {
		set_free_disk_space(stream);
	}

	return true;
}


static const char *ffmpeg_mux_getname(void *type)
{
	UNUSED_PARAMETER(type);
	return obs_module_text("FFmpegMuxer");
}

#ifndef NEW_MPEGTS_OUTPUT
static const char *ffmpeg_mpegts_mux_getname(void *type)
{
	UNUSED_PARAMETER(type);
	return obs_module_text("FFmpegMpegtsMuxer");
}
#endif

static inline void replay_buffer_clear(struct ffmpeg_muxer *stream)
{
	while (stream->packets.size > 0) {
		struct encoder_packet pkt;
		deque_pop_front(&stream->packets, &pkt, sizeof(pkt));
		obs_encoder_packet_release(&pkt);
	}

	deque_free(&stream->packets);

	while (stream->replay_pending_packets.size > 0) {
		struct encoder_packet pkt;
		deque_pop_front(&stream->replay_pending_packets, &pkt,
				    sizeof(pkt));
		obs_encoder_packet_release(&pkt);
	}
	deque_free(&stream->replay_pending_packets);

	stream->cur_size = 0;
	stream->cur_time = 0;
	stream->max_size = 0;
	stream->max_time = 0;
	stream->save_ts = 0;
	stream->keyframes = 0;
	stream->save_start_pts_usec = 0;
	os_atomic_set_bool(&stream->capturing_replay, false);
}

static void ffmpeg_mux_destroy(void *data)
{
	struct ffmpeg_muxer *stream = data;

	replay_buffer_clear(stream);
	if (stream->mux_thread_joinable)
		pthread_join(stream->mux_thread, NULL);
	for (size_t i = 0; i < stream->mux_packets.num; i++)
		obs_encoder_packet_release(&stream->mux_packets.array[i]);
	da_free(stream->mux_packets);
	deque_free(&stream->packets);

	os_process_pipe_destroy(stream->pipe);
	dstr_free(&stream->path);
	dstr_free(&stream->printable_path);
	dstr_free(&stream->stream_key);
	dstr_free(&stream->muxer_settings);
	bfree(stream);
}

static void split_file_proc(void *data, calldata_t *cd)
{
	struct ffmpeg_muxer *stream = data;
	calldata_set_bool(cd, "split_file_enabled", stream->split_file);
	if (!stream->split_file) {
		calldata_set_bool(cd, "success", false);
		calldata_set_string(cd, "error", "on demand split is disabled");
		return;
	}

	if (stream->manual_split ||
	    stream->on_demand_split_pts != 0) {
		calldata_set_bool(cd, "success", false);
		calldata_set_string(cd, "error",
				    "on demand split already in progress");
		return;
	}

	bool split_by_pts = false;
	calldata_get_bool(cd, "split_by_pts", &split_by_pts);
	split_by_pts = true;

	stream->on_demand_split_pts =
		obs_output_get_video_encoder_pts(stream->output);
	stream->on_demand_split_timestamp_ms_epoch = os_get_epoch_time_unix();

	info("split video command started [timestamp: (%" PRIu64
	     ") epoch:(%" PRIu64 ") ]",
	     stream->on_demand_split_pts,
	     stream->on_demand_split_timestamp_ms_epoch);

	if (!split_by_pts) {
		os_atomic_set_bool(&stream->manual_split, true);
	}
	calldata_set_bool(cd, "success", true);
}

static void *ffmpeg_mux_create(obs_data_t *settings, obs_output_t *output)
{
	struct ffmpeg_muxer *stream = bzalloc(sizeof(*stream));
	stream->output = output;

	if (obs_output_get_flags(output) & OBS_OUTPUT_SERVICE)
		stream->is_network = true;

	signal_handler_t *sh = obs_output_get_signal_handler(output);
	signal_handler_add(sh, "void file_changed(string next_file)");

	proc_handler_t *ph = obs_output_get_proc_handler(output);
	proc_handler_add(ph, "void split_file(out bool split_file_enabled)",
			 split_file_proc, stream);

	stream->split_index = 0;
	UNUSED_PARAMETER(settings);
	return stream;
}

#ifdef _WIN32
#define FFMPEG_MUX "ascentobs-ffmpeg-mux.exe"
#else
#define FFMPEG_MUX "ascentobs-ffmpeg-mux"
#endif

static inline bool capturing(struct ffmpeg_muxer *stream)
{
	return os_atomic_load_bool(&stream->capturing);
}

bool stopping(struct ffmpeg_muxer *stream)
{
	return os_atomic_load_bool(&stream->stopping);
}

bool active(struct ffmpeg_muxer *stream)
{
	return os_atomic_load_bool(&stream->active);
}

bool capturing_replay(struct ffmpeg_muxer *stream) {
	return os_atomic_load_bool(&stream->capturing_replay);
}

bool stopping_replay(struct ffmpeg_muxer *stream)
{
	return os_atomic_load_bool(&stream->replay_stopping);
}

static void add_video_encoder_params(struct ffmpeg_muxer *stream,
				     struct dstr *cmd, obs_encoder_t *vencoder)
{
	obs_data_t *settings = obs_encoder_get_settings(vencoder);
	int bitrate = (int)obs_data_get_int(settings, "bitrate");
	video_t *video = obs_get_video();
	const struct video_output_info *info = video_output_get_info(video);

	int codec_tag = (int)obs_data_get_int(settings, "codec_type");
#if __BYTE_ORDER == __LITTLE_ENDIAN
	codec_tag = ((codec_tag >> 24) & 0x000000FF) |
		    ((codec_tag << 8) & 0x00FF0000) |
		    ((codec_tag >> 8) & 0x0000FF00) |
		    ((codec_tag << 24) & 0xFF000000);
#endif

	obs_data_release(settings);

	enum AVColorPrimaries pri = AVCOL_PRI_UNSPECIFIED;
	enum AVColorTransferCharacteristic trc = AVCOL_TRC_UNSPECIFIED;
	enum AVColorSpace spc = AVCOL_SPC_UNSPECIFIED;
	switch (info->colorspace) {
	case VIDEO_CS_601:
		pri = AVCOL_PRI_SMPTE170M;
		trc = AVCOL_TRC_SMPTE170M;
		spc = AVCOL_SPC_SMPTE170M;
		break;
	case VIDEO_CS_DEFAULT:
	case VIDEO_CS_709:
		pri = AVCOL_PRI_BT709;
		trc = AVCOL_TRC_BT709;
		spc = AVCOL_SPC_BT709;
		break;
	case VIDEO_CS_SRGB:
		pri = AVCOL_PRI_BT709;
		trc = AVCOL_TRC_IEC61966_2_1;
		spc = AVCOL_SPC_BT709;
		break;
	case VIDEO_CS_2100_PQ:
		pri = AVCOL_PRI_BT2020;
		trc = AVCOL_TRC_SMPTE2084;
		spc = AVCOL_SPC_BT2020_NCL;
		break;
	case VIDEO_CS_2100_HLG:
		pri = AVCOL_PRI_BT2020;
		trc = AVCOL_TRC_ARIB_STD_B67;
		spc = AVCOL_SPC_BT2020_NCL;
	}

	const enum AVColorRange range = (info->range == VIDEO_RANGE_FULL)
						? AVCOL_RANGE_JPEG
						: AVCOL_RANGE_MPEG;

	const int max_luminance =
		(trc == AVCOL_TRC_SMPTE2084)
			? (int)obs_get_video_hdr_nominal_peak_level()
			: ((trc == AVCOL_TRC_ARIB_STD_B67) ? 1000 : 0);

	dstr_catf(cmd, "%s %d %d %d %d %d %d %d %d %d %d %d %d ",
		  obs_encoder_get_codec(vencoder), bitrate,
		  obs_output_get_width(stream->output),
		  obs_output_get_height(stream->output), (int)pri, (int)trc,
		  (int)spc, (int)range,
		  (int)determine_chroma_location(
			  obs_to_ffmpeg_video_format(info->format), spc),
		  max_luminance, (int)info->fps_num, (int)info->fps_den,
		  (int)codec_tag);
}

static void add_audio_encoder_params(struct dstr *cmd, obs_encoder_t *aencoder)
{
	obs_data_t *settings = obs_encoder_get_settings(aencoder);
	int bitrate = (int)obs_data_get_int(settings, "bitrate");
	audio_t *audio = obs_get_audio();
	struct dstr name = {0};

	obs_data_release(settings);

	dstr_copy(&name, obs_encoder_get_name(aencoder));
	dstr_replace(&name, "\"", "\"\"");

	dstr_catf(cmd, "\"%s\" %d %d %d %d ", name.array, bitrate,
		  (int)obs_encoder_get_sample_rate(aencoder),
		  (int)obs_encoder_get_frame_size(aencoder),
		  (int)audio_output_get_channels(audio));

	dstr_free(&name);
}

static void log_muxer_params(struct ffmpeg_muxer *stream, const char *settings)
{
	int ret;

	AVDictionary *dict = NULL;
	if ((ret = av_dict_parse_string(&dict, settings, "=", " ", 0))) {
		warn("Failed to parse muxer settings: %s\n%s", av_err2str(ret),
		     settings);

		av_dict_free(&dict);
		return;
	}

	if (av_dict_count(dict) > 0) {
		struct dstr str = {0};

		AVDictionaryEntry *entry = NULL;
		while ((entry = av_dict_get(dict, "", entry,
					    AV_DICT_IGNORE_SUFFIX)))
			dstr_catf(&str, "\n\t%s=%s", entry->key, entry->value);

		info("Using muxer settings:%s", str.array);
		dstr_free(&str);
	}

	av_dict_free(&dict);
}

static void add_stream_key(struct dstr *cmd, struct ffmpeg_muxer *stream)
{
	dstr_catf(cmd, "\"%s\" ",
		  dstr_is_empty(&stream->stream_key)
			  ? ""
			  : stream->stream_key.array);
}

static void add_muxer_params(struct dstr *cmd, struct ffmpeg_muxer *stream)
{
	struct dstr mux = {0};

	if (dstr_is_empty(&stream->muxer_settings)) {
		obs_data_t *settings = obs_output_get_settings(stream->output);
		dstr_copy(&mux,
			  obs_data_get_string(settings, "muxer_settings"));
		obs_data_release(settings);
	} else {
		dstr_copy(&mux, stream->muxer_settings.array);
	}

	log_muxer_params(stream, mux.array);

	dstr_replace(&mux, "\"", "\\\"");

	dstr_catf(cmd, "\"%s\" ", mux.array ? mux.array : "");

	dstr_free(&mux);
}

static void build_command_line(struct ffmpeg_muxer *stream, struct dstr *cmd,
			       const char *path)
{
	obs_encoder_t *vencoder = obs_output_get_video_encoder(stream->output);
	obs_encoder_t *aencoders[MAX_AUDIO_MIXES];
	int num_tracks = 0;

	for (;;) {
		obs_encoder_t *aencoder = obs_output_get_audio_encoder(
			stream->output, num_tracks);
		if (!aencoder)
			break;

		aencoders[num_tracks] = aencoder;
		num_tracks++;
	}

	dstr_init_move_array(cmd, os_get_executable_path_ptr(FFMPEG_MUX));
	dstr_insert_ch(cmd, 0, '\"');
	dstr_cat(cmd, "\" \"");

	dstr_copy(&stream->path, path);
	dstr_replace(&stream->path, "\"", "\"\"");
	dstr_cat_dstr(cmd, &stream->path);

	dstr_catf(cmd, "\" %d %d ", vencoder ? 1 : 0, num_tracks);

	if (vencoder)
		add_video_encoder_params(stream, cmd, vencoder);

	if (num_tracks) {
		const char *codec = obs_encoder_get_codec(aencoders[0]);
		dstr_cat(cmd, codec);
		dstr_cat(cmd, " ");

		for (int i = 0; i < num_tracks; i++) {
			add_audio_encoder_params(cmd, aencoders[i]);
		}
	}

	add_stream_key(cmd, stream);
	add_muxer_params(cmd, stream);
}

void start_pipe(struct ffmpeg_muxer *stream, const char *path)
{
	struct dstr cmd;
	build_command_line(stream, &cmd, path);
	stream->pipe = os_process_pipe_create(cmd.array, "w");
	dstr_free(&cmd);
}

static void set_file_not_readable_error(struct ffmpeg_muxer *stream,
					obs_data_t *settings, const char *path)
{
	struct dstr error_message;
	dstr_init_copy(&error_message, obs_module_text("UnableToWritePath"));
#ifdef _WIN32
	/* special warning for Windows 10 users about Defender */
	struct win_version_info ver;
	get_win_ver(&ver);
	if (ver.major >= 10) {
		dstr_cat(&error_message, "\n\n");
		dstr_cat(&error_message,
			 obs_module_text("WarnWindowsDefender"));
	}
#endif
	dstr_replace(&error_message, "%1", path);
	obs_output_set_last_error(stream->output, error_message.array);
	dstr_free(&error_message);
	obs_data_release(settings);
}

inline static void ts_offset_clear(struct ffmpeg_muxer *stream)
{
	stream->found_video = false;
	stream->video_pts_offset = 0;

	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		stream->found_audio[i] = false;
		stream->audio_dts_offsets[i] = 0;
	}

	stream->on_demand_split_pts = 0;
	stream->on_demand_split_timestamp_ms_epoch = 0;
}

static inline int64_t packet_pts_usec(struct encoder_packet *packet)
{
	return packet->pts * 1000000 / packet->timebase_den;
}

static inline int64_t pts_usec(int64_t pts, struct encoder_packet *packet)
{
	return pts * 1000000 / packet->timebase_den;
}

inline static void ts_offset_update(struct ffmpeg_muxer *stream,
				    struct encoder_packet *packet)
{
	if (packet->type == OBS_ENCODER_VIDEO) {
		if (!stream->found_video) {
			stream->video_pts_offset = packet->pts;
			stream->found_video = true;
		}
		return;
	}

	if (stream->found_audio[packet->track_idx])
		return;

	stream->audio_dts_offsets[packet->track_idx] = packet->dts;
	stream->found_audio[packet->track_idx] = true;
}

static inline void update_encoder_settings(struct ffmpeg_muxer *stream,
					   const char *path)
{
	obs_encoder_t *vencoder = obs_output_get_video_encoder(stream->output);
	const char *ext = strrchr(path, '.');

	/* if using m3u8, repeat headers */
	if (ext && strcmp(ext, ".m3u8") == 0) {
		obs_data_t *settings = obs_encoder_get_settings(vencoder);
		obs_data_set_bool(settings, "repeat_headers", true);
		obs_encoder_update(vencoder, settings);
		obs_data_release(settings);
	}
}

static inline bool ffmpeg_mux_start_internal(struct ffmpeg_muxer *stream,
					     obs_data_t *settings,
					     bool full_video)
{
	const char *path = obs_data_get_string(settings, "path");

	bool include_full_video =
		!full_video && // we ares not initialized full video
		obs_data_get_bool(settings, "include_full_video");

	if (include_full_video) {
		stream->split_index = 1;
		path = get_next_split_file_name(path, stream->split_index);
	}

	update_encoder_settings(stream, path);

	if (!obs_output_can_begin_data_capture(stream->output, 0))
		return false;
	if (!obs_output_initialize_encoders(stream->output, 0))
		return false;

	if (stream->is_network) {
		obs_service_t *service;
		service = obs_output_get_service(stream->output);
		if (!service)
			return false;
		path = obs_service_get_connect_info(
			service, OBS_SERVICE_CONNECT_INFO_SERVER_URL);
		stream->split_file = false;
	} else {

		stream->max_time =
			obs_data_get_int(settings, "max_time_sec") * 1000000LL;
		stream->max_size = obs_data_get_int(settings, "max_size_mb") *
				   (1024 * 1024);
		stream->split_file = obs_data_get_bool(settings, "split_file");
		stream->allow_overwrite =
			obs_data_get_bool(settings, "allow_overwrite");
		stream->cur_size = 0;
		stream->sent_headers = false;
	}

	if (full_video) {
		stream->max_size = 0;
		stream->max_time = 0;
		stream->split_file = false;
	}

	ts_offset_clear(stream);

	if (!stream->is_network) {
		/* ensure output path is writable to avoid generic error
		 * message.
		 *
		 * TODO: remove once ffmpeg-mux is refactored to pass
		 * errors back */
		FILE *test_file = os_fopen(path, "wb");

		if (!test_file) {
			set_file_not_readable_error(stream, settings, path);
			return false;
		}

		fclose(test_file);
		os_unlink(path);
	}

	stream->stream_start_time = os_get_epoch_time_unix();
	start_pipe(stream, path);

	if (!stream->pipe) {
		obs_output_set_last_error(
			stream->output, obs_module_text("HelperProcessFailed"));
		warn("Failed to create process pipe");
		return false;
	}

	if (include_full_video &&
	    !ffmpeg_mux_start_internal_full_video(stream, settings)) {
		warn("Failed to start full video process");
		return false;
	}

	/* write headers and start capture */
	os_atomic_set_bool(&stream->active, true);
	os_atomic_set_bool(&stream->capturing, true);
	os_atomic_set_bool(&stream->stopping, false);
	stream->total_bytes = 0;
	obs_output_begin_data_capture(stream->output, 0);

	info("Writing file '%s'...", stream->path.array);
	return true;
}

static bool ffmpeg_mux_start(void *data)
{
	struct ffmpeg_muxer *stream = data;

	obs_data_t *settings = obs_output_get_settings(stream->output);
	bool success = ffmpeg_mux_start_internal(stream, settings, false);

#ifdef _WIN32
	struct reg_dword min_free_fiskspace_mb;
	// IN MB!
	get_reg_dword(HKEY_CURRENT_USER, MINIMUM_FREE_DISKSPACE_LIMIT,
		      L"MinFreeDiskspaceMB", &min_free_fiskspace_mb);

	if (min_free_fiskspace_mb.return_value > 0) {
		warn("Override minimum free disk space limit: %d KB (%d MB) => %d KB (%d MB)",
		     kMinFreeDiskspaceBytes, 50,
		     min_free_fiskspace_mb.return_value * 1024 * 1024,
		     min_free_fiskspace_mb.return_value);
		kMinFreeDiskspaceBytes =
			min_free_fiskspace_mb.return_value * 1024 * 1024;
	}
#endif

	obs_data_release(settings);
	return success;
}

static inline bool
ffmpeg_mux_start_internal_full_video(struct ffmpeg_muxer *stream,
				     obs_data_t *settings)
{
	struct ffmpeg_muxer *stream_full = bzalloc(sizeof(*stream_full));
	stream->full_video = stream_full;
	stream_full->output = stream->output;
	return ffmpeg_mux_start_internal(stream_full, settings, true);
}

void deactivate_full_video(void *data)
{
	if (!data) {
		return;
	}


	struct ffmpeg_muxer *stream = data;
	info("Stopping Output of file (full) '%s'",
	     dstr_is_empty(&stream->printable_path)
		     ? stream->path.array
		     : stream->printable_path.array);

	bool ret = os_process_pipe_destroy(stream->pipe);
	stream->pipe = NULL;
	info("Output of file (full) stopped");

}

int deactivate(struct ffmpeg_muxer *stream, int code)
{
	int ret = -1;

	if (stream->is_hls) {
		if (stream->mux_thread_joinable) {
			os_event_signal(stream->stop_event);
			os_sem_post(stream->write_sem);
			pthread_join(stream->mux_thread, NULL);
			stream->mux_thread_joinable = false;
		}
	}

	if (active(stream)) {
		ret = os_process_pipe_destroy(stream->pipe);
		stream->pipe = NULL;

		os_atomic_set_bool(&stream->active, false);
		os_atomic_set_bool(&stream->sent_headers, false);

		info("Output of file '%s' stopped",
		     dstr_is_empty(&stream->printable_path)
			     ? stream->path.array
			     : stream->printable_path.array);

		if (stream->full_video) {
			deactivate_full_video(stream->full_video);
			report_video_split(stream, stream->path.array, "");
		}
		
	}

	if (code) {
		obs_output_signal_stop(stream->output, code);
	} else if (stopping(stream)) {
		obs_output_end_data_capture1(stream->output, stream->duration);
	}

	if (stream->is_hls) {
		pthread_mutex_lock(&stream->write_mutex);

		while (stream->packets.size) {
			struct encoder_packet packet;
			deque_pop_front(&stream->packets, &packet,
					sizeof(packet));
			obs_encoder_packet_release(&packet);
		}

		pthread_mutex_unlock(&stream->write_mutex);
	}

	os_atomic_set_bool(&stream->stopping, false);
	os_atomic_set_bool(&stream->replay_stopping, false);
	return ret;
}

void ffmpeg_mux_stop(void *data, uint64_t ts)
{
	struct ffmpeg_muxer *stream = data;

	if (capturing(stream) || ts == 0) {
		stream->stop_ts = (int64_t)ts / 1000LL;
		os_atomic_set_bool(&stream->stopping, true);
		os_atomic_set_bool(&stream->capturing, false);
	}
}

static void signal_failure2(struct ffmpeg_muxer *stream, int code,
			    bool read_pipe_error)
{
	char error[1024];
	int ret;	
	size_t len = 0;

	if (read_pipe_error) {
		len = os_process_pipe_read_err(stream->pipe, (uint8_t *)error,
					       sizeof(error) - 1);
	}

	if (len > 0) {
		error[len] = 0;
		warn("ffmpeg-mux: %s", error);
		obs_output_set_last_error(stream->output, error);
	}

	ret = deactivate(stream, 0);

	switch (ret) {
	case FFM_UNSUPPORTED:
		code = OBS_OUTPUT_UNSUPPORTED;
		break;
	default:
		if (stream->is_network) {
			code = OBS_OUTPUT_DISCONNECTED;
		} else {
			// code = OBS_OUTPUT_ENCODE_ERROR;
		}
	}

	if (code == OBS_OUTPUT_NO_SPACE) {
		obs_output_set_last_error(stream->output, "Out_Of_Disk_Space");
	}

	error("ffmpeg-mux: signal error (code: %d desc: %s): %s", code,
	      len > 0 ? error : "", stream->path.array);

	if (code != 0 && code != OBS_OUTPUT_NO_SPACE) {
            auto res = os_unlink(stream->path.array);
            info("ffmpeg-mux: delete file %s %d ", stream->path.array, res);
	}

	obs_output_signal_stop(stream->output, code);
	os_atomic_set_bool(&stream->capturing, false);
}

static void signal_failure(struct ffmpeg_muxer *stream)
{
	signal_failure2(stream, OBS_OUTPUT_ENCODE_ERROR, true);
}

static void find_best_filename(struct dstr *path, bool space)
{
	int num = 2;

	if (!os_file_exists(path->array))
		return;

	const char *ext = strrchr(path->array, '.');
	if (!ext)
		return;

	size_t extstart = ext - path->array;
	struct dstr testpath;
	dstr_init_copy_dstr(&testpath, path);
	for (;;) {
		dstr_resize(&testpath, extstart);
		dstr_catf(&testpath, space ? " (%d)" : "_%d", num++);
		dstr_cat(&testpath, ext);

		if (!os_file_exists(testpath.array)) {
			dstr_free(path);
			dstr_init_move(path, &testpath);
			break;
		}
	}
}

static void generate_filename(struct ffmpeg_muxer *stream, struct dstr *dst,
			      bool overwrite, bool use_path_name)
{
	obs_data_t *settings = obs_output_get_settings(stream->output);
	const char *dir = obs_data_get_string(settings, "directory");
	const char *fmt = obs_data_get_string(settings, "format");
	const char *ext = obs_data_get_string(settings, "extension");
	bool space = obs_data_get_bool(settings, "allow_spaces");
	const char *path = obs_data_get_string(settings, "path");
	char *filename =
		!use_path_name
			? os_generate_formatted_filename(ext, space, fmt)
			: get_next_split_file_name(path, stream->split_index);

	if (!use_path_name) {
		dstr_copy(dst, dir);
		dstr_replace(dst, "\\", "/");
		if (dstr_end(dst) != '/')
			dstr_cat_ch(dst, '/');
		dstr_cat(dst, filename);
	} else {
		dstr_copy(dst, filename);
	}

	char *slash = strrchr(dst->array, '/');
	if (slash) {
		*slash = 0;
		os_mkdirs(dst->array);
		*slash = '/';
	}

	if (!overwrite)
		find_best_filename(dst, space);

	bfree(filename);
	obs_data_release(settings);
}

bool write_packet(struct ffmpeg_muxer *stream, struct encoder_packet *packet)
{
	bool is_video = packet->type == OBS_ENCODER_VIDEO;
	size_t ret;

	struct ffm_packet_info info = {.pts = packet->pts,
				       .dts = packet->dts,
				       .size = (uint32_t)packet->size,
				       .index = (int)packet->track_idx,
				       .type = is_video ? FFM_PACKET_VIDEO
							: FFM_PACKET_AUDIO,
				       .keyframe = packet->keyframe};

	if (stream->split_file) {
		if (is_video) {
			info.dts -= stream->video_pts_offset;
			info.pts -= stream->video_pts_offset;
		} else {
			info.dts -= stream->audio_dts_offsets[info.index];
			info.pts -= stream->audio_dts_offsets[info.index];
		}
	}

	ret = os_process_pipe_write(stream->pipe, (const uint8_t *)&info,
				    sizeof(info));
	if (ret != sizeof(info)) {
		warn("os_process_pipe_write for info structure failed");
		signal_failure(stream);
		return false;
	}

	ret = os_process_pipe_write(stream->pipe, packet->data, packet->size);
	if (ret != packet->size) {
		warn("os_process_pipe_write for packet data failed");
		signal_failure(stream);
		return false;
	}

	stream->total_bytes += packet->size;

	stream->duration = packet->dts_usec - stream->video_offset;

	if (stream->split_file)
		stream->cur_size += packet->size;

	if (packet->type == OBS_ENCODER_VIDEO) {
		stream->last_dts_usec = packet->dts_usec;
	}

	if (!check_free_disk_space(stream, packet->size)) {
		return false;
	}

	return true;
}

static bool send_audio_headers(struct ffmpeg_muxer *stream,
			       obs_encoder_t *aencoder, size_t idx)
{
	struct encoder_packet packet = {.type = OBS_ENCODER_AUDIO,
					.timebase_den = 1,
					.track_idx = idx};

	if (!obs_encoder_get_extra_data(aencoder, &packet.data, &packet.size))
		return false;
	return write_packet(stream, &packet);
}

static bool send_video_headers(struct ffmpeg_muxer *stream)
{
	obs_encoder_t *vencoder = obs_output_get_video_encoder(stream->output);

	struct encoder_packet packet = {.type = OBS_ENCODER_VIDEO,
					.timebase_den = 1};

	if (!obs_encoder_get_extra_data(vencoder, &packet.data, &packet.size))
		return false;
	return write_packet(stream, &packet);
}

bool send_headers(struct ffmpeg_muxer *stream)
{
	obs_encoder_t *aencoder;
	size_t idx = 0;

	if (!send_video_headers(stream))
		return false;

	do {
		aencoder = obs_output_get_audio_encoder(stream->output, idx);
		if (aencoder) {
			if (!send_audio_headers(stream, aencoder, idx)) {
				return false;
			}
			idx++;
		}
	} while (aencoder);

	return true;
}

static inline bool should_split(struct ffmpeg_muxer *stream,
				struct encoder_packet *packet)
{
	/* split at video frame */
	if (packet->type != OBS_ENCODER_VIDEO)
		return false;

	/* don't split group of pictures */
	if (!packet->keyframe)
		return false;

	if (os_atomic_load_bool(&stream->manual_split))
		return true;

	/* by pts time*/
	if (stream->on_demand_split_pts != 0 &&
	    packet->sys_pts_usec >= stream->on_demand_split_pts) {
		return true;
	}

	/* reached maximum file size */
	if (stream->max_size > 0 &&
	    stream->cur_size + (int64_t)packet->size >= stream->max_size)
		return true;

	/* reached maximum duration */
	if (stream->max_time > 0 &&
	    packet->dts_usec - stream->cur_time >= stream->max_time)
		return true;

	return false;
}

static bool send_new_filename(struct ffmpeg_muxer *stream, const char *filename)
{
	size_t ret;
	uint32_t size = (uint32_t)strlen(filename);
	struct ffm_packet_info info = {.type = FFM_PACKET_CHANGE_FILE,
				       .size = size};

	ret = os_process_pipe_write(stream->pipe, (const uint8_t *)&info,
				    sizeof(info));
	if (ret != sizeof(info)) {
		warn("os_process_pipe_write for info structure failed");
		signal_failure(stream);
		return false;
	}

	ret = os_process_pipe_write(stream->pipe, (const uint8_t *)filename,
				    size);
	if (ret != size) {
		warn("os_process_pipe_write for packet data failed");
		signal_failure(stream);
		return false;
	}

	return true;
}

static void report_video_split(struct ffmpeg_muxer* stream,
	const char *path, const char *next_file_path) {
	int64_t split_video_duration_ms =
		(stream->last_dts_usec - stream->cur_time) / 1000LL; // to MS

	int64_t end_video_pts_epoch_ms =
		stream->on_demand_split_timestamp_ms_epoch;
	if (stream->on_demand_split_pts != 0) {
	   int64_t delta =
		   (stream->last_video_pts - stream->on_demand_split_pts) / 1000LL;

	  if (delta > 1000 || delta < -1000) {
		  info("split stop offset %d", delta);
	  }

	  end_video_pts_epoch_ms += (delta);
	}

	obs_output_signal_video_split(stream->output,
		                      split_video_duration_ms,
				      stream->duration / 1000,
				      end_video_pts_epoch_ms, path,
				      next_file_path);
}

static bool prepare_split_file(struct ffmpeg_muxer *stream,
			       struct encoder_packet *packet)
{
	stream->split_index++;
	struct dstr file_path = {0};
	dstr_copy(&file_path, stream->path.array);

	generate_filename(stream, &stream->path, stream->allow_overwrite, true);
	info("Changing output file to '%s'", stream->path.array);

	if (!send_new_filename(stream, stream->path.array)) {
		stream->split_index--;
		warn("Failed to send new file name");
		return false;
	}

	calldata_t cd = {0};
	signal_handler_t *sh = obs_output_get_signal_handler(stream->output);
	calldata_set_string(&cd, "next_file", stream->path.array);
	signal_handler_signal(sh, "file_changed", &cd);
	calldata_free(&cd);

	/*
	  report split info
	*/
	report_video_split(stream, file_path.array, stream->path.array);

	if (!send_headers(stream))
		return false;

	stream->cur_size = 0;
	stream->cur_time = packet->dts_usec;

	ts_offset_clear(stream);

	return true;
}

static inline bool has_audio(struct ffmpeg_muxer *stream)
{
	return !!obs_output_get_audio_encoder(stream->output, 0);
}

static void push_back_packet(mux_packets_t *packets,
			     struct encoder_packet *packet)
{
	struct encoder_packet pkt;
	obs_encoder_packet_ref(&pkt, packet);
	da_push_back(*packets, &pkt);
}

static void ffmpeg_mux_data(void *data, struct encoder_packet *packet)
{
	struct ffmpeg_muxer *stream = data;

	if (!active(stream))
		return;

	/* encoder failure */
	if (!packet) {
		warn("stop split, enocder failure");
		deactivate(stream, OBS_OUTPUT_ENCODE_ERROR);
		return;
	}

	if (stream->full_video) {
		ffmpeg_mux_data(stream->full_video, packet);
	}

	if (stream->split_file && stream->mux_packets.num) {
		int64_t pts_usec = packet_pts_usec(packet);
		struct encoder_packet *first_pkt = stream->mux_packets.array;
		int64_t first_pts_usec = packet_pts_usec(first_pkt);

		if (pts_usec >= first_pts_usec) {
			if (packet->type != OBS_ENCODER_AUDIO) {
				push_back_packet(&stream->mux_packets, packet);
				return;
			}

			if (!prepare_split_file(stream, first_pkt))
				return;
			stream->split_file_ready = true;
		}
	} else if (stream->split_file && should_split(stream, packet)) {
		if (has_audio(stream)) {
			push_back_packet(&stream->mux_packets, packet);
			return;
		} else {
			if (!prepare_split_file(stream, packet))
				return;
			stream->split_file_ready = true;
		}
	}

	if (!stream->sent_headers) {
		if (!send_headers(stream))
			return;

		stream->sent_headers = true;

		if (stream->split_file)
			stream->cur_time = packet->dts_usec;
	}

	if (stopping(stream)) {
		if (packet->sys_dts_usec >= stream->stop_ts) {
			deactivate(stream, 0);
			return;
		}
	}

	if (stream->split_file && stream->split_file_ready) {
		for (size_t i = 0; i < stream->mux_packets.num; i++) {
			struct encoder_packet *pkt =
				&stream->mux_packets.array[i];
			ts_offset_update(stream, pkt);
			write_packet(stream, pkt);
			obs_encoder_packet_release(pkt);
		}
		da_free(stream->mux_packets);
		stream->split_file_ready = false;
		os_atomic_set_bool(&stream->manual_split, false);
	}

	// save last pusth pts video frame
	if (packet->type == OBS_ENCODER_VIDEO) {
		stream->last_video_pts = packet->sys_pts_usec;
	}	

	if (stream->split_file)
		ts_offset_update(stream, packet);

	write_packet(stream, packet);
}

static obs_properties_t *ffmpeg_mux_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_text(props, "path", obs_module_text("FilePath"),
				OBS_TEXT_DEFAULT);
	return props;
}

uint64_t ffmpeg_mux_total_bytes(void *data)
{
	struct ffmpeg_muxer *stream = data;
	if (stream->full_video) {
		struct ffmpeg_muxer *full_stream = stream->full_video;
		return stream->total_bytes + full_stream->total_bytes;
	}
	return stream->total_bytes;
}

struct obs_output_info ffmpeg_muxer = {
	.id = "ffmpeg_muxer",
	.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_MULTI_TRACK |
		 OBS_OUTPUT_CAN_PAUSE,
	.get_name = ffmpeg_mux_getname,
	.create = ffmpeg_mux_create,
	.destroy = ffmpeg_mux_destroy,
	.start = ffmpeg_mux_start,
	.stop = ffmpeg_mux_stop,
	.encoded_packet = ffmpeg_mux_data,
	.get_total_bytes = ffmpeg_mux_total_bytes,
	.get_properties = ffmpeg_mux_properties,
};

static int connect_time(struct ffmpeg_muxer *stream)
{
	UNUSED_PARAMETER(stream);
	/* TODO */
	return 0;
}

#ifndef NEW_MPEGTS_OUTPUT
static int ffmpeg_mpegts_mux_connect_time(void *data)
{
	struct ffmpeg_muxer *stream = data;
	/* TODO */
	return connect_time(stream);
}

struct obs_output_info ffmpeg_mpegts_muxer = {
	.id = "ffmpeg_mpegts_muxer",
	.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_MULTI_TRACK |
		 OBS_OUTPUT_SERVICE,
	.protocols = "SRT;RIST",
	.encoded_video_codecs = "h264",
	.encoded_audio_codecs = "aac;opus",
	.get_name = ffmpeg_mpegts_mux_getname,
	.create = ffmpeg_mux_create,
	.destroy = ffmpeg_mux_destroy,
	.start = ffmpeg_mux_start,
	.stop = ffmpeg_mux_stop,
	.encoded_packet = ffmpeg_mux_data,
	.get_total_bytes = ffmpeg_mux_total_bytes,
	.get_properties = ffmpeg_mux_properties,
	.get_connect_time_ms = ffmpeg_mpegts_mux_connect_time,
};
#endif
/* ------------------------------------------------------------------------ */

static const char *replay_buffer_getname(void *type)
{
	UNUSED_PARAMETER(type);
	return obs_module_text("ReplayBuffer");
}

static void replay_buffer_hotkey2(void *data, obs_hotkey_id id,
				  obs_hotkey_t *hotkey, bool pressed,
				  calldata_t *cd)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (!pressed)
		return;

	struct ffmpeg_muxer *stream = data;

	if (os_atomic_load_bool(&stream->capturing_replay)) {
		return;
	}

	if (os_atomic_load_bool(&stream->active)) {
		obs_encoder_t *vencoder =
			obs_output_get_video_encoder(stream->output);
		if (obs_encoder_paused(vencoder)) {
			info("Could not save buffer because encoders paused");
			return;
		}

		int64_t start_time = 0;
		if (cd) {
			calldata_get_int(cd, "start_time", &start_time);
		}
		stream->save_start_pts_usec = start_time;
		stream->save_ts = 1;
		//os_gettime_ns() / 1000LL;
	}
}

static void save_replay_proc(void *data, calldata_t *cd)
{
	replay_buffer_hotkey2(data, 0, NULL, true, cd);
}

static void replay_buffer_hotkey(void *data, obs_hotkey_id id,
				 obs_hotkey_t *hotkey, bool pressed)
{
	replay_buffer_hotkey2(data, id, hotkey, pressed, 0);
}

static void get_last_replay(void *data, calldata_t *cd)
{
	struct ffmpeg_muxer *stream = data;
	if (!os_atomic_load_bool(&stream->muxing))
		calldata_set_string(cd, "path", stream->path.array);
}

static void replay_buffer_stop_capture(void *data, calldata_t *cd)
{
	struct ffmpeg_muxer *stream = data;

	if (!os_atomic_load_bool(&stream->active)) {
		calldata_set_bool(cd, "success", false);
		calldata_set_string(cd, "error", "not active");
		warn("stop replay buffer error: not active");
		return;
	}

	if (stream->save_start_pts_usec == 0 || !capturing_replay(stream)) {
		calldata_set_bool(cd, "success", false);
		calldata_set_string(cd, "error", "not capturing");
		return;
	}

	bool force = false;
	calldata_get_bool(cd, "force", &force);

	
        uint64_t now_pts =
		obs_output_get_video_encoder_pts(stream->output);

	//start_time = encoder_last_pts /* / 1000LL*/ -
	//	     (start_time * 100000LL) /*ms

	stream->save_ts = force ? 1 : now_pts /*os_gettime_ns() / 1000LL*/;

	info("stop replay buffer [timestamp: (%" PRIu64
	     ")]. start-time: (%" PRIu64 ") last_video_ts: (%" PRIu64
	     ") force:%d",
	     (stream->save_ts), stream->save_ts, stream->last_dts_usec, force);

	calldata_set_bool(cd, "success", true);
}

static void replay_buffer_start_capture_command(void *data, calldata_t *cd)
{
	struct ffmpeg_muxer *stream = data;
	int64_t start_time = 0;
	if (!cd) {
		warn("start replay buffer error. not reply data!");
		return;
	}

	if (!os_atomic_load_bool(&stream->active)) {
		calldata_set_bool(cd, "success", false);
		calldata_set_string(cd, "error", "not active");
		warn("start replay buffer error: not active");
		return;
	}

	if (os_atomic_load_bool(&stream->capturing_replay) ||
	    stream->save_start_pts_usec != 0) {
		calldata_set_bool(cd, "success", false);
		calldata_set_string(cd, "error", "already capturing");
		warn("start replay buffer error: already capturing");
		return;
	}

	calldata_get_int(cd, "start_time", &start_time);

        uint64_t encoder_last_pts =
		 obs_output_get_video_encoder_pts(stream->output);

	start_time = encoder_last_pts - (start_time * 1000LL) /*ms to nano*/;

	const char *replay_path = NULL;
	if (calldata_get_string(cd, "file_path", &replay_path) && replay_path) {
		dstr_copy(&stream->replay_path, replay_path);
	}

	stream->save_start_pts_usec = start_time;
	calldata_set_bool(cd, "success", true);
	info("start replay buffer [timestamp: (%" PRIu64
	     ")]. start-time: (%" PRIu64 ") file: %s",
	     encoder_last_pts, stream->save_start_pts_usec,
	     replay_path ? replay_path : "");

	set_free_disk_space(stream);
}

void stop_capture_replay(struct ffmpeg_muxer *stream)
{
	info("stopping replay");
	
	if (capturing_replay(stream)) {
	  os_atomic_set_bool(&stream->replay_stopping, true);
	}		
	
	stream->save_ts = 0;
	stream->save_start_pts_usec = 0;
}

static bool force_stop_capture_replay(struct ffmpeg_muxer *stream,
				      struct encoder_packet *packet)
{
	bool bTimeoutForceStop = false;
	
	if (!capturing_replay(stream) || stream->save_ts == 0) {
		return false;
	}

	int64_t current = obs_output_get_video_encoder_pts(stream->output);
	int64_t delta = (current - stream->save_ts);
	int64_t seconds = delta / 1000000LL;

	// Check if we are waiting too much to the end of replay recording
	if (seconds > MAX_REPLAY_TIMEOUT_SECONDS) {
		error("Timeout->Stop(save_start_ts: %" PRIu64
		      " - sys_pts_usec:%" PRIu64 " - save_ts: %" PRIu64 ")",
		      stream->save_start_pts_usec, packet->sys_pts_usec,
		      stream->save_ts);

		obs_output_signal_replay_warning(
			stream->output, stream->path.array, "replay_time_out");

		bTimeoutForceStop = true;
	}

	return bTimeoutForceStop;
}

static void *replay_buffer_create(obs_data_t *settings, obs_output_t *output)
{
	UNUSED_PARAMETER(settings);
	struct ffmpeg_muxer *stream = bzalloc(sizeof(*stream));
	stream->output = output;
	stream->hotkey =
		obs_hotkey_register_output(output, "ReplayBuffer.Save",
					   obs_module_text("ReplayBuffer.Save"),
					   replay_buffer_hotkey, stream);

	proc_handler_t *ph = obs_output_get_proc_handler(output);
	proc_handler_add(ph, "void save()", save_replay_proc, stream);
	proc_handler_add(ph, "void get_last_replay(out string path)",
			 get_last_replay, stream);

	proc_handler_add(ph, "void start_capute_replay()",
			 replay_buffer_start_capture_command, stream);

	proc_handler_add(ph, "void stop_capute_replay()",
			 replay_buffer_stop_capture, stream);

	signal_handler_t *sh = obs_output_get_signal_handler(output);
	signal_handler_add(sh, "void saved()");

	pthread_mutex_init_value(&stream->replay_packets_mutex);
	return stream;
}

static void replay_buffer_destroy(void *data)
{
	struct ffmpeg_muxer *stream = data;
	if (stream->hotkey)
		obs_hotkey_unregister(stream->hotkey);
	ffmpeg_mux_destroy(data);
}

static bool replay_buffer_start(void *data)
{
	struct ffmpeg_muxer *stream = data;

	if (!obs_output_can_begin_data_capture(stream->output, 0))
		return false;
	if (!obs_output_initialize_encoders(stream->output, 0))
		return false;

	obs_data_t *s = obs_output_get_settings(stream->output);
	stream->max_time = obs_data_get_int(s, "max_time_sec") * 1000000LL;
	stream->max_size = obs_data_get_int(s, "max_size_mb") * (1024 * 1024);
	obs_data_release(s);

	os_atomic_set_bool(&stream->active, true);
	os_atomic_set_bool(&stream->capturing, true);

	stream->total_bytes = 0;
	obs_output_begin_data_capture(stream->output, 0);

	stream->save_start_pts_usec = 0;
	stream->fully_armed = false;
	stream->stream_start_time = 0;
	stream->disk_space_warning_sent = false;
	os_atomic_set_bool(&stream->capturing_replay, false);

        deque_reserve(&stream->replay_pending_packets,
	              sizeof(struct encoder_packet) * 120);

	info("starting replay buffer");

	return true;
}

static bool purge_front(struct ffmpeg_muxer *stream)
{
	struct encoder_packet pkt;
	bool keyframe;

	if (!stream->packets.size)
		return false;

	deque_pop_front(&stream->packets, &pkt, sizeof(pkt));

	keyframe = pkt.type == OBS_ENCODER_VIDEO && pkt.keyframe;

	if (keyframe)
		stream->keyframes--;

	if (!stream->packets.size) {
		stream->cur_size = 0;
		stream->cur_time = 0;
	} else {
		struct encoder_packet first;
		deque_peek_front(&stream->packets, &first, sizeof(first));
		stream->cur_time = first.dts_usec;
		stream->cur_size -= (int64_t)pkt.size;
	}

	obs_encoder_packet_release(&pkt);
	return keyframe;
}

static inline void purge(struct ffmpeg_muxer *stream)
{
	if (purge_front(stream)) {
		if (!stream->fully_armed) {
			// signal fully armed
			info("replay bugger fully armed");
			obs_output_signal_replay_fully_armed(stream->output);
			stream->fully_armed = true;
		}

		struct encoder_packet pkt;

		for (;;) {
			if (!stream->packets.size)
				return;
			deque_peek_front(&stream->packets, &pkt, sizeof(pkt));
			if (pkt.type == OBS_ENCODER_VIDEO && pkt.keyframe)
				return;

			purge_front(stream);
		}
	}
}

static inline void replay_buffer_purge(struct ffmpeg_muxer *stream,
				       struct encoder_packet *pkt)
{
	if (stream->max_size) {
		if (!stream->packets.size || stream->keyframes <= 2)
			return;

		while ((stream->cur_size + (int64_t)pkt->size) >
		       stream->max_size)
			purge(stream);
	}

	if (!stream->packets.size || stream->keyframes <= 2)
		return;

	while ((pkt->dts_usec - stream->cur_time) > stream->max_time)
		purge(stream);
}

static void insert_packet(mux_packets_t *packets, struct encoder_packet *packet,
			  int64_t video_offset, int64_t *audio_offsets,
			  int64_t video_pts_offset, int64_t *audio_dts_offsets)
{
	struct encoder_packet pkt;
	size_t idx;

	obs_encoder_packet_ref(&pkt, packet);

	if (pkt.type == OBS_ENCODER_VIDEO) {
		pkt.dts_usec -= video_offset;
		pkt.dts -= video_pts_offset;
		pkt.pts -= video_pts_offset;
	} else {
		pkt.dts_usec -= audio_offsets[pkt.track_idx];
		pkt.dts -= audio_dts_offsets[pkt.track_idx];
		pkt.pts -= audio_dts_offsets[pkt.track_idx];
	}

	for (idx = packets->num; idx > 0; idx--) {
		struct encoder_packet *p = packets->array + (idx - 1);
		if (p->dts_usec < pkt.dts_usec)
			break;
	}

	da_insert(*packets, idx, &pkt);
}

static void replay_buffer_stop_mux_thread(void *data)
{
	struct ffmpeg_muxer *stream = data;
	info("closing replay file %s", stream->path.array);

	// TODO(eb): i add this to make sure all frames are
	// handled by the ffmpeg-mux process (is some case the last fames of the replay
	// are choppy)
	os_sleep_ms(30);

	os_process_pipe_destroy(stream->pipe);
	stream->pipe = NULL;

	os_atomic_set_bool(&stream->muxing, false);

	obs_output_signal_replay_ready(stream->output, stream->duration / 1000,
				       stream->replay_system_start_time,
				       stream->path.array);

	os_atomic_set_bool(&stream->capturing_replay, false);

	stream->duration = 0;
	stream->save_ts = 0;
	stream->save_start_pts_usec = 0;

	stream->video_offset = 0;
	stream->video_dts_offset = 0;
	stream->replay_system_start_time = 0;
}

static void *replay_buffer_mux_thread(void *data)
{
	struct ffmpeg_muxer *stream = data;
	bool error = false;

	bool video_created = false;
	start_pipe(stream, stream->path.array);

	if (!stream->pipe) {
		warn("Failed to create process pipe");
		error = true;
		goto error;
	}

	if (!send_headers(stream)) {
		warn("Could not write headers for file '%s'",
		     stream->path.array);
		error = true;
		goto error;
	}

	for (size_t i = 0; i < stream->mux_packets.num; i++) {
		struct encoder_packet *pkt = &stream->mux_packets.array[i];
		if (!write_packet(stream, pkt)) {
			warn("Could not write packet for file '%s'",
			     stream->path.array);
			error = true;
			goto error;
		}
		obs_encoder_packet_release(pkt);
	}
	da_free(stream->mux_packets);
	stream->split_file = true;

	info("Wrote replay buffer to '%s'", stream->path.array);
	if (capturing_replay(stream)) {
		// write to replay file until stopped
		while (!stopping(stream) && !stopping_replay(stream)) {
			// empty pending packets
			while (stream->replay_pending_packets.size > 0) {
				struct encoder_packet pkt;
				pthread_mutex_lock(
					&stream->replay_packets_mutex);

				deque_pop_front(
					&stream->replay_pending_packets, &pkt,
					sizeof(pkt));

				pthread_mutex_unlock(
					&stream->replay_packets_mutex);
				write_packet(stream, &pkt);

				obs_encoder_packet_release(&pkt);
			}
			os_sleep_ms(12); // 60 fps
		}

		// write all packets..
		while (stream->replay_pending_packets.size > 0) {
			struct encoder_packet pkt;
			deque_pop_front(&stream->replay_pending_packets,
					    &pkt,
					    sizeof(pkt));
			write_packet(stream, &pkt);

			obs_encoder_packet_release(&pkt);
		}

		// close video (replay_stopping == tire)
		replay_buffer_stop_mux_thread((void *)stream);

	} else { // not on going replay
		video_created = true;
	}
	stream->split_file = false;
error:
	os_process_pipe_destroy(stream->pipe);
	stream->pipe = NULL;
	
	for (size_t i = 0; i < stream->mux_packets.num; i++)
		obs_encoder_packet_release(&stream->mux_packets.array[i]);
	
	da_free(stream->mux_packets);

	while (stream->replay_pending_packets.size > 0) {
		struct encoder_packet pkt;
		deque_pop_front(&stream->replay_pending_packets, &pkt,
				    sizeof(pkt));
		obs_encoder_packet_release(&pkt);
	}

	if (video_created) {
		obs_output_signal_replay_ready(stream->output, stream->duration,
					       stream->replay_system_start_time,
					       stream->path.array);
	}

	os_atomic_set_bool(&stream->muxing, false);
	os_atomic_set_bool(&stream->capturing_replay, false);
	os_atomic_set_bool(&stream->replay_stopping, false);

	if (!error) {
		calldata_t cd = {0};
		signal_handler_t *sh =
			obs_output_get_signal_handler(stream->output);
		signal_handler_signal(sh, "saved", &cd);
	}

	return NULL;
}

static void replay_buffer_save(struct ffmpeg_muxer *stream)
{
	const size_t size = sizeof(struct encoder_packet);
	size_t num_packets = stream->packets.size / size;

	da_reserve(stream->mux_packets, num_packets);

	/* ---------------------------- */
	/* reorder packets */

	bool found_video = false;
	bool found_audio[MAX_AUDIO_MIXES] = {0};
	int64_t video_offset = 0;
	int64_t video_pts_offset = 0;
	int64_t audio_offsets[MAX_AUDIO_MIXES] = {0};
	int64_t audio_dts_offsets[MAX_AUDIO_MIXES] = {0};
	bool prefix_first_key_frame = false;
	size_t last_key_frame_index = 0;
	int64_t video_duration = 0;
	for (size_t i = 0; i < num_packets; i++) {
		struct encoder_packet *pkt;
		pkt = deque_data(&stream->packets, i * size);
		if (stream->save_start_pts_usec != 0) {
			if (!prefix_first_key_frame &&
			    pkt->sys_pts_usec < stream->save_start_pts_usec) {

				if (pkt->keyframe) {
					last_key_frame_index = i;
				}
				continue;
			}

			// make sure out first frame is key frame
			if (!found_video) {
				if (!pkt->keyframe &&
				    (i != (num_packets - 1))) {
					i = last_key_frame_index - 1;
					prefix_first_key_frame = true;
					continue;
				}
			}
		}

		if (pkt->type == OBS_ENCODER_VIDEO) {
			if (!found_video) {
				stream->replay_system_start_time =
					stream->stream_start_time +
					(pkt->dts_usec * 10);
				info("replay_buffer_save: pkt->dts_usec: %" PRIu64
				     "",
				     pkt->dts_usec);
				video_pts_offset = pkt->pts;
				video_offset = pkt->sys_pts_usec;
				found_video = true;
			}
		} else {
			if (!found_audio[pkt->track_idx]) {
				found_audio[pkt->track_idx] = true;
				audio_offsets[pkt->track_idx] = pkt->dts_usec;
				audio_dts_offsets[pkt->track_idx] = pkt->dts;
			}
		}

		insert_packet(&stream->mux_packets, pkt, video_offset,
			      audio_offsets, video_pts_offset,
			      audio_dts_offsets);

		if (pkt->type == OBS_ENCODER_VIDEO) {
			video_duration = pkt->dts_usec - video_offset;
		}
	}

	stream->duration = video_duration;

	/* ---------------------------- */
	/* generate filename */
	if (stream->replay_path.len) {
		dstr_copy(&stream->path, stream->replay_path.array);
		dstr_replace(&stream->path, "\\", "/");
		dstr_free(&stream->replay_path);
	} else {
		generate_filename(stream, &stream->path, true, false);
	}

	os_atomic_set_bool(&stream->muxing, true);
	stream->mux_thread_joinable = pthread_create(&stream->mux_thread, NULL,
						     replay_buffer_mux_thread,
						     stream) == 0;
	if (!stream->mux_thread_joinable) {
		warn("Failed to create muxer thread");
		os_atomic_set_bool(&stream->muxing, false);
	}
}

static void deactivate_replay_buffer(struct ffmpeg_muxer *stream, int code)
{
	if (code) {
		obs_output_signal_stop(stream->output, code);
	} else if (stopping(stream)) {
		obs_output_end_data_capture(stream->output);
	}

	if (capturing_replay(stream)) {
		stop_capture_replay(stream);
	}

	os_atomic_set_bool(&stream->active, false);
	os_atomic_set_bool(&stream->sent_headers, false);
	os_atomic_set_bool(&stream->stopping, false);	
	replay_buffer_clear(stream);

	if (stream->mux_thread_joinable) {
		pthread_join(stream->mux_thread, NULL);
		stream->mux_thread_joinable = false;
	}
}

static void replay_buffer_start_capture(struct ffmpeg_muxer *stream)
{
	const size_t size = sizeof(struct encoder_packet);
	size_t num_packets = stream->packets.size / size;

	da_reserve(stream->mux_packets, num_packets);

	/* ---------------------------- */
	/* reorder packets */
	bool found_video = false;
	bool found_audio[MAX_AUDIO_MIXES] = {0};
	stream->video_offset = 0;
	stream->video_dts_offset = 0;
	stream->video_pts_offset = 0;
	stream->replay_system_start_time = 0;
	memset(stream->audio_offsets, 0, sizeof(stream->audio_offsets));
	memset(stream->audio_dts_offsets, 0, sizeof(stream->audio_dts_offsets));

	int64_t video_duration = 0;
	int last_key_frame_index = -1;
	bool prefix_first_key_frame = false;
	int pushed_frames = 0;
	for (size_t i = 0; i < num_packets; i++) {
		struct encoder_packet *pkt;
		pkt = deque_data(&stream->packets, i * size);

		// replay has tail..
		if (stream->save_start_pts_usec != 0) {
			if (!prefix_first_key_frame) {
				if (pkt->sys_pts_usec < stream->save_start_pts_usec) {
					if (pkt->keyframe) {
						last_key_frame_index = (int)i;
					}

					// we pass all packets so use the last key frame..
					if ((i + 1) == num_packets &&
					    last_key_frame_index >= 0) {
						i = last_key_frame_index - 1;
						prefix_first_key_frame = true;
						info("apply first key frame in buffer [%d (of %d)]",
						     i, num_packets);
					}
					continue;
				}
			}

			// make sure out first frame is key frame
			if (!found_video) {
				if (!pkt->keyframe &&
				    (i != (num_packets - 1))) {
					i = last_key_frame_index - 1;
					prefix_first_key_frame = true;
					continue;
				}
			}
		}

		if (pkt->type == OBS_ENCODER_VIDEO) {
			if (!found_video) {
				stream->replay_system_start_time =
					stream->stream_start_time +
					(pkt->dts_usec * 10);
				info("replay_buffer_start_capture: pkt->dts_usec: %" PRIu64
				     "",
				     pkt->dts_usec);
				stream->video_offset = pkt->dts_usec;
				stream->video_dts_offset = pkt->dts;
				stream->video_pts_offset = pkt->pts;
				found_video = true;
			}
		} else {
			if (!found_audio[pkt->track_idx]) {
				found_audio[pkt->track_idx] = true;
				stream->audio_offsets[pkt->track_idx] =
					pkt->dts_usec;
				stream->audio_dts_offsets[pkt->track_idx] =
					pkt->dts;
			}
		}

		insert_packet(&stream->mux_packets, pkt,
			      stream->video_offset, stream->audio_offsets,
			      stream->video_pts_offset,
			      stream->audio_dts_offsets);
		pushed_frames++;

		if (pkt->type == OBS_ENCODER_VIDEO) {
			video_duration = pkt->dts_usec - stream->video_offset;
		}
	}

	static bool waiting_for_frame = false;

	// wait for buffer to be fulled with the prefix buffer
	if (!found_video) {
		if (!waiting_for_frame) { // log only once
			warn("start replay, waiting for video frame [frames: %d kf: %d packets: %d]",
			     pushed_frames, last_key_frame_index, num_packets);
		}
		waiting_for_frame = true;
		da_free(stream->mux_packets);
		return;
	}

	waiting_for_frame = false;
	stream->duration = video_duration;

	/* ---------------------------- */
	/* generate filename */
	if (stream->replay_path.len) {
		dstr_copy(&stream->path, stream->replay_path.array);
		dstr_replace(&stream->path, "\\", "/");
		dstr_free(&stream->replay_path);
	} else {
		obs_data_t *settings = obs_output_get_settings(stream->output);
		const char *dir = obs_data_get_string(settings, "directory");
		const char *fmt = obs_data_get_string(settings, "format");
		const char *ext = obs_data_get_string(settings, "extension");
		
		generate_filename(stream, &stream->path, true, false);
		obs_data_release(settings);
	}

	/* ---------------------------- */


	os_atomic_set_bool(&stream->muxing, true);
	os_atomic_set_bool(&stream->capturing_replay, true);

	os_atomic_set_bool(&stream->muxing, true);
	stream->mux_thread_joinable = pthread_create(&stream->mux_thread, NULL,
						     replay_buffer_mux_thread,
						     stream) == 0;
	info("Replay started [timestamp: (%" PRIu64
	     ")]. video offset: (%" PRIu64 ")",
	     (os_gettime_ns() / 1000LL), stream->video_offset);
}

static void replay_buffer_data(void *data, struct encoder_packet *packet)
{
	struct ffmpeg_muxer *stream = data;
	struct encoder_packet pkt;

	if (!active(stream))
		return;

	/* encoder failure */
	if (!packet) {
		deactivate_replay_buffer(stream, OBS_OUTPUT_ENCODE_ERROR);
		return;
	}

	if (stopping(stream)) {
		if (packet->sys_dts_usec >= stream->stop_ts) {
			deactivate_replay_buffer(stream, 0);
			return;
		}
	}

	obs_encoder_packet_ref(&pkt, packet);
	replay_buffer_purge(stream, &pkt);

	if (!stream->packets.size)
		stream->cur_time = pkt.dts_usec;
	stream->cur_size += pkt.size;

	if (stream->stream_start_time == 0) {
		stream->stream_start_time = os_get_epoch_time();
	}

	deque_push_back(&stream->packets, packet, sizeof(*packet));

	if (packet->type == OBS_ENCODER_VIDEO && packet->keyframe) {
		int64_t sys_pts_usec = packet_pts_usec(packet);
		stream->keyframes++;
	}

	if (stream->save_start_pts_usec != 0) {
		if (!stream->capturing_replay) {
			// need to start capture
			if (os_atomic_load_bool(&stream->muxing))
				return;

			if (stream->mux_thread_joinable) {
				pthread_join(stream->mux_thread, NULL);
				stream->mux_thread_joinable = false;
			}

			replay_buffer_start_capture(stream);

			if (!stream->capturing_replay) {
				// lets wait to packets..
				warn("replay buffer canceled?");
				return;
			}
		} else {
			obs_encoder_packet_ref(&pkt, packet);

			pthread_mutex_lock(&stream->replay_packets_mutex);
			deque_push_back(&stream->replay_pending_packets,
					    packet, sizeof(*packet));

			pthread_mutex_unlock(&stream->replay_packets_mutex);
		}
	}
	//int64_t sys_pts_usec = packet_pts_usec(packet);
 
	if (force_stop_capture_replay(stream, packet) ||
	    stream->save_ts != 0 &&
		    /*packet->sys_dts_usec*/ packet->sys_pts_usec >=
			    stream->save_ts) {
		if (stream->capturing_replay) {
			info("Finising replay buffer(save_start_ts: %" PRIu64
			     " - sys_pts_usec:%" PRIu64 " - save_ts: %" PRIu64
			     ")",
			     stream->save_start_pts_usec, packet->sys_pts_usec,
			     stream->save_ts);
			stop_capture_replay(stream);
			return;
		}

		if (os_atomic_load_bool(&stream->muxing))
			return;

		if (stream->mux_thread_joinable) {
			pthread_join(stream->mux_thread, NULL);
			stream->mux_thread_joinable = false;
		}

		stream->save_ts = 0;
		replay_buffer_save(stream);
	}
}

static void replay_buffer_defaults(obs_data_t *s)
{
	obs_data_set_default_int(s, "max_time_sec", 15);
	obs_data_set_default_int(s, "max_size_mb", 500);
	obs_data_set_default_string(s, "format", "%CCYY-%MM-%DD %hh-%mm-%ss");
	obs_data_set_default_string(s, "extension", "mp4");
	obs_data_set_default_bool(s, "allow_spaces", true);
}

struct obs_output_info replay_buffer = {
	.id = "replay_buffer",
	.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_MULTI_TRACK |
		 OBS_OUTPUT_CAN_PAUSE,
	.get_name = replay_buffer_getname,
	.create = replay_buffer_create,
	.destroy = replay_buffer_destroy,
	.start = replay_buffer_start,
	.stop = ffmpeg_mux_stop,
	.encoded_packet = replay_buffer_data,
	.get_total_bytes = ffmpeg_mux_total_bytes,
	.get_defaults = replay_buffer_defaults,
};
