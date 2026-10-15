// Программа собрана с SDL той версии, что записана в D-18.

#include <SDL3/SDL.h>
#include <gtest/gtest.h>

namespace {

TEST(Sdl, VersionIsPinned) {
    EXPECT_EQ(SDL_VERSION, SDL_VERSIONNUM(3, 4, 18));
    EXPECT_EQ(SDL_GetVersion(), SDL_VERSIONNUM(3, 4, 18));
}

}  // namespace
