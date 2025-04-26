
#include "communications/communication_channel.h"

#include <time.h>
#include <windows.h>
#include <string>

namespace libowobs {
const char kChannelIdMasterFormat[] = "%s_master";
const char kChannelIdSlaveFormat[] = "%s_slave";
const char kThreadName[] = "communications_worker_thread";

#ifdef _DEBUG
const unsigned long kHandshakeTimeoutInMS = 100 * 1000;
#else
const unsigned long kHandshakeTimeoutInMS = 10 * 1000;
#endif // _DEBUG


const uint32_t kHandshakeSignature = 0xdeadbeef;

}

using namespace libowobs;

//------------------------------------------------------------------------------
// static
CommunicationChannel* CommunicationChannel::Create(
  const char* channel_id,
  bool master,
  CommunicationChannelDelegate* delegate) {

  // this will save us from tedious null checking of |delegate_|
  if ((nullptr == channel_id) ||
      (0x00 == channel_id[0]) ||
      (nullptr == delegate)) {
    return nullptr;
  }

  std::string sender, receiver;
  CommunicationChannel::GetChannelIds(channel_id, master, receiver, sender);
  if (receiver.empty() || sender.empty()) {
    return nullptr;
  }

  CommunicationChannel* channel = new CommunicationChannel(master, delegate);
  channel->Init(receiver.c_str(), sender.c_str());
  return channel;
}

//------------------------------------------------------------------------------
CommunicationChannel::CommunicationChannel(
  bool master,
  CommunicationChannelDelegate* delegate) :
  master_(master),
  delegate_(delegate),
  state_(IDLE) {
  InitializeCriticalSection(&handshake_timeout_cs_);
}

//------------------------------------------------------------------------------
// virtual
CommunicationChannel::~CommunicationChannel() {
  delegate_ = NULL;
  DeleteCriticalSection(&handshake_timeout_cs_);
}

//------------------------------------------------------------------------------
// static
std::string CommunicationChannel::GenerateRandomChannelId() {
  static int last_generated_id_for_process = 0;
  last_generated_id_for_process++;

  static bool randomized = false;
  if (!randomized) {
    randomized = true;
    srand((unsigned int)time(nullptr));
  }

  DWORD process_id = GetCurrentProcessId();
  int random_num = (rand() + 1);

  // +3 = two periods and a null
  //char random_channel_id[sizeof(DWORD) + sizeof(int) + sizeof(int) + 3] = { 0 };
  char random_channel_id[256] = { 0 };
  sprintf_s(random_channel_id,
            sizeof(random_channel_id),
            "%d.%d.%d",
            process_id,
            last_generated_id_for_process,
            random_num);

  return random_channel_id;
}

//------------------------------------------------------------------------------
bool CommunicationChannel::Start() {
  if (!receiver_ || !sender_) {
    return false;
  }

  if (IDLE != state_) {
    return false;
  }

  if (!thread_->Start(kThreadName)) {
    return false;
  }

  if (!receiver_->Start()) {
    return false;
  }

  state_ = HANDSHAKE;

  if (master_) {
    EnterCriticalSection(&handshake_timeout_cs_);
    handshake_timeout_.reset(new TimerQueueTimer(this));
    handshake_timeout_->Start(kHandshakeTimeoutInMS);
    LeaveCriticalSection(&handshake_timeout_cs_);
    return true;
  }

  // we are the slave - so we first connect to the master
  if (!PerformSenderHandshake()) {
    state_ = DISCONNECTED;
    return false;
  }

  EnterCriticalSection(&handshake_timeout_cs_);
  handshake_timeout_.reset(new TimerQueueTimer(this));
  handshake_timeout_->Start(kHandshakeTimeoutInMS);
  LeaveCriticalSection(&handshake_timeout_cs_);

  return true;
}

//------------------------------------------------------------------------------
bool CommunicationChannel::Stop() {
  if (state_ == IDLE) {
    return false;
  }

  Thread::Task task(std::bind(&CommunicationChannel::StopOnWorkerThread, this));
  return thread_->PostTask(task);
}

//------------------------------------------------------------------------------

bool CommunicationChannel::StopNow(DWORD timeout /*= 0*/) {
  if (state_ == IDLE) {
    return false;
  }

  if (!thread_.get()) {
    return false;
  }
  return thread_->Stop(true, timeout);
}

//------------------------------------------------------------------------------
bool CommunicationChannel::Shutdown(DWORD timeout /*= INFINITE*/) {
  return StopNow(timeout);
}

//------------------------------------------------------------------------------
bool CommunicationChannel::Send(const uint8_t* data, size_t size) {
  if (!sender_ || !sender_->Valid()) {
    return false;
  }

  std::string buffer;
  buffer.assign(data, data + size);
  Thread::Task task(std::bind(&CommunicationChannel::SendOnWorkerThread,
                              this,
                              buffer));
  return thread_->PostTask(task);
}

//------------------------------------------------------------------------------
// virtual
void CommunicationChannel::OnDisconnected() {
  if (delegate_) {
    delegate_->OnDisconnected();
  }

  if (thread_.get()) {
    thread_->Stop();
  }
}

//------------------------------------------------------------------------------
// virtual
void CommunicationChannel::OnData(uint8_t* data, size_t size) {
  if ((nullptr == data) || (0 == size)) {
    return;
  }

  if (HandleHandshake(data, size)) {
    return;
  }

  if (state_ != CONNECTED) {
    return;
  }

  if (!delegate_) {
    return;
  }

  delegate_->OnData(data, size);
}

//------------------------------------------------------------------------------
// virtual
void CommunicationChannel::OnTimer(TimerQueueTimer* timer) {
  // stop timer
  EnterCriticalSection(&handshake_timeout_cs_);
  handshake_timeout_.reset(nullptr);
  LeaveCriticalSection(&handshake_timeout_cs_);

  // disconnect
  state_ = DISCONNECTED;
  receiver_.reset(nullptr); // will trigger |OnDisconnect|
  sender_.reset(nullptr);
}

//------------------------------------------------------------------------------
void CommunicationChannel::Init(const char* receiver_id,
                                const char* sender_id) {
  receiver_.reset(new Receiver(receiver_id, this));
  sender_.reset(new Sender(sender_id));
  thread_.reset(new Thread);
}

//------------------------------------------------------------------------------
bool CommunicationChannel::PerformSenderHandshake() {
  if (!sender_->Open()) {
    return false;
  }

  if (!Send((uint8_t*)&kHandshakeSignature, sizeof(kHandshakeSignature))) {
    state_ = IDLE;
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------
bool CommunicationChannel::HandleHandshake(const uint8_t* data, size_t size) {
  if (state_ != HANDSHAKE) {
    return false;
  }

  if (sizeof(kHandshakeSignature) != size) {
    return false;
  }

  uint32_t id = *((uint32_t*)data);
  if (id != kHandshakeSignature) {
    // is someone tampering with us?
    state_ = DISCONNECTED;
    receiver_.reset(nullptr); // will trigger |OnDisconnect|
    return true;
  }

  // handle master-side of handshake
  if (master_ && !PerformSenderHandshake()) {
    state_ = DISCONNECTED;
    receiver_.reset(nullptr); // will trigger |OnDisconnect|
    return true;
  }

  state_ = CONNECTED;
  EnterCriticalSection(&handshake_timeout_cs_);
  if (handshake_timeout_.get()) {
    handshake_timeout_->Stop();
  }
  LeaveCriticalSection(&handshake_timeout_cs_);
  if (delegate_) {
    delegate_->OnConnected();
  }
  return true;
}

//------------------------------------------------------------------------------
void CommunicationChannel::SendOnWorkerThread(std::string data) {
  int res = sender_->Write(data.c_str(), data.size());
  if (delegate_ && res != 0) { // not ok
    delegate_->OnSendDataError(data, res);
  }
}

//------------------------------------------------------------------------------
void CommunicationChannel::StopOnWorkerThread() {
  try {
    receiver_->Stop();
  } catch(...) {
  }

  try {
    sender_->Close();
  } catch(...) {
  }

  thread_->Stop(true);
}

//------------------------------------------------------------------------------
void CommunicationChannel::GetChannelIds(const char* channel_id,
                                         bool master,
                                         /*OUT*/std::string& receiver,
                                         /*OUT*/std::string& sender) {
  char master_channel_id[1024] = { 0 };
  char slave_channel_id[1024] = { 0 };
  sprintf_s(master_channel_id, 1024, kChannelIdMasterFormat, channel_id);
  sprintf_s(slave_channel_id, 1024, kChannelIdSlaveFormat, channel_id);

  if (master) {
    receiver = master_channel_id;
    sender = slave_channel_id;
  } else {
    receiver = slave_channel_id;
    sender = master_channel_id;
  }
}