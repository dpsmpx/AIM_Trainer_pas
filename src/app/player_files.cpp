#include "app/player_files.h"

#include <SDL3/SDL.h>

#include <format>
#include <utility>

#include "core/history.h"
#include "core/profile_file.h"

namespace aim::app {

namespace {

core::LocalTime local_now() {
    core::LocalTime t;
    SDL_Time now = 0;
    SDL_DateTime dt{};
    if (SDL_GetCurrentTime(&now) && SDL_TimeToDateTime(now, &dt, true)) {
        t = {.year = dt.year,
             .month = dt.month,
             .day = dt.day,
             .hour = dt.hour,
             .minute = dt.minute};
    }
    return t;
}

}  // namespace

PlayerFiles::PlayerFiles(std::optional<platform::Storage> storage, std::string problem)
    : storage_(std::move(storage)), problem_(std::move(problem)) {}

core::Profile PlayerFiles::load(std::vector<std::string>& messages) {
    if (storage_) {
        if (const auto state = storage_->read(core::app_state_file_name())) {
            remembered_ = core::read_last_profile(*state);
        }
    }
    return open(remembered_, messages);
}

core::Profile PlayerFiles::open(int number, std::vector<std::string>& messages) {
    if (number >= 1 && number <= core::kProfileCount) number_ = number;
    if (!storage_) {
        messages.push_back("No player folder, profile is not saved: " + problem_);
        return {};
    }
    std::string error;
    const auto save_or_say = [&](const core::Profile& profile) {
        if (!save(profile, error)) messages.push_back("Profile not saved: " + error);
    };

    const std::optional<std::string> text = storage_->read(core::stats_file_name(number_));
    if (!text) {
        // Как образец: у нового профиля сразу появляется файл по умолчанию.
        const core::Profile fresh;
        save_or_say(fresh);
        return fresh;
    }
    const core::LoadedProfile loaded = core::read_stats(*text);
    if (loaded.corrupt) {
        // Как образец: копия повреждённого файла и всё по умолчанию.
        if (!storage_->write(core::corrupt_backup_file_name(number_), *text, error))
            messages.push_back("Copy of damaged profile not saved: " + error);
        messages.push_back(
            std::format("Profile {} was damaged: reset to defaults, copy kept", number_));
        save_or_say(loaded.profile);
    } else if (!loaded.reset_fields.empty()) {
        messages.push_back(std::format("Profile {}: {} bad values reset to defaults", number_,
                                       loaded.reset_fields.size()));
        save_or_say(loaded.profile);
    } else if (number_ != remembered_) {
        if (!remember(error)) messages.push_back("Last profile not saved: " + error);
    }
    return loaded.profile;
}

bool PlayerFiles::save(const core::Profile& profile, std::string& error) {
    if (!storage_) {
        error = "no player folder";
        return false;
    }
    return storage_->write(core::stats_file_name(number_),
                           core::write_stats(profile, platform::kNewline), error) &&
           remember(error);
}

bool PlayerFiles::remember(std::string& error) {
    if (!storage_) {
        error = "no player folder";
        return false;
    }
    if (number_ == remembered_ && storage_->exists(core::app_state_file_name())) return true;
    if (!storage_->write(core::app_state_file_name(),
                         core::write_app_state(number_, platform::kNewline), error))
        return false;
    remembered_ = number_;
    return true;
}

bool PlayerFiles::log_run(const core::GameSummary& summary, std::string& error) {
    if (!storage_) {
        error = "no player folder";
        return false;
    }
    const std::string name = core::sessions_file_name(number_);
    std::string text;
    if (!storage_->exists(name)) {
        text += core::kSessionsHeader;
        text += platform::kNewline;
    }
    text += core::session_line(summary, local_now());
    text += platform::kNewline;
    return storage_->append(name, text, error);
}

}  // namespace aim::app
