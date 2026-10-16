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

std::optional<int> preset_index(const MeasureOptions& options) {
    const MeasureOptions o = normalized(options);
    for (int i = 0; i < kPresetCount; ++i) {
        const MeasureOptions p = normalized(kPresets.at(static_cast<std::size_t>(i)).options);
        if (o.stimulus == p.stimulus && o.action == p.action &&
            o.color_buttons == p.color_buttons && o.moving == p.moving && o.grows == p.grows &&
            o.bombs == p.bombs)
            return i;
    }
    return std::nullopt;
}

std::optional<std::string_view> preset_name(const MeasureOptions& options) {
    if (const std::optional<int> i = preset_index(options))
        return kPresets.at(static_cast<std::size_t>(*i)).name;
    return std::nullopt;
}

int preset_like(const MeasureOptions& options) {
    if (const std::optional<int> i = preset_index(options)) return *i;
    const MeasureOptions o = normalized(options);
    if (o.stimulus == Stimulus::Screen) return 0;
    if (o.action == Action::Hover) return 3;
    if (o.color_buttons) return 2;
    if (!o.grows) return 4;
    return 1;
}

bool option_applies(const MeasureOptions& o, OptionRow row) {
    const bool target = o.stimulus == Stimulus::Target;
    const bool click = target && o.action == Action::Click;
    switch (row) {
        case OptionRow::Stimulus:
        case OptionRow::Ending:
        case OptionRow::Amount: return true;
        case OptionRow::Action:
        case OptionRow::Moving:
        case OptionRow::Appear:
        case OptionRow::Size:
        case OptionRow::Lifetime: return target;
        case OptionRow::Buttons:
        case OptionRow::Bombs: return click;
    }
    return false;
}

bool option_toggles(OptionRow row) {
    return row != OptionRow::Size && row != OptionRow::Lifetime && row != OptionRow::Amount;
}

void toggle_option(MeasureOptions& o, OptionRow row) {
    switch (row) {
        case OptionRow::Stimulus:
            o.stimulus = o.stimulus == Stimulus::Target ? Stimulus::Screen : Stimulus::Target;
            break;
        case OptionRow::Action:
            o.action = o.action == Action::Click ? Action::Hover : Action::Click;
            break;
        case OptionRow::Buttons: o.color_buttons = !o.color_buttons; break;
        case OptionRow::Moving: o.moving = !o.moving; break;
        case OptionRow::Appear: o.grows = !o.grows; break;
        case OptionRow::Bombs: o.bombs = !o.bombs; break;
        case OptionRow::Ending:
            o.ending = o.ending == Ending::Lives ? Ending::Time : Ending::Lives;
            break;
        case OptionRow::Size:
        case OptionRow::Lifetime:
        case OptionRow::Amount: break;
    }
}

void step_option(MeasureOptions& o, OptionRow row, int delta) {
    switch (row) {
        case OptionRow::Size:
            o.target_size =
                std::clamp(o.target_size + delta * kTargetSizeStep, kMinTargetSize, kMaxTargetSize);
            break;
        case OptionRow::Lifetime:
            o.lifetime_ms =
                std::clamp(o.lifetime_ms + delta * kLifetimeStepMs, kMinLifetimeMs, kMaxLifetimeMs);
            break;
        case OptionRow::Amount:
            if (o.ending == Ending::Lives) {
                o.lives = std::clamp(o.lives + delta, kMinLives, kMaxLives);
            } else {
                o.duration_s =
                    std::clamp(o.duration_s + delta * kDurationStepS, kMinDurationS, kMaxDurationS);
            }
            break;
        case OptionRow::Stimulus:
        case OptionRow::Action:
        case OptionRow::Buttons:
        case OptionRow::Moving:
        case OptionRow::Appear:
        case OptionRow::Bombs:
        case OptionRow::Ending: break;
    }
}

std::string_view option_label(const MeasureOptions& o, OptionRow row) {
    switch (row) {
        case OptionRow::Stimulus: return "Стимул";
        case OptionRow::Action: return "Действие";
        case OptionRow::Buttons: return "Кнопки";
        case OptionRow::Moving: return "Движение";
        case OptionRow::Appear: return "Появление";
        case OptionRow::Bombs: return "Бомбы";
        case OptionRow::Size: return "Размер цели";
        case OptionRow::Lifetime: return "Жизнь цели";
        case OptionRow::Ending: return "Окончание";
        case OptionRow::Amount: return o.ending == Ending::Lives ? "Жизни" : "Время замера";
    }
    return {};
}

std::string option_value(const MeasureOptions& o, OptionRow row) {
    switch (row) {
        case OptionRow::Stimulus:
            return o.stimulus == Stimulus::Screen ? "смена цвета окна" : "цель";
        case OptionRow::Action: return o.action == Action::Hover ? "наведение" : "щелчок";
        case OptionRow::Buttons: return o.color_buttons ? "по цвету цели" : "ЛКМ";
        case OptionRow::Moving: return o.moving ? "да" : "нет";
        case OptionRow::Appear: return o.grows ? "рост и сжатие" : "сразу полная";
        case OptionRow::Bombs: return o.bombs ? "да" : "нет";
        case OptionRow::Size: return std::format("{} px", o.target_size);
        case OptionRow::Lifetime: return std::format("{} мс", o.lifetime_ms);
        case OptionRow::Ending: return o.ending == Ending::Lives ? "по жизням" : "по времени";
        case OptionRow::Amount:
            return o.ending == Ending::Lives ? std::format("{}", o.lives)
                                             : std::format("{} с", o.duration_s);
    }
    return {};
}

int reset_out_of_range(MeasureOptions& o) {
    const MeasureOptions defaults;
    int reset = 0;
    const auto check = [&](int& value, int lo, int hi, int fallback) {
        if (value >= lo && value <= hi) return;
        value = fallback;
        ++reset;
    };
    check(o.target_size, kMinTargetSize, kMaxTargetSize, defaults.target_size);
    check(o.lifetime_ms, kMinLifetimeMs, kMaxLifetimeMs, defaults.lifetime_ms);
    check(o.lives, kMinLives, kMaxLives, defaults.lives);
    check(o.duration_s, kMinDurationS, kMaxDurationS, defaults.duration_s);
    return reset;
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
