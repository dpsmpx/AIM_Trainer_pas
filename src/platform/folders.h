#pragma once

// Папка сохранений для игрока (D-33): показать её в файловом менеджере
// системы и выбрать другую системным диалогом. Диалог SDL3 асинхронный —
// ответ может прийти из другого потока, поэтому его забирает главный цикл
// (take).

#include <SDL3/SDL.h>

#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace aim::platform {

/// Открыть папку dir в файловом менеджере системы. false — error.
bool open_folder(const std::string& dir, std::string& error);

/// Ответ диалога выбора папки.
struct FolderChoice {
    std::string path;  ///< выбранная папка; пусто — отмена или ошибка
    std::string error;  ///< диалог не открылся; пусто — нет ошибки
};

class FolderPicker {
public:
    FolderPicker();
    /// Показать диалог над окном, начиная с папки start. Пока диалог открыт,
    /// второй не открывается.
    void show(SDL_Window* window, const std::string& start);
    /// Ответ, если пришёл; забирается один раз.
    std::optional<FolderChoice> take();
    [[nodiscard]] bool open() const;

private:
    struct State {
        std::mutex mutex;
        bool open = false;
        std::optional<FolderChoice> answer;
    };
    static void SDLCALL on_answer(void* userdata, const char* const* files, int filter);

    std::shared_ptr<State> state_;
};

}  // namespace aim::platform
