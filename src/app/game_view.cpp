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
int round_int(double v) {
    return core::round_half_even(v);
}

/// Format('{0:0.0}') образца.
std::string one_decimal(double v) {
    return std::format("{:.1f}", v);
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
        c.brush = theme.bomb;
        c.fill_circle(x, y, r);
        c.pen = theme.bomb_cross;
        c.pen_width = 3;
        const int cross = round_int(r * 0.5);
        c.line(x - cross, y - cross, x + cross, y + cross);
        c.line(x - cross, y + cross, x + cross, y - cross);
        return;
    }
    c.brush = theme.target(t.color());
    c.fill_circle(x, y, r);
    if (game.options().action == core::Action::Hover) {
        // Кольцо наводки: чем ближе к засчитыванию, тем ярче.
        if (t.hover_progress() > 0) {
            const int progress = std::min(round_int(255.0 * core::to_ms(t.hover_progress()) /
                                                    core::to_ms(core::kTrackingHold)),
                                          255);
            c.pen = with_alpha(theme.text, progress);
            c.pen_width = 3;
            c.circle(x, y, r + 6);
        }
    } else if (r > 8) {
        // Зоны точности на 50 % и 25 % радиуса.
        c.pen = faded(theme.text, 90.0 / 255);
        c.pen_width = 1;
        c.circle(x, y, round_int(r * 0.5));
        c.circle(x, y, round_int(r * 0.25));
    }
}

void render_heatmap(Canvas& c, const core::Theme& theme, const core::Game& game) {
    const core::MissHeatmap& map = game.miss_heatmap();
    int most = 1;
    for (const auto& column : map)
        for (const int n : column)
            most = std::max(most, n);
    constexpr int kOriginX = 40;
    constexpr int kOriginY = kH - 150;
    constexpr int kCellW = 200 / core::kHeatmapCols;
    constexpr int kCellH = 100 / core::kHeatmapRows;

    c.font.size = kTextSize;
    c.font_color = theme.text_dim;
    c.draw_text_centered(kOriginX, kOriginY - 22, kOriginX + 200, kOriginY, "Промахи");
    c.brush = theme.graph;
    c.fill_round_rect(kOriginX, kOriginY, kOriginX + 200, kOriginY + 100, 4, 4);
    for (int cx = 0; cx < core::kHeatmapCols; ++cx) {
        for (int cy = 0; cy < core::kHeatmapRows; ++cy) {
            const int n = map[static_cast<std::size_t>(cx)][static_cast<std::size_t>(cy)];
            if (n <= 0) continue;
            const int alpha = std::max(round_int(220.0 * n / most), 40);
            const int gx = kOriginX + cx * kCellW;
            const int gy = kOriginY + cy * kCellH;
            c.brush = with_alpha(theme.target_right, alpha);
            c.fill_round_rect(gx, gy, gx + kCellW, gy + kCellH, 1, 1);
        }
    }
}

}  // namespace

void render_countdown(Canvas& c, const core::Theme& theme, const core::Flow& flow,
                      core::Nanoseconds now) {
    c.clear_window(theme.background);
    const core::MeasureOptions& o = flow.options();
    c.font.size = kTitleSize;
    c.font_color = theme.text;
    c.draw_text_centered(0, kH / 2 - 154, kW, kH / 2 - 120, core::combo_name(o));
    c.font.size = kTextSize;
    c.draw_text_centered(40, kH / 2 - 118, kW - 40, kH / 2 - 96, core::describe_short(o));
    c.font.size = kCountdownSize;
    c.draw_text_centered(0, kH / 2 - 100, kW, kH / 2 + 100,
                         std::to_string(flow.countdown_value(now)));
    c.font.size = kTitleSize;
    c.draw_text_centered(
        0, kH / 2 + 114, kW, kH / 2 + 148,
        o.stimulus == core::Stimulus::Screen ? "Жми, как только окно сменит цвет" : "Приготовься!");
}

// Ячейка панели замера (D-43): подпись и значение в одну строку; поле
// значения — по самому длинному значению (widest): цифры DejaVu одной
// ширины, так что значение у правого края поля не прыгает.
struct HudCell {
    std::string label;
    std::string value;
    std::string_view widest;
};

std::array<HudCell, 5> hud_cells(const core::Game& game, int fps, core::Nanoseconds now) {
    const auto ms_text = [](double v) { return std::format("{} мс", round_int(v)); };
    const core::MeasureOptions& o = game.options();
    HudCell ending =
        o.ending == core::Ending::Time
            ? HudCell{"Время",
                      std::format("{} с", (game.time_left(now) + core::ms(999)) / core::ms(1000)),
                      "300 с"}
            : HudCell{"Жизни", std::to_string(std::max(game.lives_left(), 0)), "30"};
    if (game.screen_stimulus()) {
        const double last = game.reactions().empty() ? 0.0 : core::to_ms(game.reactions().back());
        return {{{"FPS", std::to_string(fps), "9999"},
                 {"Реакция", ms_text(last), "9999 мс"},
                 {"Медиана", ms_text(game.median_reaction_ms()), "9999 мс"},
                 {"Фальстарты", std::to_string(game.misses()), "999"},
                 std::move(ending)}};
    }
    return {{{"FPS", std::to_string(fps), "9999"},
             {"Очки", std::to_string(game.score()), "99999"},
             {"Точность", one_decimal(game.accuracy()) + "%", "100.0%"},
             {"Средняя", ms_text(game.avg_reaction_ms()), "9999 мс"},
             std::move(ending)}};
}

// Панель замера внизу: ячейки по ширине текста, посередине окна.
void render_hud(Canvas& c, const core::Theme& theme, const std::array<HudCell, 5>& cells) {
    constexpr int kPad = 10;   // от края ячейки до текста
    constexpr int kGap = 8;    // между подписью и значением
    constexpr int kSpace = 8;  // между ячейками
    c.font = {.size = kTextSize, .weight = Weight::Normal};
    const int line_h = c.line_height();
    const int h = line_h + 12;
    const int y = kH - 16 - h;
    std::array<int, 5> widths{};
    int total = kSpace * (static_cast<int>(cells.size()) - 1);
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const HudCell& cell = cells.at(i);
        const int field = std::max(c.text_width(cell.widest), c.text_width(cell.value));
        widths.at(i) = c.text_width(cell.label) + kGap + field + 2 * kPad;
        total += widths.at(i);
    }
    int x = (kW - total) / 2;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const HudCell& cell = cells.at(i);
        const int w = widths.at(i);
        c.brush = theme.hud;
        c.fill_round_rect(x, y, x + w, y + h, 12, 12);
        c.brush = core::Rgba{0, 0, 0, 0};
        const int ty = y + (h - line_h) / 2;
        c.font_color = theme.label;
        c.text_out(x + kPad, ty, cell.label);
        c.font_color = theme.text;
        c.text_out(x + w - kPad - c.text_width(cell.value), ty, cell.value);
        x += w + kSpace;
    }
}

void render_game(Canvas& c, const core::Theme& theme, const core::Game& game, Effects& effects,
                 const CursorTrail& trail, int fps, core::Nanoseconds now, bool hud) {
    const bool screen = game.screen_stimulus();
    // Замер на цвет: окно целиком — фон или сигнал (D-37).
    c.clear_window(screen && game.signal_lit(now) ? theme.signal : theme.background);
    if (!screen) {
        trail.render(c, theme);
        render_target(c, theme, game, now);
    }
    effects.render(c, theme, now);
    if (!hud) return;  // интерфейс замера выключен (D-45)

    const int old_size = c.font.size;
    const core::MeasureOptions& o = game.options();
    render_hud(c, theme, hud_cells(game, fps, now));
    c.font.size = old_size;

    if (!screen && o.action == core::Action::Click && o.color_buttons) {
        // Подсказка «какой цвет какой кнопкой»; цели под ней не появляются.
        c.brush = theme.overlay;
        c.fill_round_rect(10, 8, 190, 44, 8, 8);
        c.brush = theme.target_left;
        c.fill_circle(26, 26, 7);
        c.brush = theme.target_right;
        c.fill_circle(106, 26, 7);
        c.font.size = kTextSize;
        c.font_color = theme.text;
        c.draw_text_centered(36, 15, 96, 37, "ЛКМ");
        c.draw_text_centered(116, 15, 180, 37, "ПКМ");
        c.font.size = old_size;
    }
    if (game.combo() >= 2) {
        c.brush = theme.overlay;
        c.fill_round_rect(kW - 130, 10, kW - 10, 40, 8, 8);
        c.font.size = kTextSize;
        c.font_color = game.combo() >= 10 ? theme.gold : theme.text;
        c.draw_text_centered(kW - 130, 10, kW - 10, 40, std::format("Серия x{}", game.combo()));
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
        case core::EndReason::Lives: title = "ЖИЗНИ КОНЧИЛИСЬ"; break;
        case core::EndReason::None:
        case core::EndReason::GaveUp:
        case core::EndReason::ToMenu: break;
    }
    // Верх окна (20–60) — место уведомлений: заголовок ниже.
    c.font.size = kHeadlineSize;
    c.font_color = theme.text;
    c.draw_text_centered(0, 60, kW, 114, title);
    c.font.size = kTextSize;
    c.font_color = theme.text_dim;
    c.draw_text_centered(0, 112, kW, 132,
                         std::format("{} — Space: в меню, R: заново", core::combo_name(o)));

    // Слева — параметры замера (D-38).
    c.font.size = kTextSize;
    int y = 150;
    c.font_color = theme.accent;
    c.draw_text_centered(40, y, 380, y + 20, "Параметры");
    y += 26;
    for (const std::string& line : core::describe(o)) {
        c.pair_centered(40, y, 380, y + 20, line, theme.label, theme.text);
        y += 22;
    }

    // Справа — меры, по одной на строку: строка не шире столбца.
    y = 150;
    c.font_color = theme.accent;
    c.draw_text_centered(420, y, 760, y + 20, "Результат");
    y += 26;
    // Подпись — приглушённо, значение — ярко (D-43).
    const auto row = [&](const std::string& text, std::optional<core::Rgba> value = {}) {
        c.pair_centered(420, y, 760, y + 20, text, theme.label, value.value_or(theme.text));
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
        c.font.weight = Weight::Bold;
        c.font_color = theme.gold;
        c.draw_text_centered(420, y, 760, y + 24, "Новый рекорд набора!");
        c.font.weight = Weight::Normal;
        y += 30;
    }
    const core::Rgba gold = theme.gold;
    if (st.best)
        row("Рекорд набора: " + primary(core::primary_value(profile.runs.at(*st.best).summary)),
            gold);
    if (st.average.runs > 0)
        row(std::format("Среднее за {}: {}", st.average.runs, primary(st.average.primary)), gold);

    if (!game.screen_stimulus()) render_heatmap(c, theme, game);
}

void render_replay(Canvas& c, const core::Theme& theme, const core::ReplayPlayer& replay,
                   Effects& effects, const CursorTrail& trail, int fps, core::Nanoseconds now) {
    const core::Game& game = replay.game();
    render_game(c, theme, game, effects, trail, fps, now);
    // Курсор игрока из записи: кольцо с крестом — свой курсор зрителя другой.
    const int x = core::round_half_even(replay.cursor().x);
    const int y = core::round_half_even(replay.cursor().y);
    c.pen = theme.text;
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
    c.brush = theme.overlay;
    c.fill_round_rect(kX1, 6, kX2, 56, 8, 8);
    c.font.size = kTextSize;
    c.font_color = theme.good;
    c.draw_text_centered(kX1, 8, kX2, 30,
                         std::format("РЕПЛЕЙ · {} · {:02}.{:02}.{:04} {:02}:{:02}",
                                     core::combo_name(run.summary.options), w.day, w.month, w.year,
                                     w.hour, w.minute));
    c.font.size = kTextSize;
    c.font_color = theme.text_dim;
    c.draw_text_centered(
        kX1, 31, kX2, 53,
        finished ? "Реплей окончен · Esc, Space или M — в меню" : "Esc, Space или M — в меню");
}

}  // namespace aim::app
