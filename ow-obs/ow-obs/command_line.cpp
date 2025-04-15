// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "command_line.h"

#include <cctype>
#include <locale>
#include <ostream>
#include <algorithm>
#include <functional>

#include <windows.h>
#include <shellapi.h>

#include "base/macros.h"

CommandLine* CommandLine::current_process_commandline_ = nullptr;

namespace {

const CommandLine::CharType kSwitchTerminator[] = L"--";
const CommandLine::CharType kSwitchValueSeparator[] = L"=";

// Since we use a lazy match, make sure that longer versions (like "--") are
// listed before shorter versions (like "-") of similar prefixes.
// By putting slash last, we can control whether it is treaded as a switch
// value by changing the value of switch_prefix_count to be one less than
// the array size.
const CommandLine::CharType* const kSwitchPrefixes[] = {L"--", L"-", L"/"};
size_t switch_prefix_count = arraysize(kSwitchPrefixes);

// trim from start (in place)
static inline void ltrim(CommandLine::StringType &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(),
    std::not1(std::ptr_fun<int, int>(std::isspace))));
}

// trim from end (in place)
static inline void rtrim(CommandLine::StringType &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
    std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(CommandLine::StringType &s) {
  ltrim(s);
  rtrim(s);
}

// Test to see if a set or map contains a particular key.
// Returns true if the key is in the collection.
template <typename Collection, typename Key>
bool ContainsKey(const Collection& collection, const Key& key) {
  return collection.find(key) != collection.end();
}


size_t GetSwitchPrefixLength(const CommandLine::StringType& string) {
  for (size_t i = 0; i < switch_prefix_count; ++i) {
    CommandLine::StringType prefix(kSwitchPrefixes[i]);
    if (string.compare(0, prefix.length(), prefix) == 0)
      return prefix.length();
  }
  return 0;
}

// Fills in |switch_string| and |switch_value| if |string| is a switch.
// This will preserve the input switch prefix in the output |switch_string|.
bool IsSwitch(const CommandLine::StringType& string,
              CommandLine::StringType* switch_string,
              CommandLine::StringType* switch_value) {
  switch_string->clear();
  switch_value->clear();
  size_t prefix_length = GetSwitchPrefixLength(string);
  if (prefix_length == 0 || prefix_length == string.length())
    return false;

  const size_t equals_position = string.find(kSwitchValueSeparator);
  *switch_string = string.substr(0, equals_position);
  if (equals_position != CommandLine::StringType::npos)
    *switch_value = string.substr(equals_position + 1);
  return true;
}

// Append switches and arguments, keeping switches before arguments.
void AppendSwitchesAndArguments(CommandLine* command_line,
                                const CommandLine::StringVector& argv) {
  bool parse_switches = true;
  for (size_t i = 1; i < argv.size(); ++i) {
    CommandLine::StringType arg = argv[i];
    trim(arg);

    CommandLine::StringType switch_string;
    CommandLine::StringType switch_value;
    parse_switches &= (arg != kSwitchTerminator);
    if (parse_switches && IsSwitch(arg, &switch_string, &switch_value)) {
      command_line->AppendSwitchNative(switch_string,
                                       switch_value);
    } else {
      command_line->AppendArgNative(arg);
    }
  }
}

// Quote a string as necessary for CommandLineToArgvW compatiblity *on Windows*.
std::wstring QuoteForCommandLineToArgvW(const std::wstring& arg,
                                        bool quote_placeholders) {
  // We follow the quoting rules of CommandLineToArgvW.
  // http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
  std::wstring quotable_chars(L" \\\"");
  // We may also be required to quote '%', which is commonly used in a command
  // line as a placeholder. (It may be substituted for a string with spaces.)
  if (quote_placeholders)
    quotable_chars.push_back(L'%');
  if (arg.find_first_of(quotable_chars) == std::wstring::npos) {
    // No quoting necessary.
    return arg;
  }

  std::wstring out;
  out.push_back(L'"');
  for (size_t i = 0; i < arg.size(); ++i) {
    if (arg[i] == '\\') {
      // Find the extent of this run of backslashes.
      size_t start = i, end = start + 1;
      for (; end < arg.size() && arg[end] == '\\'; ++end) {}
      size_t backslash_count = end - start;

      // Backslashes are escapes only if the run is followed by a double quote.
      // Since we also will end the string with a double quote, we escape for
      // either a double quote or the end of the string.
      if (end == arg.size() || arg[end] == '"') {
        // To quote, we need to output 2x as many backslashes.
        backslash_count *= 2;
      }
      for (size_t j = 0; j < backslash_count; ++j)
        out.push_back('\\');

      // Advance i to one before the end to balance i++ in loop.
      i = end - 1;
    } else if (arg[i] == '"') {
      out.push_back('\\');
      out.push_back('"');
    } else {
      out.push_back(arg[i]);
    }
  }
  out.push_back('"');

  return out;
}

}  // namespace

CommandLine::CommandLine(NoProgram no_program)
    : argv_(1),
      begin_args_(1) {
  UNREFERENCED_PARAMETER(no_program);
}

CommandLine::CommandLine(const std::wstring& program)
    : argv_(1),
      begin_args_(1) {
  SetProgram(program);
}

CommandLine::CommandLine(int argc, const CommandLine::CharType* const* argv)
    : argv_(1),
      begin_args_(1) {
  InitFromArgv(argc, argv);
}

CommandLine::CommandLine(const StringVector& argv)
    : argv_(1),
      begin_args_(1) {
  InitFromArgv(argv);
}

CommandLine::CommandLine(const CommandLine& other)
    : argv_(other.argv_),
      switches_(other.switches_),
      begin_args_(other.begin_args_) {
}

CommandLine& CommandLine::operator=(const CommandLine& other) {
  argv_ = other.argv_;
  switches_ = other.switches_;
  begin_args_ = other.begin_args_;
  return *this;
}

CommandLine::~CommandLine() {
}

// static
void CommandLine::set_slash_is_not_a_switch() {
  // The last switch prefix should be slash, so adjust the size to skip it.
  switch_prefix_count = arraysize(kSwitchPrefixes) - 1;
}

// static
void CommandLine::InitUsingArgvForTesting(int argc, const wchar_t* const* argv) {
  current_process_commandline_ = new CommandLine(NO_PROGRAM);
  // On Windows we need to convert the command line arguments to string16.
  CommandLine::StringVector argv_vector;
  for (int i = 0; i < argc; ++i)
    argv_vector.push_back(argv[i]);
  current_process_commandline_->InitFromArgv(argv_vector);
}

// static
bool CommandLine::Init() {
  if (current_process_commandline_) {
    // If this is intentional, Reset() must be called first. If we are using
    // the shared build mode, we have to share a single object across multiple
    // shared libraries.
    return false;
  }

  current_process_commandline_ = new CommandLine(NO_PROGRAM);
  current_process_commandline_->ParseFromString(::GetCommandLineW());

  return true;
}

// static
void CommandLine::Reset() {
  delete current_process_commandline_;
  current_process_commandline_ = nullptr;
}

// static
CommandLine* CommandLine::ForCurrentProcess() {
  return current_process_commandline_;
}

// static
bool CommandLine::InitializedForCurrentProcess() {
  return !!current_process_commandline_;
}

// static
CommandLine CommandLine::FromString(const std::wstring& command_line) {
  CommandLine cmd(NO_PROGRAM);
  cmd.ParseFromString(command_line);
  return cmd;
}

void CommandLine::InitFromArgv(int argc,
                               const CommandLine::CharType* const* argv) {
  StringVector new_argv;
  for (int i = 0; i < argc; ++i)
    new_argv.push_back(argv[i]);
  InitFromArgv(new_argv);
}

void CommandLine::InitFromArgv(const StringVector& argv) {
  argv_ = StringVector(1);
  switches_.clear();
  begin_args_ = 1;
  SetProgram(argv.empty() ? L"" : argv[0]);
  AppendSwitchesAndArguments(this, argv);
}

std::wstring CommandLine::GetProgram() const {
  return argv_[0];
}

void CommandLine::SetProgram(const std::wstring& program) {
  std::wstring program_copy = program;
  trim(program_copy);
  argv_[0] = program_copy;
}

bool CommandLine::HasSwitch(const std::wstring& switch_string) const {
  return ContainsKey(switches_, switch_string);
}

std::string CommandLine::GetSwitchValueASCII(
  const std::wstring& switch_string) const {
  auto result = switches_.find(switch_string);
  return result == switches_.end() ?
    "" : CommandLine::UnicodeToASCII(result->second);
}

CommandLine::StringType CommandLine::GetSwitchValueNative(
    const std::wstring& switch_string) const {
  auto result = switches_.find(switch_string);
  return result == switches_.end() ? StringType() : result->second;
}

void CommandLine::AppendSwitchNative(const std::wstring& switch_string,
                                     const CommandLine::StringType& value) {
  std::wstring switch_key = switch_string;
  std::transform(switch_key.begin(), switch_key.end(), switch_key.begin(),
    [](unsigned char c) { return std::tolower(c); });

  StringType combined_switch_string(switch_key);

  size_t prefix_length = GetSwitchPrefixLength(combined_switch_string);
  auto insertion =
      switches_.insert(make_pair(switch_key.substr(prefix_length), value));
  if (!insertion.second)
    insertion.first->second = value;
  // Preserve existing switch prefixes in |argv_|; only append one if necessary.
  if (prefix_length == 0)
    combined_switch_string = kSwitchPrefixes[0] + combined_switch_string;
  if (!value.empty())
    combined_switch_string += kSwitchValueSeparator + value;
  // Append the switch and update the switches/arguments divider |begin_args_|.
  argv_.insert(argv_.begin() + begin_args_++, combined_switch_string);
}

CommandLine::StringVector CommandLine::GetArgs() const {
  // Gather all arguments after the last switch (may include kSwitchTerminator).
  StringVector args(argv_.begin() + begin_args_, argv_.end());
  // Erase only the first kSwitchTerminator (maybe "--" is a legitimate page?)
  StringVector::iterator switch_terminator =
      std::find(args.begin(), args.end(), kSwitchTerminator);
  if (switch_terminator != args.end())
    args.erase(switch_terminator);
  return args;
}


void CommandLine::AppendArgNative(const CommandLine::StringType& value) {
  argv_.push_back(value);
}

void CommandLine::AppendArguments(const CommandLine& other,
                                  bool include_program) {
  if (include_program)
    SetProgram(other.GetProgram());
  AppendSwitchesAndArguments(this, other.argv());
}

void CommandLine::ParseFromString(const std::wstring& command_line) {
  std::wstring command_line_string = command_line;
  trim(command_line_string);
  if (command_line_string.empty())
    return;

  int num_args = 0;
  wchar_t** args = NULL;
  args = ::CommandLineToArgvW(command_line_string.c_str(), &num_args);

  InitFromArgv(num_args, args);
  LocalFree(args);
}

CommandLine::StringType CommandLine::GetCommandLineStringInternal(
    bool quote_placeholders) const {
  StringType string(argv_[0]);
  string = QuoteForCommandLineToArgvW(string, quote_placeholders);

  StringType params(GetArgumentsStringInternal(quote_placeholders));
  if (!params.empty()) {
    string.append(StringType(L" "));
    string.append(params);
  }
  return string;
}

CommandLine::StringType CommandLine::GetArgumentsStringInternal(
    bool quote_placeholders) const {
  StringType params;
  // Append switches and arguments.
  bool parse_switches = true;
  for (size_t i = 1; i < argv_.size(); ++i) {
    StringType arg = argv_[i];
    StringType switch_string;
    StringType switch_value;
    parse_switches &= arg != kSwitchTerminator;
    if (i > 1)
      params.append(StringType(L" "));
    if (parse_switches && IsSwitch(arg, &switch_string, &switch_value)) {
      params.append(switch_string);
      if (!switch_value.empty()) {
        switch_value =
            QuoteForCommandLineToArgvW(switch_value, quote_placeholders);
        params.append(kSwitchValueSeparator + switch_value);
      }
    } else {
      arg = QuoteForCommandLineToArgvW(arg, quote_placeholders);
      params.append(arg);
    }
  }
  return params;
}

std::string CommandLine::UnicodeToASCII(const std::wstring& wstr) {
  if (wstr.empty()) {
    return "";
  }

  int size_needed = WideCharToMultiByte(
    CP_ACP, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);

  std::string strTo(size_needed, 0);
  WideCharToMultiByte(
    CP_ACP,
    0,
    &wstr[0],
    (int)wstr.size(),
    &strTo[0],
    size_needed,
    nullptr,
    nullptr);
  return strTo;
}
