// Одна копия программы на папку по умолчанию (D-49): вторая копия той же
// папки — занята, другая папка — свободна, после выхода первой — свободна.

#include "platform/single_instance.h"

#include <SDL3/SDL.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace aim::platform {
namespace {

TEST(SingleInstance, SecondCopyOfSameFolderIsBusy) {
    const std::string dir = "single_instance_test/";
    const std::string other_dir = "single_instance_test_other/";
    for (const std::string& d : {dir, other_dir}) {
        std::filesystem::remove_all(d);
        ASSERT_TRUE(SDL_CreateDirectory(d.c_str()));
    }
    {
        const SingleInstance first(dir);
        ASSERT_EQ(first.state(), SingleInstance::State::Owned) << first.problem();
        const SingleInstance second(dir);
        EXPECT_EQ(second.state(), SingleInstance::State::Busy);
        const SingleInstance other(other_dir);
        EXPECT_EQ(other.state(), SingleInstance::State::Owned) << "своя папка — своя копия";
    }
    const SingleInstance again(dir);
    EXPECT_EQ(again.state(), SingleInstance::State::Owned) << "первая вышла — папка свободна";
    for (const std::string& d : {dir, other_dir})
        std::filesystem::remove_all(d);
}

}  // namespace
}  // namespace aim::platform
