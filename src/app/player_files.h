#pragma once

// Файлы игрока программы: профиль, история, последний профиль (D-11, D-30) —
// текст из core/profile_file.h и core/history.h, файлы — platform/storage.h.
// Ошибки записи не молчат (AUD-17): каждая — строка для уведомления игроку.

#include <optional>
#include <string>
#include <vector>

#include "core/game.h"
#include "core/profile.h"
#include "platform/storage.h"

namespace aim::app {

class PlayerFiles {
public:
    /// storage — папка игрока; нет папки — профиль только в памяти, о чём
    /// скажет первое сообщение load.
    explicit PlayerFiles(std::optional<platform::Storage> storage, std::string problem = {});

    /// Профиль, открытый последним (AUD-21).
    core::Profile load(std::vector<std::string>& messages);
    /// Профиль number (1…kProfileCount): он же — последний открытый. Нет
    /// файла — профиль по умолчанию и записать его, как у образца;
    /// повреждённый файл — копия profileN_corrupt_backup.txt и то же; поля вне
    /// диапазона — сброшены и записаны (AUD-07). messages — что сказать игроку.
    core::Profile open(int number, std::vector<std::string>& messages);

    /// Записать профиль (и номер последнего). false — error для уведомления.
    bool save(const core::Profile& profile, std::string& error);
    /// Дописать забег в историю (заголовок — если файла ещё нет).
    bool log_run(const core::GameSummary& summary, std::string& error);
    /// История профиля целиком; нет файла — пусто.
    [[nodiscard]] std::optional<std::string> read_history() const;
    /// Имя файла истории профиля.
    [[nodiscard]] std::string history_file_name() const;

    [[nodiscard]] int profile_number() const { return number_; }
    [[nodiscard]] bool available() const { return storage_.has_value(); }

private:
    /// Записать номер профиля как последний, если он ещё не записан.
    bool remember(std::string& error);

    std::optional<platform::Storage> storage_;
    std::string problem_;
    int number_ = 1;
    int remembered_ = 1;  ///< номер в aim_trainer.ini
};

}  // namespace aim::app
