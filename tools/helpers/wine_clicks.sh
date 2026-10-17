#!/bin/bash
# Сколько кликов Windows-сборка получает со временем из сырого ввода (D-24):
# окно под Wine на Xvfb, щелчки xdotool, источник времени — из заголовка
# окна (сценарий 21 docs/MANUAL_CHECKS.md, но без Windows). Так найдена гонка
# потока сырого ввода и события SDL (docs/ARCHITECTURE.md, §3).
#
#   tools/helpers/wine_clicks.sh [КЛИКОВ] [ЖУРНАЛ]
#
# Нужна сборка пресетом windows-cross (tools/check_cpp.sh windows-cross);
# экран — :95. Итог — «raw: N  event: M  всего: K» и клики не из сырого ввода.
# Причину такого клика показывает журнал заплатки
# tools/helpers/click_debug_log.patch.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
N="${1:-100}"
LOG="${2:-$ROOT/build/wine_clicks.txt}"
EXE="$ROOT/build/windows-cross/AimTrainer.exe"
[ -f "$EXE" ] || { echo "нет $EXE — tools/check_cpp.sh windows-cross" >&2; exit 1; }

export WINEPREFIX="$ROOT/build/wineprefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
export DISPLAY="${AIM_WINE_DISPLAY:-:95}"
if ! xdotool getmouselocation >/dev/null 2>&1; then
    setsid Xvfb "$DISPLAY" -screen 0 1280x1024x24 -nolisten tcp >/dev/null 2>&1 &
    sleep 1.5
fi

# По имени процесса, а не pkill -f: тот шаблон находит и вызвавшую оболочку.
pkill -x AimTrainer.exe || true
# Своя сессия: сигналы Wine не задевают этот скрипт.
setsid wine "$EXE" >/dev/null 2>&1 &
W=
for _ in $(seq 1 60); do
    W="$(xdotool search --name "Aim Trainer" 2>/dev/null | head -1 || true)"
    if [ -n "$W" ] && xdotool getwindowname "$W" 2>/dev/null | grep -q 'C++'; then break; fi
    sleep 0.5
done
[ -n "$W" ] || { echo "окно не появилось" >&2; exit 1; }
echo "окно: $(xdotool getwindowname "$W")"
eval "$(xdotool getwindowgeometry --shell "$W")"

# Первый клик — разогрев: SDL заводит сдвиг своих меток (ARCHITECTURE, §12).
xdotool mousemove $((X + 400)) $((Y + 300)) click 1
sleep 0.5
: >"$LOG"
for i in $(seq 1 "$N"); do
    xdotool mousemove $((X + 100 + (i % 60) * 10)) $((Y + 300))
    sleep 0.03
    xdotool click $(((i % 2) * 2 + 1))  # ЛКМ и ПКМ вперемешку
    sleep 0.12
    echo "$i $(xdotool getwindowname "$W" 2>&1)" >>"$LOG"
done
pkill -x AimTrainer.exe || true
echo "raw: $(grep -c 'сырого ввода' "$LOG")  event: $(grep -c 'события SDL' "$LOG")  всего: $(wc -l <"$LOG")"
grep -v 'сырого ввода' "$LOG" | head -5 || true
