// Замерщик ввода без библиотеки — Win32 и GDI (docs/LIB_PROBES.md). Метка
// клика — msg.time (GetMessageTime), тот же счётчик тиков Windows, из которого
// SDL3 берёт время события, и пересчитана в шкалу QueryPerformanceCounter так
// же, как в SDL3; клик программа получает в оконной процедуре.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

#include <cstdint>
#include <string>

#include "probe_common.h"

namespace {

probe::Session* g_session = nullptr;
bool g_running = true;

// msg.time → шкала now() — как WIN_GetEventTimestamp в SDL3
// (src/video/windows/SDL_windowsevents.c): постоянное смещение между счётчиком
// тиков и QPC берётся по сообщению, которое пришло с наименьшей задержкой, и
// метка не бывает позже момента получения. Смещение уточняется по каждому
// сообщению окна, как у SDL3; счётчик тиков 32-битный и раз в 49,7 суток
// переполняется.
class MsgTimeClock {
public:
    void observe(DWORD msg_time, probe::Ns at) {
        const std::int64_t ts = extend(msg_time) * 1'000'000;
        const std::int64_t offset = static_cast<std::int64_t>(at) - ts;
        if (!have_offset_ || offset < offset_) offset_ = offset;
        have_offset_ = true;
    }
    probe::Ns to_ns(DWORD msg_time, probe::Ns at) {
        observe(msg_time, at);
        const std::int64_t ns = extend(msg_time) * 1'000'000 + offset_;
        return ns < 0 ? 0 : static_cast<probe::Ns>(ns);
    }

private:
    std::int64_t extend(DWORD t) {
        if (have_last_ && t < last_ && last_ - t > 0x80000000U) ++wraps_;
        last_ = t;
        have_last_ = true;
        return static_cast<std::int64_t>((wraps_ << 32) | t);
    }
    std::uint64_t wraps_ = 0;
    DWORD last_ = 0;
    bool have_last_ = false;
    std::int64_t offset_ = 0;
    bool have_offset_ = false;
};

MsgTimeClock g_msg_clock;

std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            const probe::Ns t = probe::now();
            const auto msg_time = static_cast<DWORD>(GetMessageTime());
            const int button = msg == WM_LBUTTONDOWN ? 1 : msg == WM_RBUTTONDOWN ? 2 : 3;
            if (g_session)
                g_session->click(button, static_cast<float>(GET_X_LPARAM(lp)),
                                 static_cast<float>(GET_Y_LPARAM(lp)), t,
                                 g_msg_clock.to_ns(msg_time, t), std::to_string(msg_time));
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                if (g_session) g_session->note("выход: Esc");
                g_running = false;
            }
            return 0;
        case WM_CLOSE:
            if (g_session) g_session->note("выход: окно закрыто (WM_CLOSE)");
            g_running = false;
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

}  // namespace

int main() {
    const HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wc.lpszClassName = L"AimTrainerProbeWinAPI";
    if (!RegisterClassExW(&wc)) return 1;
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rc{0, 0, probe::kWidth, probe::kHeight};
    AdjustWindowRect(&rc, style, FALSE);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"WinAPI", style, CW_USEDEFAULT, CW_USEDEFAULT,
                                rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, inst,
                                nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    // Кадр рисуется в памяти и одним BitBlt выводится в окно.
    HDC window_dc = GetDC(hwnd);
    HDC frame_dc = CreateCompatibleDC(window_dc);
    HBITMAP frame_bmp = CreateCompatibleBitmap(window_dc, probe::kWidth, probe::kHeight);
    HGDIOBJ old_bmp = SelectObject(frame_dc, frame_bmp);
    HBRUSH bg = CreateSolidBrush(RGB(probe::kBackground.r, probe::kBackground.g, probe::kBackground.b));
    HBRUSH target = CreateSolidBrush(RGB(probe::kTarget.r, probe::kTarget.g, probe::kTarget.b));
    SelectObject(frame_dc, GetStockObject(NULL_PEN));

    probe::LibInfo info;
    info.name = "WinAPI";
    info.version = "Win32";
    info.renderer = "GDI (BitBlt)";
    info.timestamp =
        "мс msg.time (GetMessageTime, счётчик тиков Windows), пересчитанные в шкалу QPC как в SDL3";
    probe::Session session(info, hwnd);
    g_session = &session;

    while (g_running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            g_msg_clock.observe(msg.time, probe::now());
            if (msg.message == WM_QUIT) {
                session.note("выход: WM_QUIT");
                g_running = false;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        const probe::View v = session.frame();
        const RECT all{0, 0, probe::kWidth, probe::kHeight};
        FillRect(frame_dc, &all, bg);
        if (v.target) {
            SelectObject(frame_dc, target);
            const int l = static_cast<int>(v.x - v.r), t = static_cast<int>(v.y - v.r);
            const int r = static_cast<int>(v.x + v.r), b = static_cast<int>(v.y + v.r);
            Ellipse(frame_dc, l, t, r, b);
        }
        const RECT bar{0, 0, static_cast<int>(v.progress * static_cast<float>(probe::kWidth)),
                       static_cast<int>(v.bar_h)};
        HBRUSH bar_brush = CreateSolidBrush(RGB(v.bar.r, v.bar.g, v.bar.b));
        FillRect(frame_dc, &bar, bar_brush);
        DeleteObject(bar_brush);
        const std::string title = session.take_title();
        if (!title.empty()) SetWindowTextW(hwnd, wide(title).c_str());
        const probe::Ns call = probe::now();
        BitBlt(window_dc, 0, 0, probe::kWidth, probe::kHeight, frame_dc, 0, 0, SRCCOPY);
        GdiFlush();
        session.presented(call, probe::now());
    }
    session.finish();
    g_session = nullptr;
    SelectObject(frame_dc, old_bmp);
    DeleteObject(frame_bmp);
    DeleteDC(frame_dc);
    DeleteObject(bg);
    DeleteObject(target);
    ReleaseDC(hwnd, window_dc);
    DestroyWindow(hwnd);
    return 0;
}
