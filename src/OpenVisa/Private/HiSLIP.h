﻿/*********************************************************************************
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
#pragma once

#include "HiSLIPProtocol.h"
#include "IOBase.h"

namespace OpenVisa
{
class HiSLIP : public IOBase
{
public:
    HiSLIP(Object::Attribute const& attr);
    ~HiSLIP();
    void connect(const Address<AddressType::HiSLIP>& address, const std::chrono::milliseconds& openTimeout);
    void send(const std::string& buffer) const override;
    std::string readAll() const override;
    std::string read(size_t size) const override;
    void close() noexcept override;
    bool connected() const noexcept override;
    size_t avalible() const noexcept override;

private:
    void initialize(std::string_view subAddr);
    void initializeAsync();
    void errorCheck(HS::HSBuffer& buffer) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
} // namespace OpenVisa