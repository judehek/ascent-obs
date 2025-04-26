#ifndef LIBASCENTOBS_COMMUNICATIONS_COMMUNICATION_CHANNEL_STD_H_
#define LIBASCENTOBS_COMMUNICATIONS_COMMUNICATION_CHANNEL_STD_H_

//-----------------------------------------------------------------------------

#include "../base/primitives.h"
#include "../base/thread.h"
#include "../base/timer_queue_timer.h"
#include "communication_channel_delegate.h"
#include "protocol.h"


//-----------------------------------------------------------------------------



//-----------------------------------------------------------------------------

struct os_process_pipe;

//-----------------------------------------------------------------------------

void DebugOutput(const char* pText, const char* pData = NULL);

//-----------------------------------------------------------------------------

#ifdef _DEBUG
#define DEBUG_PRINT(...)  DebugOutput(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

//-----------------------------------------------------------------------------

namespace libascentobs {

//-----------------------------------------------------------------------------
class CommunicationChannelStd : public ICommunicationChannel {
 public:
  // master = true if you are the master and want to control the slave
  static CommunicationChannelStd* Create(bool master,
                                       CommunicationChannelDelegate* delegate);
  
  virtual ~CommunicationChannelStd();

public:
  bool Launch(const wchar_t* path, const wchar_t* command_line  = nullptr);

  virtual bool Start() override;
  virtual bool Start(bool comInitialize) override;
  virtual bool Stop() override;
  virtual bool StopNow(DWORD timeout = 0) override;
  virtual bool Send(const uint8_t* data, size_t size) override;
  virtual bool Shutdown(DWORD timeout = INFINITE) override;
  virtual uint32_t GetProcessID() override;

public:

  bool IsRunning() { return is_running_; }
  void StartRunning() {
    is_running_ = true;
  }
  void StopRunning() {
    is_running_ = false;
  }

public:

  void SetDelegate(CommunicationChannelDelegate* delegate) {
    delegate_ = delegate;
  }

  os_process_pipe* GetPipe();
  bool IsMaster() {return master_;}

  

// ReceiverDelegate
public:

  virtual void OnDisconnected();
  virtual void OnData(uint8_t* data, size_t size);

private:

  CommunicationChannelStd(bool master, CommunicationChannelDelegate* delegate);
  bool Connect();

  void Init();

  void SendOnWorkerThread(std::string msg);
  void StopOnWorkerThread();

  static DWORD CALLBACK receiver_thread(LPVOID param);

private:
  struct os_process_pipe* pipe_ = nullptr;
  bool comInitialize_ = false;

  bool is_init_     = false;
  bool master_      = false;
  bool is_running_  = false;

  DWORD thread_id_ = 0;
  HANDLE thread_handle_ = NULL;

  CommunicationChannelDelegate* delegate_ = nullptr;

  std::unique_ptr<Thread> thread_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CommunicationChannelStd);

};

};

#endif // LIBASCENTOBS_COMMUNICATIONS_COMMUNICATION_CHANNEL_STD_H_