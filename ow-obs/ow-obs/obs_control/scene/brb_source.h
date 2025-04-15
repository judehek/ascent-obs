/*******************************************************************************
* Overwolf OBS BRB Image source
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#ifndef OWOBS_OBS_CONTROL_BRB_SOURCE_H_
#define OWOBS_OBS_CONTROL_BRB_SOURCE_H_

#include "obs_control/scene/source.h"

class BRBSource : public Source {
public:
  BRBSource(Delegate* delegate):Source(delegate) {}
  virtual ~BRBSource();

public:
  virtual bool Create(OBSData& data, obs_scene_t* scene,
    bool visible = true);

  virtual const char* name() override {
    return "brb";
  };

  virtual void SetVisible(bool visible);
  virtual bool UpdateImage(const char* image_file_path);

  void Update(OBSData& data);

public:
  virtual void OnVisiblityChanged(bool visible);

private:
  void CreateBeckgroundColor(int color);

  void UpdateImagePosition();

private:
  std::unique_ptr<SourceContext> background_source_;
  obs_scene_item*  background_scene_item_;

};

#endif // OWOBS_OBS_CONTROL_BRB_SOURCE_H_