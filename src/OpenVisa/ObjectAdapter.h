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

#include "Object.h"

namespace OpenVisa
{
class ObjectAdapter
{
public:
    [[nodiscard]] inline Object& object() { return m_object; }
    template<typename... Args>
    inline void send(const std::string& fmt, const Args&... args);
    [[nodiscard]] inline std::string readAll() { return m_object.readAll(); }
    [[nodiscard]] inline std::tuple<std::string, bool> read(unsigned long blockSize) { return m_object.read(blockSize); }
    template<typename... Args>
    [[nodiscard]] inline std::string query(const std::string& fmt, const Args&... args);
    [[nodiscard]] inline Object::Attribute& attribute() noexcept { return m_object.attribute(); }
    [[nodiscard]] inline const Object::Attribute& attribute() const noexcept { return m_object.attribute(); }
    [[nodiscard]] inline Object::CommonCommand& commonCommand() noexcept { return m_object.commonCommand(); }
    [[nodiscard]] inline std::string removeTermChars(const std::string& source) const { return m_object.removeTermChars(source); }

protected:
    inline ObjectAdapter(Object& object) : m_object(object) {}
    virtual ~ObjectAdapter() {}

private:
    Object& m_object;
};
template<typename... Args>
inline void ObjectAdapter::send(const std::string& fmt, const Args&... args)
{
    m_object.send(fmt, std::forward<const Args&>(args)...);
}
template<typename... Args>
inline std::string ObjectAdapter::query(const std::string& fmt, const Args&... args)
{
    return m_object.query(fmt, std::forward<const Args&>(args)...);
}
} // namespace OpenVisa