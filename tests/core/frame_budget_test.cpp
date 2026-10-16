// Запас кадра в замере (D-41): сводка работы кадров против половины
// периода монитора.

#include "core/frame_budget.h"

#include <gtest/gtest.h>

namespace aim::core {
namespace {

TEST(FrameBudget, ReportCountsFramesOverHalfPeriod) {
    FrameBudget b;
    EXPECT_TRUE(b.empty());
    b.start(ms(4));  // 250 Гц: запас — 2 мс
    for (int i = 0; i < 98; ++i)
        b.add(ms(1));
    b.add(ms(2));  // ровно половина — в запасе
    b.add(ms(3));  // дольше половины
    const FrameBudgetReport r = b.report();
    EXPECT_EQ(r.frames, 100U);
    EXPECT_EQ(r.median, ms(1));
    EXPECT_EQ(r.p99, ms(2));
    EXPECT_EQ(r.max, ms(3));
    EXPECT_EQ(r.over_half, 1U);
    EXPECT_EQ(describe(r), "кадр замера: 99 % — 2.00 мс при запасе 2.00 мс, дольше — 1 из 100");
}

TEST(FrameBudget, StartClearsPreviousRun) {
    FrameBudget b;
    b.start(ms(16));
    b.add(ms(20));
    b.start(ms(8));
    EXPECT_TRUE(b.empty());
    b.add(ms(1));
    EXPECT_EQ(b.report().over_half, 0U);
    EXPECT_EQ(b.report().period, ms(8));
}

}  // namespace
}  // namespace aim::core
