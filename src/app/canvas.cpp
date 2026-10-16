#include "app/canvas.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <string>
#include <vector>

namespace aim::app {

namespace {

// Центр пикселя GDI+ — целая точка, у SDL — середина клетки.
constexpr float kPixelCenter = 0.5F;
// Край сглаженной фигуры: полпикселя внутрь и наружу.
constexpr float kFeather = 0.5F;

struct Pt {
    float x = 0;
    float y = 0;
};

SDL_FColor to_fcolor(core::Rgba c, float coverage = 1.0F) {
    return {static_cast<float>(c.r) / 255.0F, static_cast<float>(c.g) / 255.0F,
            static_cast<float>(c.b) / 255.0F,
            static_cast<float>(c.a) / 255.0F * std::clamp(coverage, 0.0F, 1.0F)};
}

// Сборщик треугольников одного вызова SDL_RenderGeometry.
class Mesh {
public:
    int vertex(Pt p, SDL_FColor c) {
        vertices_.push_back({{p.x, p.y}, c, {0, 0}});
        return static_cast<int>(vertices_.size()) - 1;
    }
    void triangle(int a, int b, int c) { indices_.insert(indices_.end(), {a, b, c}); }
    void quad(int a, int b, int c, int d) {  // a-b — одна сторона, d-c — другая
        triangle(a, b, c);
        triangle(a, c, d);
    }
    void draw(SDL_Renderer* r) const {
        if (indices_.empty()) return;
        SDL_RenderGeometry(r, nullptr, vertices_.data(), static_cast<int>(vertices_.size()),
                           indices_.data(), static_cast<int>(indices_.size()));
    }

private:
    std::vector<SDL_Vertex> vertices_;
    std::vector<int> indices_;
};

// Точки эллипса с центром (cx, cy) и полуосями rx, ry — по часовой стрелке
// в координатах окна.
std::vector<Pt> ellipse_points(float cx, float cy, float rx, float ry) {
    const float r = std::max(rx, ry);
    const int n = std::clamp(static_cast<int>(r * 1.5F), 16, 128);
    std::vector<Pt> pts;
    pts.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const double a = 2.0 * std::numbers::pi * i / n;
        pts.push_back(
            {cx + rx * static_cast<float>(std::cos(a)), cy + ry * static_cast<float>(std::sin(a))});
    }
    return pts;
}

// Контур скруглённого прямоугольника, как CreatePathForDrawRoundRect
// GraphABC: стороны x1…x2−1 и y1…y2−1, углы — четверти эллипса w×h.
std::vector<Pt> round_rect_points(int x1, int y1, int x2, int y2, int w, int h) {
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);
    const float left = static_cast<float>(x1) + kPixelCenter;
    const float top = static_cast<float>(y1) + kPixelCenter;
    const float right = static_cast<float>(x2 - 1) + kPixelCenter;
    const float bottom = static_cast<float>(y2 - 1) + kPixelCenter;
    const float rx = std::min(static_cast<float>(w) / 2.0F, (right - left) / 2.0F);
    const float ry = std::min(static_cast<float>(h) / 2.0F, (bottom - top) / 2.0F);
    std::vector<Pt> pts;
    if (rx <= 0 || ry <= 0) return {{left, top}, {right, top}, {right, bottom}, {left, bottom}};
    const int steps = std::clamp(static_cast<int>(std::max(rx, ry)), 2, 16);
    const auto corner = [&](float cx, float cy, double from_deg) {
        for (int i = 0; i <= steps; ++i) {
            const double a = (from_deg + 90.0 * i / steps) * std::numbers::pi / 180.0;
            pts.push_back({cx + rx * static_cast<float>(std::cos(a)),
                           cy + ry * static_cast<float>(std::sin(a))});
        }
    };
    corner(right - rx, top + ry, 270);
    corner(right - rx, bottom - ry, 0);
    corner(left + rx, bottom - ry, 90);
    corner(left + rx, top + ry, 180);
    return pts;
}

// Нормаль наружу в вершине i выпуклого контура (обход по часовой стрелке в
// координатах окна), длиной под угол: смещение на d сдвигает обе стороны на d.
Pt outward(const std::vector<Pt>& p, std::size_t i) {
    const std::size_t n = p.size();
    const Pt prev = p[(i + n - 1) % n];
    const Pt cur = p[i];
    const Pt next = p[(i + 1) % n];
    const auto normal = [](Pt a, Pt b) {
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float len = std::hypot(dx, dy);
        if (len == 0) return Pt{0, 0};
        return Pt{dy / len, -dx / len};
    };
    const Pt n1 = normal(prev, cur);
    const Pt n2 = normal(cur, next);
    Pt m{n1.x + n2.x, n1.y + n2.y};
    const float len = std::hypot(m.x, m.y);
    if (len < 1e-6F) return n1;
    m = {m.x / len, m.y / len};
    const float cosine = std::max(m.x * n1.x + m.y * n1.y, 0.25F);
    return {m.x / cosine, m.y / cosine};
}

// Залить выпуклый контур со сглаженным краем.
void fill_convex(SDL_Renderer* r, const std::vector<Pt>& p, core::Rgba color) {
    if (p.size() < 3 || color.a == 0) return;
    Mesh mesh;
    const SDL_FColor solid = to_fcolor(color);
    const SDL_FColor clear = to_fcolor(color, 0);
    Pt center{0, 0};
    for (const Pt& v : p) {
        center.x += v.x;
        center.y += v.y;
    }
    center = {center.x / static_cast<float>(p.size()), center.y / static_cast<float>(p.size())};
    const int c = mesh.vertex(center, solid);
    const std::size_t n = p.size();
    std::vector<int> inner(n);
    std::vector<int> outer(n);
    for (std::size_t i = 0; i < n; ++i) {
        const Pt o = outward(p, i);
        Pt in{p[i].x - o.x * kFeather, p[i].y - o.y * kFeather};
        // Фигура уже двух пикселей: внутренний край не заходит за центр.
        if (std::hypot(in.x - center.x, in.y - center.y) >
            std::hypot(p[i].x - center.x, p[i].y - center.y))
            in = center;
        inner[i] = mesh.vertex(in, solid);
        outer[i] = mesh.vertex({p[i].x + o.x * kFeather, p[i].y + o.y * kFeather}, clear);
    }
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        mesh.triangle(c, inner[i], inner[j]);
        mesh.quad(inner[i], inner[j], outer[j], outer[i]);
    }
    mesh.draw(r);
}

// Обвести замкнутый контур пером ширины width со сглаженными краями.
void stroke_closed(SDL_Renderer* r, const std::vector<Pt>& p, float width, core::Rgba color) {
    if (p.size() < 3 || color.a == 0 || width <= 0) return;
    // Тоньше пикселя — край виден слабее, как у GDI+.
    const float coverage = std::min(width, 1.0F);
    const float half_core = std::max(width / 2.0F - kFeather, 0.0F);
    const float half_all = std::max(width, 1.0F) / 2.0F + kFeather;
    const SDL_FColor solid = to_fcolor(color, coverage);
    const SDL_FColor clear = to_fcolor(color, 0);
    Mesh mesh;
    const std::size_t n = p.size();
    std::vector<std::array<int, 4>> ring(n);
    for (std::size_t i = 0; i < n; ++i) {
        const Pt o = outward(p, i);
        const auto at = [&](float d) { return Pt{p[i].x + o.x * d, p[i].y + o.y * d}; };
        ring[i] = {mesh.vertex(at(-half_all), clear), mesh.vertex(at(-half_core), solid),
                   mesh.vertex(at(half_core), solid), mesh.vertex(at(half_all), clear)};
    }
    for (std::size_t i = 0; i < n; ++i) {
        const auto& a = ring[i];
        const auto& b = ring[(i + 1) % n];
        for (std::size_t k = 0; k < 3; ++k)
            mesh.quad(a[k], b[k], b[k + 1], a[k + 1]);
    }
    mesh.draw(r);
}

// Отрезок пером ширины width: торцы плоские, края сглажены.
void stroke_segment(Mesh& mesh, Pt a, Pt b, float width, core::Rgba color) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float len = std::hypot(dx, dy);
    if (len == 0) return;
    const Pt n{-dy / len, dx / len};
    const float coverage = std::min(width, 1.0F);
    const float half_core = std::max(width / 2.0F - kFeather, 0.0F);
    const float half_all = std::max(width, 1.0F) / 2.0F + kFeather;
    const SDL_FColor solid = to_fcolor(color, coverage);
    const SDL_FColor clear = to_fcolor(color, 0);
    const auto off = [&](Pt p, float d) { return Pt{p.x + n.x * d, p.y + n.y * d}; };
    const std::array<float, 4> ds{-half_all, -half_core, half_core, half_all};
    std::array<int, 4> va{};
    std::array<int, 4> vb{};
    for (std::size_t k = 0; k < 4; ++k) {
        const SDL_FColor c = (k == 0 || k == 3) ? clear : solid;
        va[k] = mesh.vertex(off(a, ds[k]), c);
        vb[k] = mesh.vertex(off(b, ds[k]), c);
    }
    for (std::size_t k = 0; k < 3; ++k)
        mesh.quad(va[k], vb[k], vb[k + 1], va[k + 1]);
}

Pt gdi(int x, int y) {
    return {static_cast<float>(x) + kPixelCenter, static_cast<float>(y) + kPixelCenter};
}

}  // namespace

Canvas::Canvas(SDL_Renderer* renderer, Text& text) : renderer_(renderer), text_(text) {}

void Canvas::clear_window(core::Rgba c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_RenderClear(renderer_);
}

void Canvas::fill_rect(int x1, int y1, int x2, int y2) {
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);
    if (brush.a == 0) return;
    SDL_SetRenderDrawColor(renderer_, brush.r, brush.g, brush.b, brush.a);
    const SDL_FRect rect{static_cast<float>(x1), static_cast<float>(y1),
                         static_cast<float>(x2 - x1), static_cast<float>(y2 - y1)};
    SDL_RenderFillRect(renderer_, &rect);
}

void Canvas::fill_round_rect(int x1, int y1, int x2, int y2, int w, int h) {
    fill_convex(renderer_, round_rect_points(x1, y1, x2, y2, w, h), brush);
}

void Canvas::draw_round_rect(int x1, int y1, int x2, int y2, int w, int h) {
    stroke_closed(renderer_, round_rect_points(x1, y1, x2, y2, w, h), static_cast<float>(pen_width),
                  pen);
}

void Canvas::fill_circle(int x, int y, int r) {
    if (r <= 0) return;
    const Pt c = gdi(x, y);
    fill_convex(renderer_, ellipse_points(c.x, c.y, static_cast<float>(r), static_cast<float>(r)),
                brush);
}

void Canvas::fill_polygon(std::span<const IntPoint> points) {
    std::vector<Pt> pts;
    pts.reserve(points.size());
    for (const IntPoint& p : points)
        pts.push_back(gdi(p.x, p.y));
    fill_convex(renderer_, pts, brush);
}

void Canvas::draw_circle(int x, int y, int r) {
    if (r <= 0) return;
    const Pt c = gdi(x, y);
    stroke_closed(renderer_, ellipse_points(c.x, c.y, static_cast<float>(r), static_cast<float>(r)),
                  static_cast<float>(pen_width), pen);
}

void Canvas::circle(int x, int y, int r) {
    fill_circle(x, y, r);
    draw_circle(x, y, r);
}

void Canvas::line(int x1, int y1, int x2, int y2) {
    if (pen.a == 0) return;
    Mesh mesh;
    stroke_segment(mesh, gdi(x1, y1), gdi(x2, y2), static_cast<float>(pen_width), pen);
    mesh.draw(renderer_);
}

void Canvas::polyline(std::span<const IntPoint> points) {
    if (points.size() < 2 || pen.a == 0) return;
    Mesh mesh;
    for (std::size_t i = 0; i + 1 < points.size(); ++i)
        stroke_segment(mesh, gdi(points[i].x, points[i].y), gdi(points[i + 1].x, points[i + 1].y),
                       static_cast<float>(pen_width), pen);
    mesh.draw(renderer_);
}

void Canvas::text_out(int x, int y, std::string_view s) {
    // GraphABCHelper.TextOut: кисть не пустая — сперва прямоугольник под
    // строкой (ширина — как MeasureString, высота — строка и ещё 1).
    const int w = text_.measure(s, font);
    const int h = text_.height(font);
    fill_rect(x, y, x + w, y + h + 1);
    text_.out(static_cast<float>(x), static_cast<float>(y), s, font, font_color);
}

void Canvas::draw_text_centered(int x, int y, int x1, int y1, std::string_view s) {
    const SDL_Rect box{x, y, x1 - x, y1 - y};
    if (box.w <= 0 || box.h <= 0) return;
    // Перенос — по ширине прямоугольника, как у эталона (libgdiplus).
    const std::vector<std::string> lines = text_.wrap(s, font, box.w);
    const int line_h = text_.height(font);
    const int block = line_h * static_cast<int>(lines.size());
    const float top = static_cast<float>(box.y) + static_cast<float>(box.h - block) / 2.0F;
    // Обрезка — только если текст выходит за прямоугольник: смена области
    // обрезки рвёт пакет отрисовки SDL.
    int widest = 0;
    for (const std::string& line : lines)
        widest = std::max(widest, text_.measure(line, font));
    const bool clip = block > box.h || widest > box.w;
    if (clip) SDL_SetRenderClipRect(renderer_, &box);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const int w = text_.measure(lines[i], font);
        text_.out(static_cast<float>(box.x) + static_cast<float>(box.w - w) / 2.0F,
                  top + static_cast<float>(line_h) * static_cast<float>(i), lines[i], font,
                  font_color);
    }
    if (clip) SDL_SetRenderClipRect(renderer_, nullptr);
}

int Canvas::text_width(std::string_view s) {
    return text_.width(s, font);
}

}  // namespace aim::app
