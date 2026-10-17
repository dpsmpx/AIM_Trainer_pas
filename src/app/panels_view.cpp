#include "app/panels_view.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/draw_util.h"
#include "core/help.h"
#include "core/layout.h"
#include "core/menu.h"
#include "core/records.h"
#include "core/theme.h"

namespace aim::app {

namespace {

using core::Rect;
constexpr int kW = core::kW;
constexpr int kH = core::kH;

// Затемнение под панелью и сама панель с рамкой (роль темы panel_border).
void panel_back(Canvas& c, const core::Theme& th, const Rect& panel) {
    c.fill_round_rect(0, 0, kW, kH, 1, 1, th.dim);
    c.fill_round_rect(panel, 14, 14, th.panel);
    c.stroke_round_rect(panel, 14, 14, {.color = th.panel_border});
}

void panel_title(Canvas& c, const core::Theme& th, const Rect& panel, int top,
                 std::string_view title) {
    c.text_centered(panel.x1, panel.y1 + top, panel.x2, panel.y1 + top + 34, title, kTitle,
                    th.text);
}

void panel_footer(Canvas& c, const core::Theme& th, const Rect& panel, std::string_view text) {
    c.text_centered(panel.x1, panel.y2 - 28, panel.x2, panel.y2 - 6, text, kText, th.text_dim);
}

// Кнопки «−» и «+» ряда со ступенями.
void steppers(Canvas& c, const core::Theme& th, const Rect& minus, const Rect& plus) {
    for (const Rect& r : {minus, plus})
        c.fill_round_rect(r, 6, 6, th.button);
    c.text_centered(minus, "-", kText, th.button_text);
    c.text_centered(plus, "+", kText, th.button_text);
}

// Тексты туториала: по одному на шаг.
constexpr auto kTutorial = std::to_array<std::string_view>({
    "Добро пожаловать в DP AIM Trainer! Выбери набор замера — замер начнётся через 3 секунды. "
    "«Свой набор» — любые опции.",
    "«Реакция на цвет»: щёлкни, как только окно сменит цвет. Раньше — фальстарт.",
    "С целями: чем ближе к центру — тем выше точность. Бомбы (белый крест) лучше не трогать!",
    "Esc — закончить замер, R — заново, M — в меню. Рекорд — у каждого набора свой. Удачи!",
});
static_assert(kTutorial.size() == core::kTutorialSteps);

}  // namespace

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
        c.text_centered(p.x1 + 20, y, p.x2 - 30, y + core::kHelpLineH, line.text,
                        line.heading ? kTextBold : kText, line.heading ? th.accent : th.text);
    }
    // Полоса прокрутки: дорожка и бегунок — доля и место видимых строк.
    const Rect track = core::help_track();
    c.fill_round_rect(track, 6, 6, th.row);
    const int track_h = track.y2 - track.y1;
    const auto total = static_cast<double>(core::kHelp.size());
    const int thumb_h = std::max(20, round_int(track_h * core::kHelpVisibleLines / total));
    const int thumb_y =
        track.y1 + (core::kHelpMaxScroll > 0 ? round_int(static_cast<double>(track_h - thumb_h) *
                                                         scroll / core::kHelpMaxScroll)
                                             : 0);
    c.fill_round_rect(track.x1, thumb_y, track.x2, thumb_y + thumb_h, 6, 6, th.accent);
    c.text_centered(p.x1, p.y2 - 26, p.x2, p.y2 - 6,
                    "↑ ↓ и колёсико — листать · Esc или клик вне панели — закрыть", kText,
                    th.text_dim);
}

std::string fit_path(Canvas& c, std::string_view prefix, std::string_view path, int width) {
    std::string tail(path);
    const auto text = [&](bool cut) { return std::string(prefix) + (cut ? "…" : "") + tail; };
    bool cut = false;
    while (!tail.empty() && c.text_width(text(cut), kText) > width) {
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
    const auto stepper = [&](int row, const std::string& text) {
        const int y = core::settings_row_y(row);
        c.pair_centered(p.x1 + 20, y, p.x2 - 120, y + core::kSettingsButton, text, kText, th.label,
                        th.text);
        steppers(c, th, core::settings_minus(row), core::settings_plus(row));
    };
    const auto toggle = [&](int row, const std::string& text) {
        const Rect r = core::settings_toggle(row);
        c.fill_round_rect(r, 8, 8, th.row);
        c.pair_centered(r, text, kText, th.label, th.text);
    };
    const auto on_off = [](bool on) { return on ? "ON" : "OFF"; };
    const core::PresetSettings& ps = s.preset;
    stepper(core::kSettingsSizeRow, std::format("Размер цели: {} px", ps.target_size));
    stepper(core::kSettingsLifetimeRow, std::format("Жизнь цели: {} мс", ps.lifetime_ms));
    stepper(core::kSettingsLivesRow, std::format("Жизни: {}", ps.lives));
    toggle(core::kSettingsEndingRow,
           ps.ending == core::Ending::Lives
               ? "Окончание: по жизням"
               : std::format("Окончание: {} с", core::MeasureOptions{}.duration_s));
    stepper(core::kSettingsAverageRow, std::format("Среднее за N замеров: {}", s.average_runs));
    toggle(core::kSettingsSoundRow, std::format("Звук: {}", on_off(s.sound)));
    toggle(core::kSettingsThemeRow, "Тема: " + core::theme_title(s.theme));
    toggle(core::kSettingsFontRow,
           std::string("Шрифт: ") + (s.font == 1 ? "DejaVu Sans Mono" : "DejaVu Sans Condensed"));
    toggle(core::kSettingsMenuEffectsRow, std::format("Эффекты меню: {}", on_off(s.menu_effects)));
    toggle(core::kSettingsHudRow, std::format("Интерфейс замера: {}", on_off(s.hud)));
    toggle(core::kSettingsReplaysRow, std::format("Запись реплеев: {}", on_off(s.replays)));
    toggle(core::kSettingsOpenFolderRow, "Открыть папку сохранений");
    const Rect folder_row = core::settings_toggle(core::kSettingsChooseFolderRow);
    toggle(core::kSettingsChooseFolderRow,
           fit_path(c, "Папка: ", folder, folder_row.x2 - folder_row.x1 - 16));

    const Rect reset = core::settings_reset();
    c.fill_round_rect(reset, 8, 8, th.control);
    c.text_centered(reset, "Сбросить", kText, th.text);
    c.text_centered(p.x1 + 10, p.y2 - 44, p.x2 - 10, p.y2 - 6,
                    "Размер, жизнь цели, жизни и окончание — для всех наборов. Клик вне "
                    "панели — закрыть.",
                    kText, th.text_dim);
}

// Опция, которая к замеру не относится, — бледная и с прочерком.
void render_constructor(Canvas& c, const core::Theme& th, const core::Profile& profile) {
    const Rect p = core::constructor_panel();
    panel_back(c, th, p);
    panel_title(c, th, p, 10, "Свой набор");
    const core::MeasureOptions& o = profile.custom;
    // Заготовки: набор того же вида — ярче.
    const std::optional<int> same = core::preset_index(o);
    for (int i = 0; i < core::kPresetCount; ++i) {
        const Rect r = core::constructor_preset(i);
        c.fill_round_rect(
            r, 6, 6, faded(th.presets.at(static_cast<std::size_t>(i)), same == i ? 1.0 : 0.45));
        c.text_centered(r, core::kPresets.at(static_cast<std::size_t>(i)).name, kText, th.text);
    }
    const core::Rgba off = faded(th.text_dim, 0.55);
    for (int r = 0; r < core::kOptionRows; ++r) {
        const auto row = static_cast<core::OptionRow>(r);
        const Rect box = core::constructor_row(r);
        const bool on = core::option_applies(o, row);
        c.fill_round_rect(box, 8, 8, on ? th.row : faded(th.row, 0.4));
        const int ty = box.y1 + (box.y2 - box.y1 - c.line_height(kText)) / 2;
        c.text(box.x1 + 12, ty, core::option_label(o, row), kText, on ? th.text : off);
        const std::string value = on ? core::option_value(o, row) : "—";
        if (core::option_toggles(row)) {
            c.text(box.x2 - 12 - c.text_width(value, kText), ty, value, kText,
                   on ? th.accent : off);
            continue;
        }
        const Rect minus = core::constructor_minus(r);
        const Rect plus = core::constructor_plus(r);
        if (on) steppers(c, th, minus, plus);
        c.text_centered(minus.x2, box.y1, plus.x1, box.y2, value, kText, on ? th.text : off);
    }
    const Rect start = core::constructor_start();
    c.fill_round_rect(start, 10, 10, th.good);
    c.text_centered(start, "Начать замер", kTextBold, th.text);
    panel_footer(c, th, p, "Заготовка заполняет опции · Esc или клик вне панели — закрыть");
}

// Свежие наборы первыми; у строки с реплеем — «▶»: щелчок проигрывает его
// (D-39).
void render_records(Canvas& c, const core::Theme& th, const core::Profile& profile,
                    const PlayerFiles& files) {
    const Rect p = core::records_panel();
    panel_back(c, th, p);
    panel_title(c, th, p, 12, "Рекорды наборов");
    const int n = profile.settings.average_runs;
    const std::vector<core::ComboStats> all = core::all_combo_stats(profile.runs, n);
    if (all.empty())
        c.text_centered(p.x1, p.y1 + 120, p.x2, p.y1 + 150, "Пока нет замеров", kText, th.text_dim);
    bool any_replay = false;
    for (int i = 0; i < std::min(core::kRecordsRows, static_cast<int>(all.size())); ++i) {
        const core::ComboStats& st = all.at(static_cast<std::size_t>(i));
        const Rect row = core::records_row(i);
        const int y = row.y1;
        const bool replay = files.has_replay(st.options);
        any_replay = any_replay || replay;
        c.fill_round_rect(row, 8, 8, th.row);
        if (replay) {
            // Треугольник «▶» слева: у набора есть реплей.
            const int cx = row.x1 + 14;
            const int cy = (row.y1 + row.y2) / 2;
            const std::array<IntPoint, 3> play{{{cx - 4, cy - 7}, {cx + 7, cy}, {cx - 4, cy + 7}}};
            c.fill_polygon(play, th.good);
        }
        c.text_centered(
            p.x1 + 28, y + 2, p.x2 - 20, y + 21,
            std::format("{}: {}", core::combo_name(st.options), core::describe_short(st.options)),
            kText, th.text);
        const std::string best =
            st.best
                ? primary_text(st.options, core::primary_value(profile.runs.at(*st.best).summary))
                : "—";
        c.text_centered(
            p.x1 + 28, y + 21, p.x2 - 20, y + 40,
            std::format("рекорд {} · среднее за {}: {} · замеров: {}", best, st.average.runs,
                        primary_text(st.options, st.average.primary), st.runs),
            kText, th.gold);
    }
    panel_footer(c, th, p,
                 any_replay ? "Клик по строке с ▶ — реплей рекорда · мимо строк — закрыть"
                            : "Клик — закрыть");
}

void render_achievements(Canvas& c, const core::Theme& th, const core::Profile& profile) {
    const Rect p = core::achievements_panel();
    panel_back(c, th, p);
    panel_title(c, th, p, 14, "Достижения");
    for (int i = 0; i < core::kAchievementCount; ++i) {
        const int y = p.y1 + 52 + i * 46;
        const auto at = static_cast<std::size_t>(i);
        const core::Rgba col = profile.achievements.at(at) ? th.gold : faded(th.text_dim, 0.6);
        c.fill_round_rect(p.x1 + 16, y, p.x2 - 16, y + 42, 8, 8, th.row);
        c.text_centered(p.x1 + 26, y + 2, p.x2 - 26, y + 21, core::kAchievements.at(at).name, kText,
                        col);
        c.text_centered(p.x1 + 26, y + 21, p.x2 - 26, y + 40,
                        core::kAchievements.at(at).description, kText, th.text_dim);
    }
    panel_footer(c, th, p, "Клик — закрыть");
}

// Последние замеры с параметрами (D-38) и экспорт CSV (D-33).
void render_history(Canvas& c, const core::Theme& th, const core::Profile& profile,
                    std::string_view file_name) {
    const Rect p = core::history_panel();
    panel_back(c, th, p);
    panel_title(c, th, p, 12, "История");
    c.text_centered(p.x1, p.y1 + 42, p.x2, p.y1 + 64,
                    std::format("Замеров в истории: {}", profile.runs.size()), kText, th.text_dim);
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
        c.text_centered(p.x1 + 16, y, p.x2 - 16, y + 22, text, kText, th.text);
        y += 22;
    }
    // «Экспорт в CSV» — в папку сохранений (D-33).
    const Rect export_button = core::history_export();
    c.fill_round_rect(export_button, 8, 8, th.control);
    c.text_centered(export_button, "Экспорт в CSV", kText, th.text);
    c.text_centered(export_button.x2 + 10, export_button.y1, p.x2 - 10, export_button.y2, file_name,
                    kText, th.text_dim);
    panel_footer(c, th, p, "Клик вне панели — закрыть");
}

void render_profiles(Canvas& c, const core::Theme& th, int current) {
    const Rect p = core::profiles_panel();
    panel_back(c, th, p);
    panel_title(c, th, p, 14, "Профили");
    for (int n = 1; n <= core::kProfileCount; ++n) {
        const Rect row = core::profile_row(n);
        c.fill_round_rect(row, 8, 8, n == current ? faded(th.accent, 0.8) : th.row);
        c.text_centered(row, std::format("Профиль {}", n), kText, th.text);
    }
    panel_footer(c, th, p, "Клик по профилю — переключить");
}

void render_tutorial(Canvas& c, const core::Theme& th, int step) {
    c.fill_round_rect(0, 0, kW, kH, 1, 1, th.dim);
    const Rect p = core::tutorial_panel();
    c.fill_round_rect(p, 14, 14, th.panel);
    const int at = std::clamp(step, 0, core::kTutorialSteps - 1);
    c.text_centered(p.x1 + 20, p.y1 + 30, p.x2 - 20, p.y1 + 130,
                    kTutorial.at(static_cast<std::size_t>(at)), kText, th.text);
    c.text_centered(p.x1, p.y2 - 54, p.x2, p.y2 - 32,
                    std::format("{} / {}", at + 1, core::kTutorialSteps), kText, th.text_dim);
    c.text_centered(p.x1, p.y2 - 30, p.x2, p.y2 - 8,
                    at < core::kTutorialSteps - 1 ? "Клик — далее" : "Клик — начать!", kText,
                    th.text_dim);
}

}  // namespace aim::app
