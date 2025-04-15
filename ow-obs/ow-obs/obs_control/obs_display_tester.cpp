#include "obs_control/obs_display_tester.h"


#include "obs_control/obs_utils.h"
#include "obs_control/scene/utils.h"
#include "obs_control/scene/monitor_source.h"
#include "obs_control/scene/game_capture_source.h"


namespace obs_control {
  #define GREY_COLOR_BACKGROUND 0xFF4C4C4C

  const int kMonitorTestBlackTextureIntevalMS = 250;
  const int kGameTestBlackTextureIntevalMS = 3000;

  const int kMonitorTestBlackTextureTimeout = 1000;
  const int kGameTestBlackTextureTimeout = 30000;

  const int kMonitorTestBlackTextureCount =
    kMonitorTestBlackTextureTimeout / kMonitorTestBlackTextureIntevalMS;

  const int kGameTestBlackTextureCount =
    kGameTestBlackTextureTimeout / kGameTestBlackTextureIntevalMS;

  uint32_t backgroundColor = GREY_COLOR_BACKGROUND;

  static HWND s_DisplayHWND = nullptr;
  HWND CreateDisplayWindow() {
    WNDCLASS wc;
    HINSTANCE instance = GetModuleHandle(NULL);
    memset(&wc, 0, sizeof(wc));
    wc.lpszClassName = TEXT("OW-OBS-DISPLAY");
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.hInstance = instance;
    wc.lpfnWndProc = (WNDPROC)DefWindowProc;

    if (!RegisterClass(&wc))
      return 0;

    return CreateWindow(TEXT("OW-OBS-DISPLAY"),
      TEXT("OW-OBS-DISPLAY-WINDOW"),
      WS_OVERLAPPEDWINDOW,
      0, 0, 120, 120,
      NULL, NULL, instance, NULL);
  }

  bool IsSupportedFormat(gs_color_format format) {
    const gs_color_format kTestBlackTextureFormats[] = {
        GS_RGBA, GS_BGRA, GS_RGBA_UNORM, GS_BGRX_UNORM, GS_BGRA_UNORM};

     return std::find(std::begin(kTestBlackTextureFormats),
                     std::end(kTestBlackTextureFormats),
                     format) != std::end(kTestBlackTextureFormats);
  }

  DisplayContext CreateDisplay() {
    if (s_DisplayHWND == nullptr) {
      s_DisplayHWND = CreateDisplayWindow();
    }

    RECT rc;
    GetClientRect(s_DisplayHWND, &rc);

    gs_init_data info = {};
    info.cx = rc.right  || 120;
    info.cy = rc.bottom || 120;
    info.format = GS_BGRA;
    info.zsformat = GS_ZS_NONE;
    info.window.hwnd = s_DisplayHWND;

    return obs_display_create(&info, backgroundColor);
  }

  //------------------------------------------------------------------------------
  void RenderWindow(void *data, uint32_t cx, uint32_t cy) {
    OBSDisplayTester* _this = static_cast<OBSDisplayTester*>(data);
    try {
      _this->OnRender(cx, cy);
    } catch (char *error) {
      blog(LOG_ERROR, "Display render test error: %s", error);
    }
  }

  }; // namespace obs_control

using namespace obs_control;

OBSDisplayTester::OBSDisplayTester(Delegate* delegate)
 :delegate_(delegate),
  is_register_(false),
  test_completed_(TestSouceTypeNone),
  test_black_texture_(false), 
  next_black_test_time_stamp(0),
  black_texture_detection_counter(0),
  black_texture_tester_(nullptr) { 

  InitializeCriticalSection(&display_critical_section_);
}

OBSDisplayTester::~OBSDisplayTester() {
  Unregister();
}

void OBSDisplayTester::Unregister() {
  if (!is_register_) {
    return;
  }

  blog(LOG_INFO, "stop display tester");
  EnterCriticalSection(&display_critical_section_);
  if (!is_register_) {
    LeaveCriticalSection(&display_critical_section_);
    return;
  }

  obs_display_remove_draw_callback(display_context_->get(),
    RenderWindow,
    this);

  is_register_ = false;
  LeaveCriticalSection(&display_critical_section_);
}

void OBSDisplayTester::ResetTest(TestSouceType test_type) {
  test_completed_ &= ~(test_type);
}

void OBSDisplayTester::Reset() {

  // reset
  test_black_texture_ = false;
  black_texture_detection_counter = 0;
  next_black_test_time_stamp = 0;

  if (black_texture_tester_) {
    gs_stagesurface_destroy(black_texture_tester_);
    black_texture_tester_ = nullptr;
  }
}

void OBSDisplayTester::Register(TestSouceType test_type) {  
  if (!display_context_.get()) {
    display_context_.reset(new DisplayContext(CreateDisplay()));
  }

  if ((test_type & test_completed_) == test_type) {
    blog(LOG_DEBUG, "skip test [%d]: already tested", test_type);
    return;
  }

  EnterCriticalSection(&display_critical_section_);
  blog(LOG_INFO, "Starting texture test [type:%d]", test_type);

  if (!is_register_) {
    Reset();
    test_black_texture_ = true;
    is_register_ = true;
    obs_display_add_draw_callback(
      display_context_->get(),
      RenderWindow, 
      this);
  }
  LeaveCriticalSection(&display_critical_section_);
}

void OBSDisplayTester::OnRender(uint32_t cx, uint32_t cy) {
  UNUSED_PARAMETER(cx);
  UNUSED_PARAMETER(cy);

  if (!test_black_texture_)
    return;

  auto game_source = static_cast<GameCaptureSource*>
    (delegate_->GetSource(OBSDisplayTester::TestSouceTypeGame));

  bool has_game_source = game_source != nullptr &&
      !game_source->compatibility_mode() &&
       game_source->IsVisible();

  if (has_game_source && 
     (!game_source->did_start_capture() ||
     !game_source->foreground())) {
    return;
  }

  auto monitor_source = static_cast<MonitorSource*>
    (delegate_->GetSource(OBSDisplayTester::TestSouceTypeMonitor));

  bool has_monitor =
    monitor_source &&
     monitor_source->IsVisible() &&
    !monitor_source->compatible_mode();

  if (!has_game_source && !has_monitor) {
    return;
  }

  auto texture = obs_render_main_texture();
  if (texture == NULL)
    return;

  uint32_t pitch;
  uint8_t *texture_data;
  auto format = gs_texture_get_color_format(texture);

  if (!IsSupportedFormat(format)) {
    blog(LOG_WARNING, "stop display color test, format is not supported [%d]",
         format);
    test_black_texture_ = false;
    return;
  }

  auto tx_width = gs_texture_get_width(texture);
  auto tx_height = gs_texture_get_height(texture);

  if (next_black_test_time_stamp == 0) {
    next_black_test_time_stamp = GetTickCount64() +
      (has_game_source ? 2000 : 1000);
  }

  if (!black_texture_tester_) {
    black_texture_tester_ = gs_stagesurface_create(
      tx_width, tx_height, GS_BGRA);

    if (black_texture_tester_ == nullptr) {
      blog(LOG_ERROR,
        "Display tester create copy texture [width:%d height%d format:%d] error. stop black tester",
        tx_width, tx_height, format);
      test_black_texture_ = false;
    } else {
      blog(LOG_INFO,
           "Display tester create copy texture [width:%d height%d format:%d] ",
           tx_width, tx_height, format);
    }
  }

  static int texture_map_failers = 0;
  if (texture_map_failers > 50)
    return;

  auto copy_texture = black_texture_tester_;

  if (copy_texture == nullptr)
    return;

  gs_stage_texture(copy_texture, texture);

    // we start to test for black texture in delay
  if (GetTickCount64() < next_black_test_time_stamp)
    return;

  if (!gs_stagesurface_map(copy_texture, &texture_data, &pitch)) {
    texture_map_failers++; // TODO: log
    return;
  }

  const int kMinColoredPixelCount = 500;
  uint32_t black_pixel = 0;
  uint32_t colored_pixel = 0;

  for (uint32_t y = 0; y < tx_height && colored_pixel < kMinColoredPixelCount; y++) {
    uint8_t *line_in = texture_data + pitch * y;
    for (uint32_t x1 = 0; x1 < tx_width && colored_pixel < kMinColoredPixelCount;) {
      if (line_in[x1] == 0 && line_in[x1 + 1] == 0 && line_in[x1 + 2] == 0) {
        black_pixel++;
      } else {
        colored_pixel++;
      }
      x1 = x1 + 4;
    }
  }

  gs_stagesurface_unmap(copy_texture);

  if (colored_pixel >= kMinColoredPixelCount) {
    blog(LOG_INFO, "found colored texture [%d]", colored_pixel);

    if (has_monitor) {
      test_completed_ |= OBSDisplayTester::TestSouceTypeMonitor;
    } else {
      test_completed_ |= OBSDisplayTester::TestSouceTypeGame;
    }
   
    Reset();

    delegate_->OnColoredTextedDetected(
      has_monitor ? 
        OBSDisplayTester::TestSouceTypeMonitor :
        OBSDisplayTester::TestSouceTypeGame);

    return;
  }

  const uint32_t max_tests_counter =
    has_monitor ? kMonitorTestBlackTextureCount : kGameTestBlackTextureCount;

  black_texture_detection_counter++;
  blog(LOG_WARNING,
    "black texture detected [colored:%d black:%d ratio:%f pitch:%d] (total: %d)",
    colored_pixel,
    black_pixel,
    (float)black_pixel / (float)(black_pixel + colored_pixel),
    pitch,
    black_texture_detection_counter);

  if (black_texture_detection_counter > max_tests_counter) {
    blog(LOG_ERROR, "black texture: shared texture [%d]?", black_texture_detection_counter);

    Reset();

    delegate_->OnBlackTextureDetected(
      has_monitor ?
      OBSDisplayTester::TestSouceTypeMonitor :
      OBSDisplayTester::TestSouceTypeGame);

    return;
  }

  next_black_test_time_stamp = (GetTickCount() +
    (has_monitor ? kMonitorTestBlackTextureIntevalMS : kGameTestBlackTextureIntevalMS));
  
}
