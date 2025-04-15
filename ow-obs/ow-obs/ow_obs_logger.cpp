/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#include "ow_obs_logger.h"
#include <time.h>
#include <stdio.h>
#include <wchar.h>
#include <wchar.h>
#include <fstream>
#include <string>
#include <mutex>
#include <sstream>
#include "util//lexer.h"
#include <util/util.hpp>
#include <util/platform.h>
#include <windows.h>
#include <dbghelp.h>
#include "command_line.h"
#include "switches.h"

#define LOG_FOLDER_PATH "Overwolf/Log/"
#define CRASHDUMP_FOLDER_PATH "Overwolf/Log/../CrashDumps/ow-obs/"
#define LOG_FILE_PREFIX "ow_obs_"
#define MAX_LOGS_COUNT 10

#define MAX_REPEATED_LINES 30
#define MAX_CHAR_VARIATION (255 * 3)

#define MAX_CRASH_REPORT_SIZE (150 * 1024)


bool OWOBSLogger::log_verbose = false;
bool OWOBSLogger::unfiltered_log = false;

using namespace std;

static wstring log_file_path;


struct BaseLexer {
  lexer lex;
public:
  inline BaseLexer() { lexer_init(&lex); }
  inline ~BaseLexer() { lexer_free(&lex); }
  operator lexer*() { return &lex; }
};

void do_log(int log_level, const char *msg, va_list args, void *param);

string GenerateTimeDateFilename(const char *extension, bool noSpace = true) {
  time_t    now = time(0);
  char      file[256] = {};
  struct tm *cur_time;

  bool is_secondary =
    CommandLine::ForCurrentProcess()->HasSwitch(switches::kCommandSecondary);

  cur_time = localtime(&now);
  snprintf(file, sizeof(file), "%s%d-%02d-%02d%c%02d-%02d-%02d-p%d%s.%s",
    LOG_FILE_PREFIX,
    cur_time->tm_year + 1900,
    cur_time->tm_mon + 1,
    cur_time->tm_mday,
    noSpace ? '_' : ' ',
    cur_time->tm_hour,
    cur_time->tm_min,
    cur_time->tm_sec,
    GetCurrentProcessId(),
    is_secondary ? "_secondary" :"",
    extension);

  return string(file);
}

char *GetConfigPathPtr(const char *name) {
  return os_get_local_config_path_ptr(name);
}

const char* GetLogLevelstr(int log_level) {
  switch (log_level) {
    case LOG_ERROR:
      return "(ERROR)";
    case LOG_WARNING:
      return "(WARNING)";
    case LOG_DEBUG:
      return "(DEBUG)";
    case LOG_INFO:
    default:
      return "(INFO)";
  }
}

string CurrentTimeString() {
  using namespace std::chrono;

  struct tm  tstruct;
  char       buf[80];

  auto tp = system_clock::now();
  auto now = system_clock::to_time_t(tp);
  tstruct = *localtime(&now);

  size_t written = strftime(buf, sizeof(buf), "%X", &tstruct);
  if (ratio_less<system_clock::period, seconds::period>::value &&
    written && (sizeof(buf) - written) > 5) {
    auto tp_secs =
      time_point_cast<seconds>(tp);
    auto millis =
      duration_cast<milliseconds>(tp - tp_secs).count();

    snprintf(buf + written, sizeof(buf) - written, ".%03u",
      static_cast<unsigned>(millis));
  }

  return buf;
}

static inline int sum_chars(const char *str) {
  int val = 0;
  for (; *str != 0; str++)
    val += *str;

  return val;
}

static inline bool too_many_repeated_entries(fstream &logFile, const char *msg,
  const char *output_str) {
  static mutex log_mutex;
  static const char *last_msg_ptr = nullptr;
  static int last_char_sum = 0;
  static char cmp_str[4096];
  static int rep_count = 0;

  int new_sum = sum_chars(output_str);

  lock_guard<mutex> guard(log_mutex);

  if (OWOBSLogger::unfiltered_log) {
    return false;
  }

  if (last_msg_ptr == msg) {
    int diff = std::abs(new_sum - last_char_sum);
    if (diff < MAX_CHAR_VARIATION) {
      return (rep_count++ >= MAX_REPEATED_LINES);
    }
  }

  if (rep_count > MAX_REPEATED_LINES) {
    logFile << CurrentTimeString() <<
      ": Last log entry repeated for " <<
      to_string(rep_count - MAX_REPEATED_LINES) <<
      " more lines" << endl;
  }

  last_msg_ptr = msg;
  strcpy(cmp_str, output_str);
  last_char_sum = new_sum;
  rep_count = 0;

  return false;
}

static inline void LogString(fstream &logFile, const char *timeString,
  char *str) {
  logFile << timeString << str << endl;
}

static inline void LogStringChunk(int log_level, fstream &logFile, char *str) {
  char *nextLine = str;
  std::stringstream timeString;
  //stream << std::hex << your_int;

  timeString << CurrentTimeString();
  string log_level_str = GetLogLevelstr(log_level);
  timeString << log_level_str;
  timeString << "[" << std::hex << GetCurrentThreadId() << "]";
  timeString << ": ";

  while (*nextLine) {
    nextLine = strchr(str, '\n');
    if (!nextLine)
      break;

    if (nextLine != str && nextLine[-1] == '\r') {
      nextLine[-1] = 0;
    } else {
      nextLine[0] = 0;
    }

    LogString(logFile, timeString.str().c_str(), str);
    nextLine++;
    str = nextLine;
  }

  LogString(logFile, timeString.str().c_str(), str);
}

void do_log(int log_level, const char *msg, va_list args, void *param) {
  fstream &logFile = *static_cast<fstream*>(param);
  char str[4096];

#ifndef _WIN32
  va_list args2;
  va_copy(args2, args);
#endif

  vsnprintf(str, 4095, msg, args);

#ifdef _WIN32
  if (IsDebuggerPresent()) {
    int wNum = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    if (wNum > 1) {
      static wstring wide_buf;
      static mutex wide_mutex;

      lock_guard<mutex> lock(wide_mutex);
      wide_buf.reserve(wNum + 1);
      wide_buf.resize(wNum - 1);
      MultiByteToWideChar(CP_UTF8, 0, str, -1, &wide_buf[0],
        wNum);
      wide_buf.push_back('\n');

      OutputDebugStringW(wide_buf.c_str());
    }
  }
#else
  def_log_handler(log_level, msg, args2, nullptr);
#endif

  if (log_level <= LOG_INFO || OWOBSLogger::log_verbose) {
    if (too_many_repeated_entries(logFile, msg, str))
      return;
    LogStringChunk(log_level, logFile, str);
  }

#if defined(_WIN32) && defined(OBS_DEBUGBREAK_ON_ERROR)
  if (log_level <= LOG_ERROR && IsDebuggerPresent())
    __debugbreak();
#endif
}

static void create_dump_file(PEXCEPTION_POINTERS exception) {
  if (log_file_path.empty())
    return;

  std::wstring dump_file_path = log_file_path;
  dump_file_path += L".dmp";

  HANDLE hFile = CreateFile(
    dump_file_path.c_str(),
    GENERIC_READ | GENERIC_WRITE,
    FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
    NULL,
    CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,
    NULL);

  MINIDUMP_EXCEPTION_INFORMATION mei;
  mei.ThreadId = GetCurrentThreadId();
  mei.ClientPointers = TRUE;
  mei.ExceptionPointers = exception;
  MiniDumpWriteDump(
    GetCurrentProcess(),
    GetCurrentProcessId(),
    hFile,
    MiniDumpNormal,
    &mei,
    NULL,
    NULL);
}

static void main_crash_handler(void* exception_ptr, const char *format, va_list args, void *param) {
  fstream &logFile = *static_cast<fstream*>(param);

  PEXCEPTION_POINTERS exception =
    static_cast<PEXCEPTION_POINTERS>(exception_ptr);

  logFile << "\n";
  logFile << "*****************************************************************" << "\n";
  logFile << "*********************** OW-OBS Crashed ********************" << "\n";
  logFile << "*****************************************************************" << "\n";

  char *text = new char[MAX_CRASH_REPORT_SIZE];

  vsnprintf(text, MAX_CRASH_REPORT_SIZE, format, args);
  text[MAX_CRASH_REPORT_SIZE - 1] = 0;
  logFile << text;

  if (exception == NULL)
    return;

  create_dump_file(exception);
}

OWOBSLogger::OWOBSLogger() {
  create_log_file();
  base_set_log_handler(do_log, &log_file_);
  base_set_crash_handler(main_crash_handler, &log_file_);
}

OWOBSLogger::~OWOBSLogger() {
  blog(LOG_INFO, "Number of memory leaks: %ld", bnum_allocs());
  base_set_log_handler(nullptr, nullptr);
}

void OWOBSLogger::create_log_file() {
  stringstream dst;

  string currentLogFile = GenerateTimeDateFilename("txt");
  dst << LOG_FOLDER_PATH << currentLogFile.c_str();

  BPtr<char> path(GetConfigPathPtr(dst.str().c_str()));
#ifdef _WIN32
  BPtr<wchar_t> wpath;
  os_utf8_to_wcs_ptr(path, 0, &wpath);

  log_file_path = wpath;

  log_file_.open(wpath,
    ios_base::in | ios_base::out | ios_base::trunc);
#else
  logFile.open(path,
    ios_base::in | ios_base::out | ios_base::trunc);
#endif

  if (log_file_.is_open()) {
    //delete_oldest_file(LOG_FOLDER_PATH);
    base_set_log_handler(do_log, &log_file_);
  } else {
    blog(LOG_ERROR, "Failed to open log file");
  }
}
