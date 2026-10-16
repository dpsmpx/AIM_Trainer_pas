#pragma once

// Экраны забега — перенос CountdownScreen, RenderGame, TCircle.Render,
// RenderPauseOverlay, RenderGameOver и RenderMissHeatmap образца
// (archive/pascal/AIM_Trainer.pas, AimCircle.pas) на холсте app/canvas.h.

#include "app/canvas.h"
#include "app/effects.h"
#include "core/flow.h"
#include "core/game.h"
#include "core/palette.h"
#include "core/time.h"

namespace aim::app {

/// Отсчёт: название режима, цифра, «Get ready!».
void render_countdown(Canvas& canvas, const core::Theme& theme, const core::Flow& flow,
                      core::Nanoseconds now);

/// Кадр забега без уведомления: след, цель, эффекты, HUD, подсказка Dual,
/// серия.
void render_game(Canvas& canvas, const core::Theme& theme, const core::Game& game, Effects& effects,
                 const CursorTrail& trail, int fps, core::Nanoseconds now);

/// Затемнение и «PAUSE» поверх кадра забега.
void render_pause(Canvas& canvas);

/// Game Over или «ПОДРЫВ!»: итоги и тепловая карта промахов, без уведомления.
void render_game_over(Canvas& canvas, const core::Theme& theme, const core::Game& game);

}  // namespace aim::app
