/******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2017 Overwolf Ltd.
******************************************************************************/

#ifndef OWOBS_SERVER_H_
#define OWOBS_SERVER_H_

//-----------------------------------------------------------------------------

#include <memory>
#include <base/macros.h>
#include <communications/communication_channel_delegate.h>
#include "obs_control/obs_control.h"
#include "message_loop.h"

//-----------------------------------------------------------------------------

class CommandLine;

//-----------------------------------------------------------------------------

class Server : public libowobs::CommunicationChannelDelegate,
                      obs_control::OBSControlCommunications {
public:

  static int Run(CommandLine* options);
  virtual ~Server();

// communications::CommunicationChannelDelegate
private:

  virtual void OnConnected();
  virtual void OnDisconnected();
  virtual void OnData(const uint8_t* data, size_t size);
  virtual void OnSendDataError(const std::string& data, int error_code);

// BSControlCommunications
private:

  virtual void Send(int command_id, OBSData& data);
  virtual void Send(int command_id);
  virtual void Shutdown();

private:

  Server();

  bool Init(CommandLine* options);
  void Run();
  bool HandleShutdownCommand(int command);

private:

  std::unique_ptr<libowobs::ICommunicationChannel> communications_;
  MessageLoop message_loop_;
  std::unique_ptr<obs_control::OBSControl> obs_control_;

  DISALLOW_COPY_AND_ASSIGN(Server);
};

#endif // OWOBS_SERVER_H_