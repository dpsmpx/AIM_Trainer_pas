#pragma once

// Время клика для замера реакции (D-12, D-24): нажатие из события SDL
// сопоставляется с нажатием из своего потока сырого ввода. Логика без окна —
// часов сама не читает: время ей передают.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/mouse.h"
#include "core/time.h"

namespace aim::core {

/// Нажатие из своего потока сырого ввода (D-24).
struct RawPress {
    Nanoseconds time = 0;
    MouseButton button = MouseButton::Left;
};

enum class ClickTimeSource : std::uint8_t {
    RawInput,  ///< свой поток сырого ввода (D-24)
    Event,  ///< событие SDL: сырого нажатия нет или оно не нашлось
};

struct ClickTime {
    Nanoseconds time = 0;
    ClickTimeSource source = ClickTimeSource::Event;
};

/// Сопоставляет нажатия из событий SDL с сырыми нажатиями (D-24).
///
/// На Windows метка события SDL — из msg.time: счётчик тиков с шагом около
/// 15,6 мс, который SDL сдвигает к своим часам так, чтобы метка не была позже
/// момента получения (WIN_GetEventTimestamp в SDL3). Поэтому метка SDL
/// раньше настоящего нажатия на 0…16 мс и почти никогда не позже него, а
/// сырое нажатие опаздывает на доли миллисекунды. Окно сопоставления —
/// несимметричное: сырое нажатие той же кнопки берётся, если оно не раньше
/// метки SDL на kRawBeforeEvent и не позже на kRawAfterEvent. Прошлое нажатие
/// той же кнопки попадает в окно, только если оно было меньше чем за 26 мс до
/// этого (10 мс окна, 16 мс шага тиков) — 38 нажатий одной кнопки в секунду:
/// так мышью не щёлкают.
///
/// Сырые нажатия, которым события SDL нет (клик по заголовку окна, мимо
/// окна), выбрасываются: те, что старше окна, сопоставить уже не с чем.
class ClickTimer {
public:
    static constexpr Nanoseconds kRawBeforeEvent = 10'000'000;
    static constexpr Nanoseconds kRawAfterEvent = 26'000'000;
    /// Дольше сырое нажатие не ждёт события: главный цикл столько не стоит.
    static constexpr Nanoseconds kMaxAge = 1'000'000'000;
    /// Сколько нажатий ждут сопоставления; лишние — самые старые — забываются.
    static constexpr std::size_t kCapacity = 64;

    /// Новое сырое нажатие; нажатия приходят по порядку времени.
    void add(const RawPress& press);

    /// Время клика по событию SDL с меткой event_time. Найденное сырое
    /// нажатие и более ранние той же кнопки больше не участвуют.
    ClickTime resolve(MouseButton button, Nanoseconds event_time);

    /// Забыть сырые нажатия старше kMaxAge на часах now.
    void forget_old(Nanoseconds now);

    [[nodiscard]] std::size_t pending() const { return pending_.size(); }

private:
    std::vector<RawPress> pending_;
};

}  // namespace aim::core
