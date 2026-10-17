#pragma once

// Одна копия программы на папку по умолчанию (D-49): вторая копия писала бы
// тот же профиль и затирала замеры первой. На Windows — именованный мьютекс
// сеанса, на других системах — блокировка файла .aim_trainer.lock в папке.
// Проверки со своей AIM_TRAINER_DATA_DIR друг другу не мешают.

#include <cstdint>
#include <string>
#include <string_view>

namespace aim::platform {

class SingleInstance {
public:
    enum class State : std::uint8_t {
        Owned,    ///< эта копия — единственная
        Busy,     ///< папку держит другая копия
        Unknown,  ///< проверить не удалось (problem): работать дальше
    };

    /// Занять папку dir для этой копии; отпускается деструктором.
    explicit SingleInstance(const std::string& dir);
    ~SingleInstance();
    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;
    SingleInstance(SingleInstance&&) = delete;
    SingleInstance& operator=(SingleInstance&&) = delete;

    [[nodiscard]] State state() const { return state_; }
    /// Почему проверить не удалось — для журнала.
    [[nodiscard]] const std::string& problem() const { return problem_; }

    /// Поднять окно другой копии с заголовком, начинающимся с title:
    /// развернуть и вывести вперёд. Только Windows; true — окно нашлось.
    static bool raise_other(std::string_view title);

private:
    State state_ = State::Unknown;
    std::string problem_;
    std::intptr_t handle_ = -1;  ///< мьютекс (Windows) или файл блокировки
};

}  // namespace aim::platform
