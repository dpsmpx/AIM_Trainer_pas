#pragma once

// Главное меню и панели поверх него — перенос RenderMainMenu, DrawModePlate,
// DrawModeButtons, DrawModeDescription, DrawGraph, DrawParticles и
// Render*Overlay образца (archive/pascal/AIM_Trainer.pas) на холсте
// app/canvas.h. Что открыто и что выбрано — core/menu.h.

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/canvas.h"
#include "app/toast.h"
#include "core/history.h"
#include "core/menu.h"
#include "core/mouse.h"
#include "core/palette.h"
#include "core/particles.h"
#include "core/profile.h"
#include "core/random.h"
#include "core/time.h"

namespace aim::app {

/// Что показывает «История»: читается при открытии панели.
struct HistoryView {
    int games = 0;
    std::array<core::TimeOfDayScore, 4> by_time_of_day{};
    std::vector<double> recent_reactions;  ///< средняя реакция последних игр, мс
    std::string file_name;                 ///< файл истории профиля
};

/// Плавная кривая через точки — DrawSmoothCurve образца (кубические кривые
/// Безье, DrawBezierLine из Base.pas).
void draw_smooth_curve(Canvas& c, std::span<const IntPoint> pts, core::Rgba color, int width);

class MenuView {
public:
    /// Меню показано: частицы заново, анимация — с нуля.
    void enter(core::Random& rng, core::Nanoseconds now);
    /// Кадр меню: частицы и часы анимации.
    void update(core::Nanoseconds now);
    /// Меню целиком, в порядке образца: фон, частицы, плашка, кнопки,
    /// описание, график, рекорды, кнопки внизу, уведомление, открытая панель,
    /// туториал.
    /// data_folder — папка сохранений для панели настроек (D-33).
    void render(Canvas& canvas, Toasts& toasts, const core::Theme& theme, const core::Menu& menu,
                const core::Profile& profile, const HistoryView& history,
                std::string_view data_folder, core::Vec2 mouse, core::Nanoseconds now) const;

private:
    void render_particles(Canvas& canvas, const core::Profile& profile, core::Vec2 mouse) const;
    void render_plate(Canvas& canvas, int mode, core::Vec2 mouse) const;

    core::Particles particles_;
    double anim_ms_ = 0;  ///< MenuAnimTimer: время в меню, мс
    core::Nanoseconds last_ = 0;
};

}  // namespace aim::app
