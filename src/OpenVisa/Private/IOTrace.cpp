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

#include "IOTrace.h"

#include <boost/asio.hpp>

#include <algorithm>
#include <ranges>

namespace OpenVisa
{
struct TraceData
{
    const bool& tx;
    const std::string& address;
    const std::string& data;
    inline operator std::string() const
    {
        std::string buf;
        buf.push_back(tx);
        serialize(buf, address);
        serialize(buf, data);
        return buf;
    }

private:
    inline void serialize(std::string& buf, const std::string& str) const
    {
        auto size  = str.size();
        auto begin = reinterpret_cast<const char*>(&size);
        buf.append_range(std::string_view(begin, begin + sizeof(size)));
        buf.append(str);
    }
};

struct IOTrace::Impl
{
    boost::asio::io_context io_context;
    boost::asio::ip::udp::socket socket { io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0) };
};

IOTrace::IOTrace(Object::Attribute const& attr) : m_impl(std::make_unique<Impl>()), m_attr(attr) {}

IOTrace::~IOTrace() {}

void IOTrace::tx(const std::string& address, const std::string& data) { trace(true, address, data); }

void IOTrace::rx(const std::string& address, const std::string& data) { trace(false, address, data); }

void IOTrace::trace(bool tx, const std::string& address, const std::string& data)
{
    m_impl->socket.send_to(boost::asio::buffer(static_cast<std::string>(TraceData { tx, address, data })),
                           boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), m_attr.ioTracePort()));
}

} // namespace OpenVisa
