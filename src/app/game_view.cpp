#include "app/game_view.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

#include "core/records.h"
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
    if (game.options().action == core::Action::Hover) {
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
    constexpr int kOriginX = 40;
    constexpr int kOriginY = kH - 150;
    constexpr int kCellW = 200 / core::kHeatmapCols;
    constexpr int kCellH = 100 / core::kHeatmapRows;

    c.font.size = 10;
    c.font_color = argb(180, 200, 200, 200);
    c.draw_text_centered(kOriginX, kOriginY - 18, kOriginX + 200, kOriginY, "Промахи");
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
    c.clear_window(theme.background);
    const core::MeasureOptions& o = flow.options();
    c.font.size = 20;
    c.font_color = argb(200, 230, 230, 230);
    c.draw_text_centered(0, kH / 2 - 150, kW, kH / 2 - 120, core::combo_name(o));
    c.font.size = 12;
    c.draw_text_centered(40, kH / 2 - 118, kW - 40, kH / 2 - 100, core::describe_short(o));
    c.font.size = 120;
    c.draw_text_centered(0, kH / 2 - 100, kW, kH / 2 + 100,
                         std::to_string(flow.countdown_value(now)));
    c.font.size = 20;
    c.draw_text_centered(
        0, kH / 2 + 116, kW, kH / 2 + 146,
        o.stimulus == core::Stimulus::Screen ? "Жми, как только окно сменит цвет" : "Get ready!");
}

void render_game(Canvas& c, const core::Theme& theme, const core::Game& game, Effects& effects,
                 const CursorTrail& trail, int fps, core::Nanoseconds now) {
    const bool screen = game.screen_stimulus();
    // Замер на цвет: окно целиком — фон или сигнал (D-37).
    c.clear_window(screen && game.signal_lit(now) ? theme.signal : theme.background);
    if (!screen) {
        trail.render(c);
        render_target(c, theme, game, now);
    }
    effects.render(c, now);

    const int old_size = c.font.size;
    const core::Rgba bg = argb(220, 60, 60, 65);
    constexpr int kInfoH = 60;
    constexpr int kCol = kW / 5;
    constexpr int kPad = 16;
    constexpr int kBottom = 16;
    const core::MeasureOptions& o = game.options();
    const std::string end_box =
        o.ending == core::Ending::Time
            ? std::format("Time:\n{}", core::to_ms(game.time_left(now)) > 0
                                           ? (game.time_left(now) + core::ms(999)) / core::ms(1000)
                                           : 0)
            : std::format("Lives:\n{}", std::max(game.lives_left(), 0));
    const std::array<std::string, 5> boxes{
        std::format("FPS:\n{}", fps),
        screen ? std::format("Last:\n{}ms", game.reactions().empty()
                                                ? 0
                                                : round_int(core::to_ms(game.reactions().back())))
               : std::format("Score:\n{}", game.score()),
        screen ? std::format("Median:\n{}ms", round_int(game.median_reaction_ms()))
               : "Accuracy:\n" + one_decimal(game.accuracy()) + "%",
        screen ? std::format("False:\n{}", game.misses())
               : "Avg:\n" + one_decimal(game.avg_reaction_ms()) + "ms",
        end_box,
    };
    for (int i = 0; i < 5; ++i) {
        const int x1 = i == 0 ? kPad : i * kCol + kPad;
        const int x2 = i == 4 ? kW - kPad : (i + 1) * kCol - kPad;
        text_with_background(c, bg, x1, kH - kInfoH, x2, kH - kBottom, 16,
                             boxes.at(static_cast<std::size_t>(i)));
    }
    c.font.size = old_size;

    if (!screen && o.action == core::Action::Click && o.color_buttons) {
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

void render_game_over(Canvas& c, const core::Theme& theme, const core::Game& game,
                      const std::optional<core::RunFinished>& run, const core::Profile& profile) {
    c.clear_window(theme.background);
    const core::GameSummary s = game.summary();
    const core::MeasureOptions& o = s.options;
    std::string_view title = "ЗАМЕР ОКОНЧЕН";
    switch (s.end) {
        case core::EndReason::Bomb: title = "ПОДРЫВ!"; break;
        case core::EndReason::Time: title = "ВРЕМЯ ВЫШЛО"; break;
        case core::EndReason::Lives: title = "GAME OVER"; break;
        case core::EndReason::None:
        case core::EndReason::GaveUp:
        case core::EndReason::ToMenu: break;
    }
    // Верх окна (20–60) — место уведомлений: заголовок ниже.
    c.font.size = 32;
    c.font_color = kClWhite;
    c.draw_text_centered(0, 66, kW, 110, title);
    c.font.size = 14;
    c.font_color = argb(220, 220, 220, 220);
    c.draw_text_centered(0, 112, kW, 132,
                         std::format("{} — Space: в меню, R: заново", core::combo_name(o)));

    // Слева — параметры замера (D-38).
    c.font.size = 13;
    int y = 150;
    c.font_color = argb(200, 160, 200, 255);
    c.draw_text_centered(40, y, 380, y + 20, "Параметры");
    y += 26;
    c.font_color = argb(220, 220, 220, 220);
    for (const std::string& line : core::describe(o)) {
        c.draw_text_centered(40, y, 380, y + 20, line);
        y += 22;
    }

    // Справа — меры, по одной на строку: строка не шире столбца.
    y = 150;
    c.font_color = argb(200, 160, 200, 255);
    c.draw_text_centered(420, y, 760, y + 20, "Результат");
    y += 26;
    c.font_color = kClWhite;
    const auto row = [&](const std::string& text) {
        c.draw_text_centered(420, y, 760, y + 20, text);
        y += 22;
    };
    if (o.stimulus == core::Stimulus::Target && o.action == core::Action::Hover) {
        row(std::format("Очки: {}", s.score));
        row(std::format("Засчитано целей: {}", s.tracked));
        row(std::format("Пропущено целей: {}", s.expired));
    } else if (game.screen_stimulus()) {
        row(std::format("Медиана реакции: {:.0f} мс", s.median_reaction_ms));
        row(std::format("Средняя реакция: {:.0f} мс", s.avg_reaction_ms));
        row(std::format("Реакций: {}", s.hits));
        row(std::format("Фальстартов: {}", s.misses));
    } else {
        row(std::format("Очки: {}", s.score));
        row("Точность: " + one_decimal(s.accuracy) + "%");
        row("Точность попадания: " + one_decimal(s.precision) + "%");
        row(std::format("Реакция средняя: {:.0f} мс", s.avg_reaction_ms));
        row(std::format("Реакция медиана: {:.0f} мс", s.median_reaction_ms));
        row(std::format("Попаданий: {}, промахов: {}", s.hits, s.misses));
        row(std::format("Пропущено целей: {}", s.expired));
        row(std::format("Лучшая серия: x{}", s.max_combo));
    }
    row(std::format("Длительность: {:.1f} с", s.duration_ms / 1000.0));

    // Рекорд и среднее за N набора.
    const core::ComboStats st = core::combo_stats(profile.runs, o, profile.settings.average_runs);
    const auto primary = [&](double v) {
        return core::lower_is_better(o) ? std::format("{:.0f} мс", v) : std::format("{:.0f}", v);
    };
    y += 10;
    if (run && run->new_best) {
        c.font.size = 16;
        c.font_color = argb(255, 255, 210, 90);
        c.draw_text_centered(420, y, 760, y + 24, "Новый рекорд набора!");
        y += 30;
        c.font.size = 13;
    }
    c.font_color = argb(220, 255, 210, 90);
    if (st.best)
        row("Рекорд набора: " + primary(core::primary_value(profile.runs.at(*st.best).summary)));
    if (st.average.runs > 0)
        row(std::format("Среднее за {}: {}", st.average.runs, primary(st.average.primary)));

    if (!game.screen_stimulus()) render_heatmap(c, game);
}

void render_replay(Canvas& c, const core::Theme& theme, const core::ReplayPlayer& replay,
                   Effects& effects, const CursorTrail& trail, int fps, core::Nanoseconds now) {
    const core::Game& game = replay.game();
    render_game(c, theme, game, effects, trail, fps, now);
    // Курсор игрока из записи: кольцо с крестом — свой курсор зрителя другой.
    const int x = core::round_half_even(replay.cursor().x);
    const int y = core::round_half_even(replay.cursor().y);
    c.pen = argb(230, 255, 255, 255);
    c.pen_width = 2;
    c.draw_circle(x, y, 8);
    c.line(x - 14, y, x - 4, y);
    c.line(x + 4, y, x + 14, y);
    c.line(x, y - 14, x, y - 4);
    c.line(x, y + 4, x, y + 14);
    c.pen_width = 1;
    // Плашка сверху: что проигрывается и как выйти.
    const core::RunRecord& run = replay.replay().run;
    const core::LocalTime& w = run.when;
    const bool finished = replay.done() && game.over();
    constexpr int kX1 = 200;
    constexpr int kX2 = kW - 140;
    c.brush = argb(190, 40, 40, 45);
    c.fill_round_rect(kX1, 8, kX2, 50, 8, 8);
    c.font.size = 12;
    c.font_color = argb(255, 120, 220, 140);
    c.draw_text_centered(kX1, 10, kX2, 28,
                         std::format("РЕПЛЕЙ · {} · {:02}.{:02}.{:04} {:02}:{:02}",
                                     core::combo_name(run.summary.options), w.day, w.month, w.year,
                                     w.hour, w.minute));
    c.font.size = 11;
    c.font_color = argb(220, 220, 220, 220);
    c.draw_text_centered(
        kX1, 29, kX2, 47,
        finished ? "Реплей окончен · Esc, Space или M — в меню" : "Esc, Space или M — в меню");
}

}  // namespace aim::app
