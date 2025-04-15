/*
 * Copyright (c) 2023 Lain Bailey <lain@obsproject.com>
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
#include <windows.h>

#include "platform.h"
#include "bmem.h"
#include "pipe.h"


#include <stdbool.h>
#include <stdint.h>


struct ipc_pipe_server {
	OVERLAPPED overlap;
	HANDLE handle;
	HANDLE ready_event;
	HANDLE stop_event;
	HANDLE thread;
	HANDLE exit_event;
	uint32_t thread_id;

	uint8_t *read_data;
	size_t size;
	size_t capacity;

	ipc_pipe_read_t read_callback;
	void *param;
	bool exit;
};

struct ipc_pipe_client {
	HANDLE handle;
};

static inline bool ipc_pipe_client_valid(ipc_pipe_client_t *pipe)
{
	return pipe->handle != NULL && pipe->handle != INVALID_HANDLE_VALUE;
}


struct os_process_pipe {
	bool read_pipe;
	HANDLE handle;
	HANDLE handle_err;
	HANDLE process;
};

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

static inline bool create_process(const char *cmd_line, HANDLE stdin_handle,
				  HANDLE stdout_handle, HANDLE stderr_handle,
				  HANDLE *process)
{
	PROCESS_INFORMATION pi = {0};
	wchar_t *cmd_line_w = NULL;
	STARTUPINFOW si = {0};
	bool success = false;

	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_FORCEOFFFEEDBACK;
	si.hStdInput = stdin_handle;
	si.hStdOutput = stdout_handle;
	si.hStdError = stderr_handle;

	DWORD flags = 0;
#ifndef SHOW_SUBPROCESSES
	flags = CREATE_NO_WINDOW;
#endif

	os_utf8_to_wcs_ptr(cmd_line, 0, &cmd_line_w);
	if (cmd_line_w) {
		success = !!CreateProcessW(NULL, cmd_line_w, NULL, NULL, true,
					   flags, NULL, NULL, &si, &pi);

		if (success) {
			*process = pi.hProcess;
			CloseHandle(pi.hThread);
		} else {
			// Not logging the full command line is intentional
			// as it may contain stream keys etc.
			blog(LOG_ERROR, "CreateProcessW failed: %lu",
			     GetLastError());
		}

		bfree(cmd_line_w);
	}

	return success;
}

os_process_pipe_t *os_process_pipe_create(const char *cmd_line,
					  const char *type)
{
	os_process_pipe_t *pp = NULL;
	bool read_pipe;
	HANDLE process;
	HANDLE output;
	HANDLE err_input, err_output;
	HANDLE input;
	bool success;

	if (!cmd_line || !type) {
		return NULL;
	}
	if (*type != 'r' && *type != 'w') {
		return NULL;
	}
	if (!create_pipe(&input, &output)) {
		return NULL;
	}

	if (!create_pipe(&err_input, &err_output)) {
		return NULL;
	}

	read_pipe = *type == 'r';

	success = !!SetHandleInformation(read_pipe ? input : output,
					 HANDLE_FLAG_INHERIT, false);
	if (!success) {
		goto error;
	}

	success = !!SetHandleInformation(err_input, HANDLE_FLAG_INHERIT, false);
	if (!success) {
		goto error;
	}

	success = create_process(cmd_line, read_pipe ? NULL : input,
				 read_pipe ? output : NULL, err_output,
				 &process);
	if (!success) {
		goto error;
	}

	pp = bmalloc(sizeof(*pp));

	pp->handle = read_pipe ? input : output;
	pp->read_pipe = read_pipe;
	pp->process = process;
	pp->handle_err = err_input;

	CloseHandle(read_pipe ? output : input);
	CloseHandle(err_output);
	return pp;

error:
	CloseHandle(output);
	CloseHandle(input);
	return NULL;
}

int os_process_pipe_destroy(os_process_pipe_t *pp)
{
	int ret = 0;

	if (pp) {
		DWORD code;

		CloseHandle(pp->handle);
		CloseHandle(pp->handle_err);

		WaitForSingleObject(pp->process, INFINITE);
		if (GetExitCodeProcess(pp->process, &code))
			ret = (int)code;

		CloseHandle(pp->process);
		bfree(pp);
	}

	return ret;
}

size_t os_process_pipe_read(os_process_pipe_t *pp, uint8_t *data, size_t len)
{
	DWORD bytes_read;
	bool success;

	if (!pp) {
		return 0;
	}
	if (!pp->read_pipe) {
		return 0;
	}

	success = !!ReadFile(pp->handle, data, (DWORD)len, &bytes_read, NULL);
	if (success && bytes_read) {
		return bytes_read;
	}

	return 0;
}

size_t os_process_pipe_read_err(os_process_pipe_t *pp, uint8_t *data,
				size_t len)
{
	DWORD bytes_read;
	bool success;

	if (!pp || !pp->handle_err) {
		return 0;
	}

	success =
		!!ReadFile(pp->handle_err, data, (DWORD)len, &bytes_read, NULL);
	if (success && bytes_read) {
		return bytes_read;
	} else
		bytes_read = GetLastError();

	return 0;
}

size_t os_process_pipe_write(os_process_pipe_t *pp, const uint8_t *data,
			     size_t len)
{
	DWORD bytes_written;
	bool success;

	if (!pp) {
		return 0;
	}
	if (pp->read_pipe) {
		return 0;
	}

	success =
		!!WriteFile(pp->handle, data, (DWORD)len, &bytes_written, NULL);
	if (success && bytes_written) {
		return bytes_written;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// ow

#pragma warning(disable : 4013)
#pragma warning(disable : 4047)
#define IPC_PIPE_BUF_SIZE 8024

static inline bool ipc_pipe_internal_create_events(ipc_pipe_server_t *pipe)
{
	HANDLE ready_event = CreateEvent(NULL, false, false, NULL);
	HANDLE stop_event = CreateEvent(NULL, false, false, NULL);
	const bool success = ready_event && stop_event;
	if (!success) {
		if (ready_event) {
			CloseHandle(ready_event);
			ready_event = NULL;
		}
		if (stop_event) {
			CloseHandle(stop_event);
			stop_event = NULL;
		}
	}
	pipe->ready_event = ready_event;
	pipe->stop_event = stop_event;
	return success;
}

static inline void *create_full_access_security_descriptor()
{
	void *sd = malloc(SECURITY_DESCRIPTOR_MIN_LENGTH);
	if (!sd) {
		return NULL;
	}

	if (!InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION)) {
		goto error;
	}

	if (!SetSecurityDescriptorDacl(sd, true, NULL, false)) {
		goto error;
	}

	return sd;

error:
	free(sd);
	return NULL;
}

static inline bool ipc_pipe_internal_create_pipe(ipc_pipe_server_t *pipe,
						 const char *name)
{
	SECURITY_ATTRIBUTES sa;
	char new_name[512];
	void *sd;
	const DWORD access = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
	const DWORD flags = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE |
			    PIPE_WAIT;

	strcpy_s(new_name, sizeof(new_name), "\\\\.\\pipe\\");
	strcat_s(new_name, sizeof(new_name), name);

	sd = create_full_access_security_descriptor();
	if (!sd) {
		return false;
	}

	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = sd;
	sa.bInheritHandle = false;

	pipe->handle = CreateNamedPipeA(new_name, access, flags, 1,
					IPC_PIPE_BUF_SIZE, IPC_PIPE_BUF_SIZE, 0,
					&sa);
	free(sd);

	return pipe->handle != INVALID_HANDLE_VALUE;
}

static inline void ipc_pipe_internal_ensure_capacity(ipc_pipe_server_t *pipe,
						     size_t new_size)
{
	if (pipe->capacity >= new_size) {
		return;
	}

	pipe->read_data = realloc(pipe->read_data, new_size);
	pipe->capacity = new_size;
}

static inline void ipc_pipe_internal_append_bytes(ipc_pipe_server_t *pipe,
						  uint8_t *bytes, size_t size)
{
	size_t new_size = pipe->size + size;
	ipc_pipe_internal_ensure_capacity(pipe, new_size);
	memcpy(pipe->read_data + pipe->size, bytes, size);
	pipe->size = new_size;
}

static inline bool ipc_pipe_internal_io_pending(void)
{
	return GetLastError() == ERROR_IO_PENDING;
}

static inline DWORD ipc_pipe_internal_server_thread(LPVOID param)
{
	ipc_pipe_server_t *pipe = param;
	uint8_t buf[IPC_PIPE_BUF_SIZE];

	/* wait for connection */
	DWORD wait = WaitForSingleObject(pipe->ready_event, INFINITE);
	if (wait != WAIT_OBJECT_0) {
		pipe->read_callback(pipe->param, NULL, 0);
		return 0;
	}

	const HANDLE handles[] = {pipe->ready_event, pipe->stop_event};
	for (;;) {
		DWORD bytes = 0;
		bool success;

		if (pipe->exit) {
			break;
		}

		success = !!ReadFile(pipe->handle, buf, IPC_PIPE_BUF_SIZE, NULL,
				     &pipe->overlap);
		if (!success && !ipc_pipe_internal_io_pending()) {
			break;
		}

		DWORD wait = WaitForMultipleObjects(2, handles,
						    FALSE, INFINITE);
		if (wait != WAIT_OBJECT_0) {
			break;
		}

		success = !!GetOverlappedResult(pipe->handle, &pipe->overlap,
						&bytes, true);
		if (!success || !bytes) {
			break;
		}

		ipc_pipe_internal_append_bytes(pipe, buf, (size_t)bytes);

		if (success) {
			pipe->read_callback(pipe->param, pipe->read_data,
					    pipe->size);
			pipe->size = 0;
		}
	}

	pipe->read_callback(pipe->param, NULL, 0);
	return 0;
}

static inline bool
ipc_pipe_internal_start_server_thread(ipc_pipe_server_t *pipe)
{
	DWORD thread_id = 0;
	pipe->thread = CreateThread(NULL, 0, ipc_pipe_internal_server_thread,
				    pipe, 0, &thread_id);
	pipe->thread_id = thread_id;
	return pipe->thread != NULL;
}

static inline bool
ipc_pipe_internal_wait_for_connection(ipc_pipe_server_t *pipe)
{
	bool success;

	pipe->overlap.hEvent = pipe->ready_event;
	success = !!ConnectNamedPipe(pipe->handle, &pipe->overlap);
	return success || (!success && ipc_pipe_internal_io_pending());
}

static inline bool ipc_pipe_internal_open_pipe(ipc_pipe_client_t *pipe,
					       const char *name)
{
	DWORD mode = PIPE_READMODE_MESSAGE;
	char new_name[512];

	strcpy_s(new_name, sizeof(new_name), "\\\\.\\pipe\\");
	strcat_s(new_name, sizeof(new_name), name);

	pipe->handle = CreateFileA(new_name, GENERIC_READ | GENERIC_WRITE, 0,
				   NULL, OPEN_EXISTING, 0, NULL);
	if (pipe->handle == INVALID_HANDLE_VALUE) {
		return false;
	}

	return !!SetNamedPipeHandleState(pipe->handle, &mode, NULL, NULL);
}

/* ------------------------------------------------------------------------- */

bool ipc_pipe_server_start(ipc_pipe_server_t *pipe, const char *name,
			   ipc_pipe_read_t read_callback, void *param)
{
	pipe->read_callback = read_callback;
	pipe->param = param;

	if (!ipc_pipe_internal_create_events(pipe)) {
		goto error;
	}
	if (!ipc_pipe_internal_create_pipe(pipe, name)) {
		goto error;
	}
	if (!ipc_pipe_internal_wait_for_connection(pipe)) {
		goto error;
	}
	if (!ipc_pipe_internal_start_server_thread(pipe)) {
		goto error;
	}

	return true;

error:
	ipc_pipe_server_free(pipe);
	return false;
}

void ipc_pipe_server_free(ipc_pipe_server_t *pipe)
{
	ipc_pipe_server_free2(pipe, INFINITE);
}

void ipc_pipe_server_free2(ipc_pipe_server_t *pipe, uint32_t timeout)
{
	if (!pipe)
		return;

	if (pipe->stop_event) {
		if (pipe->handle) {
			if (pipe->thread) {
				CancelIoEx(pipe->handle, &pipe->overlap);
				SetEvent(pipe->stop_event);
				pipe->exit = true;
				WaitForSingleObject(pipe->thread, timeout);
				CloseHandle(pipe->thread);
			}
			CloseHandle(pipe->handle);
		}
		CloseHandle(pipe->stop_event);
		CloseHandle(pipe->ready_event);
	}

	free(pipe->read_data);
	memset(pipe, 0, sizeof(*pipe));
}

bool ipc_pipe_client_open(ipc_pipe_client_t *pipe, const char *name)
{
	if (!ipc_pipe_internal_open_pipe(pipe, name)) {
		ipc_pipe_client_free(pipe);
		return false;
	}

	return true;
}

void ipc_pipe_client_free(ipc_pipe_client_t *pipe)
{
	if (!pipe)
		return;

	if (pipe->handle)
		CloseHandle(pipe->handle);

	memset(pipe, 0, sizeof(*pipe));
}

bool ipc_pipe_client_write(ipc_pipe_client_t *pipe, const void *data,
			   size_t size)
{
	DWORD bytes;

	if (!pipe) {
		return false;
	}

	if (!pipe->handle || pipe->handle == INVALID_HANDLE_VALUE) {
		return false;
	}

	return !!WriteFile(pipe->handle, data, (DWORD)size, &bytes, NULL);
}
