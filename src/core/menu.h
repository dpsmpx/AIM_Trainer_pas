#pragma once

// Главное меню без окна: выбор набора замера наведением, запуск щелчком по
// кнопке набора или плашке (D-37), «Свой набор» — конструктор со всеми
// опциями замера, кнопки внизу и панели поверх меню, туториал первого
// запуска, переключатель эффектов — щелчок по пустому месту и строка
// настроек (D-47). Геометрия — core/layout.h.
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

enum class Panel : std::uint8_t {
    None,
    Help,
    Settings,
    Records,
    Achievements,
    History,
    Profiles,
    Constructor,  ///< «Свой набор» (D-37)
};

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
/// Следующая тема по кругу: встроенные, затем темы игрока — их знает
/// программа (D-44).
struct NextTheme {};
/// Реплей лучшего замера набора — щелчок по строке «Рекордов наборов»
/// (D-39); есть ли он — знает программа.
struct PlayReplay {
    MeasureOptions options;
};
using MenuRequest = std::variant<SaveProfile, SwitchProfile, ExportHistory, OpenDataFolder,
                                 ChooseDataFolder, PlayReplay, NextTheme>;

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
    /// Ползунок справки тянется, пока зажата ЛКМ, нажатая на полосе
    /// прокрутки; left_held — состояние ЛКМ в кадре.
    void drag(Vec2 cursor, bool left_held);

    std::optional<MeasureOptions> click(const ClickInput& click) override;
    void key(const KeyInput& key) override;
    void wheel(const WheelInput& wheel) override;

    /// Профиль сменился (программа открыла другой файл).
    void set_profile_number(int number) { profile_number_ = number; }

    /// Пункт под курсором или выбранный последним: набор (kPresets) или
    /// kCustomItem.
    [[nodiscard]] int selected_preset() const { return selected_; }
    [[nodiscard]] Panel panel() const { return panel_; }
    [[nodiscard]] bool tutorial_shown() const { return tutorial_; }
    [[nodiscard]] int tutorial_step() const { return tutorial_step_; }
    /// Справка: первая видимая строка (0…kHelpMaxScroll).
    [[nodiscard]] int help_scroll() const { return help_scroll_; }
    [[nodiscard]] int profile_number() const { return profile_number_; }
    [[nodiscard]] Nanoseconds entered_at() const { return entered_at_; }

    std::vector<MenuRequest> take_requests();

private:
    [[nodiscard]] Nanoseconds since_entered(Nanoseconds t) const;
    void close_tutorial();
    void click_settings(Vec2 p);
    void click_help(Vec2 p);
    void click_records(Vec2 p);
    /// Щелчок в конструкторе; «Начать» — опции «Своего набора».
    std::optional<MeasureOptions> click_constructor(Vec2 p);
    void scroll_help(int lines);
    /// Прокрутка по высоте курсора на полосе: верх — начало, низ — конец.
    void scroll_help_to(double y);
    /// Начать выбранный набор: он же — набор профиля.
    MeasureOptions start_selected();

    Profile& profile_;
    int profile_number_;
    bool tutorial_;
    int tutorial_step_ = 0;
    Panel panel_ = Panel::None;
    int selected_ = 0;
    int help_scroll_ = 0;
    bool dragging_help_ = false;  // ЛКМ нажата на полосе и ещё не отпущена
    Nanoseconds entered_at_ = 0;
    std::vector<MenuRequest> requests_;
};

}  // namespace aim::core
