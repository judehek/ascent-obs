
#include "base/thread.h"

namespace libascentobs {

const int kStopThreadTimeoutInSeconds = 10;
const DWORD kStopThreadTimeoutMS = 10000;

//http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
  typedef struct tagTHREADNAME_INFO
  {
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
  } THREADNAME_INFO;
#pragma pack(pop)
};

using namespace libascentobs;

//-----------------------------------------------------------------------------
Thread::Thread() :
  thread_(nullptr),
  thread_id_(0),
  stopping_(false),
  finish_all_tasks_(false) {
  InitializeCriticalSection(&queue_critical_section_);
}

//-----------------------------------------------------------------------------
Thread::~Thread() {
  Stop(false, 1);

  if (thread_ != nullptr) {
    CloseHandle(thread_);
    thread_ = nullptr;
  }

  ClearQueue();
  DeleteCriticalSection(&queue_critical_section_);
}

//------------------------------------------------------------------------------
// static o
void Thread::SetCurrentThreadName(const char* thread_name) {
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = thread_name;
  info.dwThreadID = ::GetCurrentThreadId();
  info.dwFlags = 0;

  __try {
    RaiseException(MS_VC_EXCEPTION,
      0,
      sizeof(info) / sizeof(ULONG_PTR),
      (ULONG_PTR*)&info);
  }
  __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}

//-----------------------------------------------------------------------------
bool Thread::Start(const char* thread_name/*= nullptr*/,
                   bool comInitialize /*= false*/) {
  if (nullptr != thread_) {
    return false;
  }

  if (!CreateEvent()) {
    return false;
  }

  thread_name_.clear();
  if (thread_name) {
    thread_name_ = thread_name;
  }

  ClearQueue();

  stopping_ = false;

  if (thread_ != nullptr) {
    CloseHandle(thread_);
    thread_ = nullptr;
  }

  comInitialize_ = comInitialize;
  thread_ = CreateThread(nullptr,
                         0,
                         ThreadProc,
                         (LPVOID)this,
                         0,
                         &thread_id_);

  return (nullptr != thread_);
}

//-----------------------------------------------------------------------------
bool Thread::Stop(bool finish_all_task/*= false*/, DWORD timeout/*= 0*/) {
  if (nullptr == thread_ || stopping_) {
    return true;
  }

  finish_all_tasks_ = finish_all_task;

  if (GetCurrentThreadId() == thread_id_) {
    stopping_ = true;
    return true;
  }

  stopping_ = true;

  if (thread_event_ == nullptr) {
    return false;
  }

  SetEvent(thread_event_);
  bool ret = (WAIT_OBJECT_0 == WaitForSingleObject(
    thread_,
    timeout == 0 ? kStopThreadTimeoutMS : timeout));

  if (finish_all_tasks_) {
    HandleNewTaskEvent();
  }

  DestroyEvent();
  return ret;
}

//-----------------------------------------------------------------------------
bool Thread::PostTask(Task task_func) {

  if (NULL == thread_) {
    return false;
  }

  if (stopping_) {
    return false;
  }

  EnterCriticalSection(&queue_critical_section_);
  task_queue_.push(task_func);
  LeaveCriticalSection(&queue_critical_section_);

  return (TRUE == SetEvent(thread_event_));
}

//-----------------------------------------------------------------------------
bool Thread::CreateEvent() {
  thread_event_ = ::CreateEvent(nullptr,
                                FALSE, // manual reset
                                FALSE, // initial state
                                nullptr);

  return true;
}

//-----------------------------------------------------------------------------
void Thread::DestroyEvent() {
  if (nullptr != thread_event_) {
    CloseHandle(thread_event_);
    thread_event_ = nullptr;
  }
}

//-----------------------------------------------------------------------------
void Thread::ClearQueue() {
  __try {
    EnterCriticalSection(&queue_critical_section_);
    while (!task_queue_.empty()) {
      task_queue_.pop();
    }
    LeaveCriticalSection(&queue_critical_section_);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}

//-----------------------------------------------------------------------------
void Thread::HandleNewTaskEvent() {
  while (!task_queue_.empty()) {
    // don't continue processing messages if we are stopping
    if (stopping_ && !finish_all_tasks_) {
      return;
    }

    EnterCriticalSection(&queue_critical_section_);
    Task task = task_queue_.front();
    task_queue_.pop();
    LeaveCriticalSection(&queue_critical_section_);

    if (task != nullptr) {
      task();
    }
  }
}

//-----------------------------------------------------------------------------
DWORD WINAPI Thread::ThreadProc(IN LPVOID lpParameter_) {
  if (nullptr == lpParameter_) {
    return 0;
  }

  Thread* thread = (Thread*)lpParameter_;

  if (thread->comInitialize_) {
    thread->comInitialized_ = SUCCEEDED(
      CoInitializeEx(0, COINIT_APARTMENTTHREADED));
  }

  if (!thread->thread_name_.empty()) {
    Thread::SetCurrentThreadName(thread->thread_name_.c_str());
  }

  while (!thread->stopping_) {
    WaitForSingleObject(thread->thread_event_, INFINITE);
    thread->HandleNewTaskEvent();
  }

  if (thread->comInitialized_) {
    CoUninitialize();
    thread->comInitialized_ = false;
  }

  return 0;
}