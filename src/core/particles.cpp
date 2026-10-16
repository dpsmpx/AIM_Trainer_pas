#include "core/particles.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/rounding.h"
#include "core/screen.h"

namespace aim::core {

void Particles::init(Random& rng) {
    for (Particle& p : items_) {
        p.x = rng.unit() * kWindowWidth;
        p.y = rng.unit() * kWindowHeight;
        const double angle = rng.unit() * 2 * std::numbers::pi;
        p.dx = std::cos(angle);
        p.dy = std::sin(angle);
    }
}

void Particles::update(Nanoseconds dt) {
    const double step = kParticleSpeed * static_cast<double>(dt) / 1e9;
    constexpr double kW = kWindowWidth;
    constexpr double kH = kWindowHeight;
    for (Particle& p : items_) {
        p.x += p.dx * step;
        p.y += p.dy * step;
        if (p.x < 0 || p.x > kW) p.dx = -p.dx;
        if (p.y < 0 || p.y > kH) p.dy = -p.dy;
        p.x = std::clamp(p.x, 0.0, kW);
        p.y = std::clamp(p.y, 0.0, kH);
    }
}

std::optional<int> link_alpha(double dist, double limit) {
    if (dist >= limit) return std::nullopt;
    return round_half_even(120 * (1 - dist / limit));
}

}  // namespace aim::core
