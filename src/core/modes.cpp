#include "core/modes.h"

#include <cstdint>

#include "core/rules.h"

namespace aim::core {

ModeRules mode_rules(GameMode mode, int lifetime_setting_ms) {
    const auto setting =
        static_cast<std::uint64_t>(lifetime_setting_ms > 0 ? lifetime_setting_ms : 0);
    ModeRules rules;
    switch (mode) {
        case GameMode::Classic:
            rules.lifetime = ms(setting);
            rules.grows = true;
            rules.bombs = true;
            break;
        case GameMode::Dual:
            rules.lifetime = ms(setting);
            rules.grows = true;
            rules.two_buttons = true;
            rules.bombs = true;
            rules.top_reserve = kDualTopReserve;
            break;
        case GameMode::Tracking:
            // D-21: жизнь ×2,5 (2,5 с при 1000 мс), размер постоянный, бомб нет.
            rules.lifetime = ms(setting) * 5 / 2;
            rules.moves = true;
            rules.hover = true;
            break;
        case GameMode::Flick:
            // D-21: сразу полный размер, жизнь ×0,7 (700 мс при 1000 мс);
            // бомбы, очки и правило попадания — как в Classic.
            rules.lifetime = ms(setting) * 7 / 10;
            rules.bombs = true;
            break;
    }
    return rules;
}

std::string_view mode_file_name(GameMode mode) {
    switch (mode) {
        case GameMode::Classic: return "Classic";
        case GameMode::Dual: return "Dual";
        case GameMode::Tracking: return "Tracking";
        case GameMode::Flick: return "Flick";
    }
    return "Classic";
}

}  // namespace aim::core
