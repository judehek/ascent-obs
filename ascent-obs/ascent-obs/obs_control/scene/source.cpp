#include "obs_control/scene/source.h"

//------------------------------------------------------------------------------
Source::Source(Delegate* delegate) :
  delegate_(delegate),
  source_item_(nullptr) {
}

//------------------------------------------------------------------------------
Source::~Source() {
  source_item_ = nullptr;
}

//------------------------------------------------------------------------------
void Source::SetVisible(bool visible) {
  if (source_item_ == nullptr)
    return;

  if (visible != IsVisible()) {
    blog(LOG_INFO, "Update source |%s| visibility: %s",
      this->name(), visible ? "true" : "false");

    OnVisiblityChanged(visible);

  }

  if (visible) {
    OnSetVisibility();
  }
}

bool Source::IsVisible() {
  if (source_item_ == nullptr)
    return false;

  return obs_sceneitem_visible(source_item_);
}

//------------------------------------------------------------------------------
bool Source::SetTransform(obs_sceneitem_t *item, 
                          obs_bounds_type bounds_type,
                          obs_flip_type flip_type) {
  if (item == nullptr)
    return false;

  try {
    obs_video_info ovi;
    if (!obs_get_video_info(&ovi))
      return false;

    obs_transform_info itemInfo;
    vec2_set(&itemInfo.pos, 0.0f, 0.0f);
    vec2_set(&itemInfo.scale, 1.0f, 1.0f);
    itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
    itemInfo.rot = 0.0f;

    vec2_set(&itemInfo.bounds,
      float(ovi.base_width), float(ovi.base_height));

    itemInfo.bounds_type = bounds_type;
    itemInfo.flip_type = flip_type;

    itemInfo.bounds_alignment = OBS_ALIGN_CENTER;

    obs_sceneitem_set_info2(item, &itemInfo);
  } catch (...) {
    blog(LOG_ERROR, "Update source |%s| transform error", this->name());
  }

  return true;
}

void Source::OnVisiblityChanged(bool visible) {
  if (source_item_) {
    obs_sceneitem_set_visible(source_item_, visible);
  }
}
