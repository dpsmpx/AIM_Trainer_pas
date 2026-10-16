// Случайные числа и округление — как у PascalABC.NET (docs/ARCHITECTURE.md, §11).

#include "core/random.h"

#include <gtest/gtest.h>

#include <set>

#include "core/rounding.h"

namespace aim::core {
namespace {

TEST(RoundHalfEven, HalvesGoToEven) {
    EXPECT_EQ(round_half_even(0.5), 0);
    EXPECT_EQ(round_half_even(1.5), 2);
    EXPECT_EQ(round_half_even(2.5), 2);
    EXPECT_EQ(round_half_even(-0.5), 0);
    EXPECT_EQ(round_half_even(-1.5), -2);
}

TEST(RoundHalfEven, OthersGoToNearest) {
    EXPECT_EQ(round_half_even(2.49), 2);
    EXPECT_EQ(round_half_even(2.51), 3);
    EXPECT_EQ(round_half_even(-2.51), -3);
    // Очки за попадание у образца: Round(13 / SizeFactor · mult).
    EXPECT_EQ(round_half_even(13 / 0.65), 20);
    EXPECT_EQ(round_half_even(13 / 1.35), 10);
}

TEST(EngineRandom, BetweenIsInclusive) {
    EngineRandom rng(1);
    std::set<int> seen;
    for (int i = 0; i < 1000; ++i) {
        const int value = rng.between(3, 5);
        ASSERT_GE(value, 3);
        ASSERT_LE(value, 5);
        seen.insert(value);
    }
    EXPECT_EQ(seen, (std::set<int>{3, 4, 5}));
}

TEST(EngineRandom, BetweenSwapsReversedBounds) {
    EngineRandom rng(2);
    for (int i = 0; i < 100; ++i) {
        const int value = rng.between(9, 7);
        ASSERT_GE(value, 7);
        ASSERT_LE(value, 9);
    }
    EXPECT_EQ(rng.between(4, 4), 4);
}

TEST(EngineRandom, UnitIsInHalfOpenRange) {
    EngineRandom rng(3);
    for (int i = 0; i < 1000; ++i) {
        const double value = rng.unit();
        ASSERT_GE(value, 0.0);
        ASSERT_LT(value, 1.0);
    }
}

TEST(EngineRandom, SameSeedSameSequence) {
    EngineRandom a(42);
    EngineRandom b(42);
    for (int i = 0; i < 10; ++i)
        EXPECT_EQ(a.between(0, 1'000'000), b.between(0, 1'000'000));
}

}  // namespace
}  // namespace aim::core
