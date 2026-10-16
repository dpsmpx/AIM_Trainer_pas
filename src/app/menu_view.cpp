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

#include "core/layout.h"
#include "core/profile_file.h"
#include "core/rounding.h"

namespace aim::app {

namespace {

using core::Rect;
constexpr int kW = core::kW;
constexpr int kH = core::kH;
constexpr core::Rgba kClWhite{255, 255, 255, 255};

core::Rgba argb(int a, int r, int g, int b) {
    const auto byte = [](int v) { return static_cast<std::uint8_t>(v); };
    return {byte(r), byte(g), byte(b), byte(a)};
}

core::Rgba with_alpha(core::Rgba c, int a) {
    c.a = static_cast<std::uint8_t>(a);
    return c;
}

int round_int(double v) {
    return core::round_half_even(v);
}

std::string one_decimal(double v) {
    return std::format("{:.1f}", v);
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
void panel_back(Canvas& c, const Rect& panel, int dim_alpha = 210) {
    c.brush = argb(dim_alpha, 15, 15, 18);
    c.fill_round_rect(0, 0, kW, kH, 1, 1);
    c.brush = argb(235, 45, 45, 50);
    c.pen = argb(255, 90, 90, 95);
    c.pen_width = 1;
    fill_round_rect(c, panel, 14, 14);
}

void panel_title(Canvas& c, const Rect& panel, int top, std::string_view title) {
    c.font.size = 20;
    c.font_color = kClWhite;
    c.draw_text_centered(panel.x1, panel.y1 + top, panel.x2, panel.y1 + top + 30, title);
}

void panel_footer(Canvas& c, const Rect& panel, std::string_view text) {
    c.font.size = 11;
    c.font_color = argb(180, 180, 180, 180);
    c.draw_text_centered(panel.x1, panel.y2 - 24, panel.x2, panel.y2 - 6, text);
}

void render_help(Canvas& c) {
    const Rect p{(kW - 460) / 2, (kH - 330) / 2, (kW - 460) / 2 + 460, (kH - 330) / 2 + 330};
    panel_back(c, p);
    c.font.size = 20;
    c.font_color = kClWhite;
    c.draw_text_centered(p.x1, p.y1 + 16, p.x2, p.y1 + 46, "Справка");
    c.font.size = 13;
    c.font_color = argb(230, 220, 220, 220);
    int y = p.y1 + 64;
    const auto line = [&](std::string_view s, int step) {
        c.draw_text_centered(p.x1 + 20, y, p.x2 - 20, y + 20, s);
        y += step;
    };
    line("ЛКМ — выстрел / \"синяя\" кнопка в Dual Mode", 24);
    line("ПКМ — \"красная\" кнопка в Dual Mode", 24);
    line("R — начать текущий режим заново", 24);
    line("M — выйти в меню", 24);
    line("P — пауза", 24);
    line("Esc — закончить попытку", 34);
    line("Accuracy — % попаданий от всех выстрелов", 24);
    line("Precision — точность по центру круга", 34);
    line("Score — очки: меньше круг — больше очков", 24);
    c.font.size = 11;
    c.font_color = argb(180, 180, 180, 180);
    c.draw_text_centered(p.x1, p.y2 - 26, p.x2, p.y2 - 6, "Клик — закрыть");
}

void render_settings(Canvas& c, const core::Settings& s) {
    const Rect p = core::settings_panel();
    panel_back(c, p);
    c.font.size = 20;
    c.font_color = kClWhite;
    c.draw_text_centered(p.x1, p.y1 + 14, p.x2, p.y1 + 44, "Настройки");

    const std::array<std::string, 3> rows{
        std::format("Размер круга: {}", s.circle_size),
        std::format("Время жизни: {} мс", s.lifetime_ms),
        std::format("Жизни: {}", s.lives),
    };
    // Размер шрифта образец задаёт один раз: строки после первой пишутся
    // размером «−» и «+» (16), как у него.
    c.font.size = 13;
    for (int row = 0; row < 3; ++row) {
        const int y = core::settings_row_y(row);
        c.font_color = argb(230, 220, 220, 220);
        c.draw_text_centered(p.x1 + 20, y, p.x2 - 130, y + core::kSettingsButton,
                             rows.at(static_cast<std::size_t>(row)));
        // DrawSettingsStepper.
        c.brush = argb(150, 255, 255, 255);
        c.pen = argb(0, 0, 0, 0);
        fill_round_rect(c, core::settings_minus(row), 6, 6);
        c.font.size = 16;
        c.font_color = argb(255, 30, 30, 34);
        text_centered(c, core::settings_minus(row), "-");
        c.brush = argb(150, 255, 255, 255);
        fill_round_rect(c, core::settings_plus(row), 6, 6);
        text_centered(c, core::settings_plus(row), "+");
    }
    const auto toggle = [&](int row, const std::string& text) {
        const Rect r = core::settings_toggle(row);
        c.brush = argb(60, 255, 255, 255);
        fill_round_rect(c, r, 8, 8);
        c.font_color = kClWhite;
        text_centered(c, r, text);
    };
    toggle(3, std::string("Звук: ") + (s.sound ? "ON" : "OFF"));
    toggle(4, "Тема: " + std::string(core::kThemes.at(static_cast<std::size_t>(s.theme)).name));

    const Rect reset = core::settings_reset();
    c.brush = argb(160, 90, 90, 95);
    fill_round_rect(c, reset, 8, 8);
    c.font.size = 12;
    c.font_color = kClWhite;
    text_centered(c, reset, "Сбросить по умолчанию");
    // Подпись не помещается в строку и у образца (переносится и обрезается
    // по высоте 18 px) — здесь ей две строки (D-22).
    c.font.size = 11;
    c.font_color = argb(180, 180, 180, 180);
    c.draw_text_centered(p.x1, p.y2 - 44, p.x2, p.y2 - 6,
                         "Клик вне панели — закрыть. Изменения — со следующей игры.");
}

// Таблица лидеров: все четыре режима (AUD-09), у образца — только Classic и
// Dual в панели 480 px; здесь — четыре столбца в панели шире.
void render_leaderboard(Canvas& c, const core::Leaderboard& board) {
    constexpr int kPanelW = 760;
    constexpr int kPanelH = 420;
    const Rect p{(kW - kPanelW) / 2, (kH - kPanelH) / 2, (kW + kPanelW) / 2, (kH + kPanelH) / 2};
    panel_back(c, p);
    panel_title(c, p, 14, "Таблица лидеров");
    constexpr int kColW = (kPanelW - 20) / core::kModeCount;
    for (int m = 0; m < core::kModeCount; ++m) {
        const int x1 = p.x1 + 10 + m * kColW;
        const int x2 = x1 + kColW;
        c.font.size = 14;
        c.font_color = kClWhite;
        c.draw_text_centered(x1, p.y1 + 56, x2, p.y1 + 80,
                             core::kModeLooks.at(static_cast<std::size_t>(m)).title);
        c.font.size = 12;
        c.font_color = argb(220, 220, 220, 220);
        const auto& rows = board.at(static_cast<std::size_t>(m));
        for (int i = 0; i < core::kLeaderboardSize; ++i) {
            const int y = p.y1 + 90 + i * 28;
            std::string text = std::format("{}. —", i + 1);
            if (i < static_cast<int>(rows.size())) {
                const core::LeaderboardEntry& e = rows[static_cast<std::size_t>(i)];
                // В Tracking щелчков нет — точность у записи всегда 0, её не
                // показываем.
                text = m == core::mode_index(core::GameMode::Tracking)
                           ? std::format("{}. {}", i + 1, e.score)
                           : std::format("{}. {} ({}%)", i + 1, e.score, one_decimal(e.accuracy));
            }
            c.draw_text_centered(x1, y, x2, y + 24, text);
        }
    }
    panel_footer(c, p, "Клик — закрыть");
}

void render_achievements(Canvas& c, const core::Profile& profile) {
    const Rect p{(kW - 460) / 2, (kH - 440) / 2, (kW - 460) / 2 + 460, (kH - 440) / 2 + 440};
    panel_back(c, p);
    panel_title(c, p, 14, "Достижения");
    for (int i = 0; i < core::kAchievementCount; ++i) {
        const int y = p.y1 + 56 + i * 42;
        const auto at = static_cast<std::size_t>(i);
        const core::Rgba col =
            profile.achievements.at(at) ? argb(255, 255, 210, 90) : argb(140, 150, 150, 150);
        c.brush = argb(40, 255, 255, 255);
        c.fill_round_rect(p.x1 + 16, y, p.x2 - 16, y + 36, 8, 8);
        c.font_color = col;
        c.font.size = 13;
        c.draw_text_centered(p.x1 + 26, y + 2, p.x2 - 26, y + 18, core::kAchievements.at(at).name);
        c.font.size = 10;
        c.font_color = argb(180, 200, 200, 200);
        c.draw_text_centered(p.x1 + 26, y + 19, p.x2 - 26, y + 34,
                             core::kAchievements.at(at).description);
    }
    panel_footer(c, p, "Клик — закрыть");
}

void render_history(Canvas& c, const HistoryView& h) {
    const Rect p = core::history_panel();
    panel_back(c, p);
    panel_title(c, p, 14, "История");
    c.font.size = 12;
    c.font_color = argb(210, 210, 210, 210);
    c.draw_text_centered(p.x1, p.y1 + 46, p.x2, p.y1 + 64,
                         std::format("Забегов в истории: {}", h.games));

    constexpr std::array<std::string_view, 4> kBuckets{"Утро (6-12)", "День (12-18)",
                                                       "Вечер (18-23)", "Ночь (23-6)"};
    c.font.size = 13;
    int y = p.y1 + 76;
    for (std::size_t i = 0; i < kBuckets.size(); ++i) {
        c.font_color = argb(220, 220, 220, 220);
        const core::TimeOfDayScore& b = h.by_time_of_day.at(i);
        const std::string text = b.games > 0
                                     ? std::format("{}: средний счёт {} ({} игр)", kBuckets.at(i),
                                                   one_decimal(b.average_score), b.games)
                                     : std::format("{}: нет данных", kBuckets.at(i));
        c.draw_text_centered(p.x1 + 20, y, p.x1 + 500, y + 22, text);
        y += 26;
    }

    c.font.size = 12;
    c.font_color = argb(200, 200, 200, 200);
    c.draw_text_centered(p.x1 + 20, y + 8, p.x1 + 500, y + 26,
                         "Средняя реакция по последним играм:");
    y += 32;
    const int gx = p.x1 + 20;
    const int gy = y;
    constexpr int kGraphW = 480;
    constexpr int kGraphH = 140;
    c.brush = argb(120, 30, 30, 35);
    c.fill_round_rect(gx, gy, gx + kGraphW, gy + kGraphH, 6, 6);
    if (!h.recent_reactions.empty()) {
        const auto count = static_cast<int>(h.recent_reactions.size());
        int most = 100;
        for (const double v : h.recent_reactions)
            most = std::max(most, round_int(v));
        std::vector<IntPoint> pts;
        for (int i = 0; i < count; ++i) {
            const int v = round_int(h.recent_reactions[static_cast<std::size_t>(i)]);
            pts.push_back(
                {gx + round_int(i * kGraphW / (count - 1 + 0.0001)),
                 gy + kGraphH - round_int(static_cast<double>(v) * (kGraphH - 10) / most) - 5});
        }
        if (count >= 2) {
            draw_smooth_curve(c, pts, argb(255, 97, 207, 255), 2);
        } else {
            c.brush = argb(255, 97, 207, 255);
            c.fill_circle(pts[0].x, pts[0].y, 3);
        }
    }
    // «Экспорт в CSV» — после ответа автора, куда класть файл (TODO.md).
    c.font.size = 10;
    c.font_color = argb(170, 180, 180, 180);
    c.draw_text_centered(p.x1 + 250, p.y1 + 410, p.x1 + 500, p.y1 + 442, h.file_name);
    c.font.size = 11;
    c.font_color = argb(180, 180, 180, 180);
    c.draw_text_centered(p.x1, p.y1 + 436, p.x2, p.y1 + 454, "Клик вне панели — закрыть");
}

void render_profiles(Canvas& c, int current) {
    const Rect p = core::profiles_panel();
    panel_back(c, p);
    panel_title(c, p, 14, "Профили");
    for (int n = 1; n <= core::kProfileCount; ++n) {
        if (n == current) {
            c.brush = argb(200, 97, 207, 255);
            c.pen = kClWhite;
            c.pen_width = 2;
        } else {
            c.brush = argb(90, 255, 255, 255);
            c.pen = argb(0, 0, 0, 0);
            c.pen_width = 1;
        }
        const Rect row = core::profile_row(n);
        fill_round_rect(c, row, 8, 8);
        c.font.size = 14;
        c.font_color = kClWhite;
        text_centered(c, row, std::format("Профиль {}", n));
    }
    panel_footer(c, p, "Клик по профилю — переключить");
}

// Тексты туториала (TutorialTexts); у бомбы крест белый, а не красный, как
// писал образец (AUD-19).
constexpr std::array<std::string_view, core::kTutorialSteps> kTutorial{
    "Добро пожаловать в Aim Trainer! Кликай по кругам, пока они не исчезли.",
    "Чем ближе к центру — тем выше Precision (точность попадания).",
    "В Dual Mode синие круги — ЛКМ, красные — ПКМ. С бомбами (белый крест) — осторожно, "
    "лучше их не трогать!",
    "R — заново, M — в меню, P — пауза, Esc — закончить попытку. Удачи!",
};

void render_tutorial(Canvas& c, int step) {
    c.brush = argb(225, 15, 15, 18);
    c.fill_round_rect(0, 0, kW, kH, 1, 1);
    constexpr int kPanelW = 480;
    constexpr int kPanelH = 220;
    const Rect p{(kW - kPanelW) / 2, (kH - kPanelH) / 2, (kW + kPanelW) / 2, (kH + kPanelH) / 2};
    c.brush = argb(240, 45, 45, 50);
    c.pen = argb(255, 97, 207, 255);
    c.pen_width = 2;
    fill_round_rect(c, p, 14, 14);
    c.font.size = 16;
    c.font_color = kClWhite;
    c.draw_text_centered(p.x1 + 20, p.y1 + 30, p.x2 - 20, p.y1 + 130,
                         kTutorial.at(static_cast<std::size_t>(std::clamp(step, 0, 3))));
    c.font.size = 12;
    c.font_color = argb(200, 200, 200, 200);
    c.draw_text_centered(p.x1, p.y2 - 50, p.x2, p.y2 - 30, std::format("{} / 4", step + 1));
    c.font_color = argb(180, 180, 180, 180);
    c.draw_text_centered(p.x1, p.y2 - 26, p.x2, p.y2 - 8,
                         step < 3 ? "Клик — далее" : "Клик — начать!");
}

// Описание режима по строкам — как DrawModeDescription: не длиннее
// 330 / TextWidth('W') знаков, перенос по пробелу. У образца последнее слово
// всегда уходило на свою строку; здесь — только если не помещается.
std::vector<std::string> description_lines(std::string_view text, int max_chars) {
    // Знаки UTF-8: начало каждого.
    std::vector<std::size_t> starts;
    for (std::size_t i = 0; i < text.size(); ++i)
        if ((static_cast<unsigned char>(text[i]) & 0xC0U) != 0x80U) starts.push_back(i);
    const std::size_t n = starts.size();
    const auto byte_at = [&](std::size_t ch) { return ch < n ? starts[ch] : text.size(); };
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < n && lines.size() < 4) {
        std::size_t end = std::min(start + static_cast<std::size_t>(max_chars), n);  // за последним
        if (end < n) {
            std::size_t space = end;
            while (space > start && text[byte_at(space)] != ' ')
                --space;
            if (space > start) end = space + 1;
        }
        lines.emplace_back(text.substr(byte_at(start), byte_at(end) - byte_at(start)));
        start = end;
    }
    return lines;
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

void MenuView::render_particles(Canvas& c, const core::Profile& profile, core::Vec2 mouse) const {
    const auto& items = particles_.items();
    // Линии к курсору — только с эффектами меню; линии между частицами и
    // точки — всегда, как у образца.
    if (profile.settings.menu_effects) {
        for (const core::Particle& p : items) {
            const double dist = std::hypot(p.x - mouse.x, p.y - mouse.y);
            if (const auto alpha = core::link_alpha(dist, core::kCursorLink)) {
                c.pen = argb(*alpha, 100, 200, 255);
                c.pen_width = 1;
                c.line(round_int(p.x), round_int(p.y), round_int(mouse.x), round_int(mouse.y));
            }
        }
    }
    for (std::size_t i = 0; i + 1 < items.size(); ++i) {
        for (std::size_t j = i + 1; j < items.size(); ++j) {
            const double dist = std::hypot(items[i].x - items[j].x, items[i].y - items[j].y);
            if (const auto alpha = core::link_alpha(dist, core::kParticleLink)) {
                c.pen = argb(*alpha, 100, 200, 255);
                c.pen_width = 1;
                c.line(round_int(items[i].x), round_int(items[i].y), round_int(items[j].x),
                       round_int(items[j].y));
            }
        }
    }
    for (const core::Particle& p : items) {
        c.brush = argb(60, 200, 200, 255);
        c.fill_circle(round_int(p.x), round_int(p.y), 3);
    }
}

void MenuView::render_plate(Canvas& c, int mode, core::Vec2 mouse) const {
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
    c.brush = argb(bg_alpha, 255, 255, 255);
    c.pen = argb(border_alpha, 255, 255, 255);
    c.fill_round_rect(x, y, x + w, y + h, 12, 12);

    c.font.size = 18;
    c.font_color = kClWhite;
    c.brush = argb(0, 0, 0, 0);
    c.font.weight = Weight::Bold;

    const core::ModeLook& look = core::kModeLooks.at(static_cast<std::size_t>(mode));
    const int ax = x + w / 2;
    const int ay = y + h / 2 + 12;
    const double t = timer_mod(2000) / 2000;
    constexpr std::array<std::string_view, core::kModeCount> kTitles{
        "Режим - Classic", "Режим - Dual", "Режим - Tracking", "Режим - Flick"};
    const std::string_view title = kTitles.at(static_cast<std::size_t>(mode));
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
    switch (mode) {
        case 0: {  // появление и сжатие
            const int r = std::max(round_int(48 * (1 - t)), 4);
            const int alpha = fading(t);
            c.brush = with_alpha(look.circle, alpha);
            c.fill_circle(ax, ay, r);
            c.pen = argb(alpha, 180, 220, 255);
            c.pen_width = 2;
            c.circle(ax, ay, r);
            cross(ax, ay, argb(alpha, 255, 255, 255));
            break;
        }
        case 1: {  // синий и красный по очереди
            const double phase = timer_mod(4000) / 2000;
            const core::Rgba col = static_cast<int>(phase) % 2 == 0 ? look.circle : look.circle2;
            const double t2 = phase - std::floor(phase);
            const int r = std::max(round_int(42 * (1 - t2)), 4);
            const int alpha = fading(t2);
            c.brush = with_alpha(col, alpha);
            c.fill_circle(ax, ay, r);
            c.pen = argb(alpha, 255, 255, 255);
            c.pen_width = 2;
            c.circle(ax, ay, r);
            cross(ax, ay, argb(alpha, 255, 255, 255));
            break;
        }
        case 2: {  // движение со следом
            const int offset = round_int(55 * std::sin(static_cast<double>(timer) / 700));
            constexpr int kR = 20;
            for (int i = 1; i <= 6; ++i) {
                const int trail_alpha = 60 - i * 10;
                const int trail_off =
                    round_int(55 * std::sin(static_cast<double>(timer - 40LL * i) / 700));
                c.brush = with_alpha(look.circle, trail_alpha);
                c.fill_circle(ax + trail_off, ay, kR - i * 2);
            }
            c.brush = with_alpha(look.circle, 220);
            c.fill_circle(ax + offset, ay, kR);
            c.pen = kClWhite;
            c.pen_width = 2;
            c.circle(ax + offset, ay, kR);
            cross(ax + offset, ay, kClWhite);
            break;
        }
        default: {  // мелькание
            const double flick = timer_mod(500) / 500;
            if (flick < 0.25) {
                const int r = round_int(45 * (1 - flick / 0.25));
                const int alpha = round_int(255 * (1 - flick / 0.25));
                c.brush = with_alpha(look.circle, alpha);
                c.fill_circle(ax, ay, r);
                c.pen = argb(alpha, 255, 255, 255);
                c.pen_width = 2;
                c.circle(ax, ay, r);
                cross(ax, ay, argb(alpha, 255, 255, 255));
            }
            break;
        }
    }

    c.font.size = 10;
    c.font_color = kClWhite;
    c.brush = argb(0, 0, 0, 0);
    constexpr std::string_view kHint = "Кликни для старта";
    c.text_out(x + (w - c.text_width(kHint)) / 2, y + h - 24, kHint);
}

void MenuView::render(Canvas& c, Toasts& toasts, const core::Theme& theme, const core::Menu& menu,
                      const core::Profile& profile, const HistoryView& history, core::Vec2 mouse,
                      core::Nanoseconds now) const {
    c.clear_window(theme.background);
    render_particles(c, profile, mouse);
    render_plate(c, menu.selected_mode(), mouse);

    // DrawModeButtons: появление каскадом; выбранная — ярче.
    const core::Nanoseconds since = now > menu.entered_at() ? now - menu.entered_at() : 0;
    for (int i = 0; i < core::kModeCount; ++i) {
        const int entry = round_int(255 * core::mode_button_entry(i, since));
        const Rect b = core::mode_button(i, since);
        const bool selected = menu.selected_mode() == i;
        const int base = (selected ? 255 : 120) * entry / 255;
        const core::ModeLook& look = core::kModeLooks.at(static_cast<std::size_t>(i));
        c.brush = with_alpha(look.button, base);
        c.pen = argb(80 * entry / 255, 255, 255, 255);
        c.pen_width = 1;
        fill_round_rect(c, b, 8, 8);
        if (selected) {
            // Второй FillRoundRect образца: обводки нет, заливка вдвое плотнее.
            c.pen = argb(entry, 255, 255, 255);
            c.pen_width = 2;
            fill_round_rect(c, b, 8, 8);
            c.pen_width = 1;
        }
        c.font.size = 13;
        c.font_color = argb(entry, 255, 255, 255);
        c.brush = argb(0, 0, 0, 0);
        c.font.weight = Weight::Bold;
        const int tw = c.text_width(look.title);
        c.text_out(b.x1 + (core::kButtonW - tw) / 2, b.y1 + (core::kButtonH - 16) / 2, look.title);
        c.font.weight = Weight::Normal;
    }

    // DrawModeDescription.
    const Rect& d = core::kDescription;
    c.brush = argb(35, 255, 255, 255);
    c.pen = argb(55, 255, 255, 255);
    fill_round_rect(c, d, 8, 8);
    c.font.size = 11;
    c.font_color = argb(220, 255, 255, 255);
    const int max_chars = round_int(330.0 / c.text_width("W"));
    int ly = d.y1 + 14;
    for (const std::string& line : description_lines(
             core::kModeLooks.at(static_cast<std::size_t>(menu.selected_mode())).description,
             max_chars)) {
        c.brush = argb(0, 0, 0, 0);
        c.text_out(d.x1 + 12, ly, line);
        ly += 18;
    }

    // DrawGraph: последние 50 реакций по порядку (AUD-14), шкала 0–1000 мс.
    constexpr int kGraphX = 50;
    constexpr int kGraphY = kH / 2;
    constexpr int kGraphW = kW - 100;
    constexpr int kGraphH = kH / 2 - 110;
    c.brush = argb(150, 30, 30, 35);
    c.fill_round_rect(kGraphX, kGraphY, kGraphX + kGraphW, kGraphY + kGraphH, 8, 8);
    c.font.size = 10;
    c.font_color = argb(200, 200, 200, 200);
    for (int i = 0; i <= 5; ++i) {
        const int value = i * 200;
        const int y = kGraphY + kGraphH - round_int(value * kGraphH / 1000.0);
        c.pen = argb(80, 200, 200, 200);
        c.pen_width = 1;
        c.line(kGraphX, y, kGraphX + kGraphW, y);
        c.draw_text_centered(kGraphX - 50, y - 8, kGraphX - 5, y + 8, std::to_string(value));
    }
    const std::vector<int> reactions = profile.reactions.in_order();
    if (!reactions.empty()) {
        const auto count = static_cast<int>(reactions.size());
        std::vector<IntPoint> pts;
        for (int i = 0; i < count; ++i) {
            const int v = std::min(reactions[static_cast<std::size_t>(i)], 1000);
            pts.push_back({50 + round_int(static_cast<double>(i) * kGraphW / count),
                           kH / 2 + kGraphH - round_int(static_cast<double>(v) * kGraphH / 1000)});
        }
        draw_smooth_curve(c, pts, argb(255, 97, 207, 255), 2);
    }

    // Рекорды профиля. DejaVu шире Consolas: в 12 пт «Best Avg: 382.5ms» не
    // помещается в столбец 160 px и переносится — здесь 11 пт (D-22).
    c.font.size = 11;
    c.font_color = argb(200, 200, 200, 200);
    constexpr int kBottomY = kH - 50;
    constexpr int kCol = kW / 5;
    const std::array<std::string, 5> stats{
        std::format("Best Score: {}", profile.best_score),
        "Best Acc: " + one_decimal(profile.best_accuracy) + "%",
        "Best Prec: " + one_decimal(profile.best_precision) + "%",
        "Best Avg: " + one_decimal(profile.best_avg_reaction) + "ms",
        std::format("Games: {}", profile.games_played),
    };
    for (int i = 0; i < 5; ++i)
        c.draw_text_centered(kCol * i, kBottomY - 10, i == 4 ? kW : kCol * (i + 1), kBottomY + 10,
                             stats.at(static_cast<std::size_t>(i)));

    // Кнопки внизу (DrawMiniButton).
    c.font.size = 10;
    c.font_color = argb(170, 200, 200, 200);
    const std::array<std::string, 6> labels{
        "Справка", "Настройки", "Рекорды",
        "Награды", "История",   std::format("Профиль {}", menu.profile_number())};
    for (std::size_t i = 0; i < core::kMiniButtons.size(); ++i) {
        const Rect b = core::mini_button(core::kMiniButtons.at(i));
        c.pen = argb(200, 255, 255, 255);
        c.draw_round_rect(b.x1, b.y1, b.x2, b.y2, 10, 10);
        text_centered(c, b, labels.at(i));
    }
    c.font_color = argb(150, 200, 200, 200);
    c.draw_text_centered(kW - 120, kH - 20, kW - 10, kH - 10,
                         profile.settings.menu_effects ? "Effects: ON" : "Effects: OFF");

    toasts.render(c, now);

    switch (menu.panel()) {
        case core::Panel::Help: render_help(c); break;
        case core::Panel::Settings: render_settings(c, profile.settings); break;
        case core::Panel::Records: render_leaderboard(c, profile.leaderboard); break;
        case core::Panel::Achievements: render_achievements(c, profile); break;
        case core::Panel::History: render_history(c, history); break;
        case core::Panel::Profiles: render_profiles(c, menu.profile_number()); break;
        case core::Panel::None: break;
    }
    if (menu.tutorial_shown()) render_tutorial(c, menu.tutorial_step());
}

}  // namespace aim::app
