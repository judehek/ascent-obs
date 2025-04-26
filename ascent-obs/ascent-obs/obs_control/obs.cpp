#include "obs_control/obs.h"
#include "obs_control/obs_utils.h"

#include "obs_control/settings.h"
#include "obs_control/obs_audio_source_control.h"

#include "obs_control/scene/utils.h"
#include "obs_control/scene/brb_source.h"
#include "obs_control/scene/monitor_source.h"
#include "obs_control/scene/window_source.h"
#include "obs_control/scene/generic_obs_source.h"
#include "obs_control/scene/game_capture_source.h"
#include "obs_control/scene/tobii_gaze_overlay_source.h"
#include "obs_control/settings.h"
#include "command_line.h"
#include "switches.h"
#include <communications/protocol.h>
#include <mutex>
#include <numeric>

#include <util/windows/win-version.h>


using namespace settings;

namespace obs_control {

  const char kErrorChangedSettingsWhileActive[] =
    "can't change video settings while active";

  const char kErrorScenCreate[] =
    "couldn't create scene";

  const char kErrorEmptyScenCreate[] =
    "couldn't create sources";

  static const char *blacklisted_nvidia[] = {
   "GeForce MX110",
   "GeForce MX130",
   "GeForce MX150",
   "GeForce GT 1030",
   "Quadro P500",
   "GeForce 830M",
   "GeForce 840M",
   "GeForce 920M",
   "GeForce 920MX",
   "GeForce 930M",
   "GeForce 930MX",
   "GeForce 940M",
   "GeForce 940MX",
   "GeForce MX450",
   "GeForce 810M",
   "GeForce 800M",
   "GeForce 825M",
   "GeForce GTX 780 TI",
   "GeForce GTX 780",
   "GeForce GTX 760",
   "GeForce GTX 760 Ti",
   "GeForce GTX 775M",
   "GeForce GTX 770M",
   "GeForce GTX 765M",
   "GeForce GTX 760M",
   "GeForce GTX 650",
   "GeForce GTX 660",
   "GeForce GTX 880M",
   "GeForce GTX 870M",
   "GeForce 720M",
   "GeForce 710M",
   "GeForce 705M",
   "GeForce GTX 690",
   "GeForce GTX 680",
   "GeForce GTX 670",
   "GeForce GTX 660 Ti",   
   "GeForce GTX 650 Ti BOOST",
   "GeForce GTX 650 Ti",   
   "GeForce GTX 645",
   "GeForce GT 645",
   "GeForce GT 640",
   "GeForce GT 635",   
   "GeForce GT 625",
   "GeForce GT 620",
   "GeForce GT 610",
   "GeForce GT 630",
   "GeForce GT 420",
   "GeForce GT 740",
   "GeForce GT 730",
   "GeForce GT 720",
   "GeForce GT 710",
   "GeForce GT 705",
   "GeForce GT 755M",
   "GeForce GT 750M",
   "GeForce GT 745M",
   "GeForce GT 740M",
   "GeForce GT 735M",
   "GeForce GT 730M",
   "GeForce GT 720M",
   "GeForce GT 710M",
   NULL
  };

  const char* kColorSpaceKey = "color_space";
  const char* kColorFormatKey = "color_format";
  const char kVideoCustomParamterInternalKey[] = "custom_sources";

  obs_data_t* custom_source_setting = nullptr;
  
  void eraseAllSubStr(std::string & mainStr, const std::string & toErase) {
    size_t pos = std::string::npos;

    // Search for the substring in string in a loop untill nothing is found
    while ((pos = mainStr.find(toErase)) != std::string::npos) {
      // If found then erase it from string
      mainStr.erase(pos, toErase.length());
    }
  }

  bool isBlackInBlackList(const char* adapter_name, const char* codec) {
    std::string adapter(adapter_name);
    std::string codec_str(codec);
    if (codec_str.find("nvenc") == std::string::npos) {
      return false;
    }

    // nvec
    eraseAllSubStr(adapter, "NVIDIA ");
    eraseAllSubStr(adapter, "nvidia ");

    std::string adpater_name_str(adapter_name);
    for (const char **vals = blacklisted_nvidia; *vals; vals++) {       
      //if (strcmpi(*vals, adapter.c_str()) == 0) {
      if (adpater_name_str.find(*vals) != std::string::npos) {
        blog(LOG_WARNING, "Adapter '%s' is blacklisted", vals);
        return true;
      }
    }

    return false;
  }

  enum video_colorspace GetVideoColorSpaceFromName(
      const char* name) {
    enum video_colorspace colorspace = VIDEO_CS_709;
    if (!name)
      return colorspace;
    else if (strcmp(name, "Rec601") == 0)
      colorspace = VIDEO_CS_601;
    else if (strcmp(name, "Rec709") == 0)
      colorspace = VIDEO_CS_709;
    else if (strcmp(name, "Rec2100PQ") == 0)
      colorspace = VIDEO_CS_2100_PQ;
    else if (strcmp(name, "Rec2100HLG") == 0)
      colorspace = VIDEO_CS_2100_HLG;
    else if (strcmp(name, "RecsRGB") == 0)
      colorspace = VIDEO_CS_SRGB;
    return colorspace;
  }

  enum video_format GetVideoColorFormatFromName(const char* name) {
    if (!name) {
      return VIDEO_FORMAT_NV12;
    }
    if (strcmp(name, "I420") == 0)
      return VIDEO_FORMAT_I420;
    else if (strcmp(name, "NV12") == 0)
      return VIDEO_FORMAT_NV12;
    else if (strcmp(name, "I444") == 0)
      return VIDEO_FORMAT_I444;
    else if (strcmp(name, "I010") == 0)
      return VIDEO_FORMAT_I010;
    else if (strcmp(name, "P010") == 0)
      return VIDEO_FORMAT_P010;
    else if (strcmp(name, "P216") == 0)
      return VIDEO_FORMAT_P216;
    else if (strcmp(name, "P416") == 0)
      return VIDEO_FORMAT_P416;

    else
      return VIDEO_FORMAT_NV12;
  }

  };  // namespace obs_control

using namespace obs_control;
using namespace libascentobs;

//------------------------------------------------------------------------------
OBS::OBS() :
  current_visible_source_(nullptr),
  split_video_counter_(0),
  output_width_(0),
  output_height_(0),
  black_texture_tester_(nullptr),
  test_black_texture_(false),
  next_black_test_time_stamp(0),
  black_texture_detection_counter(0),
  did_notify_switchable_devices_(false),
  compatibility_mode_(false),
  capture_game_cursor_(true),
  obs_audio_controller_(new OBSAudioControl()){
}

//------------------------------------------------------------------------------
OBS::~OBS() {
  blog(LOG_INFO, "releasing obs");

  advanced_output_.reset();
  display_tester_.reset();

  if (stats_time_.get()) {
    stats_time_->Stop();
  }

  if (stop_replay_timer_.get()) {
    stop_replay_timer_->Stop();
  }

  monitor_source_.reset();
  window_source_.reset();
  brb_source_.reset();
  game_source_.reset();
  tobii_source_.reset();
  generic_obs_source_.clear();

  display_context_.reset();
  obs_audio_controller_.reset();

  std::unique_lock<std::mutex> lock(access_mutex_);
  blog(LOG_INFO, "releasing scene");
  scene_.reset(nullptr);


  blog(LOG_INFO, "remove all scenes and sources");
  auto cb = [](void*, obs_source_t* source) {

    auto* name = obs_source_get_name(source);
    blog(LOG_INFO, "remove source %s",name);
    obs_source_remove(source);
    return true;
  };

  obs_enum_scenes(cb, nullptr);
  obs_enum_sources(cb, nullptr);

  blog(LOG_INFO, "wait for destroy queue");
  do {
  } while (obs_wait_for_destroy_queue());

  
  obs_wait_for_destroy_queue();
  /* If scene data wasn't actually cleared, e.g. faulty plugin holding a
   * reference, they will still be in the hash table, enumerate them and
   * store the names for logging purposes. */
  auto cb2 = [](void* param, obs_source_t* source) {
    auto orphans = static_cast<std::vector<std::string>*>(param);
    orphans->push_back(obs_source_get_name(source));
    return true;
  };

  std::vector<std::string> orphan_sources;
  obs_enum_sources(cb2, &orphan_sources);
 
  if (!orphan_sources.empty()) {
    /* This ugly mess exists to join a vector of strings
     * with a user-defined delimiter. */
    std::string orphan_names = std::accumulate(
        orphan_sources.begin(), orphan_sources.end(), std::string(""),
        [](std::string a, std::string b) { return std::move(a) + "\n- " + b; });
    blog(LOG_ERROR,
         "Not all sources were cleared when clearing scene data:\n%s\n",
         orphan_names.c_str());
  } else {
    blog(LOG_INFO, "All scene data cleared");
  }

  blog(LOG_INFO, "------------------------------------------------");
}

//------------------------------------------------------------------------------
bool OBS::Startup(OBSControlCommunications* communications,
  libascentobs::SharedThreadPtr command_thread) {
  blog(LOG_INFO, "starting up obs");
  if (!obs_startup("en-US", nullptr, nullptr)) {
    return false;
  }

  communications_ = communications;
  command_thread_ = command_thread;

  return true;
}

//------------------------------------------------------------------------------
bool OBS::Recording() const {
  return false;
}

//------------------------------------------------------------------------------
void OBS::InitAudioSources(OBSData& audio_settings) {
  obs_audio_controller_->InitAudioSources(audio_settings,
                                         advanced_output_.get());
}
//------------------------------------------------------------------------------
bool OBS::InitVideo(OBSData& video_settings,
                    OBSData& extra_video_settings,
                    OBSData& error_result) {
  if (!DoInitVideo(video_settings, extra_video_settings, error_result)) {
    return false;
  }

  // from obs default values (HDR)
  obs_set_video_levels(300, 1000);

  obs_video_info ovi;
  if (obs_get_video_info(&ovi)) {
    output_height_ = ovi.output_height;
    output_width_ = ovi.output_width;
  }

  blog(LOG_INFO, "init obs video [width:%d height:%d]",
    output_width_, output_height_);

  return true;
}

//------------------------------------------------------------------------------
bool OBS::DoInitVideo(OBSData& video_settings, 
                      OBSData& extra_video_settings,
                      OBSData& error_result) {
  if (video_settings == nullptr) {
    video_settings = obs_data_create();
    obs_data_release(video_settings);
  }

  settings::SetDefaultVideo(video_settings);

  if (advanced_output_.get() && advanced_output_->Active()) {
    blog(LOG_ERROR, kErrorChangedSettingsWhileActive);
    obs_data_set_int(error_result,
                     protocol::kErrorCodeField,
                     protocol::events::INIT_ERROR_CURRENTLY_ACTIVE);
    return false;
  }

  if (!display_tester_.get()) {
    display_tester_.reset(new OBSDisplayTester(this));
  }

  struct obs_video_info ovi;
  ovi.adapter = 0;
  ovi.graphics_module = DL_D3D11;

  ovi.output_format = GetVideoColorFormatFromName(
      obs_data_get_string(extra_video_settings, kColorFormatKey));

  ovi.scale_type = OBS_SCALE_BICUBIC;
  ovi.colorspace = GetVideoColorSpaceFromName(
      obs_data_get_string(extra_video_settings, kColorSpaceKey));

  ovi.range = VIDEO_RANGE_PARTIAL;
  ovi.gpu_conversion = true;
  ovi.fps_den = 1;

  // Configurable
  ovi.fps_num = (uint32_t)obs_data_get_int(
    video_settings,
    settings::kSettingsVideoFPS);
  ovi.base_width = (uint32_t)obs_data_get_int(
    video_settings,
    settings::kSettingsVideoBaseWidth);
  ovi.base_height = (uint32_t)obs_data_get_int(
    video_settings,
    settings::kSettingsVideoBaseHeight);
  ovi.output_width = (uint32_t)obs_data_get_int(
    video_settings,
    settings::kSettingsVideoOutputWidth);
  ovi.output_height = (uint32_t)obs_data_get_int(
    video_settings,
    settings::kSettingsVideoOutputHeight);

  blog(LOG_INFO, "---------------------------------");
  blog(LOG_INFO,
    "ascent-obs video settings reset:\n"
    "\tbase resolution:   %dx%d\n"
    "\toutput resolution: %dx%d\n"
    "\tfps:               %d\n",
    ovi.base_width, ovi.base_height,
    ovi.output_width, ovi.output_height,
    ovi.fps_num);

  if (ovi.base_width == 0 ||
    ovi.base_height == 0) {
    ovi.base_width = 1920;
    ovi.base_height = 1080;
  }

  if (ovi.output_width == 0 ||
    ovi.output_height == 0) {
    ovi.output_width = ovi.base_width;
    ovi.output_height = ovi.base_height;
  }

  compatibility_mode_ = obs_data_get_bool(video_settings,
    settings::kSettingsVideoCompatibilityMode);

  capture_game_cursor_ = obs_data_get_bool(video_settings,
    settings::kSettingsGameCursor);

  // performance stats timer
  if (!stats_time_.get()) {
    stats_time_.reset(new libascentobs::TimerQueueTimer(this));
    stats_time_->Start(1000);
  }

  auto res = obs_reset_video(&ovi);
  if (res == 0) {
    return true;
  }

  while (res == OBS_VIDEO_CURRENTLY_ACTIVE) {
    blog(LOG_INFO, "Reset obs setting: OBS still active, wait...");
    Sleep(1000);
    res = obs_reset_video(&ovi);
    if (res == 0) {
      return true;
    }
  }

  /* Try OpenGL if DirectX fails on windows */
  ovi.graphics_module = DL_OPENGL;
  if (obs_reset_video(&ovi) == 0) {
    return true;
  }

  blog(LOG_ERROR, "unexpected error - failed to init video settings");

  obs_data_set_int(error_result,
    protocol::kErrorCodeField,
    protocol::events::INIT_ERROR_FAILED_TO_INIT);
  return false;
}

//------------------------------------------------------------------------------
void OBS::OnOutputStopped() {
  if (advanced_output_->Active())
    return;

  if (!shoutdown_on_stop_)
    return;

  // comment out until we remove the "gif" service
  /*blog(LOG_INFO, "no active output.., shutting down");

  if (communications_) {
    communications_->Shutdown();
  }*/
}

//------------------------------------------------------------------------------
void OBS::RegisterDisplay() {
  /*if (!display_context_.get()) {
    display_context_.reset(new DisplayContext(CreateDisplay()));
    }
    obs_display_add_draw_callback(display_context_->get(), RenderWindow, this);*/
}

//------------------------------------------------------------------------------
bool OBS::InitVideoEncoder(OBSData& video_encoder_settings,
                           OBSData& video_extra_options,
                           OBSData& error_result,
                           const char* type) {
  if (advanced_output_.get() && advanced_output_->Active()) {
    blog(LOG_ERROR, kErrorChangedSettingsWhileActive);
    obs_data_set_int(error_result,
                     protocol::kErrorCodeField,
                     protocol::events::INIT_ERROR_CURRENTLY_ACTIVE);
    return false;
  }

  if (video_encoder_settings == nullptr) {
    video_encoder_settings = obs_data_create();
    obs_data_release(video_encoder_settings);
  }

  // Init the video encoder with the encoder type
  if (type) {
    obs_data_set_string(video_encoder_settings, "id", type);
  }

    // video_settings.video_encoder.encoder_custom_parameters
  SET_OBS_DATA(encoder_custom_parameters,
    obs_data_get_obj(video_extra_options, kEncoderCustomParameters));

  settings::SetDefaultVideoEncoder(video_encoder_settings);
  settings::SetCustomEncoderParameters(video_encoder_settings,
                                       encoder_custom_parameters);

  SET_OBS_DATA(video_custom_parameters,
               obs_data_get_obj(video_extra_options, kCustomParameters));

  ApplyCustomParamters(video_custom_parameters);

  int keyframe_sec= 0; // auto
  bool enable_fragmented_file =
      obs_data_has_user_value(video_extra_options, "fragmented_video_file") &&
      obs_data_get_bool(video_extra_options, "fragmented_video_file");

  // create new for each encoder type tested (once if not testing)
  if (advanced_output_.get() != nullptr && type == nullptr) {
    advanced_output_->set_fragmented_file(enable_fragmented_file);
    return true;
  }

  advanced_output_.reset(AdvancedOutput::Create(this,
    video_encoder_settings,
    error_result));

  if (!advanced_output_) {
    return false;
  }

  advanced_output_->set_supported_tracks(obs_audio_controller_->active_tracks());
  advanced_output_->set_fragmented_file(enable_fragmented_file);
  
  return true;
}

bool OBS::InitScene(OBSData& scene_settings, OBSData& error_result) {
  libascentobs::CriticalSectionLock locker(sync_);

  if (!scene_.get()) {
    scene_.reset(new SceneContext(obs_scene_create("ascent obs scene")));

    if (!scene_->get_scene()) {
      blog(LOG_ERROR, kErrorScenCreate);
      obs_data_set_int(error_result,
                       protocol::kErrorCodeField,
                       protocol::events::INIT_ERROR_FAILED_TO_CREATE_SCENE);
      return false;
    }
  }

  obs_audio_controller_->InitScene(scene_->get_scene(), scene_settings);

  bool game_in_foreground = false;
  bool capture_monitor = false;
  bool capture_window = false;
  bool game_capture = false;
  bool generic_capture = false;

  OBSDataArray generic_sources_ow = obs_data_get_array(scene_settings,
    settings::kSettingsSourceAux);

  size_t generic_sources_size = 
    generic_sources_ow ? obs_data_array_count(generic_sources_ow) : 0;

  // web secondary process
  bool has_generic_capture = generic_sources_size > 0;

  keep_recording_on_lost_focus_ |=
    obs_data_get_bool(scene_settings, settings::kKeepRecordingOnLostForeground);

  if (keep_recording_on_lost_focus_) {
    blog(LOG_INFO, "keep recording on game lost focus");
  }

  // Create a window source
  SET_OBS_DATA(window_source, obs_data_get_obj(scene_settings,
                                      settings::kSettingsSourceWindowCapture));
  capture_window = InitWindowSource(window_source, error_result);

  
  SET_OBS_DATA(monitor_source, obs_data_get_obj(
    scene_settings,
    settings::kSettingsSourceMonitor));
  capture_monitor =
    InitMonitorSource(monitor_source, error_result);

  SET_OBS_DATA(game_source, obs_data_get_obj(
    scene_settings,
    settings::kSettingsSourceGame));

  if (!capture_monitor || !monitor_source_->force()) {
    game_capture = InitGameSource(game_source,error_result,
                                  game_in_foreground, capture_window);
  }

  if (!capture_window && !capture_monitor && !game_capture && !has_generic_capture) {
    obs_data_set_int(error_result,
                     protocol::kErrorCodeField,
      protocol::events::INIT_ERROR_FAILED_TO_CREATE_SOURCES);
    blog(LOG_ERROR, "no active capture source!");
    return false;
  }

  // create BRB only when monitor capture is off
  SET_OBS_DATA(brb_source, obs_data_get_obj(
    scene_settings,
    settings::kSettingsSourceBRB));

  if (!capture_monitor && game_capture) {
    InitBRBSource(brb_source, error_result);
  }

  SET_OBS_DATA(tobii_source, obs_data_get_obj(
    scene_settings,
    settings::kSettingsSourceTobii));

  if (tobii_source) {
    if (!game_source_.get() ||
      (game_source_.get() && game_source_->did_start_capture())) {
      InitTobiiGazeSource(tobii_source);
    } else {
      // create tobii only after game started: workaround
      blog(LOG_INFO, "waiting for game before init tobii");
      pending_tobii_ = tobii_source;
      obs_data_release(pending_tobii_);
    }
  }

  /////////////////////////////////////////////////////////////////////////////
  CreateGenericSourcesFromCustomParam(generic_sources_ow);
 
  // (259) allow adding custom sources directly from the JS
  if (custom_source_setting_) {
    CreateGenericSourcesFromCustomParam(custom_source_setting_);
    custom_source_setting_ = nullptr;
  }
  

  //obs_data_array_release(list);

  /////////////////////////////////////////////////////////////////////////////
  // do not update visible if already running
  if (!IsActive()) {
    UpdateSourcesVisiblity(game_in_foreground, false);
    obs_set_output_source(0, obs_scene_get_source(scene_->get_scene()));
  }

  return true;
}

//------------------------------------------------------------------------------
void OBS::CreateGenericSourcesFromCustomParam(obs_data_array_t* sources) {
  if (!sources) {
    return;
  }

  size_t generic_sources_size = obs_data_array_count(sources);         
  if (generic_sources_size == 0) {
    return;
  }

  for (int i = 0; i < generic_sources_size; i++) {
    OBSData generic_source_handle = obs_data_array_item(sources, i);

    if (generic_source_handle) {
      std::unique_ptr<Source> generic_obs_new_source(
          GenericObsSource::CreateOBSSource(this, generic_source_handle,
                                            scene_->get_scene(), true));
      if (!generic_obs_new_source.get()) {
        continue;
      }

      generic_obs_source_.push_back(std::move(generic_obs_new_source));
    }
  }
}

//------------------------------------------------------------------------------
bool OBS::AddGameSource(OBSData& game_settings) {
  libascentobs::CriticalSectionLock locker(sync_);
  if (monitor_source_.get() && monitor_source_->force()) {
    blog(LOG_INFO, "game source rejected: capture monitor only");
    return false;
  }

  game_source_capture_failure_ = false;
  int new_game_process_id = GameCaptureSource::GetGameSourceId(game_settings);
  blog(LOG_INFO, "Updating game source: %d", new_game_process_id);
  if (game_source_.get() != nullptr &&
    game_source_->game_process_id() != new_game_process_id) {

    blog(LOG_INFO, "Add game source: process updated %d->%d reset existing",
      game_source_->game_process_id(), new_game_process_id);

    //TODO(remove source)
    if (current_visible_source_ == game_source_.get()) {
      SetVisibleSource(nullptr);
    }
    RemoveGameSource();
    game_source_.reset(nullptr);
  }

  CREATE_OBS_DATA(error_result);
  bool game_in_foreground = false;
  if (!InitGameSource(game_settings, error_result, game_in_foreground, has_window_source())) {
    blog(LOG_ERROR, "add game source error");
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------
bool OBS::LoadModules() {
  obs_load_all_modules();
  //obs_register_output(&offscreen_output_info);
  //obs_log_loaded_modules();
  obs_post_load_modules();
  return true;
}

bool OBS::gs_enum_adapters_callback(void *param, const char *name, uint32_t id) {
  OBS* _this = static_cast<OBS*>(param);
  if (id > 0 || _this == nullptr || !_this->adapter_name_.empty()) {
    return false;
  }

  _this->adapter_name_ = name;
  //_this->adapter_name_ = "NVIDIA GeForce 930MX";
  return true;
}

//------------------------------------------------------------------------------
bool OBS::IsWinrtCaptureSupported() {
  
  struct win_version_info ver;
  struct win_version_info win1903 = { 10,  0, 18362, 0};
  get_win_ver(&ver);
  bool wgc_supported = win_version_compare(&ver, &win1903) >= 0;

  blog(LOG_INFO, wgc_supported ? "wgc supported" : "wgc not supported");

  return wgc_supported;
}

//------------------------------------------------------------------------------
void OBS::RetreiveSupportedVideoEncoders(OBSDataArray& encoders) {
  CLEAR_OBS_DATA_ARRAY(encoders);

  obs_get_enum_video_adapters(gs_enum_adapters_callback, this);

  bool is_nvidia_device =
    adapter_name_.find("NVIDIA") != std::string::npos;

  const char    *type;
  size_t        idx = 0;

  while (obs_enum_encoder_types(idx++, &type)) {
    const char *name = obs_encoder_get_display_name(type);
    const char *codec = obs_get_encoder_codec(type);
    uint32_t caps = obs_get_encoder_caps(type);

    if (obs_get_encoder_type(type) != OBS_ENCODER_VIDEO) {
      continue;
    }

    const char* streaming_codecs[] = {
      "h264",
      "hevc",  
      "av1"
    };

    bool is_streaming_codec = false;
    for (const char* test_codec : streaming_codecs) {
      if (strcmp(codec, test_codec) == 0) {
        is_streaming_codec = true;
        break;
      }
    }

    if ((caps & OBS_ENCODER_CAP_DEPRECATED) != 0) {
      continue;
    }

    if ((caps & OBS_ENCODER_CAP_INTERNAL) != 0) {
      continue;
    }

    if (!is_streaming_codec) {
      continue;
    }
   
    if (is_nvidia_device &&
      isBlackInBlackList(adapter_name_.c_str(), type)) {
      blog(LOG_WARNING, "Encoder %s not supported due to being blacklisted!", type);
      continue;
    }

    // Check if the encoder is Valid
    std::string status = "";
    std::string code = "";
    bool is_encoder_valid = IsEncoderValidSafe(type, status, code, codec);

    CREATE_OBS_DATA(item);
    obs_data_set_string(item, "type", type);
    obs_data_set_string(item, "description", name);
    obs_data_set_string(item, "status", status.c_str());
    obs_data_set_bool(item, "valid", is_encoder_valid);
    obs_data_set_string(item, "code", code.c_str());
    blog(LOG_INFO, "Add supported encoder: %s", name);
    obs_data_array_push_back(encoders, item);
  }
}

bool OBS::IsEncoderValidSafe(const char* type,
                             std::string& status,
                             std::string& code,
                             const char* codec) {  
  __try {
    return IsEncoderValid(type, status, code, codec);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
  blog(LOG_ERROR, "IsEncoderValid (%s) failed: crashed", type);
  code = "unknown";
  status = "crash";
  return false;
}


//------------------------------------------------------------------------------
bool OBS::IsEncoderValid(
  const char* type, std::string& status, std::string& code, const char* codec) {
  blog(LOG_INFO, "testing IsEncoderValid (%s)", type);

  if (strcmp(codec, "av1") == 0) {
    return true;
  }

  OBSData video_encoder_settings = obs_data_create();
  obs_data_release(video_encoder_settings);
  obs_data_set_string(video_encoder_settings, "id", type);

  obs_encoder_t* video_encoder = obs_video_encoder_create(
    type, "recording_h264", video_encoder_settings, nullptr);
  obs_encoder_set_video(video_encoder, obs_get_video());

  bool is_valid = is_encoder_valid(video_encoder);
  // Output the last error to log in case of not valid encoder
  if (!is_valid) {
    const char* error = obs_encoder_get_last_error(video_encoder);
    status = error ? error : "unknown";
    const char* last_code = obs_encoder_get_last_code(video_encoder);
    code = last_code ? last_code : "unknown";
    blog(LOG_ERROR, "IsEncoderValid (%s) failed: %s", type, error);
  } else {
    status = "OK";
    blog(LOG_INFO, "IsEncoderValid (%s) ended successfully", type);
  }
  return is_valid;
}

//------------------------------------------------------------------------------
void OBS::RetreiveAudioDevices(const char* source_id, OBSDataArray& devices) {
  CLEAR_OBS_DATA_ARRAY(devices);

  obs_properties_t* output_props = obs_get_source_properties(source_id);
  if (nullptr == output_props) {
    return;
  }

  obs_property_t *outputs = obs_properties_get(output_props, "device_id");
  size_t devices_count = obs_property_list_item_count(outputs);
  for (size_t i = 0; i < devices_count; ++i) {
    const char* name = obs_property_list_item_name(outputs, i);
    const char* val = obs_property_list_item_string(outputs, i);

    CREATE_OBS_DATA(item);
    obs_data_set_string(item, name, val);
    obs_data_array_push_back(devices, item);
  }

  obs_properties_destroy(output_props);
}

//------------------------------------------------------------------------------
bool OBS::HasAudioDevices(const char* source_id) {
  const char *output_id = source_id;
  obs_properties_t *props = obs_get_source_properties(output_id);
  size_t count = 0;

  if (!props) {
    return false;
  }

  obs_property_t *devices = obs_properties_get(props, "device_id");
  if (devices) {
    count = obs_property_list_item_count(devices);
  }

  obs_properties_destroy(props);

  return count != 0;
}

//------------------------------------------------------------------------------
bool OBS::ResetOutputSetting(
  OBSData& output_settings, OBSData& audio_setting, OBSData& error_result) {
  if (!advanced_output_) {
    return false;
  }

  return advanced_output_->ResetOutputSetting(
    output_settings, audio_setting, error_result);
}

//------------------------------------------------------------------------------
bool OBS::StartRecording(int identifier, OBSData& error_result) {
  if (!advanced_output_) {
    return false;
  }

  return advanced_output_->StartRecording(identifier, error_result);
}

bool OBS::StartDelayRecording(int identifier) {
  if (!advanced_output_) {
    return false;
  }

  advanced_output_->StartDelayRecording(identifier);
  return true;
}

//------------------------------------------------------------------------------
bool OBS::StartReplay(int identifier,
                      OBSData& settings,
                      OBSData& replay_settings,
                      OBSData& error_result) {
  if (!advanced_output_) {
    return false;
  }
  bool force = !generic_obs_source_.empty() && !game_source_.get();
  force |= (!game_source_.get() && monitor_source_.get());
  force |= (window_source_.get() != nullptr);
  return advanced_output_->StartReplay(identifier,
    settings,
    replay_settings,
    error_result,
    force);
}

//------------------------------------------------------------------------------
bool OBS::StartStreaming(int identifier,
                         OBSData& stream_setting,
  OBSData& error_result) {

  if (!advanced_output_) {
    return false;
  }

  return advanced_output_->StartStreaming(identifier,
                                          stream_setting,
                                          error_result);
}

//------------------------------------------------------------------------------
bool OBS::Stop(int identifier, int recording_type, bool force) {
  if (!advanced_output_) {
    return false;
  }

  blog(LOG_INFO, "Stop stream =%d", identifier);

  bool res = false;
  if (identifier == advanced_output_->identifier()) {
    res = StopRecording(force);
  } else if (identifier == advanced_output_->replay_identifier()) {
    res = StopReplay(true);
  } else if (identifier == advanced_output_->streaming_identifier()) {
    res = StopStreaming();
  } else { //  old identifier report as stopped
    blog(LOG_WARNING, "stop none active id: %d (type: %d)",
         identifier, recording_type);
    switch (recording_type) {
      case protocol::commands::recorderType::VIDEO:
        OnStoppedRecording(identifier, 0, 0, 0);
        break;
      case protocol::commands::recorderType::REPLAY:
        OnStoppedReplay(identifier, 0, nullptr);
        break;
      case protocol::commands::recorderType::STREAMING:
        OnStoppedStreaming(identifier, 0, 0, 0);
        break;
    }
  }

  return res;
}

//------------------------------------------------------------------------------
bool OBS::StopRecording(bool force) {
  if (!advanced_output_) {
    return false;
  }

  blog(LOG_INFO, "Call stopping recoding stream");
  // sync with UpdateSourcesVisiblity
  libascentobs::CriticalSectionLock locker(sync_);
  advanced_output_->StopRecording(force);
  blog(LOG_INFO, "Stopping recoding stream");
  return true;
}

//------------------------------------------------------------------------------
bool OBS::StopReplay(bool force) {
  if (!advanced_output_) {
    return false;
  }

  blog(LOG_INFO, "Call stop replay stream");
  // sync with UpdateSourcesVisiblity
  libascentobs::CriticalSectionLock locker(sync_);

  advanced_output_->StopReplay(force);

  // in case game source not attached, ( attempt_existing_hook: failure)
  // we need to release in manually
  if (!advanced_output_->RecorderActive() &&
    !advanced_output_->StreamActive()) {
    blog(LOG_INFO, "reset Game source");
    //game_source_.reset(nullptr);
  }

  blog(LOG_INFO, "Stop replay stream");
  return true;
}

//------------------------------------------------------------------------------
bool OBS::StopStreaming(bool force) {
  if (!advanced_output_) {
    return false;
  }

  // sync with UpdateSourcesVisiblity
  libascentobs::CriticalSectionLock locker(sync_);

  blog(LOG_INFO, "Call stop streaming");
  advanced_output_->StopStreaming(force);
  blog(LOG_INFO, "Stop streaming");

  return true;
}

//------------------------------------------------------------------------------
void OBS::SplitVideo() {
  if (!advanced_output_) {
    return;
  }

  // sync with UpdateSourcesVisiblity
  libascentobs::CriticalSectionLock locker(sync_);
  advanced_output_->SplitVideo();
}

//------------------------------------------------------------------------------
bool OBS::StartCaptureReplay(OBSData& data, OBSData& error_result) {
  if (!advanced_output_) {
    return false;
  }

  // sync with UpdateSourcesVisiblity
  libascentobs::CriticalSectionLock locker(sync_);
  return advanced_output_->StartCaptureReplay(data, error_result);
}

//------------------------------------------------------------------------------
bool OBS::StopCaptureReplay(OBSData& data, OBSData& error_result) {
  if (!advanced_output_) {
    return false;
  }

  // sync with UpdateSourcesVisiblity
  libascentobs::CriticalSectionLock locker(sync_);
  return advanced_output_->StopCaptureReplay(data, error_result);
}

//------------------------------------------------------------------------------
bool OBS::UpdateTobiiGazaSource(OBSData& data) {
  SET_OBS_DATA(tobii_source, obs_data_get_obj(
    data,
    settings::kSettingsSourceTobii));

  if (!InitTobiiGazeSource(tobii_source)) {
    blog(LOG_ERROR, "Fail to update Tobii gaze source");
    return false;
  }

  blog(LOG_INFO, "update Tobii Gaze source visibility: %d", tobii_source_->IsVisible());
  return true;
}

//------------------------------------------------------------------------------
bool OBS::UpdateBRB(OBSData& data) {
  CREATE_OBS_DATA(error_result);
  if (!brb_source_.get()) {
    if (!InitBRBSource(data, error_result)) {
      blog(LOG_ERROR, "UpdateBRB: Failed to crate BRB source");
      return false;
    }
  }

  brb_source_->Update(data);
  return true;
}

//------------------------------------------------------------------------------
//virtual
void OBS::OnStartedRecording(int identifier) {
  split_video_counter_ = 0;
  CREATE_OBS_DATA(data);
  if (!SetVisibleSourceName(data)) {
    blog(LOG_WARNING, "started recording but without a visible source?!");
  }

  obs_data_set_int(data,  protocol::kCommandIdentifier, identifier);
  obs_data_set_bool(data, protocol::KIsWindowCapture, has_window_source());
  communications_->Send(protocol::events::RECORDING_STARTED, data);
}

//------------------------------------------------------------------------------
// virtual
void OBS::OnStoppingRecording(int identifier) {
  CREATE_OBS_DATA(data);
  obs_data_set_int(data, protocol::kCommandIdentifier, identifier);
  communications_->Send(protocol::events::RECORDING_STOPPING, data);
}

//------------------------------------------------------------------------------
// virtual
void OBS::OnStoppedRecording(int identifier,
  int code,
  const char* last_error,
  int64_t duration_ms,
  obs_data_t* stats_data /*= nullptr*/) {

  CREATE_OBS_DATA(data);
  obs_data_set_int(data, "code", code);
  obs_data_set_int(data, "duration", duration_ms);
  obs_data_set_string(data, "last_error", last_error);
  obs_data_set_int(data, "output_width", output_width_);
  obs_data_set_int(data, "output_height", output_height_);
  obs_data_set_int(data, protocol::kCommandIdentifier, identifier);

  if (stats_data) {
    obs_data_set_obj(data, protocol::kStatsDataField, stats_data);
  }

  communications_->Send(protocol::events::RECORDING_STOPPED, data);
  split_video_counter_ = 0;

  OnOutputStopped();
}

//------------------------------------------------------------------------------
// virtual
void OBS::OnVideoSplit(int identifier,
  std::string path,
                       int64_t duration,
  int64_t split_file_duration,
  int64_t last_frame_pts,
  std::string next_video_path) {
  blog(LOG_INFO, "new split video created: %s duration: %u",
    path.c_str(), duration);

  CREATE_OBS_DATA(data);
  obs_data_set_int(data, "duration", duration);
  obs_data_set_int(data, "split_file_duration", split_file_duration);
  obs_data_set_int(data, "frame_pts", last_frame_pts);
  obs_data_set_int(data, "count", (++split_video_counter_));
  obs_data_set_string(data, "path", path.c_str());
  obs_data_set_string(data, "next_video_path", next_video_path.c_str());
  obs_data_set_int(data, "output_width", output_width_);
  obs_data_set_int(data, "output_height", output_height_);

  obs_data_set_int(data, protocol::kCommandIdentifier, identifier);
  communications_->Send(protocol::events::VIDEO_FILE_SPLIT, data);
}

//------------------------------------------------------------------------------
// virtual
void OBS::OnStartedReplay(int identifier) {
  blog(LOG_INFO, "report replay started :%d", identifier);
  CREATE_OBS_DATA(data);
  obs_data_set_int(data, protocol::kCommandIdentifier, identifier);
  obs_data_set_bool(data, protocol::KIsWindowCapture, has_window_source());
  SetVisibleSourceName(data);
  communications_->Send(protocol::events::REPLAY_STARTED, data);

  // already recording
  if (advanced_output_->ReplayActive() &&
    !advanced_output_->DelayReplayActive()) {
    NotifyGameSourceChangedSafe();
  }
}

//------------------------------------------------------------------------------
// virtual
void OBS::OnStoppingReplay(int identifier) {
  CREATE_OBS_DATA(data);
  obs_data_set_int(data, protocol::kCommandIdentifier, identifier);
  communications_->Send(protocol::events::REPLAY_STOPPING, data);

}

//------------------------------------------------------------------------------
// virtual
void OBS::OnStoppedReplay(int identifier,
  int code, const char* last_error, obs_data_t* stats_data /*= nullptr*/) {


  CREATE_OBS_DATA(data);
  obs_data_set_int(data, protocol::kCommandIdentifier, identifier);
  obs_data_set_int(data, "code", code);
  obs_data_set_string(data, "last_error", last_error);
  if (stats_data) {
    obs_data_set_obj(data, protocol::kStatsDataField, stats_data);
  }
  communications_->Send(protocol::events::REPLAY_STOPPED, data);

  OnOutputStopped();
}

//------------------------------------------------------------------------------
// virtual
void OBS::OnReplayVideoReady(int identifier,
                             std::string path,
                             int64_t duration,
                             int64_t video_start_time,
                             std::string thumbnail_folder,
                             bool stop_stream) {

  CREATE_OBS_DATA(data);
  obs_data_set_int(data, "duration", duration);
  obs_data_set_int(data, "video_start_time", video_start_time);
  obs_data_set_string(data, "path", path.c_str());
  obs_data_set_string(data, "thumbnail_folder", thumbnail_folder.c_str());
  obs_data_set_int(data, "output_width", output_width_);
  obs_data_set_int(data, "output_height", output_height_);
  obs_data_set_int(data, protocol::kCommandIdentifier, identifier);
  obs_data_set_bool(data, "disconnection", stop_stream);
  //obs_data_set_bool(data, "expanded_canvas", expandCanvas_);
  communications_->Send(protocol::events::REPLAY_CAPTURE_VIDEO_READY,
    data);

  if (command_thread_ == nullptr || !stop_stream)
    return;

  blog(LOG_WARNING, "replay is ready -> stop replay output!!");
  command_thread_->PostTask(std::bind(&OBS::OnGameQuite, this, true));
}

//------------------------------------------------------------------------------
// virtual
void OBS::OnReplayVideoError(int identifier,
  std::string path,
  std::string error) {
  CREATE_OBS_DATA(data);
  obs_data_set_int(data,
    protocol::kErrorCodeField,
    protocol::events::REPLAY_ERROR_REPLAY_OBS_ERROR);

  obs_data_set_string(data,
    protocol::kErrorDescField,
    error.c_str());

  obs_data_set_string(data, "path", path.c_str());;

  obs_data_set_int(data, protocol::kCommandIdentifier, identifier);
  communications_->Send(protocol::events::REPLAY_ERROR, data);
}

//------------------------------------------------------------------------------
// virtual
void OBS::OnReplayArmed(int identifier) {
  CREATE_OBS_DATA(data);
  obs_data_set_int(data, protocol::kCommandIdentifier, identifier);
  communications_->Send(protocol::events::REPLAY_ARMED, data);
}

//------------------------------------------------------------------------------
// virtual
void OBS::OnStartingStreaming(int identifier) {
  CREATE_OBS_DATA(data);
  obs_data_set_int(data, protocol::kCommandIdentifier, identifier);
  communications_->Send(protocol::events::STREAMING_STARTING, data);
}

//------------------------------------------------------------------------------
// virtual
void OBS::OnStartedStreaming(int identifier) {
  CREATE_OBS_DATA(data);
  obs_data_set_int(data, protocol::kCommandIdentifier, identifier);
  SetVisibleSourceName(data);
  communications_->Send(protocol::events::STREAMING_STARTED, data);
}

//------------------------------------------------------------------------------
// virtual
void OBS::OnStoppingStreaming(int identifier) {
  CREATE_OBS_DATA(data);
  obs_data_set_int(data, protocol::kCommandIdentifier, identifier);
  communications_->Send(protocol::events::STREAMING_STOPPING, data);
}

//------------------------------------------------------------------------------
// virtual
void OBS::OnStoppedStreaming(int identifier,
  int code, const char* last_error, obs_data_t* stats_data /*= nullptr*/) {

  CREATE_OBS_DATA(data);
  obs_data_set_int(data, protocol::kCommandIdentifier, identifier);
  obs_data_set_int(data, "code", code);
  obs_data_set_string(data, "last_error", last_error);
  if (stats_data) {
    obs_data_set_obj(data, protocol::kStatsDataField, stats_data);
  }

  communications_->Send(protocol::events::STREAMING_STOPPED, data);

  OnOutputStopped();
}

//------------------------------------------------------------------------------
// virtual
void OBS::OnCaptureWarning(int identifier, const char* message, obs_data_t* extra) {
  CREATE_OBS_DATA(data);
  obs_data_set_int(data, protocol::kCommandIdentifier, identifier);
  obs_data_set_string(data, "message", message);
  if (extra) {
    obs_data_set_obj(data, "extra", extra);
  }

  communications_->Send(protocol::events::OBS_WARNING, data);
}


void OBS::OnTimer(TimerQueueTimer* timer) {
  if (timer == stats_time_.get()) {
    OnStatTimer();
    return;
  }

  if (timer == stop_replay_timer_.get()) {
    OnStopReplayTimer();
    return;
  }
}

bool OBS::is_replay_capture_in_progress() {
  if (!advanced_output_.get()) {
    return false;
  }

  if (!advanced_output_->replay_output_.get())
    return false;

  return advanced_output_->replay_output_->capture_in_progress();
}

//------------------------------------------------------------------------------
// virtual TimerQueueTimerDelegate (stats_time_ - drop\ skip frames indication)
void OBS::OnStatTimer() {
  if (!advanced_output_.get()) {
    return;
  }

  if (!advanced_output_->Active()) {
    return;
  }

  advanced_output_->TestStats();
}

//------------------------------------------------------------------------------
void OBS::OnStopReplayTimer() {
  libascentobs::CriticalSectionLock locker(sync_);
  blog(LOG_WARNING, "Stop replay timeout");
  StopReplay(true);

  if (stop_replay_timer_.get()) {
    stop_replay_timer_->Stop();
  }
}

//------------------------------------------------------------------------------
bool OBS::InitWindowSource(OBSData& window_setting, OBSData& error_result) {
  if (window_setting == nullptr) {
    return false;
  }

  if (window_source_.get()) {
    blog(LOG_WARNING, "init window source: window source already created");
    return true;
  }

  if (!WindowSource::IsEnabled(window_setting)) {
    return false;
  }

  std::unique_ptr<WindowSource> window_source(new WindowSource(this));
  bool result = 
    window_source->Create(window_setting, scene_->get_scene(), false);

  if (result) {
    window_source_ = std::move(window_source);
  } 

  UNUSED_PARAMETER(error_result);

  return result;
}

//------------------------------------------------------------------------------
bool OBS::InitMonitorSource(OBSData& monitor_setting, OBSData& error_result) {
  if (monitor_setting == nullptr) {
    return false;
  }

  if (!MonitorSource::IsEnabled(monitor_setting)) {
    return false;
  }

  if (monitor_source_.get()) {
    blog(LOG_WARNING, "init monitor source: monitor source already created");
    return true;
  }

  monitor_source_.reset(new MonitorSource(this));
  bool result =
    monitor_source_->Create(monitor_setting, scene_->get_scene(), false);

  if (!result) {
    monitor_source_.reset(nullptr);
  }

  UNUSED_PARAMETER(error_result);
  return result;
}

//------------------------------------------------------------------------------
bool OBS::InitGenericOBSSource(OBSData& generic_source_handle,
  OBSData& error_result, int index) {
  UNUSED_PARAMETER(error_result);

  if (generic_obs_source_.size() > index && generic_obs_source_[index]) {
    return true;
  }

  bool enabled = obs_data_get_bool(generic_source_handle, "enabled");
  if (!enabled) {
    return false;
  }

  static bool obs_is_secondary =
    CommandLine::ForCurrentProcess()->HasSwitch(switches::kCommandSecondary);

  bool secondary = obs_data_get_bool(generic_source_handle, "secondaryFile");
  if (obs_is_secondary != secondary) {
    blog(LOG_INFO, "skip source '%s' (%d, %d)",
      obs_data_get_string(generic_source_handle, "name"),
      obs_is_secondary, secondary);
    return false;
  }
 
  std::unique_ptr<Source> generic_obs_new_source(
     GenericObsSource::CreateOBSSource(this, generic_source_handle,
                                    scene_->get_scene(), true));
  if (!generic_obs_new_source.get()) {
    return false;
  }

  generic_obs_source_.push_back(std::move(generic_obs_new_source));
  return true;
}

//------------------------------------------------------------------------------
bool OBS::InitBRBSource(OBSData& brb_setting, OBSData& error_result) {
  UNUSED_PARAMETER(error_result);
  if (brb_source_.get()) {
    return true;
  }

  std::unique_ptr<BRBSource> brb_source(new BRBSource(this));
  bool result = brb_source->Create(brb_setting, scene_->get_scene(), false);
  if (result) {
    brb_source_ = std::move(brb_source);
  }

  return result;
}

//------------------------------------------------------------------------------
bool OBS::InitTobiiGazeSource(OBSData& gaze_setting) {
  if (!tobii_source_.get()) {
    blog(LOG_INFO, "init tobii");
    tobii_source_.reset(new GazeOverlaySource(nullptr));
    bool result =
      tobii_source_->Create(gaze_setting, scene_->get_scene());

    if (!result) {
      tobii_source_.reset(nullptr);
      return false;
    }
  } else {
    blog(LOG_INFO, "tobii already init");
  }

  bool visible = obs_data_get_bool(gaze_setting, "visible");
  tobii_source_->SetVisible(visible);
  return true;
}

//------------------------------------------------------------------------------
bool OBS::InitGameSource(OBSData& game_setting,
                         OBSData& error_result,
                         bool&    foreground,
                         bool     capture_window) {
  UNUSED_PARAMETER(error_result);
  if (game_source_.get()) {
    // this may happen when switching video configuration..
    foreground =
      obs_data_get_bool(game_setting, settings::kSettingsForeground);

    blog(LOG_WARNING,
      "init game source: Game source already created. new visibility is '%d' (current: %d)",
      foreground, game_source_->foreground());

    foreground |= game_source_->foreground();
    game_source_->SetVisible(false);
    game_source_->RefreshTransform();
    game_source_->SetVisible(foreground);
    return true;
  }

  game_source_.reset(
    new GameCaptureSource(this,
    compatibility_mode_,
    capture_game_cursor_,
    capture_window));

  bool result = game_source_->Create(game_setting, scene_->get_scene(), false);

  if (!result) {
    game_source_.reset(nullptr);
    return false;
  }

  foreground = game_source_->foreground();

  if (!game_source_->compatibility_mode() && foreground &&
    game_source_->did_start_capture()) {
    blog(LOG_INFO, "Game capture stated: starting SHT TestStarting");
    if (display_tester_.get()) {
      display_tester_->Register(OBSDisplayTester::TestSouceTypeGame);
    }
  }

  return true;
}

//------------------------------------------------------------------------------
bool OBS::UsingGameSource() {
  auto* game = game_source_.get();
  if (game == nullptr) {
    return false;
  }
  // using oopo
  if (window_source_.get()) {
    return false;
  }
  bool res = game->foreground();
  return res;
}

bool OBS::HasDelayGameSource() {
  return game_source_.get() != nullptr &&
        !game_source_->did_start_capture() &&
         !window_source_.get();
}

//------------------------------------------------------------------------------
void OBS::UpdateSourcesVisiblity(bool game_in_foreground,
                                bool is_minimized) {
  libascentobs::CriticalSectionLock locker(sync_);
  Source* new_visible_source = nullptr;

  auto save_game_in_foreground = game_in_foreground;
  game_in_foreground &= (game_source_.get() != nullptr);

  blog(LOG_INFO, "update sources visibility [%d (%d) minimized:%d]",
    game_in_foreground, save_game_in_foreground, is_minimized);

  if (!game_in_foreground && game_source_.get() && !is_minimized &&
      keep_recording_on_lost_focus_) {
    blog(LOG_INFO, "override 'game_in_foreground (keep)'");
    game_in_foreground = true;
  }


  // record from monitor is case we have monitor source
  if (has_monitor_source()) {
    monitor_source_->SetVisible(!game_in_foreground);
    new_visible_source =
      !game_in_foreground ? monitor_source_.get() : new_visible_source;
  }

  // record from a window in case we have a window source
  if (window_source_.get()) {
    window_source_->SetVisible(true);
    new_visible_source = window_source_.get();
  }

  if (game_source_.get()) {
    if (!has_monitor_source()) {
      // render last frame
      game_source_->SetVisible(true);
      game_source_->SetForegroundState(game_in_foreground);
    } else { // has monitor source
      // hide the game source and use the monitor source
      // when the game is NOT in the foreground
      game_source_->SetVisible(game_in_foreground);
      game_source_->SetForegroundState(game_in_foreground);
    }

    new_visible_source =
      game_in_foreground ? game_source_.get() : new_visible_source;

    // BRB valid  only with game source
    if (brb_source_.get()) {
      brb_source_->SetVisible(!game_in_foreground);
      new_visible_source =
        !game_in_foreground ? brb_source_.get() : new_visible_source;
    }

    if (game_source_->IsVisible() && !game_source_->compatibility_mode() &&
        game_source_->did_start_capture() && display_tester_.get()) {
      display_tester_->Register(OBSDisplayTester::TestSouceTypeGame);
    }
  }

  if (monitor_source_.get() &&
    monitor_source_->IsVisible() &&
    !monitor_source_->compatible_mode() &&
    display_tester_.get()) {

    display_tester_->Register(OBSDisplayTester::TestSouceTypeMonitor);
  }

  if (monitor_source_.get() &&
    monitor_source_->IsVisible()) {
    monitor_source_->MoveTop();
  }

  if (tobii_source_.get()) {
    tobii_source_->MoveTop();
  }
  
  for (const auto& iter : generic_obs_source_) {
    if (iter && iter->IsVisible()) {
      iter->MoveTop();
    }
  }

  SetVisibleSource(new_visible_source);
}

//------------------------------------------------------------------------------
bool OBS::IsActive() {
  libascentobs::CriticalSectionLock locker(sync_);
  if (advanced_output_ == nullptr)
    return false;

  return advanced_output_->Active();
}

//--------------------------GameCaptureSourceDelegate-------------------------
void OBS::OnGameCaptureStateChanged(bool capturing,
  bool is_process_alive,
  bool compatibility_mode,
  const char* error) {
  if (command_thread_ == nullptr)
    return;

  command_thread_->PostTask(std::bind(&OBS::HandleGameCaptureStateChanged,
    this, capturing, is_process_alive, compatibility_mode,
    std::string(error ? error :"")));
}

//------------------------------------------------------------------------------
void OBS::NotifyGameSourceChangedSafe() {
  __try {
    NotifyGameSourceChanged();
  }  __except (EXCEPTION_EXECUTE_HANDLER) {
    blog(LOG_ERROR, "**** notify game source changed exception ****");
  }
}

//------------------------------------------------------------------------------
void OBS::NotifyGameSourceChanged() {
  if (current_visible_source_ == nullptr) {
    return;
  }

  CREATE_OBS_DATA(data);
  obs_data_set_string(data, "source", current_visible_source_->name());
  communications_->Send(protocol::events::DISPLAY_SOURCE_CHANGED, data);
}

//------------------------------------------------------------------------------
void OBS::StopRecordingOnGameSourceExit() {
  blog(LOG_INFO, "stop recording: no game source");
  //this->StopReplay(true);
  this->StopRecording();
  RemoveGameSource();
}

//////////////////////////////////////////////////////////////////////////
// OBSDisplayTester::Delefate
//------------------------------------------------------------------------------
void OBS::OnBlackTextureDetected(OBSDisplayTester::TestSouceType type) {
  switch (type) {
    case obs_control::OBSDisplayTester::TestSouceTypeGame:
    {
      if (game_source_.get()) {
        game_source_->SwitchToCompatibilityMode();

      }
      NotifyPossibleSwitchableDevices();

    } break;
    case obs_control::OBSDisplayTester::TestSouceTypeMonitor:
    {
      if (monitor_source_.get() && monitor_source_->IsVisible()) {
        blog(LOG_WARNING, "black texture detected [switchable devices?] :replace monitor capture");
        if (monitor_source_.get() == current_visible_source_) {
          SetVisibleSource(nullptr);
        }

        monitor_source_->SetVisible(false);
        auto monitor_id = monitor_source_->monitor_id();
        auto monitor_handle = monitor_source_->monitor_handle();
        auto force = monitor_source_->force();

        monitor_source_.reset(new MonitorSource(this));

        monitor_source_->CreateCompatibility(
          monitor_id,
          monitor_handle,
          scene_->get_scene(),
          force);

        monitor_source_->SetVisible(true);
        return;
      }
    } break;

    default:
    break;
  }

  if (command_thread_) {
    command_thread_->PostTask(std::bind(&OBS::StopDisplayTest, this));
  }
}

//------------------------------------------------------------------------------
void OBS::OnColoredTextedDetected(OBSDisplayTester::TestSouceType source_type) {

  switch (source_type) {
    case obs_control::OBSDisplayTester::TestSouceTypeGame:
    blog(LOG_INFO, "Game switchable devices black texture is ok");
    break;
    case obs_control::OBSDisplayTester::TestSouceTypeMonitor:
    blog(LOG_INFO, "Monitor Switchable devices black texture is ok");
    break;
    default:
    break;
  }

  if (command_thread_) {
    command_thread_->PostTask(std::bind(&OBS::StopDisplayTest, this));
  }
}

Source* OBS::GetSource(OBSDisplayTester::TestSouceType source_type) {
  switch (source_type) {
    case obs_control::OBSDisplayTester::TestSouceTypeGame:
    return game_source_.get();
    case obs_control::OBSDisplayTester::TestSouceTypeMonitor:
    return monitor_source_.get();
    default:
    break;
  }

  return nullptr;
}

//------------------------------------------------------------------------------
void OBS::StopDisplayTest() {
  if (!display_tester_.get()) {
    return;
  }

  display_tester_->Unregister();
}

//------------------------------------------------------------------------------
void OBS::NotifyPossibleSwitchableDevices() {
  if (did_notify_switchable_devices_)
    return;

  blog(LOG_WARNING, "!!!! Notify switchable device detected (shared memory capture) !!!!!");
  did_notify_switchable_devices_ = true;

  if (!communications_) {
    return;
  }

  CREATE_OBS_DATA(data);
  communications_->Send(protocol::events::SWITCHABLE_DEVICE_DETECTED, data);
}


void OBS::HandleGameCaptureStateChanged(bool capturing,
  bool is_process_alive,
  bool compatibility_mode,
  std::string error) {
  libascentobs::CriticalSectionLock locker(sync_);
  blog(LOG_INFO, "Game capture state changed [capture:%d process alive:%d]",
    capturing, is_process_alive);

  CREATE_OBS_DATA(error_result);
  // start recording if needed (
  if (advanced_output_) {
    if (capturing) {
      if (compatibility_mode) {
        // obs auto switch to shared memory
        StopDisplayTest();
        if (game_source_.get() && !game_source_->compatibility_mode()) {
          NotifyPossibleSwitchableDevices();
        }
      }

      StartPendingDelayRecording();

      // has pending replay
      if (advanced_output_->DelayReplayActive()) {
        int identifier = advanced_output_->replay_identifier();
        blog(LOG_INFO, "game capture started, start replay...  %d", identifier);
        if (!advanced_output_->StartReplay(error_result)) {
          obs_data_set_int(error_result, protocol::kCommandIdentifier, identifier);
          blog(LOG_ERROR, "Failed start replays %d", identifier);
          //MessageBoxA(0,"ERROR replay", "Failed start replays", 0 );
          communications_->Send(protocol::events::ERR, error_result);
          //return;
        }
      }

      // in case game started not in foreground
      if (game_source_.get() && !game_source_->foreground()) {
        game_source_->SetForegroundState(game_source_->foreground());
      }

      if (pending_tobii_) {
        //MessageBox(NULL, L"Tobii", L"Tobii", 0);
        blog(LOG_INFO, "init tobii after game started");
        InitTobiiGazeSource(pending_tobii_);
        obs_data_release(pending_tobii_);
        pending_tobii_ = nullptr;
      }

      NotifyGameSourceChangedSafe();
      UpdateSourcesVisiblity(true, false);
      return;
    } else if (!capturing && !error.empty() && advanced_output_->DelayActive()) {
      int identifier = advanced_output_->identifier();
      if (identifier == -1) {
        identifier = advanced_output_->replay_identifier();
      }
      obs_data_set_int(error_result, protocol::kCommandIdentifier, identifier);
      blog(LOG_ERROR, "Failed start replays %d (%s)", identifier, error.c_str());
      obs_data_set_int(error_result,
        protocol::kErrorCodeField,
        protocol::events::INIT_ERROR_GAME_INJECTION_ERROR);
      obs_data_set_string(error_result,
        protocol::kErrorDescField,
        error.c_str());
      //MessageBoxA(0,"ERROR replay", "Failed start replays", 0 );
      communications_->Send(protocol::events::ERR, error_result);
    }
  }

  if (is_process_alive) {
    if (has_monitor_source() && !game_source_->foreground()) {
      UpdateSourcesVisiblity(false, true);
    }

    return;
  }

  if (display_tester_.get()) {
    display_tester_->ResetTest(OBSDisplayTester::TestSouceTypeGame);
  }

  did_notify_switchable_devices_ = false;

  OnGameQuite(!is_replay_capture_in_progress());

  // exit game (!is_process_alive)
  // Don't stop recording if we are recording the game window (OOPO)
  if (!has_monitor_source() && !has_window_source() &&
      !disable_shutdown_on_game_exit_) {
    // stop recorder
    shoutdown_on_stop_ = true;
    blog(LOG_INFO, "game exit, request to stop recording");
    command_thread_->PostTask(std::bind(&OBS::StopRecordingOnGameSourceExit, this));
  }

  if (has_window_source()) {
    blog(LOG_INFO, "OOPO stopped, Don't stop, continue recording the game window");
  }

  if (game_source_) {
    game_source_capture_failure_ = !game_source_->did_start_capture();
    game_source_->SetVisible(false);
  }

  if (current_visible_source_ == game_source_.get()) {
    blog(LOG_INFO, "current visible source is null!");
    SetVisibleSource(nullptr);
  }

  game_source_.reset(nullptr);

  if (has_monitor_source()) {
    UpdateSourcesVisiblity(false, true);
  }
}

//------------------------------------------------------------------------------
void OBS::StartPendingDelayRecording() {
  if (!advanced_output_->DelayRecorderActive()) {
    return;
  }

  CREATE_OBS_DATA(error_result);
  int identifier = advanced_output_->identifier();
  blog(LOG_INFO, "game capture started, start recording... %d", identifier);
  // fail to start
  if (!StartRecording(advanced_output_->identifier(), error_result)) {
    obs_data_set_int(error_result, protocol::kCommandIdentifier, identifier);
    blog(LOG_INFO, "Failed start recording (id:%d).", identifier);
    communications_->Send(protocol::events::ERR, error_result);
  }

}

//------------------------------------------------------------------------------
void OBS::OnGameQuite(bool force) {
  blog(LOG_INFO, "game exit, stopping replay [force:%d]", force);


  if (force ||
      !advanced_output_ ||
      !advanced_output_->replay_output_.get()) {
    this->StopReplay(true);
    return;
  }

  this->StopReplay(force);
  blog(LOG_WARNING, "replay capture is in progress. delay stop replay!");
  if (!stop_replay_timer_.get()) {
    stop_replay_timer_.reset(new libascentobs::TimerQueueTimer(this));
  }

  stop_replay_timer_->Start(10000);
}

//------------------------------------------------------------------------------
bool OBS::SetVisibleSourceName(OBSData& data) {
  libascentobs::CriticalSectionLock lock(visible_source_sync_);
  if (!current_visible_source_) {
    return false;
  }

  obs_data_set_string(data, "source", current_visible_source_->name());
  return true;
}

//------------------------------------------------------------------------------
std::string OBS::GetVisibleSource() {
  libascentobs::CriticalSectionLock lock(visible_source_sync_);
  if (current_visible_source_ == nullptr)
    return "";

  return current_visible_source_->name();
}

//------------------------------------------------------------------------------
void OBS::SetVisibleSource(Source* new_visible_source) {
  {
    libascentobs::CriticalSectionLock lock(visible_source_sync_);    
    if (new_visible_source == current_visible_source_) {
      return;
    }

    blog(LOG_INFO, "set visible source %s -> %s",
         current_visible_source_ ? current_visible_source_->name() : "null",
         new_visible_source ? new_visible_source->name() : "null");

    if (nullptr == current_visible_source_) {
      // first time setting visible source - we don't inform about it since the
      // OnStartXXX callbacks will do it
      current_visible_source_ = new_visible_source;
      return;
    }

    if (current_visible_source_ != new_visible_source) {
      // we changed the source - we need to inform about it
      current_visible_source_ = new_visible_source;

      if (!advanced_output_->Active()) {
        // if we aren't recording/streaming - don't send an event about the change
        return;
      }
    }

    if (current_visible_source_ == nullptr) {
      return;
    }
  }

  if (advanced_output_->DelayRecorderActive()) {
    if (!has_monitor_source()) {
      blog(LOG_WARNING, "recoding is delayed, skip source notification (no monitor source)");
      return;
    }
    StartPendingDelayRecording();
  }


  if (current_visible_source_ != nullptr) {
    NotifyGameSourceChangedSafe();
  }
}

//------------------------------------------------------------------------------
void OBS::Shutdown() { 
  StopRecording(true);
  StopReplay(true);
  
  if (!game_source_.get()) {
    return;
  }

  RemoveGameSource();
}

//------------------------------------------------------------------------------
void OBS::RemoveGameSource() {
  blog(LOG_INFO, "remove game source");
  std::unique_lock<std::mutex> lock(access_mutex_);
  try {
    if (game_source_.get()) {
      game_source_->SetVisible(false);
      obs_sceneitem_remove(game_source_->source_scene_item());
    }
  } catch (...) {

  }
  try {
    if (current_visible_source_ == game_source_.get()) {
      SetVisibleSource(nullptr);
    }
    game_source_.reset(nullptr);
  } catch (...) {}
}

//------------------------------------------------------------------------------
void OBS::ApplyCustomParamters(OBSData& video_custom_parameters) {
  if (obs_data_get_bool(video_custom_parameters,
                        "disable_auto_shutdown_on_game_exit")) {
    disable_shutdown_on_game_exit_ = true;
    blog(LOG_INFO, "Disable shutdown on game exit!");
  }


  custom_source_setting_ = obs_data_get_array(
    video_custom_parameters, kVideoCustomParamterInternalKey);
  
}