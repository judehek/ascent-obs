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

//#include <obs-frontend-api.h>

#include "Config.h"
#include "utils/Crypto.h"
#include "utils/Platform.h"
#include "switches.hpp"

#include <util/command_line.hpp>

using namespace ow_obs_websocket;

Config::Config()
{
	Load();
}

void Config::Load()
{
	auto commnad_line = CommandLine::ForCurrentProcess();
	if (!commnad_line) {
		return;
	}
	// Process `--websocket_debug` override
	if (commnad_line->HasSwitch(switches::kdebug)) {
		// Debug does not persist on reload, so we let people override it with a flag.
		blog(LOG_INFO, "[Config::Load] --websocket_debug passed. Enabling debug logging.");
		DebugEnabled = true;
	}

	this->ServerPort = 0;
	auto port_str = commnad_line->GetSwitchValueASCII(switches::kPort);
	if (port_str.empty()) {
		return;
	}

	this->ServerPort = std::atoi(port_str.c_str());
}
