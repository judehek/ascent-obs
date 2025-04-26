
#include "obs_control/scene/brb_source.h"
#include <string>
#include <util/platform.h>

#define BACKGROUND_COLOR 0xCC0D0D0D //abgr
#define BRB_DEFAULT_IMAGE "be-right-back.png"

namespace {
  const char kSettingsFilePath[] = "path";
  const char kSettingsBackgroundColor[] = "color";
}

inline bool file_exists(const char* name) {
  if (FILE *file = fopen(name, "r")) {
    fclose(file);
    return true;
  } else {
    return false;
  }
}

BRBSource::~BRBSource() {
  background_scene_item_ = nullptr;
}

bool BRBSource::Create(OBSData& data, obs_scene_t* scene, 
  bool visible /*= true*/) {
  // (TODO) parse montir data;
  if (scene == nullptr) {
    blog(LOG_ERROR, "Failed create BRB source : scene undefined ");
    return false;
  }

  std::string image_file = BRB_DEFAULT_IMAGE;
  if (data != nullptr) {
    const char* file_path =
      obs_data_get_string(data, kSettingsFilePath);
    if (file_path && file_exists(file_path)) {
      image_file = file_path;
    } else {
      blog(LOG_WARNING, "BRB image file doesn't exists: %s", file_path);
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // background color
  // todo get it from data
  CreateBeckgroundColor(BACKGROUND_COLOR);

  background_scene_item_ = obs_scene_add(scene,
     background_source_->get_source());

  //SetTransform(source_item_, OBS_BOUNDS_SCALE_INNER);

  //////////////////////////////////////////////////////////////////////////
  // BRB Image color
  source_.reset(new SourceContext(obs_source_create("image_source",
    "BRB", nullptr, nullptr)));

  source_item_ = obs_scene_add(scene, source());

  UpdateImage("be-right-back.png");

  SetVisible(visible);
  return true;
}

bool BRBSource::UpdateImage(const char* image_file_path) {
  if (source() == nullptr)
    return false;

  obs_data_t *settings_brb_image = obs_data_create();
  obs_data_set_string(settings_brb_image, "file", image_file_path);

  obs_source_update(source(), settings_brb_image);

  obs_data_release(settings_brb_image);

  blog(LOG_INFO, "update BRB image: %s", image_file_path);

  UpdateImagePosition();

  //SetTransform(source_item_, OBS_BOUNDS_SCALE_INNER);

  return true;
}

void BRBSource::Update(OBSData& data) {
  const char* file_path =
    obs_data_get_string(data, kSettingsFilePath);

  if (file_path) {
    if (!os_file_exists(file_path)) {
      blog(LOG_WARNING, "Update BRB Image error - file not found : %s", file_path);
    } else {
      UpdateImage(file_path);
    }
  }
  
  if (obs_data_has_user_value(data, kSettingsBackgroundColor)) {
    int color = (int)obs_data_get_int(data, kSettingsBackgroundColor);
    CreateBeckgroundColor(color);
  }

  UpdateImagePosition();
}

void BRBSource::UpdateImagePosition() {
  obs_video_info ovi;
  if (!obs_get_video_info(&ovi)) {
    blog(LOG_ERROR, "Failed Update BRB position: create retrieve obs ovi setting");
    return;
  }

  int cx = ovi.output_width;
  int cy = ovi.output_height;

  auto image_width =
    obs_source_get_width(source_->get_source());
  auto image_height =
    obs_source_get_base_height(source_->get_source());

  vec2 pos = {
    cx * 0.5f - image_width  * 0.5f,
    cy * 0.5f - image_height * 0.5f
  };

  obs_sceneitem_set_pos(source_item_, &pos);
  blog(LOG_INFO, "update BRB image position: [x:%d y:%d]", (int)pos.x, (int)pos.y);
}

void BRBSource::SetVisible(bool visible) {
  __super::SetVisible(visible);

}

void BRBSource::OnVisiblityChanged(bool visible) {
  __super::OnVisiblityChanged(visible);

  if (background_scene_item_) {
    if (visible) {
      CreateBeckgroundColor(BACKGROUND_COLOR);
    }

    obs_sceneitem_set_order(background_scene_item_, 
      visible ? OBS_ORDER_MOVE_TOP : OBS_ORDER_MOVE_BOTTOM);
    obs_sceneitem_set_visible(background_scene_item_, visible);
  }

  if (visible) {
    UpdateImagePosition();
  } 

  obs_sceneitem_set_order(source_item_, 
    visible ? OBS_ORDER_MOVE_TOP : OBS_ORDER_MOVE_BOTTOM);
}

void BRBSource::CreateBeckgroundColor(int color) {

  obs_video_info ovi;
  if (!obs_get_video_info(&ovi)) {
    blog(LOG_ERROR, "Failed Update BRB background color: create retrieve obs ovi setting");
    return;
  }

  int cx = ovi.base_width;
  int cy = ovi.base_height;

  obs_data_t *color_settings = obs_data_create();
  obs_data_set_int(color_settings, "color", color); //abgr
  obs_data_set_int(color_settings, "width", cx);
  obs_data_set_int(color_settings, "height", cy);

  if (!background_source_.get()) {
    background_source_.reset(new SourceContext(
      obs_source_create("color_source", "BRB_BACKGROUND", color_settings, nullptr)));
  } else {
    obs_source_update(background_source_->get_source(), color_settings);
  }

  blog(LOG_INFO, "Set BRB image color 0x%x", color);
  obs_data_release(color_settings);
}
