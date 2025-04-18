/********************************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2015 Overwolf Ltd.
*********************************************************************************************/
#ifndef LIBASCENTOBS_BASE_CRITICAL_SECTION_H_
#define LIBASCENTOBS_BASE_CRITICAL_SECTION_H_

#include <windows.h>

namespace libowobs {

class CriticalSection {
public:
  CriticalSection() {
    InitializeCriticalSection(&critical_section_);
  }
  virtual ~CriticalSection() {
    DeleteCriticalSection(&critical_section_);
  }
  
  void Lock() {
    EnterCriticalSection(&critical_section_);
  }

  void Unlock() {
    LeaveCriticalSection(&critical_section_);
  }

private:
  CRITICAL_SECTION critical_section_;
};

}; // namespace libowobs

#endif // LIBASCENTOBS_BASE_CRITICAL_SECTION_H_