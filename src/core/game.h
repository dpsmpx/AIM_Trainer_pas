#pragma once

// Забег — перенос TGameState, ProcessClick и UpdateGame образца
// (archive/pascal/AIM_Trainer.pas, docs/ARCHITECTURE.md, §7). Отличия —
// нарочные:
//
//   * реакция — от показа первого кадра с целью до времени щелчка (D-12);
//   * серию обрывает и истёкшая обычная цель (D-14, AUD-26);
//   * щелчки разбираются до обновления кадра и по своему времени: щелчок,
//     сделанный до истечения цели, засчитывается по ней (AUD-03);
//   * часы — абсолютные; пауза сдвигает моменты цели, а не копит кадры.
//
// Что видно игроку и слышно — события (take_events): попадание, промах,
// бомба, истечение цели, засчитанная наводка. Достижения по ним —
// core/profile.h.

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

#include "core/modes.h"
#include "core/mouse.h"
#include "core/random.h"
#include "core/rules.h"
#include "core/target.h"
#include "core/time.h"

namespace aim::core {

/// Настройки, с которыми начат забег (из профиля, docs/ARCHITECTURE.md, §8.1).
struct GameSettings {
    int circle_size = 40;  ///< «Размер круга»: пик радиуса цели при масштабе 1
    int lifetime_ms = 1000;  ///< «Время жизни»; режимы — по D-21
    int lives = 15;          ///< «Жизни»
};

enum class GameEventKind : std::uint8_t {
    Hit,      ///< попадание щелчком
    Miss,     ///< промах щелчком: минус жизнь, серия обнулена
    Bomb,     ///< щелчок по бомбе: конец забега
    Expired,  ///< обычная цель истекла: минус жизнь, серия обнулена (D-14)
    Tracked,  ///< Tracking: цель засчитана наводкой
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

/// Промахи по клеткам окна для экрана Game Over: [столбец][строка].
using MissHeatmap = std::array<std::array<int, kHeatmapRows>, kHeatmapCols>;

/// Итог забега — для рекордов, таблицы лидеров, истории и достижений.
struct GameSummary {
    GameMode mode = GameMode::Classic;
    int score = 0;
    int hits = 0;
    int shots = 0;
    int misses = 0;  ///< промахи щелчком
    int max_combo = 0;
    double accuracy = 0.0;         ///< попадания / выстрелы, %
    double avg_reaction_ms = 0.0;  ///< средняя реакция попаданий, мс
    double precision = 0.0;        ///< средняя точность попаданий, %
    bool died_to_bomb = false;
    std::vector<double> reactions_ms;  ///< последние реакции забега, по порядку
};

class Game {
public:
    /// Забег в режиме mode с момента start; первая цель — сразу и не бомба.
    Game(GameMode mode, const GameSettings& settings, Random& rng, Nanoseconds start);

    /// Щелчок кнопкой в точку окна в момент t. Выстрелы — ЛКМ и ПКМ; другие
    /// кнопки, Tracking, пауза и законченный забег щелчков не принимают.
    void click(MouseButton button, Vec2 point, Nanoseconds t);

    /// Кадр на момент now: истечение цели, движение и наводка (Tracking),
    /// конец по жизням. cursor — курсор в окне (Tracking).
    void update(Nanoseconds now, Vec2 cursor);

    /// Кадр, нарисованный на момент frame_time, показан в presented_at (D-12).
    void frame_presented(Nanoseconds frame_time, Nanoseconds presented_at);

    void pause(Nanoseconds now);
    void resume(Nanoseconds now);
    /// Esc: забег окончен.
    void give_up();

    [[nodiscard]] bool paused() const { return paused_; }
    [[nodiscard]] bool over() const { return over_; }
    [[nodiscard]] bool died_to_bomb() const { return died_to_bomb_; }
    [[nodiscard]] GameMode mode() const { return mode_; }
    [[nodiscard]] const Target& target() const { return target_; }

    [[nodiscard]] int score() const { return score_; }
    [[nodiscard]] int hits() const { return hits_; }
    [[nodiscard]] int shots() const { return shots_; }
    [[nodiscard]] int misses() const { return misses_; }
    [[nodiscard]] int lives_left() const { return max_lives_ - lost_; }
    [[nodiscard]] int combo() const { return combo_; }
    [[nodiscard]] int max_combo() const { return max_combo_; }
    /// Попадания / выстрелы, %; без выстрелов — 0.
    [[nodiscard]] double accuracy() const;
    /// Средняя реакция попаданий, мс; без попаданий — 0.
    [[nodiscard]] double avg_reaction_ms() const;
    /// Средняя точность попаданий, %; без попаданий — 0.
    [[nodiscard]] double precision() const;
    /// Последние kReactionHistory реакций забега, старые первыми.
    [[nodiscard]] const std::deque<Nanoseconds>& reactions() const { return reactions_; }
    [[nodiscard]] const MissHeatmap& miss_heatmap() const { return heatmap_; }

    /// События с прошлого вызова, по порядку.
    std::vector<GameEvent> take_events();

    [[nodiscard]] GameSummary summary() const;

private:
    void respawn(Nanoseconds now);
    void lose_life();
    void record_miss(Vec2 point);

    GameMode mode_;
    ModeRules rules_;
    int circle_size_;
    int max_lives_;
    Random& rng_;
    Target target_;
    int spawned_ = 0;

    Nanoseconds last_update_;
    Nanoseconds pause_started_ = 0;
    bool paused_ = false;
    bool over_ = false;
    bool died_to_bomb_ = false;

    int score_ = 0;
    int hits_ = 0;
    int shots_ = 0;
    int misses_ = 0;
    int lost_ = 0;
    int combo_ = 0;
    int max_combo_ = 0;
    Nanoseconds total_reaction_ = 0;
    double precision_sum_ = 0.0;
    std::deque<Nanoseconds> reactions_;
    MissHeatmap heatmap_{};
    std::vector<GameEvent> events_;
};

}  // namespace aim::core
