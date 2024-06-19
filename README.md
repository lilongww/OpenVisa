# OpenVisa

[TOC]

## 简介

测试测量仪器通信库，包含TCP，Serial Port，USB，VXI11, HiSLIP等通信协议。
~~API文档：https://llongww.gitee.io/openvisa~~

由于gitee page服务已关闭，暂时无法提供在线帮助文档，请克隆仓库后，打开`doc/index.html` 查看离线API文档。

现在已发布基于OpenVisa的应用程序：OpenVisaApplication，可访问地址https://gitee.com/llongww/OpenVisaApplication获取。

## 一、编写本项目的目的

1. 常用的NI-VISA是一个商业软件，并没有多少开源免费软件可以替代；
2. 我只是想要简简单单地和设备通信而已，受够了每次安装我们的软件，都要携带几个G的通信库安装包。

## 二、开发计划
- [x] TCP socket Ascii通信
- [x] 串口 Ascii通信
- [x] USBTMC
- [x] VXI-11
- [x] HiSLIP
- [ ] GPIB

由于个人精力原因，上述开发仅实现了基本通信功能，并未实现全功能VISA，如VISA的Event等功能，虽然未来计划是跨平台的，但目前部分功能仅实现了Windows端。

## 三、编译

本项目基于C++20标准编写，在VS2022下编译通过。

### 1. 第三方库

* boost
* libusb

>我并没有太多时间来验证各版本的boost和libusb是否存在编译问题，我用的boost版本为 1.78.0，libusb库为1.0.25，如果使用其它版本的第三方库出现问题时，可以给我反馈。

### 2. 环境变量设置

* 添加 ```BOOST_ROOT``` 变量，并设置值为 BOOST库根目录，比如：C:\local\boost_1_78_0
* 添加 ```BOOST_LIBRARYDIR``` 变量，并设置值为 BOOST lib库所在目录，比如：%BOOST_ROOT%\lib64-msvc-14.3
* 添加 ```BOOST_INCLUDEDIR``` 变量，并设置值为 BOOST 头文件目录，比如 %BOOST_ROOT%

### 3. CMake设置

如果libusb的目录与OpenVisa在同级目录中，一般来说下述变量将会自动查找

* 设置 ```LIBUSB_INCLUDEDIR``` 为 libusb.h 头文件所在目录的上一级，比如 libusb.h 的所在目录为：F:\3rd\libusb\libusb\libusb.h，则将 ```LIBUSB_INCLUDEDIR``` 设为 ```F:/3rd/libusb```

* 设置```LIBUSB_INCLUDEDIR``` 后一般情况下lib和dll的路径将会自动查找如果没有自动查找，则设置如下变量

* 设置 ```LIBUSB_DEBUG_LIBRARY``` 为Debug模式下的 libusb-1.0.lib 文件所在的路径，如：```F:/3rd/libusb/x64/Debug/dll/libusb-1.0.lib```

* 设置 ```LIBUSB_RELEASE_LIBRARY``` 为Release模式下的 libusb-1.0.lib 文件所在的路径，如：```F:/3rd/libusb/x64/Release/dll/libusb-1.0.lib```

* 设置 ```LIBUSB_DEBUG_DLL``` 为Debug模式下的 libusb-1.0.dll 文件所在的路径，如：```F:/3rd/libusb/x64/Debug/dll/libusb-1.0.dll```

* 设置 ```LIBUSB_RELEASE_DLL``` 为Release模式下的 libusb-1.0.dll 文件所在的路径，如：```F:/3rd/libusb/x64/Release/dll/libusb-1.0.dll```

如果编译时没有找到 libusb.h，请检查```LIBUSB_INCLUDEDIR```的设置是否正确，如果编译动态库时出现链接错误，请检查```LIBUSB_LIBRARY``` 

**注意:**如果修改了LIBUSB_USE_STATIC变量，请删除cmake缓存重新构建，否则可能产生错误。

## 四、使用

### 1. TCP Raw Socket

```cpp
OpenVisa::Object visa;
visa.connect(OpenVisa::Address<OpenVisa::AddressType::RawSocket>("192.168.0.111", 9999));
// or visa.connect("TCPIP::192.168.0.111::9999::SOCKET");
visa.send("*IDN?");
std::cout << visa.readAll();
```

### 2. 串口

```cpp
OpenVisa::Object visa;
auto& attr = visa.attribute();
attr.setBaudRate(115200);
attr.setDataBits(OpenVisa::DataBits::Data8);
attr.setFlowControl(OpenVisa::FlowControl::None);
attr.setParity(OpenVisa::Parity::None);
attr.setStopBits(OpenVisa::StopBits::One)
visa.connect(OpenVisa::Address<OpenVisa::AddressType::SerialPort>("COM3"));
// or visa.connect("ASRL3::INSTR");
visa.send("*IDN?");
std::cout << visa.readAll();
```

### 3. USB

```cpp
OpenVisa::Object visa;
visa.connect(OpenVisa::Address<OpenVisa::AddressType::USB>(0x0a69, 0x0651, "0000000000001"));
// or visa.connect("USB::0x0A69::0x0651::0000000000001::INSTR");
visa.send("*IDN?");
std::cout << visa.readAll();
```

> 注意：如果安装过NI-VISA驱动库，我们无法访问NI-VISA的驱动，可以尝试安装这个驱动：https://pan.baidu.com/s/1onVU5vPZAHbREZ_zEVLA0A，提取码9dmy，或者使用Zadig将驱动程序替换为WinUSB。

### 4. VXI-11

```cpp
OpenVisa::Object visa;
visa.connect(OpenVisa::Address<OpenVisa::AddressType::VXI11>("192.168.0.111", "inst0"));
// or visa.connect("TCPIP::192.168.0.111::INSTR");
visa.send("*IDN?");
std::cout << visa.readAll();
```

### 5. HiSLIP

```cpp
OpenVisa::Object visa;
visa.connect(OpenVisa::Address<OpenVisa::AddressType::HiSLIP>("192.168.0.111", "hislip0"));
// or visa.connect("TCPIP::192.168.0.111::hislip0");
visa.send("*IDN?");
std::cout << visa.readAll();
```

### 6. 设备枚举
```cpp
// 枚举所有串口设备
auto ports = OpenVisa::Object::listSerialPorts();
for(const auto& port : ports)
{
    std::cout << port << std::endl;
}
// 枚举所有USB设备
auto usbs = OpenVisa::Object::listUSB();
for(const auto& usb : usbs)
{
    std::cout << OpenVisa::Object::toVisaAddressString(usb) << std::endl;
}
```

### 7. 终结符（TermChars）

* 与C语言Visa不同，C语言Visa仅支持单个字符的终结符，OpenVisa可以支持多个字符的终结符；

* 目前OpenVisa没有将发送和接收的终结符分开，他们都由Object::Attribute::terminalChars决定；

* 默认情况下OpenVisa的终结符为'\n'，如果Object::send和Object::query方法发送的数据末尾不含终结符，OpenVisa将会为发送的数据自动添加终结符，该行为由Object::Attribute::autoAppendTerminalChars属性决定；
* 默认情况下OpenVisa的Object::query和Object::readAll方法接收数据时，将会等待直到收到终结符为止，否则将超时，该行为由Object::Attribute::terminalCharsEnable属性决定。

#### 7.1 如果需要与设备进行二进制通信时，可以关闭终结符，例如：

```cpp
OpenVisa::Object visa;
auto& attr = visa.attribute();
attr.setAutoAppendTerminalChars(false);
attr.setTerminalCharsEnable(false);
```

#### 7.2 如果设备的发送和接收的终结符不同，那么请将终结符属性设为接收时的终结符，并关闭发送时自动附加终结符属性，例如：

```cpp
// 发送终结符为\n,接收终结符为#
OpenVisa::Object visa;
//... connect
auto& attr = visa.attribute();
attr.setAutoAppendTerminalChars(false);
attr.setTerminalChars("#");
//...
auto idn = visa.query("*IDN?\n");
```

## 五、参考

* USBTMC: [USBTMC_1_00.pdf, USBTMC_usb488_subclass_1_00.pdf](https://www.usb.org/document-library/test-measurement-class-specification)

* VXI-11: [vxi-11.pdf](http://www.vxibus.org/specifications.html)

* * ONC-RPC: [RFC1057](https://www.rfc-editor.org/rfc/rfc1057.html)

* HiSLIP: [IVI-6.1_HiSLIP-2.0-2020-04-23.pdf](https://www.ivifoundation.org/specifications/default.aspx)

* IEEE488.2: [ANSI/IEEE Std 488.2-1987](https://ieeexplore.ieee.org/document/213762/)
