#include "app/game_view.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

#include "core/rounding.h"
#include "core/rules.h"
#include "core/screen.h"

namespace aim::app {

namespace {

constexpr int kW = core::kWindowWidth;
constexpr int kH = core::kWindowHeight;
constexpr core::Rgba kClWhite{255, 255, 255, 255};

core::Rgba argb(int a, int r, int g, int b) {
    const auto byte = [](int v) { return static_cast<std::uint8_t>(v); };
    return {byte(r), byte(g), byte(b), byte(a)};
}

int round_int(double v) {
    return core::round_half_even(v);
}

/// Format('{0:0.0}') образца.
std::string one_decimal(double v) {
    return std::format("{:.1f}", v);
}

// DrawTextWithBackground (Base.pas): плашка, тень и белый текст.
void text_with_background(Canvas& c, core::Rgba bg, int x1, int y1, int x2, int y2, int size,
                          std::string_view text) {
    c.brush = bg;
    c.fill_round_rect(x1, y1, x2, y2, 16, 16);
    c.font.size = size;
    c.font_color = argb(120, 0, 0, 0);
    c.draw_text_centered(x1 + 2, y1 + 2, x2 + 2, y2 + 2, text);
    c.font_color = kClWhite;
    c.draw_text_centered(x1, y1, x2, y2, text);
}

// TCircle.Render.
void render_target(Canvas& c, const core::Theme& theme, const core::Game& game,
                   core::Nanoseconds now) {
    const core::Target& t = game.target();
    const int r = round_int(t.radius_at(now));
    if (r <= 0) return;
    const int x = round_int(t.center().x);
    const int y = round_int(t.center().y);
    if (t.is_bomb()) {
        c.brush = core::kBombFill;
        c.fill_circle(x, y, r);
        c.pen = kClWhite;
        c.pen_width = 3;
        const int cross = round_int(r * 0.5);
        c.line(x - cross, y - cross, x + cross, y + cross);
        c.line(x - cross, y + cross, x + cross, y - cross);
        return;
    }
    c.brush = theme.circles.at(static_cast<std::size_t>(t.color()));
    c.fill_circle(x, y, r);
    if (game.mode() == core::GameMode::Tracking) {
        // Кольцо наводки: чем ближе к засчитыванию, тем ярче.
        if (t.hover_progress() > 0) {
            const int progress = std::min(round_int(255.0 * core::to_ms(t.hover_progress()) /
                                                    core::to_ms(core::kTrackingHold)),
                                          255);
            c.pen = argb(progress, 255, 255, 255);
            c.pen_width = 3;
            c.circle(x, y, r + 6);
        }
    } else if (r > 8) {
        // Зоны точности на 50 % и 25 % радиуса.
        c.pen = argb(90, 255, 255, 255);
        c.pen_width = 1;
        c.circle(x, y, round_int(r * 0.5));
        c.circle(x, y, round_int(r * 0.25));
    }
}

void render_heatmap(Canvas& c, const core::Game& game) {
    const core::MissHeatmap& map = game.miss_heatmap();
    int most = 1;
    for (const auto& column : map)
        for (const int n : column)
            most = std::max(most, n);
    constexpr int kOriginX = kW - 220;
    constexpr int kOriginY = 20;
    constexpr int kCellW = 200 / core::kHeatmapCols;
    constexpr int kCellH = 100 / core::kHeatmapRows;

    c.font.size = 10;
    c.font_color = argb(180, 200, 200, 200);
    c.draw_text_centered(kOriginX, kOriginY - 18, kOriginX + 200, kOriginY,
                         "Промахи (последний забег)");
    c.brush = argb(80, 30, 30, 35);
    c.fill_round_rect(kOriginX, kOriginY, kOriginX + 200, kOriginY + 100, 4, 4);
    for (int cx = 0; cx < core::kHeatmapCols; ++cx) {
        for (int cy = 0; cy < core::kHeatmapRows; ++cy) {
            const int n = map[static_cast<std::size_t>(cx)][static_cast<std::size_t>(cy)];
            if (n <= 0) continue;
            const int alpha = std::max(round_int(220.0 * n / most), 40);
            const int gx = kOriginX + cx * kCellW;
            const int gy = kOriginY + cy * kCellH;
            c.brush = argb(alpha, 255, 90, 70);
            c.fill_round_rect(gx, gy, gx + kCellW, gy + kCellH, 1, 1);
        }
    }
}

}  // namespace

void render_countdown(Canvas& c, const core::Theme& theme, const core::Flow& flow,
                      core::Nanoseconds now) {
    // Цвет шрифта образец здесь не задаёт — остаётся от прошлого экрана.
    c.clear_window(theme.background);
    c.font.size = 20;
    c.draw_text_centered(
        0, kH / 2 - 140, kW, kH / 2 - 120,
        core::kModeLooks.at(static_cast<std::size_t>(core::mode_index(flow.mode()))).title);
    c.font.size = 120;
    c.draw_text_centered(0, kH / 2 - 100, kW, kH / 2 + 100,
                         std::to_string(flow.countdown_value(now)));
    c.font.size = 20;
    c.draw_text_centered(0, kH / 2 + 120, kW, kH / 2 + 140, "Get ready!");
}

void render_game(Canvas& c, const core::Theme& theme, const core::Game& game, Effects& effects,
                 const CursorTrail& trail, int fps, core::Nanoseconds now) {
    c.clear_window(theme.background);
    trail.render(c);
    render_target(c, theme, game, now);
    effects.render(c, now);

    const int old_size = c.font.size;
    const core::Rgba bg = argb(220, 60, 60, 65);
    constexpr int kInfoH = 60;
    constexpr int kCol = kW / 5;
    constexpr int kPad = 16;
    constexpr int kBottom = 16;
    text_with_background(c, bg, kPad, kH - kInfoH, kCol - kPad, kH - kBottom, 16,
                         std::format("FPS:\n{}", fps));
    text_with_background(c, bg, kCol + kPad, kH - kInfoH, 2 * kCol - kPad, kH - kBottom, 16,
                         std::format("Score:\n{}", game.score()));
    text_with_background(c, bg, 2 * kCol + kPad, kH - kInfoH, 3 * kCol - kPad, kH - kBottom, 16,
                         "Accuracy:\n" + one_decimal(game.accuracy()) + "%");
    text_with_background(c, bg, 3 * kCol + kPad, kH - kInfoH, 4 * kCol - kPad, kH - kBottom, 16,
                         "Avg:\n" + one_decimal(game.avg_reaction_ms()) + "ms");
    text_with_background(c, bg, 4 * kCol + kPad, kH - kInfoH, kW - kPad, kH - kBottom, 16,
                         std::format("Lives:\n{}", std::max(game.lives_left(), 0)));
    c.font.size = old_size;

    if (game.mode() == core::GameMode::Dual) {
        // Подсказка «какой цвет какой кнопкой»; цели под ней не появляются.
        c.brush = argb(160, 40, 40, 45);
        c.fill_round_rect(10, 10, 190, 42, 8, 8);
        c.brush = theme.circles[0];
        c.fill_circle(26, 26, 7);
        c.brush = theme.circles[1];
        c.fill_circle(106, 26, 7);
        c.font.size = 11;
        c.font_color = kClWhite;
        c.draw_text_centered(36, 18, 96, 34, "LMB");
        c.draw_text_centered(116, 18, 180, 34, "RMB");
        c.font.size = old_size;
    }
    if (game.combo() >= 2) {
        c.brush = argb(160, 40, 40, 45);
        c.fill_round_rect(kW - 130, 10, kW - 10, 40, 8, 8);
        c.font.size = 14;
        c.font_color = game.combo() >= 10 ? argb(255, 255, 210, 90) : kClWhite;
        c.draw_text_centered(kW - 130, 10, kW - 10, 40, std::format("Combo x{}", game.combo()));
        c.font.size = old_size;
    }
}

void render_pause(Canvas& c) {
    // У образца — FillRoundRect с нулевым скруглением, на котором он падает
    // (сценарий 22): здесь — заливка окна.
    c.brush = argb(150, 15, 15, 18);
    c.fill_rect(0, 0, kW, kH);
    c.font.size = 40;
    c.font_color = kClWhite;
    c.draw_text_centered(0, kH / 2 - 70, kW, kH / 2 - 20, "PAUSE");
    c.font.size = 14;
    c.font_color = argb(220, 210, 210, 210);
    c.draw_text_centered(0, kH / 2 + 10, kW, kH / 2 + 30, "P — продолжить");
    c.draw_text_centered(0, kH / 2 + 34, kW, kH / 2 + 54, "R — заново    M — в меню");
}

void render_game_over(Canvas& c, const core::Theme& theme, const core::Game& game) {
    c.clear_window(theme.background);
    c.font.size = 44;
    c.font_color = kClWhite;
    c.draw_text_centered(0, kH / 2 - 60, kW, kH / 2 + 20,
                         game.died_to_bomb() ? "ПОДРЫВ!" : "GAME OVER");
    if (game.died_to_bomb()) {
        c.font.size = 14;
        c.font_color = argb(220, 255, 120, 100);
        c.draw_text_centered(0, kH / 2 + 24, kW, kH / 2 + 44,
                             "Кликнул по бомбе — мгновенный конец забега");
        c.font_color = kClWhite;
    }
    c.font.size = 18;
    c.draw_text_centered(0, kH / 2 + 60, kW, kH / 2 + 90, "Press SPACE to continue");

    c.font.size = 15;
    int y = kH / 2 + 120;
    const auto row = [&](const std::string& s) {
        c.draw_text_centered(0, y, kW, y + 24, s);
        y += 26;
    };
    row(std::format("Score: {}", game.score()));
    row("Accuracy: " + one_decimal(game.accuracy()) + "%");
    row("Avg Reaction: " + one_decimal(game.avg_reaction_ms()) + "ms");
    row("Precision: " + one_decimal(game.precision()) + "%");
    row(std::format("Hits: {}  Misses: {}", game.hits(), game.misses()));
    row(std::format("Max Combo: x{}", game.max_combo()));

    render_heatmap(c, game);
}

}  // namespace aim::app
