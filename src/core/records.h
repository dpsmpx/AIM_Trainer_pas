#pragma once

// Рекорды по набору параметров (D-38): у каждого набора — лучший замер и
// среднее его последних N замеров. Источник — история замеров
// (core/runs.h): рекорд из неё выводится, а не хранится отдельно.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "core/runs.h"

namespace aim::core {

/// Среднее за N — по умолчанию и границы настройки.
inline constexpr int kDefaultAverageRuns = 50;
inline constexpr int kMinAverageRuns = 5;
inline constexpr int kMaxAverageRuns = 500;

/// Главная мера замера: у замера на цвет — медиана реакции (меньше —
/// лучше), у остальных — очки (больше — лучше).
[[nodiscard]] bool lower_is_better(const MeasureOptions& options);
[[nodiscard]] double primary_value(const GameSummary& summary);
/// Замер годится в рекорд: по реакции — от kRecordMinShots выстрелов и с
/// попаданиями (D-13); по очкам — с очками.
[[nodiscard]] bool counts_for_record(const GameSummary& summary);
/// a лучше b по главной мере набора; равные — нет.
[[nodiscard]] bool better(const GameSummary& a, const GameSummary& b);

/// Средние мер за последние замеры набора.
struct Averages {
    int runs = 0;  ///< сколько замеров взято
    double score = 0.0;
    double accuracy = 0.0;
    double precision = 0.0;
    double avg_reaction_ms = 0.0;     ///< по замерам с попаданиями
    double median_reaction_ms = 0.0;  ///< по замерам с попаданиями
    double primary = 0.0;             ///< главная мера
};

/// Рекорд набора.
struct ComboStats {
    std::string key;
    MeasureOptions options;
    int runs = 0;                     ///< всего замеров набора
    std::optional<std::size_t> best;  ///< индекс лучшего замера в истории
    std::optional<std::size_t> last;  ///< индекс последнего замера
    Averages average;                 ///< за последние N
};

/// Рекорд набора options по истории runs; average_runs — N.
ComboStats combo_stats(const std::vector<RunRecord>& runs, const MeasureOptions& options,
                       int average_runs);
/// Рекорды всех наборов истории, свежие первыми.
std::vector<ComboStats> all_combo_stats(const std::vector<RunRecord>& runs, int average_runs);

/// Главные меры последних count замеров набора, старые первыми, — график.
std::vector<double> recent_primary(const std::vector<RunRecord>& runs,
                                   const MeasureOptions& options, std::size_t count);

}  // namespace aim::core
