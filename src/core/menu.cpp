#include "core/menu.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include "core/help.h"
#include "core/profile_file.h"

namespace aim::core {

Menu::Menu(Profile& profile, int profile_number, bool show_tutorial)
    : profile_(profile),
      profile_number_(profile_number),
      tutorial_(show_tutorial),
      selected_(std::clamp(profile.preset, 0, kPresetCount - 1)) {}

void Menu::enter(Nanoseconds now) {
    entered_at_ = now;
}

void Menu::hover(Vec2 cursor, Nanoseconds now) {
    if (tutorial_ || panel_ != Panel::None) return;
    for (int i = 0; i < kPresetCount; ++i) {
        if (preset_button(i, since_entered(now)).contains(cursor)) {
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

MeasureOptions Menu::start_selected() {
    if (profile_.preset != selected_) {
        profile_.preset = selected_;
        requests_.emplace_back(SaveProfile{});
    }
    return selected_options(profile_);
}

void Menu::click_settings(Vec2 p) {
    constexpr std::array<std::pair<int, SettingRow>, 4> kSteppers{{
        {kSettingsSizeRow, SettingRow::TargetSize},
        {kSettingsLifetimeRow, SettingRow::Lifetime},
        {kSettingsLivesRow, SettingRow::Lives},
        {kSettingsAverageRow, SettingRow::AverageRuns},
    }};
    for (const auto& [row, setting] : kSteppers) {
        if (settings_minus(row).contains(p)) {
            step_setting(profile_.settings, setting, -1);
            requests_.emplace_back(SaveProfile{});
            return;
        }
        if (settings_plus(row).contains(p)) {
            step_setting(profile_.settings, setting, 1);
            requests_.emplace_back(SaveProfile{});
            return;
        }
    }
    Settings& s = profile_.settings;
    if (settings_toggle(kSettingsEndingRow).contains(p)) {
        s.preset.ending = s.preset.ending == Ending::Lives ? Ending::Time : Ending::Lives;
        requests_.emplace_back(SaveProfile{});
    } else if (settings_toggle(kSettingsSoundRow).contains(p)) {
        s.sound = !s.sound;
        requests_.emplace_back(SaveProfile{});
    } else if (settings_toggle(kSettingsThemeRow).contains(p)) {
        s.theme = (s.theme + 1) % kThemeCount;
        requests_.emplace_back(SaveProfile{});
    } else if (settings_toggle(kSettingsOpenFolderRow).contains(p)) {
        requests_.emplace_back(OpenDataFolder{});
    } else if (settings_toggle(kSettingsChooseFolderRow).contains(p)) {
        requests_.emplace_back(ChooseDataFolder{});
    } else if (settings_reset().contains(p)) {
        reset_settings(profile_.settings);
        requests_.emplace_back(SaveProfile{});
    } else if (!settings_panel().contains(p)) {
        panel_ = Panel::None;
    }
}

std::optional<MeasureOptions> Menu::click(const ClickInput& click) {
    // Меню читает только ЛКМ, как образец.
    if (click.button != MouseButton::Left) return std::nullopt;
    const Vec2 p = click.position;
    // Туториал — прежде всего: щелчок листает, на последнем шаге закрывает.
    if (tutorial_) {
        if (++tutorial_step_ >= kTutorialSteps) close_tutorial();
        return std::nullopt;
    }
    switch (panel_) {
        case Panel::Help: click_help(p); return std::nullopt;
        case Panel::Records:
        case Panel::Achievements:
            // Любой щелчок закрывает и дальше не идёт: набор под панелью не
            // выбирается.
            panel_ = Panel::None;
            return std::nullopt;
        case Panel::History:
            if (history_export().contains(p))
                requests_.emplace_back(ExportHistory{});
            else if (!history_panel().contains(p))
                panel_ = Panel::None;
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
        case Panel::Settings: click_settings(p); return std::nullopt;
        case Panel::None: break;
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
            if (panel == Panel::Help) help_scroll_ = 0;
            return std::nullopt;
        }
    }
    for (int i = 0; i < kPresetCount; ++i) {
        if (preset_button(i, since_entered(click.time)).contains(p)) {
            selected_ = i;
            return start_selected();
        }
    }
    if (kPlate.contains(p)) return start_selected();
    // Щелчок по пустому месту — эффекты меню вкл./выкл.; записываются со
    // следующим сохранением.
    profile_.settings.menu_effects = !profile_.settings.menu_effects;
    return std::nullopt;
}

void Menu::scroll_help(int lines) {
    help_scroll_ = std::clamp(help_scroll_ + lines, 0, kHelpMaxScroll);
}

void Menu::scroll_help_to(double y) {
    const Rect track = help_track();
    const double share = (y - track.y1) / (track.y2 - track.y1);
    help_scroll_ =
        std::clamp(static_cast<int>(std::lround(share * kHelpMaxScroll)), 0, kHelpMaxScroll);
}

void Menu::click_help(Vec2 p) {
    if (help_track().contains(p)) {
        // Полоса прокрутки: место щелчка — место в справке; дальше ползунок
        // тянется за курсором, пока ЛКМ зажата.
        scroll_help_to(p.y);
        dragging_help_ = true;
    } else if (!help_panel().contains(p)) {
        panel_ = Panel::None;
    }
}

void Menu::drag(Vec2 cursor, bool left_held) {
    if (!left_held || panel_ != Panel::Help) {
        dragging_help_ = false;
        return;
    }
    if (dragging_help_) scroll_help_to(cursor.y);
}

void Menu::wheel(const WheelInput& wheel) {
    if (panel_ != Panel::Help || tutorial_) return;
    // Щелчок колёсика — три строки; от себя — вверх.
    constexpr int kLinesPerNotch = 3;
    scroll_help(-static_cast<int>(std::lround(wheel.y * kLinesPerNotch)));
}

void Menu::key(const KeyInput& key) {
    if (panel_ == Panel::Help && !tutorial_) {
        switch (key.key) {
            case Key::Up: scroll_help(-1); return;
            case Key::Down: scroll_help(1); return;
            case Key::PageUp: scroll_help(-kHelpVisibleLines); return;
            case Key::PageDown: scroll_help(kHelpVisibleLines); return;
            case Key::Escape:
            case Key::Space:
            case Key::R:
            case Key::M: break;
        }
    }
    if (key.key != Key::Escape) return;
    panel_ = Panel::None;
    // Туториал по Esc пропускается и помечается просмотренным.
    if (tutorial_) close_tutorial();
}

std::vector<MenuRequest> Menu::take_requests() {
    return std::exchange(requests_, {});
}

}  // namespace aim::core
