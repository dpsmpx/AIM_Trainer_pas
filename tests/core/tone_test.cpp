// Тоны образца (core/tone.h): частоты и длительности Console.Beep, плавные
// края.

#include "core/tone.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <vector>

namespace aim::core {
namespace {

TEST(Tone, LengthFollowsDuration) {
    EXPECT_EQ(tone(kHitBeep, 48'000, 0.25F).size(), 1'440U);  // 30 мс
    EXPECT_EQ(tone(kGameOverBeep, 48'000, 0.25F).size(), 10'560U);
    EXPECT_TRUE(tone({.frequency_hz = 440, .duration_ms = 0}, 48'000, 0.25F).empty());
}

TEST(Tone, FrequencyByZeroCrossings) {
    // 1000 Гц за 30 мс — 30 периодов, 60 смен знака (±1 на краях).
    const std::vector<float> s = tone(kHitBeep, 48'000, 0.25F);
    int crossings = 0;
    for (std::size_t i = 1; i < s.size(); ++i)
        if ((s[i - 1] < 0) != (s[i] < 0)) ++crossings;
    EXPECT_NEAR(crossings, 60, 2);
}

TEST(Tone, QuietEdgesAndVolume) {
    const std::vector<float> s = tone(kMissBeep, 48'000, 0.25F);
    EXPECT_EQ(s.front(), 0.0F);
    EXPECT_LT(std::abs(s.back()), 0.01F);
    float peak = 0;
    for (const float v : s)
        peak = std::max(peak, std::abs(v));
    EXPECT_NEAR(peak, 0.25F, 0.01F);
}

}  // namespace
}  // namespace aim::core
