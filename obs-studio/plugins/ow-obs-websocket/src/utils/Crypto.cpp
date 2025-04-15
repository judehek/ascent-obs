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

//#include <QByteArray>
//#include <QCryptographicHash>
//#include <QRandomGenerator>

#include "Crypto.h"
#include "plugin-macros.generated.h"

static const char allowedChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
static const int allowedCharsCount = static_cast<int>(sizeof(allowedChars) - 1);

std::string Utils::Crypto::GenerateSalt()
{
	return std::string();
}

std::string Utils::Crypto::GenerateSecret(std::string password, std::string salt)
{
	return std::string();
}

bool Utils::Crypto::CheckAuthenticationString(std::string secret, std::string challenge, std::string authenticationString)
{
	return true;
}

std::string Utils::Crypto::GeneratePassword(size_t length)
{
	return std::string();
}
