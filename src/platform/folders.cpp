#include "platform/folders.h"

#include <utility>

namespace aim::platform {

bool open_folder(const std::string& dir, std::string& error) {
    // SDL_OpenURL отдаёт строку ShellExecute на Windows и xdg-open на Linux:
    // обе понимают и путь папки.
    if (SDL_OpenURL(dir.c_str())) return true;
    error = SDL_GetError();
    return false;
}

FolderPicker::FolderPicker() : state_(std::make_shared<State>()) {}

bool FolderPicker::open() const {
    const std::lock_guard lock(state_->mutex);
    return state_->open;
}

void FolderPicker::show(SDL_Window* window, const std::string& start) {
    {
        const std::lock_guard lock(state_->mutex);
        if (state_->open) return;
        state_->open = true;
    }
    // У ответа своя ссылка на состояние: окно программы могут закрыть раньше,
    // чем придёт ответ диалога.
    auto* holder = new std::shared_ptr<State>(state_);
    SDL_ShowOpenFolderDialog(on_answer, holder, window, start.empty() ? nullptr : start.c_str(),
                             false);
}

void SDLCALL FolderPicker::on_answer(void* userdata, const char* const* files, int /*filter*/) {
    const std::unique_ptr<std::shared_ptr<State>> holder(
        static_cast<std::shared_ptr<State>*>(userdata));
    State& state = **holder;
    FolderChoice choice;
    if (files == nullptr)
        choice.error = SDL_GetError();
    else if (files[0] != nullptr)
        choice.path = files[0];
    const std::lock_guard lock(state.mutex);
    state.open = false;
    state.answer = std::move(choice);
}

std::optional<FolderChoice> FolderPicker::take() {
    const std::lock_guard lock(state_->mutex);
    return std::exchange(state_->answer, std::nullopt);
}

}  // namespace aim::platform
