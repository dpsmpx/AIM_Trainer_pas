#pragma once

// Профиль игрока в памяти: настройки, выбранный набор, достижения и история
// замеров (D-37, D-38). Рекорды и средние выводятся из истории
// (core/records.h). Текст файла профиля — core/profile_file.h, истории —
// core/runs.h.

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "core/game.h"
#include "core/measure.h"
#include "core/records.h"
#include "core/runs.h"

namespace aim::core {

// ---- Настройки ----
/// Общие размер, жизнь цели и жизни наборов — до правки опций в
/// конструкторе (D-37); жизнь цели — до множителя набора (D-21).
inline constexpr int kMinBaseLifetimeMs = 500;
inline constexpr int kMaxBaseLifetimeMs = 2000;
inline constexpr int kThemeCount = 3;  ///< 0 — Тёмная, 1 — Неон, 2 — Минимализм
/// Ступени «Среднее за N».
inline constexpr std::array<int, 8> kAverageRunSteps{5, 10, 20, 30, 50, 100, 200, 500};

struct Settings {
    PresetSettings preset;
    int average_runs = kDefaultAverageRuns;  ///< N у «среднего за N» (D-38)
    bool sound = true;
    bool menu_effects = true;  ///< частицы меню, «Effects: ON/OFF»
    int theme = 0;
    bool hud = true;      ///< панели и надписи в окне замера (D-45)
    bool replays = true;  ///< писать вход замера и реплей рекорда (D-45)
};

/// Ряд панели настроек со ступенями «−»/«+».
enum class SettingRow : std::uint8_t { TargetSize, Lifetime, Lives, AverageRuns };

/// Шаг ряда на delta ступеней с границами.
void step_setting(Settings& settings, SettingRow row, int delta);
/// «Сбросить по умолчанию»: размер, жизнь цели, жизни, окончание, N, звук,
/// интерфейс замера и запись реплеев; тему и эффекты не трогает.
void reset_settings(Settings& settings);
/// Значение вне границ — к умолчанию; для настроек, прочитанных из файла.
/// Возвращает, сколько полей сброшено.
int reset_out_of_range(Settings& settings);

// ---- Достижения ----
inline constexpr int kAchievementCount = 8;

struct AchievementInfo {
    std::string_view key;  ///< ключ в файле профиля
    std::string_view name;
    std::string_view description;
};

extern const std::array<AchievementInfo, kAchievementCount> kAchievements;

/// Порог серии и счёта для достижений «Серия x100» и «Тысяча».
inline constexpr int kComboAchievement = 100;
inline constexpr int kScoreAchievement = 1000;
/// «Молниеносно» — реакция короче, мс.
inline constexpr double kFastReactionMs = 150.0;

// ---- Профиль ----
/// Пункт меню «Свой набор» — после наборов kPresets (D-37).
inline constexpr int kCustomItem = kPresetCount;
inline constexpr int kMenuItems = kPresetCount + 1;

struct Profile {
    Settings settings;
    int preset = 1;  ///< выбранный пункт меню: набор (kPresets) или kCustomItem
    /// Опции «Своего набора», как их оставил игрок: опции, которые к замеру
    /// не относятся, хранятся и вернутся вместе со своим стимулом.
    MeasureOptions custom;
    bool tutorial_seen = false;
    std::array<bool, kAchievementCount> achievements{};
    std::vector<RunRecord> runs;  ///< история замеров, по порядку
};

/// Опции пункта меню item: набор с общими настройками или «Свой набор».
MeasureOptions item_options(const Profile& profile, int item);
/// Опции выбранного пункта меню.
MeasureOptions selected_options(const Profile& profile);

/// Открыть достижение; true — открыто сейчас, а не раньше.
bool unlock(Profile& profile, int achievement);

/// Достижения по событию замера: по попаданию — «В яблочко»,
/// «Молниеносно», «Серия x100», «Тысяча». Возвращает открытые сейчас.
std::vector<int> achievements_on_event(Profile& profile, const GameEvent& event);

struct FinishResult {
    std::vector<int> unlocked;  ///< открытые сейчас достижения
    bool new_best = false;      ///< лучший замер своего набора (D-38)
};

/// Итог законченного замера: в историю, рекорд набора, достижения по итогу.
FinishResult finish_game(Profile& profile, const RunRecord& run);

}  // namespace aim::core
