// Файлы игрока программы (D-11, D-30): новый профиль сразу записан, как у
// образца; повреждённый — копия и умолчания; последний профиль запоминается
// (AUD-21); у истории один заголовок. Каталоги — временные в текущем.

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
