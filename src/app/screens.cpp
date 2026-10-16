#include "app/screens.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <string>

#include "app/draw.h"
#include "core/palette.h"
#include "core/rules.h"
#include "core/screen.h"

namespace aim::app {

namespace {

using core::GameMode;
using core::Rgba;

// Кнопки режимов — как у образца: BTNS_X, BTNS_Y, BTNS_W, BTNS_H, BTNS_GAP.
constexpr float kButtonsX = 420;
constexpr float kButtonsY = 40;
constexpr float kButtonW = 330;
constexpr float kButtonH = 34;
constexpr float kButtonGap = 8;

constexpr Rgba kText{230, 230, 235};
constexpr Rgba kDim{150, 150, 160};
constexpr Rgba kRed{230, 70, 60};

constexpr auto kW = static_cast<float>(core::kWindowWidth);
constexpr auto kH = static_cast<float>(core::kWindowHeight);

const core::Theme& theme_of(const core::Profile& profile) {
    const auto at = static_cast<std::size_t>(std::clamp(profile.settings.theme, 0, 2));
    return core::kThemes.at(at);
}

Rgba with_alpha(Rgba c, int alpha) {
    c.a = static_cast<std::uint8_t>(std::clamp(alpha, 0, 255));
    return c;
}

void render_menu(SDL_Renderer* r, const core::Profile& p, const Overlay& overlay) {
    core::Vec2 mouse;
    {
        float mx = 0;
        float my = 0;
        SDL_GetMouseState(&mx, &my);
        mouse = {mx, my};
    }
    text(r, 50, 50, "AIM TRAINER", 3, kText);
    text(r, 50, 90, "C++ port, work in progress", 1.5F, kDim);
    text(r, 50, 130, "Click a mode to start.", 1.5F, kDim);
    text(r, 50, 150, "P - pause, R - restart,", 1.5F, kDim);
    text(r, 50, 170, "M - menu, Esc - end run.", 1.5F, kDim);
    for (const GameMode m : core::kAllModes) {
        const SDL_FRect b = TempMenu::button_rect(m);
        const core::ModeLook& look =
            core::kModeLooks.at(static_cast<std::size_t>(core::mode_index(m)));
        const bool hover = TempMenu::button_at(mouse) == m;
        fill_rect(r, b.x, b.y, b.w, b.h, with_alpha(look.button, hover ? 255 : 200));
        text_centered(r, b.x + b.w / 2, b.y + 9, look.title, 2, kText);
    }
    const std::string best =
        std::format("Best Score: {}  Best Acc: {:.1f}%  Best Prec: {:.1f}%  Best Avg: {:.1f}ms",
                    p.best_score, p.best_accuracy, p.best_precision, p.best_avg_reaction);
    text(r, 16, 548, best, 1.5F, kDim);
    text(r, 16, 572, std::format("Games: {}", p.games_played), 1.5F, kDim);
    text(r, 510, 572, std::format("Profile {}", overlay.profile), 1.5F, kDim);
}

void render_countdown(SDL_Renderer* r, const core::Flow& flow, core::Nanoseconds now) {
    const core::ModeLook& look =
        core::kModeLooks.at(static_cast<std::size_t>(core::mode_index(flow.mode())));
    text_centered(r, kW / 2, 160, look.title, 2.5F, kDim);
    text_centered(r, kW / 2, 230, std::to_string(flow.countdown_value(now)), 12, kText);
    text_centered(r, kW / 2, 360, "Get ready!", 2, kDim);
}

void render_target(SDL_Renderer* r, const core::Flow& flow, const core::Theme& theme,
                   core::Nanoseconds now) {
    const core::Target& t = flow.game()->target();
    const auto radius = static_cast<float>(t.radius_at(now));
    if (radius <= 0) return;
    const auto x = static_cast<float>(t.center().x);
    const auto y = static_cast<float>(t.center().y);
    if (t.is_bomb()) {
        fill_circle(r, x, y, radius, core::kBombFill);
        const float cross = radius * 0.5F;
        thick_line(r, x - cross, y - cross, x + cross, y + cross, 3, core::kWhite);
        thick_line(r, x - cross, y + cross, x + cross, y - cross, 3, core::kWhite);
        return;
    }
    fill_circle(r, x, y, radius, theme.circles.at(static_cast<std::size_t>(t.color())));
    if (flow.mode() == GameMode::Tracking) {
        // Кольцо наводки: чем ближе к засчитыванию, тем ярче.
        if (t.hover_progress() > 0) {
            const double share = core::to_ms(t.hover_progress()) / core::to_ms(core::kTrackingHold);
            circle_outline(r, x, y, radius + 6, 3,
                           with_alpha(core::kWhite, static_cast<int>(255 * share)));
        }
    } else if (radius > 8) {
        // Зоны точности на 50 % и 25 % радиуса.
        circle_outline(r, x, y, radius * 0.5F, 1, with_alpha(core::kWhite, 90));
        circle_outline(r, x, y, radius * 0.25F, 1, with_alpha(core::kWhite, 90));
    }
}

void render_game(SDL_Renderer* r, const core::Flow& flow, const core::Theme& theme,
                 core::Nanoseconds now, const Overlay& overlay) {
    const core::Game& g = *flow.game();
    if (flow.mode() == GameMode::Dual) {
        fill_circle(r, 20, 20, 6, theme.circles[0]);
        text(r, 32, 14, "LMB", 1.5F, kText);
        fill_circle(r, 90, 20, 6, theme.circles[1]);
        text(r, 102, 14, "RMB", 1.5F, kText);
    }
    render_target(r, flow, theme, now);
    if (g.combo() >= 2) text(r, kW - 130, 14, std::format("Combo x{}", g.combo()), 1.5F, kText);
    fill_rect(r, 0, kH - core::kHudReserve + 20, kW, core::kHudReserve - 20, {60, 60, 65, 220});
    const std::string hud =
        std::format("FPS {}  Score {}  Acc {:.1f}%  Avg {:.1f}ms  Lives {}", overlay.fps, g.score(),
                    g.accuracy(), g.avg_reaction_ms(), g.lives_left());
    text(r, 16, kH - 27, hud, 1.5F, kText);
    if (g.paused()) {
        fill_rect(r, 0, 0, kW, kH, {15, 15, 18, 150});
        text_centered(r, kW / 2, 230, "PAUSE", 5, kText);
        text_centered(r, kW / 2, 300, "P - continue, R - restart, M - menu", 1.5F, kDim);
    }
}

void render_heatmap(SDL_Renderer* r, const core::Game& g) {
    constexpr float kCell = 10;
    constexpr float kLeft = kW - core::kHeatmapCols * kCell - 20;
    constexpr float kTop = 20;
    text(r, kLeft, kTop - 14, "Misses (last run)", 1, kDim);
    fill_rect(r, kLeft, kTop, core::kHeatmapCols * kCell, core::kHeatmapRows * kCell,
              {255, 255, 255, 12});
    int most = 1;
    for (const auto& column : g.miss_heatmap())
        for (const int n : column)
            most = std::max(most, n);
    for (int col = 0; col < core::kHeatmapCols; ++col) {
        for (int row = 0; row < core::kHeatmapRows; ++row) {
            const int n =
                g.miss_heatmap()[static_cast<std::size_t>(col)][static_cast<std::size_t>(row)];
            if (n == 0) continue;
            fill_rect(r, kLeft + static_cast<float>(col) * kCell,
                      kTop + static_cast<float>(row) * kCell, kCell, kCell,
                      with_alpha(kRed, 80 + 175 * n / most));
        }
    }
}

void render_game_over(SDL_Renderer* r, const core::Flow& flow) {
    const core::Game& g = *flow.game();
    render_heatmap(r, g);
    float y = 230;
    if (g.died_to_bomb()) {
        text_centered(r, kW / 2, 170, "BOMB!", 4, kRed);
    }
    text_centered(r, kW / 2, y, "GAME OVER", 6, kText);
    y += 80;
    text_centered(r, kW / 2, y, "Press SPACE to continue  (R - again)", 2, kDim);
    y += 40;
    const core::GameSummary s = g.summary();
    for (const std::string& line :
         {std::format("Score: {}", s.score), std::format("Accuracy: {:.1f}%", s.accuracy),
          std::format("Avg Reaction: {:.1f}ms", s.avg_reaction_ms),
          std::format("Precision: {:.1f}%", s.precision),
          std::format("Hits: {}  Misses: {}", s.hits, s.misses),
          std::format("Max Combo: x{}", s.max_combo)}) {
        text_centered(r, kW / 2, y, line, 2, kText);
        y += 24;
    }
}

}  // namespace

SDL_FRect TempMenu::button_rect(GameMode mode) {
    const auto i = static_cast<float>(core::mode_index(mode));
    return {kButtonsX, kButtonsY + i * (kButtonH + kButtonGap), kButtonW, kButtonH};
}

std::optional<GameMode> TempMenu::button_at(core::Vec2 p) {
    for (const GameMode m : core::kAllModes) {
        const SDL_FRect b = button_rect(m);
        if (p.x >= b.x && p.x <= b.x + b.w && p.y >= b.y && p.y <= b.y + b.h) return m;
    }
    return std::nullopt;
}

std::optional<GameMode> TempMenu::click(const core::ClickInput& click) {
    if (click.button != core::MouseButton::Left) return std::nullopt;
    return button_at(click.position);
}

void render(SDL_Renderer* renderer, const core::Flow& flow, const core::Profile& profile,
            core::Nanoseconds now, const Overlay& overlay) {
    const core::Theme& theme = theme_of(profile);
    set_color(renderer, theme.background);
    SDL_RenderClear(renderer);
    switch (flow.screen()) {
        case core::Screen::Menu: render_menu(renderer, profile, overlay); break;
        case core::Screen::Countdown: render_countdown(renderer, flow, now); break;
        case core::Screen::Game: render_game(renderer, flow, theme, now, overlay); break;
        case core::Screen::GameOver: render_game_over(renderer, flow); break;
    }
    if (!overlay.toast.empty()) {
        // Длинное сообщение (ошибка файла) — мельче, чтобы поместилось.
        const float scale = text_width(overlay.toast, 1.5F) + 24 <= kW - 16 ? 1.5F : 1.0F;
        const float w = text_width(overlay.toast, scale) + 24;
        fill_rect(renderer, (kW - w) / 2, 8, w, 28, {40, 40, 46, 230});
        text_centered(renderer, kW / 2, scale > 1.0F ? 16.0F : 18.0F, overlay.toast, scale,
                      theme.accent);
    }
}

}  // namespace aim::app
