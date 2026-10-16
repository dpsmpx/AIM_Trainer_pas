#include "app/screens.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <variant>

#include "app/game_view.h"
#include "core/history.h"
#include "core/palette.h"
#include "core/rounding.h"
#include "core/rules.h"

namespace aim::app {

namespace {

template <class... Fs>
struct Overload : Fs... {
    using Fs::operator()...;
};

}  // namespace

Screens::Screens(SDL_Renderer* renderer, Text& text, core::Profile& profile, PlayerFiles& files,
                 core::Random& rng, core::Menu& menu)
    : canvas_(renderer, text), profile_(profile), files_(files), rng_(rng), menu_(menu) {
    // Как образец при запуске: шрифт Consolas 14 (здесь — DejaVu, D-22).
    canvas_.font.size = 14;
}

void Screens::start(core::Nanoseconds now) {
    menu_.enter(now);
    menu_view_.enter(rng_, now);
}

const core::Theme& Screens::theme() const {
    return core::kThemes.at(static_cast<std::size_t>(std::clamp(profile_.settings.theme, 0, 2)));
}

void Screens::save() {
    std::string error;
    if (!files_.save(profile_, error)) message("Профиль не записан: " + error);
}

void Screens::open_history() {
    const std::optional<std::string> csv = files_.read_history();
    const std::vector<core::SessionRow> rows =
        csv ? core::read_sessions(*csv) : std::vector<core::SessionRow>{};
    history_ = {.games = static_cast<int>(rows.size()),
                .by_time_of_day = core::scores_by_time_of_day(rows),
                .recent_reactions = core::recent_avg_reactions(rows),
                .file_name = files_.history_file_name()};
}

void Screens::before_input(const core::Flow& flow, core::Vec2 mouse, core::Nanoseconds now) {
    if (flow.screen() == core::Screen::Menu) menu_.hover(mouse, now);
}

void Screens::after_update(core::Flow& flow, core::Vec2 mouse, core::Nanoseconds now) {
    const core::Theme& th = theme();
    for (const core::FlowEvent& e : flow.take_events()) {
        std::visit(
            Overload{
                [&](const core::ScreenChanged& s) {
                    if (s.to == core::Screen::Menu) {
                        menu_.enter(now);
                        menu_view_.enter(rng_, now);
                    } else if (s.to == core::Screen::Game) {
                        // InitGameState образца: эффекты и след — заново.
                        effects_.clear();
                        trail_.clear();
                    }
                },
                [&](const core::GameEvent& g) {
                    const int x = core::round_half_even(g.position.x);
                    const int y = core::round_half_even(g.position.y);
                    const core::Rgba color = th.circles.at(static_cast<std::size_t>(g.color));
                    switch (g.kind) {
                        case core::GameEventKind::Hit:
                            effects_.hit(x, y, color, g.points, rng_, now);
                            if (g.accuracy >= core::kPreciseHit) effects_.precise_flash(now);
                            break;
                        case core::GameEventKind::Tracked:
                            effects_.hit(x, y, color, g.points, rng_, now);
                            break;
                        case core::GameEventKind::Bomb: effects_.bomb_flash(now); break;
                        case core::GameEventKind::Miss:
                        case core::GameEventKind::Expired: break;
                    }
                },
                [&](const core::AchievementUnlocked& a) {
                    message("Достижение: " +
                            std::string(core::kAchievements.at(static_cast<std::size_t>(a.id)).name));
                    // Как образец: достижение записывается сразу.
                    save();
                },
                [&](const core::RunFinished& run) {
                    std::string error;
                    if (!files_.log_run(run.summary, error))
                        message("Забег не записан в историю: " + error);
                    save();
                },
            },
            e);
    }
    for (const core::MenuRequest& r : menu_.take_requests()) {
        std::visit(Overload{
                       [&](const core::SaveProfile&) { save(); },
                       [&](const core::SwitchProfile& s) {
                           std::vector<std::string> messages;
                           profile_ = files_.open(s.number, messages);
                           menu_.set_profile_number(files_.profile_number());
                           for (std::string& m : messages)
                               message(std::move(m));
                       },
                       [&](const core::LoadHistory&) { open_history(); },
                   },
                   r);
    }
    switch (flow.screen()) {
        case core::Screen::Menu: menu_view_.update(now); break;
        case core::Screen::Game:
            // UpdateCursorTrail — в каждом кадре забега, кроме паузы.
            if (flow.game() != nullptr && !flow.game()->paused())
                trail_.add(core::round_half_even(mouse.x), core::round_half_even(mouse.y));
            break;
        case core::Screen::Countdown:
        case core::Screen::GameOver: break;
    }
}

void Screens::render(const core::Flow& flow, core::Vec2 mouse, int fps, core::Nanoseconds now) {
    const core::Theme& th = theme();
    switch (flow.screen()) {
        case core::Screen::Menu:
            menu_view_.render(canvas_, toasts_, th, menu_, profile_, history_, mouse, now);
            break;
        case core::Screen::Countdown: render_countdown(canvas_, th, flow, now); break;
        case core::Screen::Game:
            if (const core::Game* g = flow.game()) {
                render_game(canvas_, th, *g, effects_, trail_, fps, now);
                toasts_.render(canvas_, now);
                if (g->paused()) render_pause(canvas_);
            }
            break;
        case core::Screen::GameOver:
            if (const core::Game* g = flow.game()) {
                render_game_over(canvas_, th, *g);
                toasts_.render(canvas_, now);
            }
            break;
    }
}

}  // namespace aim::app
