#pragma once

// История забегов profileN_sessions.csv — формат Pascal-версии (D-11;
// archive/pascal/AIM_Trainer.pas: LogSession, LoadSessionSummary,
// TimeOfDayBucket, ExportSessionsCsv; docs/ARCHITECTURE.md, §8.2). Только
// текст: файлы — платформа и программа.
//
// Отличия от образца — нарочные: «История» читает последние 500 игр, а не
// первые (AUD-15); испорченная строка пропускается, а не обнуляет историю.

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/game.h"

namespace aim::core {

/// Местные дата и время записи.
struct LocalTime {
    int year = 2026;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
};

inline constexpr std::string_view kSessionsHeader =
    "Date,Time,Hour,Mode,Score,Accuracy,AvgReaction,Hits,Misses,MaxCombo";
/// Сколько последних игр читает «История» (MAX_SESSIONS_LOADED).
inline constexpr std::size_t kSessionsShown = 500;
/// Сколько последних игр на графике средней реакции в «Истории».
inline constexpr std::size_t kRecentGamesGraph = 20;

/// Строка истории без перевода строки: дата, время, час, режим, очки,
/// точность и средняя реакция с одним знаком после точки, попадания,
/// промахи щелчком, лучшая серия.
std::string session_line(const GameSummary& summary, const LocalTime& when);

/// Имя файла экспорта: export_profileN_ГГГГММДД.csv.
std::string export_file_name(int profile, const LocalTime& when);

struct SessionRow {
    int hour = 0;
    int score = 0;
    double avg_reaction = 0.0;  ///< мс
};

/// Строки истории без заголовка, последние keep_last; строки, где меньше
/// десяти полей или час, очки и реакция — не числа, пропускаются.
std::vector<SessionRow> read_sessions(std::string_view csv, std::size_t keep_last = kSessionsShown);

/// Время суток игры: 0 — утро (6–12), 1 — день (12–18), 2 — вечер (18–23),
/// 3 — ночь (23–6).
int time_of_day(int hour);

struct TimeOfDayScore {
    int games = 0;
    double average_score = 0.0;
};

/// Средний счёт по времени суток — верх панели «История».
std::array<TimeOfDayScore, 4> scores_by_time_of_day(const std::vector<SessionRow>& rows);

/// Средняя реакция последних count игр, старые первыми, — график «Истории».
std::vector<double> recent_avg_reactions(const std::vector<SessionRow>& rows,
                                         std::size_t count = kRecentGamesGraph);

}  // namespace aim::core
