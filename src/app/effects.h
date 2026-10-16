#pragma once

// Эффекты попадания и след курсора — перенос SpawnEffect, SpawnHitEffects,
// RenderEffects, UpdateCursorTrail и RenderCursorTrail образца
// (archive/pascal/AIM_Trainer.pas). Вспышки на весь экран — заливка окна, а
// не FillRoundRect с нулевым скруглением, на котором образец падает
// (сценарий 22).

#include <array>
#include <string>

#include "app/canvas.h"
#include "core/palette.h"
#include "core/random.h"
#include "core/time.h"

namespace aim::app {

class Effects {
public:
    /// Попадание в точке (x, y) цветом цели: вспышка, волна, восемь осколков
    /// и «+очки» — SpawnHitEffects.
    void hit(int x, int y, core::Rgba color, int points, core::Random& rng, core::Nanoseconds now);
    /// Белая вспышка экрана — точное попадание (точность от 95 %).
    void precise_flash(core::Nanoseconds now);
    /// Красная вспышка экрана — подрыв на бомбе.
    void bomb_flash(core::Nanoseconds now);
    /// Новый забег: всё погасить.
    void clear();

    void render(Canvas& canvas, core::Nanoseconds now);

private:
    enum class Kind : unsigned char { Flash, Wave, Shard, Points, WhiteScreen, RedScreen };
    struct Effect {
        bool active = false;
        Kind kind = Kind::Flash;
        double x = 0;
        double y = 0;
        double vx = 0;
        double vy = 0;
        core::Nanoseconds start = 0;
        core::Nanoseconds duration = 0;
        core::Rgba color;
        std::string text;
    };
    static constexpr std::size_t kSlots = 100;  // MAX_EFFECTS

    void spawn(Kind kind, double x, double y, double vx, double vy, int duration_ms,
               core::Rgba color, std::string text, core::Nanoseconds now);

    std::array<Effect, kSlots> effects_{};
    std::size_t next_ = 0;
};

/// След курсора во время игры — 12 последних кадров (TRAIL_LENGTH).
class CursorTrail {
public:
    /// Кадр игры (не на паузе): запомнить курсор.
    void add(int x, int y);
    void clear();
    void render(Canvas& canvas) const;

private:
    static constexpr std::size_t kLength = 12;
    std::array<IntPoint, kLength> points_{};
    std::array<bool, kLength> filled_{};
    std::size_t next_ = 0;
};

}  // namespace aim::app
