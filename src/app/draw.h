#pragma once

// Простые фигуры поверх SDL_Renderer. Текст — app/text.h.

#include <SDL3/SDL.h>

#include "core/palette.h"

namespace aim::app {

void set_color(SDL_Renderer* renderer, core::Rgba color);
void fill_rect(SDL_Renderer* renderer, float x, float y, float w, float h, core::Rgba color);
void fill_circle(SDL_Renderer* renderer, float cx, float cy, float radius, core::Rgba color);
void circle_outline(SDL_Renderer* renderer, float cx, float cy, float radius, float width,
                    core::Rgba color);
void thick_line(SDL_Renderer* renderer, float x1, float y1, float x2, float y2, float width,
                core::Rgba color);

}  // namespace aim::app
