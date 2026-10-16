#pragma once

// Цвета и подписи, которые рисует программа: темы (InitThemes образца) и
// кнопки режимов (InitModes). Одно место для отрисовки и проверок (AUD-22).

#include <array>
#include <cstdint>
#include <string_view>

#include "core/modes.h"

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
};

/// Темы: 0 — Тёмная, 1 — Неон, 2 — Минимализм.
inline constexpr std::array<Theme, 3> kThemes{{
    {"Тёмная", {28, 28, 32}, {97, 207, 255}, {{{97, 207, 255, 220}, {255, 80, 80, 220}}}},
    {"Неон", {12, 8, 24}, {180, 80, 255}, {{{0, 255, 220, 230}, {255, 0, 180, 230}}}},
    {"Минимализм", {35, 35, 38}, {160, 160, 165}, {{{150, 150, 160, 220}, {180, 140, 140, 220}}}},
}};

/// Бомба: тёмный круг с белым крестом (TCircle.Render).
inline constexpr Rgba kBombFill{40, 20, 20, 230};
inline constexpr Rgba kWhite{255, 255, 255};

struct ModeLook {
    std::string_view title;  ///< подпись кнопки меню
    Rgba button;
};

/// Кнопки режимов в меню — по номеру режима (InitModes).
inline constexpr std::array<ModeLook, kModeCount> kModeLooks{{
    {"Classic Mode", {70, 130, 180}},
    {"Dual Mode", {205, 92, 92}},
    {"Tracking", {60, 179, 113}},
    {"Flick Shot", {255, 140, 0}},
}};

}  // namespace aim::core
