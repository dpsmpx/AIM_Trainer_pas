// Общее ядро замерщиков ввода: одна шкала времени, эталон сырого ввода,
// самопроверка через SendInput, протокол целей и фоновый журнал рядом с
// программой. Каждый замерщик (`probe_<библиотека>.cpp`) только создаёт окно
// своей библиотекой, рисует то, что велит Session, и сообщает ей о кликах.
//
// Зачем и как читать журнал — docs/LIB_PROBES.md (поручение автора
// 15 октября 2026: выбрать библиотеку по точности замера реакции).
//
// Заголовок не тянет <windows.h>: raylib и другие библиотеки объявляют имена,
// которые с ним конфликтуют (Rectangle, CloseWindow, DrawText).
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace probe {

using Ns = std::uint64_t;

// Время по QueryPerformanceCounter в наносекундах от запуска программы. Им
// меряется всё, чтобы числа разных библиотек были в одной шкале.
Ns now();

// Метка «событие случилось age назад» в шкале now().
Ns from_age(Ns at, std::uint64_t age_ns);

struct LibInfo {
    std::string name;       // имя библиотеки — и начало имён файлов журнала
    std::string version;
    std::string renderer;   // чем рисует и что входит в «показ кадра»
    std::string timestamp;  // откуда библиотека берёт время клика (по исходнику)
    // Свой поток сырого ввода — эталон времени клика. Выключается, когда
    // сырой ввод нужен самой библиотеке: в процессе его получает кто-то один.
    bool raw_reference = true;
    // Самопроверка проверяет, что курсор стоит там, куда его поставили. В
    // относительном режиме мыши курсор держит библиотека — проверка лишняя.
    bool check_cursor = true;
};

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
};

// Что нарисовать в этом кадре. Координаты — в клиентской области 800×600.
struct View {
    bool target = false;
    float x = 0, y = 0, r = 0;
    float progress = 0;  // 0..1 — полоса вверху окна
    Rgb bar;             // жёлтая — самопроверка, синяя — серия, красная — сбой
    float bar_h = 6;     // высота полосы; при сбое — широкая, чтобы не проглядеть
};

constexpr int kWidth = 800;
constexpr int kHeight = 600;
constexpr Rgb kBackground{28, 28, 32};
constexpr Rgb kTarget{97, 207, 255};

class Session {
public:
    // hwnd — HWND окна библиотеки: окно сдвигается целиком в рабочую область
    // экрана, самопроверка ставит курсор в его клиентскую область.
    Session(LibInfo info, void* hwnd);
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Начало кадра: что рисовать.
    View frame();
    // Библиотека отдала программе нажатие кнопки мыши. button: 1 — основная
    // (обычно левая), 2 — правая, 3 — средняя. delivered — now() в момент,
    // когда программа получила событие; lib_time — метка библиотеки в шкале
    // now(), если есть; native — та же метка в единицах библиотеки, как есть
    // (для пересчёта по таблице).
    void click(int button, float x, float y, Ns delivered, std::optional<Ns> lib_time,
               std::string_view native = {});
    // Кадр отдан на показ: now() до и после вызова функции показа.
    void presented(Ns call, Ns ret);
    // Строка в журнал с отметкой времени — например, почему программа выходит.
    void note(std::string_view text);
    // Новый заголовок окна (UTF-8) или пустая строка, если менять не нужно.
    std::string take_title();
    // Итог в журнал и остановка потоков. Повторный вызов ничего не делает.
    void finish();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace probe
