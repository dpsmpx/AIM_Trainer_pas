#pragma once

// Холст программы: фигуры и надписи со сглаживанием. Цвет, перо и шрифт —
// в каждом вызове: картинка не зависит от того, что рисовали до неё
// (AUD-46; прежний холст держал кисть, перо и шрифт между вызовами, как
// GraphABC образца). Надпись — без подложки.
//
// Координаты — целые; сглаживание — как у GDI+ со SmoothingMode.AntiAlias:
// центр пикселя — в целой точке, край фигуры размыт на пиксель. fill_rect —
// без сглаживания, как FillRectangle.

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <set>
#include <span>
#include <string>
#include <string_view>

#include "app/text.h"
#include "core/layout.h"
#include "core/palette.h"

namespace aim::app {

// Размеры шрифта, пунктов (D-43): один для всего текста; крупнее — только
// заголовки экранов и панелей, заголовок итога и реакция замера на цвет,
// отсчёт.
inline constexpr int kTextSize = 12;
inline constexpr int kTitleSize = 20;
inline constexpr int kHeadlineSize = 32;
inline constexpr int kCountdownSize = 120;
/// Цвет темы с прозрачностью a (0…255): мигание и угасание по своему закону.
inline core::Rgba with_alpha(core::Rgba c, int a) {
    c.a = static_cast<std::uint8_t>(std::clamp(a, 0, 255));
    return c;
}

/// Цвет темы с прозрачностью, умноженной на share (0…1): мигание, угасание,
/// появление (D-44).
inline core::Rgba faded(core::Rgba c, double share) {
    const double a = static_cast<double>(c.a) * std::clamp(share, 0.0, 1.0);
    c.a = static_cast<std::uint8_t>(std::lround(a));
    return c;
}

struct IntPoint {
    int x = 0;
    int y = 0;
};

/// Перо: цвет и толщина линии, px.
struct Pen {
    core::Rgba color;
    int width = 1;
};

// Шрифты надписей (D-43): один размер для всего текста; крупнее — только
// заголовки экранов и панелей, заголовок итога и реакция замера на цвет,
// отсчёт.
inline constexpr Font kText{.size = kTextSize};
inline constexpr Font kTextBold{.size = kTextSize, .weight = Weight::Bold};
inline constexpr Font kTitle{.size = kTitleSize};
inline constexpr Font kTitleBold{.size = kTitleSize, .weight = Weight::Bold};
inline constexpr Font kHeadline{.size = kHeadlineSize};
inline constexpr Font kCountdown{.size = kCountdownSize};

class Canvas {
public:
    Canvas(SDL_Renderer* renderer, Text& text);

    // ---- Фигуры ----
    /// Всё окно цветом c.
    void clear(core::Rgba c);
    /// Прямоугольник [x1, x2) × [y1, y2) без сглаживания.
    void fill_rect(int x1, int y1, int x2, int y2, core::Rgba color);
    /// Скруглённый прямоугольник: углы — дуги w×h.
    void fill_round_rect(int x1, int y1, int x2, int y2, int w, int h, core::Rgba color);
    void fill_round_rect(const core::Rect& r, int w, int h, core::Rgba color) {
        fill_round_rect(r.x1, r.y1, r.x2, r.y2, w, h, color);
    }
    /// Контур скруглённого прямоугольника.
    void stroke_round_rect(int x1, int y1, int x2, int y2, int w, int h, Pen pen);
    void stroke_round_rect(const core::Rect& r, int w, int h, Pen pen) {
        stroke_round_rect(r.x1, r.y1, r.x2, r.y2, w, h, pen);
    }
    void fill_circle(int x, int y, int r, core::Rgba color);
    void stroke_circle(int x, int y, int r, Pen pen);
    /// Выпуклый многоугольник; вершины — по часовой стрелке на экране.
    void fill_polygon(std::span<const IntPoint> points, core::Rgba color);
    /// Отрезок; торцы плоские.
    void line(int x1, int y1, int x2, int y2, Pen pen);
    /// Ломаная.
    void polyline(std::span<const IntPoint> points, Pen pen);
    /// Дуга окружности радиуса r: от угла from на sweep радиан (углы окна:
    /// 0 — вправо, π/2 — вниз).
    void arc(int x, int y, double r, double from, double sweep, Pen pen);

    // ---- Текст: без подложки ----
    /// Строка с левым верхним углом в (x, y).
    void text(int x, int y, std::string_view s, Font font, core::Rgba color);
    /// По центру прямоугольника, с переносом по словам и обрезкой по нему.
    void text_centered(int x1, int y1, int x2, int y2, std::string_view s, Font font,
                       core::Rgba color);
    void text_centered(const core::Rect& r, std::string_view s, Font font, core::Rgba color) {
        text_centered(r.x1, r.y1, r.x2, r.y2, s, font, color);
    }
    /// То же, но строки — от левого края прямоугольника, блок — сверху.
    void text_left(int x1, int y1, int x2, int y2, std::string_view s, Font font, core::Rgba color);
    /// «Подпись: значение» одной строкой по центру прямоугольника (D-43):
    /// подпись — цветом label, значение — value; без «: » — вся строка
    /// цветом value. Не помещается в ширину — переносом, как text_centered.
    void pair_centered(int x1, int y1, int x2, int y2, std::string_view s, Font font,
                       core::Rgba label, core::Rgba value);
    void pair_centered(const core::Rect& r, std::string_view s, Font font, core::Rgba label,
                       core::Rgba value) {
        pair_centered(r.x1, r.y1, r.x2, r.y2, s, font, label, value);
    }
    /// Ширина строки, px, и ещё 1 — как TextWidth.
    int text_width(std::string_view s, Font font);
    /// Высота строки шрифта, px: рамки — по ней (D-43).
    int line_height(Font font) { return text_.height(font); }
    /// Сколько строк займёт s при переносе по ширине width.
    int text_lines(std::string_view s, Font font, int width) {
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
    void text_box(int x1, int y1, int x2, int y2, std::string_view s, Font font, core::Rgba color,
                  bool centered);

    SDL_Renderer* renderer_;
    Text& text_;
    bool check_text_ = false;
    std::set<std::string, std::less<>> clipped_;
};

}  // namespace aim::app
