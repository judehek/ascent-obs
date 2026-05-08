cmake_minimum_required(VERSION 3.22...3.25)

legacy_check()

add_library(ascent-input-overlay MODULE)
add_library(OBS::ascent-input-overlay ALIAS ascent-input-overlay)

target_sources(ascent-input-overlay PRIVATE ascent-input-overlay.c ascent-input-overlay-shmem.h stb_truetype.h)

target_link_libraries(ascent-input-overlay PRIVATE OBS::libobs)

# cmake-format: off
set_target_properties_obs(ascent-input-overlay PROPERTIES FOLDER plugins PREFIX "")
# cmake-format: on
