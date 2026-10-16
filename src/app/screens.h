#pragma once

// Временные экраны шага «Время и ввод»: в C++-версию уже можно играть и мерить
// реакцию. Меню, панели, HUD, эффекты и шрифт образца — шаг «Экраны».

#include <SDL3/SDL.h>

#include <optional>
#include <string>

#include "core/flow.h"
#include "core/profile.h"

namespace aim::app {

/// Меню: четыре кнопки режимов на месте кнопок образца (BTNS_* в
/// archive/pascal/AIM_Trainer.pas); щелчок ЛКМ по кнопке запускает режим.
class TempMenu final : public core::MenuInput {
public:
    std::optional<core::GameMode> click(const core::ClickInput& click) override;
    void key(const core::KeyInput& /*key*/) override {}

    /// Кнопка режима под точкой окна.
    static std::optional<core::GameMode> button_at(core::Vec2 p);
    static SDL_FRect button_rect(core::GameMode mode);
};

/// То, что кадр показывает сверх состояния игры.
struct Overlay {
    int fps = 0;
    int profile = 1;  ///< номер профиля игрока
    std::string toast;  ///< уведомление (достижение, ошибка файла), пусто — нет
};

/// Нарисовать текущий экран на момент now.
void render(SDL_Renderer* renderer, const core::Flow& flow, const core::Profile& profile,
            core::Nanoseconds now, const Overlay& overlay);

}  // namespace aim::app
