/*******************************************************************************
* Overwolf OBS Logger
*
* Copyright (c) 2017 Overwolf Ltd.
*******************************************************************************/
#ifndef OWOBS_LOGGER_H_
#define OWOBS_LOGGER_H_
#include <obs-data.h>
#include <obs.hpp>
#include <fstream>

class OWOBSLogger {
public:
  OWOBSLogger();
  virtual ~OWOBSLogger();

  static bool log_verbose;
  static bool unfiltered_log;

private:
  void create_log_file();

private:
  std::fstream log_file_;
};
#endif //OWOBS_LOGGER_H_
