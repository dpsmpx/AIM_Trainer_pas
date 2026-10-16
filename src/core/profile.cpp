#include "core/profile.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>

#include "core/rules.h"

namespace aim::core {

const std::array<AchievementInfo, kAchievementCount> kAchievements{{
    {"first_run", "Первый замер", "Закончи свой первый замер"},
    {"bullseye", "В яблочко", "Попади почти точно в центр цели"},
    {"lightning", "Молниеносно", "Отреагируй быстрее 150 мс"},
    {"combo100", "Серия x100", "100 попаданий подряд без промаха"},
    {"thousand", "Тысяча", "Набери 1000 очков за один замер"},
    {"sniper", "Снайпер", "Закончи замер (от 20 выстрелов) с точностью 90%+"},
    {"veteran", "Ветеран", "Сделай 1000 замеров"},
    {"two_hands", "Обе руки", "Набери 500+ очков с кнопками по цвету цели"},
}};

namespace {

// Номера достижений — как в kAchievements.
constexpr int kFirstRun = 0;
constexpr int kBullseye = 1;
constexpr int kLightning = 2;
constexpr int kCombo100 = 3;
constexpr int kThousand = 4;
constexpr int kSniper = 5;
constexpr int kVeteran = 6;
constexpr int kTwoHands = 7;

constexpr int kVeteranRuns = 1000;
constexpr int kTwoHandsScore = 500;
constexpr double kSniperAccuracy = 90.0;

int step_average(int value, int delta) {
    // Ближайшая ступень к value, затем delta ступеней.
    const auto& steps = kAverageRunSteps;
    std::size_t nearest = 0;
    for (std::size_t i = 1; i < steps.size(); ++i)
        if (std::abs(steps.at(i) - value) < std::abs(steps.at(nearest) - value)) nearest = i;
    const int index =
        std::clamp(static_cast<int>(nearest) + delta, 0, static_cast<int>(steps.size()) - 1);
    return steps.at(static_cast<std::size_t>(index));
}

}  // namespace

void step_setting(Settings& settings, SettingRow row, int delta) {
    PresetSettings& p = settings.preset;
    switch (row) {
        case SettingRow::TargetSize:
            p.target_size =
                std::clamp(p.target_size + delta * kTargetSizeStep, kMinTargetSize, kMaxTargetSize);
            break;
        case SettingRow::Lifetime:
            p.lifetime_ms = std::clamp(p.lifetime_ms + delta * kLifetimeStepMs, kMinBaseLifetimeMs,
                                       kMaxBaseLifetimeMs);
            break;
        case SettingRow::Lives: p.lives = std::clamp(p.lives + delta, kMinLives, kMaxLives); break;
        case SettingRow::AverageRuns:
            settings.average_runs = step_average(settings.average_runs, delta);
            break;
    }
}

void reset_settings(Settings& settings) {
    const Settings defaults;
    settings.preset = defaults.preset;
    settings.average_runs = defaults.average_runs;
    settings.sound = defaults.sound;
    settings.hud = defaults.hud;
    settings.replays = defaults.replays;
}

int reset_out_of_range(Settings& settings) {
    const Settings defaults;
    int reset = 0;
    const auto check = [&](int& value, int low, int high, int fallback) {
        if (value < low || value > high) {
            value = fallback;
            ++reset;
        }
    };
    PresetSettings& p = settings.preset;
    check(p.target_size, kMinTargetSize, kMaxTargetSize, defaults.preset.target_size);
    check(p.lifetime_ms, kMinBaseLifetimeMs, kMaxBaseLifetimeMs, defaults.preset.lifetime_ms);
    check(p.lives, kMinLives, kMaxLives, defaults.preset.lives);
    check(settings.average_runs, kMinAverageRuns, kMaxAverageRuns, defaults.average_runs);
    if (settings.theme.empty()) {
        settings.theme = defaults.theme;
        ++reset;
    }
    check(settings.font, 0, kFontCount - 1, defaults.font);
    return reset;
}

MeasureOptions item_options(const Profile& profile, int item) {
    if (item == kCustomItem) return normalized(profile.custom);
    return preset_options(item, profile.settings.preset);
}

MeasureOptions selected_options(const Profile& profile) {
    return item_options(profile, profile.preset);
}

bool unlock(Profile& profile, int achievement) {
    if (achievement < 0 || achievement >= kAchievementCount) return false;
    bool& flag = profile.achievements[static_cast<std::size_t>(achievement)];
    if (flag) return false;
    flag = true;
    return true;
}

std::vector<int> achievements_on_event(Profile& profile, const GameEvent& event) {
    std::vector<int> opened;
    if (event.kind != GameEventKind::Hit) return opened;
    const auto try_unlock = [&](int id, bool condition) {
        if (condition && unlock(profile, id)) opened.push_back(id);
    };
    try_unlock(kBullseye, event.accuracy >= kPreciseHit);
    try_unlock(kLightning, to_ms(event.reaction) < kFastReactionMs);
    try_unlock(kCombo100, event.combo >= kComboAchievement);
    try_unlock(kThousand, event.score >= kScoreAchievement);
    return opened;
}

FinishResult finish_game(Profile& profile, const RunRecord& run) {
    FinishResult result;
    const GameSummary& summary = run.summary;
    const ComboStats before =
        combo_stats(profile.runs, summary.options, profile.settings.average_runs);
    result.new_best = counts_for_record(summary) &&
                      (!before.best || better(summary, profile.runs[*before.best].summary));
    profile.runs.push_back(run);
    profile.runs.back().summary.reactions_ms.clear();

    const auto try_unlock = [&](int id, bool condition) {
        if (condition && unlock(profile, id)) result.unlocked.push_back(id);
    };
    const auto runs = static_cast<int>(profile.runs.size());
    try_unlock(kFirstRun, runs >= 1);
    try_unlock(kSniper, summary.shots >= kRecordMinShots && summary.accuracy >= kSniperAccuracy);
    try_unlock(kVeteran, runs >= kVeteranRuns);
    try_unlock(kTwoHands,
               summary.options.color_buttons && summary.options.action == Action::Click &&
                   summary.options.stimulus == Stimulus::Target && summary.score >= kTwoHandsScore);
    return result;
}

}  // namespace aim::core
