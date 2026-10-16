#pragma once

// Текст шрифтом DejaVu (D-31, D-43): Sans Condensed — по умолчанию, Sans
// Mono — на выбор в настройках; TextOut, TextWidth и DrawTextCentered — как
// у GraphABC (docs/ARCHITECTURE.md, §11). Размер — в пунктах при 96 точках
// на дюйм.

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/palette.h"

namespace aim::app {

enum class Weight : std::uint8_t { Normal, Bold };
/// Шрифт текста программы (D-43).
enum class Family : std::uint8_t { Condensed, Mono };

/// Шрифт и надпись: размер в пунктах, начертание.
struct Font {
    int size = 14;  ///< пунктов, как Font.Size образца
    Weight weight = Weight::Normal;
};

class Text {
public:
    /// Файлы шрифтов — каталог fonts/ рядом с программой: семейство,
    /// начертание.
    static constexpr std::array<std::array<std::string_view, 2>, 2> kFiles{{
        {"DejaVuSansCondensed.ttf", "DejaVuSansCondensed-Bold.ttf"},
        {"DejaVuSansMono.ttf", "DejaVuSansMono-Bold.ttf"},
    }};

    /// Шрифт для рендера renderer. Нет файла или SDL3_ttf не завёлся —
    /// пусто и problem: какого файла нет и где он ожидался.
    static std::unique_ptr<Text> open(SDL_Renderer* renderer, std::string& problem);
    /// Масштаб монитора (D-42): раскладка — в логических пикселях, а знаки
    /// растеризуются в пикселях экрана — текст чёткий. Задаётся при запуске,
    /// до первой надписи.
    void set_scale(float scale) { scale_ = scale; }
    /// Шрифт всех надписей (настройка «Шрифт», D-43).
    void set_family(Family family) { family_ = family; }
    [[nodiscard]] Family family() const { return family_; }
    ~Text();
    Text(const Text&) = delete;
    Text& operator=(const Text&) = delete;

    /// Строка с левым верхним углом в (x, y).
    void out(float x, float y, std::string_view s, Font font, core::Rgba color);
    /// Ширина строки, px — TextWidth: ширина и ещё 1.
    int width(std::string_view s, Font font);
    /// Ширина строки, px, — как MeasureString: по раскладке строки SDL3_ttf.
    int measure(std::string_view s, Font font);
    /// Высота строки шрифта, px.
    int height(Font font);
    /// Кадр показан: давно не рисованные надписи — из кэша.
    void end_frame();
    /// Строки текста по ширине max_width: перевод строки — новая строка,
    /// перенос — по пробелам, слово длиннее строки — по знакам, как DrawString
    /// в прямоугольнике. Пробелы на концах строк отброшены.
    std::vector<std::string> wrap(std::string_view s, Font font, int max_width);

private:
    Text(SDL_Renderer* renderer, TTF_TextEngine* engine, std::string dir);
    /// Шрифт для меры строк — в логических пикселях.
    TTF_Font* font(Font font);
    /// Шрифт для рисования — в пикселях экрана (масштаб).
    TTF_Font* draw_font(Font font);
    TTF_Font* open_font(Font font, float dpi);
    using Key = std::tuple<Family, int, Weight>;
    [[nodiscard]] Key key(Font font) const { return {family_, font.size, font.weight}; }
    /// Ключ кэшей надписи: шрифт и строка.
    [[nodiscard]] std::string text_key(std::string_view s, Font font) const;
    /// Нарисовать строку: левый верхний угол (x, y).
    void draw(float x, float y, std::string_view s, Font font, core::Rgba color);

    struct Cached {
        TTF_Text* text = nullptr;
        std::uint64_t frame = 0;  ///< кадр, в котором надпись рисовалась последней
    };

    SDL_Renderer* renderer_;
    TTF_TextEngine* engine_;
    std::string dir_;
    float scale_ = 1.0F;
    Family family_ = Family::Condensed;
    std::map<Key, TTF_Font*> fonts_;
    std::map<Key, TTF_Font*> draw_fonts_;
    std::unordered_map<std::string, Cached> cache_;
    struct Measured {
        int width = 0;
        std::uint64_t frame = 0;
    };
    std::unordered_map<std::string, Measured> widths_;
    std::uint64_t frame_ = 0;
};

}  // namespace aim::app
