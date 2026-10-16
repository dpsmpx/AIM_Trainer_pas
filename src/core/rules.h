#pragma once

// Числа правил игры — одно место для логики, отрисовки и тестов (AUD-22).
// Значения — как у образца (archive/pascal/, docs/ARCHITECTURE.md, §5–7),
// кроме названных решений.

#include "core/time.h"

namespace aim::core {

/// Полоса HUD внизу окна: цели в неё не заходят (Spawn и Update образца).
inline constexpr int kHudReserve = 60;
/// Место сверху под подсказку «LMB/RMB» в Dual.
inline constexpr int kDualTopReserve = 50;

/// Масштаб цели: пик радиуса — «Размер круга» × масштаб; меньше — больше очков.
inline constexpr double kMinSizeFactor = 0.65;
inline constexpr double kMaxSizeFactor = 1.35;
/// Новая цель — не ближе двух своих радиусов к прежней; столько попыток.
inline constexpr int kSpawnAttempts = 100;

/// Доля бомб среди целей — кроме первой цели забега и Tracking.
inline constexpr double kBombChance = 0.15;

/// Очки за попадание: round(13 / масштаб × множитель серии).
inline constexpr double kBasePoints = 13.0;
/// Множитель серии: ×1,5 с 5 попаданий подряд, ×2 — с 10.
inline constexpr int kComboTier1 = 5;
inline constexpr int kComboTier2 = 10;
inline constexpr double kComboMult1 = 1.5;
inline constexpr double kComboMult2 = 2.0;

/// Точность попадания, с которой оно «в яблочко»: вспышка и достижение 1.
inline constexpr double kPreciseHit = 0.95;

/// Tracking (D-21): наводка до засчитывания; скорость цели, px/с.
inline constexpr Nanoseconds kTrackingHold = ms(900);
inline constexpr int kTrackingMinSpeed = 60;
inline constexpr int kTrackingMaxSpeed = 119;

/// Тепловая карта промахов на экране Game Over: столбцы и строки.
inline constexpr int kHeatmapCols = 20;
inline constexpr int kHeatmapRows = 15;

/// Реакций в истории: в забеге и в профиле (график меню).
inline constexpr int kReactionHistory = 50;

}  // namespace aim::core
