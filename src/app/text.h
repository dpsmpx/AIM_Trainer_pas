#pragma once

// Текст шрифтом DejaVu Sans Mono (D-22, D-31) — как у образца: TextOut,
// TextWidth и DrawTextCentered GraphABC (docs/ARCHITECTURE.md, §11). Размер
// — в пунктах при 96 точках на дюйм, как Font.Size образца.

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/palette.h"

namespace aim::app {

enum class Weight : std::uint8_t { Normal, Bold };

/// Шрифт и надпись: размер в пунктах, начертание.
struct Font {
    int size = 14;  ///< пунктов, как Font.Size образца
    Weight weight = Weight::Normal;
};

class Text {
public:
    /// Файлы шрифта — каталог fonts/ рядом с программой.
    static constexpr std::string_view kRegularFile = "DejaVuSansMono.ttf";
    static constexpr std::string_view kBoldFile = "DejaVuSansMono-Bold.ttf";

    /// Шрифт для рендера renderer. Нет файла или SDL3_ttf не завёлся —
    /// пусто и problem: какого файла нет и где он ожидался.
    static std::unique_ptr<Text> open(SDL_Renderer* renderer, std::string& problem);
    ~Text();
    Text(const Text&) = delete;
    Text& operator=(const Text&) = delete;

    /// Строка с левым верхним углом в (x, y).
    void out(float x, float y, std::string_view s, Font font, core::Rgba color);
    /// Ширина строки, px — TextWidth: ширина и ещё 1.
    int width(std::string_view s, Font font);
    /// Ширина строки, px, — как MeasureString.
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
    Text(TTF_TextEngine* engine, std::string dir);
    TTF_Font* font(Font font);
    /// Продвижение знака, px, с дробью.
    double advance(Font font);
    /// Нарисовать строку: левый верхний угол (x, y).
    void draw(float x, float y, std::string_view s, Font font, core::Rgba color);

    struct Cached {
        TTF_Text* text = nullptr;
        std::uint64_t frame = 0;  ///< кадр, в котором надпись рисовалась последней
    };

    TTF_TextEngine* engine_;
    std::string dir_;
    std::map<std::pair<int, Weight>, TTF_Font*> fonts_;
    std::map<std::pair<int, Weight>, double> advances_;
    std::unordered_map<std::string, Cached> cache_;
    std::uint64_t frame_ = 0;
};

}  // namespace aim::app
