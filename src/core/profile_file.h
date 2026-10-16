#pragma once

// Файлы игрока — свой формат с версией (D-38): профиль profileN.ini
// (настройки, выбранный набор, достижения), история profileN_runs.csv
// (core/runs.h), aim_trainer.ini папки по умолчанию (последний профиль и
// папка сохранений, D-30, D-33). Только текст: чтение и запись файлов —
// платформа и программа.
//
// Профиль — строки «ключ=значение», первой — версия формата. Неизвестные
// ключи пропускаются; значение вне смысла или границ — поле по умолчанию, и
// это видно игроку.

#include <string>
#include <string_view>
#include <vector>

#include "core/profile.h"
#include "core/runs.h"

namespace aim::core {

/// Профилей — 4, номера 1…4.
inline constexpr int kProfileCount = 4;

inline constexpr std::string_view kProfileVersionLine = "# DP AIM Trainer profile v1";

std::string profile_file_name(int profile);
std::string runs_file_name(int profile);
std::string corrupt_backup_file_name(int profile);

/// Файл профиля; newline — перевод строки системы.
std::string write_profile(const Profile& profile, std::string_view newline);

struct LoadedProfile {
    Profile profile;  ///< без истории: она — в своём файле
    /// Нет строки версии или версия другая: файл не этой программы —
    /// профиль по умолчанию, файл — в копию profileN_corrupt_backup.txt.
    bool corrupt = false;
    /// Поля, сброшенные из-за значений вне смысла или границ.
    std::vector<std::string> reset_fields;
};

/// Разобрать файл профиля. Перевод строки — любой, метка BOM пропускается.
LoadedProfile read_profile(std::string_view text);

/// Новый файл истории: строка версии и заголовок.
std::string runs_file_header(std::string_view newline);

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
/// profileN.ini, profileN_runs.csv, profileN_corrupt_backup.txt,
/// export_profileN_ГГГГММДД.csv.
bool is_player_file(std::string_view name);

/// Папка для файлов новой папки сохранений, заменённых копией из прежней
/// (D-35): backup_ГГГГММДД_ЧЧММ, со второй попытки — с «_N» на конце.
std::string backup_folder_name(const LocalTime& when, int attempt);

}  // namespace aim::core
