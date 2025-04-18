/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2017 Overwolf Ltd.
*******************************************************************************/
#ifndef LIBASCENTOBS_COMMUNICATIONS_COMMUNICATION_CHANNEL_H_
#define LIBASCENTOBS_COMMUNICATIONS_COMMUNICATION_CHANNEL_H_

#include <memory>

#include "../base/primitives.h"
#include "../base/thread.h"
#include "../base/timer_queue_timer.h"

#include "receiver.h"
#include "sender.h"
#include "protocol.h"
#include "communication_channel_delegate.h"

namespace libowobs {
//------------------------------------------------------------------------------
class CommunicationChannel : public ICommunicationChannel,
                             public ReceiverDelegate,
                             public TimerQueueTimerDelegate {
  public:
  // master = true if you are the master and want to control the slave
  static CommunicationChannel* Create(const char* channel_id,
                                      bool master,
                                      CommunicationChannelDelegate* delegate);
  virtual ~CommunicationChannel();

  static std::string GenerateRandomChannelId();

  virtual bool Start() override;
  virtual bool Stop() override;
  virtual bool StopNow(DWORD timeout = 0) override;
  virtual bool Send(const uint8_t* data, size_t size) override;
  virtual bool Shutdown(DWORD timeout = INFINITE);

  void SetDelegate(CommunicationChannelDelegate* delegate) {
    delegate_ = delegate;
  }
  
  // ReceiverDelegate
public:
  virtual void OnDisconnected();
  virtual void OnData(uint8_t* data, size_t size);

// TimerQueueTimerDelegate
public:
  virtual void OnTimer(TimerQueueTimer* timer);

private:
  CommunicationChannel(bool master, CommunicationChannelDelegate* delegate);
  void Init(const char* receiver_id, const char* sender_id);
  bool PerformSenderHandshake();
  bool HandleHandshake(const uint8_t* data, size_t size);

  void SendOnWorkerThread(std::string data);
  void StopOnWorkerThread();

  static void GetChannelIds(const char* channel_id,
                            bool master,
                            /*OUT*/std::string& receiver,
                            /*OUT*/std::string& sender);

private:
  enum State { IDLE, HANDSHAKE, CONNECTED, DISCONNECTED } state_;
  bool master_;
  CommunicationChannelDelegate* delegate_;

  std::unique_ptr<Receiver> receiver_;
  std::unique_ptr<Sender> sender_;


  CRITICAL_SECTION handshake_timeout_cs_;
  std::unique_ptr<TimerQueueTimer> handshake_timeout_;
  std::unique_ptr<Thread> thread_;

  DISALLOW_COPY_AND_ASSIGN(CommunicationChannel);
};

};

#endif // LIBASCENTOBS_COMMUNICATIONS_COMMUNICATION_CHANNEL_H_