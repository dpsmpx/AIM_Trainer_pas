// Перенос строк и обрезка по ширине (core/wrap.h, D-43): ширина — 10 px на
// знак UTF-8, как у моноширинного шрифта.

#include "core/wrap.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

namespace aim::core {
namespace {

int chars(std::string_view s) {
    int n = 0;
    for (const char c : s)
        n += (static_cast<unsigned char>(c) & 0xC0U) == 0x80U ? 0 : 1;
    return n;
}

const Measure kMono = [](std::string_view s) { return 10 * chars(s); };

using Lines = std::vector<std::string>;

TEST(Wrap, FitsOnOneLine) {
    EXPECT_EQ(wrap_lines("Классика", 80, kMono), Lines{"Классика"});
    EXPECT_EQ(wrap_lines("", 80, kMono), Lines{""});
}

TEST(Wrap, BreaksAtSpacesAndDropsTrailingOnes) {
    EXPECT_EQ(wrap_lines("Щёлкай ЛКМ по кругам", 110, kMono), (Lines{"Щёлкай ЛКМ", "по кругам"}));
    EXPECT_EQ(wrap_lines("один  два   ", 40, kMono), (Lines{"один", "два"}));
}

TEST(Wrap, NewlineStartsLine) {
    EXPECT_EQ(wrap_lines("строка\nещё", 200, kMono), (Lines{"строка", "ещё"}));
    EXPECT_EQ(wrap_lines("a\n\nb", 200, kMono), (Lines{"a", "", "b"}));
}

TEST(Wrap, LongWordBreaksByUtf8Chars) {
    EXPECT_EQ(wrap_lines("абвгдеёж", 30, kMono), (Lines{"абв", "где", "ёж"}));
    EXPECT_EQ(wrap_lines("x абвгде", 30, kMono), (Lines{"x", "абв", "где"}));
}

TEST(Wrap, EveryLineFits) {
    const std::string s =
        "Размер, жизнь цели, жизни и окончание — для всех наборов. Клик вне "
        "панели — закрыть.";
    for (int width = 10; width <= 400; width += 7)
        for (const std::string& line : wrap_lines(s, width, kMono))
            EXPECT_LE(kMono(line), width) << width << ": " << line;
}

TEST(FitLeft, KeepsShortTextAndCutsFromTheLeft) {
    EXPECT_EQ(fit_left("Папка: ", "C:/a/", 200, kMono), "Папка: C:/a/");
    const std::string cut = fit_left("Папка: ", "C:/Пользователи/игрок/AimTrainer/", 200, kMono);
    EXPECT_EQ(cut, "Папка: …/AimTrainer/");
    EXPECT_LE(kMono(cut), 200);
    EXPECT_EQ(fit_left("Папка: ", "длинный", 50, kMono), "Папка: …") << "хвоста не осталось";
}

}  // namespace
}  // namespace aim::core
