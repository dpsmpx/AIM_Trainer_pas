#pragma once

// Темы игрока (D-44): файл themes/ИМЯ.ini в папке сохранений — строки
// «ключ=цвет», цвет — #RRGGBB или #RRGGBBAA; ключ base= — встроенная тема,
// от которой берутся незаданные цвета (dark, neon или minimal; нет — dark).
// Только текст: файлы — программа (app/player_files.h).

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/palette.h"

namespace aim::core {

/// Ключ файла темы: роль цвета и что она красит.
struct ThemeKey {
    std::string_view key;
    Rgba Theme::*field = nullptr;  ///< нет — кнопка набора preset
    int preset = -1;
    std::string_view meaning;
};

/// Все ключи по порядку Theme.
extern const std::array<ThemeKey, 38> kThemeKeys;

/// Встроенная тема по слову id; нет — пусто.
const BuiltinTheme* builtin_theme(std::string_view id);
/// Тема для игрока: имя встроенной или имя файла темы.
std::string theme_title(std::string_view id);

/// Цвет #RRGGBB или #RRGGBBAA; не цвет — пусто.
std::optional<Rgba> parse_color(std::string_view text);

/// Тема из текста файла: незаданное — из base; строки не «ключ=цвет»,
/// неизвестные ключи и не цвета — в bad_lines.
struct ParsedTheme {
    Theme theme;
    int bad_lines = 0;
};
ParsedTheme read_theme(std::string_view text);
/// Текст файла темы со всеми ключами — образец для своей темы.
std::string write_theme(const Theme& theme, std::string_view newline = "\n");

/// Папка тем игрока в папке сохранений.
inline constexpr std::string_view kThemeFolder = "themes";

/// Имя файла темы: ИМЯ.ini — тема ИМЯ; иначе пусто.
std::optional<std::string> theme_name_of_file(std::string_view file);
/// Файл темы игрока: ИМЯ.ini.
bool is_theme_file(std::string_view file);

/// Следующая тема по кругу: встроенные, затем themes игрока (names, по
/// алфавиту). Текущей нет в списке — первая.
std::string next_theme(std::string_view current, const std::vector<std::string>& names);

}  // namespace aim::core
