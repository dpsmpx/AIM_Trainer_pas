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

void Toasts::render(Canvas& c, const core::Theme& theme, core::Nanoseconds now) {
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

    constexpr int kW = core::kWindowWidth;
    constexpr int kPad = 20;
    // Не шире окна (D-43): длинное — переносом, плашка — по строкам.
    const int tw = std::clamp(c.text_width(text_, kText) + 2 * kPad, 240, kW - 40);
    const int th =
        std::max(40, c.text_lines(text_, kText, tw - 2 * kPad) * c.line_height(kText) + 16);
    const double share = alpha / 255.0;
    c.fill_round_rect((kW - tw) / 2, 20, (kW + tw) / 2, 20 + th, 10, 10, faded(theme.toast, share));
    c.stroke_round_rect((kW - tw) / 2, 20, (kW + tw) / 2, 20 + th, 10, 10,
                        {.color = faded(theme.gold, share), .width = 2});
    c.text_centered((kW - tw) / 2 + kPad, 20, (kW + tw) / 2 - kPad, 20 + th, text_, kText,
                    faded(theme.toast_text, share));
}

}  // namespace aim::app
