#include "core/wrap.h"

#include <cstddef>

namespace aim::core {

namespace {

// Длина знака UTF-8, начатого в s[i].
std::size_t char_end(std::string_view s, std::size_t i) {
    std::size_t next = i + 1;
    while (next < s.size() && (static_cast<unsigned char>(s[next]) & 0xC0U) == 0x80U)
        ++next;
    return next;
}

std::string trimmed(std::string line) {
    while (!line.empty() && line.back() == ' ')
        line.pop_back();
    return line;
}

}  // namespace

std::vector<std::string> wrap_lines(std::string_view s, int max_width, const Measure& measure) {
    std::vector<std::string> lines;
    const auto fits = [&](std::string_view line) { return measure(line) <= max_width; };
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
                const std::size_t next = char_end(word, i);
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

std::string fit_left(std::string_view prefix, std::string_view text, int width,
                     const Measure& measure) {
    std::string tail(text);
    const auto line = [&](bool cut) { return std::string(prefix) + (cut ? "…" : "") + tail; };
    bool cut = false;
    while (!tail.empty() && measure(line(cut)) > width) {
        tail.erase(0, char_end(tail, 0));
        cut = true;
    }
    return line(cut);
}

}  // namespace aim::core
