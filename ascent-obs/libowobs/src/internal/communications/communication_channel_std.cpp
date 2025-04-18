/******************************************************************************
* Communication Channel Std
*
* Copyright (c) 2021 Overwolf Ltd.
******************************************************************************/

#include "communications/communication_channel_std.h"
#include "..\internal\win_ipc\pipe.h"
#include <windows.h>


#define BUFSIZE 8096
//struct _message {
//  unsigned int len = 0;
//  uint8_t chBuf[BUFSIZE] = "";
//};
//-----------------------------------------------------------------------------
#ifdef _DEBUG
void DebugOutput(const char* pText, const char* pData) {
  static DWORD start_time = GetTickCount();

  DWORD current_time = GetTickCount() - start_time;
  DWORD seconds = current_time / 1000;
  DWORD ms = current_time - (seconds * 1000);

  char string[BUFSIZE] = "";

  if (pData) {
    sprintf_s(string, BUFSIZE, "%02d:%03d %s%s\n", seconds, ms, pText, pData);
  } else {
    sprintf_s(string, BUFSIZE, "%02d:%03d %s\n", seconds, ms, pText);
  }

  printf(string);
  OutputDebugStringA(string);
}
#endif
//-----------------------------------------------------------------------------

namespace libowobs {
const char kThreadName[] = "std_communications_worker_thread";
}

//-----------------------------------------------------------------------------

using namespace libowobs;

//-----------------------------------------------------------------------------
// static
CommunicationChannelStd* CommunicationChannelStd::Create(bool master,
                                      CommunicationChannelDelegate* delegate) {
  CommunicationChannelStd* channel=new CommunicationChannelStd(master,delegate);

  if (!channel) {
    return NULL;
  }

  if (!master) {
    channel->Connect(); // Server side set all handles
  }

  channel->Init();

  return channel;
}

//-----------------------------------------------------------------------------
CommunicationChannelStd::CommunicationChannelStd(
  bool master, CommunicationChannelDelegate* delegate) :
  master_(master),
  delegate_(delegate) {
}

//-----------------------------------------------------------------------------
CommunicationChannelStd::~CommunicationChannelStd() {
  DEBUG_PRINT("~CommunicationChannelStd");
  Shutdown(1000);
  delegate_ = NULL;
}

//-----------------------------------------------------------------------------
bool CommunicationChannelStd::Shutdown(DWORD timeout /*= INFINITE*/) {
  if (!is_init_) {
    return false;

  }

  os_process_pipe_destroy(pipe_, timeout); // wait to the ascent-obs process to finish
  pipe_ = NULL;

  is_init_ = false;

  return true;
}

//-----------------------------------------------------------------------------
bool CommunicationChannelStd::Launch(const wchar_t* path,
                                     const wchar_t* command_line /*= nullptr*/) {
  if (!master_ || !path) {
    return NULL;
  }

  pipe_ = os_process_pipe_create(path, command_line);

  return pipe_ != nullptr;
}

//-----------------------------------------------------------------------------
bool CommunicationChannelStd::Connect() {
  HANDLE handle_read  = GetStdHandle(STD_INPUT_HANDLE);
  HANDLE handle_write = GetStdHandle(STD_OUTPUT_HANDLE);

  pipe_ = os_process_pipe_connect(handle_read, handle_write);

  return pipe_ != nullptr;
}

//-----------------------------------------------------------------------------
DWORD CALLBACK CommunicationChannelStd::receiver_thread(LPVOID param) {
  bool comInitialized = false;
  try {

    CommunicationChannelStd* pCommunicationChannelStd =
      (CommunicationChannelStd*)param;

    if (pCommunicationChannelStd->comInitialize_) {
      comInitialized = SUCCEEDED(
        CoInitializeEx(0, COINIT_APARTMENTTHREADED));
      if (!comInitialized) {
        ::OutputDebugStringA("!! receiver_thread thread comInitialized error !!!");
      }
    }

    uint8_t buffer[BUFSIZE] = "";
    while (pCommunicationChannelStd->IsRunning()) {
      DWORD bytes_read = (DWORD)os_process_pipe_read(
        pCommunicationChannelStd->GetPipe(), buffer, BUFSIZE);
      if (bytes_read > 0) {
        pCommunicationChannelStd->OnData(buffer, bytes_read);
      } else {
        pCommunicationChannelStd->StopRunning();
      }
    }
    pCommunicationChannelStd->OnDisconnected();
  } catch (...) {
    ::OutputDebugStringA("receiver_thread failed !!!");
  }

  if (comInitialized) {
    CoUninitialize();
  }

  return 0;
}

//-----------------------------------------------------------------------------
bool CommunicationChannelStd::Start() {
  return Start(false);
}

//-----------------------------------------------------------------------------
bool CommunicationChannelStd::Start(bool comInitialize) {
  StartRunning();

  delegate_->OnConnected();

  if (!thread_->Start(kThreadName)) {
    return false;
  }

  comInitialize_ = comInitialize;
  thread_handle_ = CreateThread(NULL, 0,receiver_thread, this, 0, &thread_id_);

  if (!thread_handle_) {
    return false;
  }

  return true;
}

//-----------------------------------------------------------------------------
bool CommunicationChannelStd::Stop() {

  StopRunning();

  Thread::Task task(std::bind(&CommunicationChannelStd::StopOnWorkerThread,this));

  return thread_->PostTask(task);
}

//-----------------------------------------------------------------------------
bool CommunicationChannelStd::StopNow(DWORD timeout /*= 0*/) {
  DEBUG_PRINT("StopNow - Start");

  StopRunning();
  WaitForSingleObject(thread_handle_, timeout);
  CloseHandle(thread_handle_);
  thread_handle_ = NULL;

  if (!thread_.get()) {
    return false;
  }

  DEBUG_PRINT("StopNow - End");

  return thread_->Stop(true, timeout);
}

//-----------------------------------------------------------------------------
bool CommunicationChannelStd::Send(const uint8_t* data, size_t size) {
  if (size > BUFSIZE) {
    DEBUG_PRINT("CommunicationChannelStd::Send error: message too big\n");
    return false;
  }

  std::string buffer;
  buffer.assign(data, data + size);
  Thread::Task task(std::bind(&CommunicationChannelStd::SendOnWorkerThread,
                    this, buffer));

  return thread_->PostTask(task);
}

//-----------------------------------------------------------------------------
void CommunicationChannelStd::OnDisconnected() {
  if (delegate_) {
    delegate_->OnDisconnected();
  }
}

//-----------------------------------------------------------------------------
void CommunicationChannelStd::OnData(uint8_t* data, size_t size) {
  if ((nullptr == data) || (0 == size)) {
    return;
  }

  if (!delegate_) {
    return;
  }

  delegate_->OnData(data, size);
}

//-----------------------------------------------------------------------------
void CommunicationChannelStd::Init() {
  thread_.reset(new Thread);
  is_init_ = true;
}

//-----------------------------------------------------------------------------
void CommunicationChannelStd::SendOnWorkerThread(std::string msg) {

  try {
    size_t bytes_written = os_process_pipe_write(pipe_,
      (const uint8_t *)msg.c_str(), msg.size());

    if (delegate_ && bytes_written <= 0) { // not ok
      delegate_->OnSendDataError(msg, (int)bytes_written);
    }
  } catch (...) {
    ::OutputDebugStringA("os_process_pipe_write failed !!!");
  }
}

//-----------------------------------------------------------------------------
void CommunicationChannelStd::StopOnWorkerThread() {
  thread_->Stop(true);
}

//-----------------------------------------------------------------------------
uint32_t CommunicationChannelStd::GetProcessID() {
  if (!pipe_) {
    return 0;
  }

  return pipe_->process_id;
}

//-----------------------------------------------------------------------------
os_process_pipe_t* CommunicationChannelStd::GetPipe() {
  return pipe_;
}