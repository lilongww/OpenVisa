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
#pragma once

#include "Endian.h"

#include <array>
#include <string>

namespace OpenVisa
{
namespace HS
{
#pragma pack(push)
#pragma pack(1)

enum class MessageType : unsigned char
{
    Initialize,
    InitializeResponse,
    FatalError,
    Error,
    AsyncLock,
    AsyncLockResponse,
    Data,
    DataEnd,
    DeviceClearComplete,
    DeviceClearAcknowledge,
    AsyncRemoteLocalControl,
    AsyncRemoteLocalResponse,
    Trigger,
    Interrupted,
    AsyncInterrupted,
    AsyncMaximumMessageSize,
    AsyncMaximumMessageSizeResponse,
    AsyncInitialize,
    AsyncInitializeResponse,
    AsyncDeviceClear,
    AsyncServiceRequest,
    AsyncStatusQuery,
    AsyncStatusResponse,
    AsyncDeviceClearAcknowledge,
    AsyncLockInfo,
    AsyncLockInfoResponse
};

enum class FatalErrorCode : unsigned char
{
    Unknown,
    MessageHeaderError,
    CommunicationWithoutBothChannel,
    InvalidInitSequence,
    ServerRefusedWithMaximumClients,
    SecureConnectionFailed
};

enum class ErrorCode : unsigned char
{
    Unknown,
    UnrecognizedMessageType,
    UnrecognizedControlCode,
    UnrecognizedVendorDefinedMessage,
    MessageTooLarge,
    AuthenticationFailed
};

struct MessageHeader
{
    unsigned char prologue[2] { 'H', 'S' };
    MessageType messageType;
    unsigned char controlCode;
    union
    {
        struct
        {
            unsigned short low;
            unsigned short high;
        } word;
        unsigned int value;
    } messageParameter { .value = 0 };
    size_t payloadLength { 0 };
    // Data
};

static_assert(sizeof(MessageHeader) == 16);

class HSBuffer : public std::string
{
public:
    using std::string::basic_string;
    template<typename T>
    requires std::is_arithmetic_v<T> || std::is_enum_v<T>
    inline void append(const T bytes)
    {
        auto val = toBigEndian(bytes);
        std::string::append(reinterpret_cast<const char*>(&val), sizeof(T));
    }
    template<typename T>
    requires std::is_arithmetic_v<T> || std::is_enum_v<T>
    inline size_t take(T& val)
    {
        val = fromBigEndian(*reinterpret_cast<T*>(data()));
        erase(begin(), begin() + sizeof(T));
        return sizeof(T);
    }
    inline bool isVaild() const noexcept
    {
        if (size() >= 2)
            return *c_str() == 'H' && *(c_str() + 1) == 'S';
        return false;
    }
    inline MessageType messageType() const { return static_cast<MessageType>(*(c_str() + 2)); }
    inline size_t payloadLength() const { return fromBigEndian(*reinterpret_cast<const size_t*>(&(c_str()[sizeof(MessageHeader) - 8]))); }
    inline bool isEnough() const noexcept // 字节数足够
    {
        if (sizeof(MessageHeader) > size())
            return false;
        else if (sizeof(MessageHeader) + payloadLength() > size())
            return false;
        return true;
    }
};

namespace Req
{
template<MessageType type>
class Request
{
public:
    inline Request(size_t payloadLen) noexcept { m_header.payloadLength = payloadLen; }
    virtual operator HSBuffer() const
    {
        HSBuffer buffer;
        buffer.append(m_header.prologue[0]);
        buffer.append(m_header.prologue[1]);
        buffer.append(m_header.messageType);
        buffer.append(m_header.controlCode);
        buffer.append(m_header.messageParameter.value);
        buffer.append(m_header.payloadLength);
        return buffer;
    }

protected:
    MessageHeader m_header { .messageType = type };
};

class Initialize : public Request<MessageType::Initialize>
{
public:
    inline Initialize(unsigned short version, unsigned short venderId, std::string_view subAddress)
        : Request(subAddress.size()), m_subAddress(subAddress)
    {
        m_header.messageParameter.word.high = version;
        m_header.messageParameter.word.low  = venderId;
        m_header.payloadLength              = subAddress.size();
    }
    operator HSBuffer() const override
    {
        auto buffer = Request::operator OpenVisa::HS::HSBuffer();
        if (m_header.payloadLength)
            buffer.std::string::append(m_subAddress);
        return buffer;
    }

private:
    std::string m_subAddress;
};

class AsyncInitialize : public Request<MessageType::AsyncInitialize>
{
public:
    inline AsyncInitialize(unsigned int sessionId) noexcept : Request(0) { m_header.messageParameter.value = sessionId; }
};

class Data : public Request<MessageType::Data>
{
public:
    inline Data(const std::string& data, unsigned int msgId, bool rmt = false) : Request(data.size()), m_data(data)
    {
        if (rmt)
            m_header.controlCode |= 0x01;
        m_header.messageParameter.value = msgId;
    }
    operator HSBuffer() const override
    {
        auto buffer = Request::operator OpenVisa::HS::HSBuffer();
        if (m_header.payloadLength)
            buffer.std::string::append(m_data);
        return buffer;
    }

private:
    std::string m_data;
};

class DataEnd : public Request<MessageType::DataEnd>
{
public:
    inline DataEnd(const std::string& data, unsigned int msgId, bool rmt = true) : Request(data.size()), m_data(data)
    {
        if (rmt)
            m_header.controlCode |= 0x01;
        m_header.messageParameter.value = msgId;
    }
    operator HSBuffer() const override
    {
        auto buffer = Request::operator OpenVisa::HS::HSBuffer();
        if (m_header.payloadLength)
            buffer.std::string::append(m_data);
        return buffer;
    }

private:
    std::string m_data;
};
} // namespace Req

namespace Resp
{
template<MessageType type>
class Response
{
public:
    virtual void parse(HSBuffer& buffer)
    {
        buffer.take(m_header.prologue[0]);
        buffer.take(m_header.prologue[1]);
        buffer.take(m_header.messageType);
        buffer.take(m_header.controlCode);
        buffer.take(m_header.messageParameter.value);
        buffer.take(m_header.payloadLength);
        if (m_header.payloadLength)
        {
            m_data = HSBuffer(buffer.begin(), buffer.begin() + m_header.payloadLength);
            buffer.erase(buffer.begin(), buffer.begin() + m_header.payloadLength);
        }
    }

protected:
    MessageHeader m_header;
    HSBuffer m_data;
};

class AsyncInitialize : public Response<MessageType::AsyncInitializeResponse>
{
public:
    inline unsigned int serverVendorId() const noexcept { return m_header.messageParameter.value; }
};

class Initialize : public Response<MessageType::InitializeResponse>
{
public:
    inline bool preferOverlap() const noexcept { return m_header.controlCode & 0x01; }
    inline bool preferSynchronized() const noexcept { return !preferOverlap(); }
    inline unsigned short version() const noexcept { return m_header.messageParameter.word.high; }
    inline unsigned short sessionId() const noexcept { return m_header.messageParameter.word.low; }
    // version >= 2.0
    inline bool encryptionMandatory() const noexcept { return m_header.controlCode & 0x02; }
    inline bool secureConnectionRequired() const noexcept { return m_header.controlCode & 0x04; }
    inline unsigned char venderSpecific() const noexcept { return m_header.controlCode >> 6; }
};

class Data : public Response<MessageType::Data>
{
public:
    inline HSBuffer data() const noexcept { return m_data; }
};

class DataEnd : public Response<MessageType::DataEnd>
{
public:
    inline HSBuffer data() const noexcept { return m_data; }
};

class FatalError : public Response<MessageType::FatalError>
{
public:
    inline FatalErrorCode code() const noexcept { return static_cast<FatalErrorCode>(m_header.controlCode); }
};

class Error : public Response<MessageType::Error>
{
public:
    inline ErrorCode code() const noexcept { return static_cast<ErrorCode>(m_header.controlCode); }
};
} // namespace Resp
#pragma pack(pop)
} // namespace HS
} // namespace OpenVisa