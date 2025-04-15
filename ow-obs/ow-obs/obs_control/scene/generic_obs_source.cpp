/*******************************************************************************
* Overwolf OBS GenericObsSource
*
* Copyright (c) 2020 Overwolf Ltd.
*******************************************************************************/

#include "obs_control/scene/generic_obs_source.h"
#include <map>
#include <vector>
#include <windows.h>
#include <dshow.h>

const char* kImageSourceId = "image_source";


namespace {
  bool ApplayTransform(const char* transform, 
                       vec2 video_size,
                       vec2 source_size,
                       vec2& pos) {
    if (!transform) {
      return false;
    }

    if (_stricmp(transform, "dockTopLeft") == 0) {
      pos.x = 0;
      pos.y = 0;
    } else if (_stricmp(transform, "dockTopRight") == 0) {      
      pos.x = video_size.x - source_size.x;
      pos.y = 0;
    } else if (_stricmp(transform, "dockTopMiddle") == 0) {
      pos.x = (video_size.x - source_size.x) * 0.5f;
      pos.y = 0;
    } else if (_stricmp(transform, "dockMiddleLeft") == 0) {
      pos.x = 0;
      pos.y = (video_size.y - source_size.y) * 0.5f;
    } else if (_stricmp(transform, "dockCenter") == 0) {
      pos.x = (video_size.x - source_size.x) * 0.5f;
      pos.y = (video_size.y - source_size.y) * 0.5f;
    } else if (_stricmp(transform, "dockMiddleRight") == 0) {
      pos.x = (video_size.x - source_size.x);
      pos.y = (video_size.y - source_size.y) * 0.5f;
    } else if (_stricmp(transform, "dockBottomLeft") == 0) {
      pos.x = 0;
      pos.y = (video_size.y - source_size.y);
    } else if (_stricmp(transform, "dockBottomMiddle") == 0) {
      pos.x = (video_size.x - source_size.x) * 0.5f;
      pos.y = (video_size.y - source_size.y);
    } else if (_stricmp(transform, "dockBottomRight") == 0) {
      pos.x = (video_size.x - source_size.x);
      pos.y = (video_size.y - source_size.y);
    } else {
      return false;
    }

    return true;
  }
}

class ImageObsSource : public GenericObsSource {
 public:
  ImageObsSource(Delegate* delegate = nullptr) : GenericObsSource(delegate) {
  }
  ~ImageObsSource() {}

  virtual bool Create(OBSData& data, obs_scene_t* scene, bool visible = true) {
    name_ = obs_data_get_string(data, "name");
    OBSData parameters = obs_data_get_obj(data, "parameters");

    if (!parameters.Get()) {
      return false;
    }

    auto file = obs_data_get_string(parameters, "file");
    if (!file) {
      file = obs_data_get_string(data, "file");
      obs_data_set_string(parameters, "file", file);
    }

    if (!file) {
      blog(LOG_ERROR, "OBS Image source '%s' missing 'file' parameter",
           name_.c_str(), kImageSourceId);
      return false;
    }

    source_.reset(new SourceContext(
        obs_source_create(kImageSourceId, name_.c_str(), parameters, nullptr)));

    auto image_width = obs_source_get_width((*source_));
    auto image_height = obs_source_get_height((*source_));

    if (image_height == 0 || image_width == 0) {
      blog(LOG_INFO, 
        "Error create OBS Image source '%s' -  of type '%s' [file: %s] (invalid file path?) [width: %d height: %d]",
        name_.c_str(), kImageSourceId, file, image_width, image_height);
      return false;
    }

   blog(LOG_INFO, "create OBS Image source '%s' -  of type '%s' [file: %s]",
         name_.c_str(), kImageSourceId, file);


    source_item_ = obs_scene_add(scene, source_->get_source());

    uint32_t width = 0, height = 0;

    if (delegate_) {
      delegate_->GetCanvasDimensions(width, height);
    }

    vec2 pos{0, 0};
    const char* transform = obs_data_get_string(data, "transform");
    if (!transform || strlen(transform) == 0) {
      transform = obs_data_get_string(parameters, "transform");
    }
    ApplayTransform(transform, {(float)width, (float)height},
                    {(float)image_width, (float)image_height}, pos); 

    blog(LOG_INFO, "apply transform ['%s'] position [%d, %d]",
         transform ? transform : "none", pos.x, pos.y);

    obs_sceneitem_set_pos(source_item_, &pos);
    obs_sceneitem_set_visible(source_item_, visible);

    return true;
  }
};

Source* GenericObsSource::CreateOBSSource(Delegate* delegate /*= nullptr*/,
                                          OBSData& data,
                                          obs_scene_t* scene,
                                          bool visible /*= true*/) {
  //MessageBox(0, L"CreateOBSSource", L"CreateOBSSource", 0);
  std::string source_id = obs_data_get_string(data, "sourceId");
  std::string source_type = obs_data_get_string(data, "source_type");
  
  Source* source = nullptr;
  if (source_id == kImageSourceId || source_type == kImageSourceId) {
    source = new ImageObsSource(delegate);
  } else {
    source = new GenericObsSource(delegate);
  }

  if (source->Create(data, scene, visible)) {
    return source;
  }

  blog(LOG_ERROR, "fail to create '%s' [id: %s type: %s] source",
       source->name(), source_id, source_type);

  delete source;
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
bool GenericObsSource::Create(OBSData& data, obs_scene_t* scene, bool visible) {
  bool expandCanvas = false;//obs_data_get_bool(data, "expandCanvas") && false;
  int stretched = (int)obs_data_get_int(data, "transform");

  auto posx = (float)obs_data_get_double(data, "posx");
  auto posy = (float)obs_data_get_double(data, "posy");

  std::string source_id = obs_data_get_string(data, "sourceId");
  name_ = obs_data_get_string(data, "name");

  obs_data_t* generic_settings = obs_data_create();
  if (!generic_settings) {
    blog(LOG_ERROR, "OBS source '%s' - fail to create setting", name_.c_str());
    return false;
  }

  blog(LOG_INFO, "create OBS source '%s' -  of type '%s'",
    name_.c_str(), source_id.c_str());

  ParseParamters(generic_settings, data);
  source_.reset(new SourceContext(obs_source_create(
    source_id.c_str(), name_.c_str(), generic_settings, nullptr)));

  obs_data_release(generic_settings);
  source_item_ = obs_scene_add(scene, source_->get_source());

  vec2 pos{ 0, 0 };
  pos.x = expandCanvas ? (0.5f + posx * 0.5f) : posx;
  pos.y = posy;

  uint32_t width = 1, height = 1;
  if (delegate_) {
    delegate_->GetCanvasDimensions(width, height);
  }

  pos.x *= width;
  pos.y *= height;

  obs_sceneitem_set_pos(source_item_, &pos);

  if (stretched) {
    auto scalex = obs_data_get_double(data, "scalex");
    auto scaley = obs_data_get_double(data, "scaley");

    vec2 bounds{ static_cast<float>(scalex * width / 2), // for expandCanvas
                 static_cast<float>(scaley * height) };

    if (!expandCanvas) {
      bounds.x = static_cast<float>(scalex * width);
    }

    blog(LOG_INFO, "OBS source '%s' bounds - scale:[%d, %d], bounds[%f, %f].",
      name_.c_str(), scalex, scaley, bounds.x, bounds.y);

    obs_sceneitem_set_bounds(source_item_, &bounds);
    obs_sceneitem_set_bounds_type(source_item_, OBS_BOUNDS_STRETCH);
  }

  obs_sceneitem_set_visible(source_item_, visible);

  return true;
}

//----------------------------------------------------------------------------
void GenericObsSource::ParseParamters(obs_data_t* settings, OBSData& data) {
  OBSDataArray list = obs_data_get_array(data, "parameters");  // TEMP array; //

  if (list == nullptr) {
    blog(LOG_WARNING, "Game capture source '%s' - no parameter", name_.c_str());
    return;
  }

  size_t size = obs_data_array_count(list);
  for (size_t i = 0; i < size; i++) {
    OBSData obj = obs_data_array_item(list, i);

    const char* param_name = obs_data_get_string(obj, "name");
    if (param_name == nullptr) {
      blog(LOG_ERROR,
        "Game capture source '%s' - parameter name is missing [index:%d]",
        name_.c_str(), i);
      continue;
    }

    auto param_type = obs_data_get_int(obj, "type");
    switch (param_type) {
      case 0:
      {
        const __int64 param_value_int = obs_data_get_int(obj, "value");
        obs_data_set_int(settings, param_name, param_value_int);
        blog(LOG_INFO, "Game capture source '%s - set parameter '%s': %d",
          name_.c_str(), param_name, param_value_int);
      } break;
      case 1:
      {
        bool param_value_bool = obs_data_get_bool(obj, "value");
        obs_data_set_bool(settings, param_name, param_value_bool);
        blog(LOG_INFO, "Game capture source '%s - set parameter '%s': %s",
          name_.c_str(), param_name, param_value_bool ? "true" : "false");
      } break;
      case 2:
      {
        const char* param_value_str = obs_data_get_string(obj, "value");
        obs_data_set_string(settings, param_name, param_value_str);
        blog(LOG_INFO, "Game capture source '%s - set parameter '%s': %s",
          name_.c_str(), param_name, param_value_str);
      } break;
      case 3:
      {
        double param_value_double = obs_data_get_default_double(obj, "value");
        obs_data_set_double(settings, param_name, param_value_double);
        blog(LOG_INFO, "Game capture source '%s - set parameter '%s': %f",
          name_.c_str(), param_name, param_value_double);
      } break;
      default:
      {
        blog(LOG_ERROR,
          "Game capture source '%s' - parameter '%s' (%d) invalid type '%d'",
          name_.c_str(), param_name, i, param_type);
      };
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
