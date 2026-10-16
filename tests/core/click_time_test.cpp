// Сопоставление нажатий событий SDL с сырыми нажатиями (D-24).

#include "core/click_time.h"

#include <gtest/gtest.h>

namespace aim::core {
namespace {

constexpr Nanoseconds ms(Nanoseconds value) {
    return value * 1'000'000;
}

TEST(ClickTimer, WithoutRawPressTimeIsFromEvent) {
    ClickTimer timer;
    const ClickTime click = timer.resolve(MouseButton::Left, ms(1000));
    EXPECT_EQ(click.source, ClickTimeSource::Event);
    EXPECT_EQ(click.time, ms(1000));
}

TEST(ClickTimer, RawPressInWindowGivesRawTime) {
    ClickTimer timer;
    timer.add({ms(1000) + 200'000, MouseButton::Left});
    const ClickTime click = timer.resolve(MouseButton::Left, ms(1000));
    EXPECT_EQ(click.source, ClickTimeSource::RawInput);
    EXPECT_EQ(click.time, ms(1000) + 200'000);
    EXPECT_EQ(timer.pending(), 0U);
}

// На Windows метка SDL — из msg.time и раньше настоящего нажатия до шага тиков.
TEST(ClickTimer, EventStampEarlierByTickStillMatches) {
    ClickTimer timer;
    timer.add({ms(1016), MouseButton::Right});
    const ClickTime click = timer.resolve(MouseButton::Right, ms(1000));
    EXPECT_EQ(click.source, ClickTimeSource::RawInput);
    EXPECT_EQ(click.time, ms(1016));
}

TEST(ClickTimer, WindowBoundsAreInclusive) {
    ClickTimer timer;
    timer.add({ms(1000) - ClickTimer::kRawBeforeEvent, MouseButton::Left});
    EXPECT_EQ(timer.resolve(MouseButton::Left, ms(1000)).source, ClickTimeSource::RawInput);
    timer.add({ms(2000) + ClickTimer::kRawAfterEvent, MouseButton::Left});
    EXPECT_EQ(timer.resolve(MouseButton::Left, ms(2000)).source, ClickTimeSource::RawInput);
}

TEST(ClickTimer, RawPressBeforeWindowIsNotMatchedAndDropped) {
    ClickTimer timer;
    timer.add({ms(1000) - ClickTimer::kRawBeforeEvent - 1, MouseButton::Left});
    const ClickTime click = timer.resolve(MouseButton::Left, ms(1000));
    EXPECT_EQ(click.source, ClickTimeSource::Event);
    EXPECT_EQ(timer.pending(), 0U);
}

TEST(ClickTimer, RawPressAfterWindowWaitsForLaterEvent) {
    ClickTimer timer;
    timer.add({ms(1000) + ClickTimer::kRawAfterEvent + 1, MouseButton::Left});
    EXPECT_EQ(timer.resolve(MouseButton::Left, ms(1000)).source, ClickTimeSource::Event);
    EXPECT_EQ(timer.pending(), 1U);
    EXPECT_EQ(timer.resolve(MouseButton::Left, ms(1030)).source, ClickTimeSource::RawInput);
}

TEST(ClickTimer, RawPressIsUsedOnce) {
    ClickTimer timer;
    timer.add({ms(1000), MouseButton::Left});
    EXPECT_EQ(timer.resolve(MouseButton::Left, ms(1000)).source, ClickTimeSource::RawInput);
    EXPECT_EQ(timer.resolve(MouseButton::Left, ms(1001)).source, ClickTimeSource::Event);
}

TEST(ClickTimer, OtherButtonIsNotMatchedAndKept) {
    ClickTimer timer;
    timer.add({ms(1000), MouseButton::Right});
    EXPECT_EQ(timer.resolve(MouseButton::Left, ms(1000)).source, ClickTimeSource::Event);
    EXPECT_EQ(timer.pending(), 1U);
    EXPECT_EQ(timer.resolve(MouseButton::Right, ms(1000)).time, ms(1000));
}

// Двойной щелчок: два нажатия одной кнопки за 90 мс — каждое к своему событию.
TEST(ClickTimer, DoubleClickPairsInOrder) {
    ClickTimer timer;
    timer.add({ms(1000), MouseButton::Left});
    timer.add({ms(1090), MouseButton::Left});
    EXPECT_EQ(timer.resolve(MouseButton::Left, ms(994)).time, ms(1000));
    EXPECT_EQ(timer.resolve(MouseButton::Left, ms(1078)).time, ms(1090));
    EXPECT_EQ(timer.pending(), 0U);
}

// Нажатие мимо окна (заголовок, другое окно) события SDL не даёт и не должно
// достаться следующему клику по окну.
TEST(ClickTimer, PressWithoutEventIsDroppedByNextClick) {
    ClickTimer timer;
    timer.add({ms(500), MouseButton::Left});
    timer.add({ms(905), MouseButton::Left});
    const ClickTime click = timer.resolve(MouseButton::Left, ms(900));
    EXPECT_EQ(click.source, ClickTimeSource::RawInput);
    EXPECT_EQ(click.time, ms(905));
    EXPECT_EQ(timer.pending(), 0U);
}

// Сырое нажатие опоздало к своему событию: оно не должно достаться
// следующему клику той же кнопки.
TEST(ClickTimer, LateRawPressDoesNotLeakIntoNextClick) {
    ClickTimer timer;
    EXPECT_EQ(timer.resolve(MouseButton::Left, ms(1000)).source, ClickTimeSource::Event);
    timer.add({ms(1001), MouseButton::Left});
    timer.add({ms(1300), MouseButton::Left});
    EXPECT_EQ(timer.resolve(MouseButton::Left, ms(1295)).time, ms(1300));
}

TEST(ClickTimer, EventNearClockStartDoesNotUnderflow) {
    ClickTimer timer;
    timer.add({0, MouseButton::Middle});
    const ClickTime click = timer.resolve(MouseButton::Middle, ms(5));
    EXPECT_EQ(click.source, ClickTimeSource::RawInput);
    EXPECT_EQ(click.time, 0U);
}

TEST(ClickTimer, ForgetOldDropsPressesOlderThanMaxAge) {
    ClickTimer timer;
    timer.add({ms(100), MouseButton::Left});
    timer.add({ms(900), MouseButton::Right});
    timer.forget_old(ms(100) + ClickTimer::kMaxAge + 1);
    EXPECT_EQ(timer.pending(), 1U);
    EXPECT_EQ(timer.resolve(MouseButton::Right, ms(900)).source, ClickTimeSource::RawInput);
}

TEST(ClickTimer, ForgetOldNearClockStartKeepsEverything) {
    ClickTimer timer;
    timer.add({ms(1), MouseButton::Left});
    timer.forget_old(ms(10));
    EXPECT_EQ(timer.pending(), 1U);
}

TEST(ClickTimer, CapacityKeepsNewestPresses) {
    ClickTimer timer;
    const auto extra = static_cast<Nanoseconds>(ClickTimer::kCapacity) + 6;
    for (Nanoseconds i = 0; i < extra; ++i)
        timer.add({ms(1000 + i * 100), MouseButton::X1});
    EXPECT_EQ(timer.pending(), ClickTimer::kCapacity);
    // Самое старое забыто, самое новое — на месте.
    EXPECT_EQ(timer.resolve(MouseButton::X1, ms(1000)).source, ClickTimeSource::Event);
    EXPECT_EQ(timer.resolve(MouseButton::X1, ms(1000 + (extra - 1) * 100)).source,
              ClickTimeSource::RawInput);
}

TEST(ClickTimer, OutOfOrderPressStillMatchesEarliestInWindow) {
    ClickTimer timer;
    timer.add({ms(1020), MouseButton::Left});
    timer.add({ms(1002), MouseButton::Left});
    EXPECT_EQ(timer.resolve(MouseButton::Left, ms(1000)).time, ms(1002));
}

}  // namespace
}  // namespace aim::core
