#pragma once

// Замер реакции — набор опций конструктора вместо режимов (D-37). Одно
// место для правил цели, готовых наборов, ключа набора в файлах (рекорд и
// среднее — по нему, D-38) и подписи параметров для игрока.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/time.h"

namespace aim::core {

/// Что меняется на экране: цель на поле или цвет всего окна.
enum class Stimulus : std::uint8_t { Target, Screen };
/// Как берётся цель: щелчком или наведением (удержанием курсора).
enum class Action : std::uint8_t { Click, Hover };
/// Когда замер кончается: по жизням или по времени.
enum class Ending : std::uint8_t { Lives, Time };

// Границы опций — конструктор и разбор файла.
inline constexpr int kMinTargetSize = 20;
inline constexpr int kMaxTargetSize = 60;
inline constexpr int kTargetSizeStep = 2;
inline constexpr int kMinLifetimeMs = 300;
inline constexpr int kMaxLifetimeMs = 5000;
inline constexpr int kLifetimeStepMs = 100;
inline constexpr int kMinLives = 1;
inline constexpr int kMaxLives = 30;
inline constexpr int kMinDurationS = 10;
inline constexpr int kMaxDurationS = 300;
inline constexpr int kDurationStepS = 10;

/// Замер на цвет: пауза до смены цвета — случайная, в этих границах.
inline constexpr Nanoseconds kScreenMinDelay = ms(1000);
inline constexpr Nanoseconds kScreenMaxDelay = ms(3000);

struct MeasureOptions {
    Stimulus stimulus = Stimulus::Target;
    Action action = Action::Click;
    bool color_buttons = false;  ///< синие — ЛКМ, красные — ПКМ; иначе только ЛКМ
    bool moving = false;  ///< цель движется и отскакивает от краёв
    bool grows = true;  ///< растёт и сжимается; иначе сразу полного размера
    bool bombs = true;       ///< бывают бомбы
    int target_size = 40;    ///< пик радиуса при масштабе 1, px
    int lifetime_ms = 1000;  ///< жизнь цели, мс
    Ending ending = Ending::Lives;
    int lives = 15;
    int duration_s = 60;

    friend bool operator==(const MeasureOptions&, const MeasureOptions&) = default;
};

/// Опции в границах, а те, что к замеру не относятся, — по умолчанию:
/// одинаковые по сути замеры дают один набор. Смена цвета — без цели и её
/// опций; наведение — без кнопок и бомб; окончание по жизням — без времени,
/// по времени — без жизней.
MeasureOptions normalized(const MeasureOptions& options);

/// Ключ набора: одна строка из нормализованных опций — рекорд и среднее
/// считаются по ней (D-38).
std::string options_key(const MeasureOptions& options);

/// Параметры замера для игрока, по одному на строку: «Стимул: цель», …
/// Только те, что относятся к замеру.
std::vector<std::string> describe(const MeasureOptions& options);
/// Те же параметры коротко, в одну строку.
std::string describe_short(const MeasureOptions& options);

/// Готовый набор конструктора.
struct Preset {
    std::string_view name;
    std::string_view description;
    MeasureOptions options;
    /// Время жизни цели — доля общей настройки (D-21): 1 — как есть.
    double lifetime_factor = 1.0;
};

inline constexpr int kPresetCount = 5;
/// Наборы по порядку списка: первым — замер на цвет (D-37).
extern const std::array<Preset, kPresetCount> kPresets;

/// Общие настройки, которые пока задают размер, жизнь цели, жизни и
/// окончание всех наборов — до правки опций в конструкторе.
struct PresetSettings {
    int target_size = 40;
    int lifetime_ms = 1000;
    int lives = 15;
    Ending ending = Ending::Lives;
};

/// Опции набора index с общими настройками.
MeasureOptions preset_options(int index, const PresetSettings& settings);
/// Имя набора, к которому относится замер (по стимулу, действию, кнопкам,
/// движению, появлению и бомбам); не набор — пусто.
std::optional<std::string_view> preset_name(const MeasureOptions& options);

/// Как живёт и засчитывается цель — из опций.
struct TargetRules {
    Nanoseconds lifetime = 0;
    bool grows = false;
    bool two_buttons = false;  ///< синие — ЛКМ, красные — ПКМ; иначе только ЛКМ
    bool moves = false;
    bool hover = false;  ///< засчитывается наводкой, щелчки не читаются
    bool bombs = false;
    int top_reserve = 0;  ///< место сверху под подсказку кнопок, px
};
TargetRules target_rules(const MeasureOptions& options);

// Опции в файлах — слова, а не числа: файл читается глазами.
std::string_view stimulus_word(Stimulus s);
std::string_view action_word(Action a);
std::string_view ending_word(Ending e);
std::optional<Stimulus> parse_stimulus(std::string_view word);
std::optional<Action> parse_action(std::string_view word);
std::optional<Ending> parse_ending(std::string_view word);

}  // namespace aim::core
