// Файлы игрока программы (D-11, D-30): новый профиль сразу записан, как у
// образца; повреждённый — копия и умолчания; последний профиль запоминается
// (AUD-21); у истории один заголовок; экспорт и смена папки сохранений
// (D-33, D-35); реплеи в replays/ (D-39). Каталоги — временные в текущем.

#include "app/player_files.h"

#include <SDL3/SDL.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/profile_file.h"
#include "core/replay.h"
#include "core/runs.h"
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
    EXPECT_EQ(disk_.read("profile1.ini"), core::write_profile(core::Profile{}, platform::kNewline));
    EXPECT_TRUE(p.runs.empty());
    EXPECT_FALSE(disk_.exists("profile1_runs.csv")) << "история — с первым замером";
}

TEST_F(PlayerFilesTest, SavedProfileIsReadBack) {
    core::Profile p;
    p.preset = 4;
    p.settings.theme = "minimal";
    {
        PlayerFiles f = files();
        f.load(messages_);
        ASSERT_TRUE(f.save(p, error_)) << error_;
    }
    PlayerFiles again = files();
    const core::Profile read = again.load(messages_);
    EXPECT_TRUE(messages_.empty());
    EXPECT_EQ(read.preset, 4);
    EXPECT_EQ(read.settings.theme, "minimal");
}

TEST_F(PlayerFilesTest, ForeignProfileIsCopiedAndReset) {
    const std::string foreign = "[Stats]\nBestScore=10\nGamesPlayed=7\n";
    ASSERT_TRUE(disk_.write("profile1.ini", foreign, error_)) << error_;
    PlayerFiles f = files();
    const core::Profile p = f.load(messages_);
    EXPECT_EQ(p.preset, core::Profile{}.preset);
    EXPECT_EQ(messages_.size(), 1U);
    EXPECT_EQ(disk_.read("profile1_corrupt_backup.txt"), foreign);
    EXPECT_EQ(disk_.read("profile1.ini"), core::write_profile(core::Profile{}, platform::kNewline));
}

TEST_F(PlayerFilesTest, OutOfRangeValueIsResetAndWritten) {
    core::Profile p;
    p.preset = 3;
    std::string text = core::write_profile(p, platform::kNewline);
    const std::string from = "lives=15";
    text.replace(text.find(from), from.size(), "lives=99");
    ASSERT_TRUE(disk_.write("profile1.ini", text, error_)) << error_;

    PlayerFiles f = files();
    const core::Profile read = f.load(messages_);
    EXPECT_EQ(messages_.size(), 1U);
    EXPECT_EQ(read.preset, 3);
    EXPECT_EQ(read.settings.preset.lives, core::Profile{}.settings.preset.lives);
    EXPECT_EQ(disk_.read("profile1.ini"), core::write_profile(read, platform::kNewline));
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
    EXPECT_TRUE(disk_.exists("profile3.ini"));
}

TEST_F(PlayerFilesTest, EffectsFlagDoesNotCarryToNewProfile) {
    PlayerFiles f = files();
    core::Profile p = f.load(messages_);
    p.settings.menu_effects = !core::Profile{}.settings.menu_effects;
    ASSERT_TRUE(f.save(p, error_)) << error_;
    const core::Profile other = f.open(2, messages_);
    EXPECT_EQ(other.settings.menu_effects, core::Profile{}.settings.menu_effects);
}

TEST_F(PlayerFilesTest, HistoryHasVersionHeaderAndARowPerRunAndIsReadBack) {
    PlayerFiles f = files();
    f.load(messages_);
    core::RunRecord run;
    run.summary.options = core::preset_options(4, {});
    run.summary.score = 77;
    ASSERT_TRUE(f.log_run(run, error_)) << error_;
    ASSERT_TRUE(f.log_run(run, error_)) << error_;

    const std::string csv = disk_.read("profile1_runs.csv").value_or("");
    const std::vector<std::string_view> lines = core::split_lines(csv);
    ASSERT_EQ(lines.size(), 4U);
    EXPECT_EQ(lines[0], core::kRunsVersionLine);
    EXPECT_EQ(lines[1], core::kRunsHeader);
    EXPECT_EQ(lines[2], core::run_line(run));
    // Испорченная строка пропускается, и игрок видит, сколько.
    ASSERT_TRUE(disk_.append("profile1_runs.csv", std::string("broken\n"), error_)) << error_;
    PlayerFiles again = files();
    const core::Profile p = again.load(messages_);
    EXPECT_EQ(p.runs.size(), 2U);
    EXPECT_EQ(p.runs.back().summary.score, 77);
    EXPECT_EQ(messages_.size(), 1U);
}

TEST_F(PlayerFilesTest, ExportCopiesHistoryNextToProfile) {
    PlayerFiles f = files();
    f.load(messages_);
    std::string name;
    EXPECT_FALSE(f.export_history({}, name, error_)) << "истории ещё нет";
    core::RunRecord run;
    run.summary.score = 5;
    ASSERT_TRUE(f.log_run(run, error_)) << error_;
    ASSERT_TRUE(f.export_history({.year = 2026, .month = 10, .day = 16}, name, error_)) << error_;
    EXPECT_EQ(name, "export_profile1_20261016.csv");
    EXPECT_EQ(disk_.read(name), disk_.read("profile1_runs.csv"));
}

TEST_F(PlayerFilesTest, ChangeFolderCopiesFilesAndIsRemembered) {
    const std::string other = dir_ + "other/";
    {
        PlayerFiles f = files();
        core::Profile p = f.load(messages_);
        p.settings.preset.lives = 27;
        ASSERT_TRUE(f.save(p, error_)) << error_;
        ASSERT_TRUE(f.log_run({}, error_)) << error_;
        const std::optional<core::Profile> moved = f.change_folder(other, kWhen, messages_);
        ASSERT_TRUE(moved.has_value());
        EXPECT_EQ(moved.value_or(core::Profile{}).settings.preset.lives, 27);
        EXPECT_EQ(f.folder(), other);
    }
    const platform::Storage target(other);
    EXPECT_TRUE(target.exists("profile1.ini"));
    EXPECT_TRUE(target.exists("profile1_runs.csv"));
    EXPECT_TRUE(disk_.exists("profile1.ini")) << "старая папка не тронута";
    // Перезапуск: папка — из aim_trainer.ini папки по умолчанию.
    PlayerFiles again = files();
    const core::Profile read = again.load(messages_);
    EXPECT_EQ(again.folder(), other);
    EXPECT_EQ(read.settings.preset.lives, 27);
    EXPECT_EQ(read.runs.size(), 1U);
}

TEST_F(PlayerFilesTest, ChangeFolderReplacesSavesThereKeepingCopies) {
    const std::string other = dir_ + "theirs/";
    const platform::Storage theirs(other);
    ASSERT_TRUE(SDL_CreateDirectory(other.c_str()));
    core::Profile their_profile;
    their_profile.settings.preset.lives = 29;
    const std::string their_text = core::write_profile(their_profile, platform::kNewline);
    ASSERT_TRUE(theirs.write("profile1.ini", their_text, error_));
    ASSERT_TRUE(theirs.write("profile3.ini", their_text, error_));
    PlayerFiles f = files();
    core::Profile mine = f.load(messages_);
    mine.settings.preset.lives = 5;
    ASSERT_TRUE(f.save(mine, error_)) << error_;
    messages_.clear();
    const std::optional<core::Profile> moved = f.change_folder(other, kWhen, messages_);
    ASSERT_TRUE(moved.has_value());
    EXPECT_EQ(moved.value_or(core::Profile{}).settings.preset.lives, 5)
        << "сохранения прежней папки — в новой";
    const platform::Storage backup(other + "backup_20261016_1532/");
    EXPECT_EQ(backup.read("profile1.ini"), their_text) << "заменённый файл не потерян";
    EXPECT_EQ(theirs.read("profile3.ini"), their_text) << "чего в прежней нет — не тронуто";
    EXPECT_FALSE(backup.exists("profile3.ini"));
    ASSERT_GE(messages_.size(), 2U);
    EXPECT_NE(messages_[1].find("backup_20261016_1532"), std::string::npos) << messages_[1];
}

// Реплей набора preset с одним шагом: курсор в (x, 0).
core::Replay replay_of(int preset, double x) {
    core::Replay r;
    r.run.summary.options = core::preset_options(preset, {});
    r.steps = {{.kind = core::ReplayStepKind::Update, .at = 1, .time = 1, .point = {x, 0}}};
    return r;
}

TEST_F(PlayerFilesTest, ReplayIsSavedReplacedAndReadBack) {
    const core::MeasureOptions classic = core::preset_options(1, {});
    {
        PlayerFiles f = files();
        f.load(messages_);
        EXPECT_FALSE(f.has_replay(classic));
        EXPECT_FALSE(f.load_replay(classic, error_)) << "реплея ещё нет";
        EXPECT_FALSE(error_.empty());
        ASSERT_TRUE(f.save_replay(replay_of(1, 10), error_)) << error_;
        ASSERT_TRUE(f.save_replay(replay_of(1, 20), error_)) << error_;
        EXPECT_TRUE(f.has_replay(classic));
        EXPECT_FALSE(f.has_replay(core::preset_options(4, {})));
    }
    const platform::Storage replays(dir_ + "replays/");
    EXPECT_TRUE(replays.exists(core::replay_file_name(1, classic)));
    // Перезапуск: реплей найден; новый лучший заменил прежний.
    PlayerFiles again = files();
    again.load(messages_);
    ASSERT_TRUE(again.has_replay(classic));
    const std::optional<core::Replay> read = again.load_replay(classic, error_);
    ASSERT_TRUE(read.has_value()) << error_;
    EXPECT_EQ(read.value_or(core::Replay{}).steps.at(0).point.x, 20);
    // У другого профиля — свои реплеи.
    again.open(2, messages_);
    EXPECT_FALSE(again.has_replay(classic));
}

TEST_F(PlayerFilesTest, BrokenReplayIsReported) {
    PlayerFiles f = files();
    f.load(messages_);
    ASSERT_TRUE(f.save_replay(replay_of(1, 10), error_)) << error_;
    const platform::Storage replays(dir_ + "replays/");
    const std::string name = core::replay_file_name(1, core::preset_options(1, {}));
    ASSERT_TRUE(replays.write(name, "broken", error_));
    EXPECT_FALSE(f.load_replay(core::preset_options(1, {}), error_));
    EXPECT_NE(error_.find("повреждён"), std::string::npos) << error_;
}

TEST_F(PlayerFilesTest, ChangeFolderCopiesReplaysKeepingTheirs) {
    const std::string other = dir_ + "theirs/";
    const std::string name = core::replay_file_name(1, core::preset_options(1, {}));
    const std::string theirs_text = core::write_replay(replay_of(1, 99), platform::kNewline);
    ASSERT_TRUE(SDL_CreateDirectory((other + "replays").c_str()));
    ASSERT_TRUE(platform::Storage(other + "replays/").write(name, theirs_text, error_)) << error_;
    PlayerFiles f = files();
    f.load(messages_);
    ASSERT_TRUE(f.save_replay(replay_of(1, 10), error_)) << error_;
    ASSERT_TRUE(f.save_replay(replay_of(4, 30), error_)) << error_;
    ASSERT_TRUE(f.change_folder(other, kWhen, messages_).has_value());
    EXPECT_TRUE(f.has_replay(core::preset_options(4, {})));
    const std::optional<core::Replay> mine = f.load_replay(core::preset_options(1, {}), error_);
    ASSERT_TRUE(mine.has_value()) << error_;
    EXPECT_EQ(mine.value_or(core::Replay{}).steps.at(0).point.x, 10)
        << "реплеи прежней папки — в новой";
    EXPECT_EQ(platform::Storage(other + "backup_20261016_1532/replays/").read(name), theirs_text)
        << "заменённый реплей не потерян";
}

TEST_F(PlayerFilesTest, PlayerThemesAreListedReadAndMoved) {
    ASSERT_TRUE(SDL_CreateDirectory((dir_ + "themes").c_str()));
    const platform::Storage themes(dir_ + "themes/");
    ASSERT_TRUE(themes.write("лес.ini", "background=#002000\n", error_)) << error_;
    ASSERT_TRUE(themes.write("дождь.ini", "base=neon\n", error_)) << error_;
    ASSERT_TRUE(themes.write("заметки.txt", "не тема", error_)) << error_;
    PlayerFiles f = files();
    f.load(messages_);
    EXPECT_EQ(f.themes(), (std::vector<std::string>{"дождь", "лес"})) << "только .ini, по алфавиту";
    EXPECT_EQ(f.read_theme("лес"), "background=#002000\n");
    EXPECT_FALSE(f.read_theme("нет такой"));
    // Темы переезжают вместе с сохранениями (D-44).
    const std::string other = dir_ + "other/";
    ASSERT_TRUE(f.change_folder(other, kWhen, messages_).has_value());
    EXPECT_EQ(f.themes(), (std::vector<std::string>{"дождь", "лес"}));
    EXPECT_TRUE(platform::Storage(other + "themes/").exists("лес.ini"));
    EXPECT_FALSE(platform::Storage(other + "themes/").exists("заметки.txt"));
}

TEST_F(PlayerFilesTest, SecondChangeInAMinuteGetsItsOwnBackup) {
    const std::string other = dir_ + "other/";
    PlayerFiles f = files();
    core::Profile p = f.load(messages_);
    // Туда-обратно с правкой профиля: в папке по умолчанию заменяется дважды.
    for (const std::string& to : {other, dir_, other, dir_}) {
        ++p.settings.preset.lives;
        ASSERT_TRUE(f.save(p, error_)) << error_;
        ASSERT_TRUE(f.change_folder(to, kWhen, messages_).has_value()) << to;
    }
    EXPECT_TRUE(platform::Storage(dir_ + "backup_20261016_1532/").exists("profile1.ini"));
    EXPECT_TRUE(platform::Storage(dir_ + "backup_20261016_1532_2/").exists("profile1.ini"));
    EXPECT_EQ(f.load(messages_).settings.preset.lives, 19);
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
