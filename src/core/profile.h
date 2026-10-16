#pragma once

// Профиль игрока в памяти: настройки, рекорды, таблица лидеров, достижения,
// история реакций (archive/pascal/AIM_Trainer.pas: TStats, TSettings,
// StepSetting, TryAddToLeaderboard, UnlockAchievement,
// FinishGameAndSaveStats; docs/ARCHITECTURE.md, §8.1, §10). Чтение и запись
// файлов — шаг «Файлы профиля». Отличия — нарочные:
//
//   * рекорды точности, точности попадания и реакции — от 20 выстрелов (D-13);
//   * история реакций — по порядку, последние 50 (AUD-14).

#include <array>
#include <cstdint>
#include <deque>
#include <string_view>
#include <vector>

#include "core/game.h"
#include "core/modes.h"

namespace aim::core {

// ---- Настройки ----

inline constexpr int kMinCircleSize = 20;
inline constexpr int kMaxCircleSize = 60;
inline constexpr int kCircleSizeStep = 2;
inline constexpr int kMinLifetimeMs = 500;
inline constexpr int kMaxLifetimeMs = 2000;
inline constexpr int kLifetimeStepMs = 100;
inline constexpr int kMinLives = 5;
inline constexpr int kMaxLives = 30;
inline constexpr int kThemeCount = 3;  ///< 0 — Тёмная, 1 — Неон, 2 — Минимализм

struct Settings {
    int circle_size = 40;
    int lifetime_ms = 1000;
    int lives = 15;
    bool sound = true;
    bool menu_effects = true;  ///< частицы меню, «Effects: ON/OFF»
    int theme = 0;
};

/// Ряд панели настроек со ступенями «−»/«+».
enum class SettingRow : std::uint8_t { CircleSize, Lifetime, Lives };

/// Шаг ряда на delta ступеней с границами (StepSetting).
void step_setting(Settings& settings, SettingRow row, int delta);
/// «Сбросить по умолчанию»: размер, время жизни, жизни и звук; тему и эффекты
/// не трогает — как SetDefaultSettings образца (прогон 16 октября 2026).
void reset_settings(Settings& settings);
/// Вернуть значения в границы — для настроек, прочитанных из файла (AUD-07).
void clamp_settings(Settings& settings);
/// Настройки забега из настроек профиля.
GameSettings game_settings(const Settings& settings);

// ---- Таблица лидеров ----

inline constexpr int kLeaderboardSize = 10;

struct LeaderboardEntry {
    int score = 0;
    double accuracy = 0.0;  ///< %
};

/// По режиму — до 10 мест, по убыванию очков.
using Leaderboard = std::array<std::vector<LeaderboardEntry>, kModeCount>;

/// Записать результат (TryAddToLeaderboard): встаёт перед первым меньшим, при
/// равных очках — после прежних. Возвращает место 0…9 или −1, если не вошёл.
int add_to_leaderboard(Leaderboard& board, GameMode mode, int score, double accuracy);

// ---- Достижения ----

inline constexpr int kAchievementCount = 8;

struct AchievementInfo {
    std::string_view name;
    std::string_view description;
};

/// Названия и условия — как InitAchievements образца.
extern const std::array<AchievementInfo, kAchievementCount> kAchievements;

/// Порог серии и счёта для достижений «Комбо x100» и «Тысяча».
inline constexpr int kComboAchievement = 100;
inline constexpr int kScoreAchievement = 1000;
/// «Молниеносно» — реакция короче, мс.
inline constexpr double kFastReactionMs = 150.0;
/// Порог выстрелов: «Снайпер» и рекорды точности и реакции (D-13).
inline constexpr int kRecordMinShots = 20;

// ---- Профиль ----

struct Profile {
    int best_score = 0;
    double best_accuracy = 0.0;      ///< %
    double best_precision = 0.0;     ///< %
    double best_avg_reaction = 0.0;  ///< мс; 0 — ещё нет
    int games_played = 0;
    bool tutorial_seen = false;
    Settings settings;
    std::deque<int> reactions;  ///< последние 50 реакций, мс, старые первыми
    std::array<bool, kAchievementCount> achievements{};
    Leaderboard leaderboard;
};

/// Открыть достижение; true — открыто сейчас, а не раньше.
bool unlock(Profile& profile, int achievement);

/// Достижения по событию забега (ProcessClick образца): по попаданию —
/// «В яблочко», «Молниеносно», «Комбо x100», «Тысяча». Возвращает открытые
/// сейчас.
std::vector<int> achievements_on_event(Profile& profile, const GameEvent& event);

struct FinishResult {
    std::vector<int> unlocked;  ///< открытые сейчас достижения
    int leaderboard_place = -1;
};

/// Итог законченного забега (FinishGameAndSaveStats): рекорды (D-13), счётчик
/// игр, таблица лидеров, достижения по итогу, история реакций.
FinishResult finish_game(Profile& profile, const GameSummary& summary);

}  // namespace aim::core
