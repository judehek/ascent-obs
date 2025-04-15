/*******************************************************************************
* Overwolf OBS Display
*
* Copyright (c) 2018 Overwolf Ltd.
*******************************************************************************/
#include <obs.h>
#include <obs.hpp>

#include <windows.h>
#include <base/macros.h>
#include "obs_control/scene/source.h"
#include <memory>

class DisplayContext;

namespace obs_control {

class OBSDisplayTester {
public: 
  enum TestSouceType : uint32_t {
    TestSouceTypeNone = 0,
    TestSouceTypeGame = 1,
    TestSouceTypeMonitor = 2
  };

  class Delegate {
    public:
      virtual void OnBlackTextureDetected(TestSouceType type) = 0;
      virtual void OnColoredTextedDetected(TestSouceType type) = 0;
      virtual Source* GetSource(TestSouceType source_type) = 0;
  };

public:
  OBSDisplayTester(Delegate* delegate);
  virtual ~OBSDisplayTester();

  void Register(TestSouceType test_type);
  void Unregister();
  void ResetTest(TestSouceType test_type);
 

private:
  friend void RenderWindow(void *data, uint32_t cx, uint32_t cy);
  
  void OnRender(uint32_t cx, uint32_t cy);

  void Reset();
  
private:
  Delegate* delegate_;

  bool is_register_;
  uint32_t test_completed_;

  bool test_black_texture_;

  uint64_t next_black_test_time_stamp;
  uint64_t black_texture_detection_counter;

  gs_stagesurf_t* black_texture_tester_;

  std::unique_ptr<DisplayContext> display_context_;

  CRITICAL_SECTION display_critical_section_;
  DISALLOW_COPY_AND_ASSIGN(OBSDisplayTester);

}; // class OBSDisplayTester
}; // namespace obs_control
