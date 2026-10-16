#pragma once

// Файлы игрока программы: профиль, история, экспорт, последний профиль и
// папка сохранений (D-11, D-30, D-33) — текст из core/profile_file.h и
// core/history.h, файлы — platform/storage.h. Ошибки записи не молчат
// (AUD-17): каждая — строка для уведомления игроку.
//
// Папок две: по умолчанию — там aim_trainer.ini (последний профиль и
// выбранная папка), и папка сохранений — там файлы игрока; пока игрок не
// выбрал другую, это одна папка.

#include <optional>
#include <string>
#include <vector>

#include "core/game.h"
#include "core/history.h"
#include "core/profile.h"
#include "core/profile_file.h"
#include "platform/storage.h"

namespace aim::app {

/// Местные дата и время сейчас — для строки истории и имени экспорта.
core::LocalTime local_now();

class PlayerFiles {
public:
    /// home — папка по умолчанию; нет папки — профиль только в памяти, о чём
    /// скажет первое сообщение load.
    explicit PlayerFiles(std::optional<platform::Storage> home, std::string problem = {});

    /// Папка сохранений из aim_trainer.ini и профиль, открытый последним
    /// (AUD-21). Выбранная папка недоступна — папка по умолчанию и сообщение.
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

    /// «Экспорт в CSV»: копия истории — export_profileN_ГГГГММДД.csv в папке
    /// сохранений (D-33). Истории нет — false и error «нет данных».
    bool export_history(const core::LocalTime& when, std::string& file_name, std::string& error);

    /// Сменить папку сохранений на dir (D-33, D-35): файлы игрока прежней
    /// папки копируются в новую; файл новой папки с тем же именем и другим
    /// содержимым сперва уходит в её подпапку backup_ГГГГММДД_ЧЧММ (when).
    /// Прежняя папка не трогается. Удалось — профиль текущего номера из новой
    /// папки; нет — пусто, папка прежняя. messages — что сказать игроку.
    std::optional<core::Profile> change_folder(const std::string& dir, const core::LocalTime& when,
                                               std::vector<std::string>& messages);

    [[nodiscard]] int profile_number() const { return number_; }
    [[nodiscard]] bool available() const { return storage_.has_value(); }
    /// Папка сохранений, с разделителем на конце; нет — пусто.
    [[nodiscard]] std::string folder() const;

private:
    /// Записать aim_trainer.ini, если номер профиля или папка изменились.
    bool remember(std::string& error);
    /// Файлы игрока из from — в target (change_folder).
    static bool copy_saves(const platform::Storage& from, const platform::Storage& target,
                           const core::LocalTime& when, std::vector<std::string>& messages);
    /// Свободная подпапка backup_… в target; name — её имя.
    static std::optional<platform::Storage> backup_folder(const platform::Storage& target,
                                                          const core::LocalTime& when,
                                                          std::string& name, std::string& error);

    std::optional<platform::Storage> home_;
    std::optional<platform::Storage> storage_;
    std::string problem_;
    int number_ = 1;
    core::AppState remembered_;  ///< что записано в aim_trainer.ini
    std::string data_dir_;  ///< выбранная папка; пусто — по умолчанию
};

}  // namespace aim::app
