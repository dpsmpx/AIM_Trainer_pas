#include "core/flow.h"

#include <climits>
#include <utility>

#include "core/overload.h"

namespace aim::core {

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
    recorder_.reset();
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
    std::shared_ptr<const Replay> replay = recorder_ ? recorder_->take(run) : nullptr;
    recorder_.reset();
    last_run_ = RunFinished{
        .record = std::move(run), .new_best = result.new_best, .replay = std::move(replay)};
    events_.emplace_back(*last_run_);
}

void Flow::show_result() {
    finish_run();
    result_shown_ = now_;
    go(Screen::GameOver);
}

void Flow::play(std::shared_ptr<const Replay> replay, Nanoseconds now, Screen back) {
    options_ = replay->run.summary.options;
    player_.emplace(std::move(replay), make_random_, now);
    replay_back_ = back;
    go(Screen::Replay);
}

bool Flow::replay_offered() const {
    return last_run_ && last_run_->new_best && last_run_->replay != nullptr;
}

bool Flow::click_result(const ClickInput& click) {
    // Как меню, итог читает только ЛКМ; щелчок из замера — мимо (AUD-02).
    if (click.button != MouseButton::Left || click.time < result_shown_ + kResultButtonsDelay)
        return true;
    // Реплей нового рекорда с записанным входом — третья кнопка (D-46).
    std::shared_ptr<const Replay> offered;
    if (last_run_ && last_run_->new_best) offered = last_run_->replay;
    const bool with_replay = offered != nullptr;
    if (result_button(ResultButton::Again, with_replay).contains(click.position)) {
        start_countdown(now_);
        return false;
    }
    if (result_button(ResultButton::Menu, with_replay).contains(click.position)) {
        go(Screen::Menu);
        return false;
    }
    if (offered && result_button(ResultButton::Replay, true).contains(click.position)) {
        play(std::move(offered), now_, Screen::GameOver);
        return false;
    }
    return true;
}

bool Flow::handle_menu(const Input& event) {
    return std::visit(Overload{[&](const ClickInput& c) {
                                   const std::optional<MeasureOptions> options = menu_.click(c);
                                   if (!options) return true;
                                   options_ = normalized(*options);
                                   start_countdown(now_);
                                   return false;
                               },
                               [&](const KeyInput& k) {
                                   menu_.key(k);
                                   return true;
                               },
                               [&](const WheelInput& w) {
                                   menu_.wheel(w);
                                   return true;
                               }},
                      event);
}

bool Flow::handle_game(const Input& event) {
    if (!game_) return true;
    // Ссылка — до смены экрана: start_countdown сбрасывает замер.
    Game& game = *game_;
    if (const auto* c = std::get_if<ClickInput>(&event)) {
        if (recorder_) recorder_->click(c->button, c->position, c->time, now_);
        game.click(c->button, c->position, c->time);
        pass_game_events();
        if (!game.over()) return true;
        show_result();
        return false;
    }
    const auto* key = std::get_if<KeyInput>(&event);
    if (key == nullptr) return true;  // колёсико замер не читает
    switch (key->key) {
        case Key::R:  // заново, замер не записывается
            start_countdown(now_);
            return false;
        case Key::M:  // в меню, замер записывается
            if (recorder_) recorder_->finish(EndReason::ToMenu, key->time, now_);
            game.finish(EndReason::ToMenu, key->time);
            finish_run();
            go(Screen::Menu);
            return false;
        case Key::Escape:  // конец попытки (D-40)
            if (recorder_) recorder_->finish(EndReason::GaveUp, key->time, now_);
            game.finish(EndReason::GaveUp, key->time);
            show_result();
            return false;
        case Key::Space:
        case Key::Up:
        case Key::Down:
        case Key::PageUp:
        case Key::PageDown: return true;
    }
    return true;
}

bool Flow::handle_result(const Input& event) {
    if (const auto* c = std::get_if<ClickInput>(&event)) return click_result(*c);
    const auto* k = std::get_if<KeyInput>(&event);
    if (k == nullptr) return true;
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

bool Flow::handle(const Input& event) {
    switch (screen_) {
        case Screen::Menu: return handle_menu(event);
        case Screen::Countdown:
            // Щелчки отсчёт не читает и гасит: в замер они не попадут (AUD-02).
            if (const auto* k = std::get_if<KeyInput>(&event); k && k->key == Key::M) {
                go(Screen::Menu);
                return false;
            }
            return true;
        case Screen::Game: return handle_game(event);
        case Screen::GameOver: return handle_result(event);
        case Screen::Replay: {
            // Реплей только смотрят: щелчки и колёсико — мимо.
            const auto* k = std::get_if<KeyInput>(&event);
            if (k == nullptr) return true;
            if (k->key == Key::Escape || k->key == Key::Space || k->key == Key::M) {
                player_.reset();
                go(replay_back_);
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
                // Запись реплеев выключена — вход не пишется (D-45).
                if (profile_.settings.replays)
                    recorder_.emplace(options_, seed_, countdown_started_ + kCountdown);
                finished_ = false;
                go(Screen::Game);
            }
            break;
        case Screen::Game: {
            if (!game_) break;
            Game& game = *game_;
            if (recorder_) recorder_->update(now, cursor);
            game.update(now, cursor);
            pass_game_events();
            if (game.over()) show_result();
            break;
        }
        case Screen::Replay:
            if (player_) {
                player_->advance(now);
                // События — эффектам и звуку; достижений и записи нет.
                for (const GameEvent& e : player_->game().take_events())
                    events_.emplace_back(e);
            }
            break;
        case Screen::Menu:
        case Screen::GameOver: break;
    }
}

void Flow::frame_presented(Nanoseconds frame_time, Nanoseconds presented_at) {
    if (screen_ != Screen::Game || !game_) return;
    if (recorder_) recorder_->shown(frame_time, presented_at);
    game_->frame_presented(frame_time, presented_at);
}

void Flow::start_replay(std::shared_ptr<const Replay> replay, Nanoseconds now) {
    if (screen_ != Screen::Menu || !replay) return;
    play(std::move(replay), now, Screen::Menu);
}

const Game* Flow::game() const {
    if (screen_ == Screen::Replay) return player_ ? &player_->game() : nullptr;
    if ((screen_ == Screen::Game || screen_ == Screen::GameOver) && game_) return &*game_;
    return nullptr;
}

const ReplayPlayer* Flow::replay() const {
    return screen_ == Screen::Replay && player_ ? &*player_ : nullptr;
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
