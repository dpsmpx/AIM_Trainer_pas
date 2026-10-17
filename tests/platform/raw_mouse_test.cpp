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

TEST_F(RawMouseTest, WaitWithoutPressesTimesOut) {
    RawMouse mouse;
    ASSERT_TRUE(mouse.active()) << mouse.problem();
    const Uint64 start = SDL_GetTicksNS();
    EXPECT_FALSE(mouse.wait_for_press(2'000'000));
    EXPECT_GE(SDL_GetTicksNS() - start, 1'000'000U);  // ждало, а не вернулось сразу
}

TEST_F(RawMouseTest, OnlyOneReceiverPerProcess) {
    const RawMouse first;
    const RawMouse second;
    EXPECT_TRUE(first.active()) << first.problem();
    EXPECT_FALSE(second.active());
    EXPECT_FALSE(second.problem().empty());
}

TEST_F(RawMouseTest, StoppedThreadIsNotActive) {
    // Поток вышел — программа это видит и не ждёт сырых нажатий (AUD-51).
    RawMouse mouse;
    ASSERT_TRUE(mouse.active()) << mouse.problem();
    mouse.stop();
    EXPECT_FALSE(mouse.active());
    EXPECT_FALSE(mouse.problem().empty());
    const Uint64 start = SDL_GetTicksNS();
    EXPECT_FALSE(mouse.wait_for_press(50'000'000));
    EXPECT_LT(SDL_GetTicksNS() - start, 40'000'000U) << "без потока — сразу";
    mouse.stop();  // второй раз — ничего
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
    mouse.stop();  // потока нет — ничего
    EXPECT_FALSE(mouse.active());
    std::vector<core::RawPress> presses;
    mouse.drain(presses);
    EXPECT_TRUE(presses.empty());
    const Uint64 start = SDL_GetTicksNS();
    EXPECT_FALSE(mouse.wait_for_press(1'000'000'000));
    EXPECT_LT(SDL_GetTicksNS() - start, 100'000'000U);  // без потока — сразу, а не через секунду
}

#endif

}  // namespace
}  // namespace aim::platform
