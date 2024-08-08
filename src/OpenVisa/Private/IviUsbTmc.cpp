/*********************************************************************************
**                                                                              **
**  Copyright (C) 2022-2024 LiLong                                              **
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
#include "IviUsbTmc.h"
#ifdef WIN32
#include "SerialPortInfoWin.h"
#include "UsbTmcProtocol.h"

#include <array>
#include <limits>

namespace OpenVisa
{
constexpr auto Class    = 0xFE;
constexpr auto SubClass = 0x03;
//          {A9FDBB24-128A-11d5-9961-00108335E361}
DEFINE_GUID(UsbTmcGuid, 0xA9FDBB24L, 0x128A, 0x11d5, 0x99, 0x61, 0x00, 0x10, 0x83, 0x35, 0xe3, 0x61);
constexpr auto UsbTmcDevClassName = "USB Test and Measurement Devices";
constexpr auto PacketSize         = 8 * 1024;

static std::string getPath(const Address<AddressType::USB>& addr)
{
    return std::format("\\\\?\\usb#vid_{:04x}&pid_{:04x}#{}#{}",
                       addr.vendorId(),
                       addr.productId(),
                       addr.serialNumber(),
                       "{A9FDBB24-128A-11d5-9961-00108335E361}");
}

static std::string getLastError()
{
    LPTSTR errorText = NULL;
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  GetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&errorText,
                  0,
                  NULL);

    if (errorText != NULL)
    {
        std::string str(errorText);
        LocalFree(errorText);
        return str;
    }
    return "Unknown error.";
}

struct IviUsbTmc::Impl
{
    HANDLE handle { nullptr };
    uint8_t tag { 0 };
    bool avalible { false };

    void updateTag()
    {
        ++tag;
        if (tag == 0)
            ++tag;
    }
    void write(const std::string& buffer)
    {
        DWORD writeBytes;
        OVERLAPPED overlapped;
        overlapped.hEvent     = CreateEvent(NULL, TRUE, FALSE, NULL);
        overlapped.Offset     = 0;
        overlapped.OffsetHigh = 0;
        WriteFile(handle, buffer.c_str(), static_cast<DWORD>(buffer.size()), &writeBytes, &overlapped);
        if (WaitForSingleObject(overlapped.hEvent, static_cast<DWORD>(base->m_attr.timeout().count())) == WAIT_TIMEOUT)
            throw std::runtime_error("Send timeout.");
    }
    IviUsbTmc* base;
    inline Impl(IviUsbTmc* b) : base(b) {}
};

IviUsbTmc::IviUsbTmc(Object::Attribute const& attr) : IOBase(attr), m_impl(std::make_unique<Impl>(this)) { init(); }

IviUsbTmc::~IviUsbTmc() {}

void IviUsbTmc::connect(const Address<AddressType::USB>& addr, const std::chrono::milliseconds& openTimeout)
{
    auto handle = CreateFileA(
        getPath(addr).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 0);
    if (handle == INVALID_HANDLE_VALUE)
    {
        throw std::exception("Open Usb failed.");
    }
    m_impl->handle = handle;
}

void IviUsbTmc::send(const std::string& buffer) const
{
    m_impl->updateTag();
    BulkOut bo(m_impl->tag, USBTMC_MSGID_DEV_DEP_MSG_OUT, PacketSize);
    bo.append(buffer);
    auto&& msgs = static_cast<std::vector<std::string>>(bo);
    for (auto&& msg : msgs)
    {
        m_impl->write(msg);
    }
}

#undef max
std::string IviUsbTmc::readAll() const { return read(std::numeric_limits<int>::max()); }

std::string IviUsbTmc::read(size_t size) const
{
    std::string buffer;
    do
    {
        std::string packs;
        BulkIn in;
        DWORD transfered;
        m_impl->updateTag();
        { // req
            std::string requrestBuffer = BulkRequest(m_impl->tag, static_cast<unsigned int>(size - buffer.size()));
            m_impl->write(requrestBuffer);
        }
        do
        {
            std::string pack;
            pack.resize(PacketSize);
            OVERLAPPED overlapped {};
            overlapped.Offset     = 0;
            overlapped.OffsetHigh = 0;
            overlapped.hEvent     = CreateEvent(NULL, TRUE, FALSE, NULL);
            if (ReadFile(m_impl->handle, pack.data(), PacketSize, &transfered, &overlapped))
            {
            }
            else if (WaitForSingleObject(overlapped.hEvent, static_cast<DWORD>(m_attr.timeout().count())) == WAIT_TIMEOUT)
            {
                throw std::runtime_error("Read timeout.");
            }
            GetOverlappedResult(m_impl->handle, &overlapped, &transfered, true);
            packs.append(pack.c_str(), overlapped.InternalHigh);
        } while (!in.parse(packs, size, m_impl->tag));
        m_impl->avalible = !in.eom();
        buffer.append(std::move(in));
    } while (buffer.size() < size && m_impl->avalible);
    return buffer;
}

void IviUsbTmc::close() noexcept
{
    if (m_impl->handle)
    {
        CloseHandle(m_impl->handle);
    }
}

bool IviUsbTmc::connected() const noexcept { return m_impl->handle; }

size_t IviUsbTmc::avalible() const noexcept { return m_impl->avalible; }

std::vector<Address<AddressType::USB>> IviUsbTmc::listUSB()
{
    std::vector<Address<AddressType::USB>> usbs;
    auto dev        = SetupDiGetClassDevsW(&UsbTmcGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    int deviceIndex = 0;
    SP_DEVINFO_DATA data;
    data.cbSize = sizeof(data);
    while (SetupDiEnumDeviceInfo(dev, deviceIndex++, &data))
    {
        auto ident = _deviceInstanceIdentifier(data.DevInst);
        bool hasVid, hasPid;
        uint16_t vid, pid;
        std::string sn;
        if (std::tie(hasVid, vid) = _parseIdent(ident, "VID_"); !hasVid)
        {
            std::tie(hasVid, vid) = _parseIdent(ident, "VEN_");
        }
        if (!hasVid)
            continue;

        if (std::tie(hasPid, pid) = _parseIdent(ident, "PID_"); !hasPid)
        {
            std::tie(hasPid, pid) = _parseIdent(ident, "DEV_");
        }
        if (!hasPid)
            continue;

        sn = _parseSerialNumber(ident);
        usbs.emplace_back(Address<AddressType::USB> { vid, pid, sn });
    }
    SetupDiDestroyDeviceInfoList(dev);
    return usbs;
}

void IviUsbTmc::reset() {}

void IviUsbTmc::init() {}

} // namespace OpenVisa
#endif