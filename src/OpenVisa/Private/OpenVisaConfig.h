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

#include "../Address.h"
#include "Asrl.h"

#include <map>
#include <memory>
#include <optional>

namespace OpenVisa
{
class OpenVisaConfig
{
public:
    static OpenVisaConfig& instance();
    const std::map<unsigned int, Asrl>& asrlMap() const;
    std::optional<int> toAsrl(const std::string& portName) const;
    std::optional<Asrl> fromAsrl(unsigned int asrl) const;
    void loadConfig();
    void saveConfig() const;

private:
    void updateAsrl();
    void addAsrl(const Asrl& addr);
    void saveDefault();

protected:
    OpenVisaConfig();
    ~OpenVisaConfig();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
} // namespace OpenVisa
