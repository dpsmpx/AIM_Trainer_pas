// Разбор файлов игрока на испорченном тексте (AUD-49): мутации верных
// файлов — профиля, последнего профиля, истории, реплея, темы — замена,
// вставка и удаление байтов, обрезка, повтор и перестановка строк — не
// роняют разбор и не дают неопределённого поведения (пресет linux-asan:
// ASan и UBSan). Что прочитано из испорченного файла, то пишется и читается
// снова тем же: в память не попадает ничего, что не переживёт записи.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "core/game.h"
#include "core/profile_file.h"
#include "core/replay.h"
#include "core/runs.h"
#include "core/theme.h"

namespace aim::core {
namespace {

constexpr int kRounds = 3000;

// Знаки, которыми портится файл: цифры, разделители форматов, знаки
// чисел, кириллица, переводы строк, ноль.
constexpr std::string_view kNoise = "0123456789-+.,;=#e\r\n \tx\xd0\xb0\0";

class Mutator {
public:
    explicit Mutator(std::uint32_t seed) : rng_(seed) {}

    std::string operator()(std::string text) {
        const int edits = pick(1, 6);
        for (int i = 0; i < edits; ++i)
            edit(text);
        return text;
    }

private:
    int pick(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng_); }
    std::size_t at(const std::string& s) {
        return s.empty() ? 0 : static_cast<std::size_t>(pick(0, static_cast<int>(s.size()) - 1));
    }
    char noise() { return kNoise[static_cast<std::size_t>(pick(0, kNoise.size() - 1))]; }

    void edit(std::string& s) {
        switch (pick(0, 6)) {
            case 0:
                if (!s.empty()) s[at(s)] = noise();
                break;
            case 1: s.insert(at(s), 1, noise()); break;
            case 2:
                if (!s.empty()) s.erase(at(s), static_cast<std::size_t>(pick(1, 8)));
                break;
            case 3: s.resize(at(s)); break;  // обрыв записи
            case 4: {                        // строка ещё раз
                const std::size_t from = s.rfind('\n', at(s));
                const std::size_t begin = from == std::string::npos ? 0 : from + 1;
                const std::size_t end = s.find('\n', begin);
                s.insert(begin, s.substr(begin, end == std::string::npos ? std::string::npos
                                                                         : end - begin + 1));
                break;
            }
            case 5: {  // огромное число
                s.insert(at(s), "99999999999999999999999");
                break;
            }
            default: {  // поменять местами два куска
                const std::size_t a = at(s);
                const std::size_t b = at(s);
                if (a < b) std::swap(s[a], s[b]);
                break;
            }
        }
    }

    std::mt19937 rng_;
};

RunRecord sample_run() {
    RunRecord r;
    r.when = {.year = 2026, .month = 10, .day = 17, .hour = 12, .minute = 30, .second = 5};
    r.summary.options = preset_options(2, PresetSettings{});
    r.summary.seed = 1234567890123ULL;
    r.summary.score = 321;
    r.summary.hits = 20;
    r.summary.shots = 24;
    r.summary.misses = 4;
    r.summary.expired = 3;
    r.summary.max_combo = 9;
    r.summary.accuracy = 83.3;
    r.summary.avg_reaction_ms = 412.5;
    r.summary.median_reaction_ms = 399;
    r.summary.precision = 71.2;
    r.summary.duration_ms = 30'000;
    r.summary.end = EndReason::Lives;
    return r;
}

TEST(ParseFuzz, Profile) {
    Profile p;
    p.settings.theme = "проба";
    p.achievements.at(1) = true;
    const std::string text = write_profile(p, "\r\n");
    Mutator mutate(1);
    for (int i = 0; i < kRounds; ++i) {
        const LoadedProfile loaded = read_profile(mutate(text));
        const LoadedProfile again = read_profile(write_profile(loaded.profile, "\n"));
        EXPECT_FALSE(again.corrupt) << i;
        EXPECT_TRUE(again.reset_fields.empty()) << i;
        EXPECT_EQ(write_profile(again.profile, "\n"), write_profile(loaded.profile, "\n")) << i;
    }
}

TEST(ParseFuzz, AppState) {
    const std::string text =
        write_app_state(AppState{.last_profile = 3, .data_dir = "D:/Игры/"}, "\n");
    Mutator mutate(2);
    for (int i = 0; i < kRounds; ++i) {
        const AppState state = read_app_state(mutate(text));
        const AppState again = read_app_state(write_app_state(state, "\n"));
        EXPECT_EQ(again.last_profile, state.last_profile) << i;
    }
}

TEST(ParseFuzz, Runs) {
    std::string csv = runs_file_header("\n");
    RunRecord r = sample_run();
    for (int i = 0; i < 5; ++i) {
        csv += run_line(r) + "\n";
        r.summary.score += 7;
        r.summary.options = preset_options(i, PresetSettings{});
    }
    Mutator mutate(3);
    for (int i = 0; i < kRounds; ++i) {
        std::size_t skipped = 0;
        const std::vector<RunRecord> runs = read_runs(mutate(csv), &skipped);
        for (const RunRecord& run : runs) {
            const std::optional<RunRecord> again = parse_run_line(run_line(run));
            ASSERT_TRUE(again.has_value()) << i << ": " << run_line(run);
            EXPECT_EQ(run_line(again.value_or(RunRecord{})), run_line(run)) << i;
        }
    }
}

TEST(ParseFuzz, Replay) {
    Replay replay;
    replay.run = sample_run();
    for (int i = 0; i < 40; ++i) {
        replay.steps.push_back({.kind = ReplayStepKind::Update,
                                .at = i * 7'000'000LL,
                                .time = i * 7'000'000LL,
                                .point = {100.5 + i, 200.25},
                                .button = MouseButton::Left,
                                .reason = EndReason::None});
        if (i % 5 == 0)
            replay.steps.push_back({.kind = ReplayStepKind::Click,
                                    .at = i * 7'000'000LL,
                                    .time = i * 7'000'000LL - 1'000,
                                    .point = {101, 202},
                                    .button = i % 10 == 0 ? MouseButton::Right : MouseButton::Left,
                                    .reason = EndReason::None});
    }
    replay.steps.push_back({.kind = ReplayStepKind::Finish,
                            .at = 280'000'000LL,
                            .time = 280'000'000LL,
                            .point = {},
                            .button = MouseButton::Left,
                            .reason = EndReason::GaveUp});
    const std::string text = write_replay(replay);
    ASSERT_TRUE(read_replay(text).has_value());
    Mutator mutate(4);
    for (int i = 0; i < kRounds; ++i) {
        const std::optional<Replay> read = read_replay(mutate(text));
        if (!read) continue;
        const std::optional<Replay> again = read_replay(write_replay(*read));
        ASSERT_TRUE(again.has_value()) << i;
        EXPECT_EQ(again.value_or(Replay{}).steps, read->steps) << i;
    }
}

TEST(ParseFuzz, Theme) {
    const std::string text = "base=neon\n" + write_theme(kThemes.back().theme);
    Mutator mutate(5);
    for (int i = 0; i < kRounds; ++i) {
        const ParsedTheme parsed = read_theme(mutate(text));
        const ParsedTheme again = read_theme(write_theme(parsed.theme));
        EXPECT_EQ(again.bad_lines, 0) << i;
        EXPECT_EQ(write_theme(again.theme), write_theme(parsed.theme)) << i;
    }
}

}  // namespace
}  // namespace aim::core
