// Прогон замера каждого набора со сверкой итога (AUD-49): настоящее меню —
// щелчок по кнопке набора, Flow, генератор замера по началу, бот. Итог
// сверяется с событиями замера и с записью в профиле: счёт — сумма очков
// событий, попадания, промахи, истёкшие и засчитанные цели — их число,
// лучшая серия — наибольшая серия событий, точность и реакция — из
// попаданий; окончание по времени — ровно через заданное время, по жизням
// — когда жизни кончились.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <variant>
#include <vector>

#include "core/flow.h"
#include "core/layout.h"
#include "core/menu.h"
#include "core/profile.h"
#include "core/random.h"

namespace aim::core {
namespace {

constexpr Nanoseconds kT0 = ms(1'000);
constexpr Nanoseconds kFrame = 6'944'444;  // 144 Гц

enum class Bot : std::uint8_t {
    Aim,  ///< щелчок по показанной цели её кнопкой, в стороне от центра; у
          ///< замера на цвет — после смены цвета, и один фальстарт
    Idle,  ///< ничего не делает
};

struct Played {
    GameSummary summary;
    std::vector<GameEvent> events;
    Profile profile;
};

Vec2 center(const Rect& r) {
    return {(r.x1 + r.x2) / 2.0, (r.y1 + r.y2) / 2.0};
}

// Набор preset с окончанием ending: щелчок по его кнопке в меню, замер не
// дольше length, затем Esc.
Played play(int preset, Ending ending, Bot bot, Nanoseconds length) {
    Played run;
    Profile& profile = run.profile;
    profile.tutorial_seen = true;
    profile.settings.preset.ending = ending;
    profile.settings.preset.lives = 5;
    EngineRandom seeds(20261017U + static_cast<unsigned>(preset));
    Menu menu(profile, 1, false);
    menu.enter(0);
    Flow flow(profile, seeds, menu, engine_random, [] { return LocalTime{.year = 2026}; });

    const std::vector<Input> start{
        ClickInput{.position = center(preset_button(preset, ms(10'000))), .time = kT0}};
    flow.input(start, kT0);
    EXPECT_EQ(flow.screen(), Screen::Countdown) << "кнопка набора " << preset;
    Nanoseconds now = kT0;
    Vec2 cursor = bot == Bot::Idle ? Vec2{1, 1} : Vec2{400, 300};
    Nanoseconds game_start = 0;
    bool false_start = false;
    Nanoseconds last_click = 0;
    for (int i = 0; i < 200'000 && flow.screen() != Screen::GameOver; ++i) {
        now += kFrame;
        std::vector<Input> in;
        if (const Game* g = flow.game(); g != nullptr && flow.screen() == Screen::Game) {
            if (game_start == 0) game_start = now;
            const Target& t = g->target();
            if (bot == Bot::Aim && g->options().action == Action::Hover) cursor = t.center();
            const bool ready = bot == Bot::Aim && now - last_click > ms(150);
            if (ready && g->screen_stimulus()) {
                if (!false_start || g->signal_lit(now - kFrame)) {
                    in.emplace_back(ClickInput{.position = cursor, .time = now - ms(1)});
                    false_start = true;
                    last_click = now;
                }
            } else if (ready && g->options().action == Action::Click && t.shown_at() &&
                       !t.is_bomb() && t.radius_at(now) > 6) {
                const MouseButton b = t.color() == 1 ? MouseButton::Right : MouseButton::Left;
                cursor = {t.center().x + t.radius_at(now) * 0.4, t.center().y};
                in.emplace_back(ClickInput{.button = b, .position = cursor, .time = now - ms(1)});
                last_click = now;
            }
            if (now - game_start >= length)
                in.emplace_back(KeyInput{.key = Key::Escape, .time = now});
        }
        flow.input(in, now);
        flow.update(now, cursor);
        flow.frame_presented(now, now + ms(2));
        for (const FlowEvent& e : flow.take_events()) {
            if (const auto* ge = std::get_if<GameEvent>(&e)) run.events.push_back(*ge);
            if (const auto* f = std::get_if<RunFinished>(&e)) run.summary = f->record.summary;
        }
    }
    EXPECT_EQ(flow.screen(), Screen::GameOver);
    return run;
}

int count(const Played& r, GameEventKind kind) {
    return static_cast<int>(
        std::ranges::count_if(r.events, [&](const GameEvent& e) { return e.kind == kind; }));
}

// Итог — из событий замера и он же — в профиле.
void expect_consistent(const Played& r) {
    const GameSummary& s = r.summary;
    ASSERT_EQ(r.profile.runs.size(), 1U) << "замер записан один раз";
    const GameSummary& kept = r.profile.runs.front().summary;
    EXPECT_EQ(kept.score, s.score);
    EXPECT_EQ(kept.hits, s.hits);
    EXPECT_EQ(kept.end, s.end);
    EXPECT_EQ(kept.duration_ms, s.duration_ms);
    EXPECT_EQ(s.hits, count(r, GameEventKind::Hit));
    EXPECT_EQ(s.misses, count(r, GameEventKind::Miss) + count(r, GameEventKind::FalseStart));
    EXPECT_EQ(s.expired, count(r, GameEventKind::Expired));
    EXPECT_EQ(s.tracked, count(r, GameEventKind::Tracked));
    EXPECT_EQ(s.shots, s.hits + s.misses + (s.end == EndReason::Bomb ? 1 : 0));
    const int points = std::accumulate(r.events.begin(), r.events.end(), 0,
                                       [](int sum, const GameEvent& e) { return sum + e.points; });
    EXPECT_EQ(s.score, points) << "счёт — сумма очков событий";
    if (!r.events.empty()) {
        EXPECT_EQ(s.score, r.events.back().score);
    }
    int best = 0;
    for (const GameEvent& e : r.events)
        best = std::max(best, e.combo);
    EXPECT_EQ(s.max_combo, best);
    EXPECT_DOUBLE_EQ(s.accuracy, s.shots > 0 ? 100.0 * s.hits / s.shots : 0.0);
    EXPECT_EQ(s.reactions_ms.size(), static_cast<std::size_t>(s.hits));
    if (!s.reactions_ms.empty()) {
        const double sum = std::accumulate(s.reactions_ms.begin(), s.reactions_ms.end(), 0.0);
        EXPECT_NEAR(s.avg_reaction_ms, sum / static_cast<double>(s.reactions_ms.size()), 1e-9);
        const auto [lo, hi] = std::ranges::minmax_element(s.reactions_ms);
        EXPECT_GE(s.median_reaction_ms, *lo);
        EXPECT_LE(s.median_reaction_ms, *hi);
    }
}

class PresetRun : public ::testing::TestWithParam<int> {};

TEST_P(PresetRun, AimedRunAddsUp) {
    const Played r = play(GetParam(), Ending::Lives, Bot::Aim, ms(8'000));
    expect_consistent(r);
    EXPECT_GE(r.summary.hits + r.summary.tracked, 2) << "бот попадал";
    if (r.summary.end != EndReason::Bomb && r.summary.end != EndReason::Lives) {
        EXPECT_EQ(r.summary.end, EndReason::GaveUp);
    }
}

TEST_P(PresetRun, TimeEndingStopsOnTime) {
    const Played r = play(GetParam(), Ending::Time, Bot::Aim, ms(600'000));
    expect_consistent(r);
    if (r.summary.end == EndReason::Bomb) return;  // бомба кончает замер раньше
    EXPECT_EQ(r.summary.end, EndReason::Time);
    EXPECT_EQ(r.summary.duration_ms, r.summary.options.duration_s * 1000);
}

TEST_P(PresetRun, IdleRunLosesLivesToExpiredTargets) {
    const Played r = play(GetParam(), Ending::Lives, Bot::Idle, ms(600'000));
    expect_consistent(r);
    const MeasureOptions& o = r.summary.options;
    if (o.stimulus == Stimulus::Screen) {
        // Без щелчков у замера на цвет жизни не тратятся: конец — по
        // времени набора или Esc.
        EXPECT_NE(r.summary.end, EndReason::Lives);
        EXPECT_EQ(r.summary.shots, 0);
        return;
    }
    EXPECT_EQ(r.summary.end, EndReason::Lives);
    EXPECT_EQ(r.summary.shots, 0);
    EXPECT_EQ(r.summary.expired, o.lives) << "каждая жизнь — истёкшей целью (D-14)";
    EXPECT_EQ(r.summary.score, 0);
}

INSTANTIATE_TEST_SUITE_P(AllPresets, PresetRun, ::testing::Range(0, kPresetCount));

}  // namespace
}  // namespace aim::core
