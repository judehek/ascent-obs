#include "server.h"
#include "obs_control/obs_utils.h"
#include "command_line.h"
#include "obs-config.h"

#include <communications\communication_channel_std.h>
#include <communications\communication_channel.h>

    //-----------------------------------------------------------------------------

namespace {
const wchar_t kCmdLineParamChannel[] = L"channel";
const wchar_t kCmdLineParamDebuggerAttach[] = L"debugger-attach";
};

using namespace libascentobs;

namespace {
void PrintFileInfo() {
  char file_path[2048] = {0};
  if (GetModuleFileNameA(NULL, file_path, 2048) == 0) {
    return;
  }
  blog(LOG_INFO, "running from: %s", file_path);
  blog(LOG_INFO, "obs version: %d.%d.%d", 
    LIBOBS_API_MAJOR_VER, LIBOBS_API_MINOR_VER, LIBOBS_API_PATCH_VER);
}
}  // namespace

//-----------------------------------------------------------------------------
// static
int Server::Run(CommandLine* options) {
  {
    PrintFileInfo();
  
    blog(LOG_INFO, "Initializing server");
    Server server;
    if (!server.Init(options)) {
      blog(LOG_ERROR, "ascent-obs initialization error");
      return -1;
    }

    server.Run();
  }
  return 0;
}

//-----------------------------------------------------------------------------
Server::Server() {
  obs_control_.reset(new obs_control::OBSControl());
}

//-----------------------------------------------------------------------------
Server::~Server() {
  obs_control_.reset();
}

//-----------------------------------------------------------------------------
// virtual
void Server::OnConnected() {
  DEBUG_PRINT("Server OnConnected\n");
  if (!obs_control_->Init(this)) {
    message_loop_.Quit();
  }
}

//-----------------------------------------------------------------------------
// virtual
void Server::OnDisconnected() {
  DEBUG_PRINT("Server OnDisconnected\n");
  blog(LOG_WARNING, "Server disconnected, Terminating");
  message_loop_.Quit();

#ifdef DEBUG
  return;
#else
  // we terminate our self -> make sure we are not hang on shutdown
  HANDLE hnd =
      OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, TRUE, GetCurrentProcessId());
  TerminateProcess(hnd, 0);
#endif  // DEBUG
}

//-----------------------------------------------------------------------------
// virtual
void Server::OnData(const uint8_t* data, size_t size) {
  try {
    std::string buffer((const char*)data, ((const char*)data) + size);

    DEBUG_PRINT("Server OnData\n", buffer.c_str());

#if _DEBUG
    blog(LOG_INFO, "command:\n %s", buffer.c_str());
#endif

    CREATE_OBS_DATA(request);
    request = obs_data_create_from_json(buffer.c_str());
    if (!obs_data_has_user_value(request, protocol::kCommandField)) {
      return;
    }

    int identifier = 0;
    if (obs_data_has_user_value(request, protocol::kCommandIdentifier)) {
      identifier = (int)obs_data_get_int(request, protocol::kCommandIdentifier);
    }

    int command = (int)obs_data_get_int(request, protocol::kCommandField);
    if (HandleShutdownCommand(command)) {
      return;
    }

    obs_control_->HandleCommand(command, identifier, request);
  } catch (...) {
  }
}

//-----------------------------------------------------------------------------
void Server::OnSendDataError(const std::string& data, int error_code) {
  DEBUG_PRINT("Server OnSendDataError\n", data.c_str());
  blog(LOG_ERROR, "Send data error [%d] : %s", error_code, data.c_str());
}

//-----------------------------------------------------------------------------
// virtual
void Server::Send(int command_id) {
  CREATE_OBS_DATA(data);
  Send(command_id, data);
}

//-----------------------------------------------------------------------------
// virtual
void Server::Send(int command_id, OBSData& data) {
  obs_data_set_int(data, protocol::kEventField, command_id);
  std::string buffer = obs_data_get_json(data);

  communications_->Send((uint8_t*)buffer.c_str(), buffer.length());
}

//-----------------------------------------------------------------------------
// virtual
void Server::Shutdown() {
  message_loop_.Quit();
}

//-----------------------------------------------------------------------------
// virtual
bool Server::Init(CommandLine* options) {
  if (options->HasSwitch(kCmdLineParamDebuggerAttach)) {
    ::MessageBox(NULL,
                 L"ascent-obs debugger attach message",
                 L"DebuggerAttach",
                 MB_OK);
  }
  std::string channel_id = options->GetSwitchValueASCII(kCmdLineParamChannel);
  if (!channel_id.empty()) {
    blog(LOG_INFO, "Channel: %s", channel_id.c_str());
    communications_.reset(libascentobs::CommunicationChannel::Create(
        channel_id.c_str(),
        false,   // master - false (we are the slave)
        this));  // delegate
  
  } else {
    blog(LOG_INFO, "Channel std");
    communications_.reset(libascentobs::CommunicationChannelStd::Create(
        false,   // master - false (we are the slave)
        this));  // delegate
  }

  if (!communications_) {
    blog(LOG_ERROR, "Fail to create communication channel");
    return false;
  }

  if (!communications_->Start(true)) {
    blog(LOG_ERROR, "Fail to start communication");
    return false;
  }

  return true;
}

//-----------------------------------------------------------------------------
void Server::Run() {
  message_loop_.Run();
}

//-----------------------------------------------------------------------------
bool Server::HandleShutdownCommand(int command) {
  if (protocol::commands::SHUTDOWN != command) {
    return false;
  }

  blog(LOG_INFO, "shut down command");

  if (obs_control_.get()) {
    obs_control_->Shutdown();
  }

  // give pending messages chance to sent
  if (communications_.get() && !communications_->StopNow(5000)) {
      blog(LOG_WARNING, "communications stop timeout");
  }
  message_loop_.Quit();
  communications_->Shutdown();

  return true;
}
