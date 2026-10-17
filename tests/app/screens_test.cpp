// Все экраны программы в тестах (AUD-49): обход с заданными часами и
// случайными числами — туториал, пункты меню, панели, конструктор, каждый
// набор (отсчёт, замер, итог), реплей с итога и из «Рекордов наборов»,
// уведомление, меню без эффектов — на рендере software без экрана, обоими
// шрифтами (D-43). Каждая надпись помещается в свою рамку; каждый экран
// рисуется — ASan и UBSan видят отрисовку целиком.
//
// С AIM_SNAPSHOT_DIR кадры обхода — BMP в эту папку: сверка до и после
// правки отрисовки (tools/helpers/snapshots.sh).

#include "app/screens.h"

#include <SDL3/SDL.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/player_files.h"
#include "app/text.h"
#include "core/flow.h"
#include "core/layout.h"
#include "core/menu.h"
#include "core/profile.h"
#include "core/random.h"
#include "core/screen.h"
#include "platform/sound.h"
#include "platform/storage.h"

namespace aim::app {
namespace {

using core::Nanoseconds;
using core::Vec2;

constexpr Nanoseconds kFrame = core::ms(16);
constexpr core::LocalTime kWhen{.year = 2026, .month = 10, .day = 17, .hour = 12, .minute = 0};

Vec2 center(const core::Rect& r) {
    return {(r.x1 + r.x2) / 2.0, (r.y1 + r.y2) / 2.0};
}

// Окно без экрана и программный рендер — как app_offscreen_frames
// (tests/CMakeLists.txt); звук — драйвер dummy.
class Sdl {
public:
    Sdl() {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
        SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        ok_ = SDL_Init(SDL_INIT_VIDEO) &&
              SDL_CreateWindowAndRenderer("screens_test", core::kWindowWidth, core::kWindowHeight,
                                          0, &window_, &renderer_);
        if (ok_) SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    }
    ~Sdl() {
        if (renderer_ != nullptr) SDL_DestroyRenderer(renderer_);
        if (window_ != nullptr) SDL_DestroyWindow(window_);
        SDL_Quit();
    }
    Sdl(const Sdl&) = delete;
    Sdl& operator=(const Sdl&) = delete;

    [[nodiscard]] bool ok() const { return ok_; }
    [[nodiscard]] SDL_Window* window() const { return window_; }
    [[nodiscard]] SDL_Renderer* renderer() const { return renderer_; }

private:
    bool ok_ = false;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
};

// Программа без главного цикла: кадры — по команде теста, время — своё.
class Tour {
public:
    Tour(Sdl& sdl, Text& text, const std::string& dir, int font)
        : files_(platform::Storage(dir)),
          profile_(load(files_)),
          menu_(profile_, files_.profile_number(), !profile_.tutorial_seen),
          flow_(profile_, rng_, menu_, core::engine_random, [] { return kWhen; }),
          screens_(sdl.window(), sdl.renderer(), text, profile_, files_, rng_, menu_, sound_),
          renderer_(sdl.renderer()),
          font_(font) {
        if (const char* out = std::getenv("AIM_SNAPSHOT_DIR")) snapshots_ = out;
        screens_.check_text(true);
        screens_.start(now_);
    }

    /// Кадр через dt: ввод, логика, отрисовка (draw — рисовать ли), показ.
    void frame(Nanoseconds dt = kFrame, bool draw = true) {
        now_ += dt;
        screens_.before_input(flow_, mouse_, false, now_);
        flow_.input(inputs_, now_);
        inputs_.clear();
        flow_.update(now_, mouse_);
        screens_.after_update(flow_, mouse_, now_);
        if (draw) screens_.render(flow_, mouse_, 60, now_);
        flow_.frame_presented(now_, now_ + core::ms(1));
    }
    void click(Vec2 p, core::MouseButton b = core::MouseButton::Left) {
        mouse_ = p;
        inputs_.emplace_back(core::ClickInput{.button = b, .position = p, .time = now_ + 1});
        frame();
    }
    void key(core::Key k) {
        inputs_.emplace_back(core::KeyInput{.key = k, .time = now_ + 1});
        frame();
    }
    void move(Vec2 p) {
        mouse_ = p;
        frame();
    }
    void wait(Nanoseconds d) {
        for (Nanoseconds t = 0; t < d; t += kFrame)
            frame(kFrame, false);
        frame(0);
    }
    /// Кадр — в снимки; пустой кадр (одним цветом) — провал.
    void shot(const std::string& name) {
        SDL_Surface* s = SDL_RenderReadPixels(renderer_, nullptr);
        ASSERT_NE(s, nullptr) << SDL_GetError();
        EXPECT_FALSE(single_color(s)) << name << ": кадр одним цветом";
        if (!snapshots_.empty()) {
            const std::string path = std::format("{}/{}_{:02}_{}.bmp", snapshots_,
                                                 font_ == 1 ? "mono" : "condensed", ++shots_, name);
            EXPECT_TRUE(SDL_SaveBMP(s, path.c_str())) << SDL_GetError();
        }
        SDL_DestroySurface(s);
    }

    /// Замер ботом на время d: щелчки по цели (кнопкой её цвета, в стороне
    /// от центра), наведение, щелчки после смены цвета окна и один
    /// фальстарт.
    void play(Nanoseconds d) {
        bool false_start = false;
        for (Nanoseconds t = 0; t < d && flow_.screen() == core::Screen::Game; t += kFrame) {
            const core::Game& g = *flow_.game();
            if (g.screen_stimulus()) {
                if (!false_start) {
                    false_start = true;
                    inputs_.emplace_back(core::ClickInput{.position = {400, 300}, .time = now_});
                } else if (g.signal_lit(now_) && t % core::ms(320) < kFrame) {
                    inputs_.emplace_back(core::ClickInput{.position = {400, 300}, .time = now_});
                }
                frame(kFrame, false);
                continue;
            }
            const core::Target& target = g.target();
            const double r = target.radius_at(now_);
            const Vec2 c = target.center();
            if (g.options().action == core::Action::Hover) {
                mouse_ = c;
            } else if (!target.is_bomb() && r > 8 && target.shown_at() &&
                       t % core::ms(240) < kFrame) {
                const auto button =
                    target.color() == 1 ? core::MouseButton::Right : core::MouseButton::Left;
                mouse_ = {c.x + r * 0.4, c.y};
                inputs_.emplace_back(
                    core::ClickInput{.button = button, .position = mouse_, .time = now_});
            }
            frame(kFrame, t % (kFrame * 6) == 0);
        }
        frame();
    }

    core::Profile& profile() { return profile_; }
    core::Flow& flow() { return flow_; }
    Screens& screens() { return screens_; }

private:
    static core::Profile load(PlayerFiles& files) {
        std::vector<std::string> messages;
        return files.load(messages);
    }
    static bool single_color(SDL_Surface* s) {
        Uint8 r0 = 0, g0 = 0, b0 = 0, a0 = 0;
        SDL_ReadSurfacePixel(s, 0, 0, &r0, &g0, &b0, &a0);
        for (int y = 0; y < s->h; y += 7) {
            for (int x = 0; x < s->w; x += 7) {
                Uint8 r = 0, g = 0, b = 0, a = 0;
                SDL_ReadSurfacePixel(s, x, y, &r, &g, &b, &a);
                if (r != r0 || g != g0 || b != b0) return false;
            }
        }
        return true;
    }

    PlayerFiles files_;
    core::EngineRandom rng_{20261017};
    core::Profile profile_;
    core::Menu menu_;
    core::Flow flow_;
    platform::Sound sound_;
    Screens screens_;
    SDL_Renderer* renderer_;
    int font_;
    Nanoseconds now_ = core::ms(1'000);
    Vec2 mouse_{700, 560};
    std::vector<core::Input> inputs_;
    std::string snapshots_;
    int shots_ = 0;
};

class ScreensTest : public ::testing::TestWithParam<int> {};

TEST_P(ScreensTest, EveryScreenDrawsAndTextFits) {
    Sdl sdl;
    ASSERT_TRUE(sdl.ok()) << SDL_GetError();
    std::string problem;
    std::unique_ptr<Text> text = Text::open(sdl.renderer(), problem);
    ASSERT_NE(text, nullptr) << problem;
    const std::string dir = std::format("screens_test_{}/", GetParam());
    std::filesystem::remove_all(dir);
    ASSERT_TRUE(SDL_CreateDirectory(dir.c_str()));
    {
        Tour tour(sdl, *text, dir, GetParam());
        tour.profile().settings.font = GetParam();

        // Туториал первого запуска: четыре шага щелчком.
        for (int step = 1; step <= core::kTutorialSteps; ++step) {
            tour.frame();
            tour.shot(std::format("tutorial_{}", step));
            tour.click({400, 300});
        }
        tour.wait(core::ms(600));
        // Пункты меню — наведением; кнопки уже на месте.
        for (int i = 0; i < core::kMenuItems; ++i) {
            tour.move(center(core::preset_button(i, core::ms(10'000))));
            tour.shot(std::format("menu_item_{}", i));
        }
        // Панели.
        const std::vector<std::pair<core::MiniButton, std::string>> panels{
            {core::MiniButton::Help, "help"},       {core::MiniButton::Settings, "settings"},
            {core::MiniButton::Records, "records"}, {core::MiniButton::Achievements, "awards"},
            {core::MiniButton::History, "history"}, {core::MiniButton::Profile, "profiles"}};
        for (const auto& [button, name] : panels) {
            tour.click(center(core::mini_button(button)));
            tour.shot("panel_" + name);
            if (button == core::MiniButton::Help) {
                for (int i = 0; i < 3; ++i)
                    tour.key(core::Key::PageDown);
                tour.shot("panel_help_end");
            }
            tour.key(core::Key::Escape);
        }
        // Конструктор: как есть и со сменой цвета окна.
        tour.click(center(core::preset_button(core::kCustomItem, core::ms(10'000))));
        tour.shot("constructor");
        tour.click(center(core::constructor_row(0)));
        tour.shot("constructor_screen");
        tour.click(center(core::constructor_row(0)));
        tour.key(core::Key::Escape);

        // Наборы: отсчёт, замер ботом, итог; у «Классики» — реплей с итога.
        for (int i = 0; i < core::kPresetCount; ++i) {
            // Кнопки наборов появляются заново после входа в меню.
            tour.wait(core::ms(600));
            tour.click(center(core::preset_button(i, core::ms(10'000))));
            tour.wait(core::ms(500));
            tour.shot(std::format("preset_{}_countdown", i));
            tour.wait(core::kCountdown);
            ASSERT_EQ(tour.flow().screen(), core::Screen::Game) << i;
            tour.play(core::ms(3'000));
            tour.shot(std::format("preset_{}_game", i));
            tour.key(core::Key::Escape);
            ASSERT_EQ(tour.flow().screen(), core::Screen::GameOver) << i;
            tour.shot(std::format("preset_{}_over_early", i));
            tour.wait(core::kResultButtonsDelay);
            tour.shot(std::format("preset_{}_over", i));
            if (i == 1) {
                ASSERT_TRUE(tour.flow().replay_offered());
                tour.click(center(core::result_button(core::ResultButton::Replay, true)));
                tour.wait(core::ms(1'500));
                tour.shot("preset_1_replay");
                tour.key(core::Key::Escape);
                tour.shot("preset_1_over_back");
            }
            tour.click(center(
                core::result_button(core::ResultButton::Menu, tour.flow().replay_offered())));
            ASSERT_EQ(tour.flow().screen(), core::Screen::Menu) << i;
        }
        // Рекорды после замеров и реплей рекорда из них.
        tour.wait(core::ms(600));
        tour.click(center(core::mini_button(core::MiniButton::Records)));
        tour.shot("records_after");
        tour.click(center(core::records_row(0)));
        tour.wait(core::ms(1'500));
        tour.shot("replay");
        tour.key(core::Key::Escape);
        tour.key(core::Key::Escape);
        // История с замерами и уведомление поверх панели.
        tour.click(center(core::mini_button(core::MiniButton::History)));
        tour.shot("history");
        tour.screens().message(
            "Проверка уведомления: длинный текст переносится и не выходит за окно — "
            "D:\\Очень\\длинная\\папка\\сохранений\\AimTrainer\\");
        tour.frame();  // уведомление появляется с первого кадра, где нарисовано
        tour.wait(core::ms(400));
        tour.shot("history_toast");
        tour.key(core::Key::Escape);
        // Эффекты меню выключены (D-47).
        tour.click(center(core::mini_button(core::MiniButton::Settings)));
        tour.click(center(core::settings_toggle(core::kSettingsMenuEffectsRow)));
        tour.shot("settings_effects_off");
        tour.key(core::Key::Escape);
        tour.shot("menu_effects_off");

        tour.screens().shutdown();
        EXPECT_EQ(tour.screens().clipped_texts(), 0U) << "надписи не поместились — журнал теста";
    }
    std::filesystem::remove_all(dir);
}

std::string font_name(const ::testing::TestParamInfo<int>& font) {
    return font.param == 1 ? "Mono" : "Condensed";
}

INSTANTIATE_TEST_SUITE_P(BothFonts, ScreensTest, ::testing::Values(0, 1), font_name);

}  // namespace
}  // namespace aim::app
