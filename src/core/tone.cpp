#include "core/tone.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace aim::core {

std::vector<float> tone(Beep beep, int sample_rate, float volume) {
    const auto count =
        static_cast<std::size_t>(std::max(0, sample_rate) * std::max(0, beep.duration_ms) / 1000);
    std::vector<float> samples(count);
    const auto fade = static_cast<std::size_t>(sample_rate / 500);  // 2 мс
    const double step = 2.0 * std::numbers::pi * beep.frequency_hz / sample_rate;
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t edge = std::min(i, count - 1 - i);
        const double envelope =
            fade > 0 && edge < fade ? static_cast<double>(edge) / static_cast<double>(fade) : 1.0;
        samples[i] =
            static_cast<float>(volume * envelope * std::sin(step * static_cast<double>(i)));
    }
    return samples;
}

}  // namespace aim::core
