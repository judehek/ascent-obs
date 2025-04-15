#include "Time.h"

#include <ctime>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

uint64_t Utils::Time::CurrentSecsSinceEpoch()
{
	 long long secondsSinceEpoch = 0;

#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    // Convert FILETIME to seconds since Epoch
    ULARGE_INTEGER ull;
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    secondsSinceEpoch = (ull.QuadPart - 116444736000000000ull) / 10000000ull;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    secondsSinceEpoch = tv.tv_sec;
#endif

    return secondsSinceEpoch;
}
