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


void SetAudioDeviceProperties(json &device, obs_source_t *source)
{

	auto *name = obs_source_get_name(source);
	if (device.contains("mono")) {
		bool mono = device["mono"];
		uint32_t flags = obs_source_get_flags(source);
		if (mono) {
			flags |= OBS_SOURCE_FLAG_FORCE_MONO;
		} else {
			flags &= ~OBS_SOURCE_FLAG_FORCE_MONO;
		}
		obs_source_set_flags(source, flags);
		blog(LOG_INFO, "update device [%s] mono: %d ", name, mono);
	}

	if (device.contains("volume")) {
		float volume = device["volume"];
		float origin = volume;
		if (volume > 20.0) {
			volume = 20.0;
		} else if (volume < 0.0) {
			volume = 0.0;
		}
		obs_source_set_volume(source, volume);
		blog(LOG_INFO, "update device [%s] volume: %f [%f]", name, volume, origin);
	}

	if (device.contains("balance")) {
		float balance = device["balance"];
		float origin = balance;
		if (balance > 1.0) {
			balance = 1.0;
		} else if (balance < 0.0) {
			balance = 0.0;
		}
		obs_source_set_balance_value(source, balance);
		blog(LOG_INFO, "update device [%s] balance: %f [%f]", name, balance, origin);
	}

	if (device.contains("tracks")) {
		uint32_t tracks = device["tracks"];
		uint32_t mixers = obs_source_get_audio_mixers(source);
		uint32_t new_mixers = mixers;

		for (uint32_t index = 0; index < MAX_AUDIO_MIXES; index++) {
			if (tracks & (1 << index)) {
				new_mixers |= (1 << index);
			} else {
				new_mixers &= ~(1 << index);
			}
		}
		obs_source_set_audio_mixers(source, new_mixers);
		blog(LOG_INFO, "update device [%s] trackes: %d ", name, new_mixers);
	}
}

void AddAudioDevice(const Request &request, json &device, OWOBSController *owobs_controller)
{
	std::string name = device["name"];

	// already exits?
	OBSSourceAutoRelease source = obs_get_source_by_name(name.c_str());

	const bool is_new = source == NULL;

	OBSDataAutoRelease settings = obs_data_create();
	bool update_device = false;

	const char kUseDeviceTimingSetting[] = "use_device_timing";
	if (device.contains(kUseDeviceTimingSetting)) {
		if (device.contains(kUseDeviceTimingSetting)) {
			obs_data_set_bool(settings, kUseDeviceTimingSetting, device[kUseDeviceTimingSetting]);
		}
		update_device = !is_new;
	}

	if (is_new) {
		std::string type = device["type"];
		const bool is_input = strcmp(type.c_str(), "input") == 0;
		std::string id = device["id"];
		auto *type_id = is_input ? owobs_controller->InputAudioSource() : owobs_controller->OutputAudioSource();

		blog(LOG_INFO, "create new audio device [%s]: %s", type_id, name.c_str());
		obs_data_set_string(settings, "device_id", id.c_str());
		source = obs_source_create(type_id, name.c_str(), settings, NULL);
	}

	if (!source) {
		blog(LOG_ERROR, "create new audio device [%s] error", name.c_str());
		return;
	}

	if (update_device) {
		std::string id = device["id"];
		blog(LOG_INFO, "update audio device [%s]", name.c_str());
		obs_data_set_string(settings, "device_id", id.c_str());
		obs_source_update(source, settings);
	}

	SetAudioDeviceProperties(device, source);

	if (is_new) {
		owobs_controller->AddSource(source);
	}
}

RequestResult RequestHandler::SetAudioDevices(const Request &request)
{
	if (!request.RequestData.contains("devices")) {
		blog(LOG_WARNING, "[SetAudioDevices] no devices");
		return RequestResult::Success();
	}

	auto devices = request.RequestData["devices"];
	auto *owobs_controller = owobs_controller_.get();
	for (auto device : devices) {
		AddAudioDevice(request, device, owobs_controller);
	}

	return RequestResult::Success();
}
