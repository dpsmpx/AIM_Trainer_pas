// DP AIM Trainer (D-08, D-25): разбор ключей и программа — app/app.h.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <optional>

#include "app/app.h"

int main(int argc, char* argv[]) {
    const std::optional<aim::app::Options> options = aim::app::parse_options(argc, argv);
    if (!options) {
        SDL_Log("%s", aim::app::kUsage);
        return 2;
    }
    return aim::app::run(*options);
}
