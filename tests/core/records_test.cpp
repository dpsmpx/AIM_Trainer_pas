// Рекорд и среднее за N — по набору параметров (D-38), главная мера набора,
// порог D-13 для реакции.

#include "core/records.h"

#include <gtest/gtest.h>

#include <vector>

namespace aim::core {
namespace {

RunRecord run(const MeasureOptions& o, int score, double median_ms = 300, int shots = 30) {
    RunRecord r;
    r.summary.options = normalized(o);
    r.summary.score = score;
    r.summary.shots = shots;
    r.summary.hits = shots;
    r.summary.accuracy = 100;
    r.summary.median_reaction_ms = median_ms;
    r.summary.avg_reaction_ms = median_ms;
    return r;
}

TEST(Records, KeyDependsOnlyOnRelevantOptions) {
    const MeasureOptions a = preset_options(0, {});
    MeasureOptions b = a;
    b.target_size = 60;  // у смены цвета размера нет
    b.lives = 3;         // окончание по времени — без жизней
    EXPECT_EQ(options_key(a), options_key(b));
    const MeasureOptions c = preset_options(1, {});
    MeasureOptions d = c;
    d.lifetime_ms = 1100;
    EXPECT_NE(options_key(c), options_key(d));
    EXPECT_EQ(options_key(a), "screen;time=60");
    EXPECT_EQ(options_key(c), "target;click;left;still;grow;bombs;size=40;life=1000;lives=15");
}

TEST(Records, BestAndAverageOfLastN) {
    const MeasureOptions classic = preset_options(1, {});
    const MeasureOptions flick = preset_options(4, {});
    std::vector<RunRecord> runs;
    for (const int score : {10, 50, 20, 30, 40, 60})
        runs.push_back(run(classic, score));
    runs.push_back(run(flick, 999));
    const ComboStats st = combo_stats(runs, classic, 5);
    EXPECT_EQ(st.runs, 6);
    ASSERT_TRUE(st.best.has_value());
    EXPECT_EQ(runs[st.best.value_or(0)].summary.score, 60);
    EXPECT_EQ(st.average.runs, 5);
    EXPECT_DOUBLE_EQ(st.average.primary, 40) << "50, 20, 30, 40, 60";
    EXPECT_EQ(st.last, 5U);
    const std::vector<double> recent = recent_primary(runs, classic, 3);
    EXPECT_EQ(recent, (std::vector<double>{30, 40, 60}));
}

TEST(Records, ScreenBestIsLowestMedianFromTwentyShots) {
    const MeasureOptions screen = preset_options(0, {});
    std::vector<RunRecord> runs{run(screen, 0, 150, 5), run(screen, 0, 280), run(screen, 0, 260),
                                run(screen, 0, 300)};
    const ComboStats st = combo_stats(runs, screen, 50);
    ASSERT_TRUE(st.best.has_value());
    EXPECT_DOUBLE_EQ(runs[st.best.value_or(0)].summary.median_reaction_ms, 260)
        << "150 — из 5 выстрелов";
    EXPECT_DOUBLE_EQ(st.average.primary, (150 + 280 + 260 + 300) / 4.0)
        << "в среднее идут все замеры";
    EXPECT_TRUE(lower_is_better(screen));
}

TEST(Records, AllCombosFreshFirst) {
    const MeasureOptions a = preset_options(1, {});
    const MeasureOptions b = preset_options(3, {});
    const std::vector<RunRecord> runs{run(a, 1), run(b, 2), run(a, 3)};
    const std::vector<ComboStats> all = all_combo_stats(runs, 50);
    ASSERT_EQ(all.size(), 2U);
    EXPECT_EQ(all[0].key, options_key(a));
    EXPECT_EQ(all[1].key, options_key(b));
    EXPECT_TRUE(all_combo_stats({}, 50).empty());
}

}  // namespace
}  // namespace aim::core
