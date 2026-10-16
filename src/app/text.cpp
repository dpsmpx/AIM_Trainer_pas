#include "app/text.h"

#include <cmath>
#include <string>
#include <utility>

namespace aim::app {

namespace {

// GraphABC: System.Drawing.Font(имя, размер) — пункты; экран — 96 точек на
// дюйм (D-31).
constexpr int kDpi = 96;

std::string fonts_dir() {
    const char* base = SDL_GetBasePath();
    return std::string(base != nullptr ? base : "") + "fonts/";
}

}  // namespace

std::unique_ptr<Text> Text::open(SDL_Renderer* renderer, std::string& problem) {
    const std::string dir = fonts_dir();
    for (const std::string_view file : {kRegularFile, kBoldFile}) {
        const std::string path = dir + std::string(file);
        if (!SDL_GetPathInfo(path.c_str(), nullptr)) {
            problem = "Font file not found: " + path;
            return nullptr;
        }
    }
    if (!TTF_Init()) {
        problem = std::string("SDL_ttf: ") + SDL_GetError();
        return nullptr;
    }
    TTF_TextEngine* engine = TTF_CreateRendererTextEngine(renderer);
    if (engine == nullptr) {
        problem = std::string("SDL_ttf: ") + SDL_GetError();
        TTF_Quit();
        return nullptr;
    }
    std::unique_ptr<Text> text(new Text(renderer, engine, dir));
    // Шрифт читается сейчас, а не на первом кадре: битый файл — тоже ошибка
    // запуска.
    for (const Weight w : {Weight::Normal, Weight::Bold}) {
        if (text->font({.size = 14, .weight = w}) == nullptr) {
            problem = std::string("SDL_ttf: ") + SDL_GetError();
            return nullptr;
        }
    }
    return text;
}

Text::Text(SDL_Renderer* renderer, TTF_TextEngine* engine, std::string dir)
    : renderer_(renderer), engine_(engine), dir_(std::move(dir)) {}

Text::~Text() {
    for (const auto& [key, f] : fonts_)
        TTF_CloseFont(f);
    TTF_DestroyRendererTextEngine(engine_);
    TTF_Quit();
}

TTF_Font* Text::font(Font font) {
    const auto key = std::make_pair(font.size, font.weight);
    if (const auto it = fonts_.find(key); it != fonts_.end()) return it->second;
    const std::string path =
        dir_ + std::string(font.weight == Weight::Bold ? kBoldFile : kRegularFile);
    TTF_Font* f = TTF_OpenFont(path.c_str(), static_cast<float>(font.size));
    if (f == nullptr) return nullptr;
    if (!TTF_SetFontSizeDPI(f, static_cast<float>(font.size), kDpi, kDpi)) {
        TTF_CloseFont(f);
        return nullptr;
    }
    fonts_.emplace(key, f);
    return f;
}

void Text::draw(float x, float y, std::string_view s, TTF_Font* f, core::Rgba color) {
    if (f == nullptr || s.empty()) return;
    TTF_Text* t = TTF_CreateText(engine_, f, s.data(), s.size());
    if (t == nullptr) return;
    TTF_SetTextColor(t, color.r, color.g, color.b, color.a);
    TTF_DrawRendererText(t, std::round(x), std::round(y));
    TTF_DestroyText(t);
}

void Text::out(float x, float y, std::string_view s, Font font, core::Rgba color) {
    draw(x, y, s, this->font(font), color);
}

void Text::centered(const SDL_FRect& box, std::string_view s, Font font, core::Rgba color) {
    TTF_Font* f = this->font(font);
    if (f == nullptr) return;
    int w = 0;
    int h = 0;
    if (!TTF_GetStringSize(f, s.data(), s.size(), &w, &h)) return;
    draw(box.x + (box.w - static_cast<float>(w)) / 2.0F,
         box.y + (box.h - static_cast<float>(TTF_GetFontHeight(f))) / 2.0F, s, f, color);
}

int Text::width(std::string_view s, Font font) {
    TTF_Font* f = this->font(font);
    int w = 0;
    int h = 0;
    if (f == nullptr || !TTF_GetStringSize(f, s.data(), s.size(), &w, &h)) return 0;
    return w + 1;
}

int Text::height(Font font) {
    TTF_Font* f = this->font(font);
    return f != nullptr ? TTF_GetFontHeight(f) : 0;
}

}  // namespace aim::app
