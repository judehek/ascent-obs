/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2017 Overwolf Ltd.
*******************************************************************************/
#ifndef LIBASCENTOBS_COMMUNICATIONS_SENDER_H_
#define LIBASCENTOBS_COMMUNICATIONS_SENDER_H_

#include <memory>
#include <string>

#include "../base/primitives.h"
#include "../base/macros.h"
struct ipc_pipe_client;

namespace libowobs {

class Sender {
public:
  Sender(const char* channel_id);
  virtual ~Sender();

  bool Open();
  bool Close();
  bool Valid();
  int Write(const void *data, size_t size);

private:
  std::string channel_id_;
  std::unique_ptr<struct ipc_pipe_client> pipe_client_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Sender);
};

};

#endif // LIBASCENTOBS_COMMUNICATIONS_SENDER_H_