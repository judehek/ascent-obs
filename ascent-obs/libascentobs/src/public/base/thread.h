
#ifndef LIBASCENTOBS_BASE_THREAD_H_
#define LIBASCENTOBS_BASE_THREAD_H_

#include <memory>
#include <queue>
#include <functional>
#include <string>

#include <windows.h>

#include "macros.h"

namespace libowobs {

class Thread {
public:
  typedef std::function<void()> Task;

public:
  Thread();
  virtual ~Thread();

public:
  static void SetCurrentThreadName(const char* thread_name);

  bool Start(const char* thread_name = nullptr, bool comInitialize = false);
  bool Stop(bool finish_all_task = false, DWORD timeout = 0);
  bool PostTask(Task task_func);

  inline bool IsRunning() {
    return ((nullptr != thread_) && (!stopping_));
  }

  DWORD GetThreadId() {
    return thread_id_;
  }

private:
  bool CreateEvent();
  void DestroyEvent();
  void ClearQueue();
  void HandleNewTaskEvent();

  static DWORD WINAPI ThreadProc(IN LPVOID lpParameter_);

private:
  // thread call CoInitializeEx
  bool comInitialize_ = false;
  bool comInitialized_ = false;

  // thread running the server
  HANDLE thread_;
  HANDLE thread_event_;
  DWORD  thread_id_;

  std::string thread_name_;
  bool stopping_;
  bool finish_all_tasks_;

  typedef std::queue<Task> TaskQueue;
  TaskQueue task_queue_;
  CRITICAL_SECTION queue_critical_section_;
};

typedef std::shared_ptr<Thread> SharedThreadPtr;
};


#endif // LIBASCENTOBS_BASE_THREAD_H_