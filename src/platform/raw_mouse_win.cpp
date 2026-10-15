// Свой поток сырого ввода мыши на Windows (D-24). Устроен так же, как поток
// сырого ввода SDL3 (src/video/windows/SDL_windowsrawinput.c): окно только для
// сообщений, мышь без флагов регистрации, приоритет TIME_CRITICAL, ожидание
// MsgWaitForMultipleObjectsEx до ввода или сигнала остановки.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <SDL3/SDL_timer.h>
#include <windows.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "platform/raw_mouse.h"

namespace aim::platform {

namespace {

using core::MouseButton;

constexpr wchar_t kWindowClass[] = L"AimTrainerRawMouse";

// Сколько нажатий ждут drain; главный цикл забирает их каждый кадр, так что
// предел нужен, только если цикл стоит.
constexpr std::size_t kMaxQueued = 256;

// Сырой ввод мыши в процессе получает один приёмник (RegisterRawInputDevices
// для одного вида устройств — одна запись на процесс).
std::atomic<bool> g_receiver_taken{false};

// Нажатия, которые Windows делает из касаний экрана и пера, помечены подписью
// в ulExtraInformation (MI_WP_SIGNATURE); SDL такие сырые события тоже
// отбрасывает (GetMouseMessageSource в SDL_windowsevents.c).
constexpr ULONG kPenOrTouchMask = 0xFFFFFF00;
constexpr ULONG kPenOrTouchSignature = 0xFF515700;

std::string win32_error(const char* call) {
    return std::string(call) + ": ошибка Windows " + std::to_string(GetLastError());
}

}  // namespace

struct RawMouse::Impl {
    std::thread thread;
    HANDLE stop_event = nullptr;
    bool owns_receiver = false;

    std::mutex mutex;
    std::condition_variable started_cv;
    bool started = false;  // под mutex; active и problem пишутся до него
    bool active = false;
    std::string problem;
    std::vector<core::RawPress> queue;  // под mutex

    void run();
    void handle(HRAWINPUT input, core::Nanoseconds time);
    void finish_start(bool ok, std::string why);
};

void RawMouse::Impl::finish_start(bool ok, std::string why) {
    const std::lock_guard lock(mutex);
    active = ok;
    problem = std::move(why);
    started = true;
    started_cv.notify_one();
}

void RawMouse::Impl::run() {
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = DefWindowProcW;  // DefWindowProcW освобождает данные WM_INPUT
    wc.hInstance = instance;
    wc.lpszClassName = kWindowClass;
    if (RegisterClassExW(&wc) == 0) {
        finish_start(false, win32_error("RegisterClassExW"));
        return;
    }
    HWND window = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                  instance, nullptr);
    if (window == nullptr) {
        finish_start(false, win32_error("CreateWindowExW"));
        UnregisterClassW(kWindowClass, instance);
        return;
    }
    RAWINPUTDEVICE device{};
    device.usUsagePage = 0x01;  // Generic Desktop
    device.usUsage = 0x02;      // мышь
    device.dwFlags = 0;  // ввод — только пока программа впереди
    device.hwndTarget = window;
    if (RegisterRawInputDevices(&device, 1, sizeof device) == FALSE) {
        finish_start(false, win32_error("RegisterRawInputDevices"));
        DestroyWindow(window);
        UnregisterClassW(kWindowClass, instance);
        return;
    }
    // Метка ставится, как только поток получил WM_INPUT: поток не должен
    // ждать, пока система отдаёт процессор другим.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    finish_start(true, {});

    for (;;) {
        // MWMO_INPUTAVAILABLE — проснуться и на ввод, который уже лежит в
        // очереди, а не только на новый.
        const DWORD woke =
            MsgWaitForMultipleObjectsEx(1, &stop_event, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (woke != WAIT_OBJECT_0 + 1) break;  // сигнал остановки или ошибка
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE) {
            if (msg.message == WM_INPUT) {
                handle(reinterpret_cast<HRAWINPUT>(msg.lParam), SDL_GetTicksNS());
            }
            DispatchMessageW(&msg);
        }
    }

    device.dwFlags = RIDEV_REMOVE;
    device.hwndTarget = nullptr;
    RegisterRawInputDevices(&device, 1, sizeof device);
    DestroyWindow(window);
    UnregisterClassW(kWindowClass, instance);
}

void RawMouse::Impl::handle(HRAWINPUT input, core::Nanoseconds time) {
    RAWINPUT data{};
    UINT size = sizeof data;
    if (GetRawInputData(input, RID_INPUT, &data, &size, sizeof(RAWINPUTHEADER)) ==
        static_cast<UINT>(-1)) {
        return;
    }
    if (data.header.dwType != RIM_TYPEMOUSE) return;
    const RAWMOUSE& mouse = data.data.mouse;
    if ((mouse.ulExtraInformation & kPenOrTouchMask) == kPenOrTouchSignature) return;
    const USHORT flags = mouse.usButtonFlags;
    constexpr USHORT kAnyDown = RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_DOWN |
                                RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_BUTTON_4_DOWN |
                                RI_MOUSE_BUTTON_5_DOWN;
    if ((flags & kAnyDown) == 0) return;

    // Кнопки настоящей мыши в сыром вводе — физические. Для левши Windows
    // переставляет их уже в сообщениях окна, и SDL отдаёт логические; здесь
    // перестановка — как у SDL (WIN_SwapButtons): у события без устройства
    // (сенсорная панель, SendInput) кнопки уже переставлены.
    const bool swap = data.header.hDevice != nullptr && GetSystemMetrics(SM_SWAPBUTTON) != 0;
    const std::lock_guard lock(mutex);
    const auto push = [&](MouseButton button) {
        if (queue.size() >= kMaxQueued) queue.erase(queue.begin());
        queue.push_back({time, button});
    };
    if ((flags & RI_MOUSE_LEFT_BUTTON_DOWN) != 0)
        push(swap ? MouseButton::Right : MouseButton::Left);
    if ((flags & RI_MOUSE_RIGHT_BUTTON_DOWN) != 0)
        push(swap ? MouseButton::Left : MouseButton::Right);
    if ((flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) != 0) push(MouseButton::Middle);
    if ((flags & RI_MOUSE_BUTTON_4_DOWN) != 0) push(MouseButton::X1);
    if ((flags & RI_MOUSE_BUTTON_5_DOWN) != 0) push(MouseButton::X2);
}

RawMouse::RawMouse() : impl_(std::make_unique<Impl>()) {
    if (g_receiver_taken.exchange(true)) {
        impl_->problem = "сырой ввод мыши уже получает другой приёмник в этой программе";
        return;
    }
    impl_->owns_receiver = true;
    impl_->stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (impl_->stop_event == nullptr) {
        impl_->problem = win32_error("CreateEventW");
        return;
    }
    // Часы SDL заводятся до потока: метки потока и событий SDL — одна шкала.
    static_cast<void>(SDL_GetTicksNS());
    try {
        impl_->thread = std::thread([impl = impl_.get()] { impl->run(); });
    } catch (const std::system_error& e) {
        impl_->problem = std::string("поток сырого ввода не запущен: ") + e.what();
        return;
    }
    std::unique_lock lock(impl_->mutex);
    impl_->started_cv.wait(lock, [this] { return impl_->started; });
}

RawMouse::~RawMouse() {
    if (impl_->thread.joinable()) {
        SetEvent(impl_->stop_event);
        impl_->thread.join();
    }
    if (impl_->stop_event != nullptr) CloseHandle(impl_->stop_event);
    if (impl_->owns_receiver) g_receiver_taken = false;
}

bool RawMouse::active() const {
    return impl_->active;
}

const std::string& RawMouse::problem() const {
    return impl_->problem;
}

void RawMouse::drain(std::vector<core::RawPress>& out) {
    const std::lock_guard lock(impl_->mutex);
    out.insert(out.end(), impl_->queue.begin(), impl_->queue.end());
    impl_->queue.clear();
}

}  // namespace aim::platform
