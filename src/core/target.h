#pragma once

// Цель — перенос TCircle образца (archive/pascal/AimCircle.pas,
// docs/ARCHITECTURE.md, §5). Отличия — нарочные:
//
//   * время — абсолютное на часах программы, а не остаток жизни: радиус и
//     попадание считаются на момент щелчка, а не обработки (AUD-03);
//   * координаты вещественные, граница Tracking — по своему радиусу и выше
//     HUD (AUD-04, AUD-18);
//   * засчитать можно только показанную цель: реакция — от первого
//     показанного кадра с ней (D-12);
//   * Tracking и Flick — по D-21 (правила режима — core/modes.h).

#include <optional>

#include "core/modes.h"
#include "core/mouse.h"
#include "core/random.h"
#include "core/time.h"

namespace aim::core {

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

/// Итог щелчка по цели.
struct HitInfo {
    bool hit = false;  ///< попадание — нужной кнопкой или по бомбе любой
    bool bomb = false;  ///< щелчок по бомбе
    double accuracy = 0.0;  ///< точность попадания 1 − расстояние/радиус, 0…1
};

class Target {
public:
    /// Новая цель в момент now. prev — центр прежней цели: новая — не ближе
    /// двух своих радиусов, до kSpawnAttempts попыток. allow_bomb — можно ли
    /// этой цели быть бомбой (не первая цель забега; режим решает сам).
    ///
    /// Порядок обращений к rng (для тестов): unit — масштаб; between x, y —
    /// место, по паре на попытку; between(0, 1) — цвет (Dual); unit — бомба
    /// (если режим и allow_bomb разрешают); unit — направление и between —
    /// скорость (Tracking).
    static Target spawn(const ModeRules& rules, int max_radius, Vec2 prev, bool allow_bomb,
                        Nanoseconds now, Random& rng);

    /// Радиус в момент t: растущей цели — треугольник 0 → пик → 0 за жизнь,
    /// остальным — пик; до появления и после жизни — 0.
    [[nodiscard]] double radius_at(Nanoseconds t) const;
    [[nodiscard]] bool alive_at(Nanoseconds t) const { return t < expires_at_; }

    /// Щелчок кнопкой button в точку p в момент t. Цель засчитывается, только
    /// если показана к моменту t и ещё жива.
    [[nodiscard]] HitInfo hit(Vec2 p, MouseButton button, Nanoseconds t) const;

    /// Очки за попадание с множителем серии: round(13 / масштаб × mult),
    /// половина — к чётному, как у образца.
    [[nodiscard]] int points(double multiplier) const;

    /// Tracking: сдвиг за dt; от края окна и HUD отскакивает по своему радиусу.
    void move(Nanoseconds dt);

    /// Tracking: курсор за dt. На цели наводка копится, вне — тает вдвое
    /// быстрее; true — набрано kTrackingHold, цель засчитана.
    bool hover(Vec2 cursor, Nanoseconds dt);

    /// Кадр, нарисованный на момент frame_time, показан в presented_at: если
    /// цель в нём видна впервые — отсюда реакция (D-12).
    void mark_shown(Nanoseconds frame_time, Nanoseconds presented_at);

    /// Сдвинуть все моменты цели на by — время паузы не идёт в жизнь цели.
    void shift(Nanoseconds by);

    [[nodiscard]] Vec2 center() const { return center_; }
    [[nodiscard]] Vec2 velocity() const { return velocity_; }
    [[nodiscard]] double size_factor() const { return size_factor_; }
    [[nodiscard]] int peak_radius() const { return peak_radius_; }
    /// 0 — синяя (ЛКМ), 1 — красная (ПКМ, только Dual).
    [[nodiscard]] int color() const { return color_; }
    [[nodiscard]] bool is_bomb() const { return bomb_; }
    [[nodiscard]] Nanoseconds spawned_at() const { return spawned_at_; }
    [[nodiscard]] Nanoseconds expires_at() const { return expires_at_; }
    [[nodiscard]] std::optional<Nanoseconds> shown_at() const { return shown_at_; }
    /// Tracking: накопленная наводка.
    [[nodiscard]] Nanoseconds hover_progress() const { return hover_; }

private:
    ModeRules rules_;
    Vec2 center_;
    Vec2 velocity_;  // px/с
    double size_factor_ = 1.0;
    int peak_radius_ = 0;
    int color_ = 0;
    bool bomb_ = false;
    Nanoseconds spawned_at_ = 0;
    Nanoseconds expires_at_ = 0;
    std::optional<Nanoseconds> shown_at_;
    Nanoseconds hover_ = 0;
};

}  // namespace aim::core
