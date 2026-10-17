#include "app/text.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "core/wrap.h"

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
    for (const auto& family : kFiles) {
        for (const std::string_view file : family) {
            const std::string path = dir + std::string(file);
            if (!SDL_GetPathInfo(path.c_str(), nullptr)) {
                problem = "Нет файла шрифта: " + path;
                return nullptr;
            }
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
    // Шрифты читаются сейчас, а не на первом кадре: битый файл — тоже ошибка
    // запуска.
    for (const Family family : {Family::Mono, Family::Condensed}) {
        text->set_family(family);
        for (const Weight w : {Weight::Normal, Weight::Bold}) {
            if (text->font({.size = 14, .weight = w}) == nullptr) {
                problem = std::string("SDL_ttf: ") + SDL_GetError();
                return nullptr;
            }
        }
    }
    return text;
}

Text::Text(SDL_Renderer* renderer, TTF_TextEngine* engine, std::string dir)
    : renderer_(renderer), engine_(engine), dir_(std::move(dir)) {}

Text::~Text() {
    for (const auto& [key, cached] : cache_)
        TTF_DestroyText(cached.text);
    for (const auto& [key, f] : fonts_)
        TTF_CloseFont(f);
    for (const auto& [key, f] : draw_fonts_)
        TTF_CloseFont(f);
    TTF_DestroyRendererTextEngine(engine_);
    TTF_Quit();
}

TTF_Font* Text::open_font(Font font, float dpi) {
    const std::string path =
        dir_ +
        std::string(
            kFiles.at(static_cast<std::size_t>(family_)).at(font.weight == Weight::Bold ? 1U : 0U));
    TTF_Font* f = TTF_OpenFont(path.c_str(), static_cast<float>(font.size));
    if (f == nullptr) return nullptr;
    const auto d = static_cast<int>(std::lround(dpi));
    if (!TTF_SetFontSizeDPI(f, static_cast<float>(font.size), d, d)) {
        TTF_CloseFont(f);
        return nullptr;
    }
    return f;
}

TTF_Font* Text::font(Font font) {
    const Key k = key(font);
    if (const auto it = fonts_.find(k); it != fonts_.end()) return it->second;
    TTF_Font* f = open_font(font, kDpi);
    if (f != nullptr) fonts_.emplace(k, f);
    return f;
}

TTF_Font* Text::draw_font(Font font) {
    if (scale_ == 1.0F) return this->font(font);
    const Key k = key(font);
    if (const auto it = draw_fonts_.find(k); it != draw_fonts_.end()) return it->second;
    TTF_Font* f = open_font(font, kDpi * scale_);
    if (f != nullptr) draw_fonts_.emplace(k, f);
    return f;
}

std::string Text::text_key(std::string_view s, Font font) const {
    std::string k = std::to_string(font.size);
    k += family_ == Family::Mono ? 'm' : 'c';
    k += font.weight == Weight::Bold ? 'b' : 'n';
    k += s;
    return k;
}

void Text::draw(float x, float y, std::string_view s, Font font, core::Rgba color) {
    TTF_Font* f = draw_font(font);
    if (f == nullptr || s.empty()) return;
    // Раскладка строки — из кэша: те же надписи рисуются каждый кадр.
    std::string key = text_key(s, font);
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        TTF_Text* t = TTF_CreateText(engine_, f, s.data(), s.size());
        if (t == nullptr) return;
        it = cache_.emplace(std::move(key), Cached{.text = t}).first;
    }
    it->second.frame = frame_;
    TTF_SetTextColor(it->second.text, color.r, color.g, color.b, color.a);
    if (scale_ == 1.0F) {
        TTF_DrawRendererText(it->second.text, std::round(x), std::round(y));
        return;
    }
    // Знаки — в пикселях экрана: масштаб рендера на время надписи — 1, а
    // место и обрезка — в тех же пикселях.
    SDL_Rect clip{};
    const bool clipped =
        SDL_RenderClipEnabled(renderer_) && SDL_GetRenderClipRect(renderer_, &clip);
    SDL_SetRenderScale(renderer_, 1.0F, 1.0F);
    if (clipped) {
        const auto px = [&](int v) {
            return static_cast<int>(std::lround(static_cast<float>(v) * scale_));
        };
        const SDL_Rect screen{px(clip.x), px(clip.y), px(clip.w), px(clip.h)};
        SDL_SetRenderClipRect(renderer_, &screen);
    }
    TTF_DrawRendererText(it->second.text, std::round(x * scale_), std::round(y * scale_));
    SDL_SetRenderScale(renderer_, scale_, scale_);
    if (clipped) SDL_SetRenderClipRect(renderer_, &clip);
}

void Text::end_frame() {
    ++frame_;
    // Не нужные секунду с лишним кадров надписи — прочь (счёт, FPS, таймеры).
    constexpr std::uint64_t kKeepFrames = 300;
    std::erase_if(cache_, [&](const auto& entry) {
        if (frame_ - entry.second.frame <= kKeepFrames) return false;
        TTF_DestroyText(entry.second.text);
        return true;
    });
    std::erase_if(widths_,
                  [&](const auto& entry) { return frame_ - entry.second.frame > kKeepFrames; });
}

void Text::out(float x, float y, std::string_view s, Font font, core::Rgba color) {
    draw(x, y, s, font, color);
}

int Text::measure(std::string_view s, Font font) {
    // Ширина — по раскладке строки SDL3_ttf, как её и нарисует TTF_Text: у
    // Condensed знаки разной ширины (D-43). Мера — из кэша: те же строки
    // меряются каждый кадр.
    if (s.empty()) return 0;
    std::string k = text_key(s, font);
    if (const auto it = widths_.find(k); it != widths_.end()) {
        it->second.frame = frame_;
        return it->second.width;
    }
    TTF_Font* f = this->font(font);
    int w = 0;
    int h = 0;
    if (f == nullptr || !TTF_GetStringSize(f, s.data(), s.size(), &w, &h)) w = 0;
    widths_.emplace(std::move(k), Measured{.width = w, .frame = frame_});
    return w;
}

int Text::width(std::string_view s, Font font) {
    return measure(s, font) + 1;
}

std::vector<std::string> Text::wrap(std::string_view s, Font font, int max_width) {
    return core::wrap_lines(s, max_width,
                            [&](std::string_view line) { return measure(line, font); });
}

int Text::height(Font font) {
    TTF_Font* f = this->font(font);
    return f != nullptr ? TTF_GetFontHeight(f) : 0;
}

}  // namespace aim::app
