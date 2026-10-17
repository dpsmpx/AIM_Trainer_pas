#pragma once

// Помощники отрисовки — одни на все экраны (AUD-47).

#include <format>
#include <string>

#include "core/measure.h"
#include "core/records.h"
#include "core/rounding.h"

namespace aim::app {

/// Round образца — банковское округление: координаты, радиусы, прозрачность.
inline int round_int(double v) {
    return core::round_half_even(v);
}

/// Число с одним знаком после точки: точность, длительность.
inline std::string one_decimal(double v) {
    return std::format("{:.1f}", v);
}

/// Главная мера замера (D-38): у замера на цвет — медиана реакции, мс; у
/// остальных — очки.
inline std::string primary_text(const core::MeasureOptions& options, double value) {
    return core::lower_is_better(options) ? std::format("{:.0f} мс", value)
                                          : std::format("{:.0f}", value);
}

}  // namespace aim::app
