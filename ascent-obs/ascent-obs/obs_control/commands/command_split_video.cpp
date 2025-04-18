/*******************************************************************************
* Overwolf OBS Controller
*
* Copyright (c) 2019 Overwolf Ltd.
*******************************************************************************/
#include "obs_control/commands/command_split_video.h"
#include "obs_control/obs_utils.h"
#include "obs_control/obs.h"
#include "obs_control/settings.h"

using namespace obs_control;
using namespace libowobs;
using namespace settings;

//------------------------------------------------------------------------------
CommandSplitVideo::CommandSplitVideo(OBS* obs,
  OBSControlCommunications* communications) :
  Command(obs, communications) {}

//------------------------------------------------------------------------------
CommandSplitVideo::~CommandSplitVideo() {

}

//------------------------------------------------------------------------------
// virtual 
void CommandSplitVideo::Perform(int identifier, OBSData& data) {
  UNUSED_PARAMETER(identifier);
  UNUSED_PARAMETER(data);
  __super::obs_->SplitVideo();
}
