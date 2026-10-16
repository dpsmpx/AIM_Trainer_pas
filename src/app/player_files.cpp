#include "app/player_files.h"

#include <SDL3/SDL.h>

#include <format>
#include <utility>

#include "core/profile_file.h"

namespace aim::app {

core::LocalTime local_now() {
    core::LocalTime t;
    SDL_Time now = 0;
    SDL_DateTime dt{};
    if (SDL_GetCurrentTime(&now) && SDL_TimeToDateTime(now, &dt, true)) {
        t = {.year = dt.year,
             .month = dt.month,
             .day = dt.day,
             .hour = dt.hour,
             .minute = dt.minute,
             .second = dt.second};
    }
    return t;
}

PlayerFiles::PlayerFiles(std::optional<platform::Storage> home, std::string problem)
    : home_(std::move(home)), storage_(home_), problem_(std::move(problem)) {}

std::string PlayerFiles::folder() const {
    return storage_ ? storage_->dir() : std::string{};
}

core::Profile PlayerFiles::load(std::vector<std::string>& messages) {
    if (home_) {
        if (const auto state = home_->read(core::app_state_file_name()))
            remembered_ = core::read_app_state(*state);
        if (!remembered_.data_dir.empty()) {
            std::string problem;
            if (auto chosen = platform::Storage::open(remembered_.data_dir, problem)) {
                storage_ = std::move(chosen);
                data_dir_ = remembered_.data_dir;
            } else {
                messages.push_back(
                    std::format("Папка сохранений {} недоступна ({}): файлы — в папке по умолчанию",
                                remembered_.data_dir, problem));
            }
        }
    }
    return open(remembered_.last_profile, messages);
}

core::Profile PlayerFiles::open(int number, std::vector<std::string>& messages) {
    if (number >= 1 && number <= core::kProfileCount) number_ = number;
    if (!storage_) {
        messages.push_back("Нет папки игрока, профиль не сохраняется: " + problem_);
        return {};
    }
    std::string error;
    const auto save_or_say = [&](const core::Profile& profile) {
        if (!save(profile, error)) messages.push_back("Профиль не записан: " + error);
    };

    core::Profile profile;
    const std::optional<std::string> text = storage_->read(core::profile_file_name(number_));
    if (!text) {
        // У нового профиля сразу появляется файл по умолчанию.
        save_or_say(profile);
    } else {
        core::LoadedProfile loaded = core::read_profile(*text);
        profile = std::move(loaded.profile);
        if (loaded.corrupt) {
            if (!storage_->write(core::corrupt_backup_file_name(number_), *text, error))
                messages.push_back("Копия повреждённого профиля не записана: " + error);
            messages.push_back(
                std::format("Профиль {} не читается: сброшен, копия сохранена", number_));
            save_or_say(profile);
        } else if (!loaded.reset_fields.empty()) {
            messages.push_back(std::format("Профиль {}: неверных значений сброшено — {}", number_,
                                           loaded.reset_fields.size()));
            save_or_say(profile);
        } else if (!remember(error)) {
            messages.push_back("Номер профиля не записан: " + error);
        }
    }
    if (const std::optional<std::string> runs = storage_->read(history_file_name())) {
        std::size_t skipped = 0;
        profile.runs = core::read_runs(*runs, &skipped);
        if (skipped > 0)
            messages.push_back(std::format("История профиля {}: испорченных строк пропущено — {}",
                                           number_, skipped));
    }
    return profile;
}

bool PlayerFiles::save(const core::Profile& profile, std::string& error) {
    if (!storage_) {
        error = "нет папки игрока";
        return false;
    }
    return storage_->write(core::profile_file_name(number_),
                           core::write_profile(profile, platform::kNewline), error) &&
           remember(error);
}

bool PlayerFiles::remember(std::string& error) {
    if (!home_) {
        error = "нет папки игрока";
        return false;
    }
    const core::AppState state{.last_profile = number_, .data_dir = data_dir_};
    if (state.last_profile == remembered_.last_profile && state.data_dir == remembered_.data_dir &&
        home_->exists(core::app_state_file_name()))
        return true;
    if (!home_->write(core::app_state_file_name(), core::write_app_state(state, platform::kNewline),
                      error))
        return false;
    remembered_ = state;
    return true;
}

std::string PlayerFiles::history_file_name() const {
    return core::runs_file_name(number_);
}

bool PlayerFiles::log_run(const core::RunRecord& run, std::string& error) {
    if (!storage_) {
        error = "нет папки игрока";
        return false;
    }
    const std::string name = history_file_name();
    std::string text;
    if (!storage_->exists(name)) text += core::runs_file_header(platform::kNewline);
    text += core::run_line(run);
    text += platform::kNewline;
    return storage_->append(name, text, error);
}

bool PlayerFiles::export_history(const core::LocalTime& when, std::string& file_name,
                                 std::string& error) {
    const std::optional<std::string> history =
        storage_ ? storage_->read(history_file_name()) : std::nullopt;
    if (!history || !storage_) {
        error = "нет данных для экспорта";
        return false;
    }
    file_name = core::export_file_name(number_, when);
    return storage_->write(file_name, *history, error);
}

std::optional<core::Profile> PlayerFiles::change_folder(const std::string& dir,
                                                        const core::LocalTime& when,
                                                        std::vector<std::string>& messages) {
    std::string problem;
    std::optional<platform::Storage> target = platform::Storage::open(dir, problem);
    if (!target || !target->writable(problem)) {
        messages.push_back(std::format("Папка {} не подходит: {}", dir, problem));
        return std::nullopt;
    }
    if (storage_ && target->dir() == storage_->dir()) {
        messages.emplace_back("Эта папка уже — папка сохранений");
        return std::nullopt;
    }
    if (storage_ && !copy_saves(*storage_, *target, when, messages)) return std::nullopt;
    // Вернулся в папку по умолчанию — в aim_trainer.ini ей строка не нужна.
    data_dir_ = home_ && target->dir() == home_->dir() ? std::string{} : dir;
    storage_ = std::move(target);
    return open(number_, messages);
}

bool PlayerFiles::copy_saves(const platform::Storage& from, const platform::Storage& target,
                             const core::LocalTime& when, std::vector<std::string>& messages) {
    // Копия, не перенос: прежняя папка остаётся как была (D-33). Файл с тем
    // же именем в новой папке не теряется — сперва его копия в backup_… (D-35).
    std::optional<platform::Storage> backup;
    std::string backup_name;
    int copied = 0;
    std::string error;
    // Список не прочитан — не «0 файлов»: иначе новая папка без сохранений
    // выглядела бы потерей прогресса.
    const std::optional<std::vector<std::string>> names = from.files(error);
    if (!names) {
        messages.push_back(std::format("Папка сохранений не читается: {} — папка прежняя", error));
        return false;
    }
    for (const std::string& name : *names) {
        if (!core::is_player_file(name)) continue;
        const std::optional<std::string> data = from.read(name);
        if (!data) {
            messages.push_back(std::format("Файл {} не читается — папка прежняя", name));
            return false;
        }
        const std::optional<std::string> there = target.read(name);
        if (there == data) continue;  // уже такой же
        if (there) {
            if (!backup) backup = backup_folder(target, when, backup_name, error);
            if (!backup || !backup->write(name, *there, error)) {
                messages.push_back(std::format(
                    "Копия файла {} новой папки не записана: {} — папка прежняя", name, error));
                return false;
            }
        }
        if (!target.write(name, *data, error)) {
            messages.push_back(
                std::format("Файл {} не скопирован: {} — папка прежняя", name, error));
            return false;
        }
        ++copied;
    }
    // Два уведомления, а не одно: длинная строка шире окна.
    messages.push_back(std::format("Папка сохранений сменилась, файлов скопировано: {}", copied));
    if (backup) messages.push_back("Прежние файлы этой папки — в " + backup_name);
    return true;
}

std::optional<platform::Storage> PlayerFiles::backup_folder(const platform::Storage& target,
                                                            const core::LocalTime& when,
                                                            std::string& name, std::string& error) {
    for (int attempt = 1; attempt <= 99; ++attempt) {
        name = core::backup_folder_name(when, attempt);
        if (target.exists(name)) continue;
        return platform::Storage::open(target.path(name), error);
    }
    error = "все имена backup_… заняты";
    return std::nullopt;
}

}  // namespace aim::app
