#pragma once

// Перенос строк и обрезка по ширине (D-43) — без шрифта: ширину строки
// меряет measure, его даёт программа (app/text.h). Так перенос проверяется
// модульными тестами (AUD-49).

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace aim::core {

/// Ширина строки в пикселях.
using Measure = std::function<int(std::string_view)>;

/// Строки текста s по ширине max_width: перевод строки — новая строка,
/// перенос — по пробелам, слово длиннее строки — по знакам UTF-8, как
/// DrawString в прямоугольнике. Пробелы на концах строк отброшены.
std::vector<std::string> wrap_lines(std::string_view s, int max_width, const Measure& measure);

/// prefix и конец text не шире width: начало text отрезается по знакам
/// UTF-8 и заменяется «…» — путь папки: «Папка: …\Roaming\AimTrainer\».
std::string fit_left(std::string_view prefix, std::string_view text, int width,
                     const Measure& measure);

}  // namespace aim::core
