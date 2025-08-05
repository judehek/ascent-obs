#include "obs_utils.h"

static const __int64 kMillisecondsPerSecond = 1000;
static const __int64 kMicrosecondsPerMillisecond = 1000;
static const __int64 kMicrosecondsPerSecond =
kMicrosecondsPerMillisecond * kMillisecondsPerSecond;
#define NANO_SECS_INTERVAL_UNITS_BETWEEN_1601_AND_1970   116444736000000000

void utils::EpochSystemTimeToUnixEpochTime(int64_t& time) {
  time -= NANO_SECS_INTERVAL_UNITS_BETWEEN_1601_AND_1970; // we start at 1601 and above
  time /= 10; // 100-nanosecs to microsecs
  time /= kMicrosecondsPerMillisecond;
}

std::string utils::join(const std::vector<uint32_t>& vec,
                       const std::string& delimiter /*= ","*/) {
  std::ostringstream oss;
  for (size_t i = 0; i < vec.size(); ++i) {
    if (i != 0) {
      oss << delimiter;
    }
    oss << vec[i];
  }
  return oss.str();
}
