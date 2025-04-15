/*
obs-websocket
Copyright (C) 2016-2021 Stephane Lepin <stephane.lepin@gmail.com>
Copyright (C) 2020-2021 Kyle Manning <tt2468@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "RequestHandler.h"
#include "../websocketserver/WebSocketServer.h"
#include "../eventhandler/types/EventSubscription.h"
#include "../WebSocketApi.h"
#include "../owobs-websocket.h"
#include "obs.h"
#include <graphics/graphics-information.h>

static constexpr uint32_t ENCODER_HIDE_FLAGS = (
	OBS_ENCODER_CAP_DEPRECATED | OBS_ENCODER_CAP_INTERNAL);

void SetOptionalStringValue(config_t *config, const Request &request, const char *section, const char *key, const char *configKey)
{
	if (!request.Contains(key)) {
		return;
	}

	std::string value = request.RequestData[key];
	config_set_string(config, section, configKey, value.c_str());
}

void SetOptionalUintValue(config_t *config, const Request &request, const char *section, const char *key, const char *configKey)
{
	if (!request.Contains(key)) {
		return;
	}

	uint64_t value = request.RequestData[key];
	config_set_uint(config, section, configKey, value);
}

void SetOptionalBoolValue(config_t *config, const Request &request, const char *section, const char *key, const char *configKey)
{
	if (!request.Contains(key)) {
		return;
	}

	bool value = request.RequestData[key];
	config_set_bool(config, section, configKey, value);
}


std::vector<json> getInputDevices(obs_properties_t *props) {
	std::vector<json> res;
	obs_property_t *inputs = obs_properties_get(props, "device_id");
	obs_property_t *defult_device_id_prop = obs_properties_get(props, "default_device_id");

	auto defult_device_id = obs_property_description(defult_device_id_prop);
	size_t count = obs_property_list_item_count(inputs);	
	for (size_t i = 0; i < count; i++) {
		const char *name = obs_property_list_item_name(inputs, i);
		const char *val = obs_property_list_item_string(inputs, i);
		json device;
		device["name"] = name;
		device["id"] = val;
		device["isDefault"] = defult_device_id_prop && 
			strcmp(defult_device_id, val) == 0;		
		res.push_back(device);
	}

	return res;
}

void QueryInformationAudio(json &responseData, OWOBSController *owobs_controller)
{
	json audio;
	obs_properties_t *input_props = obs_get_source_properties(
		owobs_controller->InputAudioSource());
	obs_properties_t *output_props = obs_get_source_properties(
		owobs_controller->OutputAudioSource());

	if (input_props) {
		audio["input"] = getInputDevices(input_props);
		obs_properties_destroy(input_props);
	}

	if (output_props) {
		audio["output"] = getInputDevices(output_props);
		obs_properties_destroy(output_props);	
	}

	responseData["audio"] = audio;
}

void SetEncoderInforamtion(const char* type, json& device) {
	const char *name = obs_encoder_get_display_name(type);
	const char *codec = obs_get_encoder_codec(type);
	
	device["name"] = name;
	device["codec"] = codec;
	device["type"] = type;

	OBSData defaults = obs_encoder_defaults(type);
	json defaultsJons = Utils::Json::ObsDataToJson(defaults, true);
	OBSProperties props = obs_get_encoder_properties(type);
	device["properties"] = Utils::Json::ObsPropsToJson(props, &defaultsJons);
}

void QueryInformationEncoders(json &responseData, OWOBSController *owobs_controller)
{
	std::vector<json> video_encoders;
	std::vector<json> audio_encoders;

	const char *type;
	size_t idx = 0;
	while (obs_enum_encoder_types(idx++, &type)) {
		json device;	
		SetEncoderInforamtion(type, device);
	
		uint32_t caps = obs_get_encoder_caps(type);
		if (obs_get_encoder_type(type) == OBS_ENCODER_VIDEO) {
			if ((caps & ENCODER_HIDE_FLAGS) != 0)
				continue;		
			video_encoders.push_back(device);
		}

		if (obs_get_encoder_type(type) == OBS_ENCODER_AUDIO) {
			audio_encoders.push_back(device);
		}
	}

	json encoders;
	encoders["video"] = video_encoders;
	encoders["audio"] = audio_encoders;
	responseData["encoders"] = encoders;
}

// adapters + monitors
void QueryInformationGraphics(json &responseData)
{
	json graphics;
	int error = 0;
	auto information = static_cast<graphics_information *>(
		get_graphics_information(false, &error));

	std::vector<json> adapaters;
	std::vector<json> monitors;

	if (information) {
		for (auto apater_iter : information->adapters) {
			json apater;
			apater["index"] = apater_iter.index;
			apater["name"] = apater_iter.name;
			apater["driver"] = apater_iter.driver;
			apater["hagsEnabled"] = apater_iter.hags_enabled;
			apater["hagsEnabledByDefault"] = apater_iter.hags_enabled_by_default;
			adapaters.push_back(apater);
		}

		for (auto monitor_iter : information->monitors) {
			json monitor;
			monitor["index"] = monitor_iter.index;
			monitor["adapterIndex"] = monitor_iter.adpater;
			monitor["id"] = monitor_iter.id;
			monitor["altId"] = monitor_iter.alt_id;
			monitor["friendlyName"] = monitor_iter.friendly_name;
			monitor["attachedToDesktop"] = monitor_iter.attachedToDesktop;
			monitor["dpi"] = monitor_iter.dpi;
			monitor["refreshRate"] = monitor_iter.refresh;
			json rect;
			rect["left"] = monitor_iter.left;
			rect["top"] = monitor_iter.top;
			rect["width"] = monitor_iter.cx;
			rect["height"] = monitor_iter.cy;
			monitor["rect"] = rect;
			monitors.push_back(monitor);
		}
	}

	graphics["adapters"] = adapaters;
	graphics["monitors"] = monitors;

	responseData["graphics"] = graphics;

}

RequestResult RequestHandler::QueryInformation(const Request &) {
	json responseData;
	QueryInformationAudio(responseData, owobs_controller_.get());
	QueryInformationEncoders(responseData, owobs_controller_.get());
	QueryInformationGraphics(responseData);

	return RequestResult::Success(responseData);
}

RequestResult RequestHandler::SetAudioSettings(const Request &request)
{
	config_t *config = owobs_controller_->Config();
	SetOptionalStringValue(config, request, "Audio", "speakerLayer", "ChannelSetup");
	SetOptionalUintValue(config, request, "Audio", "sampleRate", "SampleRate");
	SetOptionalBoolValue(config, request, "Audio", "lowLatencyAudioBuffering", "LowLatencyAudioBuffering");

	if (!owobs_controller_->ResetAudio()) {
		return RequestResult::Error(RequestStatus::OutputRunning,
					    "Video settings cannot be changed while an output is active.");
	}
	return RequestResult::Success();
}
