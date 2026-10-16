#pragma once

// Запас кадра в замере (D-41): работа кадра — разбор событий, логика,
// отрисовка и показ — не больше половины периода монитора, чтобы кадр не
// опаздывал и при всплеске. Программа меряет работу каждого кадра замера и
// по концу замера сообщает сводку (журнал, --debug-title).

#include <cstddef>
#include <string>
#include <vector>

#include "core/time.h"

namespace aim::core {

struct FrameBudgetReport {
    std::size_t frames = 0;
    Nanoseconds period = 0;  ///< период монитора
    Nanoseconds median = 0;  ///< работа кадра: медиана
    Nanoseconds p99 = 0;     ///< 99-й процентиль
    Nanoseconds max = 0;
    std::size_t over_half = 0;  ///< кадров дольше половины периода
};

class FrameBudget {
public:
    /// Новый замер на мониторе с периодом period.
    void start(Nanoseconds period);
    /// Работа одного кадра.
    void add(Nanoseconds work);
    [[nodiscard]] bool empty() const { return work_.empty(); }
    [[nodiscard]] FrameBudgetReport report() const;

private:
    Nanoseconds period_ = 0;
    std::vector<Nanoseconds> work_;
};

/// Сводка для игрока: «кадр замера: 99 % — 0,42 мс при запасе 4,17 мс,
/// дольше — 0 из 3600».
std::string describe(const FrameBudgetReport& report);

}  // namespace aim::core
