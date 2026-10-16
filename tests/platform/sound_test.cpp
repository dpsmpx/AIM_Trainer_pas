// Звук через SDL3 (platform/sound.h): тон уходит в поток и вызов не ждёт
// звука (AUD-06). Драйвер — dummy: устройства вывода у теста нет.

#include "platform/sound.h"

#include <SDL3/SDL.h>
#include <gtest/gtest.h>

namespace aim::platform {
namespace {

class SoundTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        ASSERT_TRUE(SDL_Init(0)) << SDL_GetError();
    }
    static void TearDownTestSuite() { SDL_Quit(); }
};

TEST_F(SoundTest, BeepDoesNotWaitForTheSound) {
    Sound sound;
    ASSERT_TRUE(sound.active()) << sound.problem();
    const Uint64 start = SDL_GetTicksNS();
    sound.beep(core::kGameOverBeep);  // 220 мс звука
    const Uint64 spent = SDL_GetTicksNS() - start;
    EXPECT_LT(spent, 20'000'000U) << "вызов ждал звук";
    EXPECT_GT(sound.queued_bytes(), 0);
}

TEST_F(SoundTest, NewBeepReplacesQueued) {
    Sound sound;
    ASSERT_TRUE(sound.active()) << sound.problem();
    sound.beep(core::kGameOverBeep);
    sound.beep(core::kHitBeep);
    // В очереди — не больше короткого тона: длинный оборван.
    EXPECT_LE(sound.queued_bytes(), 1'440 * static_cast<int>(sizeof(float)));
}

}  // namespace
}  // namespace aim::platform
