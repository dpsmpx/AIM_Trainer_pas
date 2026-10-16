#include "core/profile_file.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <system_error>

#include "core/text.h"

namespace aim::core {

namespace {

constexpr std::string_view kTrue = "True";
constexpr std::string_view kFalse = "False";

std::string_view boolean(bool value) {
    return value ? kTrue : kFalse;
}

// Значения, разобранные из файла, — до проверки диапазонов.
struct RawLeaderboard {
    std::array<int, kModeCount> count{};
    std::array<std::array<LeaderboardEntry, kLeaderboardSize>, kModeCount> rows{};
};

// Строка «ключ=значение» в профиль. false — значение известного ключа не
// число: файл повреждён (у образца — исключение StrToInt или StrToFloat).
bool apply(std::string_view key, std::string_view value, Profile& p, RawLeaderboard& board) {
    const auto as_int = [&](int& out) {
        const std::optional<int> v = parse_int(value);
        if (v) out = *v;
        return v.has_value();
    };
    const auto as_real = [&](double& out) {
        const std::optional<double> v = parse_real(value);
        if (v) out = *v;
        return v.has_value();
    };
    if (key == "BestScore") return as_int(p.best_score);
    if (key == "BestAccuracy") return as_real(p.best_accuracy);
    if (key == "BestPrecision") return as_real(p.best_precision);
    if (key == "BestAvgReaction") return as_real(p.best_avg_reaction);
    if (key == "GamesPlayed") return as_int(p.games_played);
    if (key == "TutorialSeen") {
        p.tutorial_seen = value == kTrue;
        return true;
    }
    if (key == "ReactionCount") return as_int(p.reactions.count);
    if (key == "ReactionIndex") return as_int(p.reactions.next);
    if (key == "ShowMainMenuEffects") {
        p.settings.menu_effects = value == kTrue;
        return true;
    }
    if (key == "SetCircleSize") return as_int(p.settings.circle_size);
    if (key == "SetLifetimeMs") return as_int(p.settings.lifetime_ms);
    if (key == "SetLives") return as_int(p.settings.lives);
    if (key == "SetSoundEnabled") {
        p.settings.sound = value == kTrue;
        return true;
    }
    if (key == "CurrentTheme") return as_int(p.settings.theme);
    if (key.starts_with("LBCount")) {
        const std::optional<int> mode = parse_int(key.substr(7));
        int count = 0;
        if (!mode || !as_int(count)) return false;
        if (*mode >= 0 && *mode < kModeCount) board.count[static_cast<std::size_t>(*mode)] = count;
        return true;
    }
    if (key.size() == 5 && key.starts_with("LB")) {
        const std::optional<int> mode = parse_int(key.substr(2, 1));
        const std::optional<int> rank = parse_int(key.substr(3, 1));
        if (!mode || !rank) return false;
        if (*mode < 0 || *mode >= kModeCount || *rank < 0 || *rank >= kLeaderboardSize) return true;
        LeaderboardEntry& e =
            board.rows[static_cast<std::size_t>(*mode)][static_cast<std::size_t>(*rank)];
        if (key[4] == 'S') return as_int(e.score);
        if (key[4] == 'A') return as_real(e.accuracy);
        return true;
    }
    if (key.starts_with("Achv")) {
        const std::optional<int> id = parse_int(key.substr(4));
        if (!id) return false;
        if (*id >= 0 && *id < kAchievementCount)
            p.achievements[static_cast<std::size_t>(*id)] = value == kTrue;
        return true;
    }
    if (key.starts_with("Reaction")) {
        const std::optional<int> index = parse_int(key.substr(8));
        int ms = 0;
        if (!index || !as_int(ms)) return false;
        if (*index >= 0 && *index < kReactionHistory)
            p.reactions.values[static_cast<std::size_t>(*index)] = ms;
        return true;
    }
    return true;  // неизвестный ключ
}

// Проверка диапазонов (AUD-07): что вне — сбросить, остальное оставить.
void validate(Profile& p, const RawLeaderboard& board, std::vector<std::string>& reset) {
    ReactionRing& ring = p.reactions;
    if (ring.count < 0 || ring.count > kReactionHistory || ring.next < 0 ||
        ring.next >= kReactionHistory) {
        ring = ReactionRing{};
        reset.emplace_back("ReactionHistory");
    }
    for (int m = 0; m < kModeCount; ++m) {
        const auto mi = static_cast<std::size_t>(m);
        const int count = board.count[mi];
        auto& rows = p.leaderboard[mi];
        rows.clear();
        if (count < 0 || count > kLeaderboardSize) {
            reset.push_back(std::format("LBCount{}", m));
            continue;
        }
        rows.assign(board.rows[mi].begin(), board.rows[mi].begin() + count);
    }
    const Settings before = p.settings;
    reset_out_of_range(p.settings);
    if (p.settings.circle_size != before.circle_size) reset.emplace_back("SetCircleSize");
    if (p.settings.lifetime_ms != before.lifetime_ms) reset.emplace_back("SetLifetimeMs");
    if (p.settings.lives != before.lives) reset.emplace_back("SetLives");
    if (p.settings.theme != before.theme) reset.emplace_back("CurrentTheme");
}

}  // namespace

std::string stats_file_name(int profile) {
    return std::format("profile{}_stats.ini", profile);
}

std::string sessions_file_name(int profile) {
    return std::format("profile{}_sessions.csv", profile);
}

std::string corrupt_backup_file_name(int profile) {
    return std::format("profile{}_corrupt_backup.txt", profile);
}

std::string format_real(double value) {
    // Как Double.ToString() .NET Framework: формат G с 15 значащими цифрами;
    // ноль — «0» и без знака.
    if (!std::isfinite(value) || value == 0.0) return "0";
    return std::format("{:.15g}", value);
}

std::string write_stats(const Profile& p, std::string_view newline) {
    std::string out;
    const auto line = [&](std::string_view text) {
        out += text;
        out += newline;
    };
    line("[Stats]");
    line(std::format("BestScore={}", p.best_score));
    line(std::format("BestAccuracy={}", format_real(p.best_accuracy)));
    line(std::format("BestPrecision={}", format_real(p.best_precision)));
    line(std::format("BestAvgReaction={}", format_real(p.best_avg_reaction)));
    line(std::format("GamesPlayed={}", p.games_played));
    line(std::format("TutorialSeen={}", boolean(p.tutorial_seen)));
    line("[Settings]");
    line(std::format("ShowMainMenuEffects={}", boolean(p.settings.menu_effects)));
    line(std::format("SetCircleSize={}", p.settings.circle_size));
    line(std::format("SetLifetimeMs={}", p.settings.lifetime_ms));
    line(std::format("SetLives={}", p.settings.lives));
    line(std::format("SetSoundEnabled={}", boolean(p.settings.sound)));
    line(std::format("CurrentTheme={}", p.settings.theme));
    line("[ReactionHistory]");
    line(std::format("ReactionCount={}", p.reactions.count));
    line(std::format("ReactionIndex={}", p.reactions.next));
    for (int i = 0; i < kReactionHistory; ++i)
        line(std::format("Reaction{}={}", i, p.reactions.values[static_cast<std::size_t>(i)]));
    line("[Achievements]");
    for (int i = 0; i < kAchievementCount; ++i)
        line(std::format("Achv{}={}", i, boolean(p.achievements[static_cast<std::size_t>(i)])));
    line("[Leaderboard]");
    for (int m = 0; m < kModeCount; ++m) {
        const auto& rows = p.leaderboard[static_cast<std::size_t>(m)];
        line(std::format("LBCount{}={}", m, rows.size()));
        for (std::size_t rank = 0; rank < rows.size(); ++rank) {
            line(std::format("LB{}{}S={}", m, rank, rows[rank].score));
            line(std::format("LB{}{}A={}", m, rank, format_real(rows[rank].accuracy)));
        }
    }
    return out;
}

LoadedProfile read_stats(std::string_view text) {
    LoadedProfile loaded;
    RawLeaderboard board;
    for (const std::string_view raw : split_lines(text)) {
        const std::size_t eq = raw.find('=');
        // У образца строка разбирается, только если «=» в ней ровно один.
        if (eq == std::string_view::npos || raw.find('=', eq + 1) != std::string_view::npos)
            continue;
        if (!apply(raw.substr(0, eq), raw.substr(eq + 1), loaded.profile, board)) {
            loaded = LoadedProfile{};
            loaded.corrupt = true;
            return loaded;
        }
    }
    validate(loaded.profile, board, loaded.reset_fields);
    return loaded;
}

std::string app_state_file_name() {
    return "aim_trainer.ini";
}

std::string write_app_state(const AppState& state, std::string_view newline) {
    std::string out = std::format("[App]{}LastProfile={}{}", newline, state.last_profile, newline);
    if (!state.data_dir.empty()) out += std::format("DataDir={}{}", state.data_dir, newline);
    return out;
}

AppState read_app_state(std::string_view text) {
    AppState state;
    for (const std::string_view raw : split_lines(text)) {
        if (raw.starts_with("LastProfile=")) {
            const std::optional<int> n = parse_int(raw.substr(12));
            if (n && *n >= 1 && *n <= kProfileCount) state.last_profile = *n;
        } else if (raw.starts_with("DataDir=")) {
            state.data_dir = std::string(raw.substr(8));
        }
    }
    return state;
}

namespace {

// prefix, затем цифры, затем suffix.
bool numbered(std::string_view name, std::string_view prefix, std::string_view suffix) {
    if (name.size() <= prefix.size() + suffix.size() || !name.starts_with(prefix) ||
        !name.ends_with(suffix))
        return false;
    const std::string_view digits =
        name.substr(prefix.size(), name.size() - prefix.size() - suffix.size());
    return std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; });
}

}  // namespace

bool is_player_file(std::string_view name) {
    if (numbered(name, "profile", "_stats.ini") || numbered(name, "profile", "_sessions.csv") ||
        numbered(name, "profile", "_corrupt_backup.txt"))
        return true;
    // export_profileN_ГГГГММДД.csv
    if (name.size() <= 18 || !name.starts_with("export_profile") || !name.ends_with(".csv"))
        return false;
    const std::string_view middle = name.substr(14, name.size() - 14 - 4);
    const std::size_t sep = middle.find('_');
    if (sep == std::string_view::npos || sep == 0 || middle.size() - sep - 1 != 8) return false;
    return std::all_of(middle.begin(), middle.end(),
                       [](char c) { return c == '_' || (c >= '0' && c <= '9'); }) &&
           middle.find('_', sep + 1) == std::string_view::npos;
}

std::string backup_folder_name(const LocalTime& when, int attempt) {
    std::string name = std::format("backup_{:04}{:02}{:02}_{:02}{:02}", when.year, when.month,
                                   when.day, when.hour, when.minute);
    if (attempt > 1) name += std::format("_{}", attempt);
    return name;
}

}  // namespace aim::core
