#include "core/records.h"

#include <algorithm>
#include <map>

#include "core/rules.h"

namespace aim::core {

bool lower_is_better(const MeasureOptions& options) {
    return options.stimulus == Stimulus::Screen;
}

double primary_value(const GameSummary& summary) {
    return lower_is_better(summary.options) ? summary.median_reaction_ms
                                            : static_cast<double>(summary.score);
}

bool counts_for_record(const GameSummary& summary) {
    if (!lower_is_better(summary.options)) return summary.score > 0;
    return summary.shots >= kRecordMinShots && summary.hits > 0;
}

bool better(const GameSummary& a, const GameSummary& b) {
    const bool a_counts = counts_for_record(a);
    const bool b_counts = counts_for_record(b);
    if (a_counts != b_counts) return a_counts;
    if (!a_counts) return false;
    return lower_is_better(a.options) ? primary_value(a) < primary_value(b)
                                      : primary_value(a) > primary_value(b);
}

namespace {

Averages average_of(const std::vector<RunRecord>& runs, const std::vector<std::size_t>& picked) {
    Averages avg;
    int with_hits = 0;
    for (const std::size_t i : picked) {
        const GameSummary& s = runs[i].summary;
        ++avg.runs;
        avg.score += s.score;
        avg.accuracy += s.accuracy;
        avg.precision += s.precision;
        avg.primary += primary_value(s);
        if (s.hits > 0) {
            ++with_hits;
            avg.avg_reaction_ms += s.avg_reaction_ms;
            avg.median_reaction_ms += s.median_reaction_ms;
        }
    }
    if (avg.runs > 0) {
        avg.score /= avg.runs;
        avg.accuracy /= avg.runs;
        avg.precision /= avg.runs;
        avg.primary /= avg.runs;
    }
    if (with_hits > 0) {
        avg.avg_reaction_ms /= with_hits;
        avg.median_reaction_ms /= with_hits;
    }
    return avg;
}

ComboStats stats_of(const std::vector<RunRecord>& runs, const std::vector<std::size_t>& of_combo,
                    const std::string& key, const MeasureOptions& options, int average_runs) {
    ComboStats stats;
    stats.key = key;
    stats.options = normalized(options);
    stats.runs = static_cast<int>(of_combo.size());
    for (const std::size_t i : of_combo) {
        if (!stats.best || better(runs[i].summary, runs[*stats.best].summary)) stats.best = i;
    }
    if (stats.best && !counts_for_record(runs[*stats.best].summary)) stats.best.reset();
    if (!of_combo.empty()) stats.last = of_combo.back();
    const auto n =
        static_cast<std::size_t>(std::clamp(average_runs, kMinAverageRuns, kMaxAverageRuns));
    const std::size_t from = of_combo.size() > n ? of_combo.size() - n : 0;
    stats.average =
        average_of(runs, {of_combo.begin() + static_cast<std::ptrdiff_t>(from), of_combo.end()});
    return stats;
}

}  // namespace

ComboStats combo_stats(const std::vector<RunRecord>& runs, const MeasureOptions& options,
                       int average_runs) {
    const std::string key = options_key(options);
    std::vector<std::size_t> of_combo;
    for (std::size_t i = 0; i < runs.size(); ++i)
        if (options_key(runs[i].summary.options) == key) of_combo.push_back(i);
    return stats_of(runs, of_combo, key, options, average_runs);
}

std::vector<ComboStats> all_combo_stats(const std::vector<RunRecord>& runs, int average_runs) {
    std::map<std::string, std::vector<std::size_t>> by_key;
    for (std::size_t i = 0; i < runs.size(); ++i)
        by_key[options_key(runs[i].summary.options)].push_back(i);
    std::vector<ComboStats> all;
    all.reserve(by_key.size());
    for (const auto& [key, of_combo] : by_key)
        all.push_back(
            stats_of(runs, of_combo, key, runs[of_combo.front()].summary.options, average_runs));
    std::sort(all.begin(), all.end(),
              [](const ComboStats& a, const ComboStats& b) { return a.last > b.last; });
    return all;
}

std::vector<double> recent_primary(const std::vector<RunRecord>& runs,
                                   const MeasureOptions& options, std::size_t count) {
    const std::string key = options_key(options);
    std::vector<double> out;
    for (auto it = runs.rbegin(); it != runs.rend() && out.size() < count; ++it)
        if (options_key(it->summary.options) == key) out.push_back(primary_value(it->summary));
    std::reverse(out.begin(), out.end());
    return out;
}

}  // namespace aim::core
