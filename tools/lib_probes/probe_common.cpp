// Общее ядро замерщиков ввода (docs/LIB_PROBES.md). Только Windows: эталон
// времени клика — сырой ввод (WM_INPUT) в своём потоке с отметкой
// QueryPerformanceCounter, а самопроверка — клики SendInput в известный момент.
#include "probe_common.h"

#ifndef _WIN32
#error "Замерщики ввода собираются только под Windows (docs/LIB_PROBES.md)."
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <timeapi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cwchar>
#include <format>
#include <mutex>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace probe {
namespace {

using namespace std::chrono_literals;

constexpr Ns kMs = 1'000'000ULL;
constexpr int kSelfTestClicks = 100;
constexpr int kTrials = 30;
constexpr float kRadius = 40.0f;
constexpr Ns kIntro = 3'000 * kMs;
constexpr Ns kTrialTimeout = 3'000 * kMs;
constexpr Ns kFinalizeDelay = 150 * kMs;  // поток сырого ввода успевает записать клик
constexpr std::size_t kSeriesCap = 2'000'000;
constexpr Rgb kBarSelfTest{255, 200, 40};
constexpr Rgb kBarFailed{230, 60, 50};

// ---------- время ----------

struct ClockBase {
    LARGE_INTEGER freq{};
    LARGE_INTEGER start{};
    ClockBase() {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
    }
};

const ClockBase& clock_base() {
    static const ClockBase base;
    return base;
}

Ns qpc_to_ns(LONGLONG qpc) {
    const ClockBase& c = clock_base();
    const LONGLONG d = qpc - c.start.QuadPart;
    if (d <= 0) return 0;
    const auto ud = static_cast<std::uint64_t>(d);
    const auto f = static_cast<std::uint64_t>(c.freq.QuadPart);
    // Без переполнения: целые секунды и остаток отдельно.
    return ud / f * 1'000'000'000ULL + ud % f * 1'000'000'000ULL / f;
}

double to_ms(Ns t) { return static_cast<double>(t) / 1e6; }
double diff_ms(Ns later, Ns earlier) {
    return (static_cast<double>(later) - static_cast<double>(earlier)) / 1e6;
}

std::string fmt3(double v) { return std::format("{:.3f}", v); }

// Ожидание до момента по QPC: Sleep, пока до цели больше 2 мс, затем
// вращение. Так момент искусственного клика не привязан к тику системного
// таймера, по которому шагают GetTickCount и msg.time.
void wait_until(Ns target) {
    for (;;) {
        const Ns t = now();
        if (t >= target) return;
        if (target - t > 2 * kMs)
            Sleep(1);
        else
            YieldProcessor();
    }
}

// ---------- строки ----------

std::string utf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0,
                                      nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr,
                        nullptr);
    return s;
}

std::wstring wide(std::string_view s) {
    if (s.empty()) return {};
    const int n =
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

// Указатель на функцию из DLL: через void(*)() — так компилятор не считает
// приведение несовместимым.
template <typename F>
F dll_function(const wchar_t* dll, const char* name) {
    HMODULE m = GetModuleHandleW(dll);
    if (!m) return nullptr;
    return reinterpret_cast<F>(reinterpret_cast<void (*)()>(GetProcAddress(m, name)));
}

bool buttons_swapped() { return GetSystemMetrics(SM_SWAPBUTTON) != 0; }

// ---------- фоновый журнал ----------

// Строки копятся в очереди и раз в 200 мс уходят в файлы из своего потока:
// запись на диск не задерживает кадр, а при падении теряется не больше 200 мс.
class Logger {
public:
    ~Logger() { stop(); }

    // Файлы — рядом с программой; если туда писать нельзя — во временную папку.
    void open(const std::string& lib) {
        std::wstring exe(32768, L'\0');
        const DWORD n = GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
        exe.resize(n);
        exe_path = utf8(exe);
        const std::wstring exe_dir = exe.substr(0, exe.find_last_of(L"\\/") + 1);
        SYSTEMTIME st{};
        GetLocalTime(&st);
        const std::wstring stem = wide(std::format("{}_{:04}{:02}{:02}_{:02}{:02}{:02}", lib,
                                                   st.wYear, st.wMonth, st.wDay, st.wHour,
                                                   st.wMinute, st.wSecond));
        if (try_open(exe_dir, stem)) {
            next_to_exe = true;
        } else {
            std::wstring tmp(MAX_PATH + 1, L'\0');
            tmp.resize(GetTempPathW(static_cast<DWORD>(tmp.size()), tmp.data()));
            try_open(tmp, stem);
        }
        thread_ = std::thread([this] { run(); });
    }

    bool ok() const { return log_ != INVALID_HANDLE_VALUE; }
    void text(std::string s) { push(false, std::move(s)); }
    void csv(std::string s) { push(true, std::move(s)); }

    void stop() {
        {
            std::lock_guard lk(m_);
            stop_ = true;
        }
        cv_.notify_all();
        if (thread_.joinable()) thread_.join();
        for (HANDLE* h : {&log_, &csv_}) {
            if (*h != INVALID_HANDLE_VALUE) CloseHandle(*h);
            *h = INVALID_HANDLE_VALUE;
        }
    }

    std::string exe_path, dir, log_path, csv_path;
    bool next_to_exe = false;

private:
    bool try_open(const std::wstring& folder, const std::wstring& stem) {
        const std::wstring base = folder + stem;
        log_ = CreateFileW((base + L".log").c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (log_ == INVALID_HANDLE_VALUE) return false;
        csv_ = CreateFileW((base + L".csv").c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (csv_ == INVALID_HANDLE_VALUE) {
            CloseHandle(log_);
            log_ = INVALID_HANDLE_VALUE;
            return false;
        }
        dir = utf8(folder);
        log_path = utf8(base + L".log");
        csv_path = utf8(base + L".csv");
        write(log_, "\xEF\xBB\xBF");  // метка UTF-8: Блокнот откроет русский текст
        return true;
    }

    void push(bool to_csv, std::string s) {
        s += "\r\n";
        std::lock_guard lk(m_);
        queue_.emplace_back(to_csv, std::move(s));
    }

    void run() {
        std::vector<std::pair<bool, std::string>> batch;
        for (;;) {
            bool stopping = false;
            {
                std::unique_lock lk(m_);
                cv_.wait_for(lk, 200ms, [this] { return stop_; });
                batch.swap(queue_);
                stopping = stop_;
            }
            for (const auto& [to_csv, s] : batch) write(to_csv ? csv_ : log_, s);
            batch.clear();
            if (stopping) {
                std::lock_guard lk(m_);
                if (queue_.empty()) break;
            }
        }
    }

    static void write(HANDLE h, std::string_view s) {
        if (h == INVALID_HANDLE_VALUE) return;
        DWORD written = 0;
        WriteFile(h, s.data(), static_cast<DWORD>(s.size()), &written, nullptr);
    }

    HANDLE log_ = INVALID_HANDLE_VALUE;
    HANDLE csv_ = INVALID_HANDLE_VALUE;
    std::mutex m_;
    std::condition_variable cv_;
    std::vector<std::pair<bool, std::string>> queue_;
    bool stop_ = false;
    std::thread thread_;
};

// ---------- эталон: сырой ввод ----------

struct RawButton {
    Ns t = 0;
    int button = 0;  // логическая кнопка: 1 — основная, 2 — правая, 3 — средняя
    bool injected = false;
    bool used = false;
};

// Поток с невидимым окном сообщений получает WM_INPUT и сразу ставит метку
// QueryPerformanceCounter: это момент, когда пакет мыши дошёл до программы, —
// раньше программа без драйвера ничего не узнаёт. Так же устроен поток сырого
// ввода SDL3 (src/video/windows/SDL_windowsrawinput.c).
class RawRef {
public:
    ~RawRef() { stop(); }

    void start() {
        thread_ = std::thread([this] { run(); });
        for (int i = 0; i < 1000 && !ready_.load(); ++i) Sleep(1);
    }

    void stop() {
        if (thread_.joinable()) {
            const DWORD id = thread_id_.load();
            if (id) PostThreadMessageW(id, WM_QUIT, 0, 0);
            thread_.join();
        }
    }

    bool ok() const { return ok_.load(); }

    void drain(std::vector<RawButton>& buttons, std::vector<double>& intervals_ms) {
        std::lock_guard lk(m_);
        buttons.insert(buttons.end(), buttons_.begin(), buttons_.end());
        intervals_ms.insert(intervals_ms.end(), intervals_.begin(), intervals_.end());
        buttons_.clear();
        intervals_.clear();
    }

private:
    static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
        return DefWindowProcW(h, m, w, l);
    }

    void run() {
        MSG msg{};
        PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);  // очередь потока
        thread_id_ = GetCurrentThreadId();
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = &RawRef::proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"AimTrainerProbeRawRef";
        RegisterClassExW(&wc);
        HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                    nullptr, wc.hInstance, nullptr);
        RAWINPUTDEVICE rid{};
        rid.usUsagePage = 0x01;  // Generic Desktop
        rid.usUsage = 0x02;      // мышь
        rid.dwFlags = 0;         // только пока окно программы впереди
        rid.hwndTarget = hwnd;
        ok_ = hwnd != nullptr && RegisterRawInputDevices(&rid, 1, sizeof rid) == TRUE;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
        ready_ = true;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            if (msg.message == WM_INPUT) handle(reinterpret_cast<HRAWINPUT>(msg.lParam), now());
            DispatchMessageW(&msg);
        }
        if (ok_) {
            rid.dwFlags = RIDEV_REMOVE;
            rid.hwndTarget = nullptr;
            RegisterRawInputDevices(&rid, 1, sizeof rid);
        }
        if (hwnd) DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
    }

    void handle(HRAWINPUT h, Ns t) {
        RAWINPUT ri{};
        UINT size = sizeof ri;
        if (GetRawInputData(h, RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) ==
            static_cast<UINT>(-1))
            return;
        if (ri.header.dwType != RIM_TYPEMOUSE) return;
        const RAWMOUSE& m = ri.data.mouse;
        const HANDLE device = ri.header.hDevice;
        const bool injected = device == nullptr;  // SendInput или тачпад
        std::lock_guard lk(m_);
        // Частота опроса — по пакетам движения настоящего устройства, у каждого
        // устройства свой отсчёт.
        if (!injected && (m.lLastX != 0 || m.lLastY != 0)) {
            auto it = std::find_if(last_.begin(), last_.end(),
                                   [device](const auto& p) { return p.first == device; });
            if (it == last_.end()) {
                last_.emplace_back(device, t);
            } else {
                intervals_.push_back(diff_ms(t, it->second));
                it->second = t;
            }
        }
        // Сырой ввод настоящего устройства — физические кнопки; Windows
        // переставляет их для левши уже в сообщениях окна, и библиотеки
        // отдают логические. Переставляем так же (WIN_SwapButtons в SDL3);
        // у SendInput и тачпада кнопки уже логические.
        const bool swap = !injected && buttons_swapped();
        const int left = swap ? 2 : 1, right = swap ? 1 : 2;
        const USHORT f = m.usButtonFlags;
        if (f & RI_MOUSE_LEFT_BUTTON_DOWN) buttons_.push_back({t, left, injected, false});
        if (f & RI_MOUSE_RIGHT_BUTTON_DOWN) buttons_.push_back({t, right, injected, false});
        if (f & RI_MOUSE_MIDDLE_BUTTON_DOWN) buttons_.push_back({t, 3, injected, false});
    }

    std::thread thread_;
    std::atomic<DWORD> thread_id_{0};
    std::atomic<bool> ready_{false};
    std::atomic<bool> ok_{false};
    std::mutex m_;
    std::vector<RawButton> buttons_;
    std::vector<double> intervals_;
    std::vector<std::pair<HANDLE, Ns>> last_;
};

// ---------- самопроверка: SendInput ----------

struct Injection {
    Ns before = 0;  // now() перед SendInput — эталон времени искусственного клика
    Ns after = 0;
    DWORD tick = 0;  // GetTickCount перед SendInput — к нему привязан msg.time
};

// Точка клика должна быть видна и принадлежать окну программы: иначе клик
// уйдёт в панель задач или в чужое окно.
bool point_is_ours(HWND hwnd, POINT pt) {
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONULL);
    if (!mon) return false;
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    if (!GetMonitorInfoW(mon, &mi) || !PtInRect(&mi.rcWork, pt)) return false;
    HWND under = WindowFromPoint(pt);
    return under && GetAncestor(under, GA_ROOT) == hwnd;
}

// Свой поток ставит курсор в угол окна и шлёт клики основной кнопкой в
// случайные моменты по QPC, не связанные ни с кадрами программы, ни с тиком
// системного таймера, — как настоящий игрок. Кнопка держится 40–70 мс, как у
// человека: библиотека, которая видит только состояние кнопки раз в кадр
// (raylib), не теряет нажатие из-за слишком короткого клика. Если окно ушло с
// переднего плана, курсор сдвинули или точка клика не принадлежит окну —
// останавливается: клик не должен попасть в чужое окно.
class Injector {
public:
    ~Injector() { stop(); }

    void start(HWND hwnd, bool check_cursor, std::uint64_t seed) {
        hwnd_ = hwnd;
        check_cursor_ = check_cursor;
        rng_.seed(seed);
        thread_ = std::thread([this] { run(); });
    }

    void stop() {
        stop_ = true;
        if (thread_.joinable()) thread_.join();
    }

    bool finished() const { return finished_.load(); }
    int sent() const { return sent_.load(); }
    std::string abort_reason() const {
        std::lock_guard lk(m_);
        return abort_reason_;
    }
    std::vector<Injection> take() {
        std::lock_guard lk(m_);
        return std::move(list_);
    }

private:
    void abort(std::string why) {
        std::lock_guard lk(m_);
        abort_reason_ = std::move(why);
    }

    bool still_ours(POINT pt) {
        if (GetForegroundWindow() != hwnd_) {
            abort("окно ушло с переднего плана");
            return false;
        }
        POINT c{};
        GetCursorPos(&c);
        if (check_cursor_ && (std::abs(c.x - pt.x) > 2 || std::abs(c.y - pt.y) > 2)) {
            abort("курсор сдвинули");
            return false;
        }
        if (!point_is_ours(hwnd_, c)) {
            abort("курсор не над окном программы (панель задач или край экрана)");
            return false;
        }
        return true;
    }

    bool send(DWORD flags) {
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = flags;
        return SendInput(1, &in, sizeof in) == 1;
    }

    void run() {
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        POINT pt{rc.left + (rc.right - rc.left) * 40 / kWidth,
                 rc.top + (rc.bottom - rc.top) * (kHeight - 40) / kHeight};
        ClientToScreen(hwnd_, &pt);
        if (GetForegroundWindow() != hwnd_) {
            abort("окно программы не на переднем плане");
            finished_ = true;
            return;
        }
        if (!point_is_ours(hwnd_, pt)) {
            abort("угол окна не виден (панель задач или край экрана)");
            finished_ = true;
            return;
        }
        SetCursorPos(pt.x, pt.y);
        Sleep(150);
        const bool swap = buttons_swapped();  // основная кнопка у левши — правая
        const DWORD down = swap ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_LEFTDOWN;
        const DWORD up = swap ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_LEFTUP;
        std::uniform_int_distribution<int> hold_us(40'000, 70'000);
        std::uniform_int_distribution<int> gap_us(25'000, 45'000);
        Ns next = now() + 20 * kMs;
        for (int i = 0; i < kSelfTestClicks && !stop_; ++i) {
            wait_until(next);
            if (!still_ours(pt)) break;
            Injection j;
            j.tick = GetTickCount();
            j.before = now();
            const bool ok = send(down);
            j.after = now();
            if (!ok) {
                abort("SendInput отклонил нажатие");
                break;
            }
            {
                std::lock_guard lk(m_);
                list_.push_back(j);
            }
            ++sent_;
            wait_until(j.before + static_cast<Ns>(hold_us(rng_)) * 1000);
            if (!send(up)) {
                abort("SendInput отклонил отпускание");
                break;
            }
            next = now() + static_cast<Ns>(gap_us(rng_)) * 1000;
        }
        finished_ = true;
    }

    HWND hwnd_ = nullptr;
    bool check_cursor_ = true;
    std::mt19937_64 rng_;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> finished_{false};
    std::atomic<int> sent_{0};
    mutable std::mutex m_;
    std::vector<Injection> list_;
    std::string abort_reason_;
};

// ---------- статистика ----------

struct Series {
    std::string name;
    std::vector<double> v;
    void add(double x) {
        if (v.size() < kSeriesCap) v.push_back(x);
    }
};

double percentile(const std::vector<double>& sorted, double q) {
    const double pos = q * static_cast<double>(sorted.size() - 1);
    const auto lo = static_cast<std::size_t>(std::floor(pos));
    const auto hi = static_cast<std::size_t>(std::ceil(pos));
    return sorted[lo] + (sorted[hi] - sorted[lo]) * (pos - static_cast<double>(lo));
}

struct Summary {
    std::size_t n = 0;
    double mean = 0, sd = 0, median = 0, p5 = 0, p95 = 0, min = 0, max = 0;
};

Summary summarize(const std::vector<double>& v) {
    Summary s;
    s.n = v.size();
    if (v.empty()) return s;
    std::vector<double> x = v;
    std::sort(x.begin(), x.end());
    double sum = 0;
    for (double a : x) sum += a;
    s.mean = sum / static_cast<double>(x.size());
    double sq = 0;
    for (double a : x) sq += (a - s.mean) * (a - s.mean);
    s.sd = x.size() > 1 ? std::sqrt(sq / static_cast<double>(x.size() - 1)) : 0.0;
    s.median = percentile(x, 0.5);
    s.p5 = percentile(x, 0.05);
    s.p95 = percentile(x, 0.95);
    s.min = x.front();
    s.max = x.back();
    return s;
}

std::string describe(const Series& s) {
    const Summary m = summarize(s.v);
    if (m.n == 0) return std::format("  {}: нет данных", s.name);
    return std::format(
        "  {}: n={} медиана={:.3f} среднее={:.3f} откл={:.3f} p5={:.3f} p95={:.3f} "
        "мин={:.3f} макс={:.3f}",
        s.name, m.n, m.median, m.mean, m.sd, m.p5, m.p95, m.min, m.max);
}

// ---------- сведения о системе ----------

struct OsVersionW {
    ULONG size, major, minor, build, platform;
    WCHAR csd[128];
};

std::string timer_resolution() {
    using NtQueryTimerResolutionFn = LONG(NTAPI*)(PULONG, PULONG, PULONG);
    if (auto f = dll_function<NtQueryTimerResolutionFn>(L"ntdll.dll", "NtQueryTimerResolution")) {
        ULONG mn = 0, mx = 0, cur = 0;
        if (f(&mn, &mx, &cur) == 0)
            return std::format("{:.3f} мс (грубейшее {:.3f}, тончайшее {:.3f})", cur / 1e4,
                               mn / 1e4, mx / 1e4);
    }
    return "неизвестно";
}

// Длительность Sleep(1) в этом процессе — какое разрешение таймера процесс
// получил на деле (с Windows 10 2004 оно своё у каждого процесса).
std::string sleep_report() {
    std::vector<double> d;
    for (int i = 0; i < 20; ++i) {
        const Ns a = now();
        Sleep(1);
        d.push_back(diff_ms(now(), a));
    }
    const Summary s = summarize(d);
    return std::format("Sleep(1) в этом процессе: медиана {:.3f} мс, макс {:.3f}\r\n", s.median,
                       s.max);
}

std::string system_report(HWND hwnd) {
    std::string out;
    using RtlGetVersionFn = LONG(WINAPI*)(OsVersionW*);
    if (auto f = dll_function<RtlGetVersionFn>(L"ntdll.dll", "RtlGetVersion")) {
        OsVersionW v{};
        v.size = sizeof v;
        if (f(&v) == 0)
            out += std::format("Windows: {}.{}.{}\r\n", v.major, v.minor, v.build);
    }
    std::wstring cpu(256, L'\0');
    DWORD bytes = static_cast<DWORD>(cpu.size() * sizeof(wchar_t));
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                     L"ProcessorNameString", RRF_RT_REG_SZ, nullptr, cpu.data(),
                     &bytes) == ERROR_SUCCESS) {
        cpu.resize(wcsnlen(cpu.c_str(), cpu.size()));
        out += std::format("Процессор: {}\r\n", utf8(cpu));
    }
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    out += std::format("Логических ядер: {}\r\n", si.dwNumberOfProcessors);
    const ClockBase& c = clock_base();
    out += std::format("Частота QueryPerformanceCounter: {} Гц\r\n", c.freq.QuadPart);
    out += std::format("Разрешение системного таймера (общее для системы): {}\r\n",
                       timer_resolution());
    out += sleep_report();
    DEVMODEW dm{};
    dm.dmSize = sizeof dm;
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm))
        out += std::format("Экран: {}×{}, {} Гц\r\n", dm.dmPelsWidth, dm.dmPelsHeight,
                           dm.dmDisplayFrequency);
    DWM_TIMING_INFO ti{};
    ti.cbSize = sizeof ti;
    if (SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &ti)) && ti.rateRefresh.uiDenominator)
        out += std::format("DWM: обновление {:.3f} Гц\r\n",
                           static_cast<double>(ti.rateRefresh.uiNumerator) /
                               static_cast<double>(ti.rateRefresh.uiDenominator));
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    const auto dpi_for = dll_function<GetDpiForWindowFn>(L"user32.dll", "GetDpiForWindow");
    out += std::format("Масштаб: процесс {} DPI, окно {} DPI\r\n",
                       IsProcessDPIAware() ? "учитывает" : "не учитывает",
                       dpi_for && hwnd ? std::to_string(dpi_for(hwnd)) : "?");
    RECT wr{}, cr{};
    GetWindowRect(hwnd, &wr);
    GetClientRect(hwnd, &cr);
    out += std::format("Окно: ({}, {})–({}, {}), клиентская область {}×{}\r\n", wr.left, wr.top,
                       wr.right, wr.bottom, cr.right - cr.left, cr.bottom - cr.top);
    out += std::format("Кнопки мыши переставлены (левша): {}\r\n",
                       buttons_swapped() ? "да" : "нет");
    SYSTEM_POWER_STATUS ps{};
    if (GetSystemPowerStatus(&ps))
        out += std::format("Питание: {}\r\n", ps.ACLineStatus == 1   ? "от сети"
                                              : ps.ACLineStatus == 0 ? "от батареи"
                                                                     : "неизвестно");
    out += std::format("Класс приоритета процесса: 0x{:x}\r\n", GetPriorityClass(GetCurrentProcess()));
    return out;
}

// Шаг GetTickCount — того же счётчика, из которого берётся msg.time сообщений
// Windows: 250 мс смотрим, на сколько он прыгает.
std::string tick_step_report() {
    std::vector<double> steps;
    const Ns end = now() + 250 * kMs;
    DWORD prev = GetTickCount();
    while (now() < end) {
        const DWORD t = GetTickCount();
        if (t != prev) {
            steps.push_back(static_cast<double>(t - prev));
            prev = t;
        }
    }
    const Summary s = summarize(steps);
    if (s.n == 0) return "Шаг GetTickCount: не изменился за 250 мс\r\n";
    return std::format("Шаг GetTickCount: медиана {:.1f} мс, мин {:.1f}, макс {:.1f} (n={})\r\n",
                       s.median, s.min, s.max, s.n);
}

// Окно целиком в рабочей области своего экрана: иначе часть целей и точка
// самопроверки могут оказаться под панелью задач или за краем экрана.
std::string fit_window(HWND hwnd) {
    RECT wr{};
    if (!hwnd || !GetWindowRect(hwnd, &wr)) return "окно: нет HWND";
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    if (!GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi))
        return "окно: нет сведений об экране";
    const RECT& work = mi.rcWork;
    if (wr.left >= work.left && wr.top >= work.top && wr.right <= work.right &&
        wr.bottom <= work.bottom)
        return "окно целиком в рабочей области экрана";
    const LONG w = wr.right - wr.left, h = wr.bottom - wr.top;
    const LONG x = std::max(work.left, std::min(wr.left, work.right - w));
    const LONG y = std::max(work.top, std::min(wr.top, work.bottom - h));
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    return std::format("окно сдвинуто в рабочую область экрана: ({}, {}) → ({}, {}){}", wr.left,
                       wr.top, x, y,
                       w > work.right - work.left || h > work.bottom - work.top
                           ? " — но не помещается в неё целиком"
                           : "");
}

enum class Phase { Intro, SelfTest, Trials, Done, Failed };

struct Click {
    int button = 0;
    float x = 0, y = 0;
    Ns delivered = 0;
    std::optional<Ns> lib;
    std::string native;
    bool used = false;
};

struct Trial {
    int seq = 0;  // 0 — клик до появления цели
    float x = 0, y = 0, r = kRadius;
    double foreperiod_ms = 0;
    Ns onset_call = 0, onset_ret = 0;
    bool shown = false;
    bool timeout = false;
    std::optional<Click> click;
    Ns finalize_at = 0;
};

std::uint64_t make_seed() {
    std::random_device rd;
    LARGE_INTEGER q{};
    QueryPerformanceCounter(&q);
    return (static_cast<std::uint64_t>(rd()) << 32) ^ rd() ^
           static_cast<std::uint64_t>(q.QuadPart) ^
           (static_cast<std::uint64_t>(GetCurrentProcessId()) << 16);
}

}  // namespace

Ns now() {
    const ClockBase& base = clock_base();  // начало отсчёта — раньше первого чтения
    (void)base;
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return qpc_to_ns(c.QuadPart);
}

Ns from_age(Ns at, std::uint64_t age_ns) { return at > age_ns ? at - age_ns : 0; }

struct Session::Impl {
    LibInfo info;
    HWND hwnd = nullptr;
    Logger log;
    RawRef raw;
    Injector inj;
    std::uint64_t seed = make_seed();
    std::mt19937_64 rng{seed};
    bool timer_raised = false;

    Phase phase = Phase::Intro;
    Ns phase_start = 0;
    std::string fail_reason;

    std::vector<RawButton> raw_buttons;
    std::vector<double> raw_intervals;
    std::vector<Click> selftest_clicks;
    Ns selftest_end_at = 0;

    std::optional<Trial> current;
    std::vector<Trial> pending;
    int trials_done = 0, hits = 0, timeouts = 0, early = 0, anticipated = 0, ignored = 0;
    Ns next_target_at = 0, last_trial_end = 0;
    bool onset_in_this_frame = false;
    double last_rt = -1;

    Ns last_present = 0;
    std::uint64_t frames = 0, frames_at_status = 0;
    Ns next_status = 0, status_from = 0;

    std::string title_shown;
    Ns title_at = 0;
    bool finished = false;

    // Самопроверка: искусственный клик SendInput — эталон.
    Series st_raw{"самопроверка: сырой ввод − SendInput, мс", {}};
    Series st_delivered{"самопроверка: получено программой − SendInput, мс", {}};
    Series st_lib{"самопроверка: метка библиотеки − SendInput, мс", {}};
    Series st_delivered_raw{"самопроверка: получено программой − сырой ввод, мс", {}};
    Series st_lib_raw{"самопроверка: метка библиотеки − сырой ввод, мс", {}};
    Series st_sendinput{"самопроверка: длительность вызова SendInput, мс", {}};
    int st_sent = 0, st_received = 0;
    // Серия целей: живой клик игрока.
    Series rt_raw{"реакция: сырой ввод − показ кадра с целью, мс", {}};
    Series rt_lib{"реакция: метка библиотеки − показ кадра с целью, мс", {}};
    Series rt_delivered{"реакция: получено программой − показ кадра с целью, мс", {}};
    Series tr_delivered_raw{"клик: получено программой − сырой ввод, мс", {}};
    Series tr_lib_raw{"клик: метка библиотеки − сырой ввод, мс", {}};
    // Фон.
    Series frame_interval{"интервал между показами кадров, мс", {}};
    Series present_block{"показ кадра: возврат − вызов, мс", {}};
    Series onset_block{"кадр с целью: возврат − вызов показа, мс", {}};
    Series raw_packet{"интервал пакетов движения мыши (сырой ввод), мс", {}};

    double foreperiod() {
        return std::uniform_real_distribution<double>(1000.0, 2000.0)(rng);
    }

    void set_phase(Phase p, Ns t) {
        phase = p;
        phase_start = t;
        title_at = 0;
    }

    void drain_raw() {
        if (!info.raw_reference) return;
        std::vector<double> intervals;
        raw.drain(raw_buttons, intervals);
        for (double d : intervals)
            if (d < 50.0) raw_packet.add(d);  // паузы между движениями — не частота опроса
    }

    // Нажатие сырого ввода, которое породило событие библиотеки: последнее
    // свободное той же кнопки не позже получения (+5 мс — поток сырого ввода
    // мог проснуться чуть позже основного) и не раньше чем за 500 мс.
    std::optional<Ns> match_raw(int button, Ns delivered) {
        RawButton* best = nullptr;
        for (RawButton& r : raw_buttons) {
            if (r.used || r.button != button) continue;
            if (r.t > delivered + 5 * kMs || delivered > r.t + 500 * kMs) continue;
            if (!best || r.t > best->t) best = &r;
        }
        if (!best) return std::nullopt;
        best->used = true;
        return best->t;
    }

    void prune_raw(Ns t) {
        std::erase_if(raw_buttons, [t](const RawButton& r) { return r.t + 10'000 * kMs < t; });
    }

    // ----- самопроверка -----

    void start_selftest(Ns t) {
        set_phase(Phase::SelfTest, t);
        log.text(std::format("[{:.3f}] самопроверка: старт, {} кликов SendInput", to_ms(t),
                             kSelfTestClicks));
        inj.start(hwnd, info.check_cursor, rng());
    }

    // Каждой инъекции достаётся только то, что пришло до следующей инъекции:
    // потерянный клик остаётся потерянным и не сдвигает следующие пары.
    void finish_selftest(Ns t) {
        inj.stop();
        drain_raw();
        std::vector<Injection> list = inj.take();
        const std::string why = inj.abort_reason();
        st_sent = static_cast<int>(list.size());
        for (std::size_t i = 0; i < list.size(); ++i) {
            const Injection& j = list[i];
            const Ns until = i + 1 < list.size() ? list[i + 1].before : j.before + 1'000 * kMs;
            std::optional<Ns> r;
            if (info.raw_reference) {
                RawButton* best = nullptr;
                for (RawButton& b : raw_buttons) {
                    if (b.used || b.t < j.before || b.t >= until) continue;
                    if (!best || b.t < best->t) best = &b;
                }
                if (best) {
                    best->used = true;
                    r = best->t;
                }
            }
            Click* c = nullptr;
            for (Click& cand : selftest_clicks) {
                if (cand.used || cand.delivered < j.before || cand.delivered >= until) continue;
                cand.used = true;
                c = &cand;
                break;
            }
            if (c) ++st_received;
            st_sendinput.add(diff_ms(j.after, j.before));
            if (r) st_raw.add(diff_ms(*r, j.before));
            if (c) st_delivered.add(diff_ms(c->delivered, j.before));
            if (c && c->lib) st_lib.add(diff_ms(*c->lib, j.before));
            if (c && r) st_delivered_raw.add(diff_ms(c->delivered, *r));
            if (c && c->lib && r) st_lib_raw.add(diff_ms(*c->lib, *r));
            log.csv(std::format(
                "selftest,{},{},{},{},,,,,,,,{},{:.1f},{},{},{},{},{},,,,{},{},{},{},{}", i + 1,
                c ? std::to_string(c->button) : "", c ? fmt3(static_cast<double>(c->x)) : "",
                c ? fmt3(static_cast<double>(c->y)) : "", fmt3(to_ms(j.before)),
                diff_ms(j.after, j.before) * 1000.0, j.tick, r ? fmt3(to_ms(*r)) : "",
                c ? fmt3(to_ms(c->delivered)) : "", c && c->lib ? fmt3(to_ms(*c->lib)) : "",
                c ? c->native : "", c && r ? fmt3(diff_ms(c->delivered, *r)) : "",
                c && c->lib && r ? fmt3(diff_ms(*c->lib, *r)) : "",
                r ? fmt3(diff_ms(*r, j.before)) : "", c ? fmt3(diff_ms(c->delivered, j.before)) : "",
                c && c->lib ? fmt3(diff_ms(*c->lib, j.before)) : ""));
        }
        const int lost = st_sent - st_received;
        log.text(std::format("[{:.3f}] самопроверка: отправлено {}, получено программой {}, "
                             "потеряно {}{}",
                             to_ms(t), st_sent, st_received, lost,
                             why.empty() ? std::string{} : ", остановлена: " + why));
        for (const Series* s : {&st_delivered, &st_lib, &st_raw, &st_delivered_raw, &st_lib_raw})
            log.text(describe(*s));
        selftest_clicks.clear();
        if (!why.empty()) {
            fail_reason = why;
            set_phase(Phase::Failed, t);
            log.text(std::format("[{:.3f}] самопроверка НЕ ПРОШЛА: {} — серия не начинается; "
                                 "закрыть (Esc) и запустить заново",
                                 to_ms(t), why));
            write_summary("ИТОГ: САМОПРОВЕРКА НЕ ПРОШЛА");
            return;
        }
        start_trials(t);
    }

    // ----- серия целей -----

    void start_trials(Ns t) {
        set_phase(Phase::Trials, t);
        last_trial_end = t;
        next_target_at = t + static_cast<Ns>(foreperiod() * 1e6);
        log.text(std::format("[{:.3f}] серия: старт, {} целей", to_ms(t), kTrials));
    }

    void update_trials(Ns t) {
        if (!current) {
            if (trials_done >= kTrials) {
                // Итог — только когда записана последняя цель.
                if (!pending.empty()) return;
                set_phase(Phase::Done, t);
                log.text(std::format("[{:.3f}] серия: готово", to_ms(t)));
                write_summary("ИТОГ ПОСЛЕ СЕРИИ");
                return;
            }
            if (t < next_target_at) return;
            Trial tr;
            tr.seq = trials_done + 1;
            std::uniform_real_distribution<float> px(kRadius + 20.0f, kWidth - kRadius - 20.0f);
            std::uniform_real_distribution<float> py(kRadius + 30.0f, kHeight - kRadius - 20.0f);
            tr.x = px(rng);
            tr.y = py(rng);
            tr.foreperiod_ms = diff_ms(t, last_trial_end);
            current = tr;
            return;
        }
        if (current->shown && t > current->onset_ret + kTrialTimeout) {
            current->timeout = true;
            current->finalize_at = t;
            pending.push_back(*current);
            current.reset();
            ++trials_done;
            ++timeouts;
            last_trial_end = t;
            next_target_at = t + static_cast<Ns>(foreperiod() * 1e6);
        }
    }

    void finalize_pending(Ns t) {
        for (auto it = pending.begin(); it != pending.end();) {
            if (t < it->finalize_at) {
                ++it;
                continue;
            }
            write_trial(*it);
            it = pending.erase(it);
        }
        prune_raw(t);
    }

    void write_trial(const Trial& tr) {
        std::optional<Ns> r;
        std::optional<int> hit;
        const Click* c = tr.click ? &*tr.click : nullptr;
        if (c) {
            r = info.raw_reference ? match_raw(c->button, c->delivered) : std::nullopt;
            const double dx = static_cast<double>(c->x - tr.x);
            const double dy = static_cast<double>(c->y - tr.y);
            if (tr.seq != 0) hit = std::sqrt(dx * dx + dy * dy) <= static_cast<double>(tr.r) ? 1 : 0;
        }
        // Нажатие раньше показа кадра с целью — упреждение, а не реакция, даже
        // если программа получила его после показа.
        const std::optional<Ns> pressed = r ? r : (c ? c->lib : std::nullopt);
        const bool anticipation = tr.seq != 0 && tr.shown && c && pressed && *pressed < tr.onset_call;
        if (anticipation) ++anticipated;
        const char* kind = tr.seq == 0 ? "early"
                           : anticipation ? "anticipation"
                           : tr.timeout   ? "timeout"
                                          : "trial";
        const bool trial = tr.seq != 0 && tr.shown && c && !anticipation;
        if (trial) {
            onset_block.add(diff_ms(tr.onset_ret, tr.onset_call));
            rt_delivered.add(diff_ms(c->delivered, tr.onset_ret));
            if (r) rt_raw.add(diff_ms(*r, tr.onset_ret));
            if (c->lib) rt_lib.add(diff_ms(*c->lib, tr.onset_ret));
        }
        if (c && r) tr_delivered_raw.add(diff_ms(c->delivered, *r));
        if (c && c->lib && r) tr_lib_raw.add(diff_ms(*c->lib, *r));
        log.csv(std::format(
            "{},{},{},{},{},{},{},{},{},{},{},{},,,,{},{},{},{},{},{},{},{},{},,,", kind, tr.seq,
            c ? std::to_string(c->button) : "", c ? fmt3(static_cast<double>(c->x)) : "",
            c ? fmt3(static_cast<double>(c->y)) : "", hit ? std::to_string(*hit) : "",
            tr.seq ? fmt3(static_cast<double>(tr.x)) : "",
            tr.seq ? fmt3(static_cast<double>(tr.y)) : "",
            tr.seq ? fmt3(static_cast<double>(tr.r)) : "", tr.seq ? fmt3(tr.foreperiod_ms) : "",
            tr.shown ? fmt3(to_ms(tr.onset_call)) : "", tr.shown ? fmt3(to_ms(tr.onset_ret)) : "",
            r ? fmt3(to_ms(*r)) : "", c ? fmt3(to_ms(c->delivered)) : "",
            c && c->lib ? fmt3(to_ms(*c->lib)) : "", c ? c->native : "",
            tr.shown && c && r ? fmt3(diff_ms(*r, tr.onset_ret)) : "",
            tr.shown && c && c->lib ? fmt3(diff_ms(*c->lib, tr.onset_ret)) : "",
            tr.shown && c ? fmt3(diff_ms(c->delivered, tr.onset_ret)) : "",
            c && r ? fmt3(diff_ms(c->delivered, *r)) : "",
            c && c->lib && r ? fmt3(diff_ms(*c->lib, *r)) : ""));
    }

    // ----- журнал -----

    void write_header(const std::string& fit) {
        log.text("ЗАМЕРЩИК ВВОДА AIM TRAINER — docs/LIB_PROBES.md");
        log.text(std::format("Библиотека: {} {}", info.name, info.version));
        log.text(std::format("Рисует: {}", info.renderer.empty() ? "—" : info.renderer));
        log.text(std::format("Метка времени клика у библиотеки: {}", info.timestamp));
        log.text(std::format("Эталон сырого ввода: {}",
                             !info.raw_reference ? "выключен (сырой ввод занят библиотекой)"
                             : raw.ok()          ? "работает"
                                                 : "НЕ ЗАРЕГИСТРИРОВАН"));
        log.text(std::format("Программа: {}", log.exe_path));
        log.text(std::format("Таблица: {}", log.csv_path));
        log.text(std::format("Зерно случайности: {:#018x}", seed));
        log.text(std::format("Разрешение таймера процесса: {}",
                             timer_raised ? "1 мс (timeBeginPeriod)" : "не повышено"));
        log.text(fit);
        log.text(system_report(hwnd) + tick_step_report());
        log.text("Время всюду — мс от запуска по QueryPerformanceCounter.");
        log.text("");
        log.csv(
            "kind,seq,button,x,y,hit,target_x,target_y,radius,foreperiod_ms,onset_call_ms,"
            "onset_ret_ms,inject_ms,sendinput_us,tick_at_inject,raw_ms,delivered_ms,lib_ms,"
            "lib_native,rt_raw_ms,rt_lib_ms,rt_delivered_ms,delivered_minus_raw_ms,"
            "lib_minus_raw_ms,raw_minus_inject_ms,delivered_minus_inject_ms,lib_minus_inject_ms");
    }

    void status(Ns t) {
        if (t < next_status) return;
        if (next_status != 0) {
            const double secs = diff_ms(t, status_from) / 1000.0;
            log.text(std::format("[{:.3f}] кадров {} за {:.1f} с ({:.0f}/с), целей {}/{}", to_ms(t),
                                 frames - frames_at_status, secs,
                                 secs > 0 ? static_cast<double>(frames - frames_at_status) / secs
                                          : 0.0,
                                 trials_done, kTrials));
        }
        frames_at_status = frames;
        status_from = t;
        next_status = t + 10'000 * kMs;
    }

    void write_summary(const char* title) {
        log.text("");
        log.text(std::format("===== {} — {} {} =====", title, info.name, info.version));
        const Summary d = summarize(st_delivered.v);
        const Summary l = summarize(st_lib.v);
        const Summary r = summarize(st_raw.v);
        const Summary f = summarize(frame_interval.v);
        log.text(std::format("Главное (самопроверка: отправлено {}, получено программой {}, "
                             "потеряно {}):",
                             st_sent, st_received, st_sent - st_received));
        log.text(d.n ? std::format("  программа узнаёт о клике через {:.3f} мс (медиана), p95 "
                                   "{:.3f}, разброс (откл.) {:.3f}",
                                   d.median, d.p95, d.sd)
                     : "  программа о кликах не узнала — самопроверка не прошла");
        log.text(l.n ? std::format("  метка библиотеки: смещение {:.3f} мс (медиана), разброс "
                                   "(откл.) {:.3f}, от p5 до p95 {:.3f} мс",
                                   l.median, l.sd, l.p95 - l.p5)
                     : "  метки времени клика у библиотеки нет");
        log.text(r.n ? std::format("  эталон (сырой ввод в своём потоке): {:.3f} мс (медиана), "
                                   "разброс {:.3f}",
                                   r.median, r.sd)
                     : "  эталона сырого ввода нет");
        log.text(f.n && f.median > 0
                     ? std::format("  кадр: {:.3f} мс (медиана) — {:.0f} кадров/с", f.median,
                                   1000.0 / f.median)
                     : "  кадров нет");
        log.text(std::format("Серия: целей {}, попаданий {}, без клика {}, кликов до цели {}, "
                             "упреждений (нажато раньше показа) {}",
                             trials_done, hits, timeouts, early, anticipated));
        log.text("Подробно:");
        for (const Series* s :
             {&st_delivered, &st_lib, &st_raw, &st_delivered_raw, &st_lib_raw, &st_sendinput,
              &rt_raw, &rt_lib, &rt_delivered, &tr_delivered_raw, &tr_lib_raw, &frame_interval,
              &present_block, &onset_block, &raw_packet})
            log.text(describe(*s));
        const Summary p = summarize(raw_packet.v);
        if (p.n && p.median > 0)
            log.text(std::format("  частота опроса мыши по сырому вводу: около {:.0f} Гц",
                                 1000.0 / p.median));
    }

    std::string make_title(Ns t) const {
        const std::string& n = info.name;
        const std::string no_log = log.ok() ? "" : " — ЖУРНАЛ НЕ ЗАПИСЫВАЕТСЯ";
        switch (phase) {
            case Phase::Intro: {
                const Ns left = phase_start + kIntro > t ? phase_start + kIntro - t : 0;
                return std::format("{} — самопроверка через {} с: не трогайте мышь{}", n,
                                   left / (1'000 * kMs) + 1, no_log);
            }
            case Phase::SelfTest:
                return std::format("{} — самопроверка {}/{}: не трогайте мышь{}", n, inj.sent(),
                                   kSelfTestClicks, no_log);
            case Phase::Trials:
                return last_rt >= 0
                           ? std::format("{} — цель {}/{}, реакция {:.0f} мс — кликайте по кругу, "
                                         "Esc — выход{}",
                                         n, trials_done, kTrials, last_rt, no_log)
                           : std::format("{} — цель {}/{} — кликайте по кругу, Esc — выход{}", n,
                                         trials_done, kTrials, no_log);
            case Phase::Done:
                return log.ok() ? std::format("{} — готово: журнал в {} — Esc — выход", n, log.dir)
                                : std::format("{} — готово, но ЖУРНАЛ НЕ ЗАПИСАН — Esc", n);
            case Phase::Failed:
                return std::format("{} — САМОПРОВЕРКА НЕ ПРОШЛА: {} — Esc и запустить заново", n,
                                   fail_reason);
        }
        return n;
    }
};

Session::Session(LibInfo info, void* hwnd) : impl_(std::make_unique<Impl>()) {
    Impl& s = *impl_;
    s.info = std::move(info);
    s.hwnd = static_cast<HWND>(hwnd);
    // Таймер 1 мс — во всех замерщиках одинаково: с Windows 10 2004 разрешение
    // у каждого процесса своё, а часть библиотек его не повышает.
    s.timer_raised = timeBeginPeriod(1) == TIMERR_NOERROR;
    const std::string fit = fit_window(s.hwnd);
    s.log.open(s.info.name);
    // Эталон — после окна библиотеки: сырой ввод мыши в процессе получает тот,
    // кто зарегистрировался последним (SFML регистрирует его при создании окна).
    if (s.info.raw_reference) s.raw.start();
    s.write_header(fit);
    const Ns t = now();
    s.set_phase(Phase::Intro, t);
    s.status(t);
}

Session::~Session() { finish(); }

View Session::frame() {
    Impl& s = *impl_;
    const Ns t = now();
    s.drain_raw();
    switch (s.phase) {
        case Phase::Intro:
            if (t >= s.phase_start + kIntro) s.start_selftest(t);
            break;
        case Phase::SelfTest:
            if (s.inj.finished() && s.selftest_end_at == 0) s.selftest_end_at = t + 400 * kMs;
            if (s.selftest_end_at != 0 && t >= s.selftest_end_at) s.finish_selftest(t);
            break;
        case Phase::Trials:
            s.update_trials(t);
            break;
        case Phase::Done:
        case Phase::Failed:
            break;
    }
    s.finalize_pending(t);
    s.status(t);

    View v;
    switch (s.phase) {
        case Phase::Intro:
            v.bar = kBarSelfTest;
            v.progress = 0;
            break;
        case Phase::SelfTest:
            v.bar = kBarSelfTest;
            v.progress = static_cast<float>(s.inj.sent()) / static_cast<float>(kSelfTestClicks);
            break;
        case Phase::Trials:
        case Phase::Done:
            v.bar = kTarget;
            v.progress = static_cast<float>(s.trials_done) / static_cast<float>(kTrials);
            break;
        case Phase::Failed:
            v.bar = kBarFailed;
            v.progress = 1;
            v.bar_h = 60;
            break;
    }
    if (s.current) {
        v.target = true;
        v.x = s.current->x;
        v.y = s.current->y;
        v.r = s.current->r;
    }
    s.onset_in_this_frame = s.current && !s.current->shown;
    return v;
}

void Session::click(int button, float x, float y, Ns delivered, std::optional<Ns> lib_time,
                    std::string_view native) {
    Impl& s = *impl_;
    Click c{button, x, y, delivered, lib_time, std::string(native), false};
    switch (s.phase) {
        case Phase::Intro:
        case Phase::Done:
        case Phase::Failed:
            ++s.ignored;
            return;
        case Phase::SelfTest:
            s.selftest_clicks.push_back(std::move(c));
            return;
        case Phase::Trials:
            break;
    }
    const Ns next = delivered + static_cast<Ns>(s.foreperiod() * 1e6);
    if (s.current && s.current->shown) {
        Trial tr = *s.current;
        tr.finalize_at = delivered + kFinalizeDelay;
        const float dx = x - tr.x, dy = y - tr.y;
        if (dx * dx + dy * dy <= tr.r * tr.r) ++s.hits;
        s.last_rt = diff_ms(delivered, tr.onset_ret);
        tr.click = std::move(c);
        s.pending.push_back(std::move(tr));
        s.current.reset();
        ++s.trials_done;
    } else {
        // Клик до появления цели: замеряется как есть, а цель откладывается.
        Trial tr;
        tr.click = std::move(c);
        tr.finalize_at = delivered + kFinalizeDelay;
        s.pending.push_back(std::move(tr));
        s.current.reset();
        ++s.early;
    }
    s.last_trial_end = delivered;
    s.next_target_at = next;
}

void Session::presented(Ns call, Ns ret) {
    Impl& s = *impl_;
    ++s.frames;
    if (s.last_present != 0) s.frame_interval.add(diff_ms(ret, s.last_present));
    s.last_present = ret;
    s.present_block.add(diff_ms(ret, call));
    if (s.onset_in_this_frame && s.current) {
        s.current->onset_call = call;
        s.current->onset_ret = ret;
        s.current->shown = true;
    }
    s.onset_in_this_frame = false;
}

void Session::note(std::string_view text) {
    impl_->log.text(std::format("[{:.3f}] {}", to_ms(now()), text));
}

std::string Session::take_title() {
    Impl& s = *impl_;
    const Ns t = now();
    if (s.title_at != 0 && t < s.title_at + 250 * kMs) return {};
    s.title_at = t;
    std::string title = s.make_title(t);
    if (title == s.title_shown) return {};
    s.title_shown = title;
    return title;
}

void Session::finish() {
    if (!impl_ || impl_->finished) return;
    Impl& s = *impl_;
    s.finished = true;
    const Ns t = now();
    s.inj.stop();
    if (s.phase == Phase::SelfTest) s.finish_selftest(t);
    for (const Trial& tr : s.pending) s.write_trial(tr);
    s.pending.clear();
    s.log.text(std::format("[{:.3f}] выход", to_ms(t)));
    s.write_summary("ИТОГ ПРИ ВЫХОДЕ");
    if (s.timer_raised) timeEndPeriod(1);
    s.raw.stop();
    s.log.stop();
}

}  // namespace probe
