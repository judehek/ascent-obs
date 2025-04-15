#pragma once

#include <obs-module.h>
#include <obs-hotkey.h>
#include <util/deque.h>
#include <util/darray.h>
#include <util/dstr.h>
#include <util/pipe.h>
#include <util/platform.h>
#include <util/threading.h>

typedef DARRAY(struct encoder_packet) mux_packets_t;

struct ffmpeg_muxer {
	obs_output_t *output;
	os_process_pipe_t *pipe;
	int64_t stop_ts;
	uint64_t total_bytes;
	bool sent_headers;
	volatile bool active;
	volatile bool capturing;
	volatile bool stopping;
	struct dstr path;
	struct dstr printable_path;
	struct dstr muxer_settings;
	struct dstr stream_key;
	int64_t duration; // full video duration
	bool disk_space_warning_sent;

	/* replay buffer and split file */
	int64_t cur_size;
	int64_t cur_time;
	int64_t max_size;
	int64_t max_time;
	int split_index;

	/* replay buffer */
	int64_t save_ts;
	int64_t save_start_pts_usec;
	struct dstr replay_path;
	bool replay_stopping;
	bool fully_armed;
	pthread_mutex_t replay_packets_mutex;
	struct deque replay_pending_packets;
	volatile bool capturing_replay;

	int keyframes;
	obs_hotkey_id hotkey;
	volatile bool muxing;
	mux_packets_t mux_packets;

	/* split file */
	bool found_video;
	bool found_audio[MAX_AUDIO_MIXES];
	int64_t video_pts_offset;
	int64_t video_dts_offset;
	int64_t video_offset;
	int64_t audio_dts_offsets[MAX_AUDIO_MIXES];
	int64_t audio_offsets[MAX_AUDIO_MIXES];

	bool split_file_ready;
	volatile bool manual_split;

	int64_t on_demand_split_pts;
	uint64_t on_demand_split_timestamp_ms_epoch;
	int64_t last_video_pts;

	/* these are accessed both by replay buffer and by HLS */
	pthread_t mux_thread;
	bool mux_thread_joinable;
	struct deque packets;

	/* HLS only */
	int keyint_sec;
	pthread_mutex_t write_mutex;
	os_sem_t *write_sem;
	os_event_t *stop_event;
	bool is_hls;
	int dropped_frames;
	int min_priority;
	int64_t last_dts_usec;

	bool is_network;
	bool split_file;
	bool allow_overwrite;

	void* full_video; // ffmpeg_muxer*
	uint64_t replay_system_start_time;
	uint64_t stream_start_time; // different usage for replay \ recorder with split
	int64_t free_disk_space;
	int64_t origin_free_disk_space;
};

bool stopping(struct ffmpeg_muxer *stream);
bool active(struct ffmpeg_muxer *stream);
void start_pipe(struct ffmpeg_muxer *stream, const char *path);
bool write_packet(struct ffmpeg_muxer *stream, struct encoder_packet *packet);
bool send_headers(struct ffmpeg_muxer *stream);
int deactivate(struct ffmpeg_muxer *stream, int code);
void ffmpeg_mux_stop(void *data, uint64_t ts);
uint64_t ffmpeg_mux_total_bytes(void *data);
