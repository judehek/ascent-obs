#include "obs_control/stream_output.h"

#include "obs_control/advanced_output.h"
#include "obs_control/obs_utils.h"
#include <communications/protocol.h>
#include <util/platform.h>

namespace obs_control {
  const char kErrorStreamAlreadyStart[] = "Stream out already started";
  const char kErrorStartCaptureGenericEncoderError[] = "failed to open encoder?";

  void OBSStreamStarting(void *data, calldata_t *params) {
    StreamOutput *output = static_cast<StreamOutput*>(data);
    obs_output_t *obj = (obs_output_t*)calldata_ptr(params, "output");

    output->OnStarted();
    output->StartAsDelay();
    int sec = (int)obs_output_get_active_delay(obj);
    blog(LOG_INFO, 
        "Starting Streaming [id:%d delay:%d]",
        output->identifier(),
        sec);

    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }

    output->advanced_output_
          ->delegate_
          ->OnStartingStreaming(output->identifier());
  }

  void OBSStreamStopping(void *data, calldata_t *params) {
    StreamOutput *output = static_cast<StreamOutput*>(data);
    obs_output_t *obj = (obs_output_t*)calldata_ptr(params, "output");
    int sec = (int)obs_output_get_active_delay(obj);

    blog(LOG_INFO,
      "Stopping streaming [id:%d delay:%d]",
      output->identifier(),
      sec);

    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }

    output->advanced_output_
          ->delegate_
          ->OnStoppingStreaming(output->identifier());

    if (output->delay_active_) {
      OBSStopStreaming(data, params);
    }
  }

  void OBSStopStreaming(void *data, calldata_t *params) {
    StreamOutput *output = static_cast<StreamOutput*>(data);
    int code = (int)calldata_int(params, "code");
    const char *last_error = calldata_string(params, "last_error");

    blog(LOG_INFO, 
         "Streaming stopped [id:%d]. code:%d error:%s",
         output->identifier(), 
         code,
         last_error ? last_error : "");

    output->active_ = false;
    output->delay_active_ = false;

    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }

    CREATE_OBS_DATA(extra_data);
    output->FillRecordingStat(extra_data);
    output->advanced_output_->delegate_->OnStoppedStreaming(
      output->identifier(),
      code,
      last_error,
      extra_data);
  }

  void OBSStartStreaming(void *data, calldata_t *params) {
    UNUSED_PARAMETER(params);
    StreamOutput *output = static_cast<StreamOutput*>(data);
    output->active_ = true;
    output->delay_active_ = false;

    blog(LOG_INFO, "Streaming started [id:%d]", output->identifier());

    if (output->advanced_output_->delegate_ == nullptr) {
      return;
    }
    output->advanced_output_->delegate_->OnStartedStreaming(output->identifier());
  }
};

using namespace libascentobs;
using namespace obs_control;

StreamOutput::StreamOutput(AdvancedOutput* advanced_output)
 : BaseOutput(advanced_output),
   is_custom_server_(false) {
}

StreamOutput::~StreamOutput() {
}

bool StreamOutput::Initialize(OBSData& error_result, const char* type) {
  if (output_ == nullptr) {
    auto service = GetService(type);
    if (!service) {
      blog(LOG_ERROR, "Can't load obs steaming service");
      obs_data_set_int(error_result,
        protocol::kErrorCodeField,
        protocol::events::INIT_ERROR_STREAM_START_NO_SERVICE_ERROR);
      return false;
    }
    // Change this line from obs_service_get_output_type to obs_service_get_preferred_output_type
    const char *server_type = obs_service_get_preferred_output_type(service);
    if (!server_type) {
      server_type = "rtmp_output";
    }
    output_ =
      obs_output_create(server_type, "adv_stream", nullptr, nullptr);
    if (output_ == nullptr) {
      blog(LOG_ERROR, "Fail to create streaming output");
      obs_data_set_int(error_result,
        protocol::kErrorCodeField,
        protocol::events::INIT_ERROR_FAILED_CREATING_OUTPUT_FILE);
      return false;
    }
    obs_output_release(output_);
    obs_output_set_service(output_, service);
  }
  if (!ConnectSignals()) {
    obs_data_set_int(error_result,
      protocol::kErrorCodeField,
      protocol::events::INIT_ERROR_FAILED_CREATING_OUTPUT_SIGNALS);
    return false;
  }
  return true;
}

bool StreamOutput::Start(int identifier,
                         OBSData& streaming_settings,
                         OBSData& error_result) {

  if (this->Active()) {
    blog(LOG_ERROR, kErrorStreamAlreadyStart);
    obs_data_set_int(error_result,
      protocol::kErrorCodeField,
      protocol::events::INIT_ERROR_FAILED_STARTING_OUTPUT_ALREADY_RUNNING);
    return false;
  }

  if (!service_) {
    blog(LOG_ERROR, "Can't load obs steaming service");
    obs_data_set_int(error_result,
      protocol::kErrorCodeField,
      protocol::events::INIT_ERROR_STREAM_START_NO_SERVICE_ERROR);
    return false;
  }

  /// update service
  {
    obs_data_t* settings = obs_data_create();
    auto service_type = obs_data_get_string(streaming_settings, "type");
    auto stream_key = obs_data_get_string(streaming_settings, "stream_key");
    auto server_url = obs_data_get_string(streaming_settings, "server_url");


    obs_data_set_string(settings, "service", service_type);
    obs_data_set_string(settings, "server", server_url);
    obs_data_set_string(settings, "key", stream_key);

    if (is_custom_server_) {
      auto use_auth = obs_data_get_bool(settings, "use_auth");
      auto username = bstrdup(obs_data_get_string(settings, "username"));
      auto password = bstrdup(obs_data_get_string(settings, "password"));

      obs_data_set_bool(settings, "use_auth", use_auth);
      obs_data_set_string(settings, "username", username);
      obs_data_set_string(settings, "password", password);
    }
    
    obs_service_update(service_, settings);
    obs_data_release(settings);
  }

  //////////////////////////////////////////////////////////////////////////
  //From OBS defaults
  bool reconnect = true;
  int retryDelay = 10;
  int maxRetries = 20; 
  bool useDelay = false;
  int delaySec = 20;
  bool preserveDelay = true;
  const char *bindIP = "default";
  bool enableNewSocketLoop = false;
  bool enableLowLatencyMode = false;

  obs_data_t *settings = obs_data_create();
  obs_data_set_string(settings, "bind_ip", bindIP);
  obs_data_set_bool(settings, "new_socket_loop_enabled",
    enableNewSocketLoop);
  obs_data_set_bool(settings, "low_latency_mode_enabled",
    enableLowLatencyMode);
  obs_output_update(output_, settings);
  obs_data_release(settings);

  if (!reconnect)
    maxRetries = 0;

  obs_output_set_delay(output_, useDelay ? delaySec : 0,
    preserveDelay ? OBS_OUTPUT_DELAY_PRESERVE : 0);

  obs_output_set_reconnect_settings(output_, maxRetries,
    retryDelay);

  obs_output_set_video_encoder(output_,
                               advanced_output_->recording_video_encoder_);
  obs_output_set_audio_encoder(output_,
                               advanced_output_->aacTrack[0], 
                               0);

  //////////////////////////////////////////////////////////////////////////

  identifier_ = identifier;
  if (obs_output_start(output_)) {
    return true;
  }

  identifier_ = -1;

  const char *type = obs_service_get_preferred_output_type(service_);
  const char *error = obs_output_get_last_error(output_);
  bool has_last_error = error && *error;

  blog(LOG_ERROR, "Stream output type '%s' failed to start!%s%s",
       type,
       has_last_error ? "  Last Error: " : "",
       has_last_error ? error : "");

  obs_data_set_int(error_result,
    protocol::kErrorCodeField,
    protocol::events::INIT_ERROR_STREAM_START_NO_SERVICE_ERROR);
  obs_data_set_string(error_result,
    protocol::kErrorDescField,
    has_last_error ? error : kErrorStartCaptureGenericEncoderError);

  return false;
}

//------------------------------------------------------------------------------
OBSService StreamOutput::GetService(const char* type) {
  if (service_ != nullptr) {
    return service_;
  }

  is_custom_server_ = (type != nullptr && strcmp(type, "Custom") == 0);
  const char* id = is_custom_server_ ? "rtmp_custom" : "rtmp_common";
  service_ = obs_service_create(id,
    "default_service",
    nullptr,
    nullptr);


  if (!service_) {
    return nullptr;
  }

  obs_service_release(service_);
  blog(LOG_INFO, "Stream service [%s] created", id);
  return service_;
}

bool StreamOutput::Start(OBSData& error_result) {

  if (this->active_) {
    blog(LOG_ERROR, "streaming start: already active");
    obs_data_set_int(error_result, protocol::kErrorCodeField,
      protocol::events::INIT_ERROR_FAILED_STARTING_OUTPUT_ALREADY_RUNNING);
    return false;
  }

  if (!obs_output_start(output_)) {
    //blog(LOG_ERROR, kErrorReplayStart);
   
    const char *error = obs_output_get_last_error(output_);

    bool driver_error = IsUpdateDriverError(error);
    obs_data_set_int(error_result,
      protocol::kErrorCodeField, !driver_error ?
        protocol::events::INIT_ERROR_FAILED_STARTING_OUTPUT_WITH_OBS_ERROR : 
        protocol::events::INIT_ERROR_FAILED_STARTING_UPDATE_DRIVER_ERROR);

    obs_data_set_string(error_result, protocol::kErrorDescField,
      error ? error : kErrorStartCaptureGenericEncoderError);

    identifier_ = -1;
    return false;
  }

  blog(LOG_INFO, "Starting streaming [id:%d]", identifier_);
  
  return true;
}

void StreamOutput::Stop(bool force) {
  if (!this->active_)
    return;

  blog(LOG_INFO, "Stop streaming [id:%d force: %d]",identifier_, force);

  if (force) {
    obs_output_force_stop(output_);
  } else {
    obs_output_stop(output_);
  }
}

bool StreamOutput::Active() const {
  return delay_active_ || active_;////obs_output_active(streaming_output_);
}

bool StreamOutput::ConnectSignals() {
  signal_handler_t *signal =
    obs_output_get_signal_handler(output_);

  if (!signal) {
    //blog(LOG_ERROR, kErrorReplayOutputSignal);
    return false;
  }

  DisconnectSignals();

  startStreaming.Connect(signal, "start",
    OBSStartStreaming, this);
  stopStreaming.Connect(signal, "stop",
    OBSStopStreaming, this);
  streamStopping.Connect(signal, "stopping",
    OBSStreamStopping, this);
  streamDelayStarting.Connect(signal, "starting",
    OBSStreamStarting, this);
  diskWarning.Connect(signal, "disk_space_warning",
    OBSDiskWarning, this);
  
  return true;
}

void StreamOutput::DisconnectSignals() {
  startStreaming.Disconnect();
  stopStreaming.Disconnect();
  streamStopping.Disconnect();
  streamDelayStarting.Disconnect();
  diskWarning.Disconnect();
}


void StreamOutput::ReportOutputStopped(
  int code, const char* last_error /*= nullptr*/) {
  advanced_output_->delegate_->OnStoppedStreaming(
    identifier_, code, last_error);
}

