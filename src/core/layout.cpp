#include "core/layout.h"

#include <algorithm>

#include "core/rounding.h"

namespace aim::core {

double mode_button_entry(int i, Nanoseconds since_enter) {
    return std::clamp((to_ms(since_enter) - i * 80.0) / 400.0, 0.0, 1.0);
}

Rect mode_button(int i, Nanoseconds since_enter) {
    const int offset = round_half_even((1 - mode_button_entry(i, since_enter)) * 20);
    const int y = kButtonsY + i * (kButtonH + kButtonGap) + offset;
    return {kButtonsX, y, kButtonsX + kButtonW, y + kButtonH};
}

Rect mini_button(MiniButton b) {
    const int x = 10 + static_cast<int>(b) * 100;
    return {x, kH - 35, x + 90, kH - 10};
}

Rect settings_panel() {
    const int x = (kW - kSettingsW) / 2;
    const int y = (kH - kSettingsH) / 2;
    return {x, y, x + kSettingsW, y + kSettingsH};
}

int settings_row_y(int row) {
    return settings_panel().y1 + 80 + row * kSettingsRowH;
}

Rect settings_minus(int row) {
    const int x = settings_panel().x1 + kSettingsW - 2 * kSettingsButton - 56;
    const int y = settings_row_y(row);
    return {x, y, x + kSettingsButton, y + kSettingsButton};
}

Rect settings_plus(int row) {
    const int x = settings_panel().x1 + kSettingsW - kSettingsButton - 16;
    const int y = settings_row_y(row);
    return {x, y, x + kSettingsButton, y + kSettingsButton};
}

Rect settings_toggle(int row) {
    const Rect p = settings_panel();
    const int y = settings_row_y(row);
    return {p.x1 + 20, y, p.x2 - 20, y + kSettingsButton};
}

Rect settings_reset() {
    const int x = settings_panel().x1 + (kSettingsW - 180) / 2;
    const int y = settings_row_y(5) + 6;
    return {x, y, x + 180, y + 32};
}

Rect history_panel() {
    const int x = (kW - kHistoryW) / 2;
    const int y = (kH - kHistoryH) / 2;
    return {x, y, x + kHistoryW, y + kHistoryH};
}

Rect history_export() {
    const Rect p = history_panel();
    return {p.x1 + 20, p.y1 + 410, p.x1 + 240, p.y1 + 442};
}

Rect profiles_panel() {
    const int x = (kW - kProfilesW) / 2;
    const int y = (kH - kProfilesH) / 2;
    return {x, y, x + kProfilesW, y + kProfilesH};
}

Rect profile_row(int number) {
    const Rect p = profiles_panel();
    const int y = p.y1 + 60 + (number - 1) * 50;
    return {p.x1 + 20, y, p.x1 + 340, y + 40};
}

}  // namespace aim::core
