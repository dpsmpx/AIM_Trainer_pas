#include "core/frame_budget.h"

#include <algorithm>
#include <format>

namespace aim::core {

void FrameBudget::start(Nanoseconds period) {
    period_ = period;
    work_.clear();
}

void FrameBudget::add(Nanoseconds work) {
    work_.push_back(work);
}

FrameBudgetReport FrameBudget::report() const {
    FrameBudgetReport r{.frames = work_.size(), .period = period_};
    if (work_.empty()) return r;
    std::vector<Nanoseconds> sorted = work_;
    std::sort(sorted.begin(), sorted.end());
    const auto at = [&](double share) {
        const auto i = static_cast<std::size_t>(share * static_cast<double>(sorted.size() - 1));
        return sorted.at(i);
    };
    r.median = at(0.5);
    r.p99 = at(0.99);
    r.max = sorted.back();
    r.over_half = static_cast<std::size_t>(std::count_if(
        sorted.begin(), sorted.end(), [&](Nanoseconds w) { return w > period_ / 2; }));
    return r;
}

std::string describe(const FrameBudgetReport& r) {
    return std::format("кадр замера: 99 % — {:.2f} мс при запасе {:.2f} мс, дольше — {} из {}",
                       to_ms(r.p99), to_ms(r.period / 2), r.over_half, r.frames);
}

}  // namespace aim::core
