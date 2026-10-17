#pragma once

// Цвета, которые рисует программа: все — из темы (D-44). Встроенные темы —
// здесь, темы игрока — файлы themes/ИМЯ.ini (core/theme.h). Одно место для
// отрисовки и проверок (AUD-22).

#include <array>
#include <cstdint>
#include <string_view>

#include "core/measure.h"

namespace aim::core {

struct Rgba {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    friend bool operator==(const Rgba&, const Rgba&) = default;
};

/// Тема: цвет каждой роли интерфейса. Где цвет мигает или гаснет, программа
/// меняет его прозрачность сама, а цвет берёт из темы.
struct Theme {
    // Окно и текст.
    Rgba background;  ///< фон окна
    Rgba accent;  ///< акцент: разделы справки, полоса прокрутки, график, рамки
    Rgba text;    ///< основной текст
    Rgba text_dim;  ///< второстепенный текст: подсказки, описания, оси
    Rgba label;  ///< подпись у пары «подпись: значение»
    Rgba gold;   ///< рекорды, достижения, рамка уведомления
    Rgba good;   ///< «Начать замер», «▶», плашка реплея
    Rgba warn;   ///< предупреждение: «Фальстарт»
    // Замер.
    Rgba target_left;   ///< цель для ЛКМ
    Rgba target_right;  ///< цель для ПКМ
    Rgba signal;        ///< замер на цвет: окно после смены цвета
    Rgba bomb;          ///< бомба
    Rgba bomb_cross;    ///< крест на бомбе
    Rgba hud;           ///< ячейки панели замера
    Rgba overlay;  ///< плашки поверх замера: серия, подсказка кнопок, реплей
    Rgba trail;    ///< след курсора
    Rgba wave;     ///< волна попадания
    Rgba target_mark;  ///< метка стороны на цели «по цвету» (D-48)
    // Панели и кнопки.
    Rgba dim;           ///< затемнение под панелью
    Rgba panel;         ///< панель
    Rgba panel_border;  ///< рамка панели
    Rgba row;           ///< подложка строки панели
    Rgba button;        ///< светлая кнопка «−»/«+»
    Rgba button_text;   ///< текст на светлой кнопке
    Rgba control;  ///< кнопка-действие: «Сбросить», «Экспорт в CSV»
    Rgba toast;    ///< фон уведомления
    Rgba toast_text;  ///< текст уведомления
    // Меню.
    Rgba plate;                              ///< плашка-превью набора
    Rgba graph;                              ///< фон графика
    Rgba grid;                               ///< сетка графика
    Rgba particles;                          ///< частицы фона меню
    std::array<Rgba, kPresetCount> presets;  ///< кнопки наборов по порядку kPresets
    Rgba custom;                             ///< кнопка «Своего набора»

    /// Цель цвета color: 0 — для ЛКМ, 1 — для ПКМ.
    [[nodiscard]] Rgba target(int color) const { return color == 1 ? target_right : target_left; }
};

namespace detail {

constexpr Theme dark_theme() {
    Theme t;
    t.background = {28, 28, 32};
    t.accent = {97, 207, 255};
    t.text = {255, 255, 255};
    t.text_dim = {190, 190, 195};
    t.label = {165, 170, 180};
    t.gold = {255, 210, 90};
    t.good = {46, 160, 90};
    t.warn = {255, 120, 100};
    t.target_left = {97, 207, 255, 220};
    t.target_right = {255, 80, 80, 220};
    t.signal = {46, 180, 90};
    t.bomb = {40, 20, 20, 230};
    t.bomb_cross = {255, 255, 255};
    t.hud = {60, 60, 65, 220};
    t.overlay = {40, 40, 45, 180};
    t.trail = {200, 220, 255};
    t.wave = {255, 255, 255};
    t.target_mark = {20, 20, 24, 210};
    t.dim = {15, 15, 18, 215};
    t.panel = {45, 45, 50, 238};
    t.panel_border = {90, 90, 95};
    t.row = {255, 255, 255, 40};
    t.button = {255, 255, 255, 150};
    t.button_text = {30, 30, 34};
    t.control = {90, 90, 95, 160};
    t.toast = {60, 50, 20, 220};
    t.toast_text = {255, 230, 160};
    t.plate = {255, 255, 255, 45};
    t.graph = {30, 30, 35, 150};
    t.grid = {200, 200, 200, 80};
    t.particles = {100, 200, 255};
    t.presets = {{{120, 90, 200}, {70, 130, 180}, {205, 92, 92}, {60, 179, 113}, {255, 140, 0}}};
    t.custom = {110, 110, 130};
    return t;
}

constexpr Theme neon_theme() {
    Theme t = dark_theme();
    t.background = {12, 8, 24};
    t.accent = {180, 80, 255};
    t.target_left = {0, 255, 220, 230};
    t.target_right = {255, 0, 180, 230};
    t.signal = {0, 230, 140};
    t.particles = {180, 80, 255};
    t.trail = {0, 255, 220};
    t.panel = {30, 20, 50, 238};
    t.panel_border = {120, 70, 180};
    return t;
}

constexpr Theme minimal_theme() {
    Theme t = dark_theme();
    t.background = {35, 35, 38};
    t.accent = {160, 160, 165};
    t.target_left = {150, 150, 160, 220};
    t.target_right = {180, 140, 140, 220};
    t.signal = {190, 190, 195};
    t.particles = {150, 150, 155};
    t.trail = {170, 170, 175};
    return t;
}

}  // namespace detail

/// Встроенная тема: слово в файлах (theme=, base=), имя для игрока, цвета.
struct BuiltinTheme {
    std::string_view id;
    std::string_view name;
    Theme theme;
};

inline constexpr std::array<BuiltinTheme, 3> kThemes{{
    {"dark", "Тёмная", detail::dark_theme()},
    {"neon", "Неон", detail::neon_theme()},
    {"minimal", "Минимализм", detail::minimal_theme()},
}};

}  // namespace aim::core
