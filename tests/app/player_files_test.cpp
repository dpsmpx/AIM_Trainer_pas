// Файлы игрока программы (D-11, D-30): новый профиль сразу записан, как у
// образца; повреждённый — копия и умолчания; последний профиль запоминается
// (AUD-21); у истории один заголовок; экспорт и смена папки сохранений
// (D-33, D-35). Каталоги — временные в текущем.

#include "app/player_files.h"

#include <SDL3/SDL.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/history.h"
#include "core/profile_file.h"
#include "core/text.h"

namespace aim::app {
namespace {

// Каталог у каждого теста свой: CTest запускает их параллельно.
std::string test_dir() {
    return std::string("player_files_test_") +
           ::testing::UnitTest::GetInstance()->current_test_info()->name() + "/";
}

// Время смены папки — для имени backup_… (D-35).
constexpr core::LocalTime kWhen{.year = 2026, .month = 10, .day = 16, .hour = 15, .minute = 32};

class PlayerFilesTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::remove_all(dir_);
        ASSERT_TRUE(SDL_CreateDirectory(dir_.c_str()));
    }
    void TearDown() override { std::filesystem::remove_all(dir_); }

    [[nodiscard]] PlayerFiles files() const { return PlayerFiles(platform::Storage(dir_)); }

    std::string dir_ = test_dir();
    platform::Storage disk_{dir_};
    std::vector<std::string> messages_;
    std::string error_;
};

TEST_F(PlayerFilesTest, NewProfileIsWrittenWithDefaults) {
    PlayerFiles f = files();
    const core::Profile p = f.load(messages_);
    EXPECT_TRUE(messages_.empty());
    EXPECT_EQ(f.profile_number(), 1);
    EXPECT_EQ(disk_.read("profile1_stats.ini"),
              core::write_stats(core::Profile{}, platform::kNewline));
    EXPECT_EQ(p.settings.lives, core::Profile{}.settings.lives);
}

TEST_F(PlayerFilesTest, SavedProfileIsReadBack) {
    core::Profile p;
    p.best_score = 321;
    p.settings.theme = 2;
    {
        PlayerFiles f = files();
        f.load(messages_);
        ASSERT_TRUE(f.save(p, error_)) << error_;
    }
    PlayerFiles again = files();
    const core::Profile read = again.load(messages_);
    EXPECT_TRUE(messages_.empty());
    EXPECT_EQ(read.best_score, 321);
    EXPECT_EQ(read.settings.theme, 2);
}

TEST_F(PlayerFilesTest, DamagedProfileIsCopiedAndReset) {
    const std::string damaged = "[Stats]\nBestScore=много\nGamesPlayed=7\n";
    ASSERT_TRUE(disk_.write("profile1_stats.ini", damaged, error_)) << error_;
    PlayerFiles f = files();
    const core::Profile p = f.load(messages_);
    EXPECT_EQ(p.games_played, 0);
    EXPECT_EQ(messages_.size(), 1U);
    EXPECT_EQ(disk_.read("profile1_corrupt_backup.txt"), damaged);
    EXPECT_EQ(disk_.read("profile1_stats.ini"),
              core::write_stats(core::Profile{}, platform::kNewline));
}

TEST_F(PlayerFilesTest, OutOfRangeValueIsResetAndWritten) {
    core::Profile p;
    p.games_played = 5;
    std::string text = core::write_stats(p, platform::kNewline);
    const std::string from = "SetLives=15";
    text.replace(text.find(from), from.size(), "SetLives=99");
    ASSERT_TRUE(disk_.write("profile1_stats.ini", text, error_)) << error_;

    PlayerFiles f = files();
    const core::Profile read = f.load(messages_);
    EXPECT_EQ(messages_.size(), 1U);
    EXPECT_EQ(read.games_played, 5);
    EXPECT_EQ(read.settings.lives, core::Profile{}.settings.lives);
    EXPECT_EQ(disk_.read("profile1_stats.ini"), core::write_stats(read, platform::kNewline));
}

TEST_F(PlayerFilesTest, LastProfileIsRemembered) {
    {
        PlayerFiles f = files();
        f.load(messages_);
        f.open(3, messages_);
    }
    PlayerFiles again = files();
    again.load(messages_);
    EXPECT_TRUE(messages_.empty());
    EXPECT_EQ(again.profile_number(), 3);
    EXPECT_TRUE(disk_.exists("profile3_stats.ini"));
}

TEST_F(PlayerFilesTest, EffectsFlagDoesNotCarryToNewProfile) {
    PlayerFiles f = files();
    core::Profile p = f.load(messages_);
    p.settings.menu_effects = !core::Profile{}.settings.menu_effects;
    ASSERT_TRUE(f.save(p, error_)) << error_;
    const core::Profile other = f.open(2, messages_);
    EXPECT_EQ(other.settings.menu_effects, core::Profile{}.settings.menu_effects);
}

TEST_F(PlayerFilesTest, HistoryHasOneHeaderAndARowPerRun) {
    PlayerFiles f = files();
    f.load(messages_);
    core::GameSummary run;
    run.mode = core::GameMode::Flick;
    run.score = 77;
    ASSERT_TRUE(f.log_run(run, error_)) << error_;
    ASSERT_TRUE(f.log_run(run, error_)) << error_;

    const std::string csv = disk_.read("profile1_sessions.csv").value_or("");
    const std::vector<std::string_view> lines = core::split_lines(csv);
    ASSERT_EQ(lines.size(), 3U);
    EXPECT_EQ(lines[0], core::kSessionsHeader);
    EXPECT_NE(lines[1].find(",Flick,77,"), std::string_view::npos);
    EXPECT_EQ(core::read_sessions(csv).size(), 2U);
}

TEST_F(PlayerFilesTest, ExportCopiesHistoryNextToProfile) {
    PlayerFiles f = files();
    f.load(messages_);
    std::string name;
    EXPECT_FALSE(f.export_history({}, name, error_)) << "истории ещё нет";
    core::GameSummary run;
    run.score = 5;
    ASSERT_TRUE(f.log_run(run, error_)) << error_;
    ASSERT_TRUE(f.export_history({.year = 2026, .month = 10, .day = 16}, name, error_)) << error_;
    EXPECT_EQ(name, "export_profile1_20261016.csv");
    EXPECT_EQ(disk_.read(name), disk_.read("profile1_sessions.csv"));
}

TEST_F(PlayerFilesTest, ChangeFolderCopiesFilesAndIsRemembered) {
    const std::string other = dir_ + "other/";
    {
        PlayerFiles f = files();
        core::Profile p = f.load(messages_);
        p.best_score = 77;
        ASSERT_TRUE(f.save(p, error_)) << error_;
        ASSERT_TRUE(f.log_run({}, error_)) << error_;
        const std::optional<core::Profile> moved = f.change_folder(other, kWhen, messages_);
        ASSERT_TRUE(moved.has_value());
        EXPECT_EQ(moved.value_or(core::Profile{}).best_score, 77);
        EXPECT_EQ(f.folder(), other);
    }
    const platform::Storage target(other);
    EXPECT_TRUE(target.exists("profile1_stats.ini"));
    EXPECT_TRUE(target.exists("profile1_sessions.csv"));
    EXPECT_TRUE(disk_.exists("profile1_stats.ini")) << "старая папка не тронута";
    // Перезапуск: папка — из aim_trainer.ini папки по умолчанию.
    PlayerFiles again = files();
    const core::Profile read = again.load(messages_);
    EXPECT_EQ(again.folder(), other);
    EXPECT_EQ(read.best_score, 77);
}

TEST_F(PlayerFilesTest, ChangeFolderReplacesSavesThereKeepingCopies) {
    const std::string other = dir_ + "theirs/";
    const platform::Storage theirs(other);
    ASSERT_TRUE(SDL_CreateDirectory(other.c_str()));
    core::Profile their_profile;
    their_profile.best_score = 999;
    const std::string their_text = core::write_stats(their_profile, platform::kNewline);
    ASSERT_TRUE(theirs.write("profile1_stats.ini", their_text, error_));
    ASSERT_TRUE(theirs.write("profile3_stats.ini", their_text, error_));
    PlayerFiles f = files();
    core::Profile mine = f.load(messages_);
    mine.best_score = 5;
    ASSERT_TRUE(f.save(mine, error_)) << error_;
    messages_.clear();
    const std::optional<core::Profile> moved = f.change_folder(other, kWhen, messages_);
    ASSERT_TRUE(moved.has_value());
    EXPECT_EQ(moved.value_or(core::Profile{}).best_score, 5)
        << "сохранения прежней папки — в новой";
    const platform::Storage backup(other + "backup_20261016_1532/");
    EXPECT_EQ(backup.read("profile1_stats.ini"), their_text) << "заменённый файл не потерян";
    EXPECT_EQ(theirs.read("profile3_stats.ini"), their_text) << "чего в прежней нет — не тронуто";
    EXPECT_FALSE(backup.exists("profile3_stats.ini"));
    ASSERT_GE(messages_.size(), 2U);
    EXPECT_NE(messages_[1].find("backup_20261016_1532"), std::string::npos) << messages_[1];
}

TEST_F(PlayerFilesTest, SecondChangeInAMinuteGetsItsOwnBackup) {
    const std::string other = dir_ + "other/";
    PlayerFiles f = files();
    core::Profile p = f.load(messages_);
    // Туда-обратно с правкой профиля: в папке по умолчанию заменяется дважды.
    for (const std::string& to : {other, dir_, other, dir_}) {
        ++p.best_score;
        ASSERT_TRUE(f.save(p, error_)) << error_;
        ASSERT_TRUE(f.change_folder(to, kWhen, messages_).has_value()) << to;
    }
    EXPECT_TRUE(platform::Storage(dir_ + "backup_20261016_1532/").exists("profile1_stats.ini"));
    EXPECT_TRUE(platform::Storage(dir_ + "backup_20261016_1532_2/").exists("profile1_stats.ini"));
    EXPECT_EQ(f.load(messages_).best_score, 4);
}

TEST_F(PlayerFilesTest, UnreadableOldFolderKeepsTheChoice) {
    const std::string target = dir_.substr(0, dir_.size() - 1) + "_target/";
    PlayerFiles f = files();
    f.load(messages_);
    std::filesystem::remove_all(dir_);  // папку сохранений унесли из-под программы
    messages_.clear();
    EXPECT_FALSE(f.change_folder(target, kWhen, messages_).has_value())
        << "пустой список — не «0 файлов»";
    EXPECT_EQ(f.folder(), dir_);
    EXPECT_EQ(messages_.size(), 1U);
    std::filesystem::remove_all(target);
}

TEST_F(PlayerFilesTest, SameFolderIsNotAChange) {
    PlayerFiles f = files();
    f.load(messages_);
    messages_.clear();
    EXPECT_FALSE(f.change_folder(dir_, kWhen, messages_).has_value());
    EXPECT_EQ(messages_.size(), 1U) << "игрок видит, что папка та же";
}

TEST_F(PlayerFilesTest, BackToDefaultFolderForgetsTheChoice) {
    PlayerFiles f = files();
    f.load(messages_);
    ASSERT_TRUE(f.change_folder(dir_ + "other/", kWhen, messages_).has_value());
    ASSERT_TRUE(f.change_folder(dir_, kWhen, messages_).has_value());
    EXPECT_EQ(f.folder(), dir_);
    const core::AppState state = core::read_app_state(disk_.read("aim_trainer.ini").value_or(""));
    EXPECT_TRUE(state.data_dir.empty()) << "папке по умолчанию строка DataDir не нужна";
}

TEST_F(PlayerFilesTest, UnavailableChosenFolderFallsBackToDefault) {
    ASSERT_TRUE(
        disk_.write("aim_trainer.ini",
                    core::write_app_state({.last_profile = 1, .data_dir = dir_ + "file_not_dir"},
                                          platform::kNewline),
                    error_));
    ASSERT_TRUE(disk_.write("file_not_dir", "x", error_));
    PlayerFiles f = files();
    f.load(messages_);
    EXPECT_EQ(f.folder(), dir_);
    ASSERT_EQ(messages_.size(), 1U);
}

TEST_F(PlayerFilesTest, NoFolderSaysSoAndKeepsProfileInMemory) {
    PlayerFiles f(std::nullopt, "no home");
    const core::Profile p = f.load(messages_);
    ASSERT_EQ(messages_.size(), 1U);
    EXPECT_NE(messages_[0].find("no home"), std::string::npos);
    EXPECT_FALSE(f.save(p, error_));
    EXPECT_FALSE(error_.empty());
}

}  // namespace
}  // namespace aim::app
