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
#include "UsbTmc.h"
#include "UsbTmcProtocol.h"

extern "C"
{
#pragma warning(push)
#pragma warning(disable : 4200)
#include <libusb/libusb.h>
#pragma warning(pop)
}

#include <array>
#include <limits>

namespace OpenVisa
{
constexpr auto RetryMax = 5;
static int transfer(libusb_device_handle* dev_handle,
                    unsigned char endpoint,
                    unsigned char* data,
                    int length,
                    int* actual_length,
                    unsigned int timeout)
{
    int i = 0;
    int r = LIBUSB_SUCCESS;
    do
    {
        r = libusb_bulk_transfer(dev_handle, endpoint, data, length, actual_length, timeout);
        if (r == LIBUSB_ERROR_PIPE)
        {
            libusb_clear_halt(dev_handle, endpoint);
        }
        i++;
    } while ((r == LIBUSB_ERROR_PIPE) && (i < RetryMax));
    return r;
}

[[noreturn]] inline void throwLibusbError(int code) { throw std::exception(libusb_strerror(code)); }

static std::string getStringDescriptor(libusb_device_handle* handle, uint8_t descIndex)
{
    uint8_t data[128];
    int r = LIBUSB_SUCCESS;
    int i = 0;
    do
    {
        r = libusb_get_string_descriptor(handle, descIndex, 0x0409, data, 128);
        if (r == LIBUSB_ERROR_PIPE)
        {
            libusb_clear_halt(handle, 0);
        }
        i++;
    } while ((r == LIBUSB_ERROR_PIPE) && (i < RetryMax));
    if (r < 0)
        throwLibusbError(r);
    if (r < 4)
    {
        if (libusb_get_string_descriptor_ascii(handle, descIndex, data, 128) > 0)
            return std::string(reinterpret_cast<char*>(data));
    }

    if (data[0] > r || data[1] != LIBUSB_DT_STRING)
        return {};

    int length = (r - 2) / 2;
    std::string descriptor;
    descriptor.resize(length);
    for (int i = 0; i < length; i++)
        descriptor[i] = data[i * 2 + 2];
    return descriptor;
}

inline bool checkUSBTMC(const libusb_interface_descriptor& desc)
{
    return desc.bLength == 0x09 && desc.bDescriptorType == 0x04 && desc.bAlternateSetting == 0x00 && desc.bInterfaceClass == 0xFE &&
           desc.bInterfaceSubClass == 0x03 && desc.bInterfaceProtocol == 0x01;
}

struct UsbTmc::Impl
{
    libusb_context* context { nullptr };
    libusb_device_handle* handle { nullptr };
    uint8_t endpoint_in;
    uint8_t endpoint_out;
    uint8_t endpoint_interrupt_in { 0 };
    bool avalibe { false };

    uint8_t tag { 0 };
    int outMaxPacketSize { 0 };
    int inMaxPacketSize { 0 };
    void updateTag()
    {
        ++tag;
        if (tag == 0)
            ++tag;
    }

    libusb_device_handle* findAndOpenDevice(const Address<AddressType::USB>& addr)
    {
        libusb_device** devs;

        auto count = libusb_get_device_list(context, &devs);
        if (count < 0)
            throwLibusbError(static_cast<int>(count));

        auto devsScope = std::shared_ptr<int>(new int,
                                              [&](int* p)
                                              {
                                                  delete p;
                                                  libusb_free_device_list(devs, 1);
                                              });

        libusb_device_descriptor deviceDescription;
        for (int i = 0; i < count; ++i)
        {
            if (libusb_get_device_descriptor(devs[i], &deviceDescription) != LIBUSB_SUCCESS)
                continue;
            if (deviceDescription.idVendor == addr.vendorId() && deviceDescription.idProduct == addr.productId())
            {
                libusb_device_handle* h;
                if (libusb_open(devs[i], &h) != LIBUSB_SUCCESS)
                    continue;
                if (auto ret = libusb_reset_device(h); ret != LIBUSB_SUCCESS)
                {
                    libusb_close(h);
                    throwLibusbError(ret);
                }
                std::string sn;
                try
                {
                    sn = getStringDescriptor(h, deviceDescription.iSerialNumber);
                }
                catch (const std::exception& e)
                {
                    libusb_close(h);
                    throw e;
                }
                if (sn != addr.serialNumber())
                {
                    libusb_close(h);
                    continue;
                }
                return h;
            }
        }
        return nullptr;
    }
};

UsbTmc::UsbTmc(Object::Attribute const& attr) : IOBase(attr), m_impl(std::make_unique<Impl>()) { init(); }

UsbTmc::~UsbTmc() { libusb_exit(m_impl->context); }

void UsbTmc::connect(const Address<AddressType::USB>& addr, const std::chrono::milliseconds& openTimeout)
{
    m_impl->handle = m_impl->findAndOpenDevice(addr);

    if (!m_impl->handle)
        throw std::exception("USB device not found.");

    auto device = libusb_get_device(m_impl->handle);

    libusb_config_descriptor* conf_desc;
    if (auto code = libusb_get_config_descriptor(device, 0, &conf_desc); code != LIBUSB_SUCCESS) // uint8_t config_index = 0;
    {
        close();
        throwLibusbError(code);
    }

    auto confDescScope = std::shared_ptr<int>(new int,
                                              [&](int* p)
                                              {
                                                  delete p;
                                                  libusb_free_config_descriptor(conf_desc);
                                              });

    std::vector<uint8_t> ifs;
    bool isUSBTMC { false };
    for (int i = 0; i < conf_desc->bNumInterfaces; i++)
    {
        for (int j = 0; j < conf_desc->interface[i].num_altsetting; j++)
        {
            if (!checkUSBTMC(conf_desc->interface[i].altsetting[j])) // USBTMC
                continue;
            ifs.push_back(i);
            bool hasIn { false };
            bool hasOut { false };
            for (int k = 0; k < conf_desc->interface[i].altsetting[i].bNumEndpoints; k++)
            {
                auto endpoint = &conf_desc->interface[i].altsetting[j].endpoint[k];
                if ((endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) & (LIBUSB_TRANSFER_TYPE_BULK))
                { // 只获取批量传输端点
                    libusb_clear_halt(m_impl->handle, endpoint->bEndpointAddress);
                    if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN)
                    {
                        if (!m_impl->endpoint_in)
                        {
                            hasIn                   = true;
                            m_impl->endpoint_in     = endpoint->bEndpointAddress;
                            m_impl->inMaxPacketSize = endpoint->wMaxPacketSize;
                        }
                    }
                    else
                    {
                        if (!m_impl->endpoint_out)
                        {
                            hasOut                   = true;
                            m_impl->endpoint_out     = endpoint->bEndpointAddress;
                            m_impl->outMaxPacketSize = endpoint->wMaxPacketSize;
                        }
                    }
                }
                else if ((endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) & (LIBUSB_TRANSFER_TYPE_INTERRUPT))
                {
                    if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN)
                    {
                        m_impl->endpoint_interrupt_in = endpoint->bEndpointAddress;
                        libusb_clear_halt(m_impl->handle, m_impl->endpoint_interrupt_in);
                    }
                }
            }
            isUSBTMC = hasIn && hasOut;
        }
    }
    if (!isUSBTMC)
    {
        close();
        throw std::exception("USB device not found.");
    }
    // 获得通讯端点并声明内核接口
    {
        libusb_set_auto_detach_kernel_driver(m_impl->handle, 1);
        for (auto i : ifs)
        {
            auto r = libusb_claim_interface(m_impl->handle, i);
            if (r != LIBUSB_SUCCESS)
            {
                close();
                throwLibusbError(r);
            }
        }
    }
}

void UsbTmc::send(const std::string& buffer) const
{
    m_impl->updateTag();
    BulkOut bo(m_impl->tag, USBTMC_MSGID_DEV_DEP_MSG_OUT, m_impl->outMaxPacketSize);
    bo.append(buffer);

    auto&& msgs = static_cast<std::vector<std::string>>(bo);

    for (auto&& msg : msgs)
    {
        if (auto code = transfer(m_impl->handle,
                                 m_impl->endpoint_out,
                                 reinterpret_cast<unsigned char*>(msg.data()),
                                 static_cast<int>(msg.size()),
                                 nullptr,
                                 static_cast<unsigned int>(m_attr.timeout().count()));
            code != LIBUSB_SUCCESS)
        {
            libusb_reset_device(m_impl->handle);
            throwLibusbError(code);
        }
    }
}
#undef max
std::string UsbTmc::readAll() const { return read(std::numeric_limits<int>::max()); }

std::string UsbTmc::read(size_t size) const
{
    std::string buffer;
    do
    {
        std::string packs;
        BulkIn in;
        int transfered;
        m_impl->updateTag();
        { // req
            std::string requrestBuffer = BulkRequest(m_impl->tag, static_cast<unsigned int>(size - buffer.size()));
            if (auto code = transfer(m_impl->handle,
                                     m_impl->endpoint_out,
                                     reinterpret_cast<unsigned char*>(requrestBuffer.data()),
                                     static_cast<int>(requrestBuffer.size()),
                                     nullptr,
                                     static_cast<unsigned int>(m_attr.timeout().count()));
                code != LIBUSB_SUCCESS)
            {
                libusb_reset_device(m_impl->handle);
                throwLibusbError(code);
            }
        }
        do
        {
            std::string pack;
            pack.resize(m_impl->inMaxPacketSize);
            if (auto code = transfer(m_impl->handle,
                                     m_impl->endpoint_in,
                                     reinterpret_cast<unsigned char*>(pack.data()),
                                     static_cast<int>(pack.size()),
                                     &transfered,
                                     static_cast<unsigned int>(m_attr.timeout().count()));
                code != LIBUSB_SUCCESS)
            {
                libusb_reset_device(m_impl->handle);
                throwLibusbError(code);
            }
            packs.append(pack.c_str(), transfered);
        } while (!in.parse(packs, size, m_impl->tag));
        m_impl->avalibe = !in.eom();
        buffer.append(std::move(in));
    } while (buffer.size() < size && m_impl->avalibe);
    return buffer;
}

void UsbTmc::close() noexcept
{
    if (m_impl->handle)
    {
        libusb_reset_device(m_impl->handle);
        libusb_close(m_impl->handle);
        m_impl->handle = nullptr;
    }
}

bool UsbTmc::connected() const noexcept { return m_impl->handle; }

size_t UsbTmc::avalible() const noexcept { return m_impl->avalibe; }

std::vector<OpenVisa::Address<OpenVisa::AddressType::USB>> UsbTmc::listUSB()
{
    std::vector<OpenVisa::Address<OpenVisa::AddressType::USB>> address;

    std::shared_ptr<libusb_context> context(std::invoke(
                                                []
                                                {
                                                    libusb_context* context { nullptr };
#if LIBUSB_API_VERSION > 0x01000109
                                                    libusb_init_context(&context, nullptr, 0);
#else
                                                    libusb_init(&context);
#endif
                                                    return context;
                                                }),
                                            [](auto context) { libusb_exit(context); });
    libusb_device** devs;

    auto count = libusb_get_device_list(context.get(), &devs);
    if (count < 0)
        throwLibusbError(static_cast<int>(count));

    auto devsScope = std::shared_ptr<int>(new int,
                                          [&](int* p)
                                          {
                                              delete p;
                                              libusb_free_device_list(devs, 1);
                                          });

    libusb_device_descriptor deviceDescription;
    for (int i = 0; i < count; ++i)
    {
        if (libusb_get_device_descriptor(devs[i], &deviceDescription) != LIBUSB_SUCCESS)
            continue;
        auto vid = deviceDescription.idVendor;
        auto pid = deviceDescription.idProduct;
        libusb_device_handle* h;
        if (libusb_open(devs[i], &h) != LIBUSB_SUCCESS)
            continue;
        if (auto ret = libusb_reset_device(h); ret != LIBUSB_SUCCESS)
        {
            libusb_close(h);
            continue;
        }
        std::string sn;
        try
        {
            sn = getStringDescriptor(h, deviceDescription.iSerialNumber);
        }
        catch (const std::exception&)
        {
            libusb_close(h);
            continue;
        }
        libusb_close(h);
        Address<AddressType::USB> addr(vid, pid, sn);
        try
        {
            Object::Attribute attr(nullptr);
            UsbTmc tmc(attr);
            tmc.connect(addr, std::chrono::milliseconds(500));
            tmc.close();
            address.push_back(addr);
        }
        catch (...)
        {
        }
    }
    return address;
}

void UsbTmc::reset() { libusb_reset_device(m_impl->handle); }

void UsbTmc::init()
{
#if LIBUSB_API_VERSION > 0x01000109
    libusb_init_context(&m_impl->context, nullptr, 0);
#else
    libusb_init(&m_impl->context);
#endif
}

} // namespace OpenVisa
