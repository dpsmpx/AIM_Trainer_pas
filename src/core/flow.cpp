#include "core/flow.h"

#include <utility>

namespace aim::core {

namespace {

// Перегрузка лямбд для std::visit.
template <class... F>
struct Overload : F... {
    using F::operator()...;
};
template <class... F>
Overload(F...) -> Overload<F...>;

}  // namespace

Flow::Flow(Profile& profile, Random& rng, MenuInput& menu)
    : profile_(profile), rng_(rng), menu_(menu) {}

void Flow::go(Screen to) {
    if (to == screen_) return;
    events_.emplace_back(ScreenChanged{.from = screen_, .to = to});
    screen_ = to;
}

void Flow::start_countdown(Nanoseconds now) {
    game_.reset();
    countdown_started_ = now;
    go(Screen::Countdown);
}

void Flow::pass_game_events() {
    if (!game_) return;
    for (const GameEvent& e : game_->take_events()) {
        events_.emplace_back(e);
        for (const int id : achievements_on_event(profile_, e))
            events_.emplace_back(AchievementUnlocked{.id = id});
    }
}

void Flow::finish_run() {
    if (!game_ || finished_) return;
    finished_ = true;
    pass_game_events();
    const GameSummary summary = game_->summary();
    const FinishResult result = finish_game(profile_, summary);
    for (const int id : result.unlocked)
        events_.emplace_back(AchievementUnlocked{.id = id});
    events_.emplace_back(
        RunFinished{.summary = summary, .leaderboard_place = result.leaderboard_place});
}

bool Flow::handle(const Input& event) {
    switch (screen_) {
        case Screen::Menu:
            return std::visit(Overload{[&](const ClickInput& c) {
                                           const std::optional<GameMode> mode = menu_.click(c);
                                           if (!mode) return true;
                                           mode_ = *mode;
                                           start_countdown(now_);
                                           return false;
                                       },
                                       [&](const KeyInput& k) {
                                           menu_.key(k);
                                           return true;
                                       }},
                              event);
        case Screen::Countdown:
            // Щелчки отсчёт не читает и гасит: в игру они не попадут (AUD-02).
            if (const auto* k = std::get_if<KeyInput>(&event); k && k->key == Key::M) {
                go(Screen::Menu);
                return false;
            }
            return true;
        case Screen::Game: {
            if (!game_) return true;
            // Ссылка — до смены экрана: start_countdown сбрасывает забег.
            Game& game = *game_;
            if (const auto* c = std::get_if<ClickInput>(&event)) {
                game.click(c->button, c->position, c->time);
                pass_game_events();
                if (!game.over()) return true;
                finish_run();
                go(Screen::GameOver);
                return false;
            }
            const auto& k = std::get<KeyInput>(event);
            switch (k.key) {
                case Key::P:
                    if (game.paused()) {
                        game.resume(k.time);
                    } else {
                        game.pause(k.time);
                    }
                    return true;
                case Key::R:  // заново, забег не записывается
                    start_countdown(now_);
                    return false;
                case Key::M:  // в меню, забег записывается
                    finish_run();
                    go(Screen::Menu);
                    return false;
                case Key::Escape:
                    game.give_up();
                    finish_run();
                    go(Screen::GameOver);
                    return false;
                case Key::Space: return true;
            }
            return true;
        }
        case Screen::GameOver: {
            const auto* k = std::get_if<KeyInput>(&event);
            if (k == nullptr) return true;  // щелчки Game Over гасит (AUD-02)
            if (k->key == Key::Space || k->key == Key::M) {
                go(Screen::Menu);
                return false;
            }
            if (k->key == Key::R) {
                start_countdown(now_);
                return false;
            }
            return true;
        }
    }
    return true;
}

void Flow::input(std::span<const Input> events, Nanoseconds now) {
    now_ = now;
    for (const Input& event : events) {
        if (!handle(event)) break;  // экран сменился: остальное — не ему
    }
}

void Flow::update(Nanoseconds now, Vec2 cursor) {
    now_ = now;
    switch (screen_) {
        case Screen::Countdown:
            if (now >= countdown_started_ + kCountdown) {
                // Забег — с конца отсчёта, а не с кадра, который его заметил.
                game_.emplace(mode_, game_settings(profile_.settings), rng_,
                              countdown_started_ + kCountdown);
                finished_ = false;
                go(Screen::Game);
            }
            break;
        case Screen::Game: {
            if (!game_) break;
            Game& game = *game_;
            game.update(now, cursor);
            pass_game_events();
            if (game.over()) {
                finish_run();
                go(Screen::GameOver);
            }
            break;
        }
        case Screen::Menu:
        case Screen::GameOver: break;
    }
}

void Flow::frame_presented(Nanoseconds frame_time, Nanoseconds presented_at) {
    if (screen_ == Screen::Game && game_) game_->frame_presented(frame_time, presented_at);
}

const Game* Flow::game() const {
    if ((screen_ == Screen::Game || screen_ == Screen::GameOver) && game_) return &*game_;
    return nullptr;
}

int Flow::countdown_value(Nanoseconds now) const {
    if (screen_ != Screen::Countdown) return 0;
    const Nanoseconds elapsed = now > countdown_started_ ? now - countdown_started_ : 0;
    const auto passed = static_cast<int>(elapsed / ms(1000));
    return passed >= 3 ? 1 : 3 - passed;
}

std::vector<FlowEvent> Flow::take_events() {
    return std::exchange(events_, {});
}

}  // namespace aim::core
