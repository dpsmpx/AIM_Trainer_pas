#pragma once

// Кнопки мыши — общие для времени клика (D-24) и правил попадания.

#include <cstdint>

namespace aim::core {

/// Логическая кнопка мыши — после перестановки кнопок для левши, как в
/// событиях SDL.
enum class MouseButton : std::uint8_t { Left, Middle, Right, X1, X2 };

}  // namespace aim::core
