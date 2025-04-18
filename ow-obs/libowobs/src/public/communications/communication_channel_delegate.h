/*******************************************************************************
* communication channel delegate
*
* Copyright (c) 2021 Overwolf Ltd.
*******************************************************************************/

#ifndef LIBASCENTOBS_COMMUNICATIONS_COMMUNICATION_CHANNEL_DELEGATE_H_
#define LIBASCENTOBS_COMMUNICATIONS_COMMUNICATION_CHANNEL_DELEGATE_H_

#include <string>
#include <windows.h>

namespace libowobs {
  struct CommunicationChannelDelegate;

  class ICommunicationChannel {
   public:
    virtual ~ICommunicationChannel(){};
    virtual bool Start() = 0;
    virtual bool Start(bool /*comInitialize*/) { return Start(); }
    virtual bool Send(const uint8_t* data, size_t size) = 0;
    virtual bool Stop() = 0;
    virtual bool StopNow(DWORD timeout = 0) = 0;
    virtual bool Shutdown(DWORD timeout = INFINITE) = 0;
    virtual uint32_t GetProcessID() { return 0; }
    virtual void SetDelegate(CommunicationChannelDelegate* /*delegate*/) {}
  };


  struct CommunicationChannelDelegate {
    virtual void OnConnected() = 0;
    virtual void OnDisconnected() = 0;
    virtual void OnData(const uint8_t* data, size_t size) = 0;
    virtual void OnSendDataError(const std::string& data, int error_code) = 0;
  };

};

#endif // LIBASCENTOBS_COMMUNICATIONS_COMMUNICATION_CHANNEL_DELEGATE_H_