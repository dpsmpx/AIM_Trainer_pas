#pragma once

// Режимы игры и правила цели в каждом (docs/ARCHITECTURE.md, §6). Tracking и
// Flick — с параметрами C++-версии (D-21), а не образца (AUD-04, AUD-10).

#include <array>
#include <cstdint>
#include <string_view>

#include "core/time.h"

namespace aim::core {

/// Режим. Порядок — как у TGameMode образца: это номер режима в таблице
/// лидеров и в файлах профиля (docs/ARCHITECTURE.md, §8).
enum class GameMode : std::uint8_t { Classic, Dual, Tracking, Flick };

inline constexpr int kModeCount = 4;
inline constexpr std::array<GameMode, kModeCount> kAllModes{GameMode::Classic, GameMode::Dual,
                                                            GameMode::Tracking, GameMode::Flick};

/// Номер режима — индекс таблиц (Ord(TGameMode) у образца).
constexpr int mode_index(GameMode mode) {
    return static_cast<int>(mode);
}

/// Как живёт и засчитывается цель.
struct ModeRules {
    Nanoseconds lifetime = 0;  ///< жизнь цели
    bool grows = false;  ///< растёт с нуля до пика к середине жизни и сжимается;
                         ///< иначе — сразу полный размер
    bool two_buttons = false;  ///< синие — ЛКМ, красные — ПКМ (Dual); иначе только ЛКМ
    bool moves = false;  ///< движется и отскакивает от краёв (Tracking)
    bool hover = false;  ///< засчитывается наводкой, щелчки не читаются (Tracking)
    bool bombs = false;   ///< бывают бомбы
    int top_reserve = 0;  ///< место сверху под подсказку, px
};

/// Правила режима при настройке «Время жизни», мс (D-21): Classic и Dual —
/// как есть, Flick — ×0,7, Tracking — ×2,5.
ModeRules mode_rules(GameMode mode, int lifetime_setting_ms);

/// Имя режима в файле истории: Classic, Dual, Tracking, Flick
/// (docs/ARCHITECTURE.md, §8.2).
std::string_view mode_file_name(GameMode mode);

}  // namespace aim::core
