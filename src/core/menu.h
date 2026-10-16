#pragma once

// Главное меню без окна: выбор набора замера наведением, запуск щелчком по
// кнопке набора или плашке (D-37), кнопки внизу и панели поверх меню,
// туториал первого запуска, переключатель эффектов — щелчок по пустому
// месту. Геометрия — core/layout.h.
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

/// Записать профиль: изменились настройки, тема, туториал, набор.
struct SaveProfile {};
/// Открыть другой профиль (1…4).
struct SwitchProfile {
    int number = 1;
};
/// «Экспорт в CSV» в «Истории» (D-33).
struct ExportHistory {};
/// Показать папку сохранений в файловом менеджере (D-33).
struct OpenDataFolder {};
/// Выбрать папку сохранений системным диалогом (D-33).
struct ChooseDataFolder {};
using MenuRequest =
    std::variant<SaveProfile, SwitchProfile, ExportHistory, OpenDataFolder, ChooseDataFolder>;

/// Шагов туториала первого запуска.
inline constexpr int kTutorialSteps = 4;

class Menu final : public MenuInput {
public:
    /// profile — профиль игрока (настройки и эффекты меню меняются в нём);
    /// show_tutorial — туториал при запуске: профиль его ещё не видел.
    Menu(Profile& profile, int profile_number, bool show_tutorial);

    /// Меню показано в момент now: кнопки наборов появляются заново.
    void enter(Nanoseconds now);
    /// Курсор над меню: кнопка набора под ним — выбранный набор, если
    /// панелей и туториала нет.
    void hover(Vec2 cursor, Nanoseconds now);

    std::optional<MeasureOptions> click(const ClickInput& click) override;
    void key(const KeyInput& key) override;

    /// Профиль сменился (программа открыла другой файл).
    void set_profile_number(int number) { profile_number_ = number; }

    /// Набор под курсором или выбранный последним (kPresets).
    [[nodiscard]] int selected_preset() const { return selected_; }
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
    /// Начать выбранный набор: он же — набор профиля.
    MeasureOptions start_selected();

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
