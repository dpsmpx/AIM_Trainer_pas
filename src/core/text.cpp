#include "core/text.h"

#include <charconv>
#include <cmath>
#include <system_error>

namespace aim::core {

namespace {

constexpr std::string_view kBom = "\xEF\xBB\xBF";

std::string_view trim(std::string_view s) {
    const auto space = [](char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
    };
    while (!s.empty() && space(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && space(s.back()))
        s.remove_suffix(1);
    return s;
}

}  // namespace

std::vector<std::string_view> split_lines(std::string_view text) {
    if (text.starts_with(kBom)) text.remove_prefix(kBom.size());
    std::vector<std::string_view> lines;
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        std::string_view line = text.substr(0, end);
        if (line.ends_with('\r')) line.remove_suffix(1);
        lines.push_back(line);
        if (end == std::string_view::npos) break;
        text.remove_prefix(end + 1);
    }
    return lines;
}

std::vector<std::string_view> split(std::string_view line, char separator) {
    std::vector<std::string_view> fields;
    for (;;) {
        const std::size_t at = line.find(separator);
        fields.push_back(line.substr(0, at));
        if (at == std::string_view::npos) break;
        line.remove_prefix(at + 1);
    }
    return fields;
}

std::optional<int> parse_int(std::string_view text) {
    text = trim(text);
    if (text.starts_with('+')) text.remove_prefix(1);
    if (text.empty()) return std::nullopt;
    int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) return std::nullopt;
    return value;
}

std::optional<double> parse_real(std::string_view text) {
    text = trim(text);
    if (text.starts_with('+')) text.remove_prefix(1);
    if (text.empty()) return std::nullopt;
    double value = 0.0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value))
        return std::nullopt;
    return value;
}

}  // namespace aim::core
