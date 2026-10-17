// DP AIM Trainer (D-08, D-25): разбор ключей и программа — app/app.h;
// исключение — окном с сообщением.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <exception>
#include <optional>
#include <string>

#include "app/app.h"

int main(int argc, char* argv[]) {
    const std::optional<aim::app::Options> options = aim::app::parse_options(argc, argv);
    if (!options) {
        SDL_Log("%s", aim::app::kUsage);
        return 2;
    }
    // Исключение (нет памяти, выход за границы) — сказать игроку окном, а не
    // закрыться молча (AUD-51).
    try {
        return aim::app::run(*options);
    } catch (const std::exception& e) {
        const std::string text = std::string("Программа остановилась из-за ошибки: ") + e.what();
        SDL_Log("%s", text.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DP AIM Trainer", text.c_str(), nullptr);
    } catch (...) {
        SDL_Log("Программа остановилась из-за неизвестной ошибки");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DP AIM Trainer",
                                 "Программа остановилась из-за неизвестной ошибки", nullptr);
    }
    return 1;
}
