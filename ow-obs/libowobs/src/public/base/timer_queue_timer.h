/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2017 Overwolf Ltd.
*******************************************************************************/
#ifndef LIBASCENTOBS_TIMER_QUEUE_TIMER_H_
#define LIBASCENTOBS_TIMER_QUEUE_TIMER_H_

#include "macros.h"

namespace libowobs {

class TimerQueueTimer;
struct TimerQueueTimerDelegate {
  virtual void OnTimer(TimerQueueTimer* timer) = 0;
};

// a win32 timer based on |CreateTimerQueueTimer|
class TimerQueueTimer {
public:
  TimerQueueTimer(TimerQueueTimerDelegate* delegate);
  virtual ~TimerQueueTimer();

  bool Start(unsigned long interval);
  bool Stop();

private:
  static void __stdcall WaitOrTimerCallback(void* parameter, unsigned char);

private:
  TimerQueueTimerDelegate* delegate_;
  void* timer_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TimerQueueTimer);
};

};

#endif // LIBASCENTOBS_TIMER_QUEUE_TIMER_H_