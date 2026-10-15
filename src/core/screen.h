#pragma once

// Окно и фон — общие для программы и её проверок (AUD-22: константы не
// дублируются).

#include <cstdint>

namespace aim::core {

/// Окно фиксированного размера: раскладка интерфейса — в пикселях под него (D-02).
constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;

struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

/// Фон темы «Тёмная» оригинала (InitThemes в AIM_Trainer.pas); темы целиком —
/// шаг «Экраны».
constexpr Rgb kDarkBackground{28, 28, 32};

}  // namespace aim::core
