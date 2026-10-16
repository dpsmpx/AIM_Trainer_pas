#pragma once

// Звук через SDL3 (ROADMAP, шаг «Звук»): тон уходит в поток вывода и играет
// сам, кадр его не ждёт — у образца Console.Beep останавливал программу на
// всё время звука и завышал реакцию (AUD-06). Нет устройства вывода —
// программа работает молча.

#include <SDL3/SDL.h>

#include <string>

#include "core/tone.h"

namespace aim::platform {

class Sound {
public:
    Sound();
    ~Sound();
    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;

    [[nodiscard]] bool active() const { return stream_ != nullptr; }
    [[nodiscard]] const std::string& problem() const { return problem_; }

    /// Сыграть тон сразу, оборвав прежний, — без ожидания.
    void beep(core::Beep beep);
    /// Байт в очереди потока — для проверки.
    [[nodiscard]] int queued_bytes() const;

private:
    SDL_AudioStream* stream_ = nullptr;
    std::string problem_;
    bool subsystem_ = false;
};

}  // namespace aim::platform
