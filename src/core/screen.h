#pragma once

// Окно и фон — общие для программы и её проверок (AUD-22: константы не
// дублируются).

#include "core/palette.h"

namespace aim::core {

/// Окно фиксированного размера: раскладка интерфейса — в пикселях под него (D-02).
constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;

/// Фон темы «Тёмная» — темы по умолчанию.
inline constexpr Rgba kDarkBackground = kThemes[0].background;

}  // namespace aim::core
