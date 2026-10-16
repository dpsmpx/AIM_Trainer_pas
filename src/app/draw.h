#pragma once

// Простые фигуры и текст поверх SDL_Renderer — для временного интерфейса
// шага «Время и ввод». Текст — встроенный отладочный шрифт SDL 8×8, только
// ASCII; шрифт DejaVu Sans Mono (D-22) — шаг «Экраны».

#include <SDL3/SDL.h>

#include <string_view>

#include "core/palette.h"

namespace aim::app {

void set_color(SDL_Renderer* renderer, core::Rgba color);
void fill_rect(SDL_Renderer* renderer, float x, float y, float w, float h, core::Rgba color);
void fill_circle(SDL_Renderer* renderer, float cx, float cy, float radius, core::Rgba color);
void circle_outline(SDL_Renderer* renderer, float cx, float cy, float radius, float width,
                    core::Rgba color);
void thick_line(SDL_Renderer* renderer, float x1, float y1, float x2, float y2, float width,
                core::Rgba color);

/// Ширина строки отладочного шрифта при масштабе scale, px.
float text_width(std::string_view text, float scale);
/// Строка отладочным шрифтом: левый верхний угол (x, y), буква 8·scale px.
void text(SDL_Renderer* renderer, float x, float y, std::string_view line, float scale,
          core::Rgba color);
/// Строка, по центру по x.
void text_centered(SDL_Renderer* renderer, float center_x, float y, std::string_view line,
                   float scale, core::Rgba color);

}  // namespace aim::app
