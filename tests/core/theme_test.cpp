// Темы (D-44): цвет #RRGGBB или #RRGGBBAA, тема игрока поверх встроенной,
// образец со всеми ключами читается в ту же тему, выбор по кругу.

#include "core/theme.h"

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

namespace aim::core {
namespace {

TEST(Theme, ParseColor) {
    EXPECT_EQ(parse_color("#1C1C20"), (Rgba{28, 28, 32, 255}));
    EXPECT_EQ(parse_color(" #ff000080 "), (Rgba{255, 0, 0, 128}));
    EXPECT_FALSE(parse_color("1C1C20"));
    EXPECT_FALSE(parse_color("#1C1C2"));
    EXPECT_FALSE(parse_color("#1C1C2G"));
    EXPECT_FALSE(parse_color("#"));
}

TEST(Theme, PlayerThemeOverBase) {
    const ParsedTheme t = read_theme(
        "; моя тема\n"
        "# комментарий\n"
        "background=#000000\n"
        "base=neon\n"
        "preset_flick=#11223344\n"
        "unknown=#000000\n"
        "text=white\n"
        "no equals sign\n");
    EXPECT_EQ(t.bad_lines, 3) << "неизвестный ключ, не цвет, не «ключ=цвет»";
    EXPECT_EQ(t.theme.background, (Rgba{0, 0, 0, 255})) << "заданное — из файла";
    EXPECT_EQ(t.theme.accent, kThemes.at(1).theme.accent)
        << "незаданное — из base, где бы ни стояла";
    EXPECT_EQ(t.theme.presets.at(4), (Rgba{0x11, 0x22, 0x33, 0x44}));
    EXPECT_EQ(t.theme.text, kThemes.at(1).theme.text);
}

TEST(Theme, DefaultBaseIsDarkAndBadBaseCounts) {
    EXPECT_EQ(read_theme("").theme.background, kThemes.front().theme.background);
    const ParsedTheme t = read_theme("base=sepia\n");
    EXPECT_EQ(t.bad_lines, 1);
    EXPECT_EQ(t.theme.background, kThemes.front().theme.background);
}

TEST(Theme, SampleHasEveryKeyAndReadsBack) {
    std::set<std::string_view> keys;
    for (const ThemeKey& k : kThemeKeys)
        EXPECT_TRUE(keys.insert(k.key).second) << "ключ дважды: " << k.key;
    for (const BuiltinTheme& b : kThemes) {
        const std::string text = write_theme(b.theme, "\r\n");
        const ParsedTheme back = read_theme(text);
        EXPECT_EQ(back.bad_lines, 0) << b.id;
        // Ключи покрывают все роли: тема из образца — та же.
        for (const ThemeKey& k : kThemeKeys) {
            const Rgba a = k.field != nullptr
                               ? back.theme.*k.field
                               : back.theme.presets.at(static_cast<std::size_t>(k.preset));
            const Rgba e = k.field != nullptr
                               ? b.theme.*k.field
                               : b.theme.presets.at(static_cast<std::size_t>(k.preset));
            EXPECT_EQ(a, e) << b.id << ": " << k.key;
        }
    }
    EXPECT_EQ(kThemeKeys.size(), sizeof(Theme) / sizeof(Rgba)) << "у каждой роли — свой ключ";
}

TEST(Theme, NamesAndCycle) {
    EXPECT_EQ(theme_name_of_file("моя.ini"), "моя");
    EXPECT_FALSE(theme_name_of_file(".ini"));
    EXPECT_FALSE(theme_name_of_file("моя.txt"));
    EXPECT_TRUE(is_theme_file("a.ini"));
    EXPECT_EQ(theme_title("neon"), "Неон");
    EXPECT_EQ(theme_title("моя"), "моя");
    const std::vector<std::string> mine{"дождь", "dark", "лес"};
    EXPECT_EQ(next_theme("dark", mine), "neon");
    EXPECT_EQ(next_theme("minimal", mine), "дождь") << "после встроенных — темы игрока";
    EXPECT_EQ(next_theme("дождь", mine), "лес") << "файл dark.ini встроенную не повторяет";
    EXPECT_EQ(next_theme("лес", mine), "dark") << "по кругу";
    EXPECT_EQ(next_theme("пропала", mine), "dark");
}

}  // namespace
}  // namespace aim::core
