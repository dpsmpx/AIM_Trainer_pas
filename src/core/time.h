#pragma once

// Время логики без окна. Часов логика сама не читает: время ей передают
// (D-20, D-25).

#include <cstdint>

namespace aim::core {

/// Время на часах программы, нс. В программе — SDL_GetTicksNS: те же часы у
/// показа кадра, у события SDL и у потока сырого ввода (D-12, D-24).
using Nanoseconds = std::uint64_t;

inline constexpr Nanoseconds kNsPerMs = 1'000'000;

/// Миллисекунды — в наносекунды.
constexpr Nanoseconds ms(std::uint64_t value) {
    return value * kNsPerMs;
}

/// Наносекунды — в миллисекунды с дробью.
constexpr double to_ms(Nanoseconds value) {
    return static_cast<double>(value) / static_cast<double>(kNsPerMs);
}

}  // namespace aim::core
