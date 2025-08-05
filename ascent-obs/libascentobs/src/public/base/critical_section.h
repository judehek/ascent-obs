#ifndef LIBASCENTOBS_BASE_CRITICAL_SECTION_H_
#define LIBASCENTOBS_BASE_CRITICAL_SECTION_H_

#include <windows.h>

namespace libascentobs {

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

}; // namespace libascentobs

#endif // LIBASCENTOBS_BASE_CRITICAL_SECTION_H_