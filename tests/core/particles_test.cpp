// Частицы фона меню (core/particles.h): скорость по времени (AUD-20), линии
// гаснут плавно (AUD-13).

#include "core/particles.h"

#include <gtest/gtest.h>

#include <cmath>
#include <optional>

#include "core/screen.h"
#include "scripted_random.h"

namespace aim::core {
namespace {

using testing::ScriptedRandom;

TEST(Particles, SpeedIsPerSecondNotPerFrame) {
    ScriptedRandom rng;
    // Первая частица: середина окна, вправо (угол 0).
    rng.units({0.5, 0.5, 0.0});
    Particles a;
    a.init(rng);
    ScriptedRandom rng2;
    rng2.units({0.5, 0.5, 0.0});
    Particles b;
    b.init(rng2);
    // Секунда: 60 кадров по 1/60 с или 240 кадров по 1/240 с — один путь.
    for (int i = 0; i < 60; ++i)
        a.update(1'000'000'000 / 60);
    for (int i = 0; i < 240; ++i)
        b.update(1'000'000'000 / 240);
    // Доли наносекунды кадра отброшены целым делением — отсюда допуск.
    EXPECT_NEAR(a.items()[0].x, b.items()[0].x, 1e-3);
    EXPECT_NEAR(a.items()[0].x - kWindowWidth / 2.0, kParticleSpeed, 1e-3);
}

TEST(Particles, BounceOffEdges) {
    ScriptedRandom rng;
    rng.units({0.999, 0.5, 0.0});  // у правого края, вправо
    Particles p;
    p.init(rng);
    p.update(ms(100));
    EXPECT_LE(p.items()[0].x, kWindowWidth);
    EXPECT_LT(p.items()[0].dx, 0) << "отражение от правого края";
}

TEST(Particles, LinkAlphaFadesWithoutJump) {
    // У образца прозрачность линии к курсору за 100 px оборачивалась и
    // становилась яркой (AUD-13): здесь — гаснет до нуля к пределу.
    int previous = 255;
    for (double d = 0; d < kCursorLink; d += 1) {
        const std::optional<int> a = link_alpha(d, kCursorLink);
        ASSERT_TRUE(a.has_value());
        EXPECT_LE(*a, previous) << d;
        EXPECT_GE(*a, 0);
        previous = *a;
    }
    EXPECT_EQ(link_alpha(0, kParticleLink), 120);
    EXPECT_EQ(link_alpha(kCursorLink, kCursorLink), std::nullopt);
    EXPECT_EQ(link_alpha(150, kParticleLink), std::nullopt);
}

}  // namespace
}  // namespace aim::core
