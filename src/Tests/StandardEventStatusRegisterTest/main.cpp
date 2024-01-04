#include <OpenVisa/CommonCommand.h>
#include <OpenVisa/Object.h>
#include <gtest/gtest.h>

TEST(StandardEventStatusRegisterTest, bool)
{
    OpenVisa::Object object;
    object.connect(OpenVisa::Address<OpenVisa::AddressType::VXI11>("192.168.1.110"));
    object.send("*IDN");
    auto status = object.commonCommand().esr();
    EXPECT_TRUE(status.commandError);
    static_cast<void>(object.query("*IDN?"));
    status = object.commonCommand().esr();
    EXPECT_FALSE(status.commandError);
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}