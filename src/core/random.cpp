#include "core/random.h"

#include <utility>

namespace aim::core {

EngineRandom::EngineRandom(std::uint64_t seed) : engine_(seed) {}

double EngineRandom::unit() {
    return std::uniform_real_distribution<double>(0.0, 1.0)(engine_);
}

int EngineRandom::between(int lo, int hi) {
    if (lo > hi) std::swap(lo, hi);
    return std::uniform_int_distribution<int>(lo, hi)(engine_);
}

}  // namespace aim::core
