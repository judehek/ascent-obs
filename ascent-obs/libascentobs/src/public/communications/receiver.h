
#ifndef LIBASCENTOBS_COMMUNICATIONS_RECEIVER_H_
#define LIBASCENTOBS_COMMUNICATIONS_RECEIVER_H_

#include <memory>
#include <string>

#include "../base/primitives.h"
#include "../base/macros.h"
struct ipc_pipe_server;

namespace libowobs {

struct ReceiverDelegate {
  virtual void OnDisconnected() = 0;
  virtual void OnData(uint8_t *data, size_t size) = 0;
};

class Receiver {
public:
  Receiver(const char* channel_id, ReceiverDelegate* delegate);
  virtual ~Receiver();

  bool Start();
  bool Stop();

private:
  static void IpcPipeRead(void *param, uint8_t *data, size_t size);

private:
  std::string channel_id_;
  ReceiverDelegate* delegate_;
  std::unique_ptr<struct ipc_pipe_server> pipe_server_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Receiver);
};

};

#endif // LIBASCENTOBS_COMMUNICATIONS_RECEIVER_H_