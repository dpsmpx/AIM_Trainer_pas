#pragma once

// Экраны программы: что показать и что сделать по событиям переходов
// (core/flow.h) и меню (core/menu.h) — эффекты попадания, след курсора,
// уведомления, запись профиля и истории (app/player_files.h), смена
// профиля, «История». Рисуют app/menu_view.h и app/game_view.h на холсте
// app/canvas.h.

#include <SDL3/SDL.h>

#include <string>

#include "app/canvas.h"
#include "app/effects.h"
#include "app/menu_view.h"
#include "app/player_files.h"
#include "app/text.h"
#include "app/toast.h"
#include "core/flow.h"
#include "core/menu.h"
#include "core/profile.h"
#include "core/random.h"

namespace aim::app {

class Screens {
public:
    Screens(SDL_Renderer* renderer, Text& text, core::Profile& profile, PlayerFiles& files,
            core::Random& rng, core::Menu& menu);

    /// Программа запущена в момент now: меню появляется.
    void start(core::Nanoseconds now);
    /// Уведомление вверху окна.
    void message(std::string text) { toasts_.push(std::move(text)); }

    /// До ввода кадра: в меню режим под курсором — выбранный.
    void before_input(const core::Flow& flow, core::Vec2 mouse, core::Nanoseconds now);
    /// После Flow::update: события переходов и меню — эффекты, файлы,
    /// уведомления; след курсора и частицы.
    void after_update(core::Flow& flow, core::Vec2 mouse, core::Nanoseconds now);
    /// Нарисовать текущий экран на момент now.
    void render(const core::Flow& flow, core::Vec2 mouse, int fps, core::Nanoseconds now);

private:
    void save();
    void open_history();
    [[nodiscard]] const core::Theme& theme() const;

    Canvas canvas_;
    core::Profile& profile_;
    PlayerFiles& files_;
    core::Random& rng_;
    core::Menu& menu_;
    MenuView menu_view_;
    Effects effects_;
    CursorTrail trail_;
    Toasts toasts_;
    HistoryView history_;
};

}  // namespace aim::app
