// Aim Trainer, C++-версия — каркас (D-08, D-25): окно 800×600 (D-02), главный
// цикл и время клика из своего потока сырого ввода (D-24). Экраны и режимы —
// следующие шаги переноса (ROADMAP/ROADMAP.md, task-cpp).
//
//   AimTrainer [--frames N] [--screenshot файл.bmp]
//
// --frames N — выйти после N кадров; --screenshot — записать последний кадр в
// BMP. Оба — для проверки без экрана (tests/CMakeLists.txt).

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <charconv>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "core/click_time.h"
#include "core/screen.h"
#include "platform/raw_mouse.h"

namespace {

using aim::core::ClickTime;
using aim::core::ClickTimer;
using aim::core::ClickTimeSource;
using aim::core::MouseButton;
using aim::core::Nanoseconds;

using aim::core::kDarkBackground;

constexpr const char* kTitle = "Aim Trainer";
// Ожидание события, пока рисовать нечего: каркас не крутит кадры впустую.
constexpr Sint32 kIdleWaitMs = 16;

struct Options {
    int frames = 0;  // 0 — до закрытия окна
    std::string screenshot;
};

std::optional<Options> parse_options(int argc, char* argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--frames" && i + 1 < argc) {
            const std::string_view value = argv[++i];
            const auto [end, error] =
                std::from_chars(value.data(), value.data() + value.size(), options.frames);
            if (error != std::errc{} || end != value.data() + value.size() || options.frames < 1) {
                return std::nullopt;
            }
        } else if (arg == "--screenshot" && i + 1 < argc) {
            options.screenshot = argv[++i];
        } else {
            return std::nullopt;
        }
    }
    return options;
}

std::optional<MouseButton> to_button(Uint8 sdl_button) {
    switch (sdl_button) {
        case SDL_BUTTON_LEFT: return MouseButton::Left;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        case SDL_BUTTON_RIGHT: return MouseButton::Right;
        case SDL_BUTTON_X1: return MouseButton::X1;
        case SDL_BUTTON_X2: return MouseButton::X2;
        default: return std::nullopt;
    }
}

std::string_view button_name(MouseButton button) {
    switch (button) {
        case MouseButton::Left: return "ЛКМ";
        case MouseButton::Middle: return "СКМ";
        case MouseButton::Right: return "ПКМ";
        case MouseButton::X1: return "кнопка 4";
        case MouseButton::X2: return "кнопка 5";
    }
    return "?";
}

double to_ms(Nanoseconds ns) {
    return static_cast<double>(ns) / 1e6;
}

// Заголовок окна — проверка D-24 у автора (docs/MANUAL_CHECKS.md, сценарий 21):
// откуда взято время клика и насколько от него отстоит метка события SDL.
std::string describe_click(MouseButton button, float x, float y, Nanoseconds event_time,
                           const ClickTime& click) {
    const std::string text =
        std::format("{} — {} ({:.0f}, {:.0f}): ", kTitle, button_name(button), x, y);
    if (click.source == ClickTimeSource::Event) return text + "время из события SDL";
    const bool event_earlier = event_time <= click.time;
    const Nanoseconds gap = event_earlier ? click.time - event_time : event_time - click.time;
    return text + std::format("время из сырого ввода, метка SDL {} на {:.1f} мс",
                              event_earlier ? "раньше" : "позже", to_ms(gap));
}

bool save_screenshot(SDL_Renderer* renderer, const std::string& path) {
    SDL_Surface* frame = SDL_RenderReadPixels(renderer, nullptr);
    if (frame == nullptr) return false;
    const bool saved = SDL_SaveBMP(frame, path.c_str());
    SDL_DestroySurface(frame);
    return saved;
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::optional<Options> options = parse_options(argc, argv);
    if (!options) {
        SDL_Log("использование: AimTrainer [--frames N] [--screenshot файл.bmp]");
        return 2;
    }
    SDL_SetAppMetadata(kTitle, nullptr, "io.github.dpsmpx.aimtrainer");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer(kTitle, aim::core::kWindowWidth, aim::core::kWindowHeight, 0,
                                     &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int status = 0;
    {
        aim::platform::RawMouse raw_mouse;
        const std::string raw_state = raw_mouse.active() ? "есть" : "нет — " + raw_mouse.problem();
        SDL_Log("SDL %d.%d.%d, рендер %s; сырой ввод мыши: %s",
                SDL_VERSIONNUM_MAJOR(SDL_GetVersion()), SDL_VERSIONNUM_MINOR(SDL_GetVersion()),
                SDL_VERSIONNUM_MICRO(SDL_GetVersion()), SDL_GetRendererName(renderer),
                raw_state.c_str());
        SDL_SetWindowTitle(
            window, std::format("{} — каркас C++: сырой ввод мыши {}", kTitle, raw_state).c_str());

        ClickTimer clicks;
        std::vector<aim::core::RawPress> fresh;
        const auto take_raw = [&] {
            raw_mouse.drain(fresh);
            for (const auto& press : fresh)
                clicks.add(press);
            fresh.clear();
        };

        bool running = true;
        int frame = 0;
        while (running) {
            SDL_Event event;
            bool have_event = SDL_WaitEventTimeout(&event, kIdleWaitMs);
            while (have_event) {
                switch (event.type) {
                    case SDL_EVENT_QUIT: running = false; break;
                    case SDL_EVENT_KEY_DOWN:
                        if (event.key.key == SDLK_ESCAPE) running = false;
                        break;
                    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                        const std::optional<MouseButton> button = to_button(event.button.button);
                        if (!button) break;
                        // Касание и перо приходят как мышь, но сырого нажатия мыши
                        // у них нет (D-24).
                        const bool from_mouse = event.button.which != SDL_TOUCH_MOUSEID &&
                                                event.button.which != SDL_PEN_MOUSEID;
                        take_raw();
                        const ClickTime click =
                            from_mouse ? clicks.resolve(*button, event.button.timestamp)
                                       : ClickTime{event.button.timestamp, ClickTimeSource::Event};
                        SDL_SetWindowTitle(window,
                                           describe_click(*button, event.button.x, event.button.y,
                                                          event.button.timestamp, click)
                                               .c_str());
                        break;
                    }
                    default: break;
                }
                have_event = SDL_PollEvent(&event);
            }
            take_raw();
            clicks.forget_old(SDL_GetTicksNS());

            SDL_SetRenderDrawColor(renderer, kDarkBackground.r, kDarkBackground.g,
                                   kDarkBackground.b, SDL_ALPHA_OPAQUE);
            SDL_RenderClear(renderer);
            ++frame;
            const bool last = options->frames > 0 && frame >= options->frames;
            if (last && !options->screenshot.empty() &&
                !save_screenshot(renderer, options->screenshot)) {
                SDL_Log("снимок кадра не записан: %s", SDL_GetError());
                status = 1;
            }
            SDL_RenderPresent(renderer);
            if (last) running = false;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return status;
}
