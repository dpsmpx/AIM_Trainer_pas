#pragma once

// Звуки образца — тоны Console.Beep (archive/pascal/AIM_Trainer.pas:
// PlayHitSound, PlayMissSound, PlayGameOverSound; archive/pascal/ARCHITECTURE.md, §10).
// Здесь — только отсчёты тона; играет их platform/sound.h, не задерживая
// кадр (AUD-06).

#include <vector>

namespace aim::core {

struct Beep {
    int frequency_hz = 0;
    int duration_ms = 0;
};

inline constexpr Beep kHitBeep{1000, 30};
inline constexpr Beep kMissBeep{300, 40};
inline constexpr Beep kGameOverBeep{200, 220};

/// Тон: синус частоты beep.frequency_hz, beep.duration_ms миллисекунд при
/// sample_rate отсчётов в секунду, моно; громкость volume (0…1). Края —
/// плавные по 2 мс, без щелчка.
std::vector<float> tone(Beep beep, int sample_rate, float volume);

}  // namespace aim::core
