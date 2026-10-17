#pragma once

// Эффекты попадания и след курсора — перенос SpawnEffect, SpawnHitEffects,
// RenderEffects, UpdateCursorTrail и RenderCursorTrail образца
// (archive/pascal/AIM_Trainer.pas). Вспышек всего окна нет (D-48): серия
// точных попаданий давала больше трёх вспышек в секунду.

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
    /// и «+очки» цветом text — SpawnHitEffects.
    void hit(int x, int y, core::Rgba color, core::Rgba text, int points, core::Random& rng,
             core::Nanoseconds now);
    /// Надпись text в точке (x, y), всплывает и гаснет: реакция или
    /// фальстарт замера на цвет.
    void label(int x, int y, std::string text, core::Rgba color, core::Nanoseconds now,
               int size = kHeadlineSize);
    /// Новый забег: всё погасить.
    void clear();
    /// Надписи («+очки», реакция, «Фальстарт»): выключены — не появляются,
    /// осколки и вспышка в точке попадания остаются (интерфейс замера, D-45).
    void show_text(bool on) { text_ = on; }

    /// Волна — цветом темы (D-44).
    void render(Canvas& canvas, const core::Theme& theme, core::Nanoseconds now);

private:
    enum class Kind : unsigned char { Flash, Wave, Shard, Points };
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
        int size = kTextSize;  ///< шрифт надписи
    };
    static constexpr std::size_t kSlots = 100;  // MAX_EFFECTS

    void spawn(Kind kind, double x, double y, double vx, double vy, int duration_ms,
               core::Rgba color, std::string text, core::Nanoseconds now);

    std::array<Effect, kSlots> effects_{};
    std::size_t next_ = 0;
    bool text_ = true;
};

/// След курсора во время игры — 12 последних кадров (TRAIL_LENGTH).
class CursorTrail {
public:
    /// Кадр игры (не на паузе): запомнить курсор.
    void add(int x, int y);
    void clear();
    void render(Canvas& canvas, const core::Theme& theme) const;

private:
    static constexpr std::size_t kLength = 12;
    std::array<IntPoint, kLength> points_{};
    std::array<bool, kLength> filled_{};
    std::size_t next_ = 0;
};

}  // namespace aim::app
