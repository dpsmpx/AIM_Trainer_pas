#include "core/click_time.h"

#include <algorithm>
#include <cstddef>

namespace aim::core {

namespace {

Nanoseconds saturating_sub(Nanoseconds a, Nanoseconds b) {
    return a > b ? a - b : 0;
}

}  // namespace

void ClickTimer::add(const RawPress& press) {
    // Поток отдаёт нажатия по порядку; если порядок всё же нарушен, нажатие
    // встаёт на своё место, и поиск ниже остаётся верным.
    const auto at = std::upper_bound(pending_.begin(), pending_.end(), press.time,
                                     [](Nanoseconds t, const RawPress& p) { return t < p.time; });
    pending_.insert(at, press);
    if (pending_.size() > kCapacity) {
        const auto extra = static_cast<std::ptrdiff_t>(pending_.size() - kCapacity);
        pending_.erase(pending_.begin(), pending_.begin() + extra);
    }
}

ClickTime ClickTimer::resolve(MouseButton button, Nanoseconds event_time) {
    const Nanoseconds earliest = saturating_sub(event_time, kRawBeforeEvent);
    const Nanoseconds latest = event_time + kRawAfterEvent;

    const auto match = std::find_if(pending_.begin(), pending_.end(), [&](const RawPress& p) {
        return p.button == button && p.time >= earliest && p.time <= latest;
    });
    ClickTime result{event_time, ClickTimeSource::Event};
    if (match != pending_.end()) {
        result = {match->time, ClickTimeSource::RawInput};
        pending_.erase(match);
    }
    // Нажатия той же кнопки раньше окна — клики, которых SDL не видел; со
    // следующими событиями этой кнопки они тоже не совпадут.
    std::erase_if(pending_,
                  [&](const RawPress& p) { return p.button == button && p.time < earliest; });
    return result;
}

void ClickTimer::forget_old(Nanoseconds now) {
    const Nanoseconds oldest = saturating_sub(now, kMaxAge);
    std::erase_if(pending_, [&](const RawPress& p) { return p.time < oldest; });
}

}  // namespace aim::core
