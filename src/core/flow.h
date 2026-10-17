#pragma once

// Экраны и переходы между ними (docs/ARCHITECTURE.md, §5):
//
//   меню ──старт──► отсчёт 3 с ──► замер ──жизни, время, бомба, Esc──► итог
//    ▲  ◄──── M ─────┘   ▲          │ R (без записи)     R, «Ещё раз» │
//    │                   └──────────┘──────────────────────────────────┘
//    └──── M (с записью) из замера; Space, M, «В меню» из итога
//
//   меню ──реплей набора──► реплей ──Esc, Space, M──► меню
//   итог ──«Реплей» нового рекорда──► реплей ──Esc, Space, M──► итог (D-46)
//
// Кнопки итога срабатывают через kResultButtonsDelay после его появления:
// щелчок, начатый в замере, их не нажимает (AUD-02).
//
// Паузы нет: замер идёт непрерывно (D-40). У каждого замера — свой
// генератор случайных чисел с записанным началом, а вход замера пишется:
// реплей проигрывает его заново (D-39, core/replay.h).
//
// Ввод — события по порядку их времени (щелчки со временем из D-24, клавиши).
// Событие достаётся экрану, активному в момент разбора; после смены экрана
// остальные события кадра отбрасываются: нажатие не переходит на следующий
// экран (AUD-02). Клавиши — события, а не флаги, гаснущие в конце кадра:
// нажатие во время кадра не теряется (AUD-16).

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "core/game.h"
#include "core/layout.h"
#include "core/measure.h"
#include "core/mouse.h"
#include "core/profile.h"
#include "core/random.h"
#include "core/replay.h"
#include "core/target.h"
#include "core/time.h"

namespace aim::core {

enum class Screen : std::uint8_t { Menu, Countdown, Game, GameOver, Replay };

/// Клавиши, которые читают экраны; остальные программа не передаёт.
enum class Key : std::uint8_t { Escape, Space, R, M, Up, Down, PageUp, PageDown };

struct ClickInput {
    MouseButton button = MouseButton::Left;
    Vec2 position;
    Nanoseconds time = 0;  ///< время нажатия (D-24)
};

struct KeyInput {
    Key key = Key::Escape;
    Nanoseconds time = 0;
};

/// Колёсико мыши: y > 0 — от себя (вверх).
struct WheelInput {
    float y = 0;
    Nanoseconds time = 0;
};

using Input = std::variant<ClickInput, KeyInput, WheelInput>;

/// Меню: что делает щелчок или клавиша. Переходам нужно знать только,
/// начат ли замер и с какими опциями.
class MenuInput {
public:
    MenuInput() = default;
    MenuInput(const MenuInput&) = delete;
    MenuInput& operator=(const MenuInput&) = delete;
    MenuInput(MenuInput&&) = delete;
    MenuInput& operator=(MenuInput&&) = delete;
    virtual ~MenuInput() = default;

    /// Щелчок в меню; опции замера — если щелчок его запускает.
    virtual std::optional<MeasureOptions> click(const ClickInput& click) = 0;
    /// Клавиша в меню (Esc закрывает панели, стрелки листают справку).
    virtual void key(const KeyInput& key) = 0;
    /// Колёсико в меню (листает справку).
    virtual void wheel(const WheelInput& wheel) = 0;
};

/// Длительность отсчёта перед игрой.
inline constexpr Nanoseconds kCountdown = ms(3000);

struct ScreenChanged {
    Screen from = Screen::Menu;
    Screen to = Screen::Menu;
};
struct AchievementUnlocked {
    int id = 0;
};
/// Замер окончен и записан в профиль; файлы пишет программа
/// (app/player_files.h).
struct RunFinished {
    RunRecord record;
    bool new_best = false;  ///< лучший замер своего набора (D-38)
    std::shared_ptr<const Replay> replay;  ///< вход замера (D-39)
};
using FlowEvent = std::variant<ScreenChanged, GameEvent, AchievementUnlocked, RunFinished>;

class Flow {
public:
    /// seeds — источник начал генераторов замеров; make_random — генератор
    /// замера по началу (тесты дают заданную последовательность). now —
    /// время записи замера (программа даёт местное время).
    Flow(Profile& profile, Random& seeds, MenuInput& menu,
         RandomFactory make_random = engine_random, std::function<LocalTime()> clock = {});

    /// Ввод кадра, по порядку времени. now — время кадра.
    void input(std::span<const Input> events, Nanoseconds now);
    /// Кадр на момент now: отсчёт, замер. cursor — курсор в окне.
    void update(Nanoseconds now, Vec2 cursor);
    /// Кадр на момент frame_time показан в presented_at (D-12).
    void frame_presented(Nanoseconds frame_time, Nanoseconds presented_at);
    /// Из меню — реплей с момента now (D-39); не в меню — ничего.
    void start_replay(std::shared_ptr<const Replay> replay, Nanoseconds now);

    /// На итоге — кнопка «Реплей»: замер — новый рекорд набора, и его вход
    /// записан (D-46).
    [[nodiscard]] bool replay_offered() const;
    /// Когда показан итог: кнопки срабатывают через kResultButtonsDelay.
    [[nodiscard]] Nanoseconds result_shown_at() const { return result_shown_; }
    /// Куда выходит реплей: в меню или на итог (D-46).
    [[nodiscard]] Screen replay_back() const { return replay_back_; }

    [[nodiscard]] Screen screen() const { return screen_; }
    [[nodiscard]] const MeasureOptions& options() const { return options_; }
    /// Замер — в игре, на итоге (итоги и тепловая карта) и в реплее; иначе
    /// нет.
    [[nodiscard]] const Game* game() const;
    /// Реплей — на его экране; иначе нет.
    [[nodiscard]] const ReplayPlayer* replay() const;
    /// Итог последнего записанного замера; нет — пусто.
    [[nodiscard]] const std::optional<RunFinished>& last_run() const { return last_run_; }
    /// Отсчёт: 3, 2, 1 на момент now; вне отсчёта — 0.
    [[nodiscard]] int countdown_value(Nanoseconds now) const;

    /// События с прошлого вызова, по порядку.
    std::vector<FlowEvent> take_events();

private:
    void go(Screen to);
    void start_countdown(Nanoseconds now);
    void finish_run();
    /// Замер записан, итог показан с момента разбираемого кадра.
    void show_result();
    /// Реплей с момента now; по выходу — экран back.
    void play(std::shared_ptr<const Replay> replay, Nanoseconds now, Screen back);
    /// Щелчок на итоге: кнопки (D-46); false — экран сменился.
    bool click_result(const ClickInput& click);
    void pass_game_events();
    /// Событие экрану; false — экран сменился, остальной ввод кадра — в сторону.
    bool handle(const Input& event);
    bool handle_menu(const Input& event);
    bool handle_game(const Input& event);
    bool handle_result(const Input& event);

    Profile& profile_;
    Random& seeds_;
    MenuInput& menu_;
    RandomFactory make_random_;
    std::function<LocalTime()> clock_;
    Nanoseconds now_ = 0;    // время разбираемого кадра
    bool finished_ = false;  // замер уже записан в профиль
    Screen screen_ = Screen::Menu;
    MeasureOptions options_;
    Nanoseconds countdown_started_ = 0;
    Nanoseconds result_shown_ = 0;
    Screen replay_back_ = Screen::Menu;  // куда выходит реплей
    std::uint64_t seed_ = 0;
    std::unique_ptr<Random> run_random_;  // раньше game_: замер держит ссылку
    std::optional<Game> game_;
    std::optional<ReplayRecorder> recorder_;  // вход замера game_
    std::optional<ReplayPlayer> player_;
    std::optional<RunFinished> last_run_;
    std::vector<FlowEvent> events_;
};

}  // namespace aim::core
