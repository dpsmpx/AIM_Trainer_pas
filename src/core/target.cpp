#include "core/target.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/rounding.h"
#include "core/rules.h"
#include "core/screen.h"

namespace aim::core {

namespace {

double seconds(Nanoseconds value) {
    return static_cast<double>(value) / 1e9;
}

}  // namespace

Target Target::spawn(const ModeRules& rules, int max_radius, Vec2 prev, bool allow_bomb,
                     Nanoseconds now, Random& rng) {
    Target target;
    target.rules_ = rules;
    target.size_factor_ = kMinSizeFactor + rng.unit() * (kMaxSizeFactor - kMinSizeFactor);
    target.peak_radius_ = round_half_even(max_radius * target.size_factor_);

    const int r = target.peak_radius_;
    const double min_dist_sq = 4.0 * r * r;
    for (int attempt = 1;; ++attempt) {
        const int x = rng.between(r, kWindowWidth - r);
        const int y = rng.between(r + rules.top_reserve, kWindowHeight - r - kHudReserve);
        target.center_ = {static_cast<double>(x), static_cast<double>(y)};
        const double dx = target.center_.x - prev.x;
        const double dy = target.center_.y - prev.y;
        if (dx * dx + dy * dy >= min_dist_sq || attempt >= kSpawnAttempts) break;
    }

    target.color_ = rules.two_buttons ? rng.between(0, 1) : 0;
    target.bomb_ = allow_bomb && rules.bombs && rng.unit() < kBombChance;
    if (rules.moves) {
        const double angle = rng.unit() * 2.0 * std::numbers::pi;
        const auto speed = static_cast<double>(rng.between(kTrackingMinSpeed, kTrackingMaxSpeed));
        target.velocity_ = {std::cos(angle) * speed, std::sin(angle) * speed};
    }
    target.spawned_at_ = now;
    target.expires_at_ = now + rules.lifetime;
    return target;
}

double Target::radius_at(Nanoseconds t) const {
    if (t < spawned_at_ || t >= expires_at_) return 0.0;
    const auto peak = static_cast<double>(peak_radius_);
    if (!rules_.grows) return peak;
    // Треугольник, как CurrentRadius образца: пик — в середине жизни.
    const auto life = static_cast<double>(rules_.lifetime);
    const auto age = static_cast<double>(t - spawned_at_);
    const double half = life / 2.0;
    const double rise = age <= half ? age / half : (life - age) / half;
    return std::max(0.0, peak * rise);
}

HitInfo Target::hit(Vec2 p, MouseButton button, Nanoseconds t) const {
    HitInfo info;
    if (!shown_at_ || t < *shown_at_ || !alive_at(t)) return info;
    const double r = radius_at(t);
    if (r <= 0.0) return info;
    const double dist = std::hypot(p.x - center_.x, p.y - center_.y);
    if (dist > r) return info;
    if (bomb_) {
        // Бомба срабатывает от любой кнопки: сам щелчок по ней — ошибка.
        info.hit = true;
        info.bomb = true;
        return info;
    }
    const MouseButton needed =
        rules_.two_buttons && color_ == 1 ? MouseButton::Right : MouseButton::Left;
    if (button != needed) return info;
    info.hit = true;
    info.accuracy = std::clamp(1.0 - dist / r, 0.0, 1.0);
    return info;
}

int Target::points(double multiplier) const {
    return round_half_even(kBasePoints / size_factor_ * multiplier);
}

void Target::move(Nanoseconds dt) {
    const double s = seconds(dt);
    center_.x += velocity_.x * s;
    center_.y += velocity_.y * s;
    // Граница — по своему радиусу и выше HUD: цель целиком в окне (AUD-18).
    const auto r = static_cast<double>(peak_radius_);
    const double left = r;
    const double right = kWindowWidth - r;
    const double top = r;
    const double bottom = kWindowHeight - r - kHudReserve;
    if (center_.x < left) velocity_.x = std::abs(velocity_.x);
    if (center_.x > right) velocity_.x = -std::abs(velocity_.x);
    if (center_.y < top) velocity_.y = std::abs(velocity_.y);
    if (center_.y > bottom) velocity_.y = -std::abs(velocity_.y);
    center_.x = std::clamp(center_.x, left, right);
    center_.y = std::clamp(center_.y, top, bottom);
}

bool Target::hover(Vec2 cursor, Nanoseconds dt) {
    const double dist = std::hypot(cursor.x - center_.x, cursor.y - center_.y);
    if (dist <= static_cast<double>(peak_radius_)) {
        hover_ += dt;
    } else {
        hover_ = hover_ > 2 * dt ? hover_ - 2 * dt : 0;
    }
    return hover_ >= kTrackingHold;
}

void Target::mark_shown(Nanoseconds frame_time, Nanoseconds presented_at) {
    if (!shown_at_ && radius_at(frame_time) > 0.0) shown_at_ = presented_at;
}

void Target::shift(Nanoseconds by) {
    spawned_at_ += by;
    expires_at_ += by;
    if (shown_at_) *shown_at_ += by;
}

}  // namespace aim::core
