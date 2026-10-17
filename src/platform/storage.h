#pragma once

// Файлы игрока в папке пользователя (D-11, D-30): чтение, запись целиком и
// дописывание. Текст файлов — core/profile_file.h, core/history.h.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aim::platform {

/// Перевод строки системы — как Writeln образца (Environment.NewLine).
#ifdef _WIN32
inline constexpr std::string_view kNewline = "\r\n";
#else
inline constexpr std::string_view kNewline = "\n";
#endif

class Storage {
public:
    /// Папка по умолчанию: $AIM_TRAINER_DATA_DIR, если задана (проверки),
    /// иначе SDL_GetPrefPath без организации — на Windows %APPDATA%\AimTrainer\,
    /// на Linux ~/.local/share/AimTrainer/ (D-30). В ней — aim_trainer.ini и,
    /// пока игрок не выбрал другую (D-33), файлы игрока. Нет — пусто и problem.
    static std::optional<Storage> default_folder(std::string& problem);
    /// Папка dir: нет — создать. Не создаётся — пусто и problem.
    static std::optional<Storage> open(std::string dir, std::string& problem);

    explicit Storage(std::string dir);

    /// Каталог с разделителем на конце.
    [[nodiscard]] const std::string& dir() const { return dir_; }
    [[nodiscard]] std::string path(std::string_view name) const;

    /// Содержимое файла; нет файла или не читается — пусто.
    [[nodiscard]] std::optional<std::string> read(std::string_view name) const;
    /// Записать целиком: во временный файл рядом и переименовать — файл не
    /// бывает записан наполовину. false — error с причиной.
    bool write(std::string_view name, std::string_view data, std::string& error) const;
    /// Дописать в конец; файла нет — создать.
    bool append(std::string_view name, std::string_view data, std::string& error) const;
    [[nodiscard]] bool exists(std::string_view name) const;
    /// Имена файлов папки (без вложенных папок); папка не читается — пусто
    /// и error.
    [[nodiscard]] std::optional<std::vector<std::string>> files(std::string& error) const;
    /// В папку можно писать: пробный файл записан и удалён.
    bool writable(std::string& error) const;

private:
    std::string dir_;
};

}  // namespace aim::platform
