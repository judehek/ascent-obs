/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2017 Overwolf Ltd.
*******************************************************************************/
#include "communications/protocol.h"

namespace libowobs {
namespace protocol {

const char kCommandField[] = "cmd";
const char kCommandIdentifier[] = "identifier";
const char kTypeField[]    = "recorder_type";
const char kEventField[] = "event";
const char kErrorCodeField[] = "code";
const char kErrorDescField[] = "desc";


const char kAudioInputDevices[] = "adio_in_devs";
const char kAudioOutputDevices[] = "adio_out_devs";
const char kVideoEncoders[] = "vid_encs";
const char kWinrtCaptureSupported[] = "winrt_capture_supported";
const char KIsWindowCapture[] = "is_window_capture";
const char kFilenameField[] = "filename";
const char kMaxFileSizeField[] = "max_file_size_bytes";
const char kEnableOnDemandSplitField[] = "enbale_on_demand_spilt_video";
const char kIncludeFullVideoField[] = "include_full_video";


const char kVideoEncoderId_x264[] = "obs_x264";
const char kVideoEncoderId_QuickSync[] = "obs_qsv11_v2";
const char kVideoEncoderId_QuickSync_HEVC[] = "obs_qsv11_hevc";
const char kVideoEncoderId_QuickSync_AV1[] = "obs_qsv11_av1";
const char kVideoEncoderId_AMF[] = "h264_texture_amf";
const char kVideoEncoderId_AMF_AV1[] = "av1_texture_amf";
const char kVideoEncoderId_AMF_HEVC[] = "h265_texture_amf";
const char kVideoEncoderId_NVENC[] = "ffmpeg_nvenc";
const char kVideoEncoderId_NVENC_NEW[] = "jim_nvenc";
const char kVideoEncoderId_NVENC_HEVC[] = "jim_hevc_nvenc";
const char kVideoEncoderId_NVENC_AV1[] = "jim_av1_nvenc";


const char kStatsDataField[] = "stats_data";

}
}