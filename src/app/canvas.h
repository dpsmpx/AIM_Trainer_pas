#pragma once

// Холст в духе GraphABC (docs/ARCHITECTURE.md, §11): кисть, перо и шрифт —
// состояние, которое держится между вызовами и кадрами, как глобальные
// Brush, Pen и Font образца. Так экраны образца переносятся построчно, со
// всеми его следствиями: Circle закрашивает кистью и обводит пером, TextOut
// подкладывает под текст прямоугольник цвета кисти.
//
// Координаты — целые, как у GraphABC; сглаживание — как у GDI+ со
// SmoothingMode.AntiAlias: центр пикселя — в целой точке, край фигуры
// размыт на пиксель. FillRect — без сглаживания, как FillRectangle.

#include <SDL3/SDL.h>

#include <span>
#include <string_view>

#include "app/text.h"
#include "core/palette.h"

namespace aim::app {

struct IntPoint {
    int x = 0;
    int y = 0;
};

class Canvas {
public:
    Canvas(SDL_Renderer* renderer, Text& text);

    // ---- Состояние: как Brush.Color, Pen.Color, Pen.Width, Font у GraphABC ----
    core::Rgba brush{255, 255, 255, 255};  ///< по умолчанию — белая кисть
    core::Rgba pen{0, 0, 0, 255};          ///< чёрное перо
    int pen_width = 1;
    Font font{.size = 10, .weight = Weight::Normal};
    core::Rgba font_color{0, 0, 0, 255};

    // ---- Фигуры ----
    /// ClearWindow(c): всё окно цветом c; кисть не меняется.
    void clear_window(core::Rgba c);
    /// FillRect/FillRectangle(x1, y1, x2, y2): без сглаживания, [x1, x2) × [y1, y2).
    void fill_rect(int x1, int y1, int x2, int y2);
    /// FillRoundRect: только заливка кистью; углы — дуги w×h.
    void fill_round_rect(int x1, int y1, int x2, int y2, int w, int h);
    /// DrawRoundRect: только контур пером.
    void draw_round_rect(int x1, int y1, int x2, int y2, int w, int h);
    /// FillCircle: заливка кистью.
    void fill_circle(int x, int y, int r);
    /// DrawCircle: контур пером.
    void draw_circle(int x, int y, int r);
    /// Circle: заливка кистью и контур пером.
    void circle(int x, int y, int r);
    /// Line: отрезок пером.
    void line(int x1, int y1, int x2, int y2);
    /// Polyline: ломаная пером.
    void polyline(std::span<const IntPoint> points);

    // ---- Текст ----
    /// TextOut(x, y, s): прямоугольник кистью под строкой, затем строка.
    void text_out(int x, int y, std::string_view s);
    /// DrawTextCentered(x, y, x1, y1, s): по центру прямоугольника, с
    /// переносом по словам и обрезкой по нему, как DrawString GDI+.
    void draw_text_centered(int x, int y, int x1, int y1, std::string_view s);
    /// TextWidth(s).
    int text_width(std::string_view s);

    [[nodiscard]] SDL_Renderer* renderer() const { return renderer_; }

private:
    SDL_Renderer* renderer_;
    Text& text_;
};

}  // namespace aim::app
