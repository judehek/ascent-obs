#ifndef ASCENT_OBS_MESSAGE_LOOP_H_
#define ASCENT_OBS_MESSAGE_LOOP_H_

#include <memory>
#include <thread>
#include <condition_variable>

class MessageLoop {
public:
  MessageLoop();
  virtual ~MessageLoop();

public:
  void Run();
  void Quit();

private:
  std::mutex access_mutex_;
  std::condition_variable conditional_variable_;

  // NOTE: it is important to set this to false in the "dtor" otherwise we
  // might end up calling the |conditional_variable_| even after the dtor was
  // called
  bool running_;

};

#endif // ASCENT_OBS_MESSAGE_LOOP_H_