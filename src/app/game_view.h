#pragma once

// Экраны замера на холсте app/canvas.h: отсчёт, кадр замера (цель или
// смена цвета окна, D-37), итог с параметрами, рекордом и средним за N
// (D-38) и тепловой картой промахов, реплей (D-39).

#include <optional>

#include "app/canvas.h"
#include "app/effects.h"
#include "core/flow.h"
#include "core/game.h"
#include "core/palette.h"
#include "core/profile.h"
#include "core/replay.h"
#include "core/time.h"

namespace aim::app {

/// Отсчёт: название набора, цифра, «Get ready!».
void render_countdown(Canvas& canvas, const core::Theme& theme, const core::Flow& flow,
                      core::Nanoseconds now);

/// Кадр замера без уведомления: след, цель или цвет окна, эффекты; с hud —
/// ещё панель мер, подсказка кнопок, серия (интерфейс замера, D-45).
void render_game(Canvas& canvas, const core::Theme& theme, const core::Game& game, Effects& effects,
                 const CursorTrail& trail, int fps, core::Nanoseconds now, bool hud = true);

/// Итог замера: причина конца, параметры, меры, рекорд и среднее за N
/// набора, тепловая карта промахов; без уведомления.
void render_game_over(Canvas& canvas, const core::Theme& theme, const core::Game& game,
                      const std::optional<core::RunFinished>& run, const core::Profile& profile);

/// Реплей: кадр замера из записи, курсор игрока, плашка «Реплей» с
/// набором и датой; в конце — как выйти.
void render_replay(Canvas& canvas, const core::Theme& theme, const core::ReplayPlayer& replay,
                   Effects& effects, const CursorTrail& trail, int fps, core::Nanoseconds now);

}  // namespace aim::app
