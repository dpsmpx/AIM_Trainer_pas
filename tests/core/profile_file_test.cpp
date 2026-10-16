// Файлы игрока — свой формат с версией (D-38): профиль profileN.ini,
// aim_trainer.ini (D-30, D-33), имена файлов игрока для смены папки (D-35).

#include "core/profile_file.h"

#include <gtest/gtest.h>

#include <string>

namespace aim::core {
namespace {

TEST(ProfileFile, WriteThenReadKeepsEverything) {
    Profile p;
    p.preset = 4;
    p.tutorial_seen = true;
    p.settings.preset = {
        .target_size = 32, .lifetime_ms = 1500, .lives = 7, .ending = Ending::Time};
    p.settings.average_runs = 100;
    p.settings.sound = false;
    p.settings.menu_effects = false;
    p.settings.theme = "моя тема";
    p.settings.hud = false;
    p.settings.replays = false;
    p.settings.font = 1;
    p.achievements[1] = true;
    p.achievements[7] = true;
    p.custom = {.stimulus = Stimulus::Screen,
                .action = Action::Hover,
                .color_buttons = true,
                .moving = true,
                .grows = false,
                .bombs = false,
                .target_size = 22,
                .lifetime_ms = 4200,
                .ending = Ending::Time,
                .lives = 3,
                .duration_s = 120};
    const std::string text = write_profile(p, "\r\n");
    EXPECT_TRUE(text.starts_with(kProfileVersionLine));
    const LoadedProfile loaded = read_profile(text);
    EXPECT_FALSE(loaded.corrupt);
    EXPECT_TRUE(loaded.reset_fields.empty());
    EXPECT_EQ(write_profile(loaded.profile, "\r\n"), text);
    EXPECT_EQ(loaded.profile.preset, 4);
    EXPECT_EQ(loaded.profile.settings.preset.ending, Ending::Time);
    EXPECT_TRUE(loaded.profile.achievements[7]);
    EXPECT_FALSE(loaded.profile.achievements[0]);
    EXPECT_EQ(loaded.profile.custom, p.custom) << "«Свой набор» — как оставлен, даже не к замеру";
    EXPECT_EQ(loaded.profile.settings.font, 1) << "Mono (D-43)";
    EXPECT_FALSE(loaded.profile.settings.hud);
    EXPECT_FALSE(loaded.profile.settings.replays);
}

TEST(ProfileFile, FontWordOrDefault) {
    const std::string base = std::string(kProfileVersionLine) + "\n";
    EXPECT_EQ(read_profile(base + "font=mono\n").profile.settings.font, 1);
    const LoadedProfile odd = read_profile(base + "font=comic\n");
    EXPECT_EQ(odd.profile.settings.font, 0) << "не то слово — Condensed";
    EXPECT_EQ(odd.reset_fields.size(), 1U);
    EXPECT_NE(write_profile(Profile{}, "\n").find("font=condensed"), std::string::npos);
}

TEST(ProfileFile, CustomItemAndItsBadValues) {
    const std::string text = std::string(kProfileVersionLine) +
                             "\npreset=5\ncustom.stimulus=laser\ncustom.buttons=color\n"
                             "custom.size=500\ncustom.lives=7\n";
    const LoadedProfile loaded = read_profile(text);
    EXPECT_EQ(loaded.profile.preset, kCustomItem);
    EXPECT_EQ(loaded.profile.custom.stimulus, Stimulus::Target) << "слово не то — по умолчанию";
    EXPECT_TRUE(loaded.profile.custom.color_buttons);
    EXPECT_EQ(loaded.profile.custom.target_size, 40) << "вне границ — по умолчанию";
    EXPECT_EQ(loaded.profile.custom.lives, 7);
    EXPECT_EQ(loaded.reset_fields.size(), 2U) << "custom.stimulus и custom";
}

TEST(ProfileFile, ForeignFileIsCorrupt) {
    // Профиль Pascal-версии и прежний формат — не этой программы (D-38).
    EXPECT_TRUE(read_profile("[Stats]\nBestScore=10\n").corrupt);
    EXPECT_TRUE(read_profile("").corrupt);
    EXPECT_TRUE(read_profile("# DP AIM Trainer profile v2\npreset=1\n").corrupt);
}

TEST(ProfileFile, BadValuesResetOthersKept) {
    const std::string text = std::string(kProfileVersionLine) +
                             "\npreset=9\nlives=abc\nsound=2\ntarget_size=50\n"
                             "theme=1\nachievement.sniper=1\nunknown_key=5\nno equals sign\n";
    const LoadedProfile loaded = read_profile(text);
    EXPECT_FALSE(loaded.corrupt);
    EXPECT_EQ(loaded.reset_fields.size(), 3U) << "preset, lives, sound";
    const Profile& p = loaded.profile;
    EXPECT_EQ(p.preset, Profile{}.preset);
    EXPECT_EQ(p.settings.preset.lives, 15);
    EXPECT_TRUE(p.settings.sound);
    EXPECT_EQ(p.settings.preset.target_size, 50);
    EXPECT_EQ(p.settings.theme, "neon") << "номер первых сборок — встроенная тема";
    EXPECT_TRUE(p.achievements[5]);
}

TEST(ProfileFile, SettingsOutOfRangeGoToDefault) {
    const std::string text =
        std::string(kProfileVersionLine) + "\nlifetime_ms=100\naverage_runs=9000\n";
    const LoadedProfile loaded = read_profile(text);
    EXPECT_EQ(loaded.profile.settings.preset.lifetime_ms, 1000);
    EXPECT_EQ(loaded.profile.settings.average_runs, 50);
    EXPECT_EQ(loaded.reset_fields.size(), 1U);
}

TEST(ProfileFile, ToleratesBomAndLineEndings) {
    const std::string text = "\xEF\xBB\xBF" + std::string(kProfileVersionLine) + "\r\npreset=0\r\n";
    const LoadedProfile loaded = read_profile(text);
    EXPECT_FALSE(loaded.corrupt);
    EXPECT_EQ(loaded.profile.preset, 0);
}

TEST(ProfileFile, NamesAndAppState) {
    EXPECT_EQ(profile_file_name(2), "profile2.ini");
    EXPECT_EQ(runs_file_name(3), "profile3_runs.csv");
    EXPECT_EQ(corrupt_backup_file_name(1), "profile1_corrupt_backup.txt");
    EXPECT_EQ(
        read_app_state(write_app_state({.last_profile = 3, .data_dir = {}}, "\r\n")).last_profile,
        3);
    EXPECT_EQ(read_app_state("LastProfile=9\n").last_profile, 1);
    EXPECT_EQ(read_app_state("").last_profile, 1);
}

TEST(ProfileFile, AppStateKeepsChosenFolder) {
    // Путь с пробелами, кириллицей и «=» — как есть (D-33).
    const AppState state{.last_profile = 2, .data_dir = "D:\\Мои игры\\a=b\\"};
    const AppState read = read_app_state(write_app_state(state, "\r\n"));
    EXPECT_EQ(read.last_profile, 2);
    EXPECT_EQ(read.data_dir, state.data_dir);
    EXPECT_EQ(write_app_state({.last_profile = 1, .data_dir = {}}, "\n"), "[App]\nLastProfile=1\n")
        << "без выбранной папки — без строки DataDir";
}

TEST(ProfileFile, PlayerFilesForFolderChange) {
    for (const char* name : {"profile1.ini", "profile4_runs.csv", "profile2_corrupt_backup.txt",
                             "export_profile3_20261016.csv"})
        EXPECT_TRUE(is_player_file(name)) << name;
    for (const char* name :
         {"aim_trainer.ini", "profile.ini", "profile1.ini.tmp", "profile1_stats.ini",
          "export_profile1_2026.csv", "export_profile_20261016.csv", "notes.txt"})
        EXPECT_FALSE(is_player_file(name)) << name;
}

TEST(ProfileFile, BackupFolderNameHasMinuteAndAttempt) {
    const LocalTime when{.year = 2026, .month = 10, .day = 6, .hour = 9, .minute = 5};
    EXPECT_EQ(backup_folder_name(when, 1), "backup_20261006_0905");
    EXPECT_EQ(backup_folder_name(when, 2), "backup_20261006_0905_2");
    EXPECT_FALSE(is_player_file(backup_folder_name(when, 1))) << "папка копий не копируется";
}

}  // namespace
}  // namespace aim::core
