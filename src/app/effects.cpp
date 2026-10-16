#include "app/effects.h"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>

#include "core/rounding.h"
#include "core/screen.h"

namespace aim::app {

namespace {

// Round образца — банковское округление.
int round_int(double v) {
    return core::round_half_even(v);
}

}  // namespace

void Effects::spawn(Kind kind, double x, double y, double vx, double vy, int duration_ms,
                    core::Rgba color, std::string text, core::Nanoseconds now) {
    if (kind == Kind::Points && !text_) return;
    Effect& e = effects_[next_];
    e = {.active = true,
         .kind = kind,
         .x = x,
         .y = y,
         .vx = vx,
         .vy = vy,
         .start = now,
         .duration = core::ms(static_cast<std::uint64_t>(duration_ms)),
         .color = color,
         .text = std::move(text)};
    // Кольцевой буфер, как у образца: слотов с запасом на короткие эффекты.
    next_ = (next_ + 1) % kSlots;
}

void Effects::hit(int x, int y, core::Rgba color, core::Rgba text, int points, core::Random& rng,
                  core::Nanoseconds now) {
    spawn(Kind::Flash, x, y, 0, 0, 250, color, {}, now);
    spawn(Kind::Wave, x, y, 0, 0, 400, color, {}, now);
    for (int i = 1; i <= 8; ++i) {
        const double angle = (i / 8.0) * 2 * std::numbers::pi + rng.unit() * 0.5;
        // Random(40) образца — целое 0…39.
        const double vx = std::cos(angle) * (60 + rng.between(0, 39));
        const double vy = std::sin(angle) * (60 + rng.between(0, 39));
        spawn(Kind::Shard, x, y, vx, vy, 500, color, {}, now);
    }
    spawn(Kind::Points, x, y, 0, -30, 700, text, "+" + std::to_string(points), now);
}

void Effects::label(int x, int y, std::string text, core::Rgba color, core::Nanoseconds now,
                    int size) {
    if (!text_) return;
    spawn(Kind::Points, x, y, 0, -30, 900, color, std::move(text), now);
    effects_[(next_ + kSlots - 1) % kSlots].size = size;
}

void Effects::precise_flash(core::Nanoseconds now) {
    spawn(Kind::WhiteScreen, 0, 0, 0, 0, 200, {}, {}, now);
}

void Effects::bomb_flash(core::Nanoseconds now) {
    spawn(Kind::RedScreen, 0, 0, 0, 0, 350, {}, {}, now);
}

void Effects::clear() {
    for (Effect& e : effects_)
        e.active = false;
    next_ = 0;
}

void Effects::render(Canvas& c, const core::Theme& theme, core::Nanoseconds now) {
    for (Effect& e : effects_) {
        if (!e.active) continue;
        if (now - e.start >= e.duration) {
            e.active = false;
            continue;
        }
        const double t = static_cast<double>(now - e.start) / static_cast<double>(e.duration);
        switch (e.kind) {
            case Kind::Flash:
                c.brush = with_alpha(e.color, round_int(180 * (1 - t)));
                c.fill_circle(round_int(e.x), round_int(e.y), round_int(6 + 34 * t));
                break;
            case Kind::Wave:
                // Circle образца ещё и заливает текущей кистью — как у него.
                c.pen = with_alpha(theme.wave, round_int(160 * (1 - t)));
                c.pen_width = 2;
                c.circle(round_int(e.x), round_int(e.y), round_int(10 + 50 * t));
                break;
            case Kind::Shard:
                c.brush = with_alpha(e.color, round_int(220 * (1 - t)));
                c.fill_circle(round_int(e.x + e.vx * t), round_int(e.y + e.vy * t + 40 * t * t),
                              round_int(4 * (1 - 0.5 * t)));
                break;
            case Kind::Points: {
                c.font.size = e.size;
                c.font_color = with_alpha(e.color, round_int(255 * (1 - t)));
                c.brush = {0, 0, 0, 0};  // без плашки под текстом
                const int tw = c.text_width(e.text);
                c.text_out(round_int(e.x) - tw / 2, round_int(e.y + e.vy * t) - e.size / 2, e.text);
                break;
            }
            case Kind::WhiteScreen:
                c.brush = with_alpha(theme.flash_hit, round_int(70 * (1 - t)));
                c.fill_rect(0, 0, core::kWindowWidth, core::kWindowHeight);
                break;
            case Kind::RedScreen:
                c.brush = with_alpha(theme.flash_bomb, round_int(140 * (1 - t)));
                c.fill_rect(0, 0, core::kWindowWidth, core::kWindowHeight);
                break;
        }
    }
}

void CursorTrail::add(int x, int y) {
    points_[next_] = {x, y};
    filled_[next_] = true;
    next_ = (next_ + 1) % kLength;
}

void CursorTrail::clear() {
    filled_.fill(false);
    next_ = 0;
}

void CursorTrail::render(Canvas& c, const core::Theme& theme) const {
    constexpr auto n = static_cast<double>(kLength);
    for (std::size_t i = 0; i < kLength; ++i) {
        const std::size_t idx = (next_ + 2 * kLength - 1 - i) % kLength;
        if (!filled_[idx]) continue;
        const double share = 1 - static_cast<double>(i) / n;
        c.brush = with_alpha(theme.trail, round_int(140 * share));
        c.fill_circle(points_[idx].x, points_[idx].y, round_int(5 * share) + 1);
    }
}

}  // namespace aim::app
