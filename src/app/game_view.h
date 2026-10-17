#pragma once

// Экраны замера на холсте app/canvas.h: отсчёт, кадр замера (цель или
// смена цвета окна, D-37), итог с параметрами, рекордом и средним за N
// (D-38) и тепловой картой промахов, реплей (D-39).

#include "app/canvas.h"
#include "app/effects.h"
#include "core/flow.h"
#include "core/game.h"
#include "core/palette.h"
#include "core/profile.h"
#include "core/replay.h"
#include "core/time.h"

namespace aim::app {

/// Отсчёт: название набора, цифра, «Приготовься!».
void render_countdown(Canvas& canvas, const core::Theme& theme, const core::Flow& flow,
                      core::Nanoseconds now);

/// Кадр замера без уведомления: след, цель или цвет окна, эффекты; с hud —
/// ещё панель мер, подсказка кнопок, серия (интерфейс замера, D-45).
void render_game(Canvas& canvas, const core::Theme& theme, const core::Game& game, Effects& effects,
                 const CursorTrail& trail, int fps, core::Nanoseconds now, bool hud = true);

/// Итог замера: причина конца, параметры, меры, рекорд и среднее за N
/// набора, тепловая карта промахов, кнопки «Ещё раз», «В меню» и «Реплей»
/// (D-46); без уведомления. mouse — подсветка кнопки под курсором.
void render_game_over(Canvas& canvas, const core::Theme& theme, const core::Game& game,
                      const core::Flow& flow, const core::Profile& profile, core::Vec2 mouse,
                      core::Nanoseconds now);

/// Реплей: кадр замера из записи, курсор игрока, плашка «Реплей» с
/// набором и датой; в конце — как выйти: в меню или на итог (back, D-46).
void render_replay(Canvas& canvas, const core::Theme& theme, const core::ReplayPlayer& replay,
                   core::Screen back, Effects& effects, const CursorTrail& trail, int fps,
                   core::Nanoseconds now);

}  // namespace aim::app
