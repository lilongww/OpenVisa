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
#include "Object.h"
#include "Attribute.h"
#include "CommonCommand.h"
#include "Private/HiSLIP.h"
#include "Private/IOTrace.h"
#include "Private/IviUsbTmc.h"
#include "Private/RawSocket.h"
#include "Private/SerialPort.h"
#include "Private/UsbTmc.h"
#include "Private/VXI11.h"
#include "Private/VisaExceptionHelper.h"
#include "SerialPortInfo.h"
#include "Utils.h"

#include <algorithm>
#include <ranges>
#include <thread>

template<typename... Args>
struct Overload : public Args...
{
    using Args::operator()...;
};

namespace OpenVisa
{
struct Object::Impl
{
    std::shared_ptr<IOBase> io;
    Attribute attr;
    CommonCommand commonCommand;
    bool verified { false };
    std::string address;
    std::chrono::system_clock::duration beforeSendTime { std::chrono::system_clock::now().time_since_epoch() };
    inline Impl(Object& obj) : commonCommand(obj), attr(&io)
    {
    }
    std::shared_ptr<IOTrace> getOrCreateIOTrace()
    {
        if (!ioTrace)
            ioTrace = std::make_shared<IOTrace>(attr);
        return ioTrace;
    }

private:
    std::shared_ptr<IOTrace> ioTrace;
};

/*!
    \class      OpenVisa::Object
    \brief      仪器通讯基础库.
    \ingroup    openvisa
    \inmodule   OpenVisa
    \inheaderfile Object.h
*/

// clang-format off
/*!
    \fn         template <VaildConnectAddress T> void OpenVisa::Object::connect(const T& addr, const std::chrono::milliseconds& openTimeout = std::chrono::milliseconds { 5000 }, const std::chrono::milliseconds& commandTimeout = std::chrono::milliseconds { 5000 });
    \brief      连接到地址为 \a addr 的设备，并且连接超时时间为 \a openTimeout, 通讯超时时间为 \a commandTimeout.
*/
// clang-format on
/*!
    \fn         template <typename ...Args> inline void OpenVisa::Object::send(const std::string& fmt, const Args&... args);
    \brief      格式化发送scpi指令，参考 std::format 格式化设置, \a fmt, \a args.
    \sa         readAll, read
*/

/*!
    \fn         template <typename ...Args> inline std::string OpenVisa::Object::query(const std::string& fmt, const Args&... args);
    \brief      格式化发送查询scpi指令，参考 std::format 格式化设置, \a fmt, \a args, 并返回查询值.
    \sa         send, readAll
*/

/*!
    \fn         template <IsAddress T> inline std::string OpenVisa::Object::toVisaAddressString(const T& addr);
    \brief      将地址类型 \a addr 转换为 Visa 连接描述字符串.
    \sa         fromVisaAddressString
*/

template<>
OPENVISA_EXPORT std::string Object::toVisaAddressString<Address<AddressType::VXI11>>(const Address<AddressType::VXI11>& addr)
{
    return std::format("TCPIP::{}::{}::INSTR", addr.ip(), addr.subAddress());
}

template<>
OPENVISA_EXPORT std::string Object::toVisaAddressString<Address<AddressType::RawSocket>>(const Address<AddressType::RawSocket>& addr)
{
    return std::format("TCPIP::{}::{}::SOCKET", addr.ip(), addr.port());
}

template<>
OPENVISA_EXPORT std::string Object::toVisaAddressString<Address<AddressType::HiSLIP>>(const Address<AddressType::HiSLIP>& addr)
{
    return std::format("TCPIP::{}::{}", addr.ip(), addr.subAddress());
}

template<>
OPENVISA_EXPORT std::string Object::toVisaAddressString<Address<AddressType::SerialPort>>(const Address<AddressType::SerialPort>& addr)
{
    if (!addr.portName().starts_with("COM") && !addr.portName().starts_with("com"))
        return {};

    std::string temp;
    std::ranges::copy(addr.portName() | std::views::filter(
                                            [](auto ch)
                                            {
                                                return ch >= '0' && ch <= '9';
                                            }),
                      std::back_inserter(temp));
    try
    {
        auto port = std::stoul(temp);
        return std::format("ASRL{}::INSTR", port);
    }
    catch (...)
    {
        return {};
    }
}

template<>
OPENVISA_EXPORT std::string Object::toVisaAddressString<Address<AddressType::USB>>(const Address<AddressType::USB>& addr)
{
    return std::format("USB::{:#04X}::{:#04X}::{}::INSTR", addr.vendorId(), addr.productId(), addr.serialNumber());
}

/*!
    \brief      构造函数.
*/
Object::Object() : m_impl(std::make_unique<Impl>(*this))
{
}

/*!
    \brief      析构函数.
*/
Object::~Object() noexcept
{
    close();
}

/*!
    \brief      关闭仪器连接.
*/
void Object::close() noexcept
{
    if (connected())
    {
        m_impl->io->close();
        m_impl->io = nullptr;
    }
}

template<>
OPENVISA_EXPORT void Object::connectImpl<Address<AddressType::RawSocket>>(const Address<AddressType::RawSocket>& addr,
                                                                          const std::chrono::milliseconds& openTimeout /*= 5000*/,
                                                                          const std::chrono::milliseconds& commandTimeout /*= 5000*/)
{
    m_impl->attr.setTimeout(commandTimeout);
    auto socket = std::make_shared<RawSocket>(m_impl->attr);
    visaReThrow(m_impl->attr,
                [&]
                {
                    socket->connect(addr, openTimeout);
                });
    m_impl->io = socket;
    afterConnected();
    m_impl->address = Object::toVisaAddressString(addr);
}

template<>
OPENVISA_EXPORT void Object::connectImpl<Address<AddressType::SerialPort>>(const Address<AddressType::SerialPort>& addr,
                                                                           const std::chrono::milliseconds& openTimeout /*= 5000*/,
                                                                           const std::chrono::milliseconds& commandTimeout /*= 5000*/)
{
    m_impl->attr.setTimeout(commandTimeout);
    auto serialPort = std::make_shared<SerialPort>(m_impl->attr);
    visaReThrow(m_impl->attr,
                [&]
                {
                    serialPort->connect(addr, openTimeout);
                });
    m_impl->io = serialPort;
    afterConnected();
    m_impl->address = Object::toVisaAddressString(addr);
}

template<>
OPENVISA_EXPORT void Object::connectImpl<Address<AddressType::USB>>(const Address<AddressType::USB>& addr,
                                                                    const std::chrono::milliseconds& openTimeout /*= 5000*/,
                                                                    const std::chrono::milliseconds& commandTimeout /*= 5000*/)
{
    m_impl->attr.setTimeout(commandTimeout);
    if (std::ranges::any_of(IviUsbTmc::listUSB(), std::bind_front(std::equal_to {}, addr)))
    {
        auto usb = std::make_shared<IviUsbTmc>(m_impl->attr);
        visaReThrow(m_impl->attr,
                    [&]
                    {
                        usb->connect(addr, openTimeout);
                    });
        m_impl->io = usb;
    }
    else
    {
        auto usb = std::make_shared<UsbTmc>(m_impl->attr);
        visaReThrow(m_impl->attr,
                    [&]
                    {
                        usb->connect(addr, openTimeout);
                    });
        m_impl->io = usb;
    }
    afterConnected();
    m_impl->address = Object::toVisaAddressString(addr);
}

template<>
OPENVISA_EXPORT void Object::connectImpl<Address<AddressType::VXI11>>(const Address<AddressType::VXI11>& addr,
                                                                      const std::chrono::milliseconds& openTimeout /*= 5000*/,
                                                                      const std::chrono::milliseconds& commandTimeout /*= 5000*/)
{
    m_impl->attr.setTimeout(commandTimeout);
    auto vxi11 = std::make_shared<VXI11>(m_impl->attr);
    visaReThrow(m_impl->attr,
                [&]
                {
                    vxi11->connect(addr, openTimeout);
                });
    m_impl->io = vxi11;
    afterConnected();
    m_impl->address = Object::toVisaAddressString<Address<AddressType::VXI11>>(addr);
}

template<>
OPENVISA_EXPORT void Object::connectImpl<Address<AddressType::HiSLIP>>(const Address<AddressType::HiSLIP>& addr,
                                                                       const std::chrono::milliseconds& openTimeout /*= 5000*/,
                                                                       const std::chrono::milliseconds& commandTimeout /*= 5000*/)
{
    m_impl->attr.setTimeout(commandTimeout);
    auto hiSLIP = std::make_shared<HiSLIP>(m_impl->attr);
    visaReThrow(m_impl->attr,
                [&]
                {
                    hiSLIP->connect(addr, openTimeout);
                });
    m_impl->io = hiSLIP;
    afterConnected();
    m_impl->address = Object::toVisaAddressString(addr);
}

template<>
OPENVISA_EXPORT void Object::connectImpl<std::string>(const std::string& addr,
                                                      const std::chrono::milliseconds& openTimeout /*= 5000*/,
                                                      const std::chrono::milliseconds& commandTimeout /*= 5000*/)
{
    AddressVariant addressVariant;
    visaReThrow(m_impl->attr,
                [&]
                {
                    addressVariant = fromVisaAddressString(addr);
                });
    std::visit(Overload { [&](const std::monostate&)
                          {
                              visaThrow(m_impl->attr, std::make_error_code(std::errc::address_not_available).message());
                          },
                          [&](const auto& addr)
                          {
                              connectImpl(addr, openTimeout, commandTimeout);
                          } },
               addressVariant);
}

/*!
    \brief      读取所有数据.
    \sa         read
*/
std::string Object::readAll()
{
    auto ret = visaReThrow(m_impl->attr,
                           [&]
                           {
                               throwNoConnection();
                               try
                               {
                                   return m_impl->io->readAll();
                               }
                               catch (...)
                               {
                                   if (m_impl->attr.commandVerify())
                                   {
                                       m_impl->io->reset();
                                       auto error       = verifyCommand();
                                       m_impl->verified = false;
                                       if (!error.empty())
                                           throw std::runtime_error(error);
                                   }
                                   throw;
                               }
                           });
    if (m_impl->attr.ioTraceEnable())
        m_impl->getOrCreateIOTrace()->rx(m_impl->address, ret);
    return ret;
}

/*!
    \brief      读取 \a blockSize 大小的数据，返回读到的数据，当没有更多的数据可读时，布尔值为false.
    \sa         readAll
*/
std::tuple<std::string, bool> Object::read(unsigned long blockSize)
{
    auto ret = visaReThrow(m_impl->attr,
                           [&]() -> std::tuple<std::string, bool>
                           {
                               throwNoConnection();
                               return { m_impl->io->read(blockSize), m_impl->io->avalible() };
                           });
    if (m_impl->attr.ioTraceEnable())
        m_impl->getOrCreateIOTrace()->rx(m_impl->address, std::get<0>(ret));
    return ret;
}

/*!
    \brief      返回仪器是否已连接.
*/
bool Object::connected() const noexcept
{
    return m_impl->io ? m_impl->io->connected() : false;
}

/*!
    \brief      返回属性设置.
*/
Object::Attribute& Object::attribute() noexcept
{
    return m_impl->attr;
}

const Object::Attribute& Object::attribute() const noexcept
{
    return m_impl->attr;
}

/*!
    \brief      IEEE488.2公共指令接口.
*/
Object::CommonCommand& Object::commonCommand() noexcept
{
    return m_impl->commonCommand;
}

/*!
    \brief      移除字符串 \a source 尾部的终结符.
    \note       若未启用终结符，则该函数不做任何处理.
    \sa         query, readAll
*/
std::string Object::removeTermChars(const std::string& source) const
{
    if (attribute().terminalCharsEnable() && source.ends_with(attribute().terminalChars()))
    {
        return source.substr(0, source.size() - attribute().terminalChars().size());
    }
    return source;
}

/*!
    \brief      列出所有串口名称.
*/
std::vector<std::string> Object::listSerialPorts()
{
    auto ports = SerialPortInfo::listPorts();
    std::vector<std::string> names(ports.size());
    std::transform(ports.begin(),
                   ports.end(),
                   names.begin(),
                   [](const auto& port)
                   {
                       return port.portName();
                   });
    return names;
}

/*!
    \brief      列出所有USBTMC驱动设备.
*/
std::vector<Address<AddressType::USB>> Object::listUSB()
{
    auto vec = UsbTmc::listUSB();
    vec.append_range(IviUsbTmc::listUSB());
    return vec;
}

/*!
    \brief      从Visa连接描述字符串 \a str 转换为地址类型联合体.
    \sa         toVisaAddressString
*/
AddressVariant Object::fromVisaAddressString(const std::string& str)
{
    try
    {
        std::string addr;
        std::ranges::transform(str,
                               std::back_inserter(addr),
                               [=](auto c)
                               {
                                   return std::tolower(c);
                               });

        auto tokens = split(addr, "::");
        if (tokens.size() < 2)
            return std::monostate {};
        if (tokens.at(0).starts_with("tcpip"))
        {
            if (addr.contains("hislip"))
            {
                auto temp = split(tokens.at(2), ",");
                return Address<AddressType::HiSLIP>(
                    tokens.at(1), temp.at(0), temp.size() == 2 ? static_cast<unsigned short>(std::stoul(temp.at(1))) : 4880);
            }
            if (addr.ends_with("instr"))
                return Address<AddressType::VXI11>(tokens.at(1), tokens.size() == 3 ? "inst0" : tokens.at(2));
            else if (addr.ends_with("socket"))
                return Address<AddressType::RawSocket>(tokens.at(1), static_cast<unsigned short>(std::stoul(tokens.at(2))));
        }
        else if (tokens.at(0).starts_with("asrl"))
        {
            std::string temp;
            std::ranges::copy(tokens.at(0) | std::views::filter(
                                                 [](auto ch)
                                                 {
                                                     return ch >= '0' && ch <= '9';
                                                 }),
                              std::back_inserter(temp));
            auto port = std::stoul(temp);
            return AddressVariant { Address<AddressType::SerialPort>(std::format("COM{}", port)) };
        }
        else if (tokens.at(0).starts_with("usb"))
        {
            auto visaTokens = split(str, "::");
            if (visaTokens.size() == 5 && !visaTokens[4].ends_with("instr") && !visaTokens[4].ends_with("INSTR"))
                return std::monostate {};
            return Address<AddressType::USB>(static_cast<unsigned short>(std::stoul(visaTokens.at(1), nullptr, 16)),
                                             static_cast<unsigned short>(std::stoul(visaTokens.at(2), nullptr, 16)),
                                             visaTokens.at(3));
        }
    }
    catch (const std::exception&)
    {
    }
    return std::monostate {};
}

void Object::sendImpl(const std::string& scpi)
{
    throwNoConnection();

    if (m_impl->attr.communicationInterval() != std::chrono::milliseconds { 0 }) // 通信间隔不为0时，等待通信间隔时间
    {
        while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch() -
                                                                     m_impl->beforeSendTime) <= m_impl->attr.communicationInterval())
        {
            std::this_thread::yield();
        }
        m_impl->beforeSendTime = std::chrono::system_clock::now().time_since_epoch();
    }

    if (m_impl->attr.autoAppendTerminalChars() && !scpi.ends_with(m_impl->attr.terminalChars()))
    {
        m_impl->io->send(scpi + m_impl->attr.terminalChars());
        if (m_impl->attr.ioTraceEnable())
            m_impl->getOrCreateIOTrace()->tx(m_impl->address, scpi + m_impl->attr.terminalChars());
    }
    else
    {
        m_impl->io->send(scpi);
        if (m_impl->attr.ioTraceEnable())
            m_impl->getOrCreateIOTrace()->tx(m_impl->address, scpi);
    }
    if (m_impl->attr.commandVerify() && !scpi.contains("?")) // 非查询指令直接进行指令验证
    {
        auto error       = verifyCommand();
        m_impl->verified = false;
        if (!error.empty())
            throw std::runtime_error(error);
    }
}

void Object::throwNoConnection() const
{
    if (!connected())
    {
        throw std::runtime_error("Device not connected.");
    }
}

std::string Object::verifyCommand()
{
    if (m_impl->verified) // 中断递归死循环
    {
        m_impl->verified = false;
        return {};
    }
    m_impl->verified = true;
    auto esr         = this->commonCommand().esr();
    if (esr.commandError)
        return "Command error.";
    else if (esr.deviceError)
        return "Device error.";
    else if (esr.executionError)
        return "Execution error.";
    else if (esr.queryError)
        return "Query error.";
    return {};
}

} // namespace OpenVisa
