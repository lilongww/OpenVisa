/*********************************************************************************
**                                                                              **
**  Copyright (C) 2022-2025 LiLong                                              **
**  This file is part of OpenVisa.                                              **
**                                                                              **
**  OpenVisa is free software: you can redistribute it and/or modify            **
**  it under the terms of the GNU Lesser General Public License as published by **
**  the Free Software Foundation, either version 3 of the License, or           **
**  (at your option) any later version.                                         **
**                                                                              **
**  OpenVisa is distributed in the hope that it will be useful,                 **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of              **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               **
**  GNU Lesser General Public License for more details.                         **
**                                                                              **
**  You should have received a copy of the GNU Lesser General Public License    **
**  along with OpenVisa.  If not, see <https://www.gnu.org/licenses/>.          **
**********************************************************************************/
#pragma once

#ifdef WIN32

extern "C"
{
#include <Windows.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <setupapi.h>

#ifndef INITGUID
#define INITGUID
#include <guiddef.h>
#undef INITGUID
#else
#include <guiddef.h>
#endif

#include <ntddmodm.h>
}

#include <string>
#include <tuple>

namespace OpenVisa
{
std::string _deviceInstanceIdentifier(DWORD deviceInstanceNumber);
std::string _property(const HDEVINFO& dev, SP_DEVINFO_DATA& data, DWORD property);
std::tuple<bool, unsigned short> _parseIdent(const std::string& str, const std::string& pattern);
std::string _parseSerialNumber(const std::string& str);
} // namespace OpenVisa
#endif