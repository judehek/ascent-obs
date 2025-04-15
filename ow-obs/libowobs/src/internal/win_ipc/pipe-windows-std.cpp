/*
 * Copyright (c) 2014 Hugh Bailey <obs.jim@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#define WIN32_LEAN_AND_MEAN

//-----------------------------------------------------------------------------

#include <windows.h>
#include "platform.h"
#include <stdlib.h>
#include "pipe.h"

//-----------------------------------------------------------------------------
static bool create_pipe(HANDLE *input, HANDLE *output) 
{
	SECURITY_ATTRIBUTES sa = {0};

	sa.nLength = sizeof(sa);
	sa.bInheritHandle = true;

	if (!CreatePipe(input, output, &sa, 0)) {
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
static inline bool create_process(const wchar_t *full_file_path,
                                  const wchar_t* command_line,
                                  HANDLE stdin_handle,
				                          HANDLE stdout_handle,
                                  HANDLE stderr_handle,
				                          HANDLE *process,
                                  uint32_t* process_id)
{
  if (!full_file_path) {
    return false;
  }
	PROCESS_INFORMATION pi = {0};
	STARTUPINFOW si = {0};
	bool success = false;

	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_FORCEOFFFEEDBACK;
	si.hStdInput = stdin_handle;
	si.hStdOutput= stdout_handle;
	si.hStdError = stderr_handle;

	DWORD flags = 0;

#ifndef SHOW_SUBPROCESSES
	flags = CREATE_NO_WINDOW;
#endif

  wchar_t full_command[1024] = { 0 };
  if (nullptr == command_line) {
    swprintf_s(full_command, 1024, L"\"%s\"", full_file_path);
  } else {
    swprintf_s(full_command, 1024, L"\"%s\" %s", full_file_path, command_line);
  }

	success = !!CreateProcessW(nullptr, full_command,
    NULL, NULL, true, flags, NULL, NULL, &si, &pi);

	if (success) {
		*process = pi.hProcess;
    *process_id = pi.dwProcessId;
		CloseHandle(pi.hThread);
	}

	return success;
}

//-----------------------------------------------------------------------------
os_process_pipe_t *os_process_pipe_create(const wchar_t *path,
                                          const wchar_t* command_line)
{
	os_process_pipe_t *pp = NULL;
	
	HANDLE process = NULL;
  HANDLE err_input = NULL, err_output = NULL;
  HANDLE child_std_in_rd = NULL, child_std_in_wr = NULL;
  HANDLE child_std_out_rd = NULL, child_std_out_wr = NULL;

  uint32_t process_id = 0;

	bool success = false;

	if (!path) {
		return NULL;
	}

	if (!create_pipe(&err_input, &err_output)) {
		return NULL;
	}

  success = !!SetHandleInformation(err_input, HANDLE_FLAG_INHERIT, false);
  if (!success) {
    goto error;
  }

  if (!create_pipe(&child_std_in_rd, &child_std_in_wr)) {
    return NULL;
  }

	success = !!SetHandleInformation(child_std_in_wr,HANDLE_FLAG_INHERIT, false);
	if (!success) {
		goto error;
	}

  if (!create_pipe(&child_std_out_rd, &child_std_out_wr)) {
    return NULL;
  }

  success = !!SetHandleInformation(child_std_out_rd,HANDLE_FLAG_INHERIT,false);
  if (!success) {
    goto error;
  }

	success = create_process(path,
                           command_line,
                           child_std_in_rd, 
                           child_std_out_wr,
				                   err_output, 
                           &process,
                           &process_id);
	if (!success) {
		goto error;
	}

	pp = (os_process_pipe_t*)malloc(sizeof(*pp));

	pp->handle_write = child_std_in_wr;
  pp->handle_read  = child_std_out_rd;
	pp->process      = process;
	pp->handle_err   = err_input;
  pp->process_id   = process_id;

	CloseHandle(child_std_in_rd);
  CloseHandle(child_std_out_wr);
	CloseHandle(err_output);

	return pp;

error:
	CloseHandle(child_std_in_wr);
	CloseHandle(child_std_in_rd);
  CloseHandle(child_std_out_wr);
  CloseHandle(child_std_out_rd);
	return NULL;
}

//-----------------------------------------------------------------------------
os_process_pipe_t *os_process_pipe_connect(HANDLE handle_read,HANDLE handle_write)
{
  os_process_pipe_t *pp = NULL;

  pp = (os_process_pipe_t*)malloc(sizeof(*pp));

  pp->handle_read = handle_read;
  pp->handle_write= handle_write;
  pp->process    = NULL;
  pp->handle_err = NULL;

  return pp;
}

//-----------------------------------------------------------------------------
int os_process_pipe_destroy(os_process_pipe_t *pp, DWORD timeout /*= INFINITE*/) 
{
	int ret = 0;

	if (pp) {
		DWORD code = 0;

    if (WAIT_TIMEOUT == WaitForSingleObject(pp->process, timeout)) {
      TerminateProcess(pp->process, code);
    } else if (GetExitCodeProcess(pp->process, &code)) {
      ret = (int)code;
    }

    if (pp->handle_read) {
      CloseHandle(pp->handle_read);
      pp->handle_read = NULL;
    }
    if (pp->handle_write) {
      CloseHandle(pp->handle_write);
      pp->handle_write = NULL;
    }

    if (pp->handle_err) {
      CloseHandle(pp->handle_err);
      pp->handle_err = NULL;
    }

		CloseHandle(pp->process);
		free(pp);
	}

	return ret;
}

//-----------------------------------------------------------------------------
size_t os_process_pipe_read(os_process_pipe_t *pp, uint8_t *data, size_t len) 
{
	if (!pp) {
		return 0;
	}

  DWORD bytes_read = 0;
  bool success = false;

	success = !!ReadFile(pp->handle_read, data, (DWORD)len, &bytes_read, NULL);

	if (success && bytes_read) {
		return bytes_read;
	}

	return 0;
}

//-----------------------------------------------------------------------------
size_t os_process_pipe_read_err(os_process_pipe_t *pp,uint8_t *data,size_t len)
{
	if (!pp || !pp->handle_err) {
		return 0;
	}

  DWORD bytes_read = 0;
  bool success = false;

	success = !!ReadFile(pp->handle_err, data, (DWORD)len, &bytes_read, NULL);

	if (success && bytes_read) {
		return bytes_read;
  } else {
    int err = GetLastError();
  }

	return 0;
}

//-----------------------------------------------------------------------------
size_t os_process_pipe_write(os_process_pipe_t *pp,const uint8_t *data,size_t len) 
{
	if (!pp) {
		return 0;
	}

  DWORD bytes_written = 0;
  bool success = false;

	success = !!WriteFile(pp->handle_write, data, (DWORD)len, &bytes_written, NULL);

 	if (success && bytes_written > 0) {
    FlushFileBuffers(pp->handle_write);
		return bytes_written;
  } else {
    int err = GetLastError(); // ERROR_IO_PENDING 
  }

	return 0;
}
