#pragma once

// Файлы игрока в папке пользователя (D-11, D-30): чтение, запись целиком и
// дописывание. Текст файлов — core/profile_file.h, core/history.h.

#include <optional>
#include <string>
#include <string_view>

namespace aim::platform {

/// Перевод строки системы — как Writeln образца (Environment.NewLine).
#ifdef _WIN32
inline constexpr std::string_view kNewline = "\r\n";
#else
inline constexpr std::string_view kNewline = "\n";
#endif

class Storage {
public:
    /// Папка игрока: $AIM_TRAINER_DATA_DIR, если задана (проверки), иначе
    /// SDL_GetPrefPath без организации — на Windows %APPDATA%\AimTrainer\, на
    /// Linux ~/.local/share/AimTrainer/ (D-30). Нет — пусто и problem.
    static std::optional<Storage> open_user_folder(std::string& problem);

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

private:
    std::string dir_;
};

}  // namespace aim::platform
