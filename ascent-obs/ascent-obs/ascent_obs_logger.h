#ifndef ASCENTOBS_LOGGER_H_
#define ASCENTOBS_LOGGER_H_
#include <obs-data.h>
#include <obs.hpp>
#include <fstream>

class ASCENTOBSLogger {
public:
  ASCENTOBSLogger();
  virtual ~ASCENTOBSLogger();

  static bool log_verbose;
  static bool unfiltered_log;

private:
  void create_log_file();

private:
  std::fstream log_file_;
};
#endif //ASCENTOBS_LOGGER_H_
