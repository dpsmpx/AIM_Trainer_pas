#include "core/flow.h"

#include <climits>
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

std::unique_ptr<Random> engine_random(std::uint64_t seed) {
    return std::make_unique<EngineRandom>(seed);
}

Flow::Flow(Profile& profile, Random& seeds, MenuInput& menu, RandomFactory make_random,
           std::function<LocalTime()> clock)
    : profile_(profile),
      seeds_(seeds),
      menu_(menu),
      make_random_(std::move(make_random)),
      clock_(std::move(clock)) {}

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
    RunRecord run{.when = clock_ ? clock_() : LocalTime{}, .summary = game_->summary()};
    run.summary.seed = seed_;
    const FinishResult result = finish_game(profile_, run);
    for (const int id : result.unlocked)
        events_.emplace_back(AchievementUnlocked{.id = id});
    last_run_ = RunFinished{.record = std::move(run), .new_best = result.new_best};
    events_.emplace_back(*last_run_);
}

bool Flow::handle(const Input& event) {
    switch (screen_) {
        case Screen::Menu:
            return std::visit(Overload{[&](const ClickInput& c) {
                                           const std::optional<MeasureOptions> options =
                                               menu_.click(c);
                                           if (!options) return true;
                                           options_ = normalized(*options);
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
                case Key::R:  // заново, замер не записывается
                    start_countdown(now_);
                    return false;
                case Key::M:  // в меню, замер записывается
                    game.finish(EndReason::ToMenu, k.time);
                    finish_run();
                    go(Screen::Menu);
                    return false;
                case Key::Escape:  // конец попытки (D-40)
                    game.finish(EndReason::GaveUp, k.time);
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
                // Замер — с конца отсчёта, а не с кадра, который его заметил;
                // у замера свой генератор с записанным началом (D-39).
                game_.reset();
                seed_ = (static_cast<std::uint64_t>(seeds_.between(0, INT_MAX)) << 31U) ^
                        static_cast<std::uint64_t>(seeds_.between(0, INT_MAX));
                run_random_ = make_random_(seed_);
                game_.emplace(options_, *run_random_, countdown_started_ + kCountdown);
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
