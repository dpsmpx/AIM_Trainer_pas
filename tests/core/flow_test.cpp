// Экраны и переходы (docs/ARCHITECTURE.md, §2): нажатия не переходят между
// экранами (AUD-02, сценарии 5–7), клавиши не теряются (AUD-16).

#include "core/flow.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "scripted_random.h"

namespace aim::core {
namespace {

using testing::ScriptedRandom;

constexpr Nanoseconds kT0 = ms(1'000);

// Меню для тестов: щелчок правее x = 400 запускает «Классику» с общими
// настройками профиля.
class FakeMenu final : public MenuInput {
public:
    explicit FakeMenu(const Profile& p) : profile(p) {}

    std::optional<MeasureOptions> click(const ClickInput& c) override {
        clicks.push_back(c);
        if (c.position.x > 400 && c.button == MouseButton::Left)
            return preset_options(1, profile.settings.preset);
        return std::nullopt;
    }
    void key(const KeyInput& k) override { keys.push_back(k); }
    void wheel(const WheelInput& w) override { wheels.push_back(w); }

    const Profile& profile;
    std::vector<ClickInput> clicks;
    std::vector<KeyInput> keys;
    std::vector<WheelInput> wheels;
};

ClickInput click_at(double x, double y, Nanoseconds t, MouseButton b = MouseButton::Left) {
    return {.button = b, .position = {x, y}, .time = t};
}

KeyInput key(Key k, Nanoseconds t) {
    return {.key = k, .time = t};
}

// Генератор замера: места целей по очереди (200, 200) и (600, 400).
std::unique_ptr<Random> scripted_spots(std::uint64_t /*seed*/) {
    auto rng = std::make_unique<ScriptedRandom>();
    for (int i = 0; i < 40; ++i)
        rng->ints(i % 2 == 0 ? std::initializer_list<int>{200, 200}
                             : std::initializer_list<int>{600, 400});
    return rng;
}

struct Fixture {
    // Меню → отсчёт щелчком в t, отсчёт до конца; возвращает начало забега.
    Nanoseconds start_game(Nanoseconds t) {
        const std::vector<Input> in{click_at(500, 50, t)};
        flow.input(in, t);
        flow.update(t + kCountdown, {});
        return t + kCountdown;
    }

    void send(std::vector<Input> in, Nanoseconds now) { flow.input(in, now); }

    Profile profile;
    ScriptedRandom seeds;
    FakeMenu menu{profile};
    Flow flow{profile, seeds, menu, scripted_spots, [] { return LocalTime{.year = 2026}; }};
};

template <class T>
int count_of(const std::vector<FlowEvent>& events) {
    int n = 0;
    for (const FlowEvent& e : events)
        n += std::holds_alternative<T>(e) ? 1 : 0;
    return n;
}

TEST(Flow, ClickStartingModeDoesNotReachGame) {
    Fixture f;
    // Щелчок запуска и ещё два в том же кадре: второй, третий — уже не меню.
    f.send({click_at(500, 50, kT0), click_at(500, 50, kT0 + ms(5)), key(Key::Escape, kT0 + ms(6))},
           kT0 + ms(10));
    EXPECT_EQ(f.flow.screen(), Screen::Countdown);
    EXPECT_EQ(f.menu.clicks.size(), 1U);
    EXPECT_EQ(f.flow.countdown_value(kT0 + ms(10)), 3);
    EXPECT_EQ(f.flow.countdown_value(kT0 + ms(1010)), 2);
    EXPECT_EQ(f.flow.countdown_value(kT0 + ms(2999)), 1);
    f.flow.update(kT0 + ms(10) + kCountdown - 1, {});
    EXPECT_EQ(f.flow.screen(), Screen::Countdown);
    f.flow.update(kT0 + ms(10) + kCountdown, {});
    ASSERT_EQ(f.flow.screen(), Screen::Game);
    const Game* g = f.flow.game();
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->shots(), 0) << "сценарий 5: до первого щелчка выстрелов нет";
    EXPECT_EQ(g->lives_left(), 15);
    EXPECT_EQ(g->target().spawned_at(), kT0 + ms(10) + kCountdown) << "замер — с конца отсчёта";
}

TEST(Flow, RightClickInMenuAndClicksInCountdownDoNotReachGame) {
    Fixture f;
    f.send({click_at(100, 100, kT0, MouseButton::Right)}, kT0);
    EXPECT_EQ(f.flow.screen(), Screen::Menu);
    f.send({click_at(500, 50, kT0 + ms(100))}, kT0 + ms(100));
    f.send({click_at(300, 300, kT0 + ms(500)), click_at(300, 300, kT0 + ms(501))}, kT0 + ms(510));
    f.flow.update(kT0 + ms(100) + kCountdown, {});
    ASSERT_EQ(f.flow.screen(), Screen::Game);
    EXPECT_EQ(f.flow.game()->shots(), 0) << "сценарий 6";
}

TEST(Flow, CountdownMGoesToMenu) {
    Fixture f;
    f.send({click_at(500, 50, kT0)}, kT0);
    f.send({key(Key::M, kT0 + ms(500))}, kT0 + ms(500));
    EXPECT_EQ(f.flow.screen(), Screen::Menu);
    f.flow.update(kT0 + kCountdown, {});
    EXPECT_EQ(f.flow.screen(), Screen::Menu);
    EXPECT_EQ(f.flow.game(), nullptr);
}

TEST(Flow, GameOverClickDoesNotReachMenu) {
    Fixture f;
    const Nanoseconds start = f.start_game(kT0);
    f.send({key(Key::Escape, start + ms(100))}, start + ms(100));
    ASSERT_EQ(f.flow.screen(), Screen::GameOver);
    ASSERT_NE(f.flow.game(), nullptr) << "итоги и тепловая карта — на Game Over";
    const std::size_t menu_clicks = f.menu.clicks.size();
    f.send({click_at(500, 50, start + ms(200)), key(Key::Space, start + ms(300))}, start + ms(300));
    EXPECT_EQ(f.flow.screen(), Screen::Menu);
    EXPECT_EQ(f.menu.clicks.size(), menu_clicks) << "сценарий 7: щелчок Game Over гаснет там же";
}

TEST(Flow, EscEndsAttemptAndRecordsIt) {
    Fixture f;
    const Nanoseconds start = f.start_game(kT0);
    f.send({key(Key::Escape, start + ms(1500))}, start + ms(1505));
    ASSERT_EQ(f.flow.screen(), Screen::GameOver);
    ASSERT_EQ(f.profile.runs.size(), 1U);
    const GameSummary& s = f.profile.runs.back().summary;
    EXPECT_EQ(s.end, EndReason::GaveUp);
    EXPECT_EQ(s.duration_ms, 1500);
    EXPECT_EQ(s.options, preset_options(1, f.profile.settings.preset));
    ASSERT_TRUE(f.flow.last_run().has_value());
    EXPECT_EQ(f.flow.last_run().value_or(RunFinished{}).record.when.year, 2026);
}

TEST(Flow, RestartDoesNotRecordRun) {
    Fixture f;
    const Nanoseconds start = f.start_game(kT0);
    f.send({click_at(5, 5, start + ms(100))}, start + ms(100));
    f.send({key(Key::R, start + ms(200))}, start + ms(200));
    EXPECT_EQ(f.flow.screen(), Screen::Countdown);
    EXPECT_TRUE(f.profile.runs.empty());
    EXPECT_EQ(count_of<RunFinished>(f.flow.take_events()), 0);
    f.flow.update(start + ms(200) + kCountdown, {});
    ASSERT_EQ(f.flow.screen(), Screen::Game);
    EXPECT_EQ(f.flow.game()->shots(), 0) << "новый замер — с нуля";
}

TEST(Flow, MenuKeyRecordsRunOnce) {
    Fixture f;
    const Nanoseconds start = f.start_game(kT0);
    f.send({key(Key::M, start + ms(100)), key(Key::M, start + ms(101))}, start + ms(105));
    EXPECT_EQ(f.flow.screen(), Screen::Menu);
    EXPECT_EQ(f.profile.runs.size(), 1U);
    EXPECT_EQ(f.menu.keys.size(), 0U) << "вторая M — уже после смены экрана";
    const std::vector<FlowEvent> events = f.flow.take_events();
    EXPECT_EQ(count_of<RunFinished>(events), 1);
}

TEST(Flow, LivesRunOutGoesToGameOverAndRecords) {
    Fixture f;
    f.profile.settings.preset.lives = 5;
    const Nanoseconds start = f.start_game(kT0);
    std::vector<Input> misses;
    misses.reserve(6);
    for (int i = 0; i < 6; ++i)
        misses.emplace_back(click_at(5, 5, start + ms(100) + ms(static_cast<Nanoseconds>(i))));
    f.send(misses, start + ms(110));
    EXPECT_EQ(f.flow.screen(), Screen::GameOver);
    EXPECT_EQ(f.flow.game()->shots(), 5) << "шестой щелчок — уже не игре";
    EXPECT_EQ(f.profile.runs.size(), 1U);
    f.flow.update(start + ms(200), {});
    EXPECT_EQ(f.profile.runs.size(), 1U) << "запись — один раз";
    f.send({key(Key::R, start + ms(300))}, start + ms(300));
    EXPECT_EQ(f.flow.screen(), Screen::Countdown);
}

TEST(Flow, HitEventsAndAchievements) {
    Fixture f;
    const Nanoseconds start = f.start_game(kT0);
    const std::vector<FlowEvent> started = f.flow.take_events();
    ASSERT_EQ(count_of<ScreenChanged>(started), 2) << "меню → отсчёт → игра";
    f.flow.update(start + ms(15), {});
    f.flow.frame_presented(start + ms(15), start + ms(16));
    const Vec2 c = f.flow.game()->target().center();
    f.send({click_at(c.x, c.y, start + ms(100))}, start + ms(105));
    std::vector<FlowEvent> events = f.flow.take_events();
    ASSERT_GE(events.size(), 3U);
    ASSERT_TRUE(std::holds_alternative<GameEvent>(events[0]));
    EXPECT_EQ(std::get<GameEvent>(events[0]).reaction, ms(84));
    // В яблочко (точность 1) и Молниеносно (84 мс).
    EXPECT_EQ(std::get<AchievementUnlocked>(events[1]).id, 1);
    EXPECT_EQ(std::get<AchievementUnlocked>(events[2]).id, 2);
    f.send({key(Key::Escape, start + ms(200))}, start + ms(200));
    events = f.flow.take_events();
    // Первый замер — достижение по итогу, затем запись замера и смена экрана.
    ASSERT_EQ(events.size(), 3U);
    EXPECT_EQ(std::get<AchievementUnlocked>(events[0]).id, 0);
    EXPECT_EQ(std::get<RunFinished>(events[1]).record.summary.hits, 1);
    EXPECT_TRUE(std::get<RunFinished>(events[1]).new_best) << "первый замер набора — рекорд";
    EXPECT_EQ(std::get<ScreenChanged>(events[2]).to, Screen::GameOver);
}

TEST(Flow, MenuGetsKeysAndWheel) {
    Fixture f;
    f.send({key(Key::Escape, kT0), WheelInput{.y = -1, .time = kT0}}, kT0);
    ASSERT_EQ(f.menu.keys.size(), 1U);
    EXPECT_EQ(f.menu.keys[0].key, Key::Escape);
    ASSERT_EQ(f.menu.wheels.size(), 1U);
    EXPECT_FLOAT_EQ(f.menu.wheels[0].y, -1);
}

TEST(Flow, WheelAndArrowsDoNotTouchRun) {
    Fixture f;
    const Nanoseconds start = f.start_game(kT0);
    f.send({WheelInput{.y = 1, .time = start + ms(10)}, key(Key::Up, start + ms(11)),
            key(Key::PageDown, start + ms(12))},
           start + ms(20));
    EXPECT_EQ(f.flow.screen(), Screen::Game);
    EXPECT_EQ(f.flow.game()->shots(), 0);
    EXPECT_TRUE(f.menu.wheels.empty());
}

}  // namespace
}  // namespace aim::core
