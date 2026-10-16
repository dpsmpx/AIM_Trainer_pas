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

/// Файл состояния программы: последний открытый профиль (AUD-21).
std::string app_state_file_name();
std::string write_app_state(int last_profile, std::string_view newline);
/// Последний профиль; нет файла, ключа или номер вне 1…4 — профиль 1.
int read_last_profile(std::string_view text);

}  // namespace aim::core
