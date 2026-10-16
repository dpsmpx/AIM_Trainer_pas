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

Game::Game(GameMode mode, const GameSettings& settings, Random& rng, Nanoseconds start)
    : mode_(mode),
      rules_(mode_rules(mode, settings.lifetime_ms)),
      circle_size_(settings.circle_size),
      max_lives_(settings.lives),
      rng_(rng),
      last_update_(start) {
    respawn(start);
}

void Game::respawn(Nanoseconds now) {
    // Первая цель забега бомбой не бывает (TryTime = 0 у образца).
    target_ = Target::spawn(rules_, circle_size_, target_.center(), spawned_ > 0, now, rng_);
    ++spawned_;
}

void Game::lose_life() {
    ++lost_;
    if (lost_ >= max_lives_) over_ = true;
}

void Game::record_miss(Vec2 point) {
    const int col = heatmap_cell(point.x, kHeatmapCols, kWindowWidth);
    const int row = heatmap_cell(point.y, kHeatmapRows, kWindowHeight);
    ++heatmap_[static_cast<std::size_t>(col)][static_cast<std::size_t>(row)];
}

void Game::click(MouseButton button, Vec2 point, Nanoseconds t) {
    if (over_ || paused_ || rules_.hover) return;
    if (button != MouseButton::Left && button != MouseButton::Right) return;
    ++shots_;
    const HitInfo info = target_.hit(point, button, t);
    if (info.bomb) {
        over_ = true;
        died_to_bomb_ = true;
        events_.push_back({.kind = GameEventKind::Bomb,
                           .position = point,
                           .color = target_.color(),
                           .combo = combo_,
                           .score = score_});
        return;
    }
    if (!info.hit) {
        combo_ = 0;
        ++misses_;
        record_miss(point);
        lose_life();
        events_.push_back({.kind = GameEventKind::Miss,
                           .position = point,
                           .color = target_.color(),
                           .combo = combo_,
                           .score = score_});
        return;
    }

    ++combo_;
    max_combo_ = std::max(max_combo_, combo_);
    const int points = target_.points(combo_multiplier(combo_));
    score_ += points;
    ++hits_;
    // Попадание возможно только по показанной цели (Target::hit), так что
    // shown_at есть и не позже t.
    const Nanoseconds reaction = t - target_.shown_at().value_or(t);
    total_reaction_ += reaction;
    reactions_.push_back(reaction);
    if (reactions_.size() > static_cast<std::size_t>(kReactionHistory)) reactions_.pop_front();
    precision_sum_ += info.accuracy;
    events_.push_back({.kind = GameEventKind::Hit,
                       .position = point,
                       .color = target_.color(),
                       .points = points,
                       .accuracy = info.accuracy,
                       .reaction = reaction,
                       .combo = combo_,
                       .score = score_});
    respawn(t);
}

void Game::update(Nanoseconds now, Vec2 cursor) {
    if (over_ || paused_) return;
    const Nanoseconds dt = now > last_update_ ? now - last_update_ : 0;
    last_update_ = std::max(last_update_, now);

    if (!target_.alive_at(now)) {
        // Истёкшая бомба исчезает без штрафа; обычная цель — минус жизнь и
        // обрыв серии (D-14).
        if (!target_.is_bomb()) {
            combo_ = 0;
            lose_life();
            events_.push_back({.kind = GameEventKind::Expired,
                               .position = target_.center(),
                               .color = target_.color(),
                               .combo = combo_,
                               .score = score_});
        }
        if (!over_) respawn(now);
        return;
    }

    if (rules_.moves) target_.move(dt);
    if (rules_.hover && target_.hover(cursor, dt)) {
        // Наводка — без множителя серии; попадания и выстрелы не считаются:
        // точность и реакция — метрики щелчков (docs/ARCHITECTURE.md, §7).
        const int points = target_.points(1.0);
        score_ += points;
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
    // Под паузой цель не «показывается»: реакция — от кадра после неё.
    if (!over_ && !paused_) target_.mark_shown(frame_time, presented_at);
}

void Game::pause(Nanoseconds now) {
    if (over_ || paused_) return;
    paused_ = true;
    pause_started_ = now;
}

void Game::resume(Nanoseconds now) {
    if (!paused_) return;
    const Nanoseconds by = now > pause_started_ ? now - pause_started_ : 0;
    target_.shift(by);
    last_update_ += by;
    paused_ = false;
}

void Game::give_up() {
    over_ = true;
}

double Game::accuracy() const {
    return shots_ > 0 ? 100.0 * hits_ / shots_ : 0.0;
}

double Game::avg_reaction_ms() const {
    return hits_ > 0 ? to_ms(total_reaction_) / hits_ : 0.0;
}

double Game::precision() const {
    return hits_ > 0 ? 100.0 * precision_sum_ / hits_ : 0.0;
}

std::vector<GameEvent> Game::take_events() {
    return std::exchange(events_, {});
}

GameSummary Game::summary() const {
    GameSummary s;
    s.mode = mode_;
    s.score = score_;
    s.hits = hits_;
    s.shots = shots_;
    s.misses = misses_;
    s.max_combo = max_combo_;
    s.accuracy = accuracy();
    s.avg_reaction_ms = avg_reaction_ms();
    s.precision = precision();
    s.died_to_bomb = died_to_bomb_;
    s.reactions_ms.reserve(reactions_.size());
    for (const Nanoseconds r : reactions_)
        s.reactions_ms.push_back(to_ms(r));
    return s;
}

}  // namespace aim::core
