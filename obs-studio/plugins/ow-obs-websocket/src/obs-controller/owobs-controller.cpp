#include "owobs-controller.hpp"

#include "obs.h"
#include <util\command_line.hpp>
#include <util\dstr.hpp>
#include <util\platform.h>
#include <util\profiler.hpp>
#include "../../../ow-obs/switches.hpp"

#if _WIN32
#include <windows.h>
#endif


#define SETTITING_FILE_NAME "settings.ini"

#ifdef __APPLE__
#define DEFAULT_CONTAINER "fragmented_mov"
#else
#define DEFAULT_CONTAINER "fragmented_mp4"
#endif

#ifdef __APPLE__
#define INPUT_AUDIO_SOURCE "coreaudio_input_capture"
#define OUTPUT_AUDIO_SOURCE "coreaudio_output_capture"
#elif _WIN32
#define INPUT_AUDIO_SOURCE "wasapi_input_capture"
#define OUTPUT_AUDIO_SOURCE "wasapi_output_capture"
#else
#define INPUT_AUDIO_SOURCE "pulse_input_capture"
#define OUTPUT_AUDIO_SOURCE "pulse_output_capture"
#endif

#define DL_OPENGL "libobs-opengl.dll" 
#define DL_D3D11  "libobs-d3d11.dll"

#define VOLUME_METER_DECAY_FAST 23.53
#define VOLUME_METER_DECAY_MEDIUM 11.76
#define VOLUME_METER_DECAY_SLOW 8.57

#ifdef _WIN32
#define IS_WIN32 1
#else
#define IS_WIN32 0
#endif

using namespace std;

bool hardwareEncodingAvailable = false;
bool nvencAvailable = false;
bool qsvAvailable = false;
bool vceAvailable = false;
bool appleAvailable = false;

struct AddSourceData {
	obs_source_t *source;
	bool visible;
	obs_transform_info *transform = nullptr;
	obs_sceneitem_crop *crop = nullptr;
	obs_blending_method *blend_method = nullptr;
	obs_blending_type *blend_mode = nullptr;
};

namespace {
void TestHardwareEncoding()
{
	size_t idx = 0;
	const char *id;
	while (obs_enum_encoder_types(idx++, &id)) {
		if (strcmp(id, "ffmpeg_nvenc") == 0)
			hardwareEncodingAvailable = nvencAvailable = true;
		else if (strcmp(id, "obs_qsv11") == 0)
			hardwareEncodingAvailable = qsvAvailable = true;
		else if (strcmp(id, "h264_texture_amf") == 0)
			hardwareEncodingAvailable = vceAvailable = true;
#ifdef __APPLE__
		else if (strcmp(id,
				"com.apple.videotoolbox.videoencoder.ave.avc") ==
				 0
#ifndef __aarch64__
			 && os_get_emulation_status() == true
#endif
		)
			if (__builtin_available(macOS 13.0, *))
				hardwareEncodingAvailable = appleAvailable =
					true;
#endif
	}
}

static inline enum video_format GetVideoFormatFromName(const char *name)
{
	if (astrcmpi(name, "I420") == 0)
		return VIDEO_FORMAT_I420;
	else if (astrcmpi(name, "NV12") == 0)
		return VIDEO_FORMAT_NV12;
	else if (astrcmpi(name, "I444") == 0)
		return VIDEO_FORMAT_I444;
	else if (astrcmpi(name, "I010") == 0)
		return VIDEO_FORMAT_I010;
	else if (astrcmpi(name, "P010") == 0)
		return VIDEO_FORMAT_P010;
	else if (astrcmpi(name, "P216") == 0)
		return VIDEO_FORMAT_P216;
	else if (astrcmpi(name, "P416") == 0)
		return VIDEO_FORMAT_P416;
#if 0 //currently unsupported
	else if (astrcmpi(name, "YVYU") == 0)
		return VIDEO_FORMAT_YVYU;
	else if (astrcmpi(name, "YUY2") == 0)
		return VIDEO_FORMAT_YUY2;
	else if (astrcmpi(name, "UYVY") == 0)
		return VIDEO_FORMAT_UYVY;
#endif
	else
		return VIDEO_FORMAT_BGRA;
}

static inline enum video_colorspace GetVideoColorSpaceFromName(const char *name)
{
	enum video_colorspace colorspace = VIDEO_CS_SRGB;
	if (strcmp(name, "601") == 0)
		colorspace = VIDEO_CS_601;
	else if (strcmp(name, "709") == 0)
		colorspace = VIDEO_CS_709;
	else if (strcmp(name, "2100PQ") == 0)
		colorspace = VIDEO_CS_2100_PQ;
	else if (strcmp(name, "2100HLG") == 0)
		colorspace = VIDEO_CS_2100_HLG;

	return colorspace;
}

static inline enum obs_scale_type GetScaleType(ConfigFile &basicConfig)
{
	const char *scaleTypeStr = config_get_string(basicConfig, "Video", "ScaleType");

	if (astrcmpi(scaleTypeStr, "bilinear") == 0)
		return OBS_SCALE_BILINEAR;
	else if (astrcmpi(scaleTypeStr, "lanczos") == 0)
		return OBS_SCALE_LANCZOS;
	else if (astrcmpi(scaleTypeStr, "area") == 0)
		return OBS_SCALE_AREA;
	else
		return OBS_SCALE_BICUBIC;
}

#if _WIN32
static HWND sDisplayHWND = nullptr;
static HWND CreateDisplayWindow()
{
	if (sDisplayHWND) {
		return sDisplayHWND;
	}
	WNDCLASS wc;
	HINSTANCE instance = GetModuleHandle(NULL);
	memset(&wc, 0, sizeof(wc));
	wc.lpszClassName = TEXT("OW-OBS-DISPLAY");
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.hInstance = instance;
	wc.lpfnWndProc = (WNDPROC)DefWindowProc;

	if (!RegisterClass(&wc)) {

		return 0;
	}

	sDisplayHWND = CreateWindow(TEXT("OW-OBS-DISPLAY"), 
		TEXT("OW-OBS-DISPLAY-WINDOW"),
		WS_OVERLAPPEDWINDOW | WS_VISIBLE, 		
		0, 0, (int)(1920 * 0.5), (int)(1080 * 0.5),
		NULL, NULL,
		instance, 
		NULL);
	return sDisplayHWND;
}

static obs_display_t *CreateDisplay(HWND hwnd)
{
	RECT rc;
	GetClientRect(hwnd, &rc);

	gs_init_data info = {};
	info.cx = rc.right;
	info.cy = rc.bottom;
	info.format = GS_RGBA;
	info.zsformat = GS_ZS_NONE;
	info.window.hwnd = hwnd;

	return obs_display_create(&info, 0);
}

static inline void GetScaleAndCenterPos(int baseCX, int baseCY, int windowCX, int windowCY, int &x, int &y, float &scale)
{
	double windowAspect, baseAspect;
	int newCX, newCY;

	windowAspect = double(windowCX) / double(windowCY);
	baseAspect = double(baseCX) / double(baseCY);

	if (windowAspect > baseAspect) {
		scale = float(windowCY) / float(baseCY);
		newCX = int(double(windowCY) * baseAspect);
		newCY = windowCY;
	} else {
		scale = float(windowCX) / float(baseCX);
		newCX = windowCX;
		newCY = int(float(windowCX) / baseAspect);
	}

	x = windowCX / 2 - newCX / 2;
	y = windowCY / 2 - newCY / 2;
}


static void RenderWindow(void *data, uint32_t cx, uint32_t cy)
{
	OWOBSController *controller = static_cast<OWOBSController *>(data);

	RECT rc;
	GetClientRect(sDisplayHWND, &rc);

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	uint32_t windowCX = rc.right - rc.left;
	uint32_t windowCY = rc.bottom - rc.top;

	uint32_t displayCX, displayCY = 0;    
	obs_display_size(controller->display(), &displayCX, &displayCY);

	if (displayCX != windowCX || windowCY != displayCY) {
		obs_display_resize(controller->display(), windowCX, windowCY);
	}

	int x, y = 0;;
	int newCX, newCY = 0;
	float scale = 0;

	GetScaleAndCenterPos(ovi.base_width, ovi.base_height, windowCX, windowCY, x, y, scale);

	newCX = int(scale * float(windowCX));
	newCY = int(scale * float(windowCY));

	gs_viewport_push();
	gs_projection_push();

	gs_ortho(0.0f, float(ovi.base_width), 0.0f, float(ovi.base_height), -100.0f, 100.0f);
	gs_set_viewport(0, 0, newCX, newCY);

	obs_render_main_texture();

	gs_projection_pop();
	gs_viewport_pop();
}

#endif

} // namespace

OWOBSController::OWOBSController()
{
	LoadConfig();
}

OWOBSController::~OWOBSController() {
}

bool OWOBSController::Init()
{
	blog(LOG_INFO, "init ow-obs controller");
	config_t *config = config_;
	if (!config) {
		blog(LOG_ERROR, "fail to load config");
		return false;
	}

	//TestHardwareEncoding();

	InitBasicConfigDefaults();

	CreateScene();

	ResetVideo();

	return true;
}

bool OWOBSController::EncoderAvailable(const char *encoder)
{
	const char *val;
	int i = 0;

	while (obs_enum_encoder_types(i++, &val)) {
		if (strcmp(val, encoder) == 0) {
			return true;
		}
	}

	return false;
}

bool OWOBSController::SourceAvailable(const char *source_id) {
	const char *val;
	int i = 0;

	while (obs_enum_source_types(i++, &val)) {
		if (strcmp(val, source_id) == 0) {
			return true;
		}
	}

	return false;
}

const char *OWOBSController::InputAudioSource() const {
	return INPUT_AUDIO_SOURCE;
}

const char *OWOBSController::OutputAudioSource() const {
	return OUTPUT_AUDIO_SOURCE;
}

const char *OWOBSController::GetRenderModule() const
{
	const char *renderer = config_get_string(config_, "Video", "Renderer");
	return (astrcmpi(renderer, "Direct3D 11") == 0) ? DL_D3D11 : DL_OPENGL;
}

void OWOBSController::CreateDebugWindow() {

#ifndef _WIN32
	return;
#endif // !_WIN32
	if (!display_.get()) {
		display_.reset(new Utils::owobs::DisplayContext(
			CreateDisplay(CreateDisplayWindow())));

		obs_display_add_draw_callback(display_->get(), RenderWindow, this);
	}	
}

int OWOBSController::ResetVideo() {
{
		blog(LOG_INFO, "reset video settings");

		ProfileScope("OWOBSController::ResetVideo");

		struct obs_video_info ovi;
		int ret = -1;

		uint32_t fpsType = (uint32_t)config_get_uint(config_, "Video", "FPSType");
		if (fpsType > 0) {
			ovi.fps_num = (uint32_t)config_get_uint(config_, "Video", "FPSNum");
			ovi.fps_den = (uint32_t)config_get_uint(config_, "Video", "FPSDen");
		} else {
			ovi.fps_num = (uint32_t)config_get_uint(config_, "Video", "FPSInt");
			ovi.fps_den = 1;
		}

		const char *colorFormat = config_get_string(config_, "Video", "ColorFormat");
		const char *colorSpace = config_get_string(config_, "Video", "ColorSpace");
		const char *colorRange = config_get_string(config_, "Video", "ColorRange");

		ovi.graphics_module = GetRenderModule();
		ovi.base_width = (uint32_t)config_get_uint(config_, "Video", "BaseCX");
		ovi.base_height = (uint32_t)config_get_uint(config_, "Video", "BaseCY");
		ovi.output_width = (uint32_t)config_get_uint(config_, "Video", "OutputCX");
		ovi.output_height = (uint32_t)config_get_uint(config_, "Video", "OutputCY");
		ovi.output_format = GetVideoFormatFromName(colorFormat);
		ovi.colorspace = GetVideoColorSpaceFromName(colorSpace);
		ovi.range = astrcmpi(colorRange, "Full") == 0 ? VIDEO_RANGE_FULL : VIDEO_RANGE_PARTIAL;
		ovi.adapter = (int)config_get_uint(config_, "Video", "AdapterIdx");
		ovi.gpu_conversion = true;
		ovi.scale_type = GetScaleType(config_);

		if (ovi.base_width < 32 || ovi.base_height < 32) {
			ovi.base_width = 1920;
			ovi.base_height = 1080;
			config_set_uint(config_, "Video", "BaseCX", 1920);
			config_set_uint(config_, "Video", "BaseCY", 1080);
		}

		if (ovi.output_width < 32 || ovi.output_height < 32) {
			ovi.output_width = ovi.base_width;
			ovi.output_height = ovi.base_height;
			config_set_uint(config_, "Video", "OutputCX", ovi.base_width);
			config_set_uint(config_, "Video", "OutputCY", ovi.base_height);
		}

		ret = obs_reset_video(&ovi);
		if (ret == OBS_VIDEO_CURRENTLY_ACTIVE) {
			blog(LOG_WARNING, "Tried to reset when already active");
			return ret;
		}

		if (ret == OBS_VIDEO_SUCCESS) {	
			const float sdr_white_level = (float)config_get_uint(
				config_, "Video", "SdrWhiteLevel");
			const float hdr_nominal_peak_level = (float)config_get_uint(
				config_, "Video", "HdrNominalPeakLevel");
			obs_set_video_levels(sdr_white_level, hdr_nominal_peak_level);

		} else {
			blog(LOG_WARNING, "Reset video error %d", ret);
		}

		return ret;
	}
}

bool OWOBSController::ResetAudio() {
	ProfileScope("OWOBSController::ResetAudio");

	struct obs_audio_info2 ai = {};
	ai.samples_per_sec = (uint32_t)config_get_uint(config_, "Audio", "SampleRate");

	const char *channelSetupStr = config_get_string(config_, "Audio", "ChannelSetup");

	if (strcmp(channelSetupStr, "SPEAKERS_MONO") == 0)
		ai.speakers = SPEAKERS_MONO;
	else if (strcmp(channelSetupStr, "SPEAKERS_STEREO") == 0)
		ai.speakers = SPEAKERS_2POINT1;
	else if (strcmp(channelSetupStr, "SPEAKERS_2POINT1") == 0)
		ai.speakers = SPEAKERS_4POINT0;
	else if (strcmp(channelSetupStr, "SPEAKERS_4POINT0") == 0)
		ai.speakers = SPEAKERS_4POINT1;
	else if (strcmp(channelSetupStr, "SPEAKERS_4POINT1") == 0)
		ai.speakers = SPEAKERS_5POINT1;
	else if (strcmp(channelSetupStr, "SPEAKERS_7POINT1") == 0)
		ai.speakers = SPEAKERS_7POINT1;
	else
		ai.speakers = SPEAKERS_STEREO;

	bool lowLatencyAudioBuffering = config_get_bool(config_, "Audio", "LowLatencyAudioBuffering");
	if (lowLatencyAudioBuffering) {
		ai.max_buffering_ms = 20;
		ai.fixed_buffering = true;
	}

	return obs_reset_audio2(&ai);
	
}

static void AddSourceFunc(void *_data, obs_scene_t *scene)
{
	AddSourceData *data = (AddSourceData *)_data;
	obs_sceneitem_t *sceneitem;

	sceneitem = obs_scene_add(scene, data->source);
	if (!sceneitem) {
		blog(LOG_ERROR, "fail to add source [%s] to scene",
			obs_source_get_name(data->source));
		return;
	}

	if (data->transform != nullptr)
		obs_sceneitem_set_info2(sceneitem, data->transform);
	if (data->crop != nullptr)
		obs_sceneitem_set_crop(sceneitem, data->crop);
	if (data->blend_method != nullptr)
		obs_sceneitem_set_blending_method(sceneitem, *data->blend_method);
	if (data->blend_mode != nullptr)
		obs_sceneitem_set_blending_mode(sceneitem, *data->blend_mode);

	obs_sceneitem_set_visible(sceneitem, data->visible);
}


void OWOBSController::AddSource(obs_source_t *source, bool stretchToOutputSize /*= false */)
{
	AddSourceData data;
	data.source = source;
	data.visible = true;

	obs_enter_graphics();
	obs_scene_atomic_update(scene_->get_scene(), AddSourceFunc, &data);
	obs_leave_graphics();

	if (stretchToOutputSize) {
		StretchToOutputSize(source);
	}
}

// from  window-basic-main.cpp:: CenterAlignSelectedItems
void OWOBSController::StretchToOutputSize(obs_source_t *source) {
	if (!source) {
		return;
	}

	OBSSceneItemAutoRelease item = obs_scene_sceneitem_from_source(
		 scene_->get_scene(),
		 source);

	if (!item.Get()) {
		return;
	}

	blog(LOG_INFO, "Strech to screen: %s", obs_source_get_name(source));

	obs_bounds_type boundsType = OBS_BOUNDS_STRETCH;
	obs_video_info ovi;
	obs_get_video_info(&ovi);

	obs_transform_info itemInfo;
	vec2_set(&itemInfo.pos, 0.0f, 0.0f);
	vec2_set(&itemInfo.scale, 1.0f, 1.0f);
	itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	itemInfo.rot = 0.0f;

	vec2_set(&itemInfo.bounds, float(ovi.base_width), float(ovi.base_height));
	itemInfo.bounds_type = boundsType;
	itemInfo.bounds_alignment = OBS_ALIGN_CENTER;
	itemInfo.crop_to_bounds = obs_sceneitem_get_bounds_crop(item);

	obs_sceneitem_set_info2(item, &itemInfo);
}

void OWOBSController::LoadConfig()
{
	auto settingFolder = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
		owobs::switches::kSetttingFolder);

	if (settingFolder.empty()) {
		blog(LOG_ERROR, "[owobs conteoller] missing setting folder");
		char currentFolder[1024] = {};
		settingFolder = os_getcwd(currentFolder, 1024);
	}

	std::string path(settingFolder);
	path += "//";
	path += SETTITING_FILE_NAME;

	auto res = config_.Open(path.c_str(), CONFIG_OPEN_ALWAYS);
	if (res == CONFIG_SUCCESS) {
		return;
	}

	blog(LOG_ERROR, "can't open/create setting file %s [%d]", path.c_str(),
	     res);
}

void OWOBSController::CreateScene() {
	scene_.reset(new Utils::owobs::SceneContext(obs_scene_create("Scene")));
	obs_set_output_source(0, scene_->get_scene_source());
}

bool OWOBSController::InitBasicConfigDefaults()
{
	uint32_t cx = 1920;
	uint32_t cy = 1080;

	config_t *basicConfig = config_;

	/* ----------------------------------------------------- */
	config_set_default_bool(basicConfig, "AdvOut", "ApplyServiceSettings",
				true);
	config_set_default_bool(basicConfig, "AdvOut", "UseRescale", false);
	config_set_default_uint(basicConfig, "AdvOut", "TrackIndex", 1);
	config_set_default_uint(basicConfig, "AdvOut", "VodTrackIndex", 2);
	config_set_default_string(basicConfig, "AdvOut", "Encoder", "obs_x264");

	config_set_default_string(basicConfig, "AdvOut", "RecType", "Standard");
	config_set_default_string(basicConfig, "AdvOut", "RecFormat2",
				  DEFAULT_CONTAINER);
	config_set_default_bool(basicConfig, "AdvOut", "RecUseRescale", false);
	config_set_default_uint(basicConfig, "AdvOut", "RecTracks", (1 << 0));
	config_set_default_uint(basicConfig, "AdvOut", "FLVTrack", 1);
	config_set_default_uint(basicConfig, "AdvOut",
				"StreamMultiTrackAudioMixes", 1);
	config_set_default_bool(basicConfig, "AdvOut", "FFOutputToFile", true);
	config_set_default_string(basicConfig, "AdvOut", "FFExtension", "mp4");
	config_set_default_uint(basicConfig, "AdvOut", "FFVBitrate", 8000);
	config_set_default_uint(basicConfig, "AdvOut", "FFVGOPSize", 250);
	config_set_default_bool(basicConfig, "AdvOut", "FFUseRescale", false);
	config_set_default_bool(basicConfig, "AdvOut", "FFIgnoreCompat", false);
	config_set_default_uint(basicConfig, "AdvOut", "FFABitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "FFAudioMixes", 1);
	config_set_default_uint(basicConfig, "AdvOut", "Track1Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track2Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track3Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track4Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track5Bitrate", 160);
	config_set_default_uint(basicConfig, "AdvOut", "Track6Bitrate", 160);

	config_set_default_uint(basicConfig, "AdvOut", "RecSplitFileTime", 15);
	config_set_default_uint(basicConfig, "AdvOut", "RecSplitFileSize",
				2048);

	config_set_default_bool(basicConfig, "AdvOut", "RecRB", false);
	config_set_default_uint(basicConfig, "AdvOut", "RecRBTime", 20);
	config_set_default_int(basicConfig, "AdvOut", "RecRBSize", 512);

	config_set_default_uint(basicConfig, "Video", "BaseCX", cx);
	config_set_default_uint(basicConfig, "Video", "BaseCY", cy);

	config_set_default_uint(basicConfig, "Video", "OutputCX", cx);
	config_set_default_uint(basicConfig, "Video", "OutputCY", cy);
	config_set_default_string(basicConfig, "Output", "FilenameFormatting",
				  "%CCYY-%MM-%DD %hh-%mm-%ss");

	config_set_default_bool(basicConfig, "Output", "DelayEnable", false);
	config_set_default_uint(basicConfig, "Output", "DelaySec", 20);
	config_set_default_bool(basicConfig, "Output", "DelayPreserve", true);

	config_set_default_bool(basicConfig, "Output", "Reconnect", true);
	config_set_default_uint(basicConfig, "Output", "RetryDelay", 2);
	config_set_default_uint(basicConfig, "Output", "MaxRetries", 25);

	config_set_default_string(basicConfig, "Output", "BindIP", "default");
	config_set_default_string(basicConfig, "Output", "IPFamily",
				  "IPv4+IPv6");
	config_set_default_bool(basicConfig, "Output", "NewSocketLoopEnable",
				false);
	config_set_default_bool(basicConfig, "Output", "LowLatencyEnable",
				false);

	config_set_default_uint(basicConfig, "Video", "FPSType", 0);
	config_set_default_string(basicConfig, "Video", "FPSCommon", "30");
	config_set_default_uint(basicConfig, "Video", "FPSInt", 30);
	config_set_default_uint(basicConfig, "Video", "FPSNum", 30);
	config_set_default_uint(basicConfig, "Video", "FPSDen", 1);
	config_set_default_string(basicConfig, "Video", "ScaleType", "bicubic");
	config_set_default_string(basicConfig, "Video", "ColorFormat", "NV12");
	config_set_default_string(basicConfig, "Video", "ColorSpace", "709");
	config_set_default_string(basicConfig, "Video", "ColorRange",
				  "Partial");
	config_set_default_uint(basicConfig, "Video", "SdrWhiteLevel", 300);
	config_set_default_uint(basicConfig, "Video", "HdrNominalPeakLevel",
				1000);

	config_set_default_string(basicConfig, "Audio", "MonitoringDeviceId",
				  "default");
	config_set_default_uint(basicConfig, "Audio", "SampleRate", 48000);
	config_set_default_string(basicConfig, "Audio", "ChannelSetup",
				  "SPEAKERS_STEREO");
	config_set_default_double(basicConfig, "Audio", "MeterDecayRate",
				  VOLUME_METER_DECAY_FAST);
	config_set_default_uint(basicConfig, "Audio", "PeakMeterType", 0);

#if _WIN32
	config_set_default_string(basicConfig, "Video", "Renderer", "Direct3D 11");
#else
	config_set_default_string(basicConfig, "Video", "Renderer", "OpenGL");
#endif

	bool useNV = EncoderAvailable("ffmpeg_nvenc");

	const char *aac_default = "ffmpeg_aac";
	if (EncoderAvailable("CoreAudio_AAC"))
		aac_default = "CoreAudio_AAC";
	else if (EncoderAvailable("libfdk_aac"))
		aac_default = "libfdk_aac";

	config_set_default_string(basicConfig, "AdvOut", "AudioEncoder",
				  aac_default);
	config_set_default_string(basicConfig, "AdvOut", "RecAudioEncoder",
				  aac_default);

	return true;
}
