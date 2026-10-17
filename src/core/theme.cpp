#include "core/theme.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <format>
#include <system_error>

#include "core/text.h"

namespace aim::core {

const std::array<ThemeKey, 37> kThemeKeys{{
    {"background", &Theme::background, -1, "фон окна"},
    {"accent", &Theme::accent, -1, "акцент: разделы справки, полоса прокрутки, график, рамки"},
    {"text", &Theme::text, -1, "основной текст"},
    {"text_dim", &Theme::text_dim, -1, "второстепенный текст"},
    {"label", &Theme::label, -1, "подпись у пары «подпись: значение»"},
    {"gold", &Theme::gold, -1, "рекорды, достижения, рамка уведомления"},
    {"good", &Theme::good, -1, "«Начать замер», «▶», плашка реплея"},
    {"warn", &Theme::warn, -1, "предупреждение: «Фальстарт»"},
    {"target_left", &Theme::target_left, -1, "цель для ЛКМ"},
    {"target_right", &Theme::target_right, -1, "цель для ПКМ"},
    {"signal", &Theme::signal, -1, "замер на цвет: окно после смены цвета"},
    {"bomb", &Theme::bomb, -1, "бомба"},
    {"bomb_cross", &Theme::bomb_cross, -1, "крест на бомбе"},
    {"hud", &Theme::hud, -1, "ячейки панели замера"},
    {"overlay", &Theme::overlay, -1, "плашки поверх замера"},
    {"trail", &Theme::trail, -1, "след курсора"},
    {"wave", &Theme::wave, -1, "волна попадания"},
    {"target_mark", &Theme::target_mark, -1, "метка стороны на цели «по цвету»"},
    {"dim", &Theme::dim, -1, "затемнение под панелью"},
    {"panel", &Theme::panel, -1, "панель"},
    {"panel_border", &Theme::panel_border, -1, "рамка панели"},
    {"row", &Theme::row, -1, "подложка строки панели"},
    {"button", &Theme::button, -1, "светлая кнопка «−»/«+»"},
    {"button_text", &Theme::button_text, -1, "текст на светлой кнопке"},
    {"control", &Theme::control, -1, "кнопка-действие"},
    {"toast", &Theme::toast, -1, "фон уведомления"},
    {"toast_text", &Theme::toast_text, -1, "текст уведомления"},
    {"plate", &Theme::plate, -1, "плашка-превью набора"},
    {"graph", &Theme::graph, -1, "фон графика"},
    {"grid", &Theme::grid, -1, "сетка графика"},
    {"particles", &Theme::particles, -1, "частицы фона меню"},
    {"preset_color", nullptr, 0, "кнопка «Реакция на цвет»"},
    {"preset_classic", nullptr, 1, "кнопка «Классика»"},
    {"preset_dual", nullptr, 2, "кнопка «Две кнопки»"},
    {"preset_tracking", nullptr, 3, "кнопка «Слежение»"},
    {"preset_flick", nullptr, 4, "кнопка «Флик»"},
    {"preset_custom", &Theme::custom, -1, "кнопка «Свой набор»"},
}};

namespace {

Rgba& color_of(Theme& t, const ThemeKey& k) {
    return k.field != nullptr ? t.*k.field : t.presets.at(static_cast<std::size_t>(k.preset));
}

Rgba color_of(const Theme& t, const ThemeKey& k) {
    return k.field != nullptr ? t.*k.field : t.presets.at(static_cast<std::size_t>(k.preset));
}

}  // namespace

const BuiltinTheme* builtin_theme(std::string_view id) {
    for (const BuiltinTheme& b : kThemes)
        if (b.id == id) return &b;
    return nullptr;
}

std::string theme_title(std::string_view id) {
    if (const BuiltinTheme* b = builtin_theme(id)) return std::string(b->name);
    return std::string(id);
}

std::optional<Rgba> parse_color(std::string_view text) {
    text = trim(text);
    if (!text.starts_with('#') || (text.size() != 7 && text.size() != 9)) return std::nullopt;
    std::array<std::uint8_t, 4> parts{0, 0, 0, 255};
    for (std::size_t i = 0; 1 + 2 * i < text.size(); ++i) {
        unsigned value = 0;
        const char* begin = text.data() + 1 + 2 * i;
        const auto [end, error] = std::from_chars(begin, begin + 2, value, 16);
        if (error != std::errc{} || end != begin + 2) return std::nullopt;
        parts.at(i) = static_cast<std::uint8_t>(value);
    }
    return Rgba{parts[0], parts[1], parts[2], parts[3]};
}

ParsedTheme read_theme(std::string_view text) {
    const std::vector<std::string_view> lines = split_lines(text);
    ParsedTheme parsed{.theme = kThemes.front().theme, .bad_lines = 0};
    // Основа — сперва: цвета поверх неё, где бы ни стояла строка base=.
    for (const std::string_view raw : lines) {
        const std::string_view line = trim(raw);
        if (!line.starts_with("base=")) continue;
        if (const BuiltinTheme* b = builtin_theme(trim(line.substr(5))))
            parsed.theme = b->theme;
        else
            ++parsed.bad_lines;
    }
    for (const std::string_view raw : lines) {
        const std::string_view line = trim(raw);
        if (line.empty() || line.starts_with('#') || line.starts_with(';') ||
            line.starts_with("base="))
            continue;
        const std::size_t eq = line.find('=');
        const std::string_view key = trim(line.substr(0, eq));
        const auto k = std::find_if(kThemeKeys.begin(), kThemeKeys.end(),
                                    [&](const ThemeKey& t) { return t.key == key; });
        const std::optional<Rgba> color =
            eq == std::string_view::npos ? std::nullopt : parse_color(line.substr(eq + 1));
        if (k == kThemeKeys.end() || !color) {
            ++parsed.bad_lines;
            continue;
        }
        color_of(parsed.theme, *k) = *color;
    }
    return parsed;
}

std::string write_theme(const Theme& theme, std::string_view newline) {
    std::string out;
    for (const ThemeKey& k : kThemeKeys) {
        const Rgba c = color_of(theme, k);
        out += std::format("; {}{}", k.meaning, newline);
        out += c.a == 255 ? std::format("{}=#{:02X}{:02X}{:02X}{}", k.key, c.r, c.g, c.b, newline)
                          : std::format("{}=#{:02X}{:02X}{:02X}{:02X}{}", k.key, c.r, c.g, c.b, c.a,
                                        newline);
    }
    return out;
}

std::optional<std::string> theme_name_of_file(std::string_view file) {
    if (file.size() <= 4 || !file.ends_with(".ini")) return std::nullopt;
    return std::string(file.substr(0, file.size() - 4));
}

bool is_theme_file(std::string_view file) {
    return theme_name_of_file(file).has_value();
}

std::string next_theme(std::string_view current, const std::vector<std::string>& names) {
    std::vector<std::string> all;
    all.reserve(kThemes.size() + names.size());
    for (const BuiltinTheme& b : kThemes)
        all.emplace_back(b.id);
    for (const std::string& n : names)
        if (!builtin_theme(n)) all.push_back(n);
    const auto it = std::find(all.begin(), all.end(), current);
    if (it == all.end() || std::next(it) == all.end()) return all.front();
    return *std::next(it);
}

}  // namespace aim::core
