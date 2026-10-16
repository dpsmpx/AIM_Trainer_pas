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

#include <cstddef>
#include <functional>
#include <set>
#include <span>
#include <string>
#include <string_view>

#include "app/text.h"
#include "core/palette.h"

namespace aim::app {

// Размеры шрифта, пунктов (D-43): один для всего текста; крупнее — только
// заголовки экранов и панелей, заголовок итога и реакция замера на цвет,
// отсчёт.
inline constexpr int kTextSize = 12;
inline constexpr int kTitleSize = 20;
inline constexpr int kHeadlineSize = 32;
inline constexpr int kCountdownSize = 120;
/// Подпись у пары «подпись: значение» — приглушённо (D-43).
inline constexpr core::Rgba kLabelColor{165, 170, 180, 255};

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
    /// FillPolygon: выпуклый многоугольник кистью; вершины — по часовой
    /// стрелке на экране.
    void fill_polygon(std::span<const IntPoint> points);
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
    /// То же, но строки — от левого края прямоугольника, блок — сверху.
    void draw_text_left(int x, int y, int x1, int y1, std::string_view s);
    /// TextWidth(s).
    int text_width(std::string_view s);
    /// «Подпись: значение» одной строкой по центру прямоугольника (D-43):
    /// подпись — цветом label, значение — value; без «: » — вся строка
    /// цветом value. Не помещается в ширину — переносом, как
    /// draw_text_centered.
    void pair_centered(int x, int y, int x1, int y1, std::string_view s, core::Rgba label,
                       core::Rgba value);
    /// Высота строки текущего шрифта, px: рамки — по ней (D-43).
    int line_height() { return text_.height(font); }
    /// Сколько строк займёт s при переносе по ширине width.
    int text_lines(std::string_view s, int width) {
        return static_cast<int>(text_.wrap(s, font, width).size());
    }
    /// Шрифт всех надписей (настройка «Шрифт», D-43).
    void set_family(Family family) { text_.set_family(family); }

    /// Проверка текста (--text-check, D-43): надпись, которая не поместилась
    /// в свой прямоугольник, — в журнал, по разу на строку.
    void check_text(bool on) { check_text_ = on; }
    /// Сколько разных надписей не поместилось.
    [[nodiscard]] std::size_t clipped_texts() const { return clipped_.size(); }

    [[nodiscard]] SDL_Renderer* renderer() const { return renderer_; }

private:
    /// Текст в прямоугольнике с переносом: по центру или от левого края.
    void draw_text_box(int x, int y, int x1, int y1, std::string_view s, bool centered);

    SDL_Renderer* renderer_;
    Text& text_;
    bool check_text_ = false;
    std::set<std::string, std::less<>> clipped_;
};

}  // namespace aim::app
