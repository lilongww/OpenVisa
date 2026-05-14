/*********************************************************************************
**                                                                              **
**  Copyright (C) 2022-2026 LiLong                                              **
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

#include "Address.h"
#include "openvisa_global.h"

#include <chrono>
#include <concepts>
#include <format>
#include <memory>
#include <string>

namespace OpenVisa
{
template<typename T>
concept IsAddress = IsVisaAddress<T>;
template<typename T>
concept ValidConnectAddress =
    IsAddress<T> || std::same_as<std::string, T> || std::is_array_v<T> || std::same_as<const char*, T> || std::same_as<char*, T>;
class OPENVISA_EXPORT Object
{
public:
    class CommonCommand;
    class Attribute;
    Object();
    virtual ~Object() noexcept;

    template<ValidConnectAddress T>
    void connect(const T& addr,
                 const std::chrono::milliseconds& openTimeout    = std::chrono::milliseconds { 5000 },
                 const std::chrono::milliseconds& commandTimeout = std::chrono::milliseconds { 5000 });
    void close() noexcept;
    template<typename... Args>
    requires(sizeof...(Args) > 0)
    inline void send(const std::format_string<Args...>& fmt, Args&&... args);
    inline void send(const std::string& scpi);
    [[nodiscard]] std::string readAll();
    [[nodiscard]] std::tuple<std::string, bool> read(unsigned long blockSize);
    template<typename... Args>
    requires(sizeof...(Args) > 0)
    [[nodiscard]] inline std::string query(const std::format_string<Args...>& fmt, Args&&... args);
    [[nodiscard]] inline std::string query(const std::string& scpi);
    [[nodiscard]] bool connected() const noexcept;
    [[nodiscard]] Attribute& attribute() noexcept;
    [[nodiscard]] const Attribute& attribute() const noexcept;
    [[nodiscard]] CommonCommand& commonCommand() noexcept;
    [[nodiscard]] std::string removeTermChars(const std::string& source) const;
    [[nodiscard]] static std::vector<std::string> listSerialPorts();
    [[nodiscard]] static std::vector<Address<AddressType::USB>> listUSB();
    template<IsAddress T>
    [[nodiscard]] static std::string toVisaAddressString(const T& addr);
    [[nodiscard]] static AddressVariant fromVisaAddressString(const std::string& str);

protected:
    virtual void afterConnected() {};
    template<typename T>
    requires IsAddress<T> || std::same_as<std::string, T>
    void connectImpl(const T& addr, const std::chrono::milliseconds& openTimeout, const std::chrono::milliseconds& commandTimeout);

private:
    void sendImpl(const std::string& scpi);
    void throwNoConnection() const;
    std::string verifyCommand();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

template<ValidConnectAddress T>
inline void Object::connect(const T& addr, const std::chrono::milliseconds& openTimeout, const std::chrono::milliseconds& commandTimeout)
{
    connectImpl(AddressHelper(addr).address, openTimeout, commandTimeout);
}

template<typename... Args>
requires(sizeof...(Args) > 0)
inline void Object::send(const std::format_string<Args...>& fmt, Args&&... args)
{
    sendImpl(std::format(fmt, std::forward<Args&&>(args)...));
}

inline void Object::send(const std::string& scpi)
{
    sendImpl(scpi);
}

template<typename... Args>
requires(sizeof...(Args) > 0)
inline std::string Object::query(const std::format_string<Args...>& fmt, Args&&... args)
{
    send(fmt, std::forward<Args&&>(args)...);
    return readAll();
}

inline std::string Object::query(const std::string& scpi)
{
    send(scpi);
    return readAll();
}

template<typename T>
struct VisaAdl
{
    inline static std::string toScpi(const T& arg)
    {
        static_assert(!sizeof(T), "Please define your ADL function: std::string toScpi(YourType).");
    }
    inline static void fromScpi(const std::string& ret, T& out)
    {
        static_assert(!sizeof(T), "Please define your ADL function: void fromScpi(const std::string&, YourType&).");
    }
};
} // namespace OpenVisa

#define VISA_ENUM_ADL_DECLARE(EnumBegin, EnumEnd, ...)                                                                                     \
    template<>                                                                                                                             \
    struct VisaAdl<decltype(EnumBegin)>                                                                                                    \
    {                                                                                                                                      \
        using Enum = decltype(EnumBegin);                                                                                                  \
        using Int  = typename std::underlying_type<Enum>::type;                                                                            \
        inline static constexpr std::initializer_list<std::string_view> m_enumStrings {##__VA_ARGS__ };                                    \
        static_assert(std::is_same_v<Enum, decltype(EnumEnd)>, "EnumBegin and EnumEnd are not of the same type.");                         \
        static_assert(m_enumStrings.size() - 1 == static_cast<Int>(EnumEnd) - static_cast<Int>(EnumBegin),                                 \
                      "Discontinuous enum or strings is not enough.");                                                                     \
        inline static std::string toScpi(Enum e)                                                                                           \
        {                                                                                                                                  \
            const auto& view = (*(m_enumStrings.begin() + (static_cast<Int>(e) - static_cast<Int>(EnumBegin))));                           \
            return std::string(view.data(), view.size());                                                                                  \
        }                                                                                                                                  \
        inline static void fromScpi(const std::string& scpi, Enum& e)                                                                      \
        {                                                                                                                                  \
            if (auto it = std::ranges::find_if(m_enumStrings,                                                                              \
                                               [&](const auto& str)                                                                        \
                                               {                                                                                           \
                                                   return scpi.starts_with(str);                                                           \
                                               });                                                                                         \
                it != m_enumStrings.end())                                                                                                 \
                e = static_cast<Enum>(std::distance(m_enumStrings.begin(), it) + static_cast<Int>(EnumBegin));                             \
            else                                                                                                                           \
                throw std::runtime_error("Unknown enum type.");                                                                            \
        }                                                                                                                                  \
    }
