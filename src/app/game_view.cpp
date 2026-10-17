#include "app/game_view.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>

#include "app/draw_util.h"
#include "core/layout.h"
#include "core/records.h"
#include "core/rules.h"
#include "core/screen.h"

namespace aim::app {

namespace {

constexpr int kW = core::kWindowWidth;
constexpr int kH = core::kWindowHeight;

// Метка стороны на цели «по цвету» (D-48): полукольцо внутри цели — у ЛКМ
// слева, у ПКМ справа; не только цвет говорит, какой кнопкой жать.
void side_mark(Canvas& c, core::Rgba color, int x, int y, int r, int button) {
    // Углы окна: от низа (π/2) через левый край до верха — левая половина;
    // от верха (−π/2) через правый край — правая.
    const double from = button == 1 ? -std::numbers::pi / 2 : std::numbers::pi / 2;
    c.arc(x, y, r * 0.72, from, std::numbers::pi,
          {.color = color, .width = std::max(2, round_int(r * 0.14))});
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
        c.fill_circle(x, y, r, theme.bomb);
        const Pen cross{.color = theme.bomb_cross, .width = 3};
        const int k = round_int(r * 0.5);
        c.line(x - k, y - k, x + k, y + k, cross);
        c.line(x - k, y + k, x + k, y - k, cross);
        return;
    }
    const core::Rgba fill = theme.target(t.color());
    c.fill_circle(x, y, r, fill);
    // Мельче — метка не видна; цвет есть и так.
    constexpr int kMarkMinRadius = 6;
    if (game.options().color_buttons && r >= kMarkMinRadius)
        side_mark(c, theme.target_mark, x, y, r, t.color());
    if (game.options().action == core::Action::Hover) {
        // Кольцо наводки: чем ближе к засчитыванию, тем ярче.
        if (t.hover_progress() > 0) {
            const int progress = std::min(round_int(255.0 * core::to_ms(t.hover_progress()) /
                                                    core::to_ms(core::kTrackingHold)),
                                          255);
            c.stroke_circle(x, y, r + 6, {.color = with_alpha(theme.text, progress), .width = 3});
        }
    } else if (r > 8) {
        // Зоны точности на 50 % и 25 % радиуса: цвет цели ещё раз — середина
        // плотнее, и тонкое кольцо.
        const Pen ring{.color = faded(theme.text, 90.0 / 255)};
        for (const double share : {0.5, 0.25}) {
            const int zone = round_int(r * share);
            c.fill_circle(x, y, zone, fill);
            c.stroke_circle(x, y, zone, ring);
        }
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

    c.text_centered(kOriginX, kOriginY - 22, kOriginX + 200, kOriginY, "Промахи", kText,
                    theme.text_dim);
    c.fill_round_rect(kOriginX, kOriginY, kOriginX + 200, kOriginY + 100, 4, 4, theme.graph);
    for (int cx = 0; cx < core::kHeatmapCols; ++cx) {
        for (int cy = 0; cy < core::kHeatmapRows; ++cy) {
            const int n = map[static_cast<std::size_t>(cx)][static_cast<std::size_t>(cy)];
            if (n <= 0) continue;
            const int alpha = std::max(round_int(220.0 * n / most), 40);
            const int gx = kOriginX + cx * kCellW;
            const int gy = kOriginY + cy * kCellH;
            c.fill_round_rect(gx, gy, gx + kCellW, gy + kCellH, 1, 1,
                              with_alpha(theme.target_right, alpha));
        }
    }
}

}  // namespace

void render_countdown(Canvas& c, const core::Theme& theme, const core::Flow& flow,
                      core::Nanoseconds now) {
    c.clear(theme.background);
    const core::MeasureOptions& o = flow.options();
    c.text_centered(0, kH / 2 - 154, kW, kH / 2 - 120, core::combo_name(o), kTitle, theme.text);
    c.text_centered(40, kH / 2 - 118, kW - 40, kH / 2 - 96, core::describe_short(o), kText,
                    theme.text);
    c.text_centered(0, kH / 2 - 100, kW, kH / 2 + 100, std::to_string(flow.countdown_value(now)),
                    kCountdown, theme.text);
    c.text_centered(
        0, kH / 2 + 114, kW, kH / 2 + 148,
        o.stimulus == core::Stimulus::Screen ? "Жми, как только окно сменит цвет" : "Приготовься!",
        kTitle, theme.text);
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
    const int line_h = c.line_height(kText);
    const int h = line_h + 12;
    const int y = kH - 16 - h;
    std::array<int, 5> widths{};
    int total = kSpace * (static_cast<int>(cells.size()) - 1);
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const HudCell& cell = cells.at(i);
        const int field =
            std::max(c.text_width(cell.widest, kText), c.text_width(cell.value, kText));
        widths.at(i) = c.text_width(cell.label, kText) + kGap + field + 2 * kPad;
        total += widths.at(i);
    }
    int x = (kW - total) / 2;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const HudCell& cell = cells.at(i);
        const int w = widths.at(i);
        c.fill_round_rect(x, y, x + w, y + h, 12, 12, theme.hud);
        const int ty = y + (h - line_h) / 2;
        c.text(x + kPad, ty, cell.label, kText, theme.label);
        c.text(x + w - kPad - c.text_width(cell.value, kText), ty, cell.value, kText, theme.text);
        x += w + kSpace;
    }
}

void render_game(Canvas& c, const core::Theme& theme, const core::Game& game, Effects& effects,
                 const CursorTrail& trail, int fps, core::Nanoseconds now, bool hud) {
    const bool screen = game.screen_stimulus();
    // Замер на цвет: окно целиком — фон или сигнал (D-37).
    c.clear(screen && game.signal_lit(now) ? theme.signal : theme.background);
    if (!screen) {
        trail.render(c, theme);
        render_target(c, theme, game, now);
    }
    effects.render(c, theme, now);
    if (!hud) return;  // интерфейс замера выключен (D-45)

    const core::MeasureOptions& o = game.options();
    render_hud(c, theme, hud_cells(game, fps, now));

    if (!screen && o.action == core::Action::Click && o.color_buttons) {
        // Подсказка «какой цвет какой кнопкой» — с теми же метками стороны
        // (D-48); цели под ней не появляются.
        c.fill_round_rect(10, 8, 190, 44, 8, 8, theme.overlay);
        constexpr int kHintR = 10;
        c.fill_circle(26, 26, kHintR, theme.target_left);
        side_mark(c, theme.target_mark, 26, 26, kHintR, 0);
        c.fill_circle(106, 26, kHintR, theme.target_right);
        side_mark(c, theme.target_mark, 106, 26, kHintR, 1);
        c.text_centered(36, 15, 96, 37, "ЛКМ", kText, theme.text);
        c.text_centered(116, 15, 180, 37, "ПКМ", kText, theme.text);
    }
    if (game.combo() >= 2) {
        c.fill_round_rect(kW - 130, 10, kW - 10, 40, 8, 8, theme.overlay);
        c.text_centered(kW - 130, 10, kW - 10, 40, std::format("Серия x{}", game.combo()), kText,
                        game.combo() >= 10 ? theme.gold : theme.text);
    }
}

// Итог слева — параметры замера (D-38).
void render_params(Canvas& c, const core::Theme& theme, const core::MeasureOptions& o) {
    constexpr int kP1 = core::kResultParamsX1;
    constexpr int kP2 = core::kResultParamsX2;
    int y = 150;
    c.text_centered(kP1, y, kP2, y + 20, "Параметры", kText, theme.accent);
    y += 26;
    for (const std::string& line : core::describe(o)) {
        c.pair_centered(kP1, y, kP2, y + 20, line, kText, theme.label, theme.text);
        y += 22;
    }
}

// Итог справа — меры, по одной на строку, рекорд и среднее за N набора.
void render_measures(Canvas& c, const core::Theme& theme, const core::Game& game,
                     const std::optional<core::RunFinished>& run, const core::Profile& profile) {
    const core::GameSummary s = game.summary();
    const core::MeasureOptions& o = s.options;
    // Строка не шире столбца.
    constexpr int kM1 = core::kResultMeasuresX1;
    constexpr int kM2 = core::kResultMeasuresX2;
    int y = 150;
    c.text_centered(kM1, y, kM2, y + 20, "Результат", kText, theme.accent);
    y += 26;
    // Подпись — приглушённо, значение — ярко (D-43).
    const auto row = [&](const std::string& text, std::optional<core::Rgba> value = {}) {
        c.pair_centered(kM1, y, kM2, y + 20, text, kText, theme.label, value.value_or(theme.text));
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
    const auto primary = [&](double v) { return primary_text(o, v); };
    y += 10;
    if (run && run->new_best) {
        c.text_centered(kM1, y, kM2, y + 24, "Новый рекорд набора!", kTextBold, theme.gold);
        y += 30;
    }
    if (st.best)
        row("Рекорд набора: " + primary(core::primary_value(profile.runs.at(*st.best).summary)),
            theme.gold);
    if (st.average.runs > 0)
        row(std::format("Среднее за {}: {}", st.average.runs, primary(st.average.primary)),
            theme.gold);
}

// Кнопки итога (D-46): до kResultButtonsDelay — бледные, под курсором —
// с обводкой.
void render_result_buttons(Canvas& c, const core::Theme& theme, const core::Flow& flow,
                           core::Vec2 mouse, core::Nanoseconds now) {
    const bool active = now >= flow.result_shown_at() + core::kResultButtonsDelay;
    const bool with_replay = flow.replay_offered();
    struct Button {
        core::ResultButton id;
        std::string_view text;
        core::Rgba fill;
    };
    const std::array<Button, 3> buttons{{{core::ResultButton::Again, "Ещё раз", theme.good},
                                         {core::ResultButton::Menu, "В меню", theme.control},
                                         {core::ResultButton::Replay, "▶ Реплей", theme.control}}};
    for (const Button& b : buttons) {
        if (b.id == core::ResultButton::Replay && !with_replay) continue;
        const core::Rect r = core::result_button(b.id, with_replay);
        c.fill_round_rect(r, 8, 8, active ? b.fill : faded(b.fill, 0.4));
        if (active && r.contains(mouse))
            c.stroke_round_rect(r, 8, 8, {.color = theme.text, .width = 2});
        c.text_centered(r, b.text, kTextBold, active ? theme.text : faded(theme.text, 0.5));
    }
    const int y = core::kResultButtonsY + core::kResultButtonH + 6;
    c.text_centered(core::kResultMeasuresX1, y, core::kResultMeasuresX2, y + 22,
                    "Space или M — в меню, R — заново", kText, theme.text_dim);
}

void render_game_over(Canvas& c, const core::Theme& theme, const core::Game& game,
                      const core::Flow& flow, const core::Profile& profile, core::Vec2 mouse,
                      core::Nanoseconds now) {
    c.clear(theme.background);
    const core::MeasureOptions& o = game.options();
    std::string_view title = "ЗАМЕР ОКОНЧЕН";
    switch (game.end_reason()) {
        case core::EndReason::Bomb: title = "ПОДРЫВ!"; break;
        case core::EndReason::Time: title = "ВРЕМЯ ВЫШЛО"; break;
        case core::EndReason::Lives: title = "ЖИЗНИ КОНЧИЛИСЬ"; break;
        case core::EndReason::None:
        case core::EndReason::GaveUp:
        case core::EndReason::ToMenu: break;
    }
    // Верх окна (20–60) — место уведомлений: заголовок ниже.
    c.text_centered(0, 60, kW, 114, title, kHeadline, theme.text);
    c.text_centered(0, 112, kW, 132, core::combo_name(o), kText, theme.text_dim);

    render_params(c, theme, o);
    render_measures(c, theme, game, flow.last_run(), profile);
    if (!game.screen_stimulus()) render_heatmap(c, theme, game);
    render_result_buttons(c, theme, flow, mouse, now);
}

void render_replay(Canvas& c, const core::Theme& theme, const core::ReplayPlayer& replay,
                   core::Screen back, Effects& effects, const CursorTrail& trail, int fps,
                   core::Nanoseconds now) {
    const core::Game& game = replay.game();
    render_game(c, theme, game, effects, trail, fps, now);
    // Курсор игрока из записи: кольцо с крестом — свой курсор зрителя другой.
    const int x = round_int(replay.cursor().x);
    const int y = round_int(replay.cursor().y);
    const Pen pen{.color = theme.text, .width = 2};
    c.stroke_circle(x, y, 8, pen);
    c.line(x - 14, y, x - 4, y, pen);
    c.line(x + 4, y, x + 14, y, pen);
    c.line(x, y - 14, x, y - 4, pen);
    c.line(x, y + 4, x, y + 14, pen);
    // Плашка сверху: что проигрывается и как выйти.
    const core::RunRecord& run = replay.replay().run;
    const core::LocalTime& w = run.when;
    const bool finished = replay.done() && game.over();
    constexpr int kX1 = 200;
    constexpr int kX2 = kW - 140;
    c.fill_round_rect(kX1, 6, kX2, 56, 8, 8, theme.overlay);
    c.text_centered(kX1, 8, kX2, 30,
                    std::format("РЕПЛЕЙ · {} · {:02}.{:02}.{:04} {:02}:{:02}",
                                core::combo_name(run.summary.options), w.day, w.month, w.year,
                                w.hour, w.minute),
                    kText, theme.good);
    const std::string_view exit =
        back == core::Screen::GameOver ? "Esc, Space или M — к итогу" : "Esc, Space или M — в меню";
    c.text_centered(kX1, 31, kX2, 53,
                    finished ? std::format("Реплей окончен · {}", exit) : std::string(exit), kText,
                    theme.text_dim);
}

}  // namespace aim::app
