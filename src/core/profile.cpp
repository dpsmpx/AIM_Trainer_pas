#include "core/profile.h"

#include <algorithm>
#include <cstddef>

#include "core/rounding.h"
#include "core/rules.h"

namespace aim::core {

const std::array<AchievementInfo, kAchievementCount> kAchievements{{
    {"Первая игра", "Сыграй свою первую игру"},
    {"В яблочко", "Попади почти точно в центр цели"},
    {"Молниеносно", "Попади с реакцией меньше 150мс"},
    {"Комбо x100", "Набери 100 попаданий подряд без промаха"},
    {"Тысяча", "Набери 1000 очков за одну игру"},
    {"Снайпер", "Закончи игру (от 20 выстрелов) с точностью 90%+"},
    {"Ветеран", "Сыграй 1000 игр"},
    {"Мастер Dual", "Набери 500+ очков в Dual Mode за одну игру"},
}};

namespace {

// Номера достижений — как в kAchievements и в файле профиля (Achv0…Achv7).
constexpr int kFirstGame = 0;
constexpr int kBullseye = 1;
constexpr int kLightning = 2;
constexpr int kCombo100 = 3;
constexpr int kThousand = 4;
constexpr int kSniper = 5;
constexpr int kVeteran = 6;
constexpr int kDualMaster = 7;

constexpr int kVeteranGames = 1000;
constexpr int kDualMasterScore = 500;
constexpr double kSniperAccuracy = 90.0;

}  // namespace

void step_setting(Settings& settings, SettingRow row, int delta) {
    switch (row) {
        case SettingRow::CircleSize:
            settings.circle_size = std::clamp(settings.circle_size + delta * kCircleSizeStep,
                                              kMinCircleSize, kMaxCircleSize);
            break;
        case SettingRow::Lifetime:
            settings.lifetime_ms = std::clamp(settings.lifetime_ms + delta * kLifetimeStepMs,
                                              kMinLifetimeMs, kMaxLifetimeMs);
            break;
        case SettingRow::Lives:
            settings.lives = std::clamp(settings.lives + delta, kMinLives, kMaxLives);
            break;
    }
}

void reset_settings(Settings& settings) {
    const Settings defaults;
    settings.circle_size = defaults.circle_size;
    settings.lifetime_ms = defaults.lifetime_ms;
    settings.lives = defaults.lives;
    settings.sound = defaults.sound;
}

void reset_out_of_range(Settings& settings) {
    const Settings defaults;
    const auto check = [](int& value, int low, int high, int fallback) {
        if (value < low || value > high) value = fallback;
    };
    check(settings.circle_size, kMinCircleSize, kMaxCircleSize, defaults.circle_size);
    check(settings.lifetime_ms, kMinLifetimeMs, kMaxLifetimeMs, defaults.lifetime_ms);
    check(settings.lives, kMinLives, kMaxLives, defaults.lives);
    check(settings.theme, 0, kThemeCount - 1, defaults.theme);
}

GameSettings game_settings(const Settings& settings) {
    return {.circle_size = settings.circle_size,
            .lifetime_ms = settings.lifetime_ms,
            .lives = settings.lives};
}

int add_to_leaderboard(Leaderboard& board, GameMode mode, int score, double accuracy) {
    auto& rows = board[static_cast<std::size_t>(mode_index(mode))];
    const auto at = std::find_if(rows.begin(), rows.end(),
                                 [&](const LeaderboardEntry& e) { return score > e.score; });
    const auto place = static_cast<int>(at - rows.begin());
    if (place >= kLeaderboardSize) return -1;
    rows.insert(at, LeaderboardEntry{.score = score, .accuracy = accuracy});
    if (rows.size() > static_cast<std::size_t>(kLeaderboardSize)) rows.pop_back();
    return place;
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

FinishResult finish_game(Profile& profile, const GameSummary& summary) {
    FinishResult result;
    profile.best_score = std::max(profile.best_score, summary.score);
    // D-13: точность и реакция — только по играм от 20 выстрелов.
    if (summary.shots >= kRecordMinShots) {
        profile.best_accuracy = std::max(profile.best_accuracy, summary.accuracy);
        if (summary.hits > 0) {
            profile.best_precision = std::max(profile.best_precision, summary.precision);
            if (profile.best_avg_reaction == 0.0 ||
                summary.avg_reaction_ms < profile.best_avg_reaction)
                profile.best_avg_reaction = summary.avg_reaction_ms;
        }
    }
    ++profile.games_played;

    result.leaderboard_place =
        add_to_leaderboard(profile.leaderboard, summary.mode, summary.score, summary.accuracy);

    const auto try_unlock = [&](int id, bool condition) {
        if (condition && unlock(profile, id)) result.unlocked.push_back(id);
    };
    try_unlock(kFirstGame, profile.games_played == 1);
    try_unlock(kSniper, summary.shots >= kRecordMinShots && summary.accuracy >= kSniperAccuracy);
    try_unlock(kVeteran, profile.games_played >= kVeteranGames);
    try_unlock(kDualMaster, summary.mode == GameMode::Dual && summary.score >= kDualMasterScore);

    for (const double r : summary.reactions_ms)
        profile.reactions.push(round_half_even(r));
    return result;
}

void ReactionRing::push(int value_ms) {
    values[static_cast<std::size_t>(next)] = value_ms;
    next = (next + 1) % kReactionHistory;
    count = std::min(count + 1, kReactionHistory);
}

std::vector<int> ReactionRing::in_order() const {
    std::vector<int> out;
    out.reserve(static_cast<std::size_t>(count));
    // Неполное кольцо — с нуля; полное — со следующей записи, она старейшая.
    const int first = count < kReactionHistory ? 0 : next;
    for (int i = 0; i < count; ++i)
        out.push_back(values[static_cast<std::size_t>((first + i) % kReactionHistory)]);
    return out;
}

}  // namespace aim::core
