#pragma once

// Файлы игрока программы: профиль, история замеров, экспорт, реплеи,
// последний профиль и папка сохранений (D-30, D-33, D-38, D-39) — текст из
// core/profile_file.h, core/runs.h и core/replay.h, файлы —
// platform/storage.h. Ошибки записи не молчат (AUD-17): каждая — строка для
// уведомления игроку.
//
// Папок две: по умолчанию — там aim_trainer.ini (последний профиль и
// выбранная папка), и папка сохранений — там файлы игрока; пока игрок не
// выбрал другую, это одна папка.

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "core/profile.h"
#include "core/profile_file.h"
#include "core/replay.h"
#include "core/runs.h"
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
    /// Профиль number (1…kProfileCount) с историей замеров: он же —
    /// последний открытый. Нет файла — профиль по умолчанию и записать его;
    /// файл не этого формата — копия profileN_corrupt_backup.txt и то же;
    /// поля вне смысла — сброшены и записаны (AUD-07); испорченные строки
    /// истории — пропущены. messages — что сказать игроку.
    core::Profile open(int number, std::vector<std::string>& messages);

    /// Записать профиль без истории (и номер последнего). false — error для
    /// уведомления.
    bool save(const core::Profile& profile, std::string& error);
    /// Дописать замер в историю (строка версии и заголовок — если файла ещё
    /// нет).
    bool log_run(const core::RunRecord& run, std::string& error);
    /// Имя файла истории профиля.
    [[nodiscard]] std::string history_file_name() const;

    /// Реплей лучшего замера набора (D-39) — в replays/ папки сохранений;
    /// прежний реплей набора заменяется.
    bool save_replay(const core::Replay& replay, std::string& error);
    /// Реплей набора options у профиля; нет или не читается — пусто и error.
    [[nodiscard]] std::optional<core::Replay> load_replay(const core::MeasureOptions& options,
                                                          std::string& error) const;
    /// У набора options профиля есть реплей.
    [[nodiscard]] bool has_replay(const core::MeasureOptions& options) const;

    /// «Экспорт в CSV»: копия истории — export_profileN_ГГГГММДД.csv в папке
    /// сохранений (D-33). Истории нет — false и error «нет данных».
    bool export_history(const core::LocalTime& when, std::string& file_name, std::string& error);

    /// Сменить папку сохранений на dir (D-33, D-35): файлы игрока прежней
    /// папки и её реплеи копируются в новую; файл новой папки с тем же именем и другим
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
    /// Перечитать, какие реплеи есть в папке сохранений.
    void scan_replays();
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
    std::set<std::string, std::less<>> replays_;  ///< файлы папки реплеев
};

}  // namespace aim::app
