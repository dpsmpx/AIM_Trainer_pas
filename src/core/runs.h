#pragma once

// История замеров profileN_runs.csv — свой формат с версией (D-38): каждый
// законченный замер — строка со всеми параметрами и мерами. Только текст:
// файлы — платформа и программа.

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/game.h"
#include "core/measure.h"

namespace aim::core {

/// Местные дата и время записи.
struct LocalTime {
    int year = 2026;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
    int second = 0;

    friend bool operator==(const LocalTime&, const LocalTime&) = default;
};

/// Замер в истории: когда и итог без списка реакций.
struct RunRecord {
    LocalTime when;
    GameSummary summary;
};

/// Первая строка файла — версия формата; вторая — заголовок столбцов.
inline constexpr std::string_view kRunsVersionLine = "# DP AIM Trainer runs v1";
inline constexpr std::string_view kRunsHeader =
    "date,time,seed,stimulus,action,buttons,moving,appear,bombs,size,lifetime_ms,ending,lives,"
    "duration_s,score,hits,shots,misses,expired,tracked,max_combo,accuracy,precision,"
    "avg_reaction_ms,median_reaction_ms,run_ms,end";

/// Строка истории без перевода строки.
std::string run_line(const RunRecord& run);
/// Строка истории — замер; не та строка (заголовок, иное число полей, не
/// число) — пусто.
std::optional<RunRecord> parse_run_line(std::string_view line);
/// Все замеры файла по порядку; испорченные строки пропускаются, их число —
/// в skipped.
std::vector<RunRecord> read_runs(std::string_view csv, std::size_t* skipped = nullptr);

/// Имя файла экспорта: export_profileN_ГГГГММДД.csv.
std::string export_file_name(int profile, const LocalTime& when);

/// Слово причины конца в файле и обратно.
std::string_view end_word(EndReason end);
std::optional<EndReason> parse_end(std::string_view word);

}  // namespace aim::core
