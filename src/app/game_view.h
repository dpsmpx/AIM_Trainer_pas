#pragma once

// Экраны замера на холсте app/canvas.h: отсчёт, кадр замера (цель или
// смена цвета окна, D-37), итог с параметрами, рекордом и средним за N
// (D-38) и тепловой картой промахов.

#include <optional>

#include "app/canvas.h"
#include "app/effects.h"
#include "core/flow.h"
#include "core/game.h"
#include "core/palette.h"
#include "core/profile.h"
#include "core/time.h"

namespace aim::app {

/// Отсчёт: название набора, цифра, «Get ready!».
void render_countdown(Canvas& canvas, const core::Theme& theme, const core::Flow& flow,
                      core::Nanoseconds now);

/// Кадр замера без уведомления: след, цель или цвет окна, эффекты, HUD,
/// подсказка кнопок, серия.
void render_game(Canvas& canvas, const core::Theme& theme, const core::Game& game, Effects& effects,
                 const CursorTrail& trail, int fps, core::Nanoseconds now);

/// Итог замера: причина конца, параметры, меры, рекорд и среднее за N
/// набора, тепловая карта промахов; без уведомления.
void render_game_over(Canvas& canvas, const core::Theme& theme, const core::Game& game,
                      const std::optional<core::RunFinished>& run, const core::Profile& profile);

}  // namespace aim::app
