#include <OpenVisa/Attribute.h>
#include <OpenVisa/Object.h>
#include <gtest/gtest.h>

TEST(CommandVerifyTest, bool)
{
    OpenVisa::Object object;
    object.connect(OpenVisa::Address<OpenVisa::AddressType::VXI11>("192.168.1.61"));
    // object.connect(OpenVisa::Address<OpenVisa::AddressType::RawSocket>("192.168.1.4", 3490));

    object.attribute().setCommandVerify(true);
    // for (int i = 0; i < 110; ++i)
    {
        EXPECT_THROW(object.send("*IDN"), std::exception);  // command error
        EXPECT_THROW(object.query("*IN?"), std::exception); // command error
        EXPECT_THROW(object.query("*IN"), std::exception);  // command error
        EXPECT_FALSE(object.query("*IDN?").empty());        // success
    }
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}