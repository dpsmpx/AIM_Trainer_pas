#pragma once

// Геометрия меню и панелей — константы и раскладка образца (PLATE_*, BTNS_*,
// DESC_*, SET_*, DrawMiniButton, HistoryPanelX, ProfilesPanelX и др. в
// archive/pascal/AIM_Trainer.pas). Одна на отрисовку и попадания (AUD-24):
// у образца кнопки внизу проверялись по другим прямоугольникам, а кнопки
// режимов — без сдвига анимации появления.

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
inline constexpr Rect kPlate{50, 40, 50 + 330, 40 + 240};  ///< плашка-превью режима
inline constexpr int kButtonsX = 420;
inline constexpr int kButtonsY = 40;
inline constexpr int kButtonW = 330;
inline constexpr int kButtonH = 34;
inline constexpr int kButtonGap = 8;
inline constexpr Rect kDescription{420, 214, 420 + 330, 214 + 76};

/// Кнопка режима i через since_enter после входа в меню: появляется
/// каскадом — через i·80 мс, за 400 мс, снизу на 20 px.
Rect mode_button(int i, Nanoseconds since_enter);
/// Доля появления кнопки i, 0…1.
double mode_button_entry(int i, Nanoseconds since_enter);

/// Кнопки внизу меню: Справка, Настройки, Рекорды, Награды, История,
/// Профиль.
enum class MiniButton : unsigned char { Help, Settings, Records, Achievements, History, Profile };
inline constexpr std::array<MiniButton, 6> kMiniButtons{
    MiniButton::Help,         MiniButton::Settings, MiniButton::Records,
    MiniButton::Achievements, MiniButton::History,  MiniButton::Profile};
Rect mini_button(MiniButton b);

// ---- Настройки ----
inline constexpr int kSettingsW = 420;
/// Выше, чем у образца (390), на две строки папки сохранений (D-33).
inline constexpr int kSettingsH = 470;
inline constexpr int kSettingsRowH = 40;
inline constexpr int kSettingsButton = 34;
Rect settings_panel();
/// Верх строки настроек: 0 — размер, 1 — время жизни, 2 — жизни, 3 — звук,
/// 4 — тема, 5 — открыть папку сохранений, 6 — выбрать её (D-33), 7 — сброс.
int settings_row_y(int row);
Rect settings_minus(int row);
Rect settings_plus(int row);
/// Строка-кнопка: 3 — звук, 4 — тема, 5 и 6 — папка сохранений.
Rect settings_toggle(int row);
inline constexpr int kSettingsOpenFolderRow = 5;
inline constexpr int kSettingsChooseFolderRow = 6;
Rect settings_reset();

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
