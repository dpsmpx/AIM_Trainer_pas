// Замерщик ввода без библиотеки — Win32 и GDI (docs/LIB_PROBES.md). Метка
// клика — GetMessageTime, тот же счётчик тиков Windows, из которого SDL3
// берёт время события; клик программа получает в оконной процедуре.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

#include <string>

#include "probe_common.h"

namespace {

probe::Session* g_session = nullptr;
bool g_running = true;

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
            const DWORD tick = GetTickCount();
            const probe::Ns t = probe::now();
            const DWORD age_ms = tick - static_cast<DWORD>(GetMessageTime());
            const int button = msg == WM_LBUTTONDOWN ? 1 : msg == WM_RBUTTONDOWN ? 2 : 3;
            if (g_session)
                g_session->click(button, static_cast<float>(GET_X_LPARAM(lp)),
                                 static_cast<float>(GET_Y_LPARAM(lp)), t,
                                 probe::from_age(t, static_cast<probe::Ns>(age_ms) * 1'000'000ULL));
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) g_running = false;
            return 0;
        case WM_CLOSE:
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
    HBRUSH bg = CreateSolidBrush(RGB(28, 28, 32));
    HBRUSH target = CreateSolidBrush(RGB(97, 207, 255));
    HBRUSH selftest = CreateSolidBrush(RGB(255, 200, 40));
    SelectObject(frame_dc, GetStockObject(NULL_PEN));

    probe::LibInfo info;
    info.name = "WinAPI";
    info.version = "Win32";
    info.renderer = "GDI (BitBlt)";
    info.timestamp = "мс GetMessageTime (счётчик тиков Windows, как у SDL3) в оконной процедуре";
    probe::Session session(info, hwnd);
    g_session = &session;

    while (g_running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_running = false;
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
        const RECT bar{0, 0, static_cast<int>(v.progress * static_cast<float>(probe::kWidth)), 6};
        FillRect(frame_dc, &bar, v.selftest ? selftest : target);
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
    DeleteObject(selftest);
    ReleaseDC(hwnd, window_dc);
    DestroyWindow(hwnd);
    return 0;
}
