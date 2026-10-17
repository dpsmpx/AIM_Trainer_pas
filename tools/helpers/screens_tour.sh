#!/bin/bash
# Обход экранов C++-версии (D-27, D-43): с чистой папки игрока — туториал,
# каждый пункт меню, каждая панель, справка до конца, конструктор, каждый
# набор (отсчёт, замер, итог), реплей с итога и из «Рекордов наборов», меню
# без эффектов — снимок каждого в КАТАЛОГ.
# Программа идёт с --text-check: надписи, которые не поместились в свой
# прямоугольник, — в конце, из журнала программы.
#
#   tools/helpers/screens_tour.sh КАТАЛОГ [condensed|mono]
#
# Второй аргумент — шрифт (настройка «Шрифт», по умолчанию condensed).
# Код выхода 1 — что-то не поместилось.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:?КАТАЛОГ для снимков}"
FONT="${2:-condensed}"
GUI="$ROOT/tools/gui.sh"
mkdir -p "$OUT"

gui() { "$GUI" port "$@"; }
shot() { gui shot "$OUT/$1.png"; }
bot() { timeout 30 python3 "$ROOT/tools/helpers/ref_bot.py" play :96 "$@" >/dev/null || true; }

gui stop >/dev/null 2>&1 || true
gui clean
AIM_PORT_ARGS=--text-check gui start >/dev/null
gui move 700 560
sleep 0.8
for step in 1 2 3 4; do
    shot "01_tutorial_$step"
    gui click 400 300
    sleep 0.3
done

if [ "$FONT" = mono ]; then
    # «Настройки» → строка «Шрифт» (core/layout.h: панель 560 px, ряд 7).
    gui click 155 577
    sleep 0.3
    gui click 400 316
    gui key Escape
    sleep 0.3
fi

# Пункты меню: наборы и «Свой набор» — наведение.
for i in 0 1 2 3 4 5; do
    gui move 585 $(( 53 + i * 31 ))
    sleep 0.4
    shot "02_menu_item_$i"
done

# Кнопки внизу: середина кнопки — 55 + i·100, 577.
i=0
for name in help settings records achievements history profiles; do
    gui click $(( 55 + i * 100 )) 577
    sleep 0.4
    shot "03_panel_$name"
    if [ "$name" = help ]; then
        gui key Page_Down Page_Down Page_Down
        sleep 0.3
        shot "03_panel_help_end"
    fi
    gui key Escape
    sleep 0.3
    i=$(( i + 1 ))
done

# Конструктор: как есть и со сменой цвета окна.
gui click 585 208
sleep 0.4
shot 04_constructor
gui click 400 154
sleep 0.3
shot 04_constructor_screen
gui click 400 154
gui key Escape
sleep 0.3

# Наборы: отсчёт, замер, итог.
policies=(none lmb dual hover lmb)
for i in 0 1 2 3 4; do
    gui click 585 $(( 53 + i * 31 ))
    sleep 1.2
    shot "05_preset_${i}_countdown"
    sleep 2.2
    if [ "$i" = 0 ]; then
        # Замер на цвет: щелчки наугад — реакции и фальстарты.
        for _ in 1 2 3 4 5 6; do
            sleep 0.9
            gui click 400 300
        done
    else
        bot "${policies[$i]}" 4
    fi
    shot "05_preset_${i}_game"
    gui key Escape
    sleep 0.7
    shot "05_preset_${i}_over"
    if [ "$i" = 1 ]; then
        # Первый замер набора — рекорд: «▶ Реплей» — третья кнопка итога
        # (core/layout.h, result_button), назад — на итог (D-46).
        gui click 706 532
        sleep 1.5
        shot "05_preset_${i}_replay"
        gui key Escape
        sleep 0.3
        shot "05_preset_${i}_over_back"
    fi
    gui key space
    sleep 0.4
done

# Реплей рекорда — первая строка «Рекордов наборов».
gui click 255 577
sleep 0.4
shot 06_records_after
gui click 400 133
sleep 2
shot 06_replay
gui key Escape
sleep 0.3

# Эффекты меню выключены (D-47): «Настройки» → ряд 8.
gui key Escape
gui click 155 577
sleep 0.3
gui click 400 348
sleep 0.2
shot 07_settings_effects_off
gui key Escape
sleep 0.3
shot 07_menu_effects_off

gui stop >/dev/null 2>&1 || true
LOG="$ROOT/build/gui/port/run.log"
echo "снимки — $OUT"
if grep -q "не помещается" "$LOG"; then
    grep "не помещается" "$LOG"
    exit 1
fi
echo "все надписи поместились"
