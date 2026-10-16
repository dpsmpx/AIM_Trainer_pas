#include "platform/storage.h"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <string>
#include <utility>

namespace aim::platform {

namespace {

constexpr const char* kAppFolder = "AimTrainer";

// Причина из SDL — для уведомления: вместо полного пути — имя файла, чтобы
// строка помещалась в окно; нет имени в причине (запись, переименование на
// Windows) — имя впереди.
void fail(std::string& error, const std::string& path, std::string_view name) {
    error = SDL_GetError();
    for (std::size_t at = error.find(path); at != std::string::npos;
         at = error.find(path, at + name.size()))
        error.replace(at, path.size(), name);
    if (error.find(name) == std::string::npos) error = std::string(name) + ": " + error;
}

bool write_io(const std::string& path, std::string_view name, const char* mode,
              std::string_view data, std::string& error) {
    SDL_IOStream* io = SDL_IOFromFile(path.c_str(), mode);
    if (io == nullptr) {
        fail(error, path, name);
        return false;
    }
    const std::size_t written = SDL_WriteIO(io, data.data(), data.size());
    const bool closed = SDL_CloseIO(io);
    if (written != data.size() || !closed) {
        fail(error, path, name);
        return false;
    }
    return true;
}

}  // namespace

std::optional<Storage> Storage::open(std::string dir, std::string& problem) {
    if (dir.empty()) {
        problem = "empty folder path";
        return std::nullopt;
    }
    if (!dir.ends_with('/') && !dir.ends_with('\\')) dir += '/';
    if (!SDL_CreateDirectory(dir.c_str())) {
        problem = SDL_GetError();
        return std::nullopt;
    }
    return Storage(std::move(dir));
}

std::optional<Storage> Storage::default_folder(std::string& problem) {
    if (const char* forced = SDL_getenv("AIM_TRAINER_DATA_DIR");
        forced != nullptr && *forced != '\0')
        return open(forced, problem);
    // Пустая организация — уровня «организации» в пути нет (D-30).
    char* pref = SDL_GetPrefPath("", kAppFolder);
    if (pref == nullptr) {
        problem = SDL_GetError();
        return std::nullopt;
    }
    Storage storage(pref);
    SDL_free(pref);
    return storage;
}

Storage::Storage(std::string dir) : dir_(std::move(dir)) {}

std::string Storage::path(std::string_view name) const {
    return dir_ + std::string(name);
}

std::optional<std::string> Storage::read(std::string_view name) const {
    std::size_t size = 0;
    void* data = SDL_LoadFile(path(name).c_str(), &size);
    if (data == nullptr) return std::nullopt;
    std::string text(static_cast<const char*>(data), size);
    SDL_free(data);
    return text;
}

bool Storage::write(std::string_view name, std::string_view data, std::string& error) const {
    const std::string target = path(name);
    const std::string temp = target + ".tmp";
    if (!write_io(temp, std::string(name) + ".tmp", "wb", data, error)) return false;
    if (!SDL_RenamePath(temp.c_str(), target.c_str())) {
        fail(error, target, name);
        SDL_RemovePath(temp.c_str());
        return false;
    }
    return true;
}

bool Storage::append(std::string_view name, std::string_view data, std::string& error) const {
    return write_io(path(name), name, "ab", data, error);
}

bool Storage::exists(std::string_view name) const {
    return SDL_GetPathInfo(path(name).c_str(), nullptr);
}

std::optional<std::vector<std::string>> Storage::files(std::string& error) const {
    struct Listing {
        const std::string* dir;
        std::vector<std::string> names;
    } listing{&dir_, {}};
    const auto callback = [](void* userdata, const char* /*dirname*/,
                             const char* fname) -> SDL_EnumerationResult {
        auto* l = static_cast<Listing*>(userdata);
        SDL_PathInfo info{};
        if (SDL_GetPathInfo((*l->dir + fname).c_str(), &info) && info.type == SDL_PATHTYPE_FILE)
            l->names.emplace_back(fname);
        return SDL_ENUM_CONTINUE;
    };
    if (!SDL_EnumerateDirectory(dir_.c_str(), callback, &listing)) {
        error = SDL_GetError();
        return std::nullopt;
    }
    return listing.names;
}

bool Storage::writable(std::string& error) const {
    constexpr std::string_view kProbe = "aim_trainer_write_probe.tmp";
    if (!write_io(path(kProbe), kProbe, "wb", "ok", error)) return false;
    SDL_RemovePath(path(kProbe).c_str());
    return true;
}

}  // namespace aim::platform
