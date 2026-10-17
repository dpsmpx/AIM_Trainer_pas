// История замеров profileN_runs.csv (D-38): все параметры и меры замера, в
// строке и обратно без потерь.

#include "core/runs.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/profile_file.h"

namespace aim::core {
namespace {

RunRecord sample() {
    RunRecord r;
    r.when = {.year = 2026, .month = 10, .day = 16, .hour = 9, .minute = 5, .second = 7};
    GameSummary& s = r.summary;
    s.options = preset_options(
        2, {.target_size = 36, .lifetime_ms = 1200, .lives = 9, .ending = Ending::Lives});
    s.seed = 1234567890123ULL;
    s.score = 345;
    s.hits = 30;
    s.shots = 33;
    s.misses = 3;
    s.expired = 4;
    s.tracked = 0;
    s.max_combo = 12;
    s.accuracy = 90.91;
    s.precision = 71.25;
    s.avg_reaction_ms = 412.5;
    s.median_reaction_ms = 398.75;
    s.duration_ms = 54321;
    s.end = EndReason::Lives;
    return r;
}

TEST(Runs, LineRoundTrip) {
    const RunRecord r = sample();
    const std::string line = run_line(r);
    EXPECT_EQ(line,
              "2026-10-16,09:05:07,1234567890123,target,click,color,0,grow,1,36,1200,lives,9,60,"
              "345,30,33,3,4,0,12,90.91,71.25,412.50,398.75,54321,lives");
    const auto parsed = parse_run_line(line);
    ASSERT_TRUE(parsed.has_value());
    const RunRecord back = parsed.value_or(RunRecord{});
    EXPECT_EQ(back.when, r.when);
    EXPECT_EQ(back.summary.options, r.summary.options);
    EXPECT_EQ(run_line(back), line);
}

TEST(Runs, ScreenRunKeepsOnlyItsOptions) {
    RunRecord r = sample();
    r.summary.options = preset_options(0, {});
    r.summary.end = EndReason::Time;
    const auto parsed = parse_run_line(run_line(r));
    ASSERT_TRUE(parsed.has_value());
    const GameSummary back = parsed.value_or(RunRecord{}).summary;
    EXPECT_EQ(back.options.stimulus, Stimulus::Screen);
    EXPECT_EQ(back.options.ending, Ending::Time);
    EXPECT_EQ(back.end, EndReason::Time);
}

TEST(Runs, FileSkipsHeaderAndBrokenLines) {
    std::string csv = runs_file_header("\r\n");
    EXPECT_TRUE(csv.starts_with(kRunsVersionLine));
    csv += run_line(sample()) + "\r\n";
    csv += "broken,line\r\n";
    csv += run_line(sample()).replace(0, 4, "20x6") + "\r\n";  // не дата
    std::string bad_accuracy = run_line(sample());
    bad_accuracy.replace(bad_accuracy.find("90.91"), 5, "190.0");
    csv += bad_accuracy + "\r\n";
    csv += run_line(sample()) + "\r\n";
    std::size_t skipped = 0;
    const std::vector<RunRecord> runs = read_runs(csv, &skipped);
    EXPECT_EQ(runs.size(), 2U);
    EXPECT_EQ(skipped, 3U);
}

TEST(Runs, OutOfBoundsLineIsBroken) {
    // Отрицательный счёт и опции вне границ конструктора — не замер: зажатые
    // в границы, они попали бы в рекорды чужого набора (AUD-50).
    const auto with_field = [](std::size_t index, std::string_view value) {
        std::vector<std::string> fields;
        const std::string line = run_line(sample());
        std::size_t from = 0;
        for (std::size_t comma = line.find(','); comma != std::string::npos;
             from = comma + 1, comma = line.find(',', from))
            fields.push_back(line.substr(from, comma - from));
        fields.push_back(line.substr(from));
        fields.at(index) = value;
        std::string out;
        for (const std::string& f : fields)
            out += (out.empty() ? "" : ",") + f;
        return out;
    };
    EXPECT_TRUE(parse_run_line(with_field(9, "36")).has_value());
    EXPECT_FALSE(parse_run_line(with_field(9, "500")).has_value()) << "размер цели";
    EXPECT_FALSE(parse_run_line(with_field(12, "0")).has_value()) << "жизни";
    EXPECT_FALSE(parse_run_line(with_field(14, "-5")).has_value()) << "счёт";
}

TEST(Runs, EndWordsAndExportName) {
    for (const EndReason e : {EndReason::None, EndReason::Lives, EndReason::Time, EndReason::Bomb,
                              EndReason::GaveUp, EndReason::ToMenu})
        EXPECT_EQ(parse_end(end_word(e)), e);
    EXPECT_FALSE(parse_end("pause").has_value());
    EXPECT_EQ(export_file_name(2, {.year = 2026, .month = 1, .day = 5}),
              "export_profile2_20260105.csv");
}

}  // namespace
}  // namespace aim::core
