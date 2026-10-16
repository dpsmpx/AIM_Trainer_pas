// Конструктор «Своего набора» (D-37): какие строки действуют, переключатели
// и шаги в границах, подписи, набор того же вида для превью.

#include "core/measure.h"

#include <gtest/gtest.h>

namespace aim::core {
namespace {

TEST(Constructor, RowsApplyLikeNormalized) {
    MeasureOptions o;
    for (int r = 0; r < kOptionRows; ++r)
        EXPECT_TRUE(option_applies(o, static_cast<OptionRow>(r))) << "у цели со щелчком — все";
    o.action = Action::Hover;
    EXPECT_FALSE(option_applies(o, OptionRow::Buttons)) << "наведение — без кнопок";
    EXPECT_FALSE(option_applies(o, OptionRow::Bombs)) << "и без бомб";
    EXPECT_TRUE(option_applies(o, OptionRow::Moving));
    o.stimulus = Stimulus::Screen;
    for (const OptionRow row :
         {OptionRow::Action, OptionRow::Buttons, OptionRow::Moving, OptionRow::Appear,
          OptionRow::Bombs, OptionRow::Size, OptionRow::Lifetime})
        EXPECT_FALSE(option_applies(o, row)) << "смена цвета — без цели";
    EXPECT_TRUE(option_applies(o, OptionRow::Ending));
    EXPECT_TRUE(option_applies(o, OptionRow::Amount));
}

TEST(Constructor, TogglesFlipAndKeepTheRest) {
    MeasureOptions o;
    o.target_size = 30;
    toggle_option(o, OptionRow::Stimulus);
    EXPECT_EQ(o.stimulus, Stimulus::Screen);
    toggle_option(o, OptionRow::Stimulus);
    EXPECT_EQ(o.stimulus, Stimulus::Target);
    EXPECT_EQ(o.target_size, 30) << "опции цели не теряются, пока стимул — цвет";
    toggle_option(o, OptionRow::Action);
    toggle_option(o, OptionRow::Buttons);
    toggle_option(o, OptionRow::Moving);
    toggle_option(o, OptionRow::Appear);
    toggle_option(o, OptionRow::Bombs);
    toggle_option(o, OptionRow::Ending);
    EXPECT_EQ(o.action, Action::Hover);
    EXPECT_TRUE(o.color_buttons);
    EXPECT_TRUE(o.moving);
    EXPECT_FALSE(o.grows);
    EXPECT_FALSE(o.bombs);
    EXPECT_EQ(o.ending, Ending::Time);
    EXPECT_TRUE(option_toggles(OptionRow::Ending));
    EXPECT_FALSE(option_toggles(OptionRow::Size));
}

TEST(Constructor, StepsStayInBounds) {
    MeasureOptions o;
    step_option(o, OptionRow::Size, 1);
    EXPECT_EQ(o.target_size, 42);
    step_option(o, OptionRow::Size, 100);
    EXPECT_EQ(o.target_size, kMaxTargetSize);
    step_option(o, OptionRow::Lifetime, -3);
    EXPECT_EQ(o.lifetime_ms, 700);
    step_option(o, OptionRow::Lifetime, -100);
    EXPECT_EQ(o.lifetime_ms, kMinLifetimeMs);
    step_option(o, OptionRow::Amount, -100);
    EXPECT_EQ(o.lives, kMinLives) << "по жизням — жизни";
    o.ending = Ending::Time;
    step_option(o, OptionRow::Amount, 2);
    EXPECT_EQ(o.duration_s, 80) << "по времени — секунды, по 10";
    step_option(o, OptionRow::Amount, 100);
    EXPECT_EQ(o.duration_s, kMaxDurationS);
    EXPECT_EQ(o.lives, kMinLives);
}

TEST(Constructor, LabelsAndValues) {
    MeasureOptions o;
    EXPECT_EQ(option_label(o, OptionRow::Amount), "Жизни");
    EXPECT_EQ(option_value(o, OptionRow::Amount), "15");
    EXPECT_EQ(option_value(o, OptionRow::Size), "40 px");
    EXPECT_EQ(option_value(o, OptionRow::Appear), "рост и сжатие");
    o.ending = Ending::Time;
    EXPECT_EQ(option_label(o, OptionRow::Amount), "Время замера");
    EXPECT_EQ(option_value(o, OptionRow::Amount), "60 с");
    o.stimulus = Stimulus::Screen;
    EXPECT_EQ(option_value(o, OptionRow::Stimulus), "смена цвета окна");
}

TEST(Constructor, PresetLikeForPreview) {
    for (int i = 0; i < kPresetCount; ++i)
        EXPECT_EQ(preset_like(preset_options(i, {})), i);
    MeasureOptions o;
    o.moving = true;  // движение со щелчком — не набор
    EXPECT_FALSE(preset_index(o));
    EXPECT_EQ(preset_like(o), 1);
    EXPECT_EQ(combo_name(o), "Свой набор");
    o.color_buttons = true;
    EXPECT_EQ(preset_like(o), 2);
    o.action = Action::Hover;
    o.grows = true;
    EXPECT_EQ(preset_like(o), 3);
}

TEST(Constructor, OutOfRangeFromFileGoesToDefault) {
    MeasureOptions o;
    o.target_size = 7;
    o.lifetime_ms = 99'999;
    o.lives = 0;
    o.duration_s = 61;  // не ступень, но в границах — оставить
    EXPECT_EQ(reset_out_of_range(o), 3);
    EXPECT_EQ(o, (MeasureOptions{.duration_s = 61}));
}

}  // namespace
}  // namespace aim::core
