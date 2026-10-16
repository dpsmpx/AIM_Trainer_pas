// Файлы игрока на диске (D-30): запись целиком, дописывание, замена старого
// файла, папка из AIM_TRAINER_DATA_DIR. Каталоги — временные в текущем.

#include "platform/storage.h"

#include <SDL3/SDL.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>

namespace aim::platform {
namespace {

// Каталог у каждого теста свой: CTest запускает их параллельно.
std::string test_dir() {
    return std::string("storage_test_") +
           ::testing::UnitTest::GetInstance()->current_test_info()->name();
}

class StorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::remove_all(dir_);
        ASSERT_TRUE(SDL_CreateDirectory(dir_.c_str()));
    }
    void TearDown() override {
        SDL_unsetenv_unsafe("AIM_TRAINER_DATA_DIR");
        std::filesystem::remove_all(dir_);
    }

    std::string dir_ = test_dir();
    Storage storage_{dir_ + "/"};
    std::string error_;
};

TEST_F(StorageTest, MissingFileReadsAsNothing) {
    EXPECT_FALSE(storage_.exists("profile1_stats.ini"));
    EXPECT_EQ(storage_.read("profile1_stats.ini"), std::nullopt);
}

TEST_F(StorageTest, WriteThenReadGivesSameBytes) {
    const std::string text = "[Stats]\r\nBestScore=42\r\n";
    ASSERT_TRUE(storage_.write("profile1_stats.ini", text, error_)) << error_;
    EXPECT_TRUE(storage_.exists("profile1_stats.ini"));
    EXPECT_EQ(storage_.read("profile1_stats.ini"), text);
}

TEST_F(StorageTest, WriteReplacesOldFileAndLeavesNoTemp) {
    ASSERT_TRUE(storage_.write("a.ini", "old and longer", error_)) << error_;
    ASSERT_TRUE(storage_.write("a.ini", "new", error_)) << error_;
    EXPECT_EQ(storage_.read("a.ini"), "new");
    EXPECT_FALSE(storage_.exists("a.ini.tmp"));
}

TEST_F(StorageTest, AppendCreatesThenAdds) {
    ASSERT_TRUE(storage_.append("s.csv", "head\n", error_)) << error_;
    ASSERT_TRUE(storage_.append("s.csv", "row\n", error_)) << error_;
    EXPECT_EQ(storage_.read("s.csv"), "head\nrow\n");
}

TEST_F(StorageTest, FailureNamesTheFileNotItsFolder) {
    const Storage missing(dir_ + "/no/such/");
    EXPECT_FALSE(missing.write("a.ini", "x", error_));
    EXPECT_NE(error_.find("a.ini.tmp"), std::string::npos) << error_;
    EXPECT_EQ(error_.find("no/such"), std::string::npos) << error_;
    // Вместо файла — каталог: дописать нельзя.
    ASSERT_TRUE(SDL_CreateDirectory((dir_ + "/s.csv").c_str()));
    EXPECT_FALSE(storage_.append("s.csv", "row\n", error_));
    EXPECT_NE(error_.find("s.csv"), std::string::npos) << error_;
    EXPECT_EQ(error_.find(dir_), std::string::npos) << error_;
}

TEST_F(StorageTest, ForcedFolderIsCreated) {
    const std::string dir = dir_ + "/data/player";
    ASSERT_EQ(SDL_setenv_unsafe("AIM_TRAINER_DATA_DIR", dir.c_str(), 1), 0);
    std::string problem;
    const Storage forced = Storage::open_user_folder(problem).value_or(Storage(""));
    ASSERT_EQ(forced.dir(), dir + "/") << problem;
    ASSERT_TRUE(forced.write("a.ini", "x", error_)) << error_;
    EXPECT_EQ(forced.read("a.ini"), "x");
}

}  // namespace
}  // namespace aim::platform
