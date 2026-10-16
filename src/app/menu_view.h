#pragma once

// Главное меню и панели поверх него на холсте app/canvas.h: наборы замера
// (D-37), рекорд и среднее за N выбранного набора (D-38), график его
// последних замеров, рекорды всех наборов, история замеров с параметрами.
// Что открыто и что выбрано — core/menu.h.

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/canvas.h"
#include "app/toast.h"
#include "core/menu.h"
#include "core/mouse.h"
#include "core/palette.h"
#include "core/particles.h"
#include "core/profile.h"
#include "core/random.h"
#include "core/time.h"

namespace aim::app {

/// Плавная кривая через точки — DrawSmoothCurve образца (кубические кривые
/// Безье, DrawBezierLine из Base.pas).
void draw_smooth_curve(Canvas& c, std::span<const IntPoint> pts, core::Rgba color, int width);

class MenuView {
public:
    /// Меню показано: частицы заново, анимация — с нуля.
    void enter(core::Random& rng, core::Nanoseconds now);
    /// Кадр меню: частицы и часы анимации.
    void update(core::Nanoseconds now);
    /// Меню целиком: фон, частицы, плашка, кнопки наборов, описание,
    /// график, рекорд набора, кнопки внизу, открытая панель, туториал,
    /// уведомления — поверх всего (AUD-33).
    /// data_folder — папка сохранений для панели настроек (D-33).
    void render(Canvas& canvas, Toasts& toasts, const core::Theme& theme, const core::Menu& menu,
                const core::Profile& profile, std::string_view data_folder, core::Vec2 mouse,
                core::Nanoseconds now) const;

private:
    void render_particles(Canvas& canvas, const core::Profile& profile, core::Vec2 mouse) const;
    void render_plate(Canvas& canvas, const core::Theme& theme, int preset, core::Vec2 mouse) const;

    core::Particles particles_;
    double anim_ms_ = 0;  ///< MenuAnimTimer: время в меню, мс
    core::Nanoseconds last_ = 0;
};

}  // namespace aim::app
