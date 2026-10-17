#include "core/layout.h"

#include <algorithm>

#include "core/help.h"
#include "core/measure.h"
#include "core/rounding.h"

namespace aim::core {

double preset_button_entry(int i, Nanoseconds since_enter) {
    return std::clamp((to_ms(since_enter) - i * 80.0) / 400.0, 0.0, 1.0);
}

Rect preset_button(int i, Nanoseconds since_enter) {
    const int offset = round_half_even((1 - preset_button_entry(i, since_enter)) * 20);
    const int y = kButtonsY + i * (kButtonH + kButtonGap) + offset;
    return {kButtonsX, y, kButtonsX + kButtonW, y + kButtonH};
}

Rect mini_button(MiniButton b) {
    const int x = 10 + static_cast<int>(b) * 100;
    return {x, kH - 35, x + 90, kH - 10};
}

Rect result_button(ResultButton b, bool with_replay) {
    constexpr int kColumnCenter = (kResultMeasuresX1 + kResultMeasuresX2) / 2;
    const int count = with_replay ? 3 : 2;
    const int total = count * kResultButtonW + (count - 1) * kResultButtonGap;
    const int x =
        kColumnCenter - total / 2 + static_cast<int>(b) * (kResultButtonW + kResultButtonGap);
    return {x, kResultButtonsY, x + kResultButtonW, kResultButtonsY + kResultButtonH};
}

Rect settings_panel() {
    const int x = (kW - kSettingsW) / 2;
    const int y = (kH - kSettingsH) / 2;
    return {x, y, x + kSettingsW, y + kSettingsH};
}

int settings_row_y(int row) {
    return settings_panel().y1 + 58 + row * kSettingsRowH;
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
    const int y = settings_row_y(kSettingsResetRow) + 4;
    return {x, y, x + 180, y + 32};
}

Rect help_panel() {
    const int x = (kW - kHelpW) / 2;
    const int y = (kH - kHelpH) / 2;
    return {x, y, x + kHelpW, y + kHelpH};
}

Rect help_track() {
    const Rect p = help_panel();
    const int top = p.y1 + kHelpTop;
    return {p.x2 - 22, top, p.x2 - 12, top + kHelpVisibleLines * kHelpLineH};
}

Rect constructor_panel() {
    const int x = (kW - kConstructorW) / 2;
    const int y = (kH - kConstructorH) / 2;
    return {x, y, x + kConstructorW, y + kConstructorH};
}

Rect constructor_preset(int i) {
    const Rect p = constructor_panel();
    constexpr int kGap = 6;
    constexpr int kPerRow = 3;
    constexpr int kChipW = (kConstructorW - 32 - (kPerRow - 1) * kGap) / kPerRow;
    const int x = p.x1 + 16 + (i % kPerRow) * (kChipW + kGap);
    const int y = p.y1 + 48 + (i / kPerRow) * 34;
    return {x, y, x + kChipW, y + 28};
}

Rect constructor_row(int row) {
    const Rect p = constructor_panel();
    const int y = p.y1 + 124 + row * kConstructorRowH;
    return {p.x1 + 16, y, p.x2 - 16, y + kConstructorRowH - 4};
}

Rect constructor_minus(int row) {
    const Rect r = constructor_row(row);
    return {r.x2 - 150, r.y1, r.x2 - 122, r.y2};
}

Rect constructor_plus(int row) {
    const Rect r = constructor_row(row);
    return {r.x2 - 28, r.y1, r.x2, r.y2};
}

Rect constructor_start() {
    const Rect p = constructor_panel();
    const int x = (p.x1 + p.x2) / 2 - 90;
    const int y = p.y1 + 124 + kOptionRows * kConstructorRowH + 6;
    return {x, y, x + 180, y + 34};
}

Rect records_panel() {
    const int x = (kW - kRecordsW) / 2;
    const int y = (kH - kRecordsH) / 2;
    return {x, y, x + kRecordsW, y + kRecordsH};
}

Rect records_row(int i) {
    const Rect p = records_panel();
    const int y = p.y1 + 50 + i * 46;
    return {p.x1 + 12, y, p.x2 - 12, y + 42};
}

Rect history_panel() {
    const int x = (kW - kHistoryW) / 2;
    const int y = (kH - kHistoryH) / 2;
    return {x, y, x + kHistoryW, y + kHistoryH};
}

Rect history_export() {
    const Rect p = history_panel();
    return {p.x1 + 20, p.y1 + 398, p.x1 + 240, p.y1 + 428};
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
