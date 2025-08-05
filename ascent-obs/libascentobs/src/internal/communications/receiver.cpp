
#include "communications/receiver.h"
#include "internal/win_ipc/pipe.h"

namespace libascentobs {
};

using namespace libascentobs;

//------------------------------------------------------------------------------
Receiver::Receiver(const char* channel_id, ReceiverDelegate* delegate) :
  channel_id_(channel_id),
  delegate_(delegate) {
}

//------------------------------------------------------------------------------
// virtual 
Receiver::~Receiver() {
  Stop();
}

//------------------------------------------------------------------------------
bool Receiver::Start() {
  if (channel_id_.empty()) {
    return false;
  }

  ipc_pipe_server_t* pipe = new ipc_pipe_server_t;
  if (nullptr == pipe) {
    return false;
  }

  memset(pipe, 0, sizeof(ipc_pipe_server_t));
  pipe_server_.reset(pipe);
  if (pipe_server_ == nullptr) {
    return false;
  }

  bool success = ipc_pipe_server_start(pipe_server_.get(), 
                                       channel_id_.c_str(),
                                       Receiver::IpcPipeRead,
                                       (void*)delegate_);

  return success;
}

//------------------------------------------------------------------------------
bool Receiver::Stop() {
  if (!pipe_server_) {
    return false;
  }

  ipc_pipe_server_free(pipe_server_.get());
  return true;
}

//------------------------------------------------------------------------------
void Receiver::IpcPipeRead(void *param, uint8_t *data, size_t size) {
  ReceiverDelegate* delegate = (ReceiverDelegate*)param;

  if (nullptr == delegate) {
    return;
  }

  if ((nullptr == data) && (0 == size)) {
    delegate->OnDisconnected();
    return;
  }

  delegate->OnData(data, size);
}

