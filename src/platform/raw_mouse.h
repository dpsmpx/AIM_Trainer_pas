#pragma once

// Свой поток сырого ввода мыши (D-24): время нажатия для замера реакции.

#include <memory>
#include <string>
#include <vector>

#include "core/click_time.h"

namespace aim::platform {

/// Поток сырого ввода мыши.
///
/// На Windows — отдельный поток с невидимым окном сообщений: мышь
/// зарегистрирована RegisterRawInputDevices без флагов (ввод приходит, пока
/// программа впереди), приоритет потока — THREAD_PRIORITY_TIME_CRITICAL, и на
/// каждое WM_INPUT с нажатием поток сразу ставит метку SDL_GetTicksNS. На
/// других системах потока нет: active() — false, нажатий нет, а время клика —
/// из события SDL.
///
/// Сырой ввод мыши в процессе получает один приёмник, поэтому приёмник — один
/// на процесс: второй экземпляр, пока жив первый, не запускается. По той же
/// причине программа не включает режим relative SDL (D-24).
///
/// Создаётся после SDL_Init: метки — часы SDL.
class RawMouse {
public:
    RawMouse();
    ~RawMouse();
    RawMouse(const RawMouse&) = delete;
    RawMouse& operator=(const RawMouse&) = delete;
    RawMouse(RawMouse&&) = delete;
    RawMouse& operator=(RawMouse&&) = delete;

    /// Поток работает и мышь зарегистрирована.
    [[nodiscard]] bool active() const;

    /// Почему потока нет — для журнала и заголовка окна; при работе — пусто.
    [[nodiscard]] const std::string& problem() const;

    /// Дописать в out нажатия, пришедшие с прошлого вызова, по порядку времени.
    void drain(std::vector<core::RawPress>& out);

    /// Ждать нажатия не дольше timeout, нс: true — есть нажатия для drain.
    /// Поток будит ожидание сразу, как поставил метку. Без потока — сразу false.
    bool wait_for_press(core::Nanoseconds timeout);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace aim::platform
