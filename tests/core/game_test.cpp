// Забег — перенос TGameState, ProcessClick и UpdateGame (docs/ARCHITECTURE.md,
// §7) с отличиями D-12, D-14 и AUD-03.

#include "core/game.h"

#include <gtest/gtest.h>

#include <vector>

#include "core/rules.h"
#include "scripted_random.h"

namespace aim::core {
namespace {

using testing::ScriptedRandom;

constexpr Nanoseconds kT0 = ms(5'000);
constexpr GameSettings kDefaults{.circle_size = 40, .lifetime_ms = 1000, .lives = 15};

// Места целей по очереди: (200, 200) и (600, 400) — далеко друг от друга, так
// что каждая цель встаёт с первой попытки. unit() не задан — 0,5: масштаб 1
// (радиус 40), не бомба.
void alternate_spots(ScriptedRandom& rng, int count) {
    for (int i = 0; i < count; ++i)
        rng.ints(i % 2 == 0 ? std::initializer_list<int>{200, 200}
                            : std::initializer_list<int>{600, 400});
}

Vec2 center_of(const Game& g) {
    return g.target().center();
}

// Показать текущую цель: кадр на момент frame, показан через 1 мс.
void show(Game& g, Nanoseconds frame) {
    g.frame_presented(frame, frame + ms(1));
}

TEST(Game, HitScoresAndMeasuresReactionFromShownFrame) {
    ScriptedRandom rng;
    alternate_spots(rng, 4);
    Game g(GameMode::Classic, kDefaults, rng, kT0);
    show(g, kT0 + ms(15));  // показан в kT0 + 16 мс
    g.click(MouseButton::Left, center_of(g), kT0 + ms(316));
    EXPECT_EQ(g.score(), 13);
    EXPECT_EQ(g.hits(), 1);
    EXPECT_EQ(g.shots(), 1);
    EXPECT_EQ(g.combo(), 1);
    EXPECT_DOUBLE_EQ(g.accuracy(), 100);
    EXPECT_DOUBLE_EQ(g.precision(), 100);
    EXPECT_DOUBLE_EQ(g.avg_reaction_ms(), 300);
    const std::vector<GameEvent> events = g.take_events();
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0].kind, GameEventKind::Hit);
    EXPECT_EQ(events[0].points, 13);
    EXPECT_EQ(events[0].reaction, ms(300));
    // Новая цель — с момента попадания, в другом месте.
    EXPECT_EQ(g.target().spawned_at(), kT0 + ms(316));
    EXPECT_DOUBLE_EQ(center_of(g).x, 600);
    EXPECT_TRUE(g.take_events().empty());
}

TEST(Game, MissCostsLifeAndRecordsHeatmap) {
    ScriptedRandom rng;
    alternate_spots(rng, 2);
    Game g(GameMode::Classic, kDefaults, rng, kT0);
    show(g, kT0 + ms(15));
    g.click(MouseButton::Left, {790, 10}, kT0 + ms(300));
    EXPECT_EQ(g.misses(), 1);
    EXPECT_EQ(g.shots(), 1);
    EXPECT_EQ(g.lives_left(), 14);
    EXPECT_DOUBLE_EQ(g.accuracy(), 0);
    EXPECT_EQ(g.miss_heatmap()[kHeatmapCols - 1][0], 1);
    EXPECT_EQ(g.take_events().at(0).kind, GameEventKind::Miss);
    EXPECT_DOUBLE_EQ(center_of(g).x, 200) << "промах цель не меняет";
}

TEST(Game, ClickBeforeTargetShownIsMiss) {
    ScriptedRandom rng;
    alternate_spots(rng, 2);
    Game g(GameMode::Classic, kDefaults, rng, kT0);
    g.click(MouseButton::Left, center_of(g), kT0 + ms(300));
    EXPECT_EQ(g.misses(), 1);
    EXPECT_EQ(g.hits(), 0);
}

TEST(Game, RightButtonInClassicIsMiss) {
    ScriptedRandom rng;
    alternate_spots(rng, 2);
    Game g(GameMode::Classic, kDefaults, rng, kT0);
    show(g, kT0 + ms(15));
    g.click(MouseButton::Right, center_of(g), kT0 + ms(300));
    EXPECT_EQ(g.misses(), 1);
}

TEST(Game, OtherButtonsAreNotShots) {
    ScriptedRandom rng;
    alternate_spots(rng, 2);
    Game g(GameMode::Classic, kDefaults, rng, kT0);
    show(g, kT0 + ms(15));
    g.click(MouseButton::Middle, center_of(g), kT0 + ms(300));
    g.click(MouseButton::X1, center_of(g), kT0 + ms(310));
    EXPECT_EQ(g.shots(), 0);
    EXPECT_EQ(g.lives_left(), 15);
}

TEST(Game, ClickBeforeExpiryCountsEvenIfProcessedLate) {
    ScriptedRandom rng;
    alternate_spots(rng, 3);
    Game g(GameMode::Classic, kDefaults, rng, kT0);
    show(g, kT0 + ms(15));
    // Щелчок за 10 мс до конца жизни разобран в кадре уже после неё (AUD-03):
    // цель ещё не сменена — щелчки идут до update, по своему времени.
    g.click(MouseButton::Left, center_of(g), kT0 + ms(990));
    EXPECT_EQ(g.hits(), 1);
    g.update(kT0 + ms(1010), {});
    EXPECT_EQ(g.lives_left(), 15) << "сбитая цель не истекает";
    // Тот же щелчок по краю: к 990 мс радиус 0,8 px — мимо.
    ScriptedRandom rng2;
    alternate_spots(rng2, 3);
    Game h(GameMode::Classic, kDefaults, rng2, kT0);
    show(h, kT0 + ms(15));
    h.click(MouseButton::Left, {center_of(h).x + 5, center_of(h).y}, kT0 + ms(990));
    EXPECT_EQ(h.misses(), 1);
}

TEST(Game, ExpiredTargetCostsLifeAndBreaksCombo) {
    ScriptedRandom rng;
    alternate_spots(rng, 8);
    Game g(GameMode::Classic, kDefaults, rng, kT0);
    Nanoseconds t = kT0;
    for (int i = 0; i < 3; ++i) {
        show(g, t + ms(15));
        g.click(MouseButton::Left, center_of(g), t + ms(400));
        t += ms(400);
    }
    ASSERT_EQ(g.combo(), 3);
    g.take_events();
    g.update(t + ms(999), {});
    EXPECT_EQ(g.lives_left(), 15);
    g.update(t + ms(1000), {});
    EXPECT_EQ(g.lives_left(), 14);
    EXPECT_EQ(g.combo(), 0) << "D-14: пропущенная цель обрывает серию";
    EXPECT_EQ(g.max_combo(), 3);
    const std::vector<GameEvent> events = g.take_events();
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0].kind, GameEventKind::Expired);
    EXPECT_EQ(g.target().spawned_at(), t + ms(1000));
}

TEST(Game, FirstTargetIsNeverBombAndBombsExpireFreely) {
    ScriptedRandom rng;
    alternate_spots(rng, 3);
    rng.units({0.5});  // масштаб первой цели; бомбу для неё не спрашивают
    Game g(GameMode::Classic, kDefaults, rng, kT0);
    EXPECT_FALSE(g.target().is_bomb());
    show(g, kT0 + ms(15));
    rng.units({0.5, 0.0});  // следующая цель: масштаб 1, бомба
    g.click(MouseButton::Left, center_of(g), kT0 + ms(300));
    ASSERT_TRUE(g.target().is_bomb());
    g.take_events();
    g.update(kT0 + ms(1300), {});
    EXPECT_EQ(g.lives_left(), 15) << "бомба, которую не тронули, гаснет без штрафа";
    EXPECT_EQ(g.combo(), 1) << "и серию не обрывает (D-14)";
    EXPECT_TRUE(g.take_events().empty());
}

TEST(Game, BombClickEndsRun) {
    ScriptedRandom rng;
    // Dual: место и цвет; вторая цель — красная бомба.
    rng.ints({200, 200, 0, 600, 400, 1});
    rng.units({0.5, 0.5, 0.0});
    Game g(GameMode::Dual, kDefaults, rng, kT0);
    show(g, kT0 + ms(15));
    g.click(MouseButton::Left, center_of(g), kT0 + ms(300));
    ASSERT_TRUE(g.target().is_bomb());
    show(g, kT0 + ms(320));
    g.click(MouseButton::Right, center_of(g), kT0 + ms(600));
    EXPECT_TRUE(g.over());
    EXPECT_TRUE(g.died_to_bomb());
    EXPECT_EQ(g.shots(), 2);
    EXPECT_EQ(g.lives_left(), 15);
    const std::vector<GameEvent> events = g.take_events();
    EXPECT_EQ(events.back().kind, GameEventKind::Bomb);
    g.click(MouseButton::Left, center_of(g), kT0 + ms(700));
    EXPECT_EQ(g.shots(), 2) << "после конца щелчки не принимаются";
}

TEST(Game, ComboMultiplierFromFiveAndTen) {
    ScriptedRandom rng;
    alternate_spots(rng, 13);
    Game g(GameMode::Classic, kDefaults, rng, kT0);
    Nanoseconds t = kT0;
    std::vector<int> points;
    for (int i = 0; i < 11; ++i) {
        show(g, t + ms(15));
        g.click(MouseButton::Left, center_of(g), t + ms(300));
        points.push_back(g.take_events().at(0).points);
        t += ms(300);
    }
    // Масштаб 1: 13 очков, ×1,5 = 19,5 → 20 (к чётному), ×2 = 26.
    EXPECT_EQ(points[3], 13);
    EXPECT_EQ(points[4], 20);
    EXPECT_EQ(points[8], 20);
    EXPECT_EQ(points[9], 26);
    EXPECT_EQ(points[10], 26);
    EXPECT_EQ(g.max_combo(), 11);
}

TEST(Game, RunEndsWhenLivesAreGone) {
    ScriptedRandom rng;
    alternate_spots(rng, 2);
    Game g(GameMode::Classic, {.circle_size = 40, .lifetime_ms = 1000, .lives = 5}, rng, kT0);
    for (int i = 0; i < 4; ++i)
        g.click(MouseButton::Left, {5, 5}, kT0 + ms(100) + ms(static_cast<Nanoseconds>(i)));
    EXPECT_FALSE(g.over());
    g.click(MouseButton::Left, {5, 5}, kT0 + ms(200));
    EXPECT_TRUE(g.over());
    EXPECT_FALSE(g.died_to_bomb());
    EXPECT_EQ(g.lives_left(), 0);
}

TEST(Game, DualNeedsMatchingButton) {
    ScriptedRandom rng;
    rng.ints({200, 200, 1});  // красная
    Game g(GameMode::Dual, kDefaults, rng, kT0);
    show(g, kT0 + ms(15));
    g.click(MouseButton::Left, center_of(g), kT0 + ms(300));
    EXPECT_EQ(g.misses(), 1);
    g.click(MouseButton::Right, center_of(g), kT0 + ms(350));
    EXPECT_EQ(g.hits(), 1);
}

TEST(Game, PauseFreezesTimeAndIgnoresInput) {
    ScriptedRandom rng;
    alternate_spots(rng, 3);
    Game g(GameMode::Classic, kDefaults, rng, kT0);
    show(g, kT0 + ms(15));
    g.pause(kT0 + ms(500));
    g.update(kT0 + ms(2000), {});
    g.click(MouseButton::Left, center_of(g), kT0 + ms(2100));
    g.frame_presented(kT0 + ms(2200), kT0 + ms(2201));
    EXPECT_EQ(g.lives_left(), 15);
    EXPECT_EQ(g.shots(), 0);
    g.resume(kT0 + ms(3500));  // пауза — 3 с
    EXPECT_EQ(g.target().expires_at(), kT0 + ms(4000));
    EXPECT_EQ(g.target().shown_at().value_or(0), kT0 + ms(3016));
    g.update(kT0 + ms(3600), {});
    EXPECT_EQ(g.lives_left(), 15) << "время паузы в жизнь цели не идёт";
    g.click(MouseButton::Left, center_of(g), kT0 + ms(3700));
    EXPECT_DOUBLE_EQ(g.avg_reaction_ms(), 684) << "реакция без паузы: 700 − 16";
}

TEST(Game, TrackingScoresByHoverWithoutShots) {
    ScriptedRandom rng;
    rng.ints({400, 300, 60});  // место и скорость 60 px/с; unit() 0,5 — масштаб 1, влево
    rng.units({0.5, 0.5});
    Game g(GameMode::Tracking, kDefaults, rng, kT0);
    g.click(MouseButton::Left, center_of(g), kT0 + ms(10));
    EXPECT_EQ(g.shots(), 0) << "в Tracking щелчки не читаются";
    Nanoseconds t = kT0;
    for (int i = 0; i < 89; ++i) {
        t += ms(10);
        g.update(t, center_of(g));
    }
    EXPECT_EQ(g.score(), 0);
    rng.ints({200, 200, 60});
    t += ms(10);
    g.update(t, center_of(g));
    EXPECT_EQ(g.score(), 13);
    EXPECT_EQ(g.hits(), 0);
    EXPECT_EQ(g.combo(), 0);
    const std::vector<GameEvent> events = g.take_events();
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0].kind, GameEventKind::Tracked);
    EXPECT_EQ(g.target().spawned_at(), t);
}

TEST(Game, TrackingTargetLivesTwoAndHalfSeconds) {
    ScriptedRandom rng;
    rng.ints({400, 300, 60, 200, 200, 60});
    Game g(GameMode::Tracking, kDefaults, rng, kT0);
    g.update(kT0 + ms(2499), {0, 0});
    EXPECT_EQ(g.lives_left(), 15);
    g.update(kT0 + ms(2500), {0, 0});
    EXPECT_EQ(g.lives_left(), 14);
}

TEST(Game, ReactionHistoryKeepsLastFiftyInOrder) {
    ScriptedRandom rng;
    alternate_spots(rng, 62);
    Game g(GameMode::Classic, kDefaults, rng, kT0);
    Nanoseconds t = kT0;
    for (int i = 0; i < 60; ++i) {
        show(g, t + ms(15));
        g.click(MouseButton::Left, center_of(g), t + ms(16) + ms(static_cast<Nanoseconds>(i)));
        t += ms(500);
    }
    ASSERT_EQ(g.reactions().size(), static_cast<std::size_t>(kReactionHistory));
    EXPECT_EQ(g.reactions().front(), ms(10));
    EXPECT_EQ(g.reactions().back(), ms(59));
    const GameSummary s = g.summary();
    EXPECT_EQ(s.reactions_ms.size(), 50U);
    EXPECT_DOUBLE_EQ(s.reactions_ms.back(), 59);
}

TEST(Game, SummaryAndGiveUp) {
    ScriptedRandom rng;
    alternate_spots(rng, 3);
    Game g(GameMode::Classic, kDefaults, rng, kT0);
    show(g, kT0 + ms(15));
    // На 500 мс радиус — пик, 40 px: 20 px от центра — точность 50 %.
    g.click(MouseButton::Left, {center_of(g).x + 20, center_of(g).y}, kT0 + ms(500));
    g.click(MouseButton::Left, {5, 5}, kT0 + ms(600));
    g.give_up();
    EXPECT_TRUE(g.over());
    const GameSummary s = g.summary();
    EXPECT_EQ(s.mode, GameMode::Classic);
    EXPECT_EQ(s.score, 13);
    EXPECT_EQ(s.hits, 1);
    EXPECT_EQ(s.shots, 2);
    EXPECT_EQ(s.misses, 1);
    EXPECT_EQ(s.max_combo, 1);
    EXPECT_DOUBLE_EQ(s.accuracy, 50);
    EXPECT_DOUBLE_EQ(s.avg_reaction_ms, 484);
    EXPECT_DOUBLE_EQ(s.precision, 50);
    EXPECT_FALSE(s.died_to_bomb);
}

}  // namespace
}  // namespace aim::core
