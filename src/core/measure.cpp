#include "core/measure.h"

#include <algorithm>
#include <cmath>
#include <format>

#include "core/rules.h"

namespace aim::core {

const std::array<Preset, kPresetCount> kPresets{{
    {"Реакция на цвет",
     "Окно меняет цвет через случайную паузу — щёлкни как можно быстрее. Щелчок раньше — "
     "фальстарт.",
     {.stimulus = Stimulus::Screen, .ending = Ending::Time},
     1.0},
    {"Классика",
     "Щёлкай ЛКМ по появляющимся кругам. Чем ближе к центру — тем выше точность.",
     {},
     1.0},
    {"Две кнопки",
     "Синие круги — ЛКМ, красные — ПКМ. Проверь обе руки.",
     {.color_buttons = true},
     1.0},
    {"Слежение",
     "Удерживай курсор на движущемся круге, пока он не засчитается.",
     {.action = Action::Hover, .moving = true, .grows = false, .bombs = false},
     2.5},
    {"Флик",
     "Круг сразу полного размера и живёт недолго. Тренируй резкий перенос прицела.",
     {.grows = false},
     0.7},
}};

MeasureOptions normalized(const MeasureOptions& options) {
    MeasureOptions o = options;
    const MeasureOptions defaults;
    o.target_size = std::clamp(o.target_size, kMinTargetSize, kMaxTargetSize);
    o.lifetime_ms = std::clamp(o.lifetime_ms, kMinLifetimeMs, kMaxLifetimeMs);
    o.lives = std::clamp(o.lives, kMinLives, kMaxLives);
    o.duration_s = std::clamp(o.duration_s, kMinDurationS, kMaxDurationS);
    if (o.stimulus == Stimulus::Screen) {
        o.action = defaults.action;
        o.color_buttons = defaults.color_buttons;
        o.moving = defaults.moving;
        o.grows = defaults.grows;
        o.bombs = defaults.bombs;
        o.target_size = defaults.target_size;
        o.lifetime_ms = defaults.lifetime_ms;
    } else if (o.action == Action::Hover) {
        o.color_buttons = false;
        o.bombs = false;
    }
    if (o.ending == Ending::Lives) {
        o.duration_s = defaults.duration_s;
    } else {
        o.lives = defaults.lives;
    }
    return o;
}

std::string options_key(const MeasureOptions& options) {
    const MeasureOptions o = normalized(options);
    std::string key = std::string(stimulus_word(o.stimulus));
    if (o.stimulus == Stimulus::Target) {
        key += std::format(";{};{};{};{};{};size={};life={}", action_word(o.action),
                           o.color_buttons ? "color" : "left", o.moving ? "moving" : "still",
                           o.grows ? "grow" : "full", o.bombs ? "bombs" : "nobombs", o.target_size,
                           o.lifetime_ms);
    }
    if (o.ending == Ending::Lives) {
        key += std::format(";lives={}", o.lives);
    } else {
        key += std::format(";time={}", o.duration_s);
    }
    return key;
}

std::vector<std::string> describe(const MeasureOptions& options) {
    const MeasureOptions o = normalized(options);
    std::vector<std::string> lines;
    lines.emplace_back(o.stimulus == Stimulus::Screen ? "Стимул: смена цвета окна"
                                                      : "Стимул: цель");
    if (o.stimulus == Stimulus::Target) {
        lines.emplace_back(o.action == Action::Hover ? "Действие: наведение" : "Действие: щелчок");
        if (o.action == Action::Click)
            lines.emplace_back(o.color_buttons ? "Кнопки: по цвету цели" : "Кнопки: ЛКМ");
        lines.emplace_back(o.moving ? "Движение: да" : "Движение: нет");
        lines.emplace_back(o.grows ? "Появление: рост и сжатие" : "Появление: сразу полная");
        if (o.action == Action::Click) lines.emplace_back(o.bombs ? "Бомбы: да" : "Бомбы: нет");
        lines.push_back(std::format("Размер цели: {} px", o.target_size));
        lines.push_back(std::format("Жизнь цели: {} мс", o.lifetime_ms));
    }
    lines.push_back(o.ending == Ending::Lives ? std::format("Окончание: {} жизней", o.lives)
                                              : std::format("Окончание: {} с", o.duration_s));
    return lines;
}

std::string describe_short(const MeasureOptions& options) {
    const MeasureOptions o = normalized(options);
    std::string text;
    if (o.stimulus == Stimulus::Screen) {
        text = "цвет окна";
    } else {
        text = o.action == Action::Hover ? "наведение" : "щелчок";
        if (o.action == Action::Click && o.color_buttons) text += ", по цвету";
        if (o.moving) text += ", движение";
        text += o.grows ? ", рост" : ", сразу";
        if (o.action == Action::Click && o.bombs) text += ", бомбы";
        text += std::format(", {} px, {} мс", o.target_size, o.lifetime_ms);
    }
    text += o.ending == Ending::Lives ? std::format(", {} жизней", o.lives)
                                      : std::format(", {} с", o.duration_s);
    return text;
}

MeasureOptions preset_options(int index, const PresetSettings& settings) {
    const Preset& preset =
        kPresets.at(static_cast<std::size_t>(std::clamp(index, 0, kPresetCount - 1)));
    MeasureOptions o = preset.options;
    if (o.stimulus == Stimulus::Target) {
        o.target_size = settings.target_size;
        o.lifetime_ms =
            static_cast<int>(std::lround(settings.lifetime_ms * preset.lifetime_factor));
        o.ending = settings.ending;
    }
    o.lives = settings.lives;
    return normalized(o);
}

std::optional<std::string_view> preset_name(const MeasureOptions& options) {
    const MeasureOptions o = normalized(options);
    for (const Preset& preset : kPresets) {
        const MeasureOptions p = normalized(preset.options);
        if (o.stimulus == p.stimulus && o.action == p.action &&
            o.color_buttons == p.color_buttons && o.moving == p.moving && o.grows == p.grows &&
            o.bombs == p.bombs)
            return preset.name;
    }
    return std::nullopt;
}

std::string_view combo_name(const MeasureOptions& options) {
    return preset_name(options).value_or("Свой набор");
}

TargetRules target_rules(const MeasureOptions& options) {
    const MeasureOptions o = normalized(options);
    TargetRules rules;
    rules.lifetime = ms(static_cast<std::uint64_t>(o.lifetime_ms));
    rules.grows = o.grows;
    rules.two_buttons = o.action == Action::Click && o.color_buttons;
    rules.moves = o.moving;
    rules.hover = o.action == Action::Hover;
    rules.bombs = o.action == Action::Click && o.bombs;
    rules.top_reserve = rules.two_buttons ? kDualTopReserve : 0;
    return rules;
}

std::string_view stimulus_word(Stimulus s) {
    return s == Stimulus::Screen ? "screen" : "target";
}

std::string_view action_word(Action a) {
    return a == Action::Hover ? "hover" : "click";
}

std::string_view ending_word(Ending e) {
    return e == Ending::Time ? "time" : "lives";
}

std::optional<Stimulus> parse_stimulus(std::string_view word) {
    if (word == "target") return Stimulus::Target;
    if (word == "screen") return Stimulus::Screen;
    return std::nullopt;
}

std::optional<Action> parse_action(std::string_view word) {
    if (word == "click") return Action::Click;
    if (word == "hover") return Action::Hover;
    return std::nullopt;
}

std::optional<Ending> parse_ending(std::string_view word) {
    if (word == "lives") return Ending::Lives;
    if (word == "time") return Ending::Time;
    return std::nullopt;
}

}  // namespace aim::core
