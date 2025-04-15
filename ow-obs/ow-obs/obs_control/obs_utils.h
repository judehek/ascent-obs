/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2017 Overwolf Ltd.
*******************************************************************************/
#ifndef OWOBS_OBS_CONTROL_OBS_UTILS_H_
#define OWOBS_OBS_CONTROL_OBS_UTILS_H_
#include <stdint.h>
#include <string>
#include <sstream>
#include <vector>

// because OBSData automatically adds to the reference count, we need to
// manually reduce 1 from the count on creation
#define CREATE_OBS_DATA(variable) \
  OBSData variable = obs_data_create(); \
  obs_data_release(variable);

#define SET_OBS_DATA(variable, data) \
  OBSData variable = data; \
  obs_data_release(variable);

#define CREATE_OBS_DATA_ARRAY(variable) \
  OBSDataArray variable = obs_data_array_create(); \
  obs_data_array_release(variable);

#define CLEAR_OBS_DATA_ARRAY(array) { \
  size_t count = obs_data_array_count(array); \
  for (size_t i = count; i > 0; --i) { \
    obs_data_array_erase(array, (i - 1)); \
  } \
}

namespace utils {
extern void EpochSystemTimeToUnixEpochTime(int64_t& time);

extern std::string join(const std::vector<uint32_t>& vec,
                 const std::string& delimiter = ",");
} // namespace utils


#endif // OWOBS_OBS_CONTROL_OBS_UTILS_H_