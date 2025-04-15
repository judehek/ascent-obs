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

#pragma once

#include <windows.h>
#include "base/primitives.h"

//-----------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#elif _MSC_VER
#ifndef inline
#define inline __inline
#endif
#endif

//-----------------------------------------------------------------------------

struct os_process_pipe {
  HANDLE handle_read;
  HANDLE handle_write;
  HANDLE handle_err;
  HANDLE process;
  uint32_t process_id;
};

typedef struct os_process_pipe os_process_pipe_t;

//-----------------------------------------------------------------------------

os_process_pipe_t *os_process_pipe_create(const wchar_t *cmd_line,
                                          const wchar_t* command_line = NULL);

os_process_pipe_t *os_process_pipe_connect(HANDLE handle_read,HANDLE handle_write);

int os_process_pipe_destroy(os_process_pipe_t *pp, DWORD timeout = INFINITE);

size_t os_process_pipe_write(os_process_pipe_t *pp, const uint8_t *data, size_t len);

size_t os_process_pipe_read(os_process_pipe_t *pp, uint8_t *data, size_t len);

size_t os_process_pipe_read_err(os_process_pipe_t *pp, uint8_t *data, size_t len);

//-----------------------------------------------------------------------------

struct ipc_pipe_server;
struct ipc_pipe_client;

typedef struct ipc_pipe_server ipc_pipe_server_t;
typedef struct ipc_pipe_client ipc_pipe_client_t;

typedef void(*ipc_pipe_read_t)(void *param, uint8_t *data, size_t size);

bool ipc_pipe_server_start(ipc_pipe_server_t *pipe, const char *name,
  ipc_pipe_read_t read_callback, void *param);

void ipc_pipe_server_free(ipc_pipe_server_t *pipe);
void ipc_pipe_server_free2(ipc_pipe_server_t *pipe,uint32_t timeout);

bool ipc_pipe_client_open(ipc_pipe_client_t *pipe, const char *name);
void ipc_pipe_client_free(ipc_pipe_client_t *pipe);
int ipc_pipe_client_write(ipc_pipe_client_t *pipe, const void *data,
  size_t size);
static inline bool ipc_pipe_client_valid(ipc_pipe_client_t *pipe);

#ifdef _WIN32
#include "pipe-windows.h"
#endif

#ifdef __cplusplus
}
#endif
