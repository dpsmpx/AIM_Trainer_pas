#include "app/app.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <charconv>
#include <cmath>
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
#include "core/frame_budget.h"
#include "core/menu.h"
#include "core/profile.h"
#include "core/random.h"
#include "core/screen.h"
#include "core/time.h"
#include "platform/raw_mouse.h"
#include "platform/sound.h"
#include "platform/storage.h"

namespace aim::app {

namespace {

using core::ClickTime;
using core::ClickTimer;
using core::ClickTimeSource;
using core::MouseButton;
using core::Nanoseconds;

constexpr const char* kWindowTitle = "DP AIM Trainer";
// Сырое нажатие и событие SDL приходят в разные потоки одновременно, и
// изредка событие разбирается раньше, чем поток сырого ввода поставил метку
// (под Wine — 2 клика из 300, метка позже на 0,1–0,9 мс). Тогда сырое нажатие
// ждём, но не дольше этого и пока оно ещё может попасть в окно сопоставления.
constexpr Nanoseconds kRawWaitNs = 2'000'000;
// Частота монитора неизвестна — кадр как у монитора 60 Гц (D-41).
constexpr float kFallbackRefreshHz = 60.0F;

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
std::optional<core::Key> to_key(SDL_Scancode scancode) {
    switch (scancode) {
        case SDL_SCANCODE_ESCAPE: return core::Key::Escape;
        case SDL_SCANCODE_SPACE: return core::Key::Space;
        case SDL_SCANCODE_R: return core::Key::R;
        case SDL_SCANCODE_M: return core::Key::M;
        case SDL_SCANCODE_UP: return core::Key::Up;
        case SDL_SCANCODE_DOWN: return core::Key::Down;
        case SDL_SCANCODE_PAGEUP: return core::Key::PageUp;
        case SDL_SCANCODE_PAGEDOWN: return core::Key::PageDown;
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

// Заголовок окна — проверка D-24 у автора (docs/MANUAL_CHECKS.md, сценарий 21):
// откуда взято время клика и насколько от него отстоит метка события SDL.
std::string describe_click(MouseButton button, float x, float y, Nanoseconds event_time,
                           const ClickTime& click) {
    const std::string text =
        std::format("{} — {} ({:.0f}, {:.0f}): ", kWindowTitle, button_name(button), x, y);
    if (click.source == ClickTimeSource::Event) return text + "время из события SDL";
    const bool event_earlier = event_time <= click.time;
    const Nanoseconds gap = event_earlier ? click.time - event_time : event_time - click.time;
    return text + std::format("время из сырого ввода, метка SDL {} на {:.1f} мс",
                              event_earlier ? "раньше" : "позже", core::to_ms(gap));
}

// Масштаб монитора для окна при запуске (D-42): масштаб главного монитора,
// но окно не больше рабочей области; меньше 1 — не уменьшать.
float startup_scale() {
    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    float scale = SDL_GetDisplayContentScale(display);
    if (!(scale > 0.0F)) scale = 1.0F;
    SDL_Rect usable{};
    if (SDL_GetDisplayUsableBounds(display, &usable) && usable.w > 0 && usable.h > 0) {
        const float fit_w = 0.95F * static_cast<float>(usable.w) / core::kWindowWidth;
        const float fit_h = 0.90F * static_cast<float>(usable.h) / core::kWindowHeight;
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

// Окно и рендер SDL; с ними закрывается и SDL — последним.
struct SdlWindow {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    SdlWindow() = default;
    SdlWindow(const SdlWindow&) = delete;
    SdlWindow& operator=(const SdlWindow&) = delete;
    SdlWindow(SdlWindow&&) = delete;
    SdlWindow& operator=(SdlWindow&&) = delete;
    ~SdlWindow() {
        if (renderer != nullptr) SDL_DestroyRenderer(renderer);
        if (window != nullptr) SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

// Папка по умолчанию и файлы игрока (D-30, D-33).
PlayerFiles open_files() {
    std::string problem;
    std::optional<platform::Storage> storage = platform::Storage::default_folder(problem);
    SDL_Log("папка по умолчанию: %s", storage ? storage->dir().c_str() : problem.c_str());
    return PlayerFiles(std::move(storage), problem);
}

// Программа в главном цикле: события, кадр, темп кадров.
class App {
public:
    App(const Options& options, SDL_Window* window, SDL_Renderer* renderer, Text& text,
        float scale);
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;
    ~App() = default;

    /// Цикл до закрытия окна или --frames; код выхода.
    int loop();

private:
    /// Очередь SDL — в ввод кадра inputs_.
    void poll_events();
    /// Нажатие кнопки мыши: точка — в логических 800×600, время — D-24.
    void on_mouse_down(SDL_Event& event);
    /// Сырые нажатия из потока — в сопоставление.
    void take_raw();
    /// Время щелчка: сырое нажатие той же кнопки рядом с меткой SDL; не
    /// пришло — ждать его не дольше kRawWaitNs (docs/ARCHITECTURE.md, §12).
    ClickTime click_time(MouseButton button, Nanoseconds event_time);
    /// Ввод, логика, отрисовка и показ кадра.
    void frame();
    /// Запас кадра в замере (D-41): работа кадра — от разбора событий до
    /// показа; по концу замера — сводка в журнал и, с --debug-title, в
    /// заголовок.
    void track_budget(Nanoseconds frame_begin);
    /// Следующий кадр — через период монитора от прошлого (D-41); отстали
    /// больше чем на кадр — не догонять пачкой.
    void pace();

    const Options& options_;
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    Text& text_;
    platform::RawMouse raw_mouse_;
    float hz_ = 0;
    Nanoseconds period_ = 0;
    std::vector<std::string> messages_;  // для игрока при запуске
    PlayerFiles files_;
    core::Profile profile_;
    core::EngineRandom rng_;
    core::Menu menu_;
    core::Flow flow_;
    platform::Sound sound_;
    Screens screens_;
    ClickTimer clicks_;
    std::vector<core::RawPress> fresh_;
    FpsCounter fps_;
    std::vector<core::Input> inputs_;
    bool running_ = true;
    int frame_ = 0;
    int status_ = 0;
    Nanoseconds next_frame_ = 0;
    core::FrameBudget budget_;  // работа кадров замера (D-41)
    bool measuring_ = false;
};

App::App(const Options& options, SDL_Window* window, SDL_Renderer* renderer, Text& text,
         float scale)
    : options_(options),
      window_(window),
      renderer_(renderer),
      text_(text),
      period_(frame_period(window, hz_)),
      files_(open_files()),
      profile_(files_.load(messages_)),
      rng_(SDL_GetPerformanceCounter()),
      // Туториал — при запуске, если профиль его ещё не видел.
      menu_(profile_, files_.profile_number(), !profile_.tutorial_seen),
      flow_(profile_, rng_, menu_, core::engine_random, local_now),
      screens_(window, renderer, text, profile_, files_, rng_, menu_, sound_) {
    const std::string raw_state = raw_mouse_.active() ? "есть" : "нет — " + raw_mouse_.problem();
    SDL_Log("SDL %d.%d.%d, рендер %s; сырой ввод мыши: %s; масштаб %.2f; монитор %.1f Гц",
            SDL_VERSIONNUM_MAJOR(SDL_GetVersion()), SDL_VERSIONNUM_MINOR(SDL_GetVersion()),
            SDL_VERSIONNUM_MICRO(SDL_GetVersion()), SDL_GetRendererName(renderer),
            raw_state.c_str(), static_cast<double>(scale), static_cast<double>(hz_));
    if (options.debug_title)
        SDL_SetWindowTitle(
            window, std::format("{} — C++: сырой ввод мыши {}", kWindowTitle, raw_state).c_str());
    if (files_.available()) SDL_Log("папка сохранений: %s", files_.folder().c_str());
    if (!sound_.active()) SDL_Log("звука нет: %s", sound_.problem().c_str());
    screens_.start(SDL_GetTicksNS());
    screens_.check_text(options.text_check);
    for (std::string& m : messages_)
        screens_.message(std::move(m));
    messages_.clear();
    next_frame_ = SDL_GetTicksNS();
}

int App::loop() {
    while (running_) {
        const Nanoseconds frame_begin = SDL_GetTicksNS();
        poll_events();
        frame();
        track_budget(frame_begin);
        pace();
    }
    screens_.shutdown();
    if (options_.text_check && screens_.clipped_texts() > 0) {
        SDL_Log("не поместилось надписей: %zu", screens_.clipped_texts());
        if (status_ == 0) status_ = 3;
    }
    return status_;
}

void App::take_raw() {
    raw_mouse_.drain(fresh_);
    for (const auto& press : fresh_)
        clicks_.add(press);
    fresh_.clear();
}

ClickTime App::click_time(MouseButton button, Nanoseconds event_time) {
    take_raw();
    ClickTime click = clicks_.resolve(button, event_time);
    const Nanoseconds deadline =
        std::min(SDL_GetTicksNS() + kRawWaitNs, event_time + ClickTimer::kRawAfterEvent);
    for (Nanoseconds now = SDL_GetTicksNS();
         click.source == ClickTimeSource::Event && raw_mouse_.active() && now < deadline;
         now = SDL_GetTicksNS()) {
        raw_mouse_.wait_for_press(deadline - now);
        take_raw();
        click = clicks_.resolve(button, event_time);
    }
    return click;
}

void App::on_mouse_down(SDL_Event& event) {
    // Точка — в логических 800×600 (D-42).
    SDL_ConvertEventToRenderCoordinates(renderer_, &event);
    const std::optional<MouseButton> button = to_button(event.button.button);
    if (!button) return;
    // Касание и перо приходят как мышь, но сырого нажатия мыши у них нет
    // (D-24).
    const bool from_mouse =
        event.button.which != SDL_TOUCH_MOUSEID && event.button.which != SDL_PEN_MOUSEID;
    const ClickTime click = from_mouse ? click_time(*button, event.button.timestamp)
                                       : ClickTime{event.button.timestamp, ClickTimeSource::Event};
    if (options_.debug_title)
        SDL_SetWindowTitle(window_, describe_click(*button, event.button.x, event.button.y,
                                                   event.button.timestamp, click)
                                        .c_str());
    inputs_.emplace_back(core::ClickInput{
        .button = *button, .position = {event.button.x, event.button.y}, .time = click.time});
}

void App::poll_events() {
    inputs_.clear();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT: running_ = false; break;
            case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
            case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
                // Другой монитор или режим — другая частота (D-41).
                period_ = frame_period(window_, hz_);
                SDL_Log("монитор %.1f Гц", static_cast<double>(hz_));
                break;
            case SDL_EVENT_MOUSE_WHEEL: {
                float y = event.wheel.y;
                if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) y = -y;
                inputs_.emplace_back(core::WheelInput{.y = y, .time = event.wheel.timestamp});
                break;
            }
            case SDL_EVENT_KEY_DOWN: {
                // Автоповтор — только у стрелок: справка листается удержанием.
                const bool arrows = event.key.scancode == SDL_SCANCODE_UP ||
                                    event.key.scancode == SDL_SCANCODE_DOWN;
                if (event.key.repeat && !arrows) break;
                if (const auto key = to_key(event.key.scancode))
                    inputs_.emplace_back(core::KeyInput{.key = *key, .time = event.key.timestamp});
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN: on_mouse_down(event); break;
            default: break;
        }
    }
    take_raw();
}

void App::frame() {
    const Nanoseconds now = SDL_GetTicksNS();
    clicks_.forget_old(now);
    float wx = 0;
    float wy = 0;
    const SDL_MouseButtonFlags held = SDL_GetMouseState(&wx, &wy);
    float mx = wx;
    float my = wy;
    SDL_RenderCoordinatesFromWindow(renderer_, wx, wy, &mx, &my);
    const core::Vec2 mouse{mx, my};
    screens_.before_input(flow_, mouse, (held & SDL_BUTTON_LMASK) != 0, now);
    flow_.input(inputs_, now);
    flow_.update(now, mouse);
    screens_.after_update(flow_, mouse, now);
    screens_.render(flow_, mouse, fps_.fps(), now);
    ++frame_;
    const bool last = options_.frames > 0 && frame_ >= options_.frames;
    if (last && !options_.screenshot.empty() && !save_screenshot(renderer_, options_.screenshot)) {
        SDL_Log("снимок кадра не записан: %s", SDL_GetError());
        status_ = 1;
    }
    SDL_RenderPresent(renderer_);
    text_.end_frame();
    // Кадр нарисован на момент now и показан сейчас: отсюда реакция (D-12).
    flow_.frame_presented(now, SDL_GetTicksNS());
    fps_.frame(now);
    if (last) running_ = false;
}

void App::track_budget(Nanoseconds frame_begin) {
    const bool in_run = flow_.screen() == core::Screen::Game;
    if (in_run && !measuring_) budget_.start(period_);
    if (in_run) budget_.add(SDL_GetTicksNS() - frame_begin);
    if (!in_run && measuring_ && !budget_.empty()) {
        const std::string report = core::describe(budget_.report());
        SDL_Log("%s", report.c_str());
        if (options_.debug_title)
            SDL_SetWindowTitle(window_, std::format("{} — {}", kWindowTitle, report).c_str());
    }
    measuring_ = in_run;
}

void App::pace() {
    next_frame_ += period_;
    const Nanoseconds after = SDL_GetTicksNS();
    if (after < next_frame_) {
        SDL_DelayPrecise(next_frame_ - after);
    } else if (after - next_frame_ > period_) {
        next_frame_ = after;
    }
}

}  // namespace

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
        } else if (arg == "--text-check") {
            options.text_check = true;
        } else {
            return std::nullopt;
        }
    }
    return options;
}

int run(const Options& options) {
    SDL_SetAppMetadata(kWindowTitle, nullptr, "io.github.dpsmpx.aimtrainer");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }
    SdlWindow sdl;
    const float scale = startup_scale();
    const auto scaled = [&](int v) {
        return static_cast<int>(std::lround(static_cast<float>(v) * scale));
    };
    if (!SDL_CreateWindowAndRenderer(kWindowTitle, scaled(core::kWindowWidth),
                                     scaled(core::kWindowHeight), 0, &sdl.window, &sdl.renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
        return 1;
    }
    SDL_SetRenderDrawBlendMode(sdl.renderer, SDL_BLENDMODE_BLEND);
    // Рисуется логическое 800×600; масштаб — у рендера, у текста — свой.
    SDL_SetRenderScale(sdl.renderer, scale, scale);
    std::string font_problem;
    // Знаки шрифта — текстуры рендера: текст закрывается раньше окна.
    std::unique_ptr<Text> text = Text::open(sdl.renderer, font_problem);
    if (!text) {
        // Без шрифта программе нечего показать (D-31): сказать и выйти.
        SDL_Log("%s", font_problem.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kWindowTitle, font_problem.c_str(),
                                 sdl.window);
        return 1;
    }
    text->set_scale(scale);
    App app(options, sdl.window, sdl.renderer, *text, scale);
    return app.loop();
}

}  // namespace aim::app
