#pragma once

// Округление, как Round в PascalABC.NET: Convert.ToInt32 — банковское,
// половина к чётному (archive/pascal/ARCHITECTURE.md, §11; docs/PORTING.md, §4).
// std::lround округлял бы половину от нуля: Round(2.5) у образца — 2.

#include <cmath>
#include <limits>

namespace aim::core {

/// Ближайшее целое, половина — к чётному: 0,5 → 0, 1,5 → 2, 2,5 → 2. Вне
/// диапазона int — его край, NaN — 0: приведение такого числа к int —
/// неопределённое поведение, а число может прийти из файла (AUD-50).
inline int round_half_even(double value) {
    if (std::isnan(value)) return 0;
    // std::nearbyint в режиме округления по умолчанию (к ближайшему, половина
    // к чётному); программа этот режим не меняет.
    const double rounded = std::nearbyint(value);
    constexpr auto kMax = static_cast<double>(std::numeric_limits<int>::max());
    constexpr auto kMin = static_cast<double>(std::numeric_limits<int>::min());
    if (rounded >= kMax) return std::numeric_limits<int>::max();
    if (rounded <= kMin) return std::numeric_limits<int>::min();
    return static_cast<int>(rounded);
}

}  // namespace aim::core
