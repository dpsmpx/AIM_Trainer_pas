// Реплей (D-39): замер, проигранный из записи, даёт тот же итог; текст файла
// читается в ту же запись; экран реплея ничего не записывает.

#include "core/replay.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <random>
#include <variant>
#include <vector>

#include "core/flow.h"
#include "scripted_random.h"

namespace aim::core {
namespace {

constexpr Nanoseconds kT0 = ms(2'000);
constexpr Nanoseconds kFrame = 6'944'444;  // 144 Гц

// Меню: щелчок запускает набор preset с настройками профиля.
class PresetMenu final : public MenuInput {
public:
    PresetMenu(const Profile& p, int preset) : profile(p), preset_(preset) {}

    std::optional<MeasureOptions> click(const ClickInput& /*click*/) override {
        return preset_options(preset_, profile.settings.preset);
    }
    void key(const KeyInput& /*key*/) override {}
    void wheel(const WheelInput& /*wheel*/) override {}

    const Profile& profile;

private:
    int preset_;
};

struct Played {
    std::shared_ptr<const Replay> replay;
    GameSummary summary;
};

// Игрок-бот: курсор тянется к цели, щелчок — когда цель показана, иногда
// мимо и раньше времени; у замера на цвет — после показа сигнала, иногда
// фальстарт. Замер идёт frames кадров, потом Esc, если ещё не кончился.
Played play(int preset, Ending ending, int frames, std::uint32_t bot_seed) {
    Profile profile;
    profile.settings.preset.ending = ending;
    profile.settings.preset.lives = 5;
    testing::ScriptedRandom seeds;
    seeds.ints({123456, 654321});
    PresetMenu menu(profile, preset);
    Flow flow(profile, seeds, menu);
    std::mt19937 bot(bot_seed);
    std::uniform_int_distribution<int> chance(0, 99);
    std::uniform_real_distribution<double> jitter(-30.0, 30.0);

    const std::vector<Input> start{ClickInput{.position = {500, 50}, .time = kT0}};
    flow.input(start, kT0);
    Nanoseconds now = kT0;
    Vec2 cursor{400, 300};
    int game_frames = 0;
    for (int i = 0; i < 100'000 && flow.screen() != Screen::GameOver; ++i) {
        now += kFrame;
        std::vector<Input> in;
        if (const Game* g = flow.game(); g != nullptr && flow.screen() == Screen::Game) {
            ++game_frames;
            const Target& t = g->target();
            cursor.x += (t.center().x - cursor.x) * 0.3 + jitter(bot) * 0.1;
            cursor.y += (t.center().y - cursor.y) * 0.3 + jitter(bot) * 0.1;
            const int roll = chance(bot);
            // Сырой ввод — до 3 мс раньше кадра, бывает и раньше прошлого кадра.
            const Nanoseconds when = now - static_cast<Nanoseconds>(roll % 4) * ms(1);
            if (g->screen_stimulus()) {
                if ((g->signal_lit(now - kFrame) && roll < 30) || roll == 99)
                    in.emplace_back(ClickInput{.position = cursor, .time = when});
            } else if (t.shown_at() && roll < 12) {
                const Vec2 aim{t.center().x + jitter(bot), t.center().y + jitter(bot)};
                const MouseButton b = t.color() == 1 ? MouseButton::Right : MouseButton::Left;
                in.emplace_back(ClickInput{.button = b, .position = aim, .time = when});
            }
            if (game_frames == frames)
                in.emplace_back(KeyInput{.key = Key::Escape, .time = now - ms(1)});
        }
        flow.input(in, now);
        flow.update(now, cursor);
        flow.frame_presented(now, now + ms(4) + static_cast<Nanoseconds>(i % 3) * 100'000);
    }
    EXPECT_EQ(flow.screen(), Screen::GameOver);
    Played played;
    for (const FlowEvent& e : flow.take_events())
        if (const auto* run = std::get_if<RunFinished>(&e)) {
            played.replay = run->replay;
            played.summary = run->record.summary;
        }
    return played;
}

// Проиграть запись с начала start до конца.
GameSummary replay_summary(const std::shared_ptr<const Replay>& replay, Nanoseconds start) {
    ReplayPlayer player(replay, engine_random, start);
    for (Nanoseconds t = start; !player.done(); t += kFrame)
        player.advance(t);
    return player.game().summary();
}

void expect_same(const GameSummary& a, const GameSummary& b) {
    EXPECT_EQ(a.score, b.score);
    EXPECT_EQ(a.hits, b.hits);
    EXPECT_EQ(a.shots, b.shots);
    EXPECT_EQ(a.misses, b.misses);
    EXPECT_EQ(a.expired, b.expired);
    EXPECT_EQ(a.tracked, b.tracked);
    EXPECT_EQ(a.max_combo, b.max_combo);
    EXPECT_EQ(a.reactions_ms, b.reactions_ms);
    EXPECT_EQ(a.precision, b.precision);
    EXPECT_EQ(a.duration_ms, b.duration_ms);
    EXPECT_EQ(a.end, b.end);
}

class ReplayOfPreset : public ::testing::TestWithParam<int> {};

TEST_P(ReplayOfPreset, SameInputSameRun) {
    for (const Ending ending : {Ending::Lives, Ending::Time}) {
        const Played p = play(GetParam(), ending, 1500, 7U + static_cast<unsigned>(GetParam()));
        ASSERT_NE(p.replay, nullptr);
        EXPECT_GT(p.summary.shots + p.summary.tracked, 3) << "бот сыграл";
        EXPECT_EQ(p.replay->run.summary.seed, p.summary.seed);
        // Другое начало на часах — тот же замер: логика считает разности.
        expect_same(replay_summary(p.replay, ms(77'777)), p.summary);
        // И после файла.
        const std::optional<Replay> read = read_replay(write_replay(*p.replay));
        ASSERT_TRUE(read.has_value());
        expect_same(replay_summary(std::make_shared<const Replay>(read.value_or(Replay{})), kT0),
                    p.summary);
    }
}

INSTANTIATE_TEST_SUITE_P(AllPresets, ReplayOfPreset, ::testing::Range(0, kPresetCount));

TEST(ReplayFile, RoundTrip) {
    Replay r;
    r.run.when = {.year = 2026, .month = 10, .day = 16, .hour = 18, .minute = 5, .second = 9};
    r.run.summary.options = preset_options(3, {});
    r.run.summary.seed = 18'446'744'073'709'551'000ULL;
    r.run.summary.score = 42;
    r.steps = {
        {.kind = ReplayStepKind::Shown, .at = 0, .time = 4'000'123, .point = {}},
        {.kind = ReplayStepKind::Click,
         .at = 6'944'444,
         .time = -1'500'000,
         .point = {123.456789012345, 0.1},
         .button = MouseButton::Right},
        {.kind = ReplayStepKind::Update, .at = 6'944'444, .time = 6'944'444, .point = {1.0 / 3, 2}},
        {.kind = ReplayStepKind::Finish,
         .at = 13'888'888,
         .time = 13'000'000,
         .point = {},
         .button = MouseButton::Left,
         .reason = EndReason::GaveUp},
    };
    const std::string text = write_replay(r, "\r\n");
    EXPECT_TRUE(text.starts_with(std::string(kReplayVersionLine) + "\r\n"));
    const Replay back = read_replay(text).value_or(Replay{});
    EXPECT_EQ(back.steps, r.steps) << "вещественные — точно, отрицательное время — как было";
    EXPECT_EQ(run_line(back.run), run_line(r.run));
}

TEST(ReplayFile, BrokenLineRejectsReplay) {
    Replay r;
    r.run.summary.options = preset_options(1, {});
    r.steps = {{.kind = ReplayStepKind::Update, .at = 1, .time = 1, .point = {2, 3}}};
    const std::string text = write_replay(r);
    ASSERT_TRUE(read_replay(text));
    EXPECT_FALSE(read_replay(text + "u,5,x,1\n")) << "с пропуском шаг замер не повторит";
    EXPECT_FALSE(read_replay(text + "c,5,4,left,1\n"));
    EXPECT_FALSE(read_replay(text + "e,5,4,nope\n"));
    EXPECT_FALSE(read_replay("# DP AIM Trainer replay v2\n"));
    EXPECT_FALSE(read_replay(""));
}

TEST(ReplayFile, NamesByProfileAndCombo) {
    EXPECT_EQ(replay_file_name(1, preset_options(0, {})), "profile1_screen_time=60.replay");
    EXPECT_EQ(replay_file_name(2, preset_options(1, {})),
              "profile2_target_click_left_still_grow_bombs_size=40_life=1000_lives=15.replay");
    EXPECT_TRUE(is_replay_file("profile3_screen_time=60.replay"));
    EXPECT_FALSE(is_replay_file("profile_screen.replay"));
    EXPECT_FALSE(is_replay_file("profileX_screen.replay"));
    EXPECT_FALSE(is_replay_file("profile1_runs.csv"));
}

TEST(ReplayScreen, PlaysWithoutRecordingAndEscGoesBack) {
    const Played p = play(1, Ending::Lives, 600, 3U);
    ASSERT_NE(p.replay, nullptr);
    Profile profile;
    testing::ScriptedRandom seeds;
    PresetMenu menu(profile, 1);
    Flow flow(profile, seeds, menu);
    flow.start_replay(p.replay, kT0);
    ASSERT_EQ(flow.screen(), Screen::Replay);
    ASSERT_NE(flow.replay(), nullptr);
    ASSERT_NE(flow.game(), nullptr);
    // Щелчки реплей не читает; события замера — эффектам.
    const std::vector<Input> click{ClickInput{.position = {400, 300}, .time = kT0 + ms(5)}};
    flow.input(click, kT0 + ms(5));
    int game_events = 0;
    for (Nanoseconds t = kT0; !flow.replay()->done(); t += kFrame) {
        flow.update(t, {});
        for (const FlowEvent& e : flow.take_events()) {
            game_events += std::holds_alternative<GameEvent>(e) ? 1 : 0;
            EXPECT_FALSE(std::holds_alternative<RunFinished>(e));
            EXPECT_FALSE(std::holds_alternative<AchievementUnlocked>(e));
        }
    }
    EXPECT_GT(game_events, 0);
    expect_same(flow.game()->summary(), p.summary);
    EXPECT_TRUE(profile.runs.empty()) << "реплей в историю не пишется";
    const std::vector<Input> esc{KeyInput{.key = Key::Escape, .time = kT0 + ms(100'000)}};
    flow.input(esc, kT0 + ms(100'000));
    EXPECT_EQ(flow.screen(), Screen::Menu);
    EXPECT_EQ(flow.replay(), nullptr);
}

TEST(ReplayScreen, OnlyFromMenu) {
    const Played p = play(0, Ending::Time, 300, 5U);
    Profile profile;
    testing::ScriptedRandom seeds;
    PresetMenu menu(profile, 1);
    Flow flow(profile, seeds, menu);
    const std::vector<Input> start{ClickInput{.position = {500, 50}, .time = kT0}};
    flow.input(start, kT0);
    ASSERT_EQ(flow.screen(), Screen::Countdown);
    flow.start_replay(p.replay, kT0 + ms(10));
    EXPECT_EQ(flow.screen(), Screen::Countdown);
}

TEST(ReplayRecording, OffInSettingsMeansNoReplay) {
    Profile profile;
    profile.settings.replays = false;  // D-45
    testing::ScriptedRandom seeds;
    PresetMenu menu(profile, 1);
    Flow flow(profile, seeds, menu);
    const std::vector<Input> start{ClickInput{.position = {500, 50}, .time = kT0}};
    flow.input(start, kT0);
    flow.update(kT0 + kCountdown, {});
    flow.update(kT0 + kCountdown + kFrame, {});
    const std::vector<Input> esc{KeyInput{.key = Key::Escape, .time = kT0 + kCountdown + ms(10)}};
    flow.input(esc, kT0 + kCountdown + ms(10));
    int finished = 0;
    for (const FlowEvent& e : flow.take_events())
        if (const auto* run = std::get_if<RunFinished>(&e)) {
            ++finished;
            EXPECT_EQ(run->replay, nullptr);
        }
    EXPECT_EQ(finished, 1) << "замер записан в историю, реплея нет";
}

TEST(ReplayRecording, RestartDropsRecording) {
    Profile profile;
    testing::ScriptedRandom seeds;
    PresetMenu menu(profile, 1);
    Flow flow(profile, seeds, menu);
    const std::vector<Input> start{ClickInput{.position = {500, 50}, .time = kT0}};
    flow.input(start, kT0);
    flow.update(kT0 + kCountdown, {});
    flow.update(kT0 + kCountdown + kFrame, {});
    const std::vector<Input> restart{KeyInput{.key = Key::R, .time = kT0 + kCountdown + ms(20)}};
    flow.input(restart, kT0 + kCountdown + ms(20));
    const Nanoseconds second = kT0 + kCountdown + ms(20) + kCountdown;
    flow.update(second, {});
    flow.update(second + kFrame, {40, 50});
    const std::vector<Input> esc{KeyInput{.key = Key::Escape, .time = second + ms(10)}};
    flow.input(esc, second + ms(10));
    std::shared_ptr<const Replay> replay;
    for (const FlowEvent& e : flow.take_events())
        if (const auto* run = std::get_if<RunFinished>(&e)) replay = run->replay;
    ASSERT_NE(replay, nullptr);
    ASSERT_EQ(replay->steps.size(), 2U) << "только шаги второго замера";
    EXPECT_EQ(replay->steps[0].kind, ReplayStepKind::Update);
    EXPECT_EQ(replay->steps[0].at, static_cast<std::int64_t>(kFrame));
    EXPECT_EQ(replay->steps[0].point.x, 40);
    EXPECT_EQ(replay->steps[1].kind, ReplayStepKind::Finish);
    EXPECT_EQ(replay->steps[1].reason, EndReason::GaveUp);
}

}  // namespace
}  // namespace aim::core
