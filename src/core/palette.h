#pragma once

// Цвета и подписи, которые рисует программа: темы и кнопки наборов замера.
// Одно место для отрисовки и проверок (AUD-22).

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
};

struct Theme {
    std::string_view name;
    Rgba background;
    Rgba accent;
    std::array<Rgba, 2> circles;  ///< цель: 0 — синяя (ЛКМ), 1 — красная (ПКМ)
    Rgba signal;  ///< замер на цвет: окно после смены цвета
};

/// Темы: 0 — Тёмная, 1 — Неон, 2 — Минимализм.
inline constexpr std::array<Theme, 3> kThemes{{
    {"Тёмная",
     {28, 28, 32},
     {97, 207, 255},
     {{{97, 207, 255, 220}, {255, 80, 80, 220}}},
     {46, 180, 90}},
    {"Неон",
     {12, 8, 24},
     {180, 80, 255},
     {{{0, 255, 220, 230}, {255, 0, 180, 230}}},
     {0, 230, 140}},
    {"Минимализм",
     {35, 35, 38},
     {160, 160, 165},
     {{{150, 150, 160, 220}, {180, 140, 140, 220}}},
     {190, 190, 195}},
}};

/// Бомба: тёмный круг с белым крестом.
inline constexpr Rgba kBombFill{40, 20, 20, 230};
inline constexpr Rgba kWhite{255, 255, 255};

struct PresetLook {
    Rgba button;
    Rgba circle;   ///< круг на плашке-превью
    Rgba circle2;  ///< второй круг (кнопки по цвету — красный)
};

/// Кнопки наборов в меню — по номеру набора (kPresets). Анимация плашки —
/// своя у каждого набора.
inline constexpr std::array<PresetLook, kPresetCount> kPresetLooks{{
    {{120, 90, 200}, {46, 180, 90}, {46, 180, 90}},
    {{70, 130, 180}, {30, 144, 255}, {30, 144, 255}},
    {{205, 92, 92}, {30, 144, 255}, {220, 20, 60}},
    {{60, 179, 113}, {46, 139, 87}, {46, 139, 87}},
    {{255, 140, 0}, {255, 165, 0}, {255, 165, 0}},
}};
/// Кнопка «Своего набора» (D-37).
inline constexpr Rgba kCustomButton{110, 110, 130};

}  // namespace aim::core
