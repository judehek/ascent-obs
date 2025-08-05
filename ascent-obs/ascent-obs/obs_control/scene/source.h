#ifndef ASCENTOBS_OBS_CONTROL_SOURCE_H_
#define ASCENTOBS_OBS_CONTROL_SOURCE_H_

#include <memory>
#include "utils.h"

struct obs_scene_item;


class Source {
public:
  class Delegate {
    public:
      virtual void GetCanvasDimensions(uint32_t& output_width,
                                       uint32_t& output_height) = 0;

  };
public:
  Source(Delegate* delegate);
  virtual ~Source();

public:
  virtual bool Create(OBSData& data,
                      obs_scene_t* scene,
                      bool visible = true) = 0;  
  virtual const char* name() { return "source"; };
  virtual void SetVisible(bool visible);
  virtual bool IsVisible();

  inline obs_source_t* source() {
    auto s = source_.get();
    if (s == nullptr)
      return nullptr;

    return s->get_source();
  }

  inline obs_scene_item* source_scene_item() {
    return source_item_;
  }

  inline void MoveTop() {
    if (!source_item_)
      return;

    obs_sceneitem_set_order(source_item_, OBS_ORDER_MOVE_TOP);
  }

protected:
  bool SetTransform(obs_sceneitem_t *item, 
                    obs_bounds_type bounds_type,
                    obs_flip_type flip_type= OBS_FLIP_NONE);

  virtual void OnVisiblityChanged(bool visible);
  virtual void OnSetVisibility() {}

protected:
  Delegate* delegate_ = nullptr;
  std::unique_ptr<SourceContext> source_;
  obs_scene_item* source_item_;
};

#endif // ASCENTOBS_OBS_CONTROL_SOURCE_H_