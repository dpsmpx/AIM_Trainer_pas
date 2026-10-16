#include "core/history.h"

#include <format>
#include <optional>

#include "core/modes.h"
#include "core/text.h"

namespace aim::core {

std::string session_line(const GameSummary& s, const LocalTime& when) {
    // Format('{0:0.0}') образца округляет половину от нуля, std::format — по
    // точному двоичному значению; разница — только на точной половине.
    return std::format("{}-{:02}-{:02},{:02}:{:02},{},{},{},{:.1f},{:.1f},{},{},{}", when.year,
                       when.month, when.day, when.hour, when.minute, when.hour,
                       mode_file_name(s.mode), s.score, s.accuracy, s.avg_reaction_ms, s.hits,
                       s.misses, s.max_combo);
}

std::string export_file_name(int profile, const LocalTime& when) {
    return std::format("export_profile{}_{}{:02}{:02}.csv", profile, when.year, when.month,
                       when.day);
}

std::vector<SessionRow> read_sessions(std::string_view csv, std::size_t keep_last) {
    std::vector<SessionRow> rows;
    bool header = true;
    for (const std::string_view line : split_lines(csv)) {
        if (header) {  // первая строка — заголовок, как у образца
            header = false;
            continue;
        }
        const std::vector<std::string_view> f = split(line, ',');
        if (f.size() < 10) continue;
        const std::optional<int> hour = parse_int(f[2]);
        const std::optional<int> score = parse_int(f[4]);
        const std::optional<double> reaction = parse_real(f[6]);
        if (!hour || !score || !reaction) continue;
        rows.push_back({.hour = *hour, .score = *score, .avg_reaction = *reaction});
    }
    if (rows.size() > keep_last)
        rows.erase(rows.begin(), rows.end() - static_cast<std::ptrdiff_t>(keep_last));
    return rows;
}

int time_of_day(int hour) {
    if (hour >= 6 && hour < 12) return 0;
    if (hour >= 12 && hour < 18) return 1;
    if (hour >= 18 && hour < 23) return 2;
    return 3;
}

std::array<TimeOfDayScore, 4> scores_by_time_of_day(const std::vector<SessionRow>& rows) {
    std::array<TimeOfDayScore, 4> out{};
    std::array<double, 4> sum{};
    for (const SessionRow& r : rows) {
        const auto bucket = static_cast<std::size_t>(time_of_day(r.hour));
        sum[bucket] += r.score;
        ++out[bucket].games;
    }
    for (std::size_t i = 0; i < out.size(); ++i)
        if (out[i].games > 0) out[i].average_score = sum[i] / out[i].games;
    return out;
}

std::vector<double> recent_avg_reactions(const std::vector<SessionRow>& rows, std::size_t count) {
    const std::size_t first = rows.size() > count ? rows.size() - count : 0;
    std::vector<double> out;
    out.reserve(rows.size() - first);
    for (std::size_t i = first; i < rows.size(); ++i)
        out.push_back(rows[i].avg_reaction);
    return out;
}

}  // namespace aim::core
