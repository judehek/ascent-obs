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

RequestResult AddMonitor(const Request &request, const json &properites, OWOBSController *owobs_controller)
{
	if (!properites.contains("id") || properites["id"].is_null()) {
		return RequestResult::Error(RequestStatus::MissingRequestField, "Your request is missing the `monitorId' field.");	
	}

	int capture_type = 0;
	if (properites.contains("type")) {
		capture_type = properites["type"];
	}

	const char *kBitBltMonitorCapture = "monitor_capture_old";
	const char *type = "monitor_capture";
	if (capture_type == 3) { /*BitBlt*/
		if (OWOBSController::SourceAvailable(kBitBltMonitorCapture)) {
			type = kBitBltMonitorCapture;
		} else {
			capture_type = 0;
			blog(LOG_WARNING, "BitBlt monitor capture is not supported");
		}
	}

	std::string monitor_id = properites["id"];
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_int(settings, "method", capture_type);
	obs_data_set_string(settings, "monitor_id", monitor_id.c_str());
	if (properites.contains("captureCursor")) {
		obs_data_set_bool(settings, "capture_cursor", properites["captureCursor"]);
	}

	if (properites.contains("forceSDR")) {
		obs_data_set_bool(settings, "force_sdr", properites["forceSDR"]);
	}

	if (capture_type == 3) {
		obs_data_set_int(settings, "monitor", 10);
		obs_data_set_bool(settings, "compatibility", true);
	}

	OBSSourceAutoRelease source = obs_source_create(type, "Display Capture", settings, NULL);
	if (!source) {
		return RequestResult::Error(RequestStatus::ResourceCreationFailed, "Creation of the Display Capture failed.");
	}

	bool stretchToOutputSize =
		// default is true
		!properites.contains("stretchToOutputSize") || 
		 (bool)properites["stretchToOutputSize"];

	owobs_controller->AddSource(source, stretchToOutputSize);

	return RequestResult::Success();
}

RequestResult RequestHandler::AddMonitorSource(const Request &request) {
	return AddMonitor(request, request.RequestData, owobs_controller_.get());
}

RequestResult RequestHandler::AddSource(const Request& request) {
	RequestStatus::RequestStatus statusCode;
	std::string comment;
	if (!request.ValidateString("sourceType", statusCode, comment)) {
		return RequestResult::Error(statusCode, comment);
	}

	if (!request.ValidateObject("properties", statusCode, comment)) {
		return RequestResult::Error(statusCode, comment);
	}

	std::string source_type = request.RequestData["sourceType"];
	if (strcmp(source_type.c_str(), "Display") == 0) {
		return AddMonitor(
			request, request.RequestData["properties"], owobs_controller_.get());
	}

	blog(LOG_ERROR, "AddSource error. Unknown source type: %s", source_type.c_str());
	return RequestResult::Error(
		RequestStatus::InvalidResourceType,
		"Unknown source type");
}
