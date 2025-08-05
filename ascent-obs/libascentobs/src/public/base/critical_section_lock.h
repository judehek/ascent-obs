#ifndef LIBASCENTOBS_BASE_CRITICAL_SECTION_LOCK_H_
#define LIBASCENTOBS_BASE_CRITICAL_SECTION_LOCK_H_

#include "critical_section.h"

namespace libascentobs {

class CriticalSectionLock {
public:
  CriticalSectionLock(CriticalSection& critica_section)
    : critical_section_(critica_section) {
    critical_section_.Lock();
  }

  virtual ~CriticalSectionLock() {
    critical_section_.Unlock();
  }

  // suppress warning C4512: assignment operator could not be generated
  CriticalSectionLock& operator=(const CriticalSectionLock& obj) {
    this->critical_section_ = obj.critical_section_;
    return *this; 
  }

private:
  CriticalSection& critical_section_;
};
}; // namespace libascentobs

#endif // LIBASCENTOBS_BASE_CRITICAL_SECTION_LOCK_H_