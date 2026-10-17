#pragma once

// Разбор текста файлов: строки и числа — как StrToInt и StrToFloat образца
// (archive/pascal/ARCHITECTURE.md, §11): пробелы по краям и знак допускаются, точка —
// разделитель дробной части, всё прочее — не число.

#include <optional>
#include <string_view>
#include <vector>

namespace aim::core {

/// Строки текста: перевод строки — LF или CRLF, метка BOM в начале
/// пропускается, пустая последняя строка после финального перевода — нет.
std::vector<std::string_view> split_lines(std::string_view text);

/// Строка без пробельных знаков по краям.
std::string_view trim(std::string_view s);

/// Поля строки через разделитель.
std::vector<std::string_view> split(std::string_view line, char separator);

/// Целое: пробелы по краям, знак, цифры; иначе или вне int — пусто.
std::optional<int> parse_int(std::string_view text);

/// Вещественное с точкой: пробелы по краям, знак, порядок; иначе — пусто.
std::optional<double> parse_real(std::string_view text);

}  // namespace aim::core
