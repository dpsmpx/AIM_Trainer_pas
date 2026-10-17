#include "core/random.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace aim::core {

EngineRandom::EngineRandom(std::uint64_t seed) : engine_(seed) {}

// Свои отображения вместо std::uniform_*_distribution: те по стандарту
// зависят от библиотеки (libstdc++, libc++ и MSVC дают разные числа), а
// замер по записанному началу должен повторяться одинаково везде (D-39).

double EngineRandom::unit() {
    // Старшие 53 бита — вещественное в [0, 1) с шагом 2^-53.
    constexpr double kStep = 1.0 / static_cast<double>(std::uint64_t{1} << 53U);
    return static_cast<double>(engine_() >> 11U) * kStep;
}

int EngineRandom::between(int lo, int hi) {
    if (lo > hi) std::swap(lo, hi);
    const auto span = static_cast<std::uint64_t>(static_cast<std::int64_t>(hi) - lo) + 1;
    // Без перекоса: числа из неполного хвоста (2^64 mod span) отбрасываются.
    const std::uint64_t tail = (std::numeric_limits<std::uint64_t>::max() % span + 1) % span;
    std::uint64_t x = engine_();
    while (tail != 0 && x >= std::uint64_t{0} - tail)
        x = engine_();
    return static_cast<int>(static_cast<std::int64_t>(lo) + static_cast<std::int64_t>(x % span));
}

std::unique_ptr<Random> engine_random(std::uint64_t seed) {
    return std::make_unique<EngineRandom>(seed);
}

}  // namespace aim::core
