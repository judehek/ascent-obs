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

/**
 * Gets data about the current plugin and RPC version.
 *
 * @responseField obsVersion            | String        | Current OBS Studio version
 * @responseField obsWebSocketVersion   | String        | Current obs-websocket version
 * @responseField rpcVersion            | Number        | Current latest obs-websocket RPC version
 * @responseField availableRequests     | Array<String> | Array of available RPC requests for the currently negotiated RPC version
 * @responseField supportedImageFormats | Array<String> | Image formats available in `GetSourceScreenshot` and `SaveSourceScreenshot` requests.
 * @responseField platform              | String        | Name of the platform. Usually `windows`, `macos`, or `ubuntu` (linux flavor). Not guaranteed to be any of those
 * @responseField platformDescription   | String        | Description of the platform, like `Windows 10 (10.0)`
 *
 * @requestType GetVersion
 * @complexity 1
 * @rpcVersion -1
 * @initialVersion 5.0.0
 * @category general
 * @api requests
 */
RequestResult RequestHandler::GetVersion(const Request &)
{
	json responseData;
	responseData["obsVersion"] = Utils::Obs::StringHelper::GetObsVersion();
	responseData["obsWebSocketVersion"] = OWOBS_WEBSOCKET_VERSION;
	responseData["rpcVersion"] = OWOBS_WEBSOCKET_RPC_VERSION;
	responseData["availableRequests"] = GetRequestList();

	//QList<QByteArray> imageWriterFormats = QImageWriter::supportedImageFormats();
	//std::vector<std::string> supportedImageFormats;
	//for (const QByteArray &format : imageWriterFormats) {
	//	supportedImageFormats.push_back(format.toStdString());
	//}
	//responseData["supportedImageFormats"] = supportedImageFormats;

	//responseData["platform"] = QSysInfo::productType().toStdString();
	//responseData["platformDescription"] = QSysInfo::prettyProductName().toStdString();

	return RequestResult::Success(responseData);
}

/**
 * Gets statistics about OBS, obs-websocket, and the current session.
 *
 * @responseField cpuUsage                         | Number | Current CPU usage in percent
 * @responseField memoryUsage                      | Number | Amount of memory in MB currently being used by OBS
 * @responseField availableDiskSpace               | Number | Available disk space on the device being used for recording storage
 * @responseField activeFps                        | Number | Current FPS being rendered
 * @responseField averageFrameRenderTime           | Number | Average time in milliseconds that OBS is taking to render a frame
 * @responseField renderSkippedFrames              | Number | Number of frames skipped by OBS in the render thread
 * @responseField renderTotalFrames                | Number | Total number of frames outputted by the render thread
 * @responseField outputSkippedFrames              | Number | Number of frames skipped by OBS in the output thread
 * @responseField outputTotalFrames                | Number | Total number of frames outputted by the output thread
 * @responseField webSocketSessionIncomingMessages | Number | Total number of messages received by obs-websocket from the client
 * @responseField webSocketSessionOutgoingMessages | Number | Total number of messages sent by obs-websocket to the client
 *
 * @requestType GetStats
 * @complexity 2
 * @rpcVersion -1
 * @initialVersion 5.0.0
 * @category general
 * @api requests
 */
RequestResult RequestHandler::GetStats(const Request &)
{
	json responseData = Utils::Obs::ObjectHelper::GetStats();

	if (_session) {
		responseData["webSocketSessionIncomingMessages"] = _session->IncomingMessages();
		responseData["webSocketSessionOutgoingMessages"] = _session->OutgoingMessages();
	} else {
		responseData["webSocketSessionIncomingMessages"] = nullptr;
		responseData["webSocketSessionOutgoingMessages"] = nullptr;
	}

	return RequestResult::Success(responseData);
}

/**
 * Custom event emitted by `BroadcastCustomEvent`.
 * 
 * @dataField eventData | Object | Custom event data
 *
 * @eventType CustomEvent
 * @eventSubscription General
 * @complexity 1
 * @rpcVersion -1
 * @initialVersion 5.0.0
 * @category general
 * @api events
 */

/**
 * Broadcasts a `CustomEvent` to all WebSocket clients. Receivers are clients which are identified and subscribed.
 *
 * @requestField eventData | Object | Data payload to emit to all receivers
 *
 * @requestType BroadcastCustomEvent
 * @complexity 1
 * @rpcVersion -1
 * @initialVersion 5.0.0
 * @category general
 * @api requests
 */
RequestResult RequestHandler::BroadcastCustomEvent(const Request &request)
{
	RequestStatus::RequestStatus statusCode;
	std::string comment;
	if (!request.ValidateObject("eventData", statusCode, comment))
		return RequestResult::Error(statusCode, comment);

	auto webSocketServer = GetWebSocketServer();
	if (!webSocketServer)
		return RequestResult::Error(RequestStatus::RequestProcessingFailed, "Unable to send event due to internal error.");

	webSocketServer->BroadcastEvent(EventSubscription::General, "CustomEvent", request.RequestData["eventData"]);

	return RequestResult::Success();
}

/**
 * Call a request registered to a vendor.
 *
 * A vendor is a unique name registered by a third-party plugin or script, which allows for custom requests and events to be added to obs-websocket.
 * If a plugin or script implements vendor requests or events, documentation is expected to be provided with them.
 *
 * @requestField vendorName   | String | Name of the vendor to use
 * @requestField requestType  | String | The request type to call
 * @requestField ?requestData | Object | Object containing appropriate request data | {}
 *
 * @responseField vendorName   | String | Echoed of `vendorName`
 * @responseField requestType  | String | Echoed of `requestType`
 * @responseField responseData | Object | Object containing appropriate response data. {} if request does not provide any response data
 *
 * @requestType CallVendorRequest
 * @complexity 3
 * @rpcVersion -1
 * @initialVersion 5.0.0
 * @category general
 * @api requests
 */
RequestResult RequestHandler::CallVendorRequest(const Request &request)
{
	RequestStatus::RequestStatus statusCode;
	std::string comment;
	if (!request.ValidateString("vendorName", statusCode, comment) ||
	    !request.ValidateString("requestType", statusCode, comment))
		return RequestResult::Error(statusCode, comment);

	std::string vendorName = request.RequestData["vendorName"];
	std::string requestType = request.RequestData["requestType"];

	OBSDataAutoRelease requestData = obs_data_create();
	if (request.Contains("requestData")) {
		if (!request.ValidateOptionalObject("requestData", statusCode, comment, true))
			return RequestResult::Error(statusCode, comment);

		requestData = Utils::Json::JsonToObsData(request.RequestData["requestData"]);
	}

	OBSDataAutoRelease obsResponseData = obs_data_create();

	auto webSocketApi = GetWebSocketApi();
	if (!webSocketApi)
		return RequestResult::Error(RequestStatus::RequestProcessingFailed,
					    "Unable to call request due to internal error.");

	auto ret = webSocketApi->PerformVendorRequest(vendorName, requestType, requestData, obsResponseData);
	switch (ret) {
	default:
	case WebSocketApi::RequestReturnCode::Normal:
		break;
	case WebSocketApi::RequestReturnCode::NoVendor:
		return RequestResult::Error(RequestStatus::ResourceNotFound, "No vendor was found by that name.");
	case WebSocketApi::RequestReturnCode::NoVendorRequest:
		return RequestResult::Error(RequestStatus::ResourceNotFound, "No request was found by that name.");
	}

	json responseData;
	responseData["vendorName"] = vendorName;
	responseData["requestType"] = requestType;
	responseData["responseData"] = Utils::Json::ObsDataToJson(obsResponseData);

	return RequestResult::Success(responseData);
}

/**
 * Sleeps for a time duration or number of frames. Only available in request batches with types `SERIAL_REALTIME` or `SERIAL_FRAME`.
 *
 * @requestField ?sleepMillis | Number | Number of milliseconds to sleep for (if `SERIAL_REALTIME` mode) | >= 0, <= 50000
 * @requestField ?sleepFrames | Number | Number of frames to sleep for (if `SERIAL_FRAME` mode) | >= 0, <= 10000
 *
 * @requestType Sleep
 * @complexity 2
 * @rpcVersion -1
 * @initialVersion 5.0.0
 * @category general
 * @api requests
 */
RequestResult RequestHandler::Sleep(const Request &request)
{
	RequestStatus::RequestStatus statusCode;
	std::string comment;

	if (request.ExecutionType == RequestBatchExecutionType::SerialRealtime) {
		if (!request.ValidateNumber("sleepMillis", statusCode, comment, 0, 50000))
			return RequestResult::Error(statusCode, comment);
		int64_t sleepMillis = request.RequestData["sleepMillis"];
		std::this_thread::sleep_for(std::chrono::milliseconds(sleepMillis));
		return RequestResult::Success();
	} else if (request.ExecutionType == RequestBatchExecutionType::SerialFrame) {
		if (!request.ValidateNumber("sleepFrames", statusCode, comment, 0, 10000))
			return RequestResult::Error(statusCode, comment);
		RequestResult ret = RequestResult::Success();
		ret.SleepFrames = request.RequestData["sleepFrames"];
		return ret;
	} else {
		return RequestResult::Error(RequestStatus::UnsupportedRequestBatchExecutionType);
	}
}

