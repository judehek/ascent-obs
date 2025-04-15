/*******************************************************************************
* Overwolf OBS Scene utils
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#ifndef OWOBS_OBS_CONTROL_UTILS_H_
#define OWOBS_OBS_CONTROL_UTILS_H_

#include <obs.hpp>

class SourceContext {
  obs_source_t *source;
public:
  inline SourceContext(obs_source_t *source) : source(source) {}
  inline ~SourceContext() { obs_source_release(source); }
  inline operator obs_source_t*() { return source; }
  inline obs_source_t* get_source() { return source; }
};

class SceneContext {
  obs_scene_t *scene;
public:
  inline SceneContext(obs_scene_t *scene) : scene(scene) {}
  inline ~SceneContext() { obs_scene_release(scene); }
  inline obs_scene_t* get_scene() { return scene; }
  inline operator obs_scene_t*() { return scene; }
};

/* --------------------------------------------------- */

class DisplayContext {
  obs_display_t *display;

public:
  inline DisplayContext() :display(nullptr) {}
  inline DisplayContext(obs_display_t *display) : display(display) {}
  inline ~DisplayContext() { if (display) obs_display_destroy(display); }
  inline operator obs_display_t*() { return display; }

  inline obs_display_t* get() { return display; }
};

#endif // OWOBS_OBS_CONTROL_UTILS_H_