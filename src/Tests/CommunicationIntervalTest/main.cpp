#include <OpenVisa/Attribute.h>
#include <OpenVisa/Object.h>
#include <gtest/gtest.h>

#include <ranges>

using namespace std::chrono_literals;
TEST(CommunicationIntervalTest, bool)
{
    OpenVisa::Object obj;
    obj.connect(OpenVisa::Address<OpenVisa::AddressType::RawSocket>("127.0.0.1", 5025));
    for (auto before = std::chrono::system_clock::now().time_since_epoch(); auto i : std::views::iota(0, 100))
    {
        obj.send("0\n");
        auto now = std::chrono::system_clock::now().time_since_epoch();
        EXPECT_TRUE(std::chrono::duration_cast<std::chrono::milliseconds>(now - before) < 50ms);
        before = now;
    }
    obj.attribute().setCommunicationInterval(100ms);
    for (auto before = std::chrono::system_clock::now().time_since_epoch(); auto i : std::views::iota(0, 100))
    {
        obj.send("100\n");
        auto now = std::chrono::system_clock::now().time_since_epoch();
        auto d   = std::chrono::duration_cast<std::chrono::milliseconds>(now - before);
        if (i != 0)
        {
            EXPECT_TRUE(d >= 100ms);
        }
        before = now;
    }
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}