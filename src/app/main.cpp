// Aim Trainer, C++-версия (D-08, D-25): окно 800×600 (D-02) и главный цикл.
// Шаг «Время и ввод»: часы кадра — SDL_GetTicksNS; щелчок — точка из события
// SDL и время из своего потока сырого ввода (D-24); клавиши — события, по
// физическому положению; реакция — от показа кадра с целью (D-12). Экраны —
// app/screens.h; профиль и история — в папке игрока (D-11, D-30;
// app/player_files.h).
//
//   AimTrainer [--frames N] [--screenshot файл.bmp]
//
// --frames N — выйти после N кадров; --screenshot — записать последний кадр в
// BMP. Оба — для проверки без экрана (tests/CMakeLists.txt).

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "app/player_files.h"
#include "app/screens.h"
#include "app/text.h"
#include "core/click_time.h"
#include "core/flow.h"
#include "core/menu.h"
#include "core/profile.h"
#include "core/random.h"
#include "core/screen.h"
#include "platform/raw_mouse.h"
#include "platform/sound.h"
#include "platform/storage.h"

namespace {

using aim::core::ClickTime;
using aim::core::ClickTimer;
using aim::core::ClickTimeSource;
using aim::core::MouseButton;
using aim::core::Nanoseconds;

constexpr const char* kTitle = "Aim Trainer";
// Сырое нажатие и событие SDL приходят в разные потоки одновременно, и
// изредка событие разбирается раньше, чем поток сырого ввода поставил метку
// (под Wine — 2 клика из 300, метка позже на 0,1–0,9 мс). Тогда сырое нажатие
// ждём, но не дольше этого и пока оно ещё может попасть в окно сопоставления.
constexpr Nanoseconds kRawWaitNs = 2'000'000;
// Предел частоты кадров — как у образца (TARGET_FPS = 240, LimitFrameRate):
// цикл не жжёт ядро впустую, а кадр короче, чем у любого монитора.
constexpr Nanoseconds kMinFrame = 1'000'000'000 / 240;

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

// Клавиши — по физическому положению (scancode): R и M работают и в
// русской раскладке.
std::optional<aim::core::Key> to_key(SDL_Scancode scancode) {
    switch (scancode) {
        case SDL_SCANCODE_ESCAPE: return aim::core::Key::Escape;
        case SDL_SCANCODE_SPACE: return aim::core::Key::Space;
        case SDL_SCANCODE_R: return aim::core::Key::R;
        case SDL_SCANCODE_M: return aim::core::Key::M;
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

// Частота кадров для HUD: кадров за последнюю целую секунду.
class FpsCounter {
public:
    void frame(Nanoseconds now) {
        ++count_;
        if (now - since_ >= 1'000'000'000) {
            fps_ = count_;
            count_ = 0;
            since_ = now;
        }
    }
    [[nodiscard]] int fps() const { return fps_; }

private:
    Nanoseconds since_ = 0;
    int count_ = 0;
    int fps_ = 0;
};

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
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    std::string font_problem;
    std::unique_ptr<aim::app::Text> text = aim::app::Text::open(renderer, font_problem);
    if (!text) {
        // Без шрифта программе нечего показать (D-31): сказать и выйти.
        SDL_Log("%s", font_problem.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, font_problem.c_str(), window);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
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
        SDL_SetWindowTitle(window,
                           std::format("{} — C++: сырой ввод мыши {}", kTitle, raw_state).c_str());

        std::string problem;
        std::optional<aim::platform::Storage> storage =
            aim::platform::Storage::default_folder(problem);
        SDL_Log("папка по умолчанию: %s", storage ? storage->dir().c_str() : problem.c_str());
        aim::app::PlayerFiles files(std::move(storage), problem);
        std::vector<std::string> messages;
        aim::core::Profile profile = files.load(messages);
        if (files.available()) SDL_Log("папка сохранений: %s", files.folder().c_str());

        aim::core::EngineRandom rng(SDL_GetPerformanceCounter());
        // Туториал — при запуске, если профиль его ещё не видел (как образец).
        aim::core::Menu menu(profile, files.profile_number(), !profile.tutorial_seen);
        aim::core::Flow flow(profile, rng, menu, aim::core::engine_random, aim::app::local_now);
        aim::platform::Sound sound;
        if (!sound.active()) SDL_Log("звука нет: %s", sound.problem().c_str());
        aim::app::Screens screens(window, renderer, *text, profile, files, rng, menu, sound);
        screens.start(SDL_GetTicksNS());
        for (std::string& m : messages)
            screens.message(std::move(m));

        ClickTimer clicks;
        std::vector<aim::core::RawPress> fresh;
        const auto take_raw = [&] {
            raw_mouse.drain(fresh);
            for (const auto& press : fresh)
                clicks.add(press);
            fresh.clear();
        };
        const auto click_time = [&](MouseButton button, Nanoseconds event_time) {
            take_raw();
            ClickTime click = clicks.resolve(button, event_time);
            const Nanoseconds deadline =
                std::min(SDL_GetTicksNS() + kRawWaitNs, event_time + ClickTimer::kRawAfterEvent);
            for (Nanoseconds now = SDL_GetTicksNS();
                 click.source == ClickTimeSource::Event && raw_mouse.active() && now < deadline;
                 now = SDL_GetTicksNS()) {
                raw_mouse.wait_for_press(deadline - now);
                take_raw();
                click = clicks.resolve(button, event_time);
            }
            return click;
        };

        FpsCounter fps;
        std::vector<aim::core::Input> inputs;
        bool running = true;
        int frame = 0;
        while (running) {
            const Nanoseconds frame_start = SDL_GetTicksNS();
            inputs.clear();
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_EVENT_QUIT: running = false; break;
                    case SDL_EVENT_KEY_DOWN: {
                        if (event.key.repeat) break;
                        if (const auto key = to_key(event.key.scancode))
                            inputs.emplace_back(
                                aim::core::KeyInput{.key = *key, .time = event.key.timestamp});
                        break;
                    }
                    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                        const std::optional<MouseButton> button = to_button(event.button.button);
                        if (!button) break;
                        // Касание и перо приходят как мышь, но сырого нажатия мыши
                        // у них нет (D-24).
                        const bool from_mouse = event.button.which != SDL_TOUCH_MOUSEID &&
                                                event.button.which != SDL_PEN_MOUSEID;
                        const ClickTime click =
                            from_mouse ? click_time(*button, event.button.timestamp)
                                       : ClickTime{event.button.timestamp, ClickTimeSource::Event};
                        SDL_SetWindowTitle(window,
                                           describe_click(*button, event.button.x, event.button.y,
                                                          event.button.timestamp, click)
                                               .c_str());
                        inputs.emplace_back(
                            aim::core::ClickInput{.button = *button,
                                                  .position = {event.button.x, event.button.y},
                                                  .time = click.time});
                        break;
                    }
                    default: break;
                }
            }
            take_raw();

            const Nanoseconds now = SDL_GetTicksNS();
            clicks.forget_old(now);
            float mx = 0;
            float my = 0;
            SDL_GetMouseState(&mx, &my);
            const aim::core::Vec2 mouse{mx, my};
            screens.before_input(flow, mouse, now);
            flow.input(inputs, now);
            flow.update(now, mouse);
            screens.after_update(flow, mouse, now);
            screens.render(flow, mouse, fps.fps(), now);
            ++frame;
            const bool last = options->frames > 0 && frame >= options->frames;
            if (last && !options->screenshot.empty() &&
                !save_screenshot(renderer, options->screenshot)) {
                SDL_Log("снимок кадра не записан: %s", SDL_GetError());
                status = 1;
            }
            SDL_RenderPresent(renderer);
            text->end_frame();
            // Кадр нарисован на момент now и показан сейчас: отсюда реакция (D-12).
            flow.frame_presented(now, SDL_GetTicksNS());
            fps.frame(now);
            if (last) running = false;

            const Nanoseconds spent = SDL_GetTicksNS() - frame_start;
            if (spent < kMinFrame) SDL_DelayPrecise(kMinFrame - spent);
        }
    }

    text.reset();  // знаки шрифта — текстуры рендера: раньше него
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return status;
}
