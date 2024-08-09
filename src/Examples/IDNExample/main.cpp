﻿#include <OpenVisa/Attribute.h>
#include <OpenVisa/Object.h>
#include <OpenVisa/Utils.h>

#include <iostream>

int main(int argc, char* argv[])
{
    try
    {
        OpenVisa::Object obj;
        // obj.connect(OpenVisa::Address<OpenVisa::AddressType::HiSLIP>("192.168.2.111"));
        // obj.connect(OpenVisa::Address<OpenVisa::AddressType::VXI11>("192.168.2.111", "inst0"));
        // obj.connect("TCPIP::192.168.2.111::INSTR");
        obj.connect("USB::0XF4EC::0X1101::SDG6XFCQ7R0609::INSTR");
        obj.attribute().setDeviceName("ZNA 43");
        // const char* str = "TCPIP::192.168.2.111::INSTR";
        //  obj.connect(str);
        // obj.connect(const_cast<char*>(str));
        //  obj.connect(OpenVisa::Address<OpenVisa::AddressType::USB>(0x1AB1, 0x0514, "DS7A241200172"));
        //   for (int i = 0; i < 1024; i++)
        //       std::cout << obj.query("*IDN?");
        //    auto buffer = obj.query("MMEMory:DATA? 'C:\\R_S\\Instr\\install\\FoxitReader_Setup.exe'");
        //    auto buffer = obj.query("MMEMory:DATA? 'C:\\Program Files (x86)\\Rohde-Schwarz\\RsVisa\\VisaManual.pdf'");
        //    std::cout << buffer.size();
        //    std::string buffer(1024 * 1024, '1');
        //    auto str = std::to_string(1024 * 1024);
        //    obj.send("MMEM:CDIR 'C:\\R_S'");
        //    obj.send("MMEMory:DATA '1.txt',#{}{}{}", str.size(), str, buffer);
        // obj.send(":SENSe1:SWEep:POINts?");
        // std::cout << obj.readAll() << std::endl;
        for (int i = 0; i < 10000; i++)
            std::cout << obj.query("*IDN?");
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
    return 0;
}