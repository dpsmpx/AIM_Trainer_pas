// DP AIM Trainer (D-08, D-25): окно и главный цикл. Окно — логические
// 800×600, умноженные на масштаб монитора при запуске (D-42); кадр — с
// частотой монитора (D-41); часы кадра — SDL_GetTicksNS; щелчок — точка из
// события SDL и время из своего потока сырого ввода (D-24); клавиши —
// события, по физическому положению; реакция — от показа кадра со стимулом
// (D-12). Экраны — app/screens.h; файлы игрока — app/player_files.h.
//
//   AimTrainer [--frames N] [--screenshot файл.bmp] [--debug-title]
//
// --frames N — выйти после N кадров; --screenshot — записать последний кадр в
// BMP. Оба — для проверки без экрана (tests/CMakeLists.txt). --debug-title —
// в заголовке окна откуда взято время щелчка (сценарий 21, D-24).

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

constexpr const char* kTitle = "DP AIM Trainer";
// Сырое нажатие и событие SDL приходят в разные потоки одновременно, и
// изредка событие разбирается раньше, чем поток сырого ввода поставил метку
// (под Wine — 2 клика из 300, метка позже на 0,1–0,9 мс). Тогда сырое нажатие
// ждём, но не дольше этого и пока оно ещё может попасть в окно сопоставления.
constexpr Nanoseconds kRawWaitNs = 2'000'000;
// Частота монитора неизвестна — кадр как у монитора 60 Гц (D-41).
constexpr float kFallbackRefreshHz = 60.0F;

struct Options {
    int frames = 0;  // 0 — до закрытия окна
    std::string screenshot;
    bool debug_title = false;
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
        } else if (arg == "--debug-title") {
            options.debug_title = true;
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

// Масштаб монитора для окна при запуске (D-42): масштаб главного монитора,
// но окно не больше рабочей области; меньше 1 — не уменьшать.
float startup_scale() {
    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    float scale = SDL_GetDisplayContentScale(display);
    if (!(scale > 0.0F)) scale = 1.0F;
    SDL_Rect usable{};
    if (SDL_GetDisplayUsableBounds(display, &usable) && usable.w > 0 && usable.h > 0) {
        const float fit_w = 0.95F * static_cast<float>(usable.w) / aim::core::kWindowWidth;
        const float fit_h = 0.90F * static_cast<float>(usable.h) / aim::core::kWindowHeight;
        scale = std::min({scale, fit_w, fit_h});
    }
    return std::max(scale, 1.0F);
}

// Период кадра — частота монитора, на котором окно (D-41).
Nanoseconds frame_period(SDL_Window* window, float& hz) {
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
    hz = mode != nullptr && mode->refresh_rate > 0.0F ? mode->refresh_rate : kFallbackRefreshHz;
    return static_cast<Nanoseconds>(1e9 / static_cast<double>(hz));
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
    const float scale = startup_scale();
    const auto scaled = [&](int v) {
        return static_cast<int>(std::lround(static_cast<float>(v) * scale));
    };
    if (!SDL_CreateWindowAndRenderer(kTitle, scaled(aim::core::kWindowWidth),
                                     scaled(aim::core::kWindowHeight), 0, &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    // Рисуется логическое 800×600; масштаб — у рендера, у текста — свой.
    SDL_SetRenderScale(renderer, scale, scale);
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
    text->set_scale(scale);

    int status = 0;
    {
        aim::platform::RawMouse raw_mouse;
        const std::string raw_state = raw_mouse.active() ? "есть" : "нет — " + raw_mouse.problem();
        float hz = 0;
        Nanoseconds period = frame_period(window, hz);
        SDL_Log("SDL %d.%d.%d, рендер %s; сырой ввод мыши: %s; масштаб %.2f; монитор %.1f Гц",
                SDL_VERSIONNUM_MAJOR(SDL_GetVersion()), SDL_VERSIONNUM_MINOR(SDL_GetVersion()),
                SDL_VERSIONNUM_MICRO(SDL_GetVersion()), SDL_GetRendererName(renderer),
                raw_state.c_str(), static_cast<double>(scale), static_cast<double>(hz));
        if (options->debug_title)
            SDL_SetWindowTitle(
                window, std::format("{} — C++: сырой ввод мыши {}", kTitle, raw_state).c_str());

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
        Nanoseconds next_frame = SDL_GetTicksNS();
        while (running) {
            inputs.clear();
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_EVENT_QUIT: running = false; break;
                    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
                    case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
                        // Другой монитор или режим — другая частота (D-41).
                        period = frame_period(window, hz);
                        SDL_Log("монитор %.1f Гц", static_cast<double>(hz));
                        break;
                    case SDL_EVENT_KEY_DOWN: {
                        if (event.key.repeat) break;
                        if (const auto key = to_key(event.key.scancode))
                            inputs.emplace_back(
                                aim::core::KeyInput{.key = *key, .time = event.key.timestamp});
                        break;
                    }
                    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                        // Точка — в логических 800×600 (D-42).
                        SDL_ConvertEventToRenderCoordinates(renderer, &event);
                        const std::optional<MouseButton> button = to_button(event.button.button);
                        if (!button) break;
                        // Касание и перо приходят как мышь, но сырого нажатия мыши
                        // у них нет (D-24).
                        const bool from_mouse = event.button.which != SDL_TOUCH_MOUSEID &&
                                                event.button.which != SDL_PEN_MOUSEID;
                        const ClickTime click =
                            from_mouse ? click_time(*button, event.button.timestamp)
                                       : ClickTime{event.button.timestamp, ClickTimeSource::Event};
                        if (options->debug_title)
                            SDL_SetWindowTitle(
                                window, describe_click(*button, event.button.x, event.button.y,
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
            float wx = 0;
            float wy = 0;
            SDL_GetMouseState(&wx, &wy);
            float mx = wx;
            float my = wy;
            SDL_RenderCoordinatesFromWindow(renderer, wx, wy, &mx, &my);
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

            // Следующий кадр — через период монитора от прошлого (D-41);
            // отстали больше чем на кадр — не догонять пачкой.
            next_frame += period;
            const Nanoseconds after = SDL_GetTicksNS();
            if (after < next_frame) {
                SDL_DelayPrecise(next_frame - after);
            } else if (after - next_frame > period) {
                next_frame = after;
            }
        }
    }

    text.reset();  // знаки шрифта — текстуры рендера: раньше него
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return status;
}
