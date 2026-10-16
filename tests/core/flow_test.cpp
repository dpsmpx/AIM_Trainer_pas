// Экраны и переходы (docs/ARCHITECTURE.md, §2): нажатия не переходят между
// экранами (AUD-02, сценарии 5–7), клавиши не теряются (AUD-16).

#include "core/flow.h"

#include <gtest/gtest.h>

#include <optional>
#include <variant>
#include <vector>

#include "scripted_random.h"

namespace aim::core {
namespace {

using testing::ScriptedRandom;

constexpr Nanoseconds kT0 = ms(1'000);

// Меню для тестов: щелчок правее x = 400 запускает Classic.
class FakeMenu final : public MenuInput {
public:
    std::optional<GameMode> click(const ClickInput& c) override {
        clicks.push_back(c);
        if (c.position.x > 400 && c.button == MouseButton::Left) return GameMode::Classic;
        return std::nullopt;
    }
    void key(const KeyInput& k) override { keys.push_back(k); }

    std::vector<ClickInput> clicks;
    std::vector<KeyInput> keys;
};

ClickInput click_at(double x, double y, Nanoseconds t, MouseButton b = MouseButton::Left) {
    return {.button = b, .position = {x, y}, .time = t};
}

KeyInput key(Key k, Nanoseconds t) {
    return {.key = k, .time = t};
}

struct Fixture {
    Fixture() {
        for (int i = 0; i < 40; ++i)
            rng.ints(i % 2 == 0 ? std::initializer_list<int>{200, 200}
                                : std::initializer_list<int>{600, 400});
    }

    // Меню → отсчёт щелчком в t, отсчёт до конца; возвращает начало забега.
    Nanoseconds start_game(Nanoseconds t) {
        const std::vector<Input> in{click_at(500, 50, t)};
        flow.input(in, t);
        flow.update(t + kCountdown, {});
        return t + kCountdown;
    }

    void send(std::vector<Input> in, Nanoseconds now) { flow.input(in, now); }

    Profile profile;
    ScriptedRandom rng;
    FakeMenu menu;
    Flow flow{profile, rng, menu};
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
    EXPECT_EQ(g->target().spawned_at(), kT0 + ms(10) + kCountdown) << "забег — с конца отсчёта";
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

TEST(Flow, PauseKeysInOneFrameAreAllHandled) {
    Fixture f;
    const Nanoseconds start = f.start_game(kT0);
    f.send({key(Key::P, start + ms(100))}, start + ms(105));
    ASSERT_TRUE(f.flow.game()->paused());
    // Две P в одном кадре — пауза и снятие: ни одна не теряется (AUD-16).
    f.send({key(Key::P, start + ms(2100)), key(Key::P, start + ms(2150))}, start + ms(2160));
    EXPECT_TRUE(f.flow.game()->paused());
    f.send({key(Key::P, start + ms(3100))}, start + ms(3105));
    ASSERT_FALSE(f.flow.game()->paused());
    // Пауза длилась 100 → 2100 и 2150 → 3100: жизнь цели сдвинута на 2950 мс.
    EXPECT_EQ(f.flow.game()->target().expires_at(), start + ms(1000) + ms(2950));
}

TEST(Flow, RestartDoesNotRecordRun) {
    Fixture f;
    const Nanoseconds start = f.start_game(kT0);
    f.send({click_at(5, 5, start + ms(100))}, start + ms(100));
    f.send({key(Key::R, start + ms(200))}, start + ms(200));
    EXPECT_EQ(f.flow.screen(), Screen::Countdown);
    EXPECT_EQ(f.profile.games_played, 0);
    EXPECT_EQ(count_of<RunFinished>(f.flow.take_events()), 0);
    f.flow.update(start + ms(200) + kCountdown, {});
    ASSERT_EQ(f.flow.screen(), Screen::Game);
    EXPECT_EQ(f.flow.game()->shots(), 0) << "новый забег — с нуля";
}

TEST(Flow, MenuKeyRecordsRunOnce) {
    Fixture f;
    const Nanoseconds start = f.start_game(kT0);
    f.send({key(Key::M, start + ms(100)), key(Key::M, start + ms(101))}, start + ms(105));
    EXPECT_EQ(f.flow.screen(), Screen::Menu);
    EXPECT_EQ(f.profile.games_played, 1);
    EXPECT_EQ(f.menu.keys.size(), 0U) << "вторая M — уже после смены экрана";
    const std::vector<FlowEvent> events = f.flow.take_events();
    EXPECT_EQ(count_of<RunFinished>(events), 1);
}

TEST(Flow, LivesRunOutGoesToGameOverAndRecords) {
    Fixture f;
    f.profile.settings.lives = 5;
    const Nanoseconds start = f.start_game(kT0);
    std::vector<Input> misses;
    misses.reserve(6);
    for (int i = 0; i < 6; ++i)
        misses.emplace_back(click_at(5, 5, start + ms(100) + ms(static_cast<Nanoseconds>(i))));
    f.send(misses, start + ms(110));
    EXPECT_EQ(f.flow.screen(), Screen::GameOver);
    EXPECT_EQ(f.flow.game()->shots(), 5) << "шестой щелчок — уже не игре";
    EXPECT_EQ(f.profile.games_played, 1);
    f.flow.update(start + ms(200), {});
    EXPECT_EQ(f.profile.games_played, 1) << "запись — один раз";
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
    // Первая игра — достижение по итогу, затем запись забега и смена экрана.
    ASSERT_EQ(events.size(), 3U);
    EXPECT_EQ(std::get<AchievementUnlocked>(events[0]).id, 0);
    EXPECT_EQ(std::get<RunFinished>(events[1]).summary.hits, 1);
    EXPECT_EQ(std::get<ScreenChanged>(events[2]).to, Screen::GameOver);
}

TEST(Flow, MenuGetsKeys) {
    Fixture f;
    f.send({key(Key::Escape, kT0)}, kT0);
    ASSERT_EQ(f.menu.keys.size(), 1U);
    EXPECT_EQ(f.menu.keys[0].key, Key::Escape);
}

}  // namespace
}  // namespace aim::core
