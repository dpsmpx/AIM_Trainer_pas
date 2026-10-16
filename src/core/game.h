#pragma once

// Замер — по опциям конструктора (core/measure.h, D-37; docs/ARCHITECTURE.md,
// §12):
//
//   * стимул — цель на поле или смена цвета окна;
//   * реакция — от показа первого кадра со стимулом до времени щелчка (D-12);
//   * серию обрывает промах и истёкшая обычная цель (D-14);
//   * щелчки разбираются до обновления кадра и по своему времени: щелчок,
//     сделанный до истечения цели, засчитывается по ней (AUD-03);
//   * паузы нет: замер идёт непрерывно (D-40);
//   * конец — по жизням, по времени, на бомбе или по слову игрока.
//
// Что видно игроку и слышно — события (take_events). Достижения по ним —
// core/profile.h.

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/measure.h"
#include "core/mouse.h"
#include "core/random.h"
#include "core/rules.h"
#include "core/target.h"
#include "core/time.h"

namespace aim::core {

enum class GameEventKind : std::uint8_t {
    Hit,  ///< попадание щелчком; у замера на цвет — реакция на смену цвета
    Miss,  ///< промах щелчком: серия обнулена, по жизням — минус жизнь
    Bomb,  ///< щелчок по бомбе: конец замера
    Expired,  ///< обычная цель истекла: серия обнулена, по жизням — минус жизнь (D-14)
    Tracked,  ///< цель засчитана наведением
    FalseStart,  ///< замер на цвет: щелчок раньше смены цвета — промах
};

struct GameEvent {
    GameEventKind kind = GameEventKind::Hit;
    Vec2 position;             ///< щелчка; у Expired и Tracked — центр цели
    int color = 0;             ///< цвет цели: 0 — синий, 1 — красный
    int points = 0;            ///< очки за событие
    double accuracy = 0.0;     ///< точность попадания, 0…1
    Nanoseconds reaction = 0;  ///< реакция попадания (D-12)
    int combo = 0;             ///< серия после события
    int score = 0;             ///< счёт после события
};

/// Почему замер кончился.
enum class EndReason : std::uint8_t {
    None,    ///< ещё идёт
    Lives,   ///< кончились жизни
    Time,    ///< кончилось время
    Bomb,    ///< щелчок по бомбе
    GaveUp,  ///< Esc — конец попытки (D-40)
    ToMenu,  ///< M — в меню
};

/// Промахи по клеткам окна для итога замера: [столбец][строка].
using MissHeatmap = std::array<std::array<int, kHeatmapRows>, kHeatmapCols>;

/// Итог замера — для рекордов, истории и достижений (D-38).
struct GameSummary {
    MeasureOptions options;
    std::uint64_t seed = 0;  ///< начало генератора замера (D-39)
    int score = 0;
    int hits = 0;
    int shots = 0;
    int misses = 0;   ///< промахи щелчком и фальстарты
    int expired = 0;  ///< истёкшие обычные цели
    int tracked = 0;  ///< цели, засчитанные наведением
    int max_combo = 0;
    double accuracy = 0.0;            ///< попадания / выстрелы, %
    double avg_reaction_ms = 0.0;     ///< средняя реакция попаданий, мс
    double median_reaction_ms = 0.0;  ///< медиана реакции попаданий, мс
    double precision = 0.0;           ///< средняя точность попаданий, %
    int duration_ms = 0;              ///< от начала до конца замера
    EndReason end = EndReason::None;
    std::vector<double> reactions_ms;  ///< все реакции замера, по порядку
};

/// Медиана; пусто — 0.
double median(std::vector<double> values);

class Game {
public:
    /// Замер с опциями options с момента start; первая цель — сразу и не
    /// бомба, у замера на цвет — первая пауза.
    Game(const MeasureOptions& options, Random& rng, Nanoseconds start);

    /// Щелчок кнопкой в точку окна в момент t. По цели — ЛКМ и ПКМ, у
    /// замера на цвет — ещё и СКМ; наведение и законченный замер щелчков не
    /// принимают.
    void click(MouseButton button, Vec2 point, Nanoseconds t);

    /// Кадр на момент now: конец по времени, истечение цели, движение и
    /// наводка. cursor — курсор в окне.
    void update(Nanoseconds now, Vec2 cursor);

    /// Кадр, нарисованный на момент frame_time, показан в presented_at (D-12).
    void frame_presented(Nanoseconds frame_time, Nanoseconds presented_at);

    /// Конец по слову игрока: Esc или M.
    void finish(EndReason why, Nanoseconds now);

    [[nodiscard]] bool over() const { return end_ != EndReason::None; }
    [[nodiscard]] EndReason end_reason() const { return end_; }
    [[nodiscard]] const MeasureOptions& options() const { return options_; }
    [[nodiscard]] bool screen_stimulus() const { return options_.stimulus == Stimulus::Screen; }
    [[nodiscard]] const Target& target() const { return target_; }
    /// Замер на цвет: окно в момент t — уже другого цвета.
    [[nodiscard]] bool signal_lit(Nanoseconds t) const { return t >= signal_at_; }

    [[nodiscard]] int score() const { return score_; }
    [[nodiscard]] int hits() const { return hits_; }
    [[nodiscard]] int shots() const { return shots_; }
    [[nodiscard]] int misses() const { return misses_; }
    [[nodiscard]] int lives_left() const { return max_lives_ - lost_; }
    /// Окончание по времени: осталось на момент now.
    [[nodiscard]] Nanoseconds time_left(Nanoseconds now) const;
    [[nodiscard]] int combo() const { return combo_; }
    [[nodiscard]] int max_combo() const { return max_combo_; }
    /// Попадания / выстрелы, %; без выстрелов — 0.
    [[nodiscard]] double accuracy() const;
    /// Средняя реакция попаданий, мс; без попаданий — 0.
    [[nodiscard]] double avg_reaction_ms() const;
    /// Медиана реакции попаданий, мс; без попаданий — 0.
    [[nodiscard]] double median_reaction_ms() const;
    /// Средняя точность попаданий, %; без попаданий — 0.
    [[nodiscard]] double precision() const;
    /// Реакции замера, по порядку.
    [[nodiscard]] const std::vector<Nanoseconds>& reactions() const { return reactions_; }
    [[nodiscard]] const MissHeatmap& miss_heatmap() const { return heatmap_; }

    /// События с прошлого вызова, по порядку.
    std::vector<GameEvent> take_events();

    [[nodiscard]] GameSummary summary() const;

private:
    void respawn(Nanoseconds now);
    void next_signal(Nanoseconds from);
    void lose_life(Nanoseconds at);
    void end(EndReason why, Nanoseconds at);
    void record_miss(Vec2 point);
    void click_screen(MouseButton button, Vec2 point, Nanoseconds t);
    void click_target(MouseButton button, Vec2 point, Nanoseconds t);
    void hit(Nanoseconds reaction);

    MeasureOptions options_;
    TargetRules rules_;
    int max_lives_;
    Random& rng_;
    Target target_;
    int spawned_ = 0;
    // Замер на цвет: когда окно сменит цвет и когда это показано.
    Nanoseconds signal_at_ = 0;
    std::optional<Nanoseconds> signal_shown_;

    Nanoseconds start_;
    Nanoseconds ends_at_ = 0;  // окончание по времени
    Nanoseconds last_update_;
    Nanoseconds ended_at_ = 0;
    EndReason end_ = EndReason::None;

    int score_ = 0;
    int hits_ = 0;
    int shots_ = 0;
    int misses_ = 0;
    int expired_ = 0;
    int tracked_ = 0;
    int lost_ = 0;
    int combo_ = 0;
    int max_combo_ = 0;
    Nanoseconds total_reaction_ = 0;
    double precision_sum_ = 0.0;
    std::vector<Nanoseconds> reactions_;
    MissHeatmap heatmap_{};
    std::vector<GameEvent> events_;
};

}  // namespace aim::core
