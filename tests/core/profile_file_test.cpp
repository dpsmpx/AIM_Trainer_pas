// Файл профиля и история — формат Pascal-версии (D-11). Эталон — файлы,
// которые записал образец (archive/pascal/) под Mono 16 октября 2026, в
// tools/gui.sh ref: три забега (Classic, Dual, Flick), «Время жизни» 900 мс.

#include "core/profile_file.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "core/history.h"
#include "core/text.h"

namespace aim::core {
namespace {

// Файл профиля, записанный образцом (перевод строки — LF: Mono на Linux).
constexpr std::string_view kPascalStats = R"([Stats]
BestScore=94
BestAccuracy=66.6666666666667
BestPrecision=80.2156177156177
BestAvgReaction=382.5
GamesPlayed=3
TutorialSeen=True
[Settings]
ShowMainMenuEffects=True
SetCircleSize=40
SetLifetimeMs=900
SetLives=15
SetSoundEnabled=True
CurrentTheme=0
[ReactionHistory]
ReactionCount=10
ReactionIndex=10
Reaction0=523
Reaction1=558
Reaction2=383
Reaction3=443
Reaction4=509
Reaction5=472
Reaction6=368
Reaction7=405
Reaction8=282
Reaction9=475
Reaction10=0
Reaction11=0
Reaction12=0
Reaction13=0
Reaction14=0
Reaction15=0
Reaction16=0
Reaction17=0
Reaction18=0
Reaction19=0
Reaction20=0
Reaction21=0
Reaction22=0
Reaction23=0
Reaction24=0
Reaction25=0
Reaction26=0
Reaction27=0
Reaction28=0
Reaction29=0
Reaction30=0
Reaction31=0
Reaction32=0
Reaction33=0
Reaction34=0
Reaction35=0
Reaction36=0
Reaction37=0
Reaction38=0
Reaction39=0
Reaction40=0
Reaction41=0
Reaction42=0
Reaction43=0
Reaction44=0
Reaction45=0
Reaction46=0
Reaction47=0
Reaction48=0
Reaction49=0
[Achievements]
Achv0=True
Achv1=False
Achv2=False
Achv3=False
Achv4=False
Achv5=False
Achv6=False
Achv7=False
[Leaderboard]
LBCount0=1
LB00S=94
LB00A=66.6666666666667
LBCount1=1
LB10S=64
LB10A=50
LBCount2=0
LBCount3=1
LB30S=0
LB30A=0
)";

// История, записанная образцом.
constexpr std::string_view kPascalSessions =
    R"(Date,Time,Hour,Mode,Score,Accuracy,AvgReaction,Hits,Misses,MaxCombo
2026-10-16,10:49,10,Classic,94,66.7,481.3,6,3,6
2026-10-16,10:49,10,Dual,64,50.0,382.5,4,4,2
2026-10-16,10:49,10,Flick,0,0.0,0.0,0,5,0
)";

std::string with_crlf(std::string_view text) {
    std::string out;
    for (const char c : text) {
        if (c == '\n') out += '\r';
        out += c;
    }
    return out;
}

std::string replace_line(std::string_view text, std::string_view from, std::string_view to) {
    std::string out(text);
    const std::size_t at = out.find(from);
    EXPECT_NE(at, std::string::npos) << from;
    if (at != std::string::npos) out.replace(at, from.size(), to);
    return out;
}

TEST(ProfileFile, PascalFileReadsAndWritesBackUnchanged) {
    const LoadedProfile loaded = read_stats(kPascalStats);
    ASSERT_FALSE(loaded.corrupt);
    EXPECT_TRUE(loaded.reset_fields.empty());
    EXPECT_EQ(write_stats(loaded.profile, "\n"), kPascalStats) << "D-11: запись файл не меняет";
    const std::string crlf = with_crlf(kPascalStats);
    EXPECT_EQ(write_stats(read_stats(crlf).profile, "\r\n"), crlf) << "так же на Windows";
}

TEST(ProfileFile, PascalFileValues) {
    const Profile p = read_stats(kPascalStats).profile;
    EXPECT_EQ(p.best_score, 94);
    EXPECT_DOUBLE_EQ(p.best_accuracy, 66.6666666666667);
    EXPECT_DOUBLE_EQ(p.best_avg_reaction, 382.5);
    EXPECT_EQ(p.games_played, 3);
    EXPECT_TRUE(p.tutorial_seen);
    EXPECT_EQ(p.settings.lifetime_ms, 900);
    EXPECT_TRUE(p.settings.menu_effects);
    EXPECT_EQ(p.reactions.count, 10);
    EXPECT_EQ(p.reactions.in_order().front(), 523);
    EXPECT_TRUE(p.achievements[0]);
    EXPECT_FALSE(p.achievements[1]);
    ASSERT_EQ(p.leaderboard[0].size(), 1U);
    EXPECT_EQ(p.leaderboard[0][0].score, 94);
    ASSERT_EQ(p.leaderboard[1].size(), 1U);
    EXPECT_DOUBLE_EQ(p.leaderboard[1][0].accuracy, 50);
    EXPECT_TRUE(p.leaderboard[2].empty());
    ASSERT_EQ(p.leaderboard[3].size(), 1U);
}

TEST(ProfileFile, NonNumberMakesFileCorruptLikeOriginal) {
    // Сценарий 13а: копия файла и всё по умолчанию.
    const LoadedProfile loaded =
        read_stats(replace_line(kPascalStats, "BestScore=94", "BestScore=abc"));
    EXPECT_TRUE(loaded.corrupt);
    EXPECT_EQ(loaded.profile.best_score, 0);
    EXPECT_EQ(loaded.profile.games_played, 0);
    EXPECT_FALSE(loaded.profile.tutorial_seen);
    EXPECT_TRUE(read_stats("LB0xS=5\n").corrupt) << "номер места — не цифра";
    EXPECT_TRUE(read_stats("BestAccuracy=\n").corrupt) << "пустое значение";
}

TEST(ProfileFile, OutOfRangeFieldsResetOthersKept) {
    // Сценарий 13б (AUD-07): образец с этим падал при запуске.
    std::string text = replace_line(kPascalStats, "ReactionCount=10", "ReactionCount=60");
    text = replace_line(text, "ReactionIndex=10", "ReactionIndex=-1");
    text = replace_line(text, "LBCount0=1", "LBCount0=11");
    const LoadedProfile loaded = read_stats(text);
    ASSERT_FALSE(loaded.corrupt);
    EXPECT_EQ(loaded.reset_fields, (std::vector<std::string>{"ReactionHistory", "LBCount0"}));
    const Profile& p = loaded.profile;
    EXPECT_EQ(p.reactions.count, 0);
    EXPECT_TRUE(p.reactions.in_order().empty());
    EXPECT_TRUE(p.leaderboard[0].empty());
    EXPECT_EQ(p.leaderboard[1].size(), 1U) << "соседние поля не тронуты";
    EXPECT_EQ(p.best_score, 94);
    EXPECT_EQ(p.games_played, 3);
}

TEST(ProfileFile, SettingsOutOfRangeGoToDefault) {
    const LoadedProfile loaded =
        read_stats("SetCircleSize=7\nSetLifetimeMs=90000\nSetLives=3\nCurrentTheme=9\n");
    EXPECT_EQ(loaded.profile.settings.circle_size, 40);
    EXPECT_EQ(loaded.profile.settings.lifetime_ms, 1000);
    EXPECT_EQ(loaded.profile.settings.lives, 15);
    EXPECT_EQ(loaded.profile.settings.theme, 0);
    EXPECT_EQ(loaded.reset_fields.size(), 4U);
}

TEST(ProfileFile, ToleratesBomUnknownKeysAndOddLines) {
    const LoadedProfile loaded =
        read_stats("\xEF\xBB\xBF[Stats]\r\nBestScore= 12 \r\nFoo=bar\r\nA=B=C\r\nno equals\r\n");
    EXPECT_FALSE(loaded.corrupt);
    EXPECT_EQ(loaded.profile.best_score, 12);
    const LoadedProfile empty = read_stats("");
    EXPECT_FALSE(empty.corrupt);
    EXPECT_EQ(empty.profile.settings.circle_size, 40);
}

TEST(ProfileFile, RealsLikeDotNet) {
    EXPECT_EQ(format_real(0.0), "0");
    EXPECT_EQ(format_real(-0.0), "0");
    EXPECT_EQ(format_real(50.0), "50");
    EXPECT_EQ(format_real(382.5), "382.5");
    EXPECT_EQ(format_real(200.0 / 3.0), "66.6666666666667");
    EXPECT_EQ(format_real(80.2156177156177), "80.2156177156177");
}

TEST(ProfileFile, NamesAndAppState) {
    EXPECT_EQ(stats_file_name(2), "profile2_stats.ini");
    EXPECT_EQ(sessions_file_name(3), "profile3_sessions.csv");
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
        << "без выбранной папки — файл как раньше";
}

TEST(ProfileFile, PlayerFilesForFolderChange) {
    for (const char* name : {"profile1_stats.ini", "profile4_sessions.csv",
                             "profile2_corrupt_backup.txt", "export_profile3_20261016.csv"})
        EXPECT_TRUE(is_player_file(name)) << name;
    for (const char* name :
         {"aim_trainer.ini", "profile_stats.ini", "profile1_stats.ini.tmp",
          "export_profile1_2026.csv", "export_profile_20261016.csv", "notes.txt"})
        EXPECT_FALSE(is_player_file(name)) << name;
}

TEST(ProfileFile, BackupFolderNameHasMinuteAndAttempt) {
    const LocalTime when{.year = 2026, .month = 10, .day = 6, .hour = 9, .minute = 5};
    EXPECT_EQ(backup_folder_name(when, 1), "backup_20261006_0905");
    EXPECT_EQ(backup_folder_name(when, 2), "backup_20261006_0905_2");
    EXPECT_FALSE(is_player_file(backup_folder_name(when, 1))) << "папка копий не копируется";
}

TEST(History, LineLikeOriginal) {
    GameSummary s;
    s.mode = GameMode::Dual;
    s.score = 64;
    s.accuracy = 50.0;
    s.avg_reaction_ms = 382.5;
    s.hits = 4;
    s.misses = 4;
    s.max_combo = 2;
    const std::vector<std::string_view> lines = split_lines(kPascalSessions);
    ASSERT_EQ(lines.size(), 4U);
    EXPECT_EQ(lines[0], kSessionsHeader);
    EXPECT_EQ(session_line(s, {.year = 2026, .month = 10, .day = 16, .hour = 10, .minute = 49}),
              lines[2]);
    s.mode = GameMode::Classic;
    s.score = 94;
    s.accuracy = 200.0 / 3.0;
    s.avg_reaction_ms = 2888.0 / 6.0;
    s.hits = 6;
    s.misses = 3;
    s.max_combo = 6;
    EXPECT_EQ(session_line(s, {.year = 2026, .month = 10, .day = 16, .hour = 10, .minute = 49}),
              lines[1]);
    EXPECT_EQ(
        session_line(s, {.year = 2027, .month = 1, .day = 2, .hour = 3, .minute = 4}).substr(0, 18),
        "2027-01-02,03:04,3");
}

TEST(History, ReadsRowsAndKeepsLast) {
    const std::vector<SessionRow> rows = read_sessions(kPascalSessions);
    ASSERT_EQ(rows.size(), 3U);
    EXPECT_EQ(rows[0].score, 94);
    EXPECT_EQ(rows[0].hour, 10);
    EXPECT_DOUBLE_EQ(rows[1].avg_reaction, 382.5);
    std::string many(kSessionsHeader);
    many += "\n";
    for (int i = 0; i < 600; ++i)
        many += std::to_string(i) + ",00:00,5,Classic," + std::to_string(i) + ",0.0,0.0,0,0,0\n";
    many += "broken,line\n";
    const std::vector<SessionRow> last = read_sessions(many);
    ASSERT_EQ(last.size(), kSessionsShown);
    EXPECT_EQ(last.front().score, 100) << "AUD-15: последние 500, а не первые";
    EXPECT_EQ(last.back().score, 599);
}

TEST(History, TimeOfDayAndRecentReactions) {
    EXPECT_EQ(time_of_day(6), 0);
    EXPECT_EQ(time_of_day(11), 0);
    EXPECT_EQ(time_of_day(12), 1);
    EXPECT_EQ(time_of_day(18), 2);
    EXPECT_EQ(time_of_day(23), 3);
    EXPECT_EQ(time_of_day(5), 3);
    const std::vector<SessionRow> rows{
        {.hour = 7, .score = 10, .avg_reaction = 300},
        {.hour = 8, .score = 20, .avg_reaction = 310},
        {.hour = 20, .score = 5, .avg_reaction = 320},
    };
    const auto by = scores_by_time_of_day(rows);
    EXPECT_EQ(by[0].games, 2);
    EXPECT_DOUBLE_EQ(by[0].average_score, 15);
    EXPECT_EQ(by[1].games, 0);
    EXPECT_EQ(by[2].games, 1);
    EXPECT_EQ(recent_avg_reactions(rows, 2), (std::vector<double>{310, 320}));
    EXPECT_EQ(export_file_name(2, {.year = 2026, .month = 10, .day = 6}),
              "export_profile2_20261006.csv");
}

}  // namespace
}  // namespace aim::core
