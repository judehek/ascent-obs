#include <windows.h>
#include "graphics-hook.h"

static HMODULE ow_handle = NULL;

typedef bool(__cdecl *callback_type1)(void *);
typedef bool(__cdecl *callback_type2)(void *, void*);


static inline void *get_proc(const char *name)
{
	return (void *)GetProcAddress(ow_handle, name);
}

static bool __cdecl ow_d3d9_capture(void *device, void *surface)
{
	return capture_d3d9(device, surface);
}

static bool __cdecl ow_d3d9_reset_hook()
{
	return reset_d3d9();
}

static bool __cdecl ow_d3d11_capture(void *swap_chain,
					  void *backbuffer_ptr)
{
	return capture_d3d11(swap_chain, backbuffer_ptr);
}

static bool __cdecl ow_d3d11_reset()
{
	return reset_d3d11();
}

static bool __cdecl ow_d3d12_capture(void *swap_chain, void *queue_ptr)
{
	return capture_d3d12(swap_chain, queue_ptr);
}

static bool __cdecl ow_d3d12_reset()
{
	return reset_d3d12();
}

static bool __cdecl ow_ogl_capture(HDC hdc)
{
	return capture_ogl(hdc);
}

static bool __cdecl ow_ogl_reset(HGLRC hrc)
{
	return reset_ogl(hrc);
}

static bool __cdecl ascent_vulkan_capture(void *queue, const void *info,
					 void *source)
{
	return capture_vulkan(queue, info, source);
}

static bool __cdecl ascent_vulkan_reset(void *device)
{
	return reset_vulkan(device);
}

bool hook_ow(void)
{
	if (!ow_handle) {
		ow_handle = GetModuleHandleW(L"owclient");
	}

	if (!ow_handle) {
		return false;
	}

	set_external_log_callback(
		static_cast<log_message_callback>(get_proc("write_log")));

	static_cast<callback_type2>(get_proc("set_d3d9_capture_callbacks"))(
		 &ow_d3d9_capture, &ow_d3d9_reset_hook);

	static_cast<callback_type2>(
		get_proc("set_d3d11_capture_callbacks"))(&ow_d3d11_capture,
							 &ow_d3d11_reset);
	static_cast<callback_type2>(
		get_proc("set_d3d12_capture_callbacks"))(&ow_d3d12_capture,
							 &ow_d3d12_reset);

	static_cast<callback_type2>(get_proc("set_ogl_capture_callbacks"))(
		&ow_ogl_capture, &ow_ogl_reset);

	static_cast<callback_type2>(get_proc("set_vulkan_capture_callbacks"))(
		&ascent_vulkan_capture, &ascent_vulkan_reset);

	return true;
}
