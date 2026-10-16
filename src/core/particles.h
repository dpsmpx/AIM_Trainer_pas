#pragma once

// Частицы фона меню — перенос TParticle и DrawParticles образца
// (archive/pascal/AIM_Trainer.pas). Отличия — нарочные:
//
//   * скорость — по времени, а не 1 px за кадр (AUD-20): 1 px за кадр при
//     240 кадрах в секунду — пределе образца, то есть 240 px/с;
//   * линии к курсору — до 200 px и гаснут к 200 px: у образца прозрачность
//     считалась от 100 px и за ними оборачивалась в яркую (AUD-13).

#include <array>
#include <optional>

#include "core/random.h"
#include "core/time.h"

namespace aim::core {

inline constexpr int kParticleCount = 100;
/// Линии между частицами — ближе этого, px (LINE_DIST).
inline constexpr double kParticleLink = 100.0;
/// Линии от частиц к курсору — ближе этого, px (LINE_DIST · 2).
inline constexpr double kCursorLink = 2 * kParticleLink;
/// Скорость частицы, px/с.
inline constexpr double kParticleSpeed = 240.0;

struct Particle {
    double x = 0;
    double y = 0;
    double dx = 0;  ///< направление, единичный вектор
    double dy = 0;
};

class Particles {
public:
    /// Случайно по окну, случайное направление — при каждом входе в меню.
    void init(Random& rng);
    /// Сдвиг за dt; от краёв окна отражаются.
    void update(Nanoseconds dt);
    [[nodiscard]] const std::array<Particle, kParticleCount>& items() const { return items_; }

private:
    std::array<Particle, kParticleCount> items_{};
};

/// Прозрачность линии длины dist при пределе limit: 120 у самой частицы,
/// 0 на пределе; дальше предела линии нет.
std::optional<int> link_alpha(double dist, double limit);

}  // namespace aim::core
