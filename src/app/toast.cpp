#include "app/toast.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "core/rounding.h"
#include "core/screen.h"

namespace aim::app {

namespace {

constexpr core::Nanoseconds kShown = core::ms(3000);
constexpr core::Nanoseconds kFade = core::ms(300);

}  // namespace

void Toasts::push(std::string text) {
    queue_.push_back(std::move(text));
}

void Toasts::render(Canvas& c, core::Nanoseconds now) {
    if (active_ && now - start_ > kShown) active_ = false;
    if (!active_) {
        if (queue_.empty()) return;
        text_ = std::move(queue_.front());
        queue_.pop_front();
        start_ = now;
        active_ = true;
    }
    const core::Nanoseconds elapsed = now - start_;
    int alpha = 255;
    if (elapsed < kFade) {
        alpha = core::round_half_even(255.0 * static_cast<double>(elapsed) /
                                      static_cast<double>(kFade));
    } else if (elapsed > kShown - kFade) {
        alpha = core::round_half_even(255.0 * static_cast<double>(kShown - elapsed) /
                                      static_cast<double>(kFade));
    }
    alpha = std::clamp(alpha, 0, 255);
    const auto a = [](int v) { return static_cast<std::uint8_t>(v); };

    constexpr int kW = core::kWindowWidth;
    c.font.size = 13;
    const int tw = std::max(c.text_width(text_) + 40, 240);
    c.brush = {60, 50, 20, a(core::round_half_even(220.0 * alpha / 255))};
    c.pen = {255, 210, 90, a(alpha)};
    c.pen_width = 2;
    c.fill_round_rect((kW - tw) / 2, 20, (kW + tw) / 2, 60, 10, 10);
    c.font_color = {255, 230, 160, a(alpha)};
    c.draw_text_centered((kW - tw) / 2, 20, (kW + tw) / 2, 60, text_);
}

}  // namespace aim::app
