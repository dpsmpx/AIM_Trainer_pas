// Профиль: настройки, выбранный набор, итог замера — история, рекорд набора
// и достижения (D-13, D-37, D-38).

#include "core/profile.h"

#include <gtest/gtest.h>

#include <vector>

namespace aim::core {
namespace {

RunRecord run_of(const MeasureOptions& options, int score, int shots = 30, int hits = 25,
                 double median_ms = 300) {
    RunRecord r;
    r.summary.options = normalized(options);
    r.summary.score = score;
    r.summary.shots = shots;
    r.summary.hits = hits;
    r.summary.accuracy = shots > 0 ? 100.0 * hits / shots : 0;
    r.summary.median_reaction_ms = median_ms;
    r.summary.avg_reaction_ms = median_ms;
    return r;
}

TEST(Settings, StepsAndBounds) {
    Settings s;
    for (int i = 0; i < 25; ++i)
        step_setting(s, SettingRow::TargetSize, -1);
    EXPECT_EQ(s.preset.target_size, kMinTargetSize);
    for (int i = 0; i < 25; ++i)
        step_setting(s, SettingRow::TargetSize, 1);
    EXPECT_EQ(s.preset.target_size, kMaxTargetSize);
    step_setting(s, SettingRow::Lifetime, -12);
    EXPECT_EQ(s.preset.lifetime_ms, kMinBaseLifetimeMs);
    step_setting(s, SettingRow::Lifetime, 3);
    EXPECT_EQ(s.preset.lifetime_ms, 800);
    step_setting(s, SettingRow::Lives, 100);
    EXPECT_EQ(s.preset.lives, kMaxLives);
    step_setting(s, SettingRow::Lives, -100);
    EXPECT_EQ(s.preset.lives, kMinLives);
}

TEST(Settings, AverageRunsStepsThroughList) {
    Settings s;
    EXPECT_EQ(s.average_runs, 50);
    step_setting(s, SettingRow::AverageRuns, 1);
    EXPECT_EQ(s.average_runs, 100);
    step_setting(s, SettingRow::AverageRuns, -3);
    EXPECT_EQ(s.average_runs, 20);
    step_setting(s, SettingRow::AverageRuns, -10);
    EXPECT_EQ(s.average_runs, 5);
    s.average_runs = 47;  // не ступень — от ближайшей
    step_setting(s, SettingRow::AverageRuns, 1);
    EXPECT_EQ(s.average_runs, 100);
}

TEST(Settings, ResetKeepsThemeAndEffects) {
    Settings s;
    s.preset = {.target_size = 60, .lifetime_ms = 2000, .lives = 30, .ending = Ending::Time};
    s.average_runs = 200;
    s.sound = false;
    s.menu_effects = false;
    s.theme = 1;
    s.hud = false;
    s.replays = false;
    s.font = 1;
    reset_settings(s);
    EXPECT_EQ(s.preset.target_size, 40);
    EXPECT_EQ(s.preset.lifetime_ms, 1000);
    EXPECT_EQ(s.preset.lives, 15);
    EXPECT_EQ(s.preset.ending, Ending::Lives);
    EXPECT_EQ(s.average_runs, 50);
    EXPECT_TRUE(s.sound);
    EXPECT_TRUE(s.hud) << "интерфейс замера — снова включён (D-45)";
    EXPECT_TRUE(s.replays);
    EXPECT_FALSE(s.menu_effects);
    EXPECT_EQ(s.theme, 1);
    EXPECT_EQ(s.font, 1) << "шрифт сброс не трогает, как тему";
}

TEST(Settings, OutOfRangeFromFileGoesToDefault) {
    Settings s;
    s.preset.target_size = 7;
    s.preset.lifetime_ms = 9000;
    s.average_runs = 0;
    s.theme = 9;
    EXPECT_EQ(reset_out_of_range(s), 4);
    EXPECT_EQ(s.preset.target_size, 40);
    EXPECT_EQ(s.preset.lifetime_ms, 1000);
    EXPECT_EQ(s.average_runs, 50);
    EXPECT_EQ(s.theme, 0);
    EXPECT_EQ(reset_out_of_range(s), 0);
}

TEST(Profile, SelectedOptionsArePresetWithSettings) {
    Profile p;
    p.preset = 4;  // «Флик»
    p.settings.preset.lifetime_ms = 2000;
    p.settings.preset.target_size = 30;
    const MeasureOptions o = selected_options(p);
    EXPECT_EQ(o.lifetime_ms, 1400) << "жизнь цели Флика — ×0,7 (D-21)";
    EXPECT_EQ(o.target_size, 30);
    EXPECT_FALSE(o.grows);
}

TEST(FinishGame, AppendsRunAndFindsNewBestPerCombo) {
    Profile p;
    const MeasureOptions classic = preset_options(1, {});
    const MeasureOptions flick = preset_options(4, {});
    EXPECT_TRUE(finish_game(p, run_of(classic, 100)).new_best);
    EXPECT_FALSE(finish_game(p, run_of(classic, 90)).new_best);
    EXPECT_FALSE(finish_game(p, run_of(classic, 100)).new_best) << "равный — не новый";
    EXPECT_TRUE(finish_game(p, run_of(flick, 10)).new_best) << "у другого набора — свой рекорд";
    EXPECT_TRUE(finish_game(p, run_of(classic, 101)).new_best);
    EXPECT_EQ(p.runs.size(), 5U);
}

TEST(FinishGame, ReactionRecordNeedsTwentyShotsByD13) {
    Profile p;
    const MeasureOptions screen = preset_options(0, {});
    EXPECT_FALSE(finish_game(p, run_of(screen, 0, 5, 5, 150)).new_best)
        << "5 реакций — в рекорд не идут";
    EXPECT_TRUE(finish_game(p, run_of(screen, 0, 20, 19, 300)).new_best);
    EXPECT_TRUE(finish_game(p, run_of(screen, 0, 25, 25, 250)).new_best) << "меньше — лучше";
    EXPECT_FALSE(finish_game(p, run_of(screen, 0, 25, 25, 260)).new_best);
}

TEST(FinishGame, AchievementsOnFinish) {
    Profile p;
    const MeasureOptions two = preset_options(2, {});
    const FinishResult first = finish_game(p, run_of(two, 600, 30, 28));
    // Первый замер, «Снайпер» (28 из 30 — 93 %), «Обе руки» (600 очков).
    EXPECT_EQ(first.unlocked, (std::vector<int>{0, 5, 7}));
    EXPECT_TRUE(finish_game(p, run_of(two, 700)).unlocked.empty());
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
    EXPECT_TRUE(achievements_on_event(q, tracked).empty()) << "только по попаданию щелчком";
    EXPECT_FALSE(unlock(q, 8));
    EXPECT_EQ(kAchievements[7].key, "two_hands");
}

}  // namespace
}  // namespace aim::core
