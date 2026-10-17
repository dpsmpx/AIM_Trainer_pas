#pragma once

// Реплей замера (D-39): записан вход, а не картинка — опции, начало
// генератора и вызовы Game по порядку: щелчки, кадры с курсором, показы
// кадров, конец по слову игрока; время — от начала замера. Логика замера
// детерминирована, так что тот же вход даёт тот же замер. Запись ведёт
// Flow (core/flow.h), проигрывает ReplayPlayer; текст файла — здесь, файлы
// — программа (app/player_files.h).

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/game.h"
#include "core/measure.h"
#include "core/mouse.h"
#include "core/random.h"
#include "core/runs.h"
#include "core/target.h"
#include "core/time.h"

namespace aim::core {

/// Какой вызов Game записан.
enum class ReplayStepKind : std::uint8_t {
    Click,   ///< Game::click
    Update,  ///< Game::update: кадр и курсор
    Shown,   ///< Game::frame_presented
    Finish,  ///< Game::finish: Esc или M
};

/// Шаг записи. Время — наносекунды от начала замера; у щелчка, сделанного
/// до начала (сырой ввод раньше события SDL), — меньше нуля.
struct ReplayStep {
    ReplayStepKind kind = ReplayStepKind::Update;
    std::int64_t at = 0;  ///< кадр, в котором сделан вызов
    std::int64_t time = 0;  ///< щелчка, показа кадра или конца; у Update — как at
    Vec2 point;  ///< точка щелчка; у Update — курсор
    MouseButton button = MouseButton::Left;
    EndReason reason = EndReason::None;

    friend bool operator==(const ReplayStep&, const ReplayStep&) = default;
};

/// Реплей: замер (когда, опции, начало генератора, итог) и его вход.
struct Replay {
    RunRecord run;
    std::vector<ReplayStep> steps;
};

/// Первая строка файла — версия формата; вторая и третья — заголовок и
/// строка замера, как в истории (core/runs.h); дальше — шаги.
inline constexpr std::string_view kReplayVersionLine = "# DP AIM Trainer replay v1";

/// Папка реплеев в папке сохранений.
inline constexpr std::string_view kReplayFolder = "replays";

/// Имя файла реплея набора options у профиля: profileN_<ключ набора>.replay,
/// «;» ключа — «_».
std::string replay_file_name(int profile, const MeasureOptions& options);
/// Файл реплея профиля: profileN_….replay.
bool is_replay_file(std::string_view name);

/// Текст файла реплея.
std::string write_replay(const Replay& replay, std::string_view newline = "\n");
/// Реплей из текста; не тот формат или испорченная строка — пусто: реплей
/// с пропуском не повторит замер.
std::optional<Replay> read_replay(std::string_view text);

/// Запись входа замера: шаги — по ходу, итог — в конце.
class ReplayRecorder {
public:
    /// Замер с опциями options и началом генератора seed начался в start.
    ReplayRecorder(const MeasureOptions& options, std::uint64_t seed, Nanoseconds start);

    void click(MouseButton button, Vec2 point, Nanoseconds t, Nanoseconds frame);
    void update(Nanoseconds now, Vec2 cursor);
    void shown(Nanoseconds frame_time, Nanoseconds presented_at);
    void finish(EndReason why, Nanoseconds t, Nanoseconds frame);

    /// Замер окончен: реплей с итогом run.
    std::shared_ptr<const Replay> take(const RunRecord& run);

private:
    [[nodiscard]] std::int64_t offset(Nanoseconds t) const;

    std::shared_ptr<Replay> replay_;
    Nanoseconds start_;
};

/// Проигрывание: замер заново из записи в реальном времени.
class ReplayPlayer {
public:
    /// Замер записи, начатый в start на часах проигрывания; генератор — по
    /// началу из записи.
    ReplayPlayer(std::shared_ptr<const Replay> replay, const RandomFactory& make_random,
                 Nanoseconds start);

    /// Сделать шаги, чей кадр наступил к now; ход замера — как при записи.
    void advance(Nanoseconds now);

    [[nodiscard]] const Replay& replay() const { return *replay_; }
    [[nodiscard]] const Game& game() const { return game_; }
    [[nodiscard]] Game& game() { return game_; }
    /// Все шаги сделаны.
    [[nodiscard]] bool done() const { return next_ >= replay_->steps.size(); }
    /// Курсор игрока в последнем сделанном кадре.
    [[nodiscard]] Vec2 cursor() const { return cursor_; }
    /// Последний сделанный кадр на часах проигрывания: на этот момент
    /// замер и рисуется — как его видел игрок.
    [[nodiscard]] Nanoseconds frame_time() const { return frame_time_; }

private:
    [[nodiscard]] Nanoseconds clock(std::int64_t offset) const;

    std::shared_ptr<const Replay> replay_;
    std::unique_ptr<Random> random_;  // раньше game_: замер держит ссылку
    Nanoseconds start_;
    Game game_;
    std::size_t next_ = 0;
    Vec2 cursor_;
    Nanoseconds frame_time_;
};

}  // namespace aim::core
