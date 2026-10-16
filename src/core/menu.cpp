#include "core/menu.h"

#include <array>
#include <utility>

#include "core/profile_file.h"

namespace aim::core {

Menu::Menu(Profile& profile, int profile_number, bool show_tutorial)
    : profile_(profile), profile_number_(profile_number), tutorial_(show_tutorial) {}

void Menu::enter(Nanoseconds now) {
    entered_at_ = now;
}

void Menu::hover(Vec2 cursor, Nanoseconds now) {
    if (tutorial_ || panel_ != Panel::None) return;
    for (int i = 0; i < kModeCount; ++i) {
        if (mode_button(i, since_entered(now)).contains(cursor)) {
            selected_ = i;
            return;
        }
    }
}

Nanoseconds Menu::since_entered(Nanoseconds t) const {
    return t > entered_at_ ? t - entered_at_ : 0;
}

void Menu::close_tutorial() {
    tutorial_ = false;
    profile_.tutorial_seen = true;
    requests_.emplace_back(SaveProfile{});
}

std::optional<GameMode> Menu::start_selected() {
    return kAllModes.at(static_cast<std::size_t>(selected_));
}

void Menu::click_settings(Vec2 p) {
    constexpr std::array<SettingRow, 3> kRows{SettingRow::CircleSize, SettingRow::Lifetime,
                                              SettingRow::Lives};
    for (int row = 0; row < 3; ++row) {
        const SettingRow r = kRows.at(static_cast<std::size_t>(row));
        if (settings_minus(row).contains(p)) {
            step_setting(profile_.settings, r, -1);
            requests_.emplace_back(SaveProfile{});
        } else if (settings_plus(row).contains(p)) {
            step_setting(profile_.settings, r, 1);
            requests_.emplace_back(SaveProfile{});
        }
    }
    if (settings_toggle(3).contains(p)) {
        profile_.settings.sound = !profile_.settings.sound;
        requests_.emplace_back(SaveProfile{});
    } else if (settings_toggle(4).contains(p)) {
        profile_.settings.theme = (profile_.settings.theme + 1) % kThemeCount;
        requests_.emplace_back(SaveProfile{});
    } else if (settings_reset().contains(p)) {
        reset_settings(profile_.settings);
        requests_.emplace_back(SaveProfile{});
    } else if (!settings_panel().contains(p)) {
        panel_ = Panel::None;
    }
}

std::optional<GameMode> Menu::click(const ClickInput& click) {
    // Меню читает только ЛКМ, как образец.
    if (click.button != MouseButton::Left) return std::nullopt;
    const Vec2 p = click.position;
    // Туториал — прежде всего: щелчок листает, на последнем шаге закрывает.
    if (tutorial_) {
        if (++tutorial_step_ >= kTutorialSteps) close_tutorial();
        return std::nullopt;
    }
    switch (panel_) {
        case Panel::Help:
        case Panel::Records:
        case Panel::Achievements:
            // Любой щелчок закрывает и дальше не идёт: режим под панелью не
            // выбирается.
            panel_ = Panel::None;
            return std::nullopt;
        case Panel::History:
            // «Экспорт в CSV» — после ответа автора (TODO.md).
            if (!history_panel().contains(p)) panel_ = Panel::None;
            return std::nullopt;
        case Panel::Profiles:
            for (int n = 1; n <= kProfileCount; ++n) {
                if (profile_row(n).contains(p)) {
                    if (n != profile_number_) requests_.emplace_back(SwitchProfile{.number = n});
                    return std::nullopt;
                }
            }
            if (!profiles_panel().contains(p)) panel_ = Panel::None;
            return std::nullopt;
        case Panel::Settings:
            click_settings(p);
            return std::nullopt;
        case Panel::None:
            break;
    }
    constexpr std::array<std::pair<MiniButton, Panel>, 6> kOpens{{
        {MiniButton::Help, Panel::Help},
        {MiniButton::Settings, Panel::Settings},
        {MiniButton::Records, Panel::Records},
        {MiniButton::Achievements, Panel::Achievements},
        {MiniButton::History, Panel::History},
        {MiniButton::Profile, Panel::Profiles},
    }};
    for (const auto& [button, panel] : kOpens) {
        if (mini_button(button).contains(p)) {
            panel_ = panel;
            if (panel == Panel::History) requests_.emplace_back(LoadHistory{});
            return std::nullopt;
        }
    }
    for (int i = 0; i < kModeCount; ++i) {
        if (mode_button(i, since_entered(click.time)).contains(p)) {
            selected_ = i;
            return start_selected();
        }
    }
    if (kPlate.contains(p)) return start_selected();
    // Щелчок по пустому месту — эффекты меню вкл./выкл.; записываются со
    // следующим сохранением, как у образца.
    profile_.settings.menu_effects = !profile_.settings.menu_effects;
    return std::nullopt;
}

void Menu::key(const KeyInput& key) {
    if (key.key != Key::Escape) return;
    panel_ = Panel::None;
    // Туториал по Esc пропускается и помечается просмотренным.
    if (tutorial_) close_tutorial();
}

std::vector<MenuRequest> Menu::take_requests() {
    return std::exchange(requests_, {});
}

}  // namespace aim::core
