#include "app/screens.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "app/game_view.h"
#include "core/overload.h"
#include "core/palette.h"
#include "core/rounding.h"
#include "core/rules.h"
#include "core/theme.h"

namespace aim::app {

Screens::Screens(SDL_Window* window, SDL_Renderer* renderer, Text& text, core::Profile& profile,
                 PlayerFiles& files, core::Random& rng, core::Menu& menu, platform::Sound& sound)
    : window_(window),
      canvas_(renderer, text),
      profile_(profile),
      files_(files),
      rng_(rng),
      menu_(menu),
      sound_(sound) {}

void Screens::shutdown() {
    in_run_ = false;
    if (save_pending_) save();
}

void Screens::start(core::Nanoseconds now) {
    menu_.enter(now);
    menu_view_.enter(rng_, now);
}

const core::Theme& Screens::theme() {
    const std::string& name = profile_.settings.theme;
    std::string key = files_.folder() + '\n' + name;
    if (key == theme_for_) return theme_;
    theme_for_ = std::move(key);
    const core::BuiltinTheme& fallback = core::kThemes.front();
    if (const core::BuiltinTheme* b = core::builtin_theme(name)) {
        theme_ = b->theme;
        return theme_;
    }
    const std::optional<std::string> text = files_.read_theme(name);
    if (!text) {
        theme_ = fallback.theme;
        message(std::format("Тема «{}» не найдена — «{}»", name, fallback.name));
        return theme_;
    }
    const core::ParsedTheme parsed = core::read_theme(*text);
    theme_ = parsed.theme;
    if (parsed.bad_lines > 0)
        message(std::format("Тема «{}»: неверных строк — {}", name, parsed.bad_lines));
    return theme_;
}

void Screens::save() {
    if (in_run_) {
        save_pending_ = true;
        return;
    }
    save_pending_ = false;
    std::string error;
    if (!files_.save(profile_, error)) message("Профиль не записан: " + error);
}

void Screens::play(core::Beep beep) {
    if (profile_.settings.sound) sound_.beep(beep);
}

void Screens::take_folder_choice() {
    const std::optional<platform::FolderChoice> choice = folder_picker_.take();
    if (!choice) return;
    if (!choice->error.empty()) {
        message("Выбор папки недоступен: " + choice->error);
        return;
    }
    if (choice->path.empty()) return;  // отмена
    // Несохранённое (переключатель эффектов) — в прежнюю папку до копирования.
    save();
    std::vector<std::string> messages;
    if (std::optional<core::Profile> moved =
            files_.change_folder(choice->path, local_now(), messages)) {
        profile_ = *moved;
        menu_.set_profile_number(files_.profile_number());
    }
    for (std::string& m : messages)
        message(std::move(m));
}

void Screens::before_input(const core::Flow& flow, core::Vec2 mouse, bool left_held,
                           core::Nanoseconds now) {
    if (flow.screen() != core::Screen::Menu) return;
    menu_.hover(mouse, now);
    menu_.drag(mouse, left_held);
}

void Screens::screen_changed(const core::ScreenChanged& s, core::Nanoseconds now) {
    // Замер кончился — отложенная запись профиля (AUD-40).
    in_run_ = s.to == core::Screen::Game;
    if (!in_run_ && save_pending_) save();
    if (s.to == core::Screen::Menu) {
        menu_.enter(now);
        menu_view_.enter(rng_, now);
    } else if (s.to == core::Screen::Game || s.to == core::Screen::Replay) {
        // Новый замер: эффекты и след — заново.
        effects_.clear();
        trail_.clear();
    } else if (s.to == core::Screen::GameOver && s.from == core::Screen::Game) {
        // Конец замера; возврат из реплея на итог — молча.
        play(core::kGameOverBeep);
    }
}

void Screens::game_event(const core::GameEvent& g, bool screen_stimulus, const core::Theme& th,
                         core::Nanoseconds now) {
    const int x = core::round_half_even(g.position.x);
    const int y = core::round_half_even(g.position.y);
    const core::Rgba color = th.target(g.color);
    switch (g.kind) {
        case core::GameEventKind::Hit:
            play(core::kHitBeep);
            if (screen_stimulus) {
                // Замер на цвет: реакция — надписью посреди окна.
                effects_.label(core::kWindowWidth / 2, core::kWindowHeight / 2,
                               std::format("{:.0f} мс", core::to_ms(g.reaction)), th.text, now);
                break;
            }
            effects_.hit(x, y, color, th.text, g.points, rng_, now);
            break;
        case core::GameEventKind::FalseStart:
            play(core::kMissBeep);
            effects_.label(core::kWindowWidth / 2, core::kWindowHeight / 2, "Фальстарт", th.warn,
                           now);
            break;
        case core::GameEventKind::Tracked:
            effects_.hit(x, y, color, th.text, g.points, rng_, now);
            play(core::kHitBeep);
            break;
        case core::GameEventKind::Miss: play(core::kMissBeep); break;
        // Подрыв — итогом «ПОДРЫВ!» и звуком конца (D-48).
        case core::GameEventKind::Bomb:
        case core::GameEventKind::Expired: break;
    }
}

void Screens::run_finished(const core::RunFinished& run) {
    std::string error;
    if (!files_.log_run(run.record, error)) message("Замер не записан в историю: " + error);
    // Лучший замер набора — реплеем вместо прежнего (D-39).
    if (run.new_best && run.replay && !files_.save_replay(*run.replay, error))
        message("Реплей не записан: " + error);
    save();
}

void Screens::menu_request(const core::MenuRequest& r, core::Flow& flow, core::Nanoseconds now) {
    std::visit(
        core::Overload{
            [&](const core::SaveProfile&) { save(); },
            [&](const core::SwitchProfile& s) {
                std::vector<std::string> messages;
                profile_ = files_.open(s.number, messages);
                menu_.set_profile_number(files_.profile_number());
                for (std::string& m : messages)
                    message(std::move(m));
            },
            [&](const core::ExportHistory&) {
                std::string name;
                std::string error;
                if (files_.export_history(local_now(), name, error))
                    message("CSV экспортирован: " + name);
                else
                    message("Ошибка экспорта: " + error);
            },
            [&](const core::OpenDataFolder&) {
                std::string error;
                if (!platform::open_folder(files_.folder(), error))
                    message("Папка не открылась: " + error);
            },
            [&](const core::ChooseDataFolder&) { folder_picker_.show(window_, files_.folder()); },
            [&](const core::NextTheme&) {
                profile_.settings.theme =
                    core::next_theme(profile_.settings.theme, files_.themes());
                save();
            },
            [&](const core::PlayReplay& p) {
                std::string error;
                std::optional<core::Replay> replay = files_.load_replay(p.options, error);
                if (!replay) {
                    message(error);
                    return;
                }
                flow.start_replay(std::make_shared<const core::Replay>(std::move(*replay)), now);
            },
        },
        r);
}

void Screens::after_update(core::Flow& flow, core::Vec2 mouse, core::Nanoseconds now) {
    const core::Theme& th = theme();
    // Без интерфейса замера — без надписей эффектов; реплей — как был (D-45).
    effects_.show_text(profile_.settings.hud || flow.screen() != core::Screen::Game);
    const bool screen_stimulus = flow.options().stimulus == core::Stimulus::Screen;
    for (const core::FlowEvent& e : flow.take_events()) {
        std::visit(
            core::Overload{
                [&](const core::ScreenChanged& s) { screen_changed(s, now); },
                [&](const core::GameEvent& g) { game_event(g, screen_stimulus, th, now); },
                [&](const core::AchievementUnlocked& a) {
                    message(
                        "Достижение: " +
                        std::string(core::kAchievements.at(static_cast<std::size_t>(a.id)).name));
                    // Посреди замера — запись по его концу (AUD-40).
                    save();
                },
                [&](const core::RunFinished& run) { run_finished(run); },
            },
            e);
    }
    for (const core::MenuRequest& r : menu_.take_requests())
        menu_request(r, flow, now);
    take_folder_choice();
    switch (flow.screen()) {
        case core::Screen::Menu: menu_view_.update(now); break;
        case core::Screen::Game:
            trail_.add(core::round_half_even(mouse.x), core::round_half_even(mouse.y));
            break;
        case core::Screen::Replay:
            // След — курсора игрока из записи.
            if (const core::ReplayPlayer* r = flow.replay())
                trail_.add(core::round_half_even(r->cursor().x),
                           core::round_half_even(r->cursor().y));
            break;
        case core::Screen::Countdown:
        case core::Screen::GameOver: break;
    }
}

void Screens::render(const core::Flow& flow, core::Vec2 mouse, int fps, core::Nanoseconds now) {
    const core::Theme& th = theme();
    canvas_.set_family(profile_.settings.font == 1 ? Family::Mono : Family::Condensed);
    switch (flow.screen()) {
        case core::Screen::Menu:
            menu_view_.render(canvas_, toasts_, th, menu_, profile_, files_, mouse, now);
            break;
        case core::Screen::Countdown: render_countdown(canvas_, th, flow, now); break;
        case core::Screen::Game:
            if (const core::Game* g = flow.game()) {
                const bool hud = profile_.settings.hud;
                render_game(canvas_, th, *g, effects_, trail_, fps, now, hud);
                if (hud) toasts_.render(canvas_, th, now);
            }
            break;
        case core::Screen::GameOver:
            if (const core::Game* g = flow.game()) {
                render_game_over(canvas_, th, *g, flow, profile_, mouse, now);
                toasts_.render(canvas_, th, now);
            }
            break;
        case core::Screen::Replay:
            if (const core::ReplayPlayer* r = flow.replay()) {
                render_replay(canvas_, th, *r, flow.replay_back(), effects_, trail_, fps, now);
                toasts_.render(canvas_, th, now);
            }
            break;
    }
}

}  // namespace aim::app
