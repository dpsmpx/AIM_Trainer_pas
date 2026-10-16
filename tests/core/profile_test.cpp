// Профиль: настройки, таблица лидеров, рекорды (D-13), достижения, история
// реакций (docs/ARCHITECTURE.md, §8.1, §10).

#include "core/profile.h"

#include <gtest/gtest.h>

#include <vector>

namespace aim::core {
namespace {

TEST(Settings, StepsAndBoundsLikeSettingsPanel) {
    Settings s;
    for (int i = 0; i < 15; ++i)
        step_setting(s, SettingRow::CircleSize, -1);
    EXPECT_EQ(s.circle_size, kMinCircleSize);
    for (int i = 0; i < 25; ++i)
        step_setting(s, SettingRow::CircleSize, 1);
    EXPECT_EQ(s.circle_size, kMaxCircleSize);
    step_setting(s, SettingRow::Lifetime, -12);
    EXPECT_EQ(s.lifetime_ms, kMinLifetimeMs);
    step_setting(s, SettingRow::Lifetime, 3);
    EXPECT_EQ(s.lifetime_ms, 800);
    step_setting(s, SettingRow::Lives, 100);
    EXPECT_EQ(s.lives, kMaxLives);
    step_setting(s, SettingRow::Lives, -100);
    EXPECT_EQ(s.lives, kMinLives);
}

TEST(Settings, ResetKeepsThemeAndEffects) {
    Settings s{.circle_size = 60,
               .lifetime_ms = 2000,
               .lives = 30,
               .sound = false,
               .menu_effects = false,
               .theme = 1};
    reset_settings(s);
    EXPECT_EQ(s.circle_size, 40);
    EXPECT_EQ(s.lifetime_ms, 1000);
    EXPECT_EQ(s.lives, 15);
    EXPECT_TRUE(s.sound);
    EXPECT_FALSE(s.menu_effects);
    EXPECT_EQ(s.theme, 1);
}

TEST(Settings, ClampFromFile) {
    Settings s{.circle_size = 7, .lifetime_ms = 99999, .lives = -3, .theme = 5};
    clamp_settings(s);
    EXPECT_EQ(s.circle_size, kMinCircleSize);
    EXPECT_EQ(s.lifetime_ms, kMaxLifetimeMs);
    EXPECT_EQ(s.lives, kMinLives);
    EXPECT_EQ(s.theme, 0);
    const GameSettings g = game_settings(Settings{});
    EXPECT_EQ(g.circle_size, 40);
    EXPECT_EQ(g.lifetime_ms, 1000);
    EXPECT_EQ(g.lives, 15);
}

TEST(Leaderboard, SortedTopTenEqualScoresAfterOld) {
    Leaderboard board;
    EXPECT_EQ(add_to_leaderboard(board, GameMode::Dual, 100, 50), 0);
    EXPECT_EQ(add_to_leaderboard(board, GameMode::Dual, 300, 60), 0);
    EXPECT_EQ(add_to_leaderboard(board, GameMode::Dual, 100, 70), 2) << "равные — после прежних";
    const auto& dual = board[static_cast<std::size_t>(mode_index(GameMode::Dual))];
    ASSERT_EQ(dual.size(), 3U);
    EXPECT_EQ(dual[0].score, 300);
    EXPECT_DOUBLE_EQ(dual[1].accuracy, 50);
    EXPECT_DOUBLE_EQ(dual[2].accuracy, 70);
    EXPECT_TRUE(board[0].empty()) << "другие режимы не тронуты";
    for (int i = 0; i < 7; ++i)
        add_to_leaderboard(board, GameMode::Dual, 200, 0);
    EXPECT_EQ(dual.size(), 10U);
    EXPECT_EQ(add_to_leaderboard(board, GameMode::Dual, 100, 0), -1) << "11-е место не пишется";
    EXPECT_EQ(add_to_leaderboard(board, GameMode::Dual, 250, 0), 1);
    EXPECT_EQ(dual.size(), 10U);
    EXPECT_EQ(dual.back().score, 100);
}

TEST(Leaderboard, AllFourModesKept) {
    Leaderboard board;
    for (const GameMode m : kAllModes)
        add_to_leaderboard(board, m, 10 + mode_index(m), 0);
    for (const GameMode m : kAllModes)
        EXPECT_EQ(board[static_cast<std::size_t>(mode_index(m))].at(0).score, 10 + mode_index(m));
}

GameSummary summary_of(GameMode mode, int score, int hits, int shots, double avg_ms,
                       double precision) {
    GameSummary s;
    s.mode = mode;
    s.score = score;
    s.hits = hits;
    s.shots = shots;
    s.misses = shots - hits;
    s.accuracy = shots > 0 ? 100.0 * hits / shots : 0.0;
    s.avg_reaction_ms = avg_ms;
    s.precision = precision;
    return s;
}

TEST(FinishGame, RecordsNeedTwentyShotsByD13) {
    Profile p;
    finish_game(p, summary_of(GameMode::Classic, 13, 1, 1, 300, 90));
    EXPECT_EQ(p.best_score, 13) << "счёт — по любой игре";
    EXPECT_EQ(p.games_played, 1);
    EXPECT_DOUBLE_EQ(p.best_accuracy, 0) << "1 выстрел — не рекорд (D-13)";
    EXPECT_DOUBLE_EQ(p.best_precision, 0);
    EXPECT_DOUBLE_EQ(p.best_avg_reaction, 0);

    finish_game(p, summary_of(GameMode::Classic, 200, 18, 20, 420, 61));
    EXPECT_DOUBLE_EQ(p.best_accuracy, 90);
    EXPECT_DOUBLE_EQ(p.best_precision, 61);
    EXPECT_DOUBLE_EQ(p.best_avg_reaction, 420);

    finish_game(p, summary_of(GameMode::Classic, 100, 10, 25, 380, 50));
    EXPECT_DOUBLE_EQ(p.best_accuracy, 90) << "хуже — не меняет";
    EXPECT_DOUBLE_EQ(p.best_precision, 61);
    EXPECT_DOUBLE_EQ(p.best_avg_reaction, 380) << "реакция — чем меньше, тем лучше";
    EXPECT_EQ(p.best_score, 200);
    EXPECT_EQ(p.games_played, 3);
}

TEST(FinishGame, TrackingDoesNotTouchAccuracyRecords) {
    Profile p;
    finish_game(p, summary_of(GameMode::Tracking, 500, 0, 0, 0, 0));
    EXPECT_EQ(p.best_score, 500);
    EXPECT_DOUBLE_EQ(p.best_accuracy, 0);
    EXPECT_EQ(p.leaderboard[static_cast<std::size_t>(mode_index(GameMode::Tracking))].size(), 1U);
}

TEST(FinishGame, AchievementsOnFinish) {
    Profile p;
    FinishResult r = finish_game(p, summary_of(GameMode::Classic, 10, 18, 20, 400, 50));
    EXPECT_EQ(r.unlocked, (std::vector<int>{0, 5}))
        << "первая игра и снайпер (90 % от 20 выстрелов)";
    EXPECT_EQ(r.leaderboard_place, 0);
    r = finish_game(p, summary_of(GameMode::Classic, 10, 19, 19, 400, 50));
    EXPECT_TRUE(r.unlocked.empty()) << "повторно не открываются";

    Profile q;
    r = finish_game(q, summary_of(GameMode::Classic, 10, 18, 19, 400, 50));
    EXPECT_EQ(r.unlocked, (std::vector<int>{0})) << "снайпер — только от 20 выстрелов";
    r = finish_game(q, summary_of(GameMode::Dual, 499, 10, 10, 400, 50));
    EXPECT_TRUE(r.unlocked.empty());
    r = finish_game(q, summary_of(GameMode::Dual, 500, 10, 10, 400, 50));
    EXPECT_EQ(r.unlocked, (std::vector<int>{7}));

    Profile v;
    v.games_played = 998;
    EXPECT_TRUE(finish_game(v, summary_of(GameMode::Flick, 1, 0, 0, 0, 0)).unlocked.empty());
    EXPECT_EQ(finish_game(v, summary_of(GameMode::Flick, 1, 0, 0, 0, 0)).unlocked,
              (std::vector<int>{6}));
}

TEST(FinishGame, ReactionHistoryKeepsLastFiftyInOrder) {
    Profile p;
    for (int i = 0; i < 40; ++i)
        p.reactions.push_back(1000 + i);
    GameSummary s = summary_of(GameMode::Classic, 0, 0, 0, 0, 0);
    for (int i = 0; i < 20; ++i)
        s.reactions_ms.push_back(200.0 + i + 0.5);
    finish_game(p, s);
    ASSERT_EQ(p.reactions.size(), 50U);
    EXPECT_EQ(p.reactions.front(), 1010);
    EXPECT_EQ(p.reactions[29], 1039);
    EXPECT_EQ(p.reactions[30], 200) << "200,5 → к чётному";
    EXPECT_EQ(p.reactions.back(), 220) << "219,5 → к чётному";
}

TEST(Achievements, OnHitEvents) {
    Profile p;
    GameEvent hit{.kind = GameEventKind::Hit,
                  .position = {},
                  .accuracy = 0.5,
                  .reaction = ms(200),
                  .combo = 1,
                  .score = 13};
    EXPECT_TRUE(achievements_on_event(p, hit).empty());
    hit.accuracy = 0.95;
    EXPECT_EQ(achievements_on_event(p, hit), (std::vector<int>{1}));
    hit.reaction = ms(149);
    EXPECT_EQ(achievements_on_event(p, hit), (std::vector<int>{2}));
    hit.combo = 100;
    hit.score = 1000;
    EXPECT_EQ(achievements_on_event(p, hit), (std::vector<int>{3, 4}));
    EXPECT_TRUE(achievements_on_event(p, hit).empty());

    Profile q;
    const GameEvent tracked{.kind = GameEventKind::Tracked, .position = {}, .score = 5000};
    EXPECT_TRUE(achievements_on_event(q, tracked).empty()) << "как у образца: только по щелчку";
    EXPECT_FALSE(unlock(q, 8));
    EXPECT_EQ(kAchievements[7].name, "Мастер Dual");
}

}  // namespace
}  // namespace aim::core
