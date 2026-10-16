#pragma once

// Главное меню без окна — перенос UpdateMenuInput, StartScreen и
// SwitchProfile образца (archive/pascal/AIM_Trainer.pas): выбор режима
// наведением, запуск щелчком по кнопке или плашке, кнопки внизу и панели
// поверх меню, туториал первого запуска, переключатель эффектов — щелчок
// по пустому месту. Геометрия — core/layout.h.
//
// Файлы меню не трогает: что записать и что прочитать, программа берёт из
// take_requests.

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "core/flow.h"
#include "core/layout.h"
#include "core/profile.h"

namespace aim::core {

enum class Panel : std::uint8_t { None, Help, Settings, Records, Achievements, History, Profiles };

/// Записать профиль: изменились настройки, тема, туториал.
struct SaveProfile {};
/// Открыть другой профиль (1…4).
struct SwitchProfile {
    int number = 1;
};
/// Открыта «История»: перечитать историю профиля.
struct LoadHistory {};
using MenuRequest = std::variant<SaveProfile, SwitchProfile, LoadHistory>;

/// Шагов туториала первого запуска.
inline constexpr int kTutorialSteps = 4;

class Menu final : public MenuInput {
public:
    /// profile — профиль игрока (настройки и эффекты меню меняются в нём);
    /// show_tutorial — туториал при запуске: профиль его ещё не видел.
    Menu(Profile& profile, int profile_number, bool show_tutorial);

    /// Меню показано в момент now: кнопки режимов появляются заново.
    void enter(Nanoseconds now);
    /// Курсор над меню: кнопка режима под ним — выбранный режим, если панелей
    /// и туториала нет (GetModeBtnAt).
    void hover(Vec2 cursor, Nanoseconds now);

    std::optional<GameMode> click(const ClickInput& click) override;
    void key(const KeyInput& key) override;

    /// Профиль сменился (программа открыла другой файл).
    void set_profile_number(int number) { profile_number_ = number; }

    [[nodiscard]] int selected_mode() const { return selected_; }
    [[nodiscard]] Panel panel() const { return panel_; }
    [[nodiscard]] bool tutorial_shown() const { return tutorial_; }
    [[nodiscard]] int tutorial_step() const { return tutorial_step_; }
    [[nodiscard]] int profile_number() const { return profile_number_; }
    [[nodiscard]] Nanoseconds entered_at() const { return entered_at_; }

    std::vector<MenuRequest> take_requests();

private:
    [[nodiscard]] Nanoseconds since_entered(Nanoseconds t) const;
    void close_tutorial();
    void click_settings(Vec2 p);
    std::optional<GameMode> start_selected();

    Profile& profile_;
    int profile_number_;
    bool tutorial_;
    int tutorial_step_ = 0;
    Panel panel_ = Panel::None;
    int selected_ = 0;
    Nanoseconds entered_at_ = 0;
    std::vector<MenuRequest> requests_;
};

}  // namespace aim::core
