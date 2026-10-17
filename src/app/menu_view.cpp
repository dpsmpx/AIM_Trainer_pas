#include "app/menu_view.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include "app/draw_util.h"
#include "app/panels_view.h"
#include "core/layout.h"
#include "core/profile_file.h"
#include "core/records.h"

namespace aim::app {

namespace {

using core::Rect;
constexpr int kW = core::kW;
constexpr int kH = core::kH;

// DrawBezierLine (Base.pas): 51 точка кривой, ломаной пером.
void bezier(Canvas& c, IntPoint p1, IntPoint c1, IntPoint c2, IntPoint p2, Pen pen) {
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
    c.polyline(pts, pen);
}

// Анимация набора kind (kPresets) на плашке-превью: центр (ax, ay), timer —
// время в меню, мс. Круги — цвета целей темы.
void render_preview(Canvas& c, const core::Theme& theme, int kind, int ax, int ay,
                    long long timer) {
    const auto timer_mod = [&](long long m) { return static_cast<double>(timer % m); };
    const double t = timer_mod(2000) / 2000;
    const auto cross = [&](int cx, int cy, core::Rgba color) {
        c.line(cx - 6, cy, cx + 6, cy, {.color = color});
        c.line(cx, cy - 6, cx, cy + 6, {.color = color});
    };
    const auto fading = [](double u) {
        return u > 0.85 ? round_int(255 * (1 - (u - 0.85) / 0.15)) : 255;
    };
    // Круг превью: заливка дважды — плотнее, как у целей; обводка и крест.
    const auto target = [&](int cx, int r, core::Rgba fill, core::Rgba line) {
        c.fill_circle(cx, ay, r, fill);
        c.fill_circle(cx, ay, r, fill);
        c.stroke_circle(cx, ay, r, {.color = line, .width = 2});
        cross(cx, ay, line);
    };
    switch (kind) {
        case 0: {  // окно меняет цвет: пауза, затем сигнал на 0,6 с
            const double cycle = timer_mod(2400) / 2400;
            const bool lit = cycle > 0.75;
            c.fill_round_rect(ax - 90, ay - 50, ax + 90, ay + 50, 10, 10,
                              lit ? theme.signal : theme.background);
            if (lit) c.text_centered(ax - 90, ay - 50, ax + 90, ay + 50, "Жми!", kText, theme.text);
            break;
        }
        case 1: {  // появление и сжатие
            const int r = std::max(round_int(48 * (1 - t)), 4);
            const int alpha = fading(t);
            target(ax, r, with_alpha(theme.target_left, alpha), with_alpha(theme.text, alpha));
            break;
        }
        case 2: {  // синий и красный по очереди
            const double phase = timer_mod(4000) / 2000;
            const core::Rgba col =
                static_cast<int>(phase) % 2 == 0 ? theme.target_left : theme.target_right;
            const double t2 = phase - std::floor(phase);
            const int r = std::max(round_int(42 * (1 - t2)), 4);
            const int alpha = fading(t2);
            target(ax, r, with_alpha(col, alpha), with_alpha(theme.text, alpha));
            break;
        }
        case 3: {  // движение со следом
            const int offset = round_int(55 * std::sin(static_cast<double>(timer) / 700));
            constexpr int kR = 20;
            for (int i = 1; i <= 6; ++i) {
                const int trail_alpha = 60 - i * 10;
                const int trail_off =
                    round_int(55 * std::sin(static_cast<double>(timer - 40LL * i) / 700));
                c.fill_circle(ax + trail_off, ay, kR - i * 2,
                              with_alpha(theme.target_left, trail_alpha));
            }
            target(ax + offset, kR, with_alpha(theme.target_left, 220), theme.text);
            break;
        }
        default: {  // мелькание
            const double flick = timer_mod(500) / 500;
            if (flick < 0.25) {
                const int r = round_int(45 * (1 - flick / 0.25));
                const int alpha = round_int(255 * (1 - flick / 0.25));
                target(ax, r, with_alpha(theme.target_left, alpha), with_alpha(theme.text, alpha));
            }
            break;
        }
    }
}

// Название пункта меню: набор или «Свой набор».
std::string_view item_name(int i) {
    return i == core::kCustomItem ? std::string_view("Свой набор")
                                  : core::kPresets.at(static_cast<std::size_t>(i)).name;
}

void render_preset_buttons(Canvas& c, const core::Theme& theme, int preset,
                           core::Nanoseconds since) {
    // Кнопки наборов и «Своего набора»: появление каскадом; выбранная — ярче.
    for (int i = 0; i < core::kMenuItems; ++i) {
        const int entry = round_int(255 * core::preset_button_entry(i, since));
        const Rect b = core::preset_button(i, since);
        const bool selected = preset == i;
        const int base = (selected ? 255 : 120) * entry / 255;
        const core::Rgba button =
            i == core::kCustomItem ? theme.custom : theme.presets.at(static_cast<std::size_t>(i));
        c.fill_round_rect(b, 8, 8, with_alpha(button, base));
        // Выбранная — заливка дважды, вдвое плотнее.
        if (selected) c.fill_round_rect(b, 8, 8, with_alpha(button, base));
        const std::string_view name = item_name(i);
        const int tw = c.text_width(name, kTextBold);
        c.text(b.x1 + (core::kButtonW - tw) / 2, b.y1 + (core::kButtonH - 16) / 2, name, kTextBold,
               with_alpha(theme.text, entry));
    }
}

void render_description(Canvas& c, const core::Theme& theme, const core::MeasureOptions& options,
                        int preset) {
    // Описание набора.
    const Rect& d = core::kDescription;
    c.fill_round_rect(d, 8, 8, theme.row);
    const std::string description =
        preset == core::kCustomItem
            ? std::format("Свои опции: {}. Щелчок — конструктор.", core::describe_short(options))
            : std::string(core::kPresets.at(static_cast<std::size_t>(preset)).description);
    // Перенос — по мере строк шрифта (D-43): у Condensed знаки разной ширины.
    c.text_left(d.x1 + 12, d.y1 + 6, d.x2 - 10, d.y2 - 6, description, kText, theme.text);
}

void render_graph(Canvas& c, const core::Theme& theme, const core::Profile& profile,
                  const core::MeasureOptions& options) {
    // График: главная мера последних замеров выбранного набора (D-38).
    const std::vector<double> recent = core::recent_primary(
        profile.runs, options, static_cast<std::size_t>(profile.settings.average_runs));
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
    c.fill_round_rect(kGraphX, kGraphY, kGraphX + kGraphW, kGraphY + kGraphH, 8, 8, theme.graph);
    for (int i = 0; i <= 5; ++i) {
        const double value = top * i / 5;
        const int y = kGraphY + kGraphH - round_int(value * kGraphH / top);
        c.line(kGraphX, y, kGraphX + kGraphW, y, {.color = theme.grid});
        c.text_centered(kGraphX - 50, y - 11, kGraphX - 5, y + 11, std::format("{:.0f}", value),
                        kText, theme.text_dim);
    }
    if (recent.empty()) {
        c.text_centered(kGraphX, kGraphY, kGraphX + kGraphW, kGraphY + kGraphH,
                        "Пока нет замеров этого набора", kText, theme.text_dim);
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
        if (count >= 2)
            draw_smooth_curve(c, pts, {.color = theme.accent, .width = 2});
        else
            c.fill_circle(pts[0].x, pts[0].y, 4, theme.accent);
    }
}

// Рекорд и среднее за N выбранного набора, кнопки внизу, «Эффекты».
void render_bottom(Canvas& c, const core::Theme& theme, const core::Profile& profile,
                   const core::MeasureOptions& options, int profile_number) {
    // Рекорд и среднее за N выбранного набора (D-38).
    const int n = profile.settings.average_runs;
    const core::ComboStats st = core::combo_stats(profile.runs, options, n);
    const bool by_reaction = core::lower_is_better(options);
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
                        stats.at(static_cast<std::size_t>(i)), kText, theme.label, theme.text);

    // Кнопки внизу (DrawMiniButton).
    const std::array<std::string, 6> labels{"Справка", "Настройки",
                                            "Рекорды", "Награды",
                                            "История", std::format("Профиль {}", profile_number)};
    for (std::size_t i = 0; i < core::kMiniButtons.size(); ++i) {
        const Rect b = core::mini_button(core::kMiniButtons.at(i));
        c.stroke_round_rect(b, 10, 10, {.color = faded(theme.text, 0.8)});
        c.text_centered(b, labels.at(i), kText, theme.text_dim);
    }
    c.text_centered(kW - 200, kH - 30, kW - 10, kH - 8,
                    profile.settings.menu_effects ? "Эффекты: ON" : "Эффекты: OFF", kText,
                    theme.text_dim);
}

}  // namespace

void draw_smooth_curve(Canvas& c, std::span<const IntPoint> pts, Pen pen) {
    const std::size_t n = pts.size();
    if (n < 2) return;
    if (n == 2) {
        c.line(pts[0].x, pts[0].y, pts[1].x, pts[1].y, pen);
        return;
    }
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
               pen);
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
        if (const auto alpha = core::link_alpha(dist, core::kCursorLink))
            c.line(round_int(p.x), round_int(p.y), round_int(mouse.x), round_int(mouse.y),
                   {.color = with_alpha(theme.particles, *alpha)});
    }
    for (std::size_t i = 0; i + 1 < items.size(); ++i) {
        for (std::size_t j = i + 1; j < items.size(); ++j) {
            const double dist = std::hypot(items[i].x - items[j].x, items[i].y - items[j].y);
            if (const auto alpha = core::link_alpha(dist, core::kParticleLink))
                c.line(round_int(items[i].x), round_int(items[i].y), round_int(items[j].x),
                       round_int(items[j].y), {.color = with_alpha(theme.particles, *alpha)});
        }
    }
    for (const core::Particle& p : items)
        c.fill_circle(round_int(p.x), round_int(p.y), 3, faded(theme.particles, 60.0 / 255));
}

void MenuView::render_plate(Canvas& c, const core::Theme& theme, std::string_view title, int kind,
                            core::Vec2 mouse) const {
    const Rect& plate = core::kPlate;
    const int x = plate.x1;
    const int y = plate.y1;
    const int w = plate.x2 - plate.x1;
    const int h = plate.y2 - plate.y1;
    const auto timer = static_cast<long long>(anim_ms_);

    // Наведённая плашка пульсирует с периодом 1,5 с.
    int bg_alpha = 45;
    if (plate.contains(mouse)) {
        const double pulse =
            0.5 + 0.5 * std::sin(static_cast<double>(timer % 1500) / 1500.0 * 2 * std::numbers::pi);
        bg_alpha = round_int(45 + 30 * pulse);
    }
    c.fill_round_rect(plate, 12, 12, faded(theme.plate, bg_alpha / 45.0));
    c.text(x + (w - c.text_width(title, kTitleBold)) / 2, y + 14, title, kTitleBold, theme.text);

    render_preview(c, theme, kind, x + w / 2, y + h / 2 + 12, static_cast<long long>(anim_ms_));

    constexpr std::string_view kHint = "Кликни для старта";
    c.text(x + (w - c.text_width(kHint, kText)) / 2, y + h - 24, kHint, kText, theme.text);
}

void MenuView::render(Canvas& c, Toasts& toasts, const core::Theme& theme, const core::Menu& menu,
                      const core::Profile& profile, const PlayerFiles& files, core::Vec2 mouse,
                      core::Nanoseconds now) const {
    c.clear(theme.background);
    render_particles(c, theme, profile, mouse);
    const int preset = menu.selected_preset();
    const bool custom = preset == core::kCustomItem;
    // Опции выбранного пункта: набор с общими настройками или «Свой набор».
    const core::MeasureOptions options = core::item_options(profile, preset);
    render_plate(c, theme, item_name(preset), custom ? core::preset_like(options) : preset, mouse);
    render_preset_buttons(c, theme, preset, now - std::min(now, menu.entered_at()));
    render_description(c, theme, options, preset);
    render_graph(c, theme, profile, options);
    render_bottom(c, theme, profile, options, menu.profile_number());

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
