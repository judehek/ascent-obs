/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2017 Overwolf Ltd.
*******************************************************************************/
#include "communications/sender.h"
#include "internal/win_ipc/pipe.h"

namespace libowobs {
};

using namespace libowobs;

//------------------------------------------------------------------------------
Sender::Sender(const char* channel_id) :
  channel_id_(channel_id) {
}

//------------------------------------------------------------------------------
// virtual 
Sender::~Sender() {
   Close();
}

//------------------------------------------------------------------------------
bool Sender::Open() {
  if (channel_id_.empty()) {
    return false;
  }

  ipc_pipe_client_t* pipe = new ipc_pipe_client_t;
  if (nullptr == pipe) {
    return false;
  }

  memset(pipe, 0, sizeof(ipc_pipe_client_t));
  pipe_client_.reset(pipe);

  return ipc_pipe_client_open(pipe_client_.get(), channel_id_.c_str());
}

//------------------------------------------------------------------------------
bool Sender::Close() {
  if (!pipe_client_) {
    return false;
  }

  ipc_pipe_client_free(pipe_client_.get());
  return true;
}

//------------------------------------------------------------------------------
bool Sender::Valid() {
  if (!pipe_client_) {
    return false;
  }

  return ipc_pipe_client_valid(pipe_client_.get());
}

//------------------------------------------------------------------------------
int Sender::Write(const void *data, size_t size) {
  return ipc_pipe_client_write(pipe_client_.get(), data, size);
}