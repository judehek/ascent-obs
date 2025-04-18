#include <windows.h>
#include <obs-data.h>
#include <obs.hpp>
#include <obs-audio-controls.h>

#include "obs_control/obs_utils.h"
#include "obs_control/obs.h"

#include "command_line.h"
#include "server.h"
#include "ascent_obs_logger.h"

//-----------------------------------------------------------------------------
extern "C" {
  // http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
  _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;

  // http://developer.amd.com/community/blog/2015/10/02/amd-enduro-system-for-developers/
  __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

//-----------------------------------------------------------------------------
enum PROCESS_DPI_AWARENESS {
  PROCESS_DPI_UNAWARE = 0,
  PROCESS_SYSTEM_DPI_AWARE = 1,
  PROCESS_PER_MONITOR_DPI_AWARE = 2
};

typedef HRESULT(__stdcall *SetProcessDpiAwareness_func)(_In_ int value);

//-----------------------------------------------------------------------------
bool SetDPIAware() {
  auto handle_shcore = LoadLibrary(L"Shcore.dll");

  if (!handle_shcore) {
    return false;
  }
  
  auto dpiAware = GetProcAddress(handle_shcore, "SetProcessDpiAwareness");
  
  if (!dpiAware) {
    return false;
  }

  HRESULT hRes = (SetProcessDpiAwareness_func(dpiAware))(PROCESS_PER_MONITOR_DPI_AWARE);

  if (hRes != S_OK) {
    return false;
  }

  return true;
}

DECLARE_HANDLE(OBS_DPI_AWARENESS_CONTEXT);
#define OBS_DPI_AWARENESS_CONTEXT_UNAWARE ((OBS_DPI_AWARENESS_CONTEXT)-1)
#define OBS_DPI_AWARENESS_CONTEXT_SYSTEM_AWARE ((OBS_DPI_AWARENESS_CONTEXT)-2)
#define OBS_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE \
  ((OBS_DPI_AWARENESS_CONTEXT)-3)
#define OBS_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 \
  ((OBS_DPI_AWARENESS_CONTEXT)-4)

static bool SetHighDPIv2Scaling() {
  static BOOL(WINAPI * func)(OBS_DPI_AWARENESS_CONTEXT) = nullptr;
  func = reinterpret_cast<decltype(func)>(GetProcAddress(
      GetModuleHandleW(L"USER32"), "SetProcessDpiAwarenessContext"));
  if (!func) {
    return false;
  }

  return !!func(OBS_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

//-----------------------------------------------------------------------------
static void load_debug_privilege(void) {
  const DWORD flags = TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY;
  TOKEN_PRIVILEGES tp;
  HANDLE token;
  LUID val;

  if (!OpenProcessToken(GetCurrentProcess(), flags, &token)) {
    return;
  }

  if (!!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &val)) {
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = val;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    AdjustTokenPrivileges(token, false, &tp, sizeof(tp), NULL, NULL);
  }

  if (!!LookupPrivilegeValue(NULL, SE_INC_BASE_PRIORITY_NAME, &val)) {
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = val;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(token, false, &tp, sizeof(tp), NULL, NULL)) {
      blog(LOG_INFO,
           "Could not set privilege to "
           "increase GPU priority");
    }
  }

  CloseHandle(token);
}


//-----------------------------------------------------------------------------
int main() {
  //MessageBoxA(NULL, "WinMain", "ascent-obs", MB_OK);

  int ret = 0;
  SetErrorMode(SEM_FAILCRITICALERRORS);

  {
    // from obs
    const HMODULE hRtwq = LoadLibrary(L"RTWorkQ.dll");
    if (hRtwq) {
      typedef HRESULT(STDAPICALLTYPE * PFN_RtwqStartup)();
      PFN_RtwqStartup func =
          (PFN_RtwqStartup)GetProcAddress(hRtwq, "RtwqStartup");
      func();
    }

    CommandLine::Init();
    ASCENTOBSLogger logger;

    if (!SetHighDPIv2Scaling()) {
      SetDPIAware();
    }

    load_debug_privilege();
  
    ret = Server::Run(CommandLine::ForCurrentProcess());

#ifdef _WIN32
    if (hRtwq) {
      typedef HRESULT(STDAPICALLTYPE * PFN_RtwqShutdown)();
      PFN_RtwqShutdown func =
          (PFN_RtwqShutdown)GetProcAddress(hRtwq, "RtwqShutdown");
      func();
      FreeLibrary(hRtwq);
    }
#endif
    base_set_log_handler(nullptr, nullptr);
  }

  return ret;
}
