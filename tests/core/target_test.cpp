// Режимы (D-21) и цель — перенос TCircle (docs/ARCHITECTURE.md, §5, §6).

#include "core/target.h"

#include <gtest/gtest.h>

#include <cmath>

#include "core/modes.h"
#include "core/rules.h"
#include "core/screen.h"
#include "scripted_random.h"

namespace aim::core {
namespace {

using testing::ScriptedRandom;

constexpr Nanoseconds kT0 = ms(10'000);

// unit() = 0,5 → масштаб 1,0: пик радиуса равен «Размеру круга».
Target classic_at(int x, int y, ScriptedRandom& rng, bool allow_bomb = false) {
    rng.units({0.5}).ints({x, y});
    return Target::spawn(mode_rules(GameMode::Classic, 1000), 40, {0, 0}, allow_bomb, kT0, rng);
}

TEST(ModeRules, LifetimeFollowsSettingByD21) {
    for (const int setting : {500, 1000, 2000}) {
        const auto s = static_cast<Nanoseconds>(setting);
        EXPECT_EQ(mode_rules(GameMode::Classic, setting).lifetime, ms(s));
        EXPECT_EQ(mode_rules(GameMode::Dual, setting).lifetime, ms(s));
        EXPECT_EQ(mode_rules(GameMode::Flick, setting).lifetime, ms(s) * 7 / 10);
        EXPECT_EQ(mode_rules(GameMode::Tracking, setting).lifetime, ms(s) * 5 / 2);
    }
    EXPECT_EQ(mode_rules(GameMode::Flick, 1000).lifetime, ms(700));
    EXPECT_EQ(mode_rules(GameMode::Tracking, 1000).lifetime, ms(2500));
}

TEST(ModeRules, FlagsPerMode) {
    const ModeRules classic = mode_rules(GameMode::Classic, 1000);
    EXPECT_TRUE(classic.grows);
    EXPECT_TRUE(classic.bombs);
    EXPECT_FALSE(classic.two_buttons);
    const ModeRules dual = mode_rules(GameMode::Dual, 1000);
    EXPECT_TRUE(dual.two_buttons);
    EXPECT_EQ(dual.top_reserve, kDualTopReserve);
    const ModeRules tracking = mode_rules(GameMode::Tracking, 1000);
    EXPECT_FALSE(tracking.grows);
    EXPECT_FALSE(tracking.bombs);
    EXPECT_TRUE(tracking.moves);
    EXPECT_TRUE(tracking.hover);
    const ModeRules flick = mode_rules(GameMode::Flick, 1000);
    EXPECT_FALSE(flick.grows);  // D-21: сразу полный размер
    EXPECT_TRUE(flick.bombs);
    EXPECT_FALSE(flick.two_buttons);
}

TEST(ModeRules, FileNamesAndIndexes) {
    EXPECT_EQ(mode_file_name(GameMode::Classic), "Classic");
    EXPECT_EQ(mode_file_name(GameMode::Dual), "Dual");
    EXPECT_EQ(mode_file_name(GameMode::Tracking), "Tracking");
    EXPECT_EQ(mode_file_name(GameMode::Flick), "Flick");
    EXPECT_EQ(mode_index(GameMode::Tracking), 2);
}

TEST(TargetSpawn, SizeFactorAndPeakRadius) {
    ScriptedRandom rng;
    rng.units({0.0});
    const Target small =
        Target::spawn(mode_rules(GameMode::Classic, 1000), 40, {}, false, kT0, rng);
    EXPECT_DOUBLE_EQ(small.size_factor(), kMinSizeFactor);
    EXPECT_EQ(small.peak_radius(), 26);  // 40 · 0,65
    rng.units({0.75});
    const Target big = Target::spawn(mode_rules(GameMode::Classic, 1000), 20, {}, false, kT0, rng);
    EXPECT_DOUBLE_EQ(big.size_factor(), 1.175);
    EXPECT_EQ(big.peak_radius(), 24);  // 23,5 → к чётному
}

TEST(TargetSpawn, PlacementRangesAndReserves) {
    ScriptedRandom rng;
    // Нижние границы: радиус от края, в Dual — ещё 50 px сверху под подсказку.
    rng.units({0.5}).ints({40, 40 + kDualTopReserve, 0});
    const Target dual =
        Target::spawn(mode_rules(GameMode::Dual, 1000), 40, {700, 500}, false, kT0, rng);
    EXPECT_DOUBLE_EQ(dual.center().x, 40);
    EXPECT_DOUBLE_EQ(dual.center().y, 40 + kDualTopReserve);
    // Верхние границы: радиус от края и HUD снизу.
    rng.units({0.5}).ints({kWindowWidth - 40, kWindowHeight - 40 - kHudReserve});
    const Target classic =
        Target::spawn(mode_rules(GameMode::Classic, 1000), 40, {0, 0}, false, kT0, rng);
    EXPECT_DOUBLE_EQ(classic.center().x, kWindowWidth - 40);
    EXPECT_DOUBLE_EQ(classic.center().y, kWindowHeight - 40 - kHudReserve);
}

TEST(TargetSpawn, KeepsDistanceFromPreviousTarget) {
    ScriptedRandom rng;
    // Первые две попытки ближе двух радиусов (80 px) к прежней цели.
    rng.units({0.5}).ints({400, 300, 450, 330, 600, 300});
    const Target t =
        Target::spawn(mode_rules(GameMode::Classic, 1000), 40, {400, 300}, false, kT0, rng);
    EXPECT_DOUBLE_EQ(t.center().x, 600);
    EXPECT_DOUBLE_EQ(t.center().y, 300);
}

TEST(TargetSpawn, GivesUpAfterAttemptLimit) {
    ScriptedRandom rng;
    rng.units({0.5});  // ints кончились — всегда нижняя граница, рядом с прежней
    const Target t =
        Target::spawn(mode_rules(GameMode::Classic, 1000), 40, {40, 40}, false, kT0, rng);
    EXPECT_DOUBLE_EQ(t.center().x, 40);
    EXPECT_DOUBLE_EQ(t.center().y, 40);
}

TEST(TargetSpawn, BombOnlyWhenAllowedAndModeHasBombs) {
    ScriptedRandom rng;
    EXPECT_FALSE(classic_at(400, 300, rng.units({}), false).is_bomb());
    rng.units({0.5}).ints({400, 300}).units({0.1});
    EXPECT_TRUE(
        Target::spawn(mode_rules(GameMode::Classic, 1000), 40, {}, true, kT0, rng).is_bomb());
    rng.units({0.5}).ints({400, 300}).units({0.2});
    EXPECT_FALSE(
        Target::spawn(mode_rules(GameMode::Classic, 1000), 40, {}, true, kT0, rng).is_bomb());
    // В Tracking бомб нет: unit() на бомбу не берётся, следующий — направление.
    rng.units({0.5, 0.0}).ints({400, 300, 90});
    const Target tracking =
        Target::spawn(mode_rules(GameMode::Tracking, 1000), 40, {}, true, kT0, rng);
    EXPECT_FALSE(tracking.is_bomb());
    EXPECT_DOUBLE_EQ(tracking.velocity().x, 90);
    EXPECT_NEAR(tracking.velocity().y, 0, 1e-9);
}

TEST(TargetSpawn, DualColorAndLifetime) {
    ScriptedRandom rng;
    rng.units({0.5}).ints({400, 300, 1});
    const Target t = Target::spawn(mode_rules(GameMode::Dual, 1200), 40, {}, false, kT0, rng);
    EXPECT_EQ(t.color(), 1);
    EXPECT_EQ(t.spawned_at(), kT0);
    EXPECT_EQ(t.expires_at(), kT0 + ms(1200));
}

TEST(TargetRadius, GrowingIsTriangle) {
    ScriptedRandom rng;
    const Target t = classic_at(400, 300, rng);
    EXPECT_DOUBLE_EQ(t.radius_at(kT0), 0);
    EXPECT_DOUBLE_EQ(t.radius_at(kT0 + ms(250)), 20);
    EXPECT_DOUBLE_EQ(t.radius_at(kT0 + ms(500)), 40);
    EXPECT_DOUBLE_EQ(t.radius_at(kT0 + ms(750)), 20);
    EXPECT_DOUBLE_EQ(t.radius_at(kT0 + ms(1000)), 0);
    EXPECT_FALSE(t.alive_at(kT0 + ms(1000)));
    EXPECT_DOUBLE_EQ(t.radius_at(kT0 - 1), 0);
}

TEST(TargetRadius, FlickAndTrackingAreFullSizeAtOnce) {
    ScriptedRandom rng;
    rng.units({0.5}).ints({400, 300});
    const Target flick = Target::spawn(mode_rules(GameMode::Flick, 1000), 40, {}, false, kT0, rng);
    EXPECT_DOUBLE_EQ(flick.radius_at(kT0), 40);
    EXPECT_DOUBLE_EQ(flick.radius_at(kT0 + ms(699)), 40);
    EXPECT_DOUBLE_EQ(flick.radius_at(kT0 + ms(700)), 0);
    rng.units({0.5, 0.25}).ints({400, 300, 60});
    const Target tracking =
        Target::spawn(mode_rules(GameMode::Tracking, 1000), 40, {}, false, kT0, rng);
    EXPECT_DOUBLE_EQ(tracking.radius_at(kT0 + ms(2499)), 40);
}

TEST(TargetHit, OnlyAfterShownAndWhileAlive) {
    ScriptedRandom rng;
    Target t = classic_at(400, 300, rng);
    EXPECT_FALSE(t.hit({400, 300}, MouseButton::Left, kT0 + ms(500)).hit) << "ещё не показана";
    t.mark_shown(kT0, kT0 + ms(5));  // в кадре на момент появления радиус 0
    EXPECT_FALSE(t.shown_at());
    t.mark_shown(kT0 + ms(16), kT0 + ms(20));
    ASSERT_TRUE(t.shown_at());
    EXPECT_EQ(t.shown_at().value_or(0), kT0 + ms(20));
    EXPECT_FALSE(t.hit({400, 300}, MouseButton::Left, kT0 + ms(19)).hit) << "щелчок до показа";
    EXPECT_TRUE(t.hit({400, 300}, MouseButton::Left, kT0 + ms(500)).hit);
    EXPECT_FALSE(t.hit({400, 300}, MouseButton::Left, kT0 + ms(1000)).hit) << "жизнь кончилась";
    t.mark_shown(kT0 + ms(600), kT0 + ms(610));
    EXPECT_EQ(t.shown_at().value_or(0), kT0 + ms(20)) << "первый показ не переписывается";
}

TEST(TargetHit, RadiusAtClickTimeAndAccuracy) {
    ScriptedRandom rng;
    Target t = classic_at(400, 300, rng);
    t.mark_shown(kT0 + ms(16), kT0 + ms(16));
    const HitInfo center = t.hit({400, 300}, MouseButton::Left, kT0 + ms(500));
    EXPECT_TRUE(center.hit);
    EXPECT_DOUBLE_EQ(center.accuracy, 1.0);
    const HitInfo half = t.hit({420, 300}, MouseButton::Left, kT0 + ms(500));
    EXPECT_DOUBLE_EQ(half.accuracy, 0.5);
    EXPECT_TRUE(t.hit({440, 300}, MouseButton::Left, kT0 + ms(500)).hit) << "край — попадание";
    EXPECT_FALSE(t.hit({441, 300}, MouseButton::Left, kT0 + ms(500)).hit);
    // В момент 250 мс радиус 20: та же точка 30 px — мимо.
    EXPECT_FALSE(t.hit({430, 300}, MouseButton::Left, kT0 + ms(250)).hit);
}

TEST(TargetHit, ButtonsByMode) {
    ScriptedRandom rng;
    Target classic = classic_at(400, 300, rng);
    classic.mark_shown(kT0 + ms(16), kT0 + ms(16));
    EXPECT_FALSE(classic.hit({400, 300}, MouseButton::Right, kT0 + ms(500)).hit);

    rng.units({0.5}).ints({400, 300, 1});
    Target red = Target::spawn(mode_rules(GameMode::Dual, 1000), 40, {}, false, kT0, rng);
    red.mark_shown(kT0 + ms(16), kT0 + ms(16));
    EXPECT_FALSE(red.hit({400, 300}, MouseButton::Left, kT0 + ms(500)).hit);
    EXPECT_TRUE(red.hit({400, 300}, MouseButton::Right, kT0 + ms(500)).hit);

    rng.units({0.5}).ints({400, 300, 0});
    Target blue = Target::spawn(mode_rules(GameMode::Dual, 1000), 40, {}, false, kT0, rng);
    blue.mark_shown(kT0 + ms(16), kT0 + ms(16));
    EXPECT_TRUE(blue.hit({400, 300}, MouseButton::Left, kT0 + ms(500)).hit);
    EXPECT_FALSE(blue.hit({400, 300}, MouseButton::Right, kT0 + ms(500)).hit);
}

TEST(TargetHit, BombAnyButton) {
    ScriptedRandom rng;
    rng.units({0.5}).ints({400, 300}).units({0.0});
    Target bomb = Target::spawn(mode_rules(GameMode::Classic, 1000), 40, {}, true, kT0, rng);
    ASSERT_TRUE(bomb.is_bomb());
    bomb.mark_shown(kT0 + ms(16), kT0 + ms(16));
    for (const MouseButton b : {MouseButton::Left, MouseButton::Right}) {
        const HitInfo info = bomb.hit({405, 300}, b, kT0 + ms(500));
        EXPECT_TRUE(info.hit);
        EXPECT_TRUE(info.bomb);
    }
}

TEST(TargetPoints, ThirteenOverSizeTimesCombo) {
    ScriptedRandom rng;
    rng.units({0.0}).ints({400, 300});
    const Target smallest =
        Target::spawn(mode_rules(GameMode::Classic, 1000), 40, {}, false, kT0, rng);
    EXPECT_EQ(smallest.points(1.0), 20);  // 13 / 0,65
    EXPECT_EQ(smallest.points(1.5), 30);
    EXPECT_EQ(smallest.points(2.0), 40);
    const Target unit = classic_at(400, 300, rng);
    EXPECT_EQ(unit.points(1.0), 13);
    EXPECT_EQ(unit.points(1.5), 20);  // 19,5 → к чётному
}

Target tracking_at(int x, int y, double angle_unit, int speed, ScriptedRandom& rng) {
    rng.units({0.5, angle_unit}).ints({x, y, speed});
    return Target::spawn(mode_rules(GameMode::Tracking, 1000), 40, {0, 0}, false, kT0, rng);
}

TEST(TargetMove, MovesAtSpeedInAnyDirection) {
    ScriptedRandom rng;
    Target t = tracking_at(400, 300, 0.125, 100, rng);  // 45°
    t.move(ms(1000));
    EXPECT_NEAR(t.center().x, 400 + 100 / std::sqrt(2.0), 1e-9);
    EXPECT_NEAR(t.center().y, 300 + 100 / std::sqrt(2.0), 1e-9);
    // Малые шаги складываются: 250 шагов по 4 мс — то же, что 1 с (AUD-04).
    Target u = tracking_at(400, 300, 0.125, 100, rng);
    for (int i = 0; i < 250; ++i)
        u.move(ms(4));
    EXPECT_NEAR(u.center().x, t.center().x, 1e-6);
    EXPECT_NEAR(u.center().y, t.center().y, 1e-6);
}

TEST(TargetMove, BouncesByOwnRadiusAboveHud) {
    ScriptedRandom rng;
    Target down = tracking_at(400, 470, 0.25, 100, rng);  // вниз, к HUD
    down.move(ms(1000));
    const double bottom = kWindowHeight - 40 - kHudReserve;
    EXPECT_DOUBLE_EQ(down.center().y, bottom);
    EXPECT_LT(down.velocity().y, 0);
    Target left = tracking_at(60, 300, 0.5, 100, rng);  // влево, к краю
    left.move(ms(1000));
    EXPECT_DOUBLE_EQ(left.center().x, 40);
    EXPECT_GT(left.velocity().x, 0);
    // Долгое движение не выводит цель за край и под HUD.
    Target wander = tracking_at(400, 300, 0.1, 119, rng);
    for (int i = 0; i < 5000; ++i) {
        wander.move(ms(7));
        ASSERT_GE(wander.center().x, 40);
        ASSERT_LE(wander.center().x, kWindowWidth - 40);
        ASSERT_GE(wander.center().y, 40);
        ASSERT_LE(wander.center().y, bottom);
    }
}

TEST(TargetHover, CompletesAfterHoldAndDecaysTwiceAsFast) {
    ScriptedRandom rng;
    Target t = tracking_at(400, 300, 0.0, 60, rng);
    EXPECT_FALSE(t.hover({400, 300}, ms(500)));
    EXPECT_EQ(t.hover_progress(), ms(500));
    EXPECT_FALSE(t.hover({0, 0}, ms(100)));
    EXPECT_EQ(t.hover_progress(), ms(300));
    EXPECT_FALSE(t.hover({0, 0}, ms(1000)));
    EXPECT_EQ(t.hover_progress(), 0U);
    EXPECT_FALSE(t.hover({420, 300}, ms(899)));
    EXPECT_TRUE(t.hover({420, 300}, ms(1)));
}

TEST(TargetShift, PauseMovesAllMoments) {
    ScriptedRandom rng;
    Target t = classic_at(400, 300, rng);
    t.mark_shown(kT0 + ms(16), kT0 + ms(17));
    t.shift(ms(3000));
    EXPECT_EQ(t.spawned_at(), kT0 + ms(3000));
    EXPECT_EQ(t.expires_at(), kT0 + ms(4000));
    EXPECT_EQ(t.shown_at().value_or(0), kT0 + ms(3017));
    EXPECT_DOUBLE_EQ(t.radius_at(kT0 + ms(3500)), 40);
}

}  // namespace
}  // namespace aim::core
