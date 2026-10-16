#pragma once

// Экраны и переходы между ними — перенос главного цикла образца:
// StartScreen, CountdownScreen, GameLoop, GameOverScreen
// (archive/pascal/AIM_Trainer.pas, docs/ARCHITECTURE.md, §2).
//
//   меню ──старт──► отсчёт 3 с ──► игра ──конец жизней, бомба, Esc──► Game Over
//    ▲  ◄──── M ─────┘   ▲          │ R (без записи)                  │ R
//    │                   └──────────┘──────────────────────────────────┘
//    └──── M (с записью) из игры; Space или M из Game Over
//
// Ввод — события по порядку их времени (щелчки со временем из D-24, клавиши).
// Событие достаётся экрану, активному в момент разбора; после смены экрана
// остальные события кадра отбрасываются: нажатие не переходит на следующий
// экран (AUD-02). Клавиши — события, а не флаги, гаснущие в конце кадра:
// нажатие во время кадра не теряется (AUD-16).

#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "core/game.h"
#include "core/modes.h"
#include "core/mouse.h"
#include "core/profile.h"
#include "core/random.h"
#include "core/target.h"
#include "core/time.h"

namespace aim::core {

enum class Screen : std::uint8_t { Menu, Countdown, Game, GameOver };

/// Клавиши, которые читают экраны; остальные программа не передаёт.
enum class Key : std::uint8_t { Escape, Space, P, R, M };

struct ClickInput {
    MouseButton button = MouseButton::Left;
    Vec2 position;
    Nanoseconds time = 0;  ///< время нажатия (D-24)
};

struct KeyInput {
    Key key = Key::Escape;
    Nanoseconds time = 0;
};

using Input = std::variant<ClickInput, KeyInput>;

/// Меню: что делает щелчок или клавиша. Полное меню — шаг «Экраны»;
/// переходам нужно знать только, начат ли режим.
class MenuInput {
public:
    MenuInput() = default;
    MenuInput(const MenuInput&) = delete;
    MenuInput& operator=(const MenuInput&) = delete;
    MenuInput(MenuInput&&) = delete;
    MenuInput& operator=(MenuInput&&) = delete;
    virtual ~MenuInput() = default;

    /// Щелчок в меню; режим — если щелчок его запускает.
    virtual std::optional<GameMode> click(const ClickInput& click) = 0;
    /// Клавиша в меню (Esc закрывает панели — шаг «Экраны»).
    virtual void key(const KeyInput& key) = 0;
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
/// Забег окончен и записан в профиль (FinishGameAndSaveStats образца); файлы
/// — шаг «Файлы профиля».
struct RunFinished {
    GameSummary summary;
    int leaderboard_place = -1;
};
using FlowEvent = std::variant<ScreenChanged, GameEvent, AchievementUnlocked, RunFinished>;

class Flow {
public:
    Flow(Profile& profile, Random& rng, MenuInput& menu);

    /// Ввод кадра, по порядку времени. now — время кадра.
    void input(std::span<const Input> events, Nanoseconds now);
    /// Кадр на момент now: отсчёт, забег. cursor — курсор в окне (Tracking).
    void update(Nanoseconds now, Vec2 cursor);
    /// Кадр на момент frame_time показан в presented_at (D-12).
    void frame_presented(Nanoseconds frame_time, Nanoseconds presented_at);

    [[nodiscard]] Screen screen() const { return screen_; }
    [[nodiscard]] GameMode mode() const { return mode_; }
    /// Забег — в игре и на Game Over (итоги и тепловая карта); иначе нет.
    [[nodiscard]] const Game* game() const;
    /// Отсчёт: 3, 2, 1 на момент now; вне отсчёта — 0.
    [[nodiscard]] int countdown_value(Nanoseconds now) const;

    /// События с прошлого вызова, по порядку.
    std::vector<FlowEvent> take_events();

private:
    void go(Screen to);
    void start_countdown(Nanoseconds now);
    void finish_run();
    void pass_game_events();
    /// Событие экрану; false — экран сменился, остальной ввод кадра — в сторону.
    bool handle(const Input& event);

    Profile& profile_;
    Random& rng_;
    MenuInput& menu_;
    Nanoseconds now_ = 0;    // время разбираемого кадра
    bool finished_ = false;  // забег уже записан в профиль
    Screen screen_ = Screen::Menu;
    GameMode mode_ = GameMode::Classic;
    Nanoseconds countdown_started_ = 0;
    std::optional<Game> game_;
    std::vector<FlowEvent> events_;
};

}  // namespace aim::core
