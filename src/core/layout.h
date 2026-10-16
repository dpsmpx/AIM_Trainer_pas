#pragma once

// Геометрия меню и панелей в логических 800×600 — одна на отрисовку и
// попадания (AUD-24).

#include <array>

#include "core/screen.h"
#include "core/target.h"
#include "core/time.h"

namespace aim::core {

/// Прямоугольник окна [x1, x2] × [y1, y2] — границы входят, как у проверок
/// попадания образца.
struct Rect {
    int x1 = 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;

    [[nodiscard]] bool contains(Vec2 p) const {
        return p.x >= static_cast<double>(x1) && p.x <= static_cast<double>(x2) &&
               p.y >= static_cast<double>(y1) && p.y <= static_cast<double>(y2);
    }
};

inline constexpr int kW = kWindowWidth;
inline constexpr int kH = kWindowHeight;

// ---- Меню ----
inline constexpr Rect kPlate{50, 40, 50 + 330, 40 + 240};  ///< плашка-превью набора
inline constexpr int kButtonsX = 420;
inline constexpr int kButtonsY = 40;
inline constexpr int kButtonW = 330;
inline constexpr int kButtonH = 30;
inline constexpr int kButtonGap = 6;
inline constexpr Rect kDescription{420, 222, 420 + 330, 222 + 68};

/// Кнопка набора i через since_enter после входа в меню: появляется
/// каскадом — через i·80 мс, за 400 мс, снизу на 20 px.
Rect preset_button(int i, Nanoseconds since_enter);
/// Доля появления кнопки i, 0…1.
double preset_button_entry(int i, Nanoseconds since_enter);

/// Кнопки внизу меню: Справка, Настройки, Рекорды, Награды, История,
/// Профиль.
enum class MiniButton : unsigned char { Help, Settings, Records, Achievements, History, Profile };
inline constexpr std::array<MiniButton, 6> kMiniButtons{
    MiniButton::Help,         MiniButton::Settings, MiniButton::Records,
    MiniButton::Achievements, MiniButton::History,  MiniButton::Profile};
Rect mini_button(MiniButton b);

// ---- Настройки ----
inline constexpr int kSettingsW = 420;
inline constexpr int kSettingsH = 520;
inline constexpr int kSettingsRowH = 38;
inline constexpr int kSettingsButton = 32;
// Ряды настроек по порядку.
inline constexpr int kSettingsSizeRow = 0;      ///< размер цели, «−»/«+»
inline constexpr int kSettingsLifetimeRow = 1;  ///< жизнь цели, «−»/«+»
inline constexpr int kSettingsLivesRow = 2;     ///< жизни, «−»/«+»
inline constexpr int kSettingsEndingRow = 3;   ///< окончание: жизни или время
inline constexpr int kSettingsAverageRow = 4;  ///< среднее за N, «−»/«+» (D-38)
inline constexpr int kSettingsSoundRow = 5;
inline constexpr int kSettingsThemeRow = 6;
inline constexpr int kSettingsOpenFolderRow = 7;  ///< открыть папку сохранений (D-33)
inline constexpr int kSettingsChooseFolderRow = 8;  ///< выбрать её (D-33)
inline constexpr int kSettingsResetRow = 9;
Rect settings_panel();
int settings_row_y(int row);
Rect settings_minus(int row);
Rect settings_plus(int row);
/// Строка-кнопка во всю ширину панели.
Rect settings_toggle(int row);
Rect settings_reset();

// ---- Справка ----
inline constexpr int kHelpW = 620;
inline constexpr int kHelpH = 400;
inline constexpr int kHelpTop = 56;  ///< от верха панели до первой строки
inline constexpr int kHelpLineH = 22;  ///< строка справки
Rect help_panel();
/// Полоса прокрутки справки: щелчок по ней — прокрутка к месту.
Rect help_track();

// ---- Рекорды наборов ----
inline constexpr int kRecordsW = 760;
inline constexpr int kRecordsH = 470;
inline constexpr int kRecordsRows = 10;  ///< строк наборов на панели
Rect records_panel();
/// Строка набора i (0…kRecordsRows − 1), свежие сверху: щелчок — реплей
/// лучшего замера набора (D-39).
Rect records_row(int i);

// ---- История и профили ----
inline constexpr int kHistoryW = 520;
inline constexpr int kHistoryH = 460;
Rect history_panel();
Rect history_export();
inline constexpr int kProfilesW = 360;
inline constexpr int kProfilesH = 320;
Rect profiles_panel();
/// Строка профиля number (1…4).
Rect profile_row(int number);

}  // namespace aim::core
