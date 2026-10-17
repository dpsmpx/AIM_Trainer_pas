#include "platform/single_instance.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#endif

#include <cstdint>
#include <format>
#include <string>

namespace aim::platform {

#ifdef _WIN32

namespace {

// Имя мьютекса — от папки: FNV-1a, без учёта регистра букв латиницы (пути
// Windows регистра не различают).
std::uint64_t folder_hash(const std::string& dir) {
    std::uint64_t h = 14695981039346656037ULL;
    for (const char ch : dir) {
        auto c = static_cast<unsigned char>(ch);
        if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c - 'A' + 'a');
        if (c == '\\') c = '/';
        h = (h ^ c) * 1099511628211ULL;
    }
    return h;
}

}  // namespace

SingleInstance::SingleInstance(const std::string& dir) {
    const std::wstring name = std::format(L"Local\\DPAimTrainer-{:016x}", folder_hash(dir));
    HANDLE mutex = CreateMutexW(nullptr, FALSE, name.c_str());
    if (mutex == nullptr) {
        problem_ = std::format("CreateMutexW: ошибка Windows {}", GetLastError());
        return;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        state_ = State::Busy;
        return;
    }
    handle_ = reinterpret_cast<std::intptr_t>(mutex);
    state_ = State::Owned;
}

SingleInstance::~SingleInstance() {
    if (state_ == State::Owned) CloseHandle(reinterpret_cast<HANDLE>(handle_));
}

namespace {

struct Search {
    std::wstring title;
    HWND found = nullptr;
};

BOOL CALLBACK find_window(HWND window, LPARAM param) {
    auto& search = *reinterpret_cast<Search*>(param);
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process == GetCurrentProcessId() || IsWindowVisible(window) == FALSE) return TRUE;
    wchar_t text[256] = {};
    const int n = GetWindowTextW(window, text, 256);
    if (n < static_cast<int>(search.title.size()) ||
        std::wstring(text, search.title.size()) != search.title)
        return TRUE;
    search.found = window;
    return FALSE;
}

}  // namespace

bool SingleInstance::raise_other(std::string_view title) {
    Search search;
    const int n =
        MultiByteToWideChar(CP_UTF8, 0, title.data(), static_cast<int>(title.size()), nullptr, 0);
    search.title.resize(static_cast<std::size_t>(n));
    MultiByteToWideChar(CP_UTF8, 0, title.data(), static_cast<int>(title.size()),
                        search.title.data(), n);
    EnumWindows(find_window, reinterpret_cast<LPARAM>(&search));
    if (search.found == nullptr) return false;
    if (IsIconic(search.found) != FALSE) ShowWindow(search.found, SW_RESTORE);
    SetForegroundWindow(search.found);
    return true;
}

#else

SingleInstance::SingleInstance(const std::string& dir) {
    const std::string path = dir + ".aim_trainer.lock";
    const int fd = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) {
        problem_ = path + ": " + std::strerror(errno);
        return;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        const int error = errno;
        close(fd);
        if (error == EWOULDBLOCK) {
            state_ = State::Busy;
        } else {
            problem_ = path + ": " + std::strerror(error);
        }
        return;
    }
    handle_ = fd;
    state_ = State::Owned;
}

SingleInstance::~SingleInstance() {
    if (state_ == State::Owned) close(static_cast<int>(handle_));
}

bool SingleInstance::raise_other(std::string_view /*title*/) {
    return false;
}

#endif

}  // namespace aim::platform
