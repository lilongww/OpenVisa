﻿/*********************************************************************************
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

#include "SerialPortInfoWin.h"

#ifdef WIN32
#include "../SerialPortInfo.h"

#include <cuchar>
#include <regex>
#include <tuple>

namespace OpenVisa
{
std::string toString(const wchar_t* str, size_t len)
{
    auto need = WideCharToMultiByte(CP_ACP, 0, str, static_cast<unsigned long>(len), nullptr, 0, nullptr, nullptr);
    if (!need)
        return {};
    std::string buffer(need, '0');
    need = WideCharToMultiByte(
        CP_ACP, 0, str, static_cast<unsigned long>(len), buffer.data(), static_cast<int>(buffer.size()), nullptr, nullptr);
    buffer.resize(need);
    return buffer;
}

static std::string _portName(const HDEVINFO& dev, SP_DEVINFO_DATA& data)
{
    HKEY hkey = SetupDiOpenDevRegKey(dev, &data, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
    wchar_t name[MAX_PATH];
    DWORD len = MAX_PATH;
    auto ret  = RegQueryValueExW(hkey, L"PortName", NULL, NULL, (LPBYTE)name, &len);
    RegCloseKey(hkey);
    if (ret == EXIT_SUCCESS)
        return toString(name, std::wcslen(name));
    return {};
}

std::string _property(const HDEVINFO& dev, SP_DEVINFO_DATA& data, DWORD property)
{
    wchar_t str[MAX_PATH];
    DWORD len = MAX_PATH;
    auto ret  = SetupDiGetDeviceRegistryPropertyW(dev, &data, property, NULL, reinterpret_cast<LPBYTE>(str), MAX_PATH, &len);
    if (ret && len > 0)
    {
        return toString(str, std::wcslen(str));
    }
    return {};
}

std::string _deviceInstanceIdentifier(DWORD deviceInstanceNumber)
{
    wchar_t outputBuffer[MAX_DEVICE_ID_LEN + 1] {};
    if (CM_Get_Device_IDW(deviceInstanceNumber, &outputBuffer[0], MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS)
    {
        return {};
    }
    return toString(outputBuffer, std::wcslen(outputBuffer));
}

std::tuple<bool, unsigned short> _parseIdent(const std::string& str, const std::string& pattern)
{
    auto index = str.find(pattern);
    if (index != std::string::npos)
    {
        try
        {
            return { true, static_cast<unsigned short>(std::stoul(str.substr(index + 4, 4), nullptr, 16)) };
        }
        catch (...)
        {
        }
    }
    return { false, 0 };
}

std::string _parseSerialNumber(const std::string& str)
{
    auto firstbound = str.find_last_of('\\');
    auto lastbound  = str.find_first_of('_', firstbound);
    if (str.starts_with("USB\\"))
    {
        if (lastbound != str.size() - 3)
            lastbound = str.size();
        auto ampersand = str.find_first_of('&', firstbound);
        if (ampersand != -1 && ampersand < lastbound)
            return {};
    }
    else if (str.starts_with("FTDIBUS\\"))
    {
        firstbound = str.find_last_of('+');
        lastbound  = str.find_first_of('\\', firstbound);
        if (lastbound == -1)
            return {};
    }
    else
    {
        return {};
    }

    return str.substr(firstbound + 1, lastbound - firstbound - 1);
}

std::vector<SerialPortInfo> SerialPortInfo::listPorts()
{
    static const struct
    {
        GUID guid;
        DWORD flags;
    } setupTokens[] = { { GUID_DEVCLASS_PORTS, DIGCF_PRESENT },
                        { GUID_DEVCLASS_MODEM, DIGCF_PRESENT },
                        { GUID_DEVINTERFACE_COMPORT, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE },
                        { GUID_DEVINTERFACE_MODEM, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE } };

    std::vector<SerialPortInfo> ports;
    auto tokens = sizeof(setupTokens) / sizeof(setupTokens[0]);
    for (int i = 0; i < tokens; ++i)
    {
        auto dev        = SetupDiGetClassDevsW(&setupTokens[i].guid, NULL, NULL, setupTokens[i].flags);
        int deviceIndex = 0;
        SP_DEVINFO_DATA data;
        data.cbSize = sizeof(data);
        while (SetupDiEnumDeviceInfo(dev, deviceIndex++, &data))
        {
            const auto& portName = _portName(dev, data);
            if (portName.contains("LPT") || portName.empty())
                continue;
            SerialPortInfo info;
            info.setPortName(portName);
            info.setDescrption(_property(dev, data, SPDRP_DEVICEDESC));
            info.setHardwareID(_property(dev, data, SPDRP_HARDWAREID));
            info.setManufacturer(_property(dev, data, SPDRP_MFG));

            auto ident = _deviceInstanceIdentifier(data.DevInst);
            if (auto [hasVid, vid] = _parseIdent(ident, "VID_"); hasVid)
            {
                info.setVendorId(vid);
            }
            else if (auto [hasVid, vid] = _parseIdent(ident, "VEN_"); hasVid)
            {
                info.setVendorId(vid);
            }

            if (auto [hasPid, pid] = _parseIdent(ident, "PID_"); hasPid)
            {
                info.setProductId(pid);
            }
            else if (auto [hasPid, pid] = _parseIdent(ident, "DEV_"); hasPid)
            {
                info.setProductId(pid);
            }
            info.setSerialNumber(_parseSerialNumber(ident));
            ports.push_back(std::move(info));
        }
        SetupDiDestroyDeviceInfoList(dev);
    }
    std::ranges::sort(ports, [](const auto& a, const auto& b) { return a.portName() < b.portName(); });
    auto ret = std::ranges::unique(ports, [](const auto& a, const auto& b) { return a.portName() == b.portName(); });
    ports.erase(ret.begin(), ret.end());
    return ports;
}
} // namespace OpenVisa

#endif
