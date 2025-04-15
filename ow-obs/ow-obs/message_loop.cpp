/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2017 Overwolf Ltd.
*******************************************************************************/
#include "message_loop.h"

#include <windows.h>

//------------------------------------------------------------------------------
MessageLoop::MessageLoop() : running_(false) {

}

//------------------------------------------------------------------------------
MessageLoop::~MessageLoop() {
  running_ = false;
}

//------------------------------------------------------------------------------
void MessageLoop::Run() {
  running_ = true;
  std::unique_lock<std::mutex> lock(access_mutex_);
  conditional_variable_.wait(lock);
  running_ = false;
}

//------------------------------------------------------------------------------
void MessageLoop::Quit() {
  if (!running_) {
    return;
  }

  conditional_variable_.notify_one();
}
