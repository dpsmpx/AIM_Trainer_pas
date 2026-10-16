#include "core/profile_file.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <optional>
#include <string>

#include "core/text.h"

namespace aim::core {

std::string profile_file_name(int profile) {
    return std::format("profile{}.ini", profile);
}

std::string runs_file_name(int profile) {
    return std::format("profile{}_runs.csv", profile);
}

std::string corrupt_backup_file_name(int profile) {
    return std::format("profile{}_corrupt_backup.txt", profile);
}

std::string write_profile(const Profile& p, std::string_view newline) {
    std::string out;
    const auto line = [&](std::string_view text) {
        out += text;
        out += newline;
    };
    const Settings& s = p.settings;
    line(kProfileVersionLine);
    line(std::format("tutorial_seen={}", p.tutorial_seen ? 1 : 0));
    line(std::format("preset={}", p.preset));
    line(std::format("target_size={}", s.preset.target_size));
    line(std::format("lifetime_ms={}", s.preset.lifetime_ms));
    line(std::format("lives={}", s.preset.lives));
    line(std::format("ending={}", ending_word(s.preset.ending)));
    line(std::format("average_runs={}", s.average_runs));
    line(std::format("sound={}", s.sound ? 1 : 0));
    line(std::format("menu_effects={}", s.menu_effects ? 1 : 0));
    line(std::format("theme={}", s.theme));
    line(std::format("hud={}", s.hud ? 1 : 0));
    line(std::format("replays={}", s.replays ? 1 : 0));
    // «Свой набор» (D-37): все опции словами, как в истории.
    const MeasureOptions& c = p.custom;
    line(std::format("custom.stimulus={}", stimulus_word(c.stimulus)));
    line(std::format("custom.action={}", action_word(c.action)));
    line(std::format("custom.buttons={}", c.color_buttons ? "color" : "left"));
    line(std::format("custom.moving={}", c.moving ? 1 : 0));
    line(std::format("custom.appear={}", c.grows ? "grow" : "full"));
    line(std::format("custom.bombs={}", c.bombs ? 1 : 0));
    line(std::format("custom.size={}", c.target_size));
    line(std::format("custom.lifetime_ms={}", c.lifetime_ms));
    line(std::format("custom.ending={}", ending_word(c.ending)));
    line(std::format("custom.lives={}", c.lives));
    line(std::format("custom.duration_s={}", c.duration_s));
    for (std::size_t i = 0; i < kAchievements.size(); ++i)
        line(std::format("achievement.{}={}", kAchievements.at(i).key,
                         p.achievements.at(i) ? 1 : 0));
    return out;
}

namespace {

// Значение ключа в профиль; false — значение вне смысла: поле остаётся по
// умолчанию.
bool apply(std::string_view key, std::string_view value, Profile& p) {
    Settings& s = p.settings;
    const auto as_int = [&](int& out) {
        const std::optional<int> v = parse_int(value);
        if (v) out = *v;
        return v.has_value();
    };
    const auto as_bool = [&](bool& out) {
        if (value != "0" && value != "1") return false;
        out = value == "1";
        return true;
    };
    // Слово из двух: yes — true, no — false.
    const auto as_word = [&](bool& out, std::string_view yes, std::string_view no) {
        if (value != yes && value != no) return false;
        out = value == yes;
        return true;
    };
    if (key == "tutorial_seen") return as_bool(p.tutorial_seen);
    if (key == "preset") return as_int(p.preset);
    if (key == "target_size") return as_int(s.preset.target_size);
    if (key == "lifetime_ms") return as_int(s.preset.lifetime_ms);
    if (key == "lives") return as_int(s.preset.lives);
    if (key == "ending") {
        const std::optional<Ending> e = parse_ending(value);
        if (e) s.preset.ending = *e;
        return e.has_value();
    }
    if (key == "average_runs") return as_int(s.average_runs);
    if (key == "sound") return as_bool(s.sound);
    if (key == "menu_effects") return as_bool(s.menu_effects);
    if (key == "theme") return as_int(s.theme);
    if (key == "hud") return as_bool(s.hud);
    if (key == "replays") return as_bool(s.replays);
    MeasureOptions& c = p.custom;
    if (key == "custom.stimulus") {
        const std::optional<Stimulus> v = parse_stimulus(value);
        if (v) c.stimulus = *v;
        return v.has_value();
    }
    if (key == "custom.action") {
        const std::optional<Action> v = parse_action(value);
        if (v) c.action = *v;
        return v.has_value();
    }
    if (key == "custom.buttons") return as_word(c.color_buttons, "color", "left");
    if (key == "custom.moving") return as_bool(c.moving);
    if (key == "custom.appear") return as_word(c.grows, "grow", "full");
    if (key == "custom.bombs") return as_bool(c.bombs);
    if (key == "custom.size") return as_int(c.target_size);
    if (key == "custom.lifetime_ms") return as_int(c.lifetime_ms);
    if (key == "custom.ending") {
        const std::optional<Ending> v = parse_ending(value);
        if (v) c.ending = *v;
        return v.has_value();
    }
    if (key == "custom.lives") return as_int(c.lives);
    if (key == "custom.duration_s") return as_int(c.duration_s);
    if (key.starts_with("achievement.")) {
        const std::string_view name = key.substr(12);
        for (std::size_t i = 0; i < kAchievements.size(); ++i)
            if (kAchievements.at(i).key == name) return as_bool(p.achievements.at(i));
    }
    return true;  // неизвестный ключ — пропустить
}

}  // namespace

LoadedProfile read_profile(std::string_view text) {
    LoadedProfile loaded;
    const std::vector<std::string_view> lines = split_lines(text);
    if (lines.empty() || lines.front() != kProfileVersionLine) {
        loaded.corrupt = true;
        return loaded;
    }
    Profile& p = loaded.profile;
    for (std::size_t i = 1; i < lines.size(); ++i) {
        const std::string_view raw = lines[i];
        if (raw.empty() || raw.starts_with('#')) continue;
        const std::size_t eq = raw.find('=');
        if (eq == std::string_view::npos) continue;
        const std::string_view key = raw.substr(0, eq);
        if (!apply(key, raw.substr(eq + 1), p)) loaded.reset_fields.emplace_back(key);
    }
    const Profile defaults;
    if (p.preset < 0 || p.preset >= kMenuItems) {
        p.preset = defaults.preset;
        loaded.reset_fields.emplace_back("preset");
    }
    if (reset_out_of_range(p.settings) > 0) loaded.reset_fields.emplace_back("settings");
    if (reset_out_of_range(p.custom) > 0) loaded.reset_fields.emplace_back("custom");
    return loaded;
}

std::string runs_file_header(std::string_view newline) {
    return std::format("{}{}{}{}", kRunsVersionLine, newline, kRunsHeader, newline);
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
    if (numbered(name, "profile", ".ini") || numbered(name, "profile", "_runs.csv") ||
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
