#include "core/runs.h"

#include <charconv>
#include <format>
#include <system_error>

#include "core/text.h"

namespace aim::core {

namespace {

constexpr std::size_t kRunFields = 27;

std::optional<bool> parse_flag(std::string_view word, std::string_view yes, std::string_view no) {
    if (word == yes) return true;
    if (word == no) return false;
    return std::nullopt;
}

std::optional<std::uint64_t> parse_u64(std::string_view text) {
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) return std::nullopt;
    return value;
}

// Дата ГГГГ-ММ-ДД и время ЧЧ:ММ:СС.
std::optional<LocalTime> parse_when(std::string_view date, std::string_view time) {
    const std::vector<std::string_view> d = split(date, '-');
    const std::vector<std::string_view> t = split(time, ':');
    if (d.size() != 3 || t.size() != 3) return std::nullopt;
    const auto year = parse_int(d[0]);
    const auto month = parse_int(d[1]);
    const auto day = parse_int(d[2]);
    const auto hour = parse_int(t[0]);
    const auto minute = parse_int(t[1]);
    const auto second = parse_int(t[2]);
    if (!year || !month || !day || !hour || !minute || !second) return std::nullopt;
    if (*month < 1 || *month > 12 || *day < 1 || *day > 31 || *hour < 0 || *hour > 23 ||
        *minute < 0 || *minute > 59 || *second < 0 || *second > 60)
        return std::nullopt;
    return LocalTime{.year = *year,
                     .month = *month,
                     .day = *day,
                     .hour = *hour,
                     .minute = *minute,
                     .second = *second};
}

}  // namespace

std::string_view end_word(EndReason end) {
    switch (end) {
        case EndReason::None: return "none";
        case EndReason::Lives: return "lives";
        case EndReason::Time: return "time";
        case EndReason::Bomb: return "bomb";
        case EndReason::GaveUp: return "esc";
        case EndReason::ToMenu: return "menu";
    }
    return "none";
}

std::optional<EndReason> parse_end(std::string_view word) {
    for (const EndReason e : {EndReason::None, EndReason::Lives, EndReason::Time, EndReason::Bomb,
                              EndReason::GaveUp, EndReason::ToMenu})
        if (end_word(e) == word) return e;
    return std::nullopt;
}

std::string run_line(const RunRecord& run) {
    const GameSummary& s = run.summary;
    const MeasureOptions o = normalized(s.options);
    const LocalTime& w = run.when;
    return std::format(
        "{:04}-{:02}-{:02},{:02}:{:02}:{:02},{},{},{},{},{},{},{},{},{},{},{},{},"
        "{},{},{},{},{},{},{},{:.2f},{:.2f},{:.2f},{:.2f},{},{}",
        w.year, w.month, w.day, w.hour, w.minute, w.second, s.seed, stimulus_word(o.stimulus),
        action_word(o.action), o.color_buttons ? "color" : "left", o.moving ? 1 : 0,
        o.grows ? "grow" : "full", o.bombs ? 1 : 0, o.target_size, o.lifetime_ms,
        ending_word(o.ending), o.lives, o.duration_s, s.score, s.hits, s.shots, s.misses, s.expired,
        s.tracked, s.max_combo, s.accuracy, s.precision, s.avg_reaction_ms, s.median_reaction_ms,
        s.duration_ms, end_word(s.end));
}

std::optional<RunRecord> parse_run_line(std::string_view line) {
    const std::vector<std::string_view> f = split(line, ',');
    if (f.size() != kRunFields) return std::nullopt;
    RunRecord run;
    const auto when = parse_when(f[0], f[1]);
    const auto seed = parse_u64(f[2]);
    const auto stimulus = parse_stimulus(f[3]);
    const auto action = parse_action(f[4]);
    const auto buttons = parse_flag(f[5], "color", "left");
    const auto moving = parse_flag(f[6], "1", "0");
    const auto grows = parse_flag(f[7], "grow", "full");
    const auto bombs = parse_flag(f[8], "1", "0");
    const auto size = parse_int(f[9]);
    const auto lifetime = parse_int(f[10]);
    const auto ending = parse_ending(f[11]);
    const auto lives = parse_int(f[12]);
    const auto duration = parse_int(f[13]);
    if (!when || !seed || !stimulus || !action || !buttons || !moving || !grows || !bombs ||
        !size || !lifetime || !ending || !lives || !duration)
        return std::nullopt;
    const auto score = parse_int(f[14]);
    const auto hits = parse_int(f[15]);
    const auto shots = parse_int(f[16]);
    const auto misses = parse_int(f[17]);
    const auto expired = parse_int(f[18]);
    const auto tracked = parse_int(f[19]);
    const auto max_combo = parse_int(f[20]);
    const auto accuracy = parse_real(f[21]);
    const auto precision = parse_real(f[22]);
    const auto avg = parse_real(f[23]);
    const auto med = parse_real(f[24]);
    const auto run_ms = parse_int(f[25]);
    const auto end = parse_end(f[26]);
    if (!score || !hits || !shots || !misses || !expired || !tracked || !max_combo || !accuracy ||
        !precision || !avg || !med || !run_ms || !end)
        return std::nullopt;
    // Меры вне смысла — испорченная строка, а не замер (AUD-50).
    if (*score < 0 || *hits < 0 || *shots < 0 || *misses < 0 || *expired < 0 || *tracked < 0 ||
        *max_combo < 0 || *run_ms < 0 || *accuracy < 0 || *accuracy > 100 || *precision < 0 ||
        *precision > 100 || *avg < 0 || *avg > 1e6 || *med < 0 || *med > 1e6)
        return std::nullopt;
    run.when = *when;
    GameSummary& s = run.summary;
    s.seed = *seed;
    const MeasureOptions written{.stimulus = *stimulus,
                                 .action = *action,
                                 .color_buttons = *buttons,
                                 .moving = *moving,
                                 .grows = *grows,
                                 .bombs = *bombs,
                                 .target_size = *size,
                                 .lifetime_ms = *lifetime,
                                 .ending = *ending,
                                 .lives = *lives,
                                 .duration_s = *duration};
    // Опции вне границ конструктора — испорченная строка: зажатые в
    // границы, они попали бы в рекорды чужого набора (AUD-50).
    if (MeasureOptions bounds = written; reset_out_of_range(bounds) > 0) return std::nullopt;
    s.options = normalized(written);
    s.score = *score;
    s.hits = *hits;
    s.shots = *shots;
    s.misses = *misses;
    s.expired = *expired;
    s.tracked = *tracked;
    s.max_combo = *max_combo;
    s.accuracy = *accuracy;
    s.precision = *precision;
    s.avg_reaction_ms = *avg;
    s.median_reaction_ms = *med;
    s.duration_ms = *run_ms;
    s.end = *end;
    return run;
}

std::vector<RunRecord> read_runs(std::string_view csv, std::size_t* skipped) {
    std::vector<RunRecord> runs;
    std::size_t bad = 0;
    for (const std::string_view line : split_lines(csv)) {
        if (line.empty() || line.starts_with('#') || line == kRunsHeader) continue;
        if (auto run = parse_run_line(line)) {
            runs.push_back(std::move(*run));
        } else {
            ++bad;
        }
    }
    if (skipped != nullptr) *skipped = bad;
    return runs;
}

std::string export_file_name(int profile, const LocalTime& when) {
    return std::format("export_profile{}_{}{:02}{:02}.csv", profile, when.year, when.month,
                       when.day);
}

}  // namespace aim::core
