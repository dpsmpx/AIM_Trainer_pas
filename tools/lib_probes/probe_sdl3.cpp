// Замерщик ввода на SDL3 (docs/LIB_PROBES.md).
//
// Обычный режим: время клика SDL3 берёт из msg.time сообщения Windows
// (WIN_GetEventTimestamp в src/video/windows/SDL_windowsevents.c).
// PROBE_SDL3_RAW — относительный режим мыши: SDL читает сырой ввод в своём
// потоке и ставит метку SDL_GetTicksNS при получении пакета
// (src/video/windows/SDL_windowsrawinput.c); курсор рисует программа.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <numbers>
#include <string>

#include "probe_common.h"

namespace {

void fill_circle(SDL_Renderer* r, float cx, float cy, float rad, SDL_FColor col) {
    constexpr int kSeg = 64;
    std::array<SDL_Vertex, kSeg + 1> v{};
    std::array<int, kSeg * 3> idx{};
    v[0] = SDL_Vertex{{cx, cy}, col, {0, 0}};
    for (int i = 0; i < kSeg; ++i) {
        const float a = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) /
                        static_cast<float>(kSeg);
        v[static_cast<std::size_t>(i) + 1] =
            SDL_Vertex{{cx + rad * std::cos(a), cy + rad * std::sin(a)}, col, {0, 0}};
        const auto k = static_cast<std::size_t>(i) * 3;
        idx[k] = 0;
        idx[k + 1] = i + 1;
        idx[k + 2] = (i + 1) % kSeg + 1;
    }
    SDL_RenderGeometry(r, nullptr, v.data(), kSeg + 1, idx.data(), kSeg * 3);
}

int to_probe_button(Uint8 b) {
    switch (b) {
        case SDL_BUTTON_LEFT: return 1;
        case SDL_BUTTON_RIGHT: return 2;
        case SDL_BUTTON_MIDDLE: return 3;
        default: return 0;
    }
}

}  // namespace

int main(int, char*[]) {
#ifdef PROBE_SDL3_RAW
    const char* name = "SDL3_raw";
#else
    const char* name = "SDL3";
#endif
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer(name, probe::kWidth, probe::kHeight, 0, &window, &renderer))
        return 1;
    SDL_SetRenderVSync(renderer, 0);
#ifdef PROBE_SDL3_RAW
    SDL_SetWindowRelativeMouseMode(window, true);
#endif

    const int ver = SDL_GetVersion();
    probe::LibInfo info;
    info.name = name;
    info.version = std::format("{}.{}.{}", SDL_VERSIONNUM_MAJOR(ver), SDL_VERSIONNUM_MINOR(ver),
                               SDL_VERSIONNUM_MICRO(ver));
    info.renderer = SDL_GetRendererName(renderer);
#ifdef PROBE_SDL3_RAW
    info.timestamp = "нс SDL_GetTicksNS в потоке сырого ввода SDL (относительный режим мыши)";
    info.raw_reference = false;  // сырой ввод мыши занят самой SDL
    info.check_cursor = false;   // курсор держит SDL
#else
    info.timestamp = "нс SDL_GetTicksNS, пересчитанные из msg.time сообщения Windows (мс)";
#endif
    probe::Session session(info, SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                                        SDL_PROP_WINDOW_WIN32_HWND_POINTER,
                                                        nullptr));
    float cursor_x = probe::kWidth / 2.0f;
    float cursor_y = probe::kHeight / 2.0f;
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_EVENT_QUIT:
                    session.note("выход: окно закрыто");
                    running = false;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (e.key.key == SDLK_ESCAPE) {
                        session.note("выход: Esc");
                        running = false;
                    }
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    cursor_x = std::clamp(cursor_x + e.motion.xrel, 0.0f,
                                          static_cast<float>(probe::kWidth));
                    cursor_y = std::clamp(cursor_y + e.motion.yrel, 0.0f,
                                          static_cast<float>(probe::kHeight));
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                    const Uint64 lib_now = SDL_GetTicksNS();
                    const probe::Ns t = probe::now();
                    const Uint64 age =
                        lib_now > e.button.timestamp ? lib_now - e.button.timestamp : 0;
#ifdef PROBE_SDL3_RAW
                    const float x = cursor_x, y = cursor_y;
#else
                    const float x = e.button.x, y = e.button.y;
#endif
                    session.click(to_probe_button(e.button.button), x, y, t,
                                  probe::from_age(t, age), std::to_string(e.button.timestamp));
                    break;
                }
                default:
                    break;
            }
        }

        const probe::View v = session.frame();
        SDL_SetRenderDrawColor(renderer, probe::kBackground.r, probe::kBackground.g,
                               probe::kBackground.b, 255);
        SDL_RenderClear(renderer);
        if (v.target)
            fill_circle(renderer, v.x, v.y, v.r,
                        SDL_FColor{probe::kTarget.r / 255.0f, probe::kTarget.g / 255.0f,
                                   probe::kTarget.b / 255.0f, 1.0f});
        SDL_SetRenderDrawColor(renderer, v.bar.r, v.bar.g, v.bar.b, 255);
        const SDL_FRect bar{0, 0, v.progress * static_cast<float>(probe::kWidth), v.bar_h};
        SDL_RenderFillRect(renderer, &bar);
#ifdef PROBE_SDL3_RAW
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        const SDL_FRect h{cursor_x - 8, cursor_y - 1, 17, 3};
        const SDL_FRect w{cursor_x - 1, cursor_y - 8, 3, 17};
        SDL_RenderFillRect(renderer, &h);
        SDL_RenderFillRect(renderer, &w);
#endif
        const std::string title = session.take_title();
        if (!title.empty()) SDL_SetWindowTitle(window, title.c_str());
        const probe::Ns call = probe::now();
        SDL_RenderPresent(renderer);
        session.presented(call, probe::now());
    }
    session.finish();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
