#include "app/draw.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <string>

namespace aim::app {

namespace {

constexpr int kSegments = 64;
constexpr auto kSegmentCount = static_cast<std::size_t>(kSegments);
constexpr float kGlyph = 8.0F;  // SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE

SDL_FColor to_fcolor(core::Rgba c) {
    return {static_cast<float>(c.r) / 255.0F, static_cast<float>(c.g) / 255.0F,
            static_cast<float>(c.b) / 255.0F, static_cast<float>(c.a) / 255.0F};
}

SDL_FPoint on_circle(float cx, float cy, float radius, int i) {
    const auto angle = static_cast<float>(2.0 * std::numbers::pi * i / kSegments);
    return {cx + radius * std::cos(angle), cy + radius * std::sin(angle)};
}

}  // namespace

void set_color(SDL_Renderer* renderer, core::Rgba color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void fill_rect(SDL_Renderer* renderer, float x, float y, float w, float h, core::Rgba color) {
    set_color(renderer, color);
    const SDL_FRect rect{x, y, w, h};
    SDL_RenderFillRect(renderer, &rect);
}

void fill_circle(SDL_Renderer* renderer, float cx, float cy, float radius, core::Rgba color) {
    if (radius <= 0.0F) return;
    // Веер треугольников из центра.
    std::array<SDL_Vertex, kSegmentCount + 1> vertices{};
    std::array<int, kSegmentCount * 3> indices{};
    const SDL_FColor fc = to_fcolor(color);
    vertices[0] = {{cx, cy}, fc, {0, 0}};
    for (int i = 0; i < kSegments; ++i) {
        vertices[static_cast<std::size_t>(i) + 1] = {on_circle(cx, cy, radius, i), fc, {0, 0}};
        const auto at = static_cast<std::size_t>(i) * 3;
        indices[at] = 0;
        indices[at + 1] = i + 1;
        indices[at + 2] = (i + 1) % kSegments + 1;
    }
    SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

void circle_outline(SDL_Renderer* renderer, float cx, float cy, float radius, float width,
                    core::Rgba color) {
    if (radius <= 0.0F) return;
    // Кольцо — полоса треугольников между внешним и внутренним кругом.
    const float inner = std::max(0.0F, radius - width / 2.0F);
    const float outer = radius + width / 2.0F;
    std::array<SDL_Vertex, kSegmentCount * 2> vertices{};
    std::array<int, kSegmentCount * 6> indices{};
    const SDL_FColor fc = to_fcolor(color);
    for (int i = 0; i < kSegments; ++i) {
        const auto v = static_cast<std::size_t>(i) * 2;
        vertices[v] = {on_circle(cx, cy, outer, i), fc, {0, 0}};
        vertices[v + 1] = {on_circle(cx, cy, inner, i), fc, {0, 0}};
        const int a = i * 2;
        const int b = (i + 1) % kSegments * 2;
        const auto at = static_cast<std::size_t>(i) * 6;
        indices[at] = a;
        indices[at + 1] = a + 1;
        indices[at + 2] = b;
        indices[at + 3] = b;
        indices[at + 4] = a + 1;
        indices[at + 5] = b + 1;
    }
    SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

void thick_line(SDL_Renderer* renderer, float x1, float y1, float x2, float y2, float width,
                core::Rgba color) {
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    const float len = std::hypot(dx, dy);
    if (len <= 0.0F) return;
    const float nx = -dy / len * width / 2.0F;
    const float ny = dx / len * width / 2.0F;
    const SDL_FColor fc = to_fcolor(color);
    const std::array<SDL_Vertex, 4> vertices{{{{x1 + nx, y1 + ny}, fc, {0, 0}},
                                              {{x1 - nx, y1 - ny}, fc, {0, 0}},
                                              {{x2 + nx, y2 + ny}, fc, {0, 0}},
                                              {{x2 - nx, y2 - ny}, fc, {0, 0}}}};
    const std::array<int, 6> indices{0, 1, 2, 2, 1, 3};
    SDL_RenderGeometry(renderer, nullptr, vertices.data(), 4, indices.data(), 6);
}

float text_width(std::string_view text, float scale) {
    return static_cast<float>(text.size()) * kGlyph * scale;
}

void text(SDL_Renderer* renderer, float x, float y, std::string_view line, float scale,
          core::Rgba color) {
    set_color(renderer, color);
    const std::string copy(line);
    SDL_SetRenderScale(renderer, scale, scale);
    SDL_RenderDebugText(renderer, x / scale, y / scale, copy.c_str());
    SDL_SetRenderScale(renderer, 1.0F, 1.0F);
}

void text_centered(SDL_Renderer* renderer, float center_x, float y, std::string_view line,
                   float scale, core::Rgba color) {
    text(renderer, center_x - text_width(line, scale) / 2.0F, y, line, scale, color);
}

}  // namespace aim::app
