#include "obs_qa_flags.h"
#include "util/windows/win-registry.h"

//-----------------------------------------------------------------------------

#define OBS_SUB_KEY   "SOFTWARE\\OverwolfQA\\OBS"
#define ENCODER_CODE  "EncoderCode"

//-----------------------------------------------------------------------------
OBSQAFlags* OBSQAFlags::Instance() {
  static OBSQAFlags obs_qa_flags;
  return &obs_qa_flags;
}

//-----------------------------------------------------------------------------
OBSQAFlags::OBSQAFlags() {

}

//-----------------------------------------------------------------------------
OBSQAFlags::~OBSQAFlags() {

}

//-----------------------------------------------------------------------------
bool OBSQAFlags::GetOverrideCodeFromRegistry(std::string& code) {
  struct reg_sz reg = { 0 };

  if (!get_reg_string(HKEY_CURRENT_USER, OBS_SUB_KEY, ENCODER_CODE, &reg)) {
    return false;
  }

  code = reg.return_value;

  return true;
}

