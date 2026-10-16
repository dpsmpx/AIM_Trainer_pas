#include "core/game.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/screen.h"

namespace aim::core {

namespace {

double combo_multiplier(int combo) {
    if (combo >= kComboTier2) return kComboMult2;
    if (combo >= kComboTier1) return kComboMult1;
    return 1.0;
}

int heatmap_cell(double coord, int cells, int size) {
    const auto cell = static_cast<int>(std::floor(coord * cells / size));
    return std::clamp(cell, 0, cells - 1);
}

}  // namespace

double median(std::vector<double> values) {
    if (values.empty()) return 0.0;
    const std::size_t mid = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(mid),
                     values.end());
    const double upper = values[mid];
    if (values.size() % 2 == 1) return upper;
    const double lower =
        *std::max_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(mid));
    return (lower + upper) / 2.0;
}

Game::Game(const MeasureOptions& options, Random& rng, Nanoseconds start)
    : options_(normalized(options)),
      rules_(target_rules(options_)),
      max_lives_(options_.lives),
      rng_(rng),
      start_(start),
      last_update_(start) {
    if (options_.ending == Ending::Time)
        ends_at_ = start + ms(static_cast<std::uint64_t>(options_.duration_s) * 1000);
    if (screen_stimulus()) {
        next_signal(start);
    } else {
        respawn(start);
    }
}

void Game::respawn(Nanoseconds now) {
    // Первая цель замера бомбой не бывает.
    target_ =
        Target::spawn(rules_, options_.target_size, target_.center(), spawned_ > 0, now, rng_);
    ++spawned_;
}

void Game::next_signal(Nanoseconds from) {
    const int delay_ms = rng_.between(static_cast<int>(kScreenMinDelay / ms(1)),
                                      static_cast<int>(kScreenMaxDelay / ms(1)));
    signal_at_ = from + ms(static_cast<std::uint64_t>(delay_ms));
    signal_shown_.reset();
}

void Game::lose_life(Nanoseconds at) {
    if (options_.ending != Ending::Lives) return;
    ++lost_;
    if (lost_ >= max_lives_) end(EndReason::Lives, at);
}

void Game::end(EndReason why, Nanoseconds at) {
    if (over()) return;
    end_ = why;
    ended_at_ = std::max(at, start_);
}

void Game::record_miss(Vec2 point) {
    const int col = heatmap_cell(point.x, kHeatmapCols, kWindowWidth);
    const int row = heatmap_cell(point.y, kHeatmapRows, kWindowHeight);
    ++heatmap_[static_cast<std::size_t>(col)][static_cast<std::size_t>(row)];
}

void Game::hit(Nanoseconds reaction) {
    ++combo_;
    max_combo_ = std::max(max_combo_, combo_);
    ++hits_;
    total_reaction_ += reaction;
    reactions_.push_back(reaction);
}

void Game::click(MouseButton button, Vec2 point, Nanoseconds t) {
    if (over()) return;
    if (options_.ending == Ending::Time && t >= ends_at_) {
        // Щелчок после конца времени в замер не идёт.
        end(EndReason::Time, ends_at_);
        return;
    }
    if (screen_stimulus()) {
        click_screen(button, point, t);
    } else {
        click_target(button, point, t);
    }
}

void Game::click_screen(MouseButton button, Vec2 point, Nanoseconds t) {
    if (button != MouseButton::Left && button != MouseButton::Right &&
        button != MouseButton::Middle)
        return;
    ++shots_;
    if (!signal_shown_ || t < *signal_shown_) {
        // Фальстарт: щелчок раньше, чем смена цвета показана (D-12).
        combo_ = 0;
        ++misses_;
        events_.push_back({.kind = GameEventKind::FalseStart,
                           .position = point,
                           .combo = combo_,
                           .score = score_});
        lose_life(t);
        next_signal(t);
        return;
    }
    const Nanoseconds reaction = t - *signal_shown_;
    hit(reaction);
    events_.push_back({.kind = GameEventKind::Hit,
                       .position = point,
                       .reaction = reaction,
                       .combo = combo_,
                       .score = score_});
    next_signal(t);
}

void Game::click_target(MouseButton button, Vec2 point, Nanoseconds t) {
    if (rules_.hover) return;
    if (button != MouseButton::Left && button != MouseButton::Right) return;
    ++shots_;
    const HitInfo info = target_.hit(point, button, t);
    if (info.bomb) {
        events_.push_back({.kind = GameEventKind::Bomb,
                           .position = point,
                           .color = target_.color(),
                           .combo = combo_,
                           .score = score_});
        end(EndReason::Bomb, t);
        return;
    }
    if (!info.hit) {
        combo_ = 0;
        ++misses_;
        record_miss(point);
        events_.push_back({.kind = GameEventKind::Miss,
                           .position = point,
                           .color = target_.color(),
                           .combo = combo_,
                           .score = score_});
        lose_life(t);
        return;
    }

    // Попадание возможно только по показанной цели (Target::hit), так что
    // shown_at есть и не позже t.
    hit(t - target_.shown_at().value_or(t));
    const int points = target_.points(combo_multiplier(combo_));
    score_ += points;
    precision_sum_ += info.accuracy;
    events_.push_back({.kind = GameEventKind::Hit,
                       .position = point,
                       .color = target_.color(),
                       .points = points,
                       .accuracy = info.accuracy,
                       .reaction = reactions_.back(),
                       .combo = combo_,
                       .score = score_});
    respawn(t);
}

void Game::update(Nanoseconds now, Vec2 cursor) {
    if (over()) return;
    if (options_.ending == Ending::Time && now >= ends_at_) {
        end(EndReason::Time, ends_at_);
        return;
    }
    const Nanoseconds dt = now > last_update_ ? now - last_update_ : 0;
    last_update_ = std::max(last_update_, now);
    if (screen_stimulus()) return;

    if (!target_.alive_at(now)) {
        // Истёкшая бомба исчезает без штрафа; обычная цель — обрыв серии и,
        // по жизням, минус жизнь (D-14).
        if (!target_.is_bomb()) {
            combo_ = 0;
            ++expired_;
            events_.push_back({.kind = GameEventKind::Expired,
                               .position = target_.center(),
                               .color = target_.color(),
                               .combo = combo_,
                               .score = score_});
            lose_life(now);
        }
        if (!over()) respawn(now);
        return;
    }

    if (rules_.moves) target_.move(dt);
    if (rules_.hover && target_.hover(cursor, dt)) {
        // Наведение — без множителя серии; попадания и выстрелы не считаются:
        // точность и реакция — меры щелчков.
        const int points = target_.points(1.0);
        score_ += points;
        ++tracked_;
        events_.push_back({.kind = GameEventKind::Tracked,
                           .position = target_.center(),
                           .color = target_.color(),
                           .points = points,
                           .combo = combo_,
                           .score = score_});
        respawn(now);
    }
}

void Game::frame_presented(Nanoseconds frame_time, Nanoseconds presented_at) {
    if (over()) return;
    if (screen_stimulus()) {
        if (!signal_shown_ && signal_lit(frame_time)) signal_shown_ = presented_at;
        return;
    }
    target_.mark_shown(frame_time, presented_at);
}

void Game::finish(EndReason why, Nanoseconds now) {
    end(why, now);
}

Nanoseconds Game::time_left(Nanoseconds now) const {
    if (options_.ending != Ending::Time) return 0;
    const Nanoseconds at = over() ? ended_at_ : now;
    return at < ends_at_ ? ends_at_ - at : 0;
}

double Game::accuracy() const {
    return shots_ > 0 ? 100.0 * hits_ / shots_ : 0.0;
}

double Game::avg_reaction_ms() const {
    return hits_ > 0 ? to_ms(total_reaction_) / hits_ : 0.0;
}

double Game::median_reaction_ms() const {
    std::vector<double> values;
    values.reserve(reactions_.size());
    for (const Nanoseconds r : reactions_)
        values.push_back(to_ms(r));
    return median(std::move(values));
}

double Game::precision() const {
    return hits_ > 0 && !screen_stimulus() ? 100.0 * precision_sum_ / hits_ : 0.0;
}

std::vector<GameEvent> Game::take_events() {
    return std::exchange(events_, {});
}

GameSummary Game::summary() const {
    GameSummary s;
    s.options = options_;
    s.score = score_;
    s.hits = hits_;
    s.shots = shots_;
    s.misses = misses_;
    s.expired = expired_;
    s.tracked = tracked_;
    s.max_combo = max_combo_;
    s.accuracy = accuracy();
    s.avg_reaction_ms = avg_reaction_ms();
    s.median_reaction_ms = median_reaction_ms();
    s.precision = precision();
    s.end = end_;
    const Nanoseconds until = over() ? ended_at_ : last_update_;
    s.duration_ms = static_cast<int>((until > start_ ? until - start_ : 0) / ms(1));
    s.reactions_ms.reserve(reactions_.size());
    for (const Nanoseconds r : reactions_)
        s.reactions_ms.push_back(to_ms(r));
    return s;
}

}  // namespace aim::core
