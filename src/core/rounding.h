#pragma once

// Округление, как Round в PascalABC.NET: Convert.ToInt32 — банковское,
// половина к чётному (archive/pascal/ARCHITECTURE.md, §11; docs/PORTING.md, §4).
// std::lround округлял бы половину от нуля: Round(2.5) у образца — 2.

#include <cmath>

namespace aim::core {

/// Ближайшее целое, половина — к чётному: 0,5 → 0, 1,5 → 2, 2,5 → 2.
inline int round_half_even(double value) {
    // std::nearbyint в режиме округления по умолчанию (к ближайшему, половина
    // к чётному); программа этот режим не меняет.
    return static_cast<int>(std::nearbyint(value));
}

}  // namespace aim::core
