// Свой поток сырого ввода мыши (D-24). Нажатий тест не делает: сырой ввод
// приходит, только пока программа впереди, а у теста окна нет.

#include "platform/raw_mouse.h"

#include <SDL3/SDL.h>
#include <gtest/gtest.h>

#include <vector>

namespace aim::platform {
namespace {

class RawMouseTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { ASSERT_TRUE(SDL_Init(0)) << SDL_GetError(); }
    static void TearDownTestSuite() { SDL_Quit(); }
};

#ifdef _WIN32

TEST_F(RawMouseTest, StartsAndRegistersMouse) {
    RawMouse mouse;
    EXPECT_TRUE(mouse.active()) << mouse.problem();
    EXPECT_TRUE(mouse.problem().empty());
    std::vector<core::RawPress> presses;
    mouse.drain(presses);  // без нажатий — пусто, но и не падает
}

TEST_F(RawMouseTest, OnlyOneReceiverPerProcess) {
    const RawMouse first;
    const RawMouse second;
    EXPECT_TRUE(first.active()) << first.problem();
    EXPECT_FALSE(second.active());
    EXPECT_FALSE(second.problem().empty());
}

TEST_F(RawMouseTest, ReceiverIsFreeAgainAfterStop) {
    {
        const RawMouse first;
        ASSERT_TRUE(first.active()) << first.problem();
    }
    const RawMouse second;
    EXPECT_TRUE(second.active()) << second.problem();
}

#else

TEST_F(RawMouseTest, NotAvailableOutsideWindows) {
    RawMouse mouse;
    EXPECT_FALSE(mouse.active());
    EXPECT_FALSE(mouse.problem().empty());
    std::vector<core::RawPress> presses;
    mouse.drain(presses);
    EXPECT_TRUE(presses.empty());
}

#endif

}  // namespace
}  // namespace aim::platform
