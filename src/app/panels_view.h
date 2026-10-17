#pragma once

// Панели поверх меню и туториал первого запуска на холсте app/canvas.h:
// справка, настройки, «Свой набор» (D-37), рекорды наборов (D-38, D-39),
// достижения, история, профили. Что открыто — core/menu.h, геометрия —
// core/layout.h; меню под панелями — app/menu_view.h.

#include <string>
#include <string_view>

#include "app/canvas.h"
#include "app/player_files.h"
#include "core/palette.h"
#include "core/profile.h"

namespace aim::app {

/// Справка: видимые строки с прокрутки scroll и полоса прокрутки.
void render_help(Canvas& c, const core::Theme& th, int scroll);
/// Настройки; folder — папка сохранений (D-33).
void render_settings(Canvas& c, const core::Theme& th, const core::Settings& s,
                     std::string_view folder);
/// «Свой набор»: заготовки, строки опций, «Начать замер» (D-37).
void render_constructor(Canvas& c, const core::Theme& th, const core::Profile& profile);
/// Рекорды наборов: лучший замер и среднее за N; «▶» — у набора с реплеем.
void render_records(Canvas& c, const core::Theme& th, const core::Profile& profile,
                    const PlayerFiles& files);
void render_achievements(Canvas& c, const core::Theme& th, const core::Profile& profile);
/// Последние замеры с параметрами и «Экспорт в CSV»; file_name — файл истории.
void render_history(Canvas& c, const core::Theme& th, const core::Profile& profile,
                    std::string_view file_name);
/// Профили; current — открытый.
void render_profiles(Canvas& c, const core::Theme& th, int current);
/// Туториал первого запуска: шаг step (0…kTutorialSteps − 1).
void render_tutorial(Canvas& c, const core::Theme& th, int step);

/// Путь, обрезанный слева до ширины width шрифтом kText: «…\Roaming\AimTrainer\».
std::string fit_path(Canvas& c, std::string_view prefix, std::string_view path, int width);

}  // namespace aim::app
