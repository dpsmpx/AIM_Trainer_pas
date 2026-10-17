#include "app/menu_view.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numbers>
#include <string>
#include <string_view>

#include "core/help.h"
#include "core/layout.h"
#include "core/profile_file.h"
#include "core/records.h"
#include "core/rounding.h"
#include "core/theme.h"

namespace aim::app {

namespace {

using core::Rect;
constexpr int kW = core::kW;
constexpr int kH = core::kH;
constexpr core::Rgba kNone{0, 0, 0, 0};  ///< прозрачно: без пера, без плашки под текстом

int round_int(double v) {
    return core::round_half_even(v);
}

std::string one_decimal(double v) {
    return std::format("{:.1f}", v);
}

// Главная мера замера (D-38): у замера на цвет — медиана реакции, у
// остальных — очки.
std::string primary_text(const core::MeasureOptions& options, double value) {
    return core::lower_is_better(options) ? std::format("{:.0f} мс", value)
                                          : std::format("{:.0f}", value);
}

void fill_round_rect(Canvas& c, const Rect& r, int w, int h) {
    c.fill_round_rect(r.x1, r.y1, r.x2, r.y2, w, h);
}

void text_centered(Canvas& c, const Rect& r, std::string_view s) {
    c.draw_text_centered(r.x1, r.y1, r.x2, r.y2, s);
}

// DrawBezierLine (Base.pas): 51 точка кривой, ломаной пером.
void bezier(Canvas& c, IntPoint p1, IntPoint c1, IntPoint c2, IntPoint p2, core::Rgba color,
            int width) {
    constexpr int kPoints = 51;  // шаг 0,02
    std::array<IntPoint, kPoints> pts{};
    double t = 0.0;
    for (IntPoint& p : pts) {
        const double nt = 1 - t;
        const auto at = [&](int a, int b, int cc, int d) {
            return round_int(nt * nt * nt * a + 3 * nt * nt * t * b + 3 * nt * t * t * cc +
                             t * t * t * d);
        };
        p = {at(p1.x, c1.x, c2.x, p2.x), at(p1.y, c1.y, c2.y, p2.y)};
        t += 0.02;
    }
    c.pen = color;
    c.pen_width = width;
    c.polyline(pts);
}

// Затемнение под панелью и сама панель (общее у Render*Overlay).
void panel_back(Canvas& c, const core::Theme& th, const Rect& panel) {
    c.brush = th.dim;
    c.fill_round_rect(0, 0, kW, kH, 1, 1);
    c.brush = th.panel;
    c.pen = th.panel_border;
    c.pen_width = 1;
    fill_round_rect(c, panel, 14, 14);
}

void panel_title(Canvas& c, const core::Theme& th, const Rect& panel, int top,
                 std::string_view title) {
    c.font.size = kTitleSize;
    c.font_color = th.text;
    c.draw_text_centered(panel.x1, panel.y1 + top, panel.x2, panel.y1 + top + 34, title);
}

void panel_footer(Canvas& c, const core::Theme& th, const Rect& panel, std::string_view text) {
    c.font.size = kTextSize;
    c.font_color = th.text_dim;
    c.draw_text_centered(panel.x1, panel.y2 - 28, panel.x2, panel.y2 - 6, text);
}

// Справка: видимые строки с прокрутки и полоса прокрутки справа.
void render_help(Canvas& c, const core::Theme& th, int scroll) {
    const Rect p = core::help_panel();
    panel_back(c, th, p);
    panel_title(c, th, p, 14, "Справка");
    const int top = p.y1 + core::kHelpTop;
    for (int i = 0; i < core::kHelpVisibleLines; ++i) {
        const int at = scroll + i;
        if (at >= static_cast<int>(core::kHelp.size())) break;
        const core::HelpLine& line = core::kHelp.at(static_cast<std::size_t>(at));
        const int y = top + i * core::kHelpLineH;
        c.font.size = kTextSize;
        c.font.weight = line.heading ? Weight::Bold : Weight::Normal;
        c.font_color = line.heading ? th.accent : th.text;
        c.draw_text_centered(p.x1 + 20, y, p.x2 - 30, y + core::kHelpLineH, line.text);
    }
    c.font.weight = Weight::Normal;
    // Полоса прокрутки: дорожка и бегунок — доля и место видимых строк.
    const Rect track = core::help_track();
    c.pen = kNone;
    c.brush = th.row;
    fill_round_rect(c, track, 6, 6);
    const int track_h = track.y2 - track.y1;
    const auto total = static_cast<double>(core::kHelp.size());
    const int thumb_h = std::max(20, round_int(track_h * core::kHelpVisibleLines / total));
    const int thumb_y =
        track.y1 + (core::kHelpMaxScroll > 0 ? round_int(static_cast<double>(track_h - thumb_h) *
                                                         scroll / core::kHelpMaxScroll)
                                             : 0);
    c.brush = th.accent;
    c.fill_round_rect(track.x1, thumb_y, track.x2, thumb_y + thumb_h, 6, 6);
    c.font.size = kTextSize;
    c.font_color = th.text_dim;
    c.draw_text_centered(p.x1, p.y2 - 26, p.x2, p.y2 - 6,
                         "↑ ↓ и колёсико — листать · Esc или клик вне панели — закрыть");
}

// Путь, обрезанный слева до ширины width: «…\Roaming\AimTrainer\».
std::string fit_path(Canvas& c, std::string_view prefix, std::string_view path, int width) {
    std::string tail(path);
    const auto text = [&](bool cut) { return std::string(prefix) + (cut ? "…" : "") + tail; };
    bool cut = false;
    while (!tail.empty() && c.text_width(text(cut)) > width) {
        // Отбросить первый знак UTF-8.
        std::size_t next = 1;
        while (next < tail.size() && (static_cast<unsigned char>(tail[next]) & 0xC0U) == 0x80U)
            ++next;
        tail.erase(0, next);
        cut = true;
    }
    return text(cut);
}

void render_settings(Canvas& c, const core::Theme& th, const core::Settings& s,
                     std::string_view folder) {
    const Rect p = core::settings_panel();
    panel_back(c, th, p);
    panel_title(c, th, p, 12, "Настройки");

    // Один размер шрифта во всех рядах (D-43).
    c.font.size = kTextSize;
    const auto stepper = [&](int row, const std::string& text) {
        const int y = core::settings_row_y(row);
        c.pair_centered(p.x1 + 20, y, p.x2 - 120, y + core::kSettingsButton, text, th.label,
                        th.text);
        c.pen = kNone;
        for (const Rect& r : {core::settings_minus(row), core::settings_plus(row)}) {
            c.brush = th.button;
            fill_round_rect(c, r, 6, 6);
        }
        c.font_color = th.button_text;
        text_centered(c, core::settings_minus(row), "-");
        text_centered(c, core::settings_plus(row), "+");
    };
    const auto toggle = [&](int row, const std::string& text) {
        const Rect r = core::settings_toggle(row);
        c.brush = th.row;
        c.pen = kNone;
        fill_round_rect(c, r, 8, 8);
        c.pair_centered(r.x1, r.y1, r.x2, r.y2, text, th.label, th.text);
    };
    const core::PresetSettings& ps = s.preset;
    stepper(core::kSettingsSizeRow, std::format("Размер цели: {} px", ps.target_size));
    stepper(core::kSettingsLifetimeRow, std::format("Жизнь цели: {} мс", ps.lifetime_ms));
    stepper(core::kSettingsLivesRow, std::format("Жизни: {}", ps.lives));
    toggle(core::kSettingsEndingRow,
           ps.ending == core::Ending::Lives
               ? "Окончание: по жизням"
               : std::format("Окончание: {} с", core::MeasureOptions{}.duration_s));
    stepper(core::kSettingsAverageRow, std::format("Среднее за N замеров: {}", s.average_runs));
    toggle(core::kSettingsSoundRow, std::string("Звук: ") + (s.sound ? "ON" : "OFF"));
    toggle(core::kSettingsThemeRow, "Тема: " + core::theme_title(s.theme));
    toggle(core::kSettingsFontRow,
           std::string("Шрифт: ") + (s.font == 1 ? "DejaVu Sans Mono" : "DejaVu Sans Condensed"));
    toggle(core::kSettingsMenuEffectsRow,
           std::string("Эффекты меню: ") + (s.menu_effects ? "ON" : "OFF"));
    toggle(core::kSettingsHudRow, std::string("Интерфейс замера: ") + (s.hud ? "ON" : "OFF"));
    toggle(core::kSettingsReplaysRow, std::string("Запись реплеев: ") + (s.replays ? "ON" : "OFF"));
    toggle(core::kSettingsOpenFolderRow, "Открыть папку сохранений");
    const Rect folder_row = core::settings_toggle(core::kSettingsChooseFolderRow);
    toggle(core::kSettingsChooseFolderRow,
           fit_path(c, "Папка: ", folder, folder_row.x2 - folder_row.x1 - 16));

    const Rect reset = core::settings_reset();
    c.brush = th.control;
    fill_round_rect(c, reset, 8, 8);
    c.font_color = th.text;
    text_centered(c, reset, "Сбросить");
    c.font.size = kTextSize;
    c.font_color = th.text_dim;
    c.draw_text_centered(p.x1 + 10, p.y2 - 44, p.x2 - 10, p.y2 - 6,
                         "Размер, жизнь цели, жизни и окончание — для всех наборов. Клик вне "
                         "панели — закрыть.");
}

// «Свой набор» (D-37): заготовки-наборы, строки опций, «Начать». Опция, которая
// к замеру не относится, — бледная и с прочерком.
void render_constructor(Canvas& c, const core::Theme& th, const core::Profile& profile) {
    const Rect p = core::constructor_panel();
    panel_back(c, th, p);
    panel_title(c, th, p, 10, "Свой набор");
    const core::MeasureOptions& o = profile.custom;
    // Заготовки: набор того же вида — ярче.
    const std::optional<int> same = core::preset_index(o);
    c.font.size = kTextSize;
    for (int i = 0; i < core::kPresetCount; ++i) {
        const Rect r = core::constructor_preset(i);
        c.brush = faded(th.presets.at(static_cast<std::size_t>(i)), same == i ? 1.0 : 0.45);
        c.pen = kNone;
        fill_round_rect(c, r, 6, 6);
        c.font_color = th.text;
        text_centered(c, r, core::kPresets.at(static_cast<std::size_t>(i)).name);
    }
    c.font.size = kTextSize;
    for (int r = 0; r < core::kOptionRows; ++r) {
        const auto row = static_cast<core::OptionRow>(r);
        const Rect box = core::constructor_row(r);
        const bool on = core::option_applies(o, row);
        // Опция не к замеру — бледнее.
        const core::Rgba off = faded(th.text_dim, 0.55);
        c.brush = on ? th.row : faded(th.row, 0.4);
        c.pen = kNone;
        fill_round_rect(c, box, 8, 8);
        const int ty = box.y1 + (box.y2 - box.y1 - c.line_height()) / 2;
        c.brush = kNone;
        c.font_color = on ? th.text : off;
        c.text_out(box.x1 + 12, ty, core::option_label(o, row));
        const std::string value = on ? core::option_value(o, row) : "—";
        if (core::option_toggles(row)) {
            c.font_color = on ? th.accent : off;
            c.text_out(box.x2 - 12 - c.text_width(value), ty, value);
            continue;
        }
        const Rect minus = core::constructor_minus(r);
        const Rect plus = core::constructor_plus(r);
        if (on) {
            for (const Rect& b : {minus, plus}) {
                c.brush = th.button;
                fill_round_rect(c, b, 6, 6);
            }
            c.font_color = th.button_text;
            text_centered(c, minus, "-");
            text_centered(c, plus, "+");
        }
        c.font_color = on ? th.text : off;
        c.draw_text_centered(minus.x2, box.y1, plus.x1, box.y2, value);
    }
    const Rect start = core::constructor_start();
    c.brush = th.good;
    c.pen = kNone;
    fill_round_rect(c, start, 10, 10);
    c.font_color = th.text;
    c.font.weight = Weight::Bold;
    text_centered(c, start, "Начать замер");
    c.font.weight = Weight::Normal;
    panel_footer(c, th, p, "Заготовка заполняет опции · Esc или клик вне панели — закрыть");
}

// Рекорды наборов (D-38): лучший замер и среднее за N, свежие наборы первыми;
// у строки с реплеем — «▶»: щелчок проигрывает его (D-39).
void render_records(Canvas& c, const core::Theme& th, const core::Profile& profile,
                    const PlayerFiles& files) {
    const Rect p = core::records_panel();
    panel_back(c, th, p);
    panel_title(c, th, p, 12, "Рекорды наборов");
    const int n = profile.settings.average_runs;
    const std::vector<core::ComboStats> all = core::all_combo_stats(profile.runs, n);
    if (all.empty()) {
        c.font.size = kTextSize;
        c.font_color = th.text_dim;
        c.draw_text_centered(p.x1, p.y1 + 120, p.x2, p.y1 + 150, "Пока нет замеров");
    }
    bool any_replay = false;
    for (int i = 0; i < std::min(core::kRecordsRows, static_cast<int>(all.size())); ++i) {
        const core::ComboStats& st = all.at(static_cast<std::size_t>(i));
        const Rect row = core::records_row(i);
        const int y = row.y1;
        const bool replay = files.has_replay(st.options);
        any_replay = any_replay || replay;
        c.brush = th.row;
        c.pen = kNone;
        c.fill_round_rect(row.x1, row.y1, row.x2, row.y2, 8, 8);
        if (replay) {
            // Треугольник «▶» слева: у набора есть реплей.
            const int cx = row.x1 + 14;
            const int cy = (row.y1 + row.y2) / 2;
            c.brush = th.good;
            const std::array<IntPoint, 3> play{{{cx - 4, cy - 7}, {cx + 7, cy}, {cx - 4, cy + 7}}};
            c.fill_polygon(play);
        }
        c.font.size = kTextSize;
        c.font_color = th.text;
        c.draw_text_centered(
            p.x1 + 28, y + 2, p.x2 - 20, y + 21,
            std::format("{}: {}", core::combo_name(st.options), core::describe_short(st.options)));
        const std::string best =
            st.best
                ? primary_text(st.options, core::primary_value(profile.runs.at(*st.best).summary))
                : "—";
        c.font_color = th.gold;
        c.draw_text_centered(
            p.x1 + 28, y + 21, p.x2 - 20, y + 40,
            std::format("рекорд {} · среднее за {}: {} · замеров: {}", best, st.average.runs,
                        primary_text(st.options, st.average.primary), st.runs));
    }
    panel_footer(c, th, p,
                 any_replay ? "Клик по строке с ▶ — реплей рекорда · мимо строк — закрыть"
                            : "Клик — закрыть");
}

void render_achievements(Canvas& c, const core::Theme& th, const core::Profile& profile) {
    const Rect p{(kW - 560) / 2, (kH - 470) / 2, (kW - 560) / 2 + 560, (kH - 470) / 2 + 470};
    panel_back(c, th, p);
    panel_title(c, th, p, 14, "Достижения");
    for (int i = 0; i < core::kAchievementCount; ++i) {
        const int y = p.y1 + 52 + i * 46;
        const auto at = static_cast<std::size_t>(i);
        const core::Rgba col = profile.achievements.at(at) ? th.gold : faded(th.text_dim, 0.6);
        c.brush = th.row;
        c.fill_round_rect(p.x1 + 16, y, p.x2 - 16, y + 42, 8, 8);
        c.font_color = col;
        c.font.size = kTextSize;
        c.draw_text_centered(p.x1 + 26, y + 2, p.x2 - 26, y + 21, core::kAchievements.at(at).name);
        c.font_color = th.text_dim;
        c.draw_text_centered(p.x1 + 26, y + 21, p.x2 - 26, y + 40,
                             core::kAchievements.at(at).description);
    }
    panel_footer(c, th, p, "Клик — закрыть");
}

// Последние замеры с параметрами (D-38) и экспорт CSV (D-33).
void render_history(Canvas& c, const core::Theme& th, const core::Profile& profile,
                    std::string_view file_name) {
    const Rect p = core::history_panel();
    panel_back(c, th, p);
    panel_title(c, th, p, 12, "История");
    c.font.size = kTextSize;
    c.font_color = th.text_dim;
    c.draw_text_centered(p.x1, p.y1 + 42, p.x2, p.y1 + 64,
                         std::format("Замеров в истории: {}", profile.runs.size()));
    constexpr int kRows = 14;
    const auto& runs = profile.runs;
    const std::size_t from = runs.size() > kRows ? runs.size() - kRows : 0;
    int y = p.y1 + 70;
    for (std::size_t i = runs.size(); i > from; --i) {
        const core::RunRecord& r = runs.at(i - 1);
        const core::GameSummary& g = r.summary;
        std::string text = std::format("{:02}.{:02} {:02}:{:02}  {}  ", r.when.day, r.when.month,
                                       r.when.hour, r.when.minute, core::combo_name(g.options));
        text += core::lower_is_better(g.options)
                    ? std::format("{:.0f} мс, фальстартов {}", g.median_reaction_ms, g.misses)
                    : std::format("{} очков, {}%, {:.0f} мс", g.score, one_decimal(g.accuracy),
                                  g.avg_reaction_ms);
        c.font_color = th.text;
        c.draw_text_centered(p.x1 + 16, y, p.x2 - 16, y + 22, text);
        y += 22;
    }
    // «Экспорт в CSV» — в папку сохранений (D-33).
    const Rect export_button = core::history_export();
    c.brush = th.control;
    c.pen = kNone;
    fill_round_rect(c, export_button, 8, 8);
    c.font.size = kTextSize;
    c.font_color = th.text;
    text_centered(c, export_button, "Экспорт в CSV");
    c.font_color = th.text_dim;
    c.draw_text_centered(export_button.x2 + 10, export_button.y1, p.x2 - 10, export_button.y2,
                         file_name);
    panel_footer(c, th, p, "Клик вне панели — закрыть");
}

void render_profiles(Canvas& c, const core::Theme& th, int current) {
    const Rect p = core::profiles_panel();
    panel_back(c, th, p);
    panel_title(c, th, p, 14, "Профили");
    for (int n = 1; n <= core::kProfileCount; ++n) {
        if (n == current) {
            c.brush = faded(th.accent, 0.8);
            c.pen = th.text;
            c.pen_width = 2;
        } else {
            c.brush = th.row;
            c.pen = kNone;
            c.pen_width = 1;
        }
        const Rect row = core::profile_row(n);
        fill_round_rect(c, row, 8, 8);
        c.font.size = kTextSize;
        c.font_color = th.text;
        text_centered(c, row, std::format("Профиль {}", n));
    }
    panel_footer(c, th, p, "Клик по профилю — переключить");
}

// Тексты туториала.
constexpr std::array<std::string_view, core::kTutorialSteps> kTutorial{
    "Добро пожаловать в DP AIM Trainer! Выбери набор замера — замер начнётся через 3 секунды. "
    "«Свой набор» — любые опции.",
    "«Реакция на цвет»: щёлкни, как только окно сменит цвет. Раньше — фальстарт.",
    "С целями: чем ближе к центру — тем выше точность. Бомбы (белый крест) лучше не трогать!",
    "Esc — закончить замер, R — заново, M — в меню. Рекорд — у каждого набора свой. Удачи!",
};

void render_tutorial(Canvas& c, const core::Theme& th, int step) {
    c.brush = th.dim;
    c.fill_round_rect(0, 0, kW, kH, 1, 1);
    constexpr int kPanelW = 480;
    constexpr int kPanelH = 220;
    const Rect p{(kW - kPanelW) / 2, (kH - kPanelH) / 2, (kW + kPanelW) / 2, (kH + kPanelH) / 2};
    c.brush = th.panel;
    c.pen = th.accent;
    c.pen_width = 2;
    fill_round_rect(c, p, 14, 14);
    c.font.size = kTextSize;
    c.font_color = th.text;
    c.draw_text_centered(p.x1 + 20, p.y1 + 30, p.x2 - 20, p.y1 + 130,
                         kTutorial.at(static_cast<std::size_t>(std::clamp(step, 0, 3))));
    c.font.size = kTextSize;
    c.font_color = th.text_dim;
    c.draw_text_centered(p.x1, p.y2 - 54, p.x2, p.y2 - 32, std::format("{} / 4", step + 1));
    c.font_color = th.text_dim;
    c.draw_text_centered(p.x1, p.y2 - 30, p.x2, p.y2 - 8,
                         step < 3 ? "Клик — далее" : "Клик — начать!");
}

}  // namespace

void draw_smooth_curve(Canvas& c, std::span<const IntPoint> pts, core::Rgba color, int width) {
    const std::size_t n = pts.size();
    if (n < 2) return;
    if (n == 2) {
        c.pen = color;
        c.pen_width = width;
        c.line(pts[0].x, pts[0].y, pts[1].x, pts[1].y);
        return;
    }
    c.pen = color;
    c.pen_width = width;
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const IntPoint p0 = pts[i];
        const IntPoint p1 = pts[i + 1];
        // Касательные; деление целых — с отбрасыванием, как div образца.
        const IntPoint t0 =
            i == 0 ? IntPoint{p1.x - p0.x, p1.y - p0.y}
                   : IntPoint{(pts[i + 1].x - pts[i - 1].x) / 2, (pts[i + 1].y - pts[i - 1].y) / 2};
        const IntPoint t1 =
            i == n - 2 ? IntPoint{p1.x - p0.x, p1.y - p0.y}
                       : IntPoint{(pts[i + 2].x - pts[i].x) / 2, (pts[i + 2].y - pts[i].y) / 2};
        bezier(c, p0, {p0.x + t0.x / 3, p0.y + t0.y / 3}, {p1.x - t1.x / 3, p1.y - t1.y / 3}, p1,
               color, width);
    }
}

void MenuView::enter(core::Random& rng, core::Nanoseconds now) {
    particles_.init(rng);
    last_ = now;
}

void MenuView::update(core::Nanoseconds now) {
    // Как MenuAnimTimer образца: часы назад или долгая пауза между кадрами —
    // один кадр.
    core::Nanoseconds dt = now >= last_ ? now - last_ : core::ms(16);
    last_ = now;
    if (dt > core::ms(200)) dt = core::ms(16);
    anim_ms_ += core::to_ms(dt);
    particles_.update(dt);
}

void MenuView::render_particles(Canvas& c, const core::Theme& theme, const core::Profile& profile,
                                core::Vec2 mouse) const {
    // Эффекты меню выключены — ни частиц, ни связей, ни линий к курсору (D-47).
    if (!profile.settings.menu_effects) return;
    const auto& items = particles_.items();
    for (const core::Particle& p : items) {
        const double dist = std::hypot(p.x - mouse.x, p.y - mouse.y);
        if (const auto alpha = core::link_alpha(dist, core::kCursorLink)) {
            c.pen = with_alpha(theme.particles, *alpha);
            c.pen_width = 1;
            c.line(round_int(p.x), round_int(p.y), round_int(mouse.x), round_int(mouse.y));
        }
    }
    for (std::size_t i = 0; i + 1 < items.size(); ++i) {
        for (std::size_t j = i + 1; j < items.size(); ++j) {
            const double dist = std::hypot(items[i].x - items[j].x, items[i].y - items[j].y);
            if (const auto alpha = core::link_alpha(dist, core::kParticleLink)) {
                c.pen = with_alpha(theme.particles, *alpha);
                c.pen_width = 1;
                c.line(round_int(items[i].x), round_int(items[i].y), round_int(items[j].x),
                       round_int(items[j].y));
            }
        }
    }
    for (const core::Particle& p : items) {
        c.brush = faded(theme.particles, 60.0 / 255);
        c.fill_circle(round_int(p.x), round_int(p.y), 3);
    }
}

void MenuView::render_plate(Canvas& c, const core::Theme& theme, std::string_view title, int kind,
                            core::Vec2 mouse) const {
    const Rect& plate = core::kPlate;
    const int x = plate.x1;
    const int y = plate.y1;
    const int w = plate.x2 - plate.x1;
    const int h = plate.y2 - plate.y1;
    const auto timer = static_cast<long long>(anim_ms_);
    const auto timer_mod = [&](long long m) { return static_cast<double>(timer % m); };

    int bg_alpha = 45;
    int border_alpha = 70;
    if (plate.contains(mouse)) {
        // Наведённая плашка пульсирует с периодом 1,5 с.
        const double pulse = 0.5 + 0.5 * std::sin(timer_mod(1500) / 1500.0 * 2 * std::numbers::pi);
        bg_alpha = round_int(45 + 30 * pulse);
        border_alpha = round_int(70 + 150 * pulse);
    }
    // DrawPlateBg: FillRoundRect заливает, но не обводит — перо не видно.
    c.brush = faded(theme.plate, bg_alpha / 45.0);
    c.pen = with_alpha(theme.text, border_alpha);
    c.fill_round_rect(x, y, x + w, y + h, 12, 12);

    c.font.size = kTitleSize;
    c.font_color = theme.text;
    c.brush = kNone;
    c.font.weight = Weight::Bold;

    // Круги превью — цвета целей темы.
    const int ax = x + w / 2;
    const int ay = y + h / 2 + 12;
    const double t = timer_mod(2000) / 2000;
    c.text_out(x + (w - c.text_width(title)) / 2, y + 14, title);
    c.font.weight = Weight::Normal;

    const auto cross = [&](int cx, int cy, core::Rgba color) {
        c.pen = color;
        c.pen_width = 1;
        c.line(cx - 6, cy, cx + 6, cy);
        c.line(cx, cy - 6, cx, cy + 6);
    };
    const auto fading = [](double u) {
        return u > 0.85 ? round_int(255 * (1 - (u - 0.85) / 0.15)) : 255;
    };
    switch (kind) {
        case 0: {  // окно меняет цвет: пауза, затем сигнал на 0,6 с
            const double cycle = timer_mod(2400) / 2400;
            const bool lit = cycle > 0.75;
            c.brush = lit ? theme.signal : theme.background;
            c.pen = faded(theme.text, 0.47);
            c.pen_width = 1;
            c.fill_round_rect(ax - 90, ay - 50, ax + 90, ay + 50, 10, 10);
            if (lit) {
                c.font.size = kTextSize;
                c.font_color = theme.text;
                c.draw_text_centered(ax - 90, ay - 50, ax + 90, ay + 50, "Жми!");
            }
            break;
        }
        case 1: {  // появление и сжатие
            const int r = std::max(round_int(48 * (1 - t)), 4);
            const int alpha = fading(t);
            c.brush = with_alpha(theme.target_left, alpha);
            c.fill_circle(ax, ay, r);
            c.pen = with_alpha(theme.text, alpha);
            c.pen_width = 2;
            c.circle(ax, ay, r);
            cross(ax, ay, with_alpha(theme.text, alpha));
            break;
        }
        case 2: {  // синий и красный по очереди
            const double phase = timer_mod(4000) / 2000;
            const core::Rgba col =
                static_cast<int>(phase) % 2 == 0 ? theme.target_left : theme.target_right;
            const double t2 = phase - std::floor(phase);
            const int r = std::max(round_int(42 * (1 - t2)), 4);
            const int alpha = fading(t2);
            c.brush = with_alpha(col, alpha);
            c.fill_circle(ax, ay, r);
            c.pen = with_alpha(theme.text, alpha);
            c.pen_width = 2;
            c.circle(ax, ay, r);
            cross(ax, ay, with_alpha(theme.text, alpha));
            break;
        }
        case 3: {  // движение со следом
            const int offset = round_int(55 * std::sin(static_cast<double>(timer) / 700));
            constexpr int kR = 20;
            for (int i = 1; i <= 6; ++i) {
                const int trail_alpha = 60 - i * 10;
                const int trail_off =
                    round_int(55 * std::sin(static_cast<double>(timer - 40LL * i) / 700));
                c.brush = with_alpha(theme.target_left, trail_alpha);
                c.fill_circle(ax + trail_off, ay, kR - i * 2);
            }
            c.brush = with_alpha(theme.target_left, 220);
            c.fill_circle(ax + offset, ay, kR);
            c.pen = theme.text;
            c.pen_width = 2;
            c.circle(ax + offset, ay, kR);
            cross(ax + offset, ay, theme.text);
            break;
        }
        default: {  // мелькание
            const double flick = timer_mod(500) / 500;
            if (flick < 0.25) {
                const int r = round_int(45 * (1 - flick / 0.25));
                const int alpha = round_int(255 * (1 - flick / 0.25));
                c.brush = with_alpha(theme.target_left, alpha);
                c.fill_circle(ax, ay, r);
                c.pen = with_alpha(theme.text, alpha);
                c.pen_width = 2;
                c.circle(ax, ay, r);
                cross(ax, ay, with_alpha(theme.text, alpha));
            }
            break;
        }
    }

    c.font.size = kTextSize;
    c.font_color = theme.text;
    c.brush = kNone;
    constexpr std::string_view kHint = "Кликни для старта";
    c.text_out(x + (w - c.text_width(kHint)) / 2, y + h - 24, kHint);
}

void MenuView::render(Canvas& c, Toasts& toasts, const core::Theme& theme, const core::Menu& menu,
                      const core::Profile& profile, const PlayerFiles& files, core::Vec2 mouse,
                      core::Nanoseconds now) const {
    c.clear_window(theme.background);
    render_particles(c, theme, profile, mouse);
    const int preset = menu.selected_preset();
    const bool custom = preset == core::kCustomItem;
    // Опции выбранного пункта: набор с общими настройками или «Свой набор».
    const core::MeasureOptions options = core::item_options(profile, preset);
    const auto item_name = [](int i) {
        return i == core::kCustomItem ? std::string_view("Свой набор")
                                      : core::kPresets.at(static_cast<std::size_t>(i)).name;
    };
    render_plate(c, theme, item_name(preset), custom ? core::preset_like(options) : preset, mouse);

    // Кнопки наборов и «Своего набора»: появление каскадом; выбранная — ярче.
    const core::Nanoseconds since = now > menu.entered_at() ? now - menu.entered_at() : 0;
    for (int i = 0; i < core::kMenuItems; ++i) {
        const int entry = round_int(255 * core::preset_button_entry(i, since));
        const Rect b = core::preset_button(i, since);
        const bool selected = preset == i;
        const int base = (selected ? 255 : 120) * entry / 255;
        const core::Rgba button =
            i == core::kCustomItem ? theme.custom : theme.presets.at(static_cast<std::size_t>(i));
        c.brush = with_alpha(button, base);
        c.pen = with_alpha(theme.text, 80 * entry / 255);
        c.pen_width = 1;
        fill_round_rect(c, b, 8, 8);
        if (selected) {
            // Второй FillRoundRect образца: обводки нет, заливка вдвое плотнее.
            c.pen = with_alpha(theme.text, entry);
            c.pen_width = 2;
            fill_round_rect(c, b, 8, 8);
            c.pen_width = 1;
        }
        c.font.size = kTextSize;
        c.font_color = with_alpha(theme.text, entry);
        c.brush = kNone;
        c.font.weight = Weight::Bold;
        const std::string_view name = item_name(i);
        const int tw = c.text_width(name);
        c.text_out(b.x1 + (core::kButtonW - tw) / 2, b.y1 + (core::kButtonH - 16) / 2, name);
        c.font.weight = Weight::Normal;
    }

    // Описание набора.
    const Rect& d = core::kDescription;
    c.brush = theme.row;
    c.pen = faded(theme.text, 55.0 / 255);
    fill_round_rect(c, d, 8, 8);
    c.font.size = kTextSize;
    c.font_color = theme.text;
    const std::string description =
        custom ? std::format("Свои опции: {}. Щелчок — конструктор.", core::describe_short(options))
               : std::string(core::kPresets.at(static_cast<std::size_t>(preset)).description);
    // Перенос — по мере строк шрифта (D-43): у Condensed знаки разной ширины.
    c.draw_text_left(d.x1 + 12, d.y1 + 6, d.x2 - 10, d.y2 - 6, description);

    // График: главная мера последних замеров выбранного набора (D-38).
    const int n = profile.settings.average_runs;
    const std::vector<double> recent =
        core::recent_primary(profile.runs, options, static_cast<std::size_t>(n));
    constexpr int kGraphX = 50;
    constexpr int kGraphY = core::kDescription.y2 + 8;  // под описанием набора
    constexpr int kGraphW = kW - 100;
    constexpr int kGraphH = kH - 110 - kGraphY;
    const bool by_reaction = core::lower_is_better(options);
    double top = 1000.0;
    if (!by_reaction) {
        // Очки: шкала до ближайшего «круглого» числа над лучшим замером.
        const double most = recent.empty() ? 0.0 : *std::max_element(recent.begin(), recent.end());
        for (const double step : {100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 50000.0}) {
            top = step;
            if (most <= step) break;
        }
    }
    c.brush = theme.graph;
    c.fill_round_rect(kGraphX, kGraphY, kGraphX + kGraphW, kGraphY + kGraphH, 8, 8);
    c.font.size = kTextSize;
    c.font_color = theme.text_dim;
    for (int i = 0; i <= 5; ++i) {
        const double value = top * i / 5;
        const int y = kGraphY + kGraphH - round_int(value * kGraphH / top);
        c.pen = theme.grid;
        c.pen_width = 1;
        c.line(kGraphX, y, kGraphX + kGraphW, y);
        c.draw_text_centered(kGraphX - 50, y - 11, kGraphX - 5, y + 11,
                             std::format("{:.0f}", value));
    }
    if (recent.empty()) {
        c.font.size = kTextSize;
        c.font_color = theme.text_dim;
        c.draw_text_centered(kGraphX, kGraphY, kGraphX + kGraphW, kGraphY + kGraphH,
                             "Пока нет замеров этого набора");
    } else {
        const auto count = static_cast<int>(recent.size());
        std::vector<IntPoint> pts;
        for (int i = 0; i < count; ++i) {
            const double v = std::min(recent[static_cast<std::size_t>(i)], top);
            pts.push_back(
                {kGraphX + (count > 1 ? round_int(static_cast<double>(i) * kGraphW / (count - 1))
                                      : kGraphW / 2),
                 kGraphY + kGraphH - round_int(v * kGraphH / top)});
        }
        if (count >= 2) {
            draw_smooth_curve(c, pts, theme.accent, 2);
        } else {
            c.brush = theme.accent;
            c.fill_circle(pts[0].x, pts[0].y, 4);
        }
    }

    // Рекорд и среднее за N выбранного набора (D-38).
    const core::ComboStats st = core::combo_stats(profile.runs, options, n);
    c.font.size = kTextSize;
    c.font_color = theme.text_dim;
    constexpr int kBottomY = kH - 50;
    constexpr int kCol = kW / 3;
    const std::string measure = by_reaction ? "медиана" : "очки";
    const std::array<std::string, 3> stats{
        std::format(
            "Рекорд ({}): {}", measure,
            st.best ? primary_text(options, core::primary_value(profile.runs.at(*st.best).summary))
                    : "—"),
        std::format("Среднее за {}: {}", st.average.runs > 0 ? st.average.runs : n,
                    st.average.runs > 0 ? primary_text(options, st.average.primary) : "—"),
        std::format("Замеров набора: {}", st.runs),
    };
    for (int i = 0; i < 3; ++i)
        c.pair_centered(kCol * i, kBottomY - 11, i == 2 ? kW : kCol * (i + 1), kBottomY + 11,
                        stats.at(static_cast<std::size_t>(i)), theme.label, theme.text);

    // Кнопки внизу (DrawMiniButton).
    c.font.size = kTextSize;
    c.font_color = theme.text_dim;
    const std::array<std::string, 6> labels{
        "Справка", "Настройки", "Рекорды",
        "Награды", "История",   std::format("Профиль {}", menu.profile_number())};
    for (std::size_t i = 0; i < core::kMiniButtons.size(); ++i) {
        const Rect b = core::mini_button(core::kMiniButtons.at(i));
        c.pen = faded(theme.text, 0.8);
        c.draw_round_rect(b.x1, b.y1, b.x2, b.y2, 10, 10);
        text_centered(c, b, labels.at(i));
    }
    c.font_color = theme.text_dim;
    c.draw_text_centered(kW - 200, kH - 30, kW - 10, kH - 8,
                         profile.settings.menu_effects ? "Эффекты: ON" : "Эффекты: OFF");

    switch (menu.panel()) {
        case core::Panel::Help: render_help(c, theme, menu.help_scroll()); break;
        case core::Panel::Settings:
            render_settings(c, theme, profile.settings, files.folder());
            break;
        case core::Panel::Records: render_records(c, theme, profile, files); break;
        case core::Panel::Achievements: render_achievements(c, theme, profile); break;
        case core::Panel::History:
            render_history(c, theme, profile, core::runs_file_name(menu.profile_number()));
            break;
        case core::Panel::Profiles: render_profiles(c, theme, menu.profile_number()); break;
        case core::Panel::Constructor: render_constructor(c, theme, profile); break;
        case core::Panel::None: break;
    }
    if (menu.tutorial_shown()) render_tutorial(c, theme, menu.tutorial_step());
    // Уведомления — поверх панелей: ответ на действие в панели виден (AUD-33).
    toasts.render(c, theme, now);
}

}  // namespace aim::app
