// Главное меню без окна (core/menu.h) — как UpdateMenuInput и StartScreen
// образца; геометрия одна для отрисовки и попаданий (AUD-24).

#include "core/menu.h"

#include <gtest/gtest.h>

#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "core/help.h"
#include "core/layout.h"

namespace aim::core {
namespace {

// Меню давно открыто: кнопки режимов уже на месте.
constexpr Nanoseconds kEntered = ms(1'000);
constexpr Nanoseconds kLater = ms(5'000);

Vec2 center(const Rect& r) {
    return {(r.x1 + r.x2) / 2.0, (r.y1 + r.y2) / 2.0};
}

ClickInput click_at(Vec2 p, Nanoseconds t = kLater, MouseButton b = MouseButton::Left) {
    return {.button = b, .position = p, .time = t};
}

class MenuTest : public ::testing::Test {
protected:
    MenuTest() { menu_.enter(kEntered); }

    template <class T>
    [[nodiscard]] int count(const std::vector<MenuRequest>& requests) const {
        int n = 0;
        for (const MenuRequest& r : requests)
            n += std::holds_alternative<T>(r) ? 1 : 0;
        return n;
    }

    Profile profile_;
    Menu menu_{profile_, 1, false};
};

TEST(MenuTutorial, ClicksPageAndLastClosesAndSaves) {
    Profile profile;
    Menu menu(profile, 1, true);
    menu.enter(kEntered);
    for (int step = 1; step < kTutorialSteps; ++step) {
        // Щелчок по кнопке набора под туториалом замер не запускает.
        EXPECT_EQ(menu.click(click_at(center(preset_button(0, kLater)))), std::nullopt);
        EXPECT_EQ(menu.tutorial_step(), step);
        EXPECT_TRUE(menu.tutorial_shown());
    }
    EXPECT_EQ(menu.click(click_at({5, 5})), std::nullopt);
    EXPECT_FALSE(menu.tutorial_shown());
    EXPECT_TRUE(profile.tutorial_seen);
    EXPECT_EQ(menu.take_requests().size(), 1U);
}

TEST(MenuTutorial, EscapeSkipsAndMarksSeen) {
    Profile profile;
    Menu menu(profile, 1, true);
    menu.key({.key = Key::Escape, .time = kLater});
    EXPECT_FALSE(menu.tutorial_shown());
    EXPECT_TRUE(profile.tutorial_seen);
    EXPECT_EQ(menu.take_requests().size(), 1U);
}

TEST_F(MenuTest, PresetButtonStartsItsPresetAndRemembersIt) {
    const auto options = menu_.click(click_at(center(preset_button(3, kLater - kEntered))));
    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options.value_or(MeasureOptions{}).action, Action::Hover) << "«Слежение»";
    EXPECT_EQ(menu_.selected_preset(), 3);
    EXPECT_EQ(profile_.preset, 3);
    EXPECT_EQ(count<SaveProfile>(menu_.take_requests()), 1) << "выбранный набор — в профиль";
}

TEST_F(MenuTest, PlateStartsHoveredPresetFirstIsScreen) {
    menu_.hover(center(preset_button(0, kLater - kEntered)), kLater);
    EXPECT_EQ(menu_.selected_preset(), 0);
    const auto options = menu_.click(click_at(center(kPlate)));
    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options.value_or(MeasureOptions{}).stimulus, Stimulus::Screen)
        << "первым — замер на цвет (D-37)";
}

TEST_F(MenuTest, ButtonIsHitWhereItIsDrawnDuringEntry) {
    // Сразу после входа кнопка 0 ниже своего места на 20 px (AUD-24: у
    // образца попадание проверялось по месту без сдвига).
    const Rect drawn = preset_button(0, 0);
    EXPECT_EQ(drawn.y1, kButtonsY + 20);
    const Rect resting = preset_button(0, kLater);
    EXPECT_EQ(resting.y1, kButtonsY);
    Menu menu(profile_, 1, false);
    menu.enter(kLater);
    EXPECT_EQ(menu.click(click_at({center(drawn).x, resting.y1 + 5.0}, kLater)), std::nullopt)
        << "место без сдвига — ещё не кнопка";
}

TEST_F(MenuTest, EmptySpaceTogglesEffectsRightClickDoesNothing) {
    const bool before = profile_.settings.menu_effects;
    EXPECT_EQ(menu_.click(click_at({400, 20}, kLater, MouseButton::Right)), std::nullopt);
    EXPECT_EQ(profile_.settings.menu_effects, before);
    EXPECT_EQ(menu_.click(click_at({400, 20})), std::nullopt);
    EXPECT_NE(profile_.settings.menu_effects, before);
    EXPECT_TRUE(menu_.take_requests().empty()) << "записывается с другим сохранением";
}

TEST_F(MenuTest, InformationPanelsCloseOnAnyClick) {
    for (const auto& [button, panel] : {std::pair{MiniButton::Records, Panel::Records},
                                        std::pair{MiniButton::Achievements, Panel::Achievements}}) {
        menu_.click(click_at(center(mini_button(button))));
        EXPECT_EQ(menu_.panel(), panel);
        // Щелчок по кнопке набора под панелью только закрывает её.
        EXPECT_EQ(menu_.click(click_at(center(preset_button(0, kLater)))), std::nullopt);
        EXPECT_EQ(menu_.panel(), Panel::None);
    }
}

TEST_F(MenuTest, CustomItemOpensConstructorAndStartsItsOptions) {
    // «Свой набор» — шестая кнопка: щелчок открывает конструктор, а не замер.
    EXPECT_EQ(menu_.click(click_at(center(preset_button(kCustomItem, kLater)))), std::nullopt);
    ASSERT_EQ(menu_.panel(), Panel::Constructor);
    EXPECT_EQ(menu_.selected_preset(), kCustomItem);
    menu_.take_requests();
    // Заготовка «Флик» заполняет опции, ничего не запуская.
    menu_.click(click_at(center(constructor_preset(4))));
    EXPECT_EQ(profile_.custom, preset_options(4, profile_.settings.preset));
    // Переключатель и шаги.
    menu_.click(click_at(center(constructor_row(static_cast<int>(OptionRow::Ending)))));
    EXPECT_EQ(profile_.custom.ending, Ending::Time);
    const int amount = static_cast<int>(OptionRow::Amount);
    menu_.click(click_at(center(constructor_plus(amount))));
    EXPECT_EQ(profile_.custom.duration_s, 70);
    menu_.click(click_at(center(constructor_minus(amount))));
    menu_.click(click_at(center(constructor_minus(amount))));
    EXPECT_EQ(profile_.custom.duration_s, 50);
    // Щелчок по строке мимо «−»/«+» — ничего.
    menu_.click(click_at(center(constructor_row(amount))));
    EXPECT_EQ(profile_.custom.duration_s, 50);
    EXPECT_EQ(count<SaveProfile>(menu_.take_requests()), 5) << "каждая правка записывается";
    // Опция не к замеру не правится: у смены цвета размера цели нет.
    menu_.click(click_at(center(constructor_row(static_cast<int>(OptionRow::Stimulus)))));
    const int size = static_cast<int>(OptionRow::Size);
    menu_.click(click_at(center(constructor_plus(size))));
    EXPECT_EQ(profile_.custom.target_size, 40);
    menu_.click(click_at(center(constructor_row(static_cast<int>(OptionRow::Stimulus)))));
    // «Начать замер» — опции «Своего набора», панель закрыта, пункт запомнен.
    const std::optional<MeasureOptions> started =
        menu_.click(click_at(center(constructor_start())));
    ASSERT_TRUE(started.has_value());
    EXPECT_EQ(started.value_or(MeasureOptions{}), normalized(profile_.custom));
    EXPECT_EQ(started.value_or(MeasureOptions{}).duration_s, 50);
    EXPECT_FALSE(started.value_or(MeasureOptions{}).grows) << "Флик — сразу полная";
    EXPECT_EQ(menu_.panel(), Panel::None);
    EXPECT_EQ(profile_.preset, kCustomItem);
    // Плашка при выбранном «Своём наборе» запускает его же.
    EXPECT_EQ(menu_.click(click_at(center(kPlate))), normalized(profile_.custom));
}

TEST_F(MenuTest, ConstructorClosesOnClickOutsideAndEsc) {
    menu_.click(click_at(center(preset_button(kCustomItem, kLater))));
    menu_.click(click_at({5, 5}));
    EXPECT_EQ(menu_.panel(), Panel::None);
    menu_.click(click_at(center(preset_button(kCustomItem, kLater))));
    menu_.key({.key = Key::Escape, .time = kLater});
    EXPECT_EQ(menu_.panel(), Panel::None);
}

TEST_F(MenuTest, RecordsRowAsksForItsReplay) {
    // Два набора в истории: свежий — сверху (D-38).
    RunRecord classic;
    classic.summary.options = preset_options(1, {});
    classic.summary.score = 100;
    RunRecord flick = classic;
    flick.summary.options = preset_options(4, {});
    profile_.runs = {classic, flick};
    menu_.click(click_at(center(mini_button(MiniButton::Records))));
    ASSERT_EQ(menu_.panel(), Panel::Records);
    menu_.take_requests();
    menu_.click(click_at(center(records_row(1))));
    const std::vector<MenuRequest> requests = menu_.take_requests();
    ASSERT_EQ(requests.size(), 1U);
    const auto* play = std::get_if<PlayReplay>(&requests.front());
    ASSERT_NE(play, nullptr);
    EXPECT_EQ(play->options, classic.summary.options) << "вторая строка — набор постарше";
    EXPECT_EQ(menu_.panel(), Panel::Records) << "реплей просит программу, панель открыта";
    // Строки без набора — мимо: панель закрывается, реплея не просит.
    menu_.click(click_at(center(records_row(2))));
    EXPECT_TRUE(menu_.take_requests().empty());
    EXPECT_EQ(menu_.panel(), Panel::None);
}

TEST_F(MenuTest, HelpScrollsByArrowsWheelAndBar) {
    menu_.click(click_at(center(mini_button(MiniButton::Help))));
    ASSERT_EQ(menu_.panel(), Panel::Help);
    EXPECT_EQ(menu_.help_scroll(), 0);
    ASSERT_GT(kHelpMaxScroll, 0) << "справка длиннее панели — есть что листать";
    menu_.key({.key = Key::Up, .time = kLater});
    EXPECT_EQ(menu_.help_scroll(), 0) << "выше начала не листается";
    menu_.key({.key = Key::Down, .time = kLater});
    menu_.key({.key = Key::Down, .time = kLater});
    EXPECT_EQ(menu_.help_scroll(), 2);
    menu_.wheel({.y = -1, .time = kLater});  // к себе — вниз, три строки
    EXPECT_EQ(menu_.help_scroll(), 5);
    menu_.wheel({.y = 1, .time = kLater});
    EXPECT_EQ(menu_.help_scroll(), 2);
    // Страниц — с запасом: справка растёт, а ниже конца не листается.
    for (int i = 0; i <= kHelpMaxScroll / kHelpVisibleLines + 1; ++i)
        menu_.key({.key = Key::PageDown, .time = kLater});
    EXPECT_EQ(menu_.help_scroll(), kHelpMaxScroll) << "ниже конца не листается";
    // Полоса: щелчок по верху — к началу, по низу — к концу.
    const Rect track = help_track();
    menu_.click(click_at({center(track).x, static_cast<double>(track.y1)}));
    EXPECT_EQ(menu_.help_scroll(), 0);
    menu_.click(click_at({center(track).x, static_cast<double>(track.y2)}));
    EXPECT_EQ(menu_.help_scroll(), kHelpMaxScroll);
    // Щелчок по тексту справки её не закрывает; вне панели и Esc — закрывают.
    menu_.click(click_at(center(help_panel())));
    EXPECT_EQ(menu_.panel(), Panel::Help);
    menu_.click(click_at({5, 5}));
    EXPECT_EQ(menu_.panel(), Panel::None);
    menu_.click(click_at(center(mini_button(MiniButton::Help))));
    EXPECT_EQ(menu_.help_scroll(), 0) << "открыта заново — с начала";
    menu_.key({.key = Key::Escape, .time = kLater});
    EXPECT_EQ(menu_.panel(), Panel::None);
}

TEST_F(MenuTest, HelpThumbFollowsCursorWhileLeftHeld) {
    menu_.click(click_at(center(mini_button(MiniButton::Help))));
    const Rect track = help_track();
    const double x = center(track).x;
    menu_.drag({x, static_cast<double>(track.y2)}, true);
    EXPECT_EQ(menu_.help_scroll(), 0) << "ЛКМ нажата не на полосе — не тянется";
    menu_.click(click_at({x, static_cast<double>(track.y1)}));
    menu_.drag({x, center(track).y}, true);
    EXPECT_EQ(menu_.help_scroll(), (kHelpMaxScroll + 1) / 2);
    menu_.drag({x + 200, static_cast<double>(track.y2 + 100)}, true);
    EXPECT_EQ(menu_.help_scroll(), kHelpMaxScroll) << "курсор ушёл с полосы — тянется дальше";
    menu_.drag({x, static_cast<double>(track.y1)}, false);
    EXPECT_EQ(menu_.help_scroll(), kHelpMaxScroll) << "ЛКМ отпущена — ползунок стоит";
    menu_.drag({x, static_cast<double>(track.y1)}, true);
    EXPECT_EQ(menu_.help_scroll(), kHelpMaxScroll) << "новое нажатие — только щелчком по полосе";
}

TEST_F(MenuTest, WheelOutsideHelpDoesNothing) {
    menu_.wheel({.y = -3, .time = kLater});
    menu_.key({.key = Key::Down, .time = kLater});
    EXPECT_EQ(menu_.help_scroll(), 0);
    EXPECT_EQ(menu_.panel(), Panel::None);
}

TEST_F(MenuTest, HistoryOpensClosesOutside) {
    menu_.click(click_at(center(mini_button(MiniButton::History))));
    EXPECT_EQ(menu_.panel(), Panel::History);
    EXPECT_TRUE(menu_.take_requests().empty()) << "история — уже в профиле";
    menu_.click(click_at(center(history_panel())));
    EXPECT_EQ(menu_.panel(), Panel::History);
    menu_.click(click_at({5, 5}));
    EXPECT_EQ(menu_.panel(), Panel::None);
}

TEST_F(MenuTest, HoverDoesNotSelectUnderPanel) {
    menu_.click(click_at(center(mini_button(MiniButton::Help))));
    menu_.hover(center(preset_button(3, kLater)), kLater);
    EXPECT_EQ(menu_.selected_preset(), 1) << "под панелью — прежний набор профиля";
}

TEST_F(MenuTest, SettingsStepToggleResetAndClose) {
    menu_.click(click_at(center(mini_button(MiniButton::Settings))));
    ASSERT_EQ(menu_.panel(), Panel::Settings);
    menu_.click(click_at(center(settings_plus(kSettingsSizeRow))));
    EXPECT_EQ(profile_.settings.preset.target_size, 42);
    menu_.click(click_at(center(settings_minus(kSettingsLivesRow))));
    EXPECT_EQ(profile_.settings.preset.lives, 14);
    menu_.click(click_at(center(settings_plus(kSettingsAverageRow))));
    EXPECT_EQ(profile_.settings.average_runs, 100);
    menu_.click(click_at(center(settings_toggle(kSettingsEndingRow))));
    EXPECT_EQ(profile_.settings.preset.ending, Ending::Time);
    menu_.click(click_at(center(settings_toggle(kSettingsSoundRow))));
    EXPECT_FALSE(profile_.settings.sound);
    // Тему по кругу листает программа: темы игрока знает она (D-44).
    menu_.click(click_at(center(settings_toggle(kSettingsThemeRow))));
    EXPECT_EQ(count<NextTheme>(menu_.take_requests()), 1);
    EXPECT_EQ(profile_.settings.theme, "dark");
    menu_.click(click_at(center(settings_toggle(kSettingsFontRow))));
    EXPECT_EQ(profile_.settings.font, 1) << "Condensed → Mono (D-43)";
    menu_.click(click_at(center(settings_toggle(kSettingsFontRow))));
    EXPECT_EQ(profile_.settings.font, 0);
    menu_.click(click_at(center(settings_toggle(kSettingsHudRow))));
    EXPECT_FALSE(profile_.settings.hud);
    menu_.click(click_at(center(settings_toggle(kSettingsReplaysRow))));
    EXPECT_FALSE(profile_.settings.replays);
    EXPECT_EQ(count<SaveProfile>(menu_.take_requests()), 4);
    menu_.click(click_at(center(settings_reset())));
    EXPECT_EQ(profile_.settings.preset.target_size, 40);
    EXPECT_EQ(profile_.settings.preset.ending, Ending::Lives);
    EXPECT_EQ(profile_.settings.average_runs, 50);
    EXPECT_TRUE(profile_.settings.sound);
    EXPECT_TRUE(profile_.settings.hud);
    EXPECT_TRUE(profile_.settings.replays);
    // Щелчок внутри панели мимо кнопок — панель открыта; снаружи — закрыта.
    menu_.click(click_at({static_cast<double>(settings_panel().x1 + 5),
                          static_cast<double>(settings_panel().y1 + 5)}));
    EXPECT_EQ(menu_.panel(), Panel::Settings);
    menu_.click(click_at({5, 5}));
    EXPECT_EQ(menu_.panel(), Panel::None);
}

TEST_F(MenuTest, DataFolderRowsAndExportAreRequests) {
    menu_.click(click_at(center(mini_button(MiniButton::Settings))));
    menu_.click(click_at(center(settings_toggle(kSettingsOpenFolderRow))));
    menu_.click(click_at(center(settings_toggle(kSettingsChooseFolderRow))));
    std::vector<MenuRequest> r = menu_.take_requests();
    EXPECT_EQ(count<OpenDataFolder>(r), 1);
    EXPECT_EQ(count<ChooseDataFolder>(r), 1);
    EXPECT_EQ(menu_.panel(), Panel::Settings);
    // Строки папки — выше сброса и внутри панели.
    EXPECT_LT(settings_toggle(kSettingsChooseFolderRow).y2, settings_reset().y1);
    EXPECT_LT(settings_reset().y2, settings_panel().y2 - 44)
        << "под сбросом — подпись в две строки";
    for (int row = 0; row < kSettingsResetRow; ++row)
        EXPECT_LE(settings_row_y(row) + kSettingsButton, settings_row_y(row + 1)) << row;
    menu_.key({.key = Key::Escape, .time = kLater});

    menu_.click(click_at(center(mini_button(MiniButton::History))));
    menu_.take_requests();
    menu_.click(click_at(center(history_export())));
    r = menu_.take_requests();
    EXPECT_EQ(count<ExportHistory>(r), 1);
    EXPECT_EQ(menu_.panel(), Panel::History);
}

TEST_F(MenuTest, ProfilesSwitchOnlyToAnother) {
    menu_.click(click_at(center(mini_button(MiniButton::Profile))));
    ASSERT_EQ(menu_.panel(), Panel::Profiles);
    menu_.click(click_at(center(profile_row(1))));
    EXPECT_TRUE(menu_.take_requests().empty()) << "профиль 1 уже открыт";
    menu_.click(click_at(center(profile_row(3))));
    const std::vector<MenuRequest> r = menu_.take_requests();
    ASSERT_EQ(r.size(), 1U);
    ASSERT_TRUE(std::holds_alternative<SwitchProfile>(r[0]));
    EXPECT_EQ(std::get<SwitchProfile>(r[0]).number, 3);
    EXPECT_EQ(menu_.panel(), Panel::Profiles) << "панель остаётся";
    menu_.key({.key = Key::Escape, .time = kLater});
    EXPECT_EQ(menu_.panel(), Panel::None);
}

TEST(Layout, MiniButtonsAreTheDrawnRectangles) {
    // (10, H−35)–(100, H−10), шаг 100.
    const Rect help = mini_button(MiniButton::Help);
    EXPECT_EQ(help.x1, 10);
    EXPECT_EQ(help.x2, 100);
    EXPECT_EQ(help.y1, kH - 35);
    EXPECT_EQ(help.y2, kH - 10);
    EXPECT_EQ(mini_button(MiniButton::Profile).x1, 510);
    EXPECT_FALSE(help.contains({103, 570})) << "у образца попадание шло до 105";
}

}  // namespace
}  // namespace aim::core
