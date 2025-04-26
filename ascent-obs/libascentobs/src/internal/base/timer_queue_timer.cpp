
#include "base/timer_queue_timer.h"
#include <windows.h>

namespace libowobs {

};

using namespace libowobs;

//------------------------------------------------------------------------------
TimerQueueTimer::TimerQueueTimer(TimerQueueTimerDelegate* delegate) :
  delegate_(delegate),
  timer_(nullptr) {
}

//------------------------------------------------------------------------------
// virtual 
TimerQueueTimer::~TimerQueueTimer() {
  Stop();
}

//------------------------------------------------------------------------------
bool TimerQueueTimer::Start(unsigned long interval) {
  if (nullptr != timer_) {
    return false;
  }

  if (0 == interval) {
    return false;
  }

  if (nullptr == delegate_) {
    return false;
  }

  if (!CreateTimerQueueTimer(&timer_,
                             NULL, 
                             WaitOrTimerCallback,
                             (void*)this, 
                             (DWORD)interval,
                             (DWORD)interval,
                             WT_EXECUTEDEFAULT)) {
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------
bool TimerQueueTimer::Stop() {
  if (nullptr == timer_) {
    return false;
  }

  DeleteTimerQueueTimer(NULL, timer_, NULL);
  timer_ = nullptr;
  return true;
}

//------------------------------------------------------------------------------
// static 
VOID CALLBACK TimerQueueTimer::WaitOrTimerCallback(PVOID parameter, BOOLEAN) {
  TimerQueueTimer* timer = (TimerQueueTimer*)parameter;
  timer->delegate_->OnTimer(timer);
}
