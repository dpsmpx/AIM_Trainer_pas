#pragma once

// Программа: окно и главный цикл. Окно — логические 800×600, умноженные на
// масштаб монитора при запуске (D-42); кадр — с частотой монитора (D-41);
// часы кадра — SDL_GetTicksNS; щелчок — точка из события SDL и время из
// своего потока сырого ввода (D-24); клавиши — события, по физическому
// положению; реакция — от показа кадра со стимулом (D-12). Экраны —
// app/screens.h; файлы игрока — app/player_files.h.

#include <optional>
#include <string>

namespace aim::app {

/// Ключи запуска:
///
///   AimTrainer [--frames N] [--screenshot файл.bmp] [--debug-title] [--text-check]
///
/// --frames N — выйти после N кадров; --screenshot — записать последний кадр
/// в BMP. Оба — для проверки без экрана (tests/CMakeLists.txt).
/// --debug-title — в заголовке окна откуда взято время щелчка (сценарий 21,
/// D-24) и запас кадра замера (сценарий 4, D-41). --text-check — каждую
/// надпись, которая не поместилась в свой прямоугольник, — в журнал; были
/// такие — код выхода 3 (D-43).
struct Options {
    int frames = 0;  ///< 0 — до закрытия окна
    std::string screenshot;
    bool debug_title = false;
    bool text_check = false;
};

/// Ключи запуска; неверные — пусто.
std::optional<Options> parse_options(int argc, char* argv[]);
/// Строка «использование» для журнала.
inline constexpr const char* kUsage =
    "использование: AimTrainer [--frames N] [--screenshot файл.bmp] [--debug-title] "
    "[--text-check]";

/// Окно, главный цикл до закрытия; код выхода: 0 — всё хорошо, 1 — окно,
/// шрифт или снимок не удались, 3 — с --text-check не поместились надписи.
int run(const Options& options);

}  // namespace aim::app
