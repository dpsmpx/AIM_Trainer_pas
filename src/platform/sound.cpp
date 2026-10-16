#include "platform/sound.h"

#include <vector>

namespace aim::platform {

namespace {

constexpr int kSampleRate = 48'000;
constexpr float kVolume = 0.25F;

}  // namespace

Sound::Sound() {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        problem_ = SDL_GetError();
        return;
    }
    subsystem_ = true;
    const SDL_AudioSpec spec{.format = SDL_AUDIO_F32, .channels = 1, .freq = kSampleRate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (stream_ == nullptr) {
        problem_ = SDL_GetError();
        return;
    }
    SDL_ResumeAudioStreamDevice(stream_);
}

Sound::~Sound() {
    if (stream_ != nullptr) SDL_DestroyAudioStream(stream_);
    if (subsystem_) SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void Sound::beep(core::Beep beep) {
    if (stream_ == nullptr) return;
    const std::vector<float> samples = core::tone(beep, kSampleRate, kVolume);
    // Новый звук обрывает прежний: очередь не копится, тон — вовремя.
    SDL_ClearAudioStream(stream_);
    SDL_PutAudioStreamData(stream_, samples.data(),
                           static_cast<int>(samples.size() * sizeof(float)));
}

int Sound::queued_bytes() const {
    return stream_ != nullptr ? SDL_GetAudioStreamQueued(stream_) : 0;
}

}  // namespace aim::platform
