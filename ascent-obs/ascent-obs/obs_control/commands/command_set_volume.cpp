
#include "obs_control/commands/command_set_volume.h"
#include "obs_control/obs_utils.h"
#include "obs_control/obs.h"
#include "obs_control/settings.h"

using namespace obs_control;
using namespace libowobs;
using namespace settings;

//------------------------------------------------------------------------------
CommandSetVolume::CommandSetVolume(OBS* obs,
                                   OBSControlCommunications* communications) :
  Command(obs, communications) {
}

//------------------------------------------------------------------------------
CommandSetVolume::~CommandSetVolume() {

}

//------------------------------------------------------------------------------
// virtual
void CommandSetVolume::Perform(int identifier, OBSData& data) {
  UNUSED_PARAMETER(identifier);
  // audio settings
  SET_OBS_DATA(audio_settings, obs_data_get_obj(data, kSettingsAudio));
  __super::obs_->audio_control()->SetVolume(audio_settings);
}
