#pragma once

// Файл профиля profileN_stats.ini — формат Pascal-версии (D-11;
// archive/pascal/AIM_Trainer.pas: SaveStats, LoadStats;
// docs/ARCHITECTURE.md, §8.1). Только текст: чтение и запись файлов —
// платформа и программа.
//
// Отличия от образца — нарочные:
//   * значения вне диапазона сбрасываются по одному полю, остальное
//     сохраняется; образец с ними падал или переставал писать файл (AUD-07);
//   * настройки и тема — в границах панели настроек.

#include <string>
#include <string_view>
#include <vector>

#include "core/history.h"
#include "core/profile.h"

namespace aim::core {

/// Профилей — 4 (MAX_PROFILES), номера 1…4.
inline constexpr int kProfileCount = 4;

std::string stats_file_name(int profile);
std::string sessions_file_name(int profile);
std::string corrupt_backup_file_name(int profile);

/// Файл профиля. newline — перевод строки системы: у образца Writeln пишет
/// Environment.NewLine (CRLF на Windows, LF на Linux), и так файл, записанный
/// образцом на той же системе, после чтения и записи не меняется (D-11).
std::string write_stats(const Profile& profile, std::string_view newline);

struct LoadedProfile {
    Profile profile;
    /// Известный ключ с нечисловым значением: файл повреждён — профиль по
    /// умолчанию, файл — в копию (profileN_corrupt_backup.txt), как у образца.
    bool corrupt = false;
    /// Поля, сброшенные из-за значений вне диапазона (AUD-07).
    std::vector<std::string> reset_fields;
};

/// Разобрать файл профиля. Перевод строки — любой, метка BOM пропускается,
/// неизвестные ключи и строки без одного «=» — тоже.
LoadedProfile read_stats(std::string_view text);

/// Вещественное, как real.ToString() образца: до 15 значащих цифр, точка.
std::string format_real(double value);

/// Состояние программы — aim_trainer.ini в папке по умолчанию: последний
/// открытый профиль (AUD-21) и выбранная папка сохранений (D-33).
struct AppState {
    int last_profile = 1;
    std::string data_dir;  ///< пусто — папка по умолчанию
};

std::string app_state_file_name();
std::string write_app_state(const AppState& state, std::string_view newline);
/// Нет файла, ключа или номер вне 1…4 — профиль 1; нет DataDir — пусто.
AppState read_app_state(std::string_view text);

/// Файл игрока — то, что копируется в новую папку сохранений (D-33, D-35):
/// profileN_stats.ini, profileN_sessions.csv, profileN_corrupt_backup.txt,
/// export_profileN_ГГГГММДД.csv.
bool is_player_file(std::string_view name);

/// Папка для файлов новой папки сохранений, заменённых копией из прежней
/// (D-35): backup_ГГГГММДД_ЧЧММ, со второй попытки — с «_N» на конце.
std::string backup_folder_name(const LocalTime& when, int attempt);

}  // namespace aim::core
