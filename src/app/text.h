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
#include <utility>

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

    /// Строка с левым верхним углом в (x, y) — TextOut.
    void out(float x, float y, std::string_view s, Font font, core::Rgba color);
    /// По центру прямоугольника — DrawTextCentered(x, y, x1, y1).
    void centered(const SDL_FRect& box, std::string_view s, Font font, core::Rgba color);
    /// Ширина строки, px — TextWidth: округлённая ширина и ещё 1.
    int width(std::string_view s, Font font);
    /// Высота строки шрифта, px.
    int height(Font font);

private:
    Text(SDL_Renderer* renderer, TTF_TextEngine* engine, std::string dir);
    TTF_Font* font(Font font);
    /// Нарисовать строку: левый верхний угол (x, y).
    void draw(float x, float y, std::string_view s, TTF_Font* f, core::Rgba color);

    SDL_Renderer* renderer_;
    TTF_TextEngine* engine_;
    std::string dir_;
    std::map<std::pair<int, Weight>, TTF_Font*> fonts_;
};

}  // namespace aim::app
