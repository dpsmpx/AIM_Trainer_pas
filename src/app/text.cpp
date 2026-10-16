#include "app/text.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
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
            problem = "Нет файла шрифта: " + path;
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
        dir_ + std::string(font.weight == Weight::Bold ? kBoldFile : kRegularFile);
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
    const auto key = std::make_pair(font.size, font.weight);
    if (const auto it = fonts_.find(key); it != fonts_.end()) return it->second;
    TTF_Font* f = open_font(font, kDpi);
    if (f != nullptr) fonts_.emplace(key, f);
    return f;
}

TTF_Font* Text::draw_font(Font font) {
    if (scale_ == 1.0F) return this->font(font);
    const auto key = std::make_pair(font.size, font.weight);
    if (const auto it = draw_fonts_.find(key); it != draw_fonts_.end()) return it->second;
    TTF_Font* f = open_font(font, kDpi * scale_);
    if (f != nullptr) draw_fonts_.emplace(key, f);
    return f;
}

void Text::draw(float x, float y, std::string_view s, Font font, core::Rgba color) {
    TTF_Font* f = draw_font(font);
    if (f == nullptr || s.empty()) return;
    // Раскладка строки — из кэша: те же надписи рисуются каждый кадр.
    std::string key = std::to_string(font.size);
    key += font.weight == Weight::Bold ? 'b' : 'n';
    key += s;
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
}

void Text::out(float x, float y, std::string_view s, Font font, core::Rgba color) {
    draw(x, y, s, font, color);
}

int Text::measure(std::string_view s, Font font) {
    // Как MeasureString с GenericTypographic: сумма продвижений знаков. Шрифт
    // моноширинный (D-22): у всех знаков, и у кириллицы, продвижение одно —
    // дробное, как в раскладке строки SDL3_ttf.
    std::size_t chars = 0;
    for (const char ch : s)
        if ((static_cast<unsigned char>(ch) & 0xC0U) != 0x80U) ++chars;
    return static_cast<int>(std::lround(static_cast<double>(chars) * advance(font)));
}

double Text::advance(Font font) {
    const auto key = std::make_pair(font.size, font.weight);
    if (const auto it = advances_.find(key); it != advances_.end()) return it->second;
    TTF_Font* f = this->font(font);
    double adv = 0;
    if (f != nullptr) {
        // Разность длинных строк: выступ последнего знака сокращается.
        const std::string short_run(64, '0');
        const std::string long_run(128, '0');
        int w1 = 0;
        int w2 = 0;
        int h = 0;
        if (TTF_GetStringSize(f, short_run.data(), short_run.size(), &w1, &h) &&
            TTF_GetStringSize(f, long_run.data(), long_run.size(), &w2, &h))
            adv = static_cast<double>(w2 - w1) / 64.0;
    }
    advances_.emplace(key, adv);
    return adv;
}

int Text::width(std::string_view s, Font font) {
    return measure(s, font) + 1;
}

std::vector<std::string> Text::wrap(std::string_view s, Font font, int max_width) {
    std::vector<std::string> lines;
    const auto trimmed = [](std::string line) {
        while (!line.empty() && line.back() == ' ')
            line.pop_back();
        return line;
    };
    const auto fits = [&](std::string_view line) { return measure(line, font) <= max_width; };
    std::size_t start = 0;
    for (;;) {
        const std::size_t nl = s.find('\n', start);
        const std::string_view paragraph =
            s.substr(start, nl == std::string_view::npos ? std::string_view::npos : nl - start);
        std::string line;
        std::size_t at = 0;
        while (at < paragraph.size()) {
            // Слово с пробелами за ним.
            std::size_t end = paragraph.find(' ', at);
            end = end == std::string_view::npos ? paragraph.size() : end;
            while (end < paragraph.size() && paragraph[end] == ' ')
                ++end;
            const std::string_view word = paragraph.substr(at, end - at);
            at = end;
            if (fits(trimmed(line + std::string(word)))) {
                line += word;
                continue;
            }
            if (!trimmed(line).empty()) lines.push_back(trimmed(line));
            line.clear();
            if (fits(trimmed(std::string(word)))) {
                line = word;
                continue;
            }
            // Слово длиннее строки — по знакам UTF-8.
            for (std::size_t i = 0; i < word.size();) {
                std::size_t next = i + 1;
                while (next < word.size() &&
                       (static_cast<unsigned char>(word[next]) & 0xC0U) == 0x80U)
                    ++next;
                const std::string piece(word.substr(i, next - i));
                if (!line.empty() && !fits(line + piece)) {
                    lines.push_back(trimmed(line));
                    line.clear();
                }
                line += piece;
                i = next;
            }
        }
        lines.push_back(trimmed(line));
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return lines;
}

int Text::height(Font font) {
    TTF_Font* f = this->font(font);
    return f != nullptr ? TTF_GetFontHeight(f) : 0;
}

}  // namespace aim::app
