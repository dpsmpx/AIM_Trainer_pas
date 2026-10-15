// Замерщик ввода на SDL2 (docs/LIB_PROBES.md). Время события SDL2 — SDL_GetTicks
// в миллисекундах в момент, когда событие ставится в очередь при опросе
// (SDL_PeepEventsInternal в src/events/SDL_events.c), а не момент клика.
#include <SDL.h>
#include <SDL_syswm.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <numbers>
#include <string>

#include "probe_common.h"

namespace {

void fill_circle(SDL_Renderer* r, float cx, float cy, float rad, SDL_Color col) {
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
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    SDL_Window* window = SDL_CreateWindow("SDL2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          probe::kWidth, probe::kHeight, 0);
    if (!window) return 1;
    // Без SDL_RENDERER_PRESENTVSYNC: кадры не ждут обновления экрана.
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) return 1;

    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    void* hwnd = SDL_GetWindowWMInfo(window, &wm) ? static_cast<void*>(wm.info.win.window) : nullptr;
    SDL_version ver;
    SDL_GetVersion(&ver);
    SDL_RendererInfo ri;
    SDL_GetRendererInfo(renderer, &ri);
    probe::LibInfo info;
    info.name = "SDL2";
    info.version = std::format("{}.{}.{}", ver.major, ver.minor, ver.patch);
    info.renderer = ri.name;
    info.timestamp = "мс SDL_GetTicks в момент постановки события в очередь при опросе";
    probe::Session session(info, hwnd);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    session.note("выход: окно закрыто");
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        session.note("выход: Esc");
                        running = false;
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN: {
                    const Uint32 lib_now = SDL_GetTicks();
                    const probe::Ns t = probe::now();
                    const Uint32 age_ms = lib_now - e.button.timestamp;
                    session.click(to_probe_button(e.button.button), static_cast<float>(e.button.x),
                                  static_cast<float>(e.button.y), t,
                                  probe::from_age(t, static_cast<probe::Ns>(age_ms) * 1'000'000ULL),
                                  std::to_string(e.button.timestamp));
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
                        SDL_Color{probe::kTarget.r, probe::kTarget.g, probe::kTarget.b, 255});
        SDL_SetRenderDrawColor(renderer, v.bar.r, v.bar.g, v.bar.b, 255);
        const SDL_FRect bar{0, 0, v.progress * static_cast<float>(probe::kWidth), v.bar_h};
        SDL_RenderFillRectF(renderer, &bar);
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
