#!/bin/bash
# Обход экранов для сверки C++-версии с эталоном (шаг «Экраны», D-26, D-27):
# с чистой папки игрока — туториал, меню, каждая панель, отсчёт, забег с
# попаданием, пауза, Game Over — снимок каждого в КАТАЛОГ.
#
#   tools/helpers/screens_tour.sh ЦЕЛЬ КАТАЛОГ
#
# ЦЕЛЬ — ref или port (tools/gui.sh). Паузу у эталона не снимает: он на ней
# падает (сценарий 22). Сравнить пары — compare.png: montage рядом, ref слева.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TARGET="${1:?ЦЕЛЬ: ref или port}"
OUT="${2:?КАТАЛОГ для снимков}"
GUI="$ROOT/tools/gui.sh"
DISPLAY_OF=":96"
[ "$TARGET" = ref ] && DISPLAY_OF=":97"
mkdir -p "$OUT"

shot() { "$GUI" "$TARGET" shot "$OUT/$1.png"; }

"$GUI" "$TARGET" stop >/dev/null 2>&1 || true
"$GUI" "$TARGET" clean
"$GUI" "$TARGET" start >/dev/null
"$GUI" "$TARGET" move 700 560
sleep 0.8
shot 01_tutorial
"$GUI" "$TARGET" key Escape
sleep 0.3
"$GUI" "$TARGET" move 700 560
sleep 0.5
shot 02_menu

# Кнопки внизу: x — середина кнопки, y — её середина (DrawMiniButton).
i=3
for name in help settings records achievements history profiles; do
    x=$(( 55 + (i - 3) * 100 ))
    "$GUI" "$TARGET" click "$x" 577
    sleep 0.4
    shot "$(printf '%02d' "$i")_$name"
    "$GUI" "$TARGET" key Escape
    sleep 0.3
    i=$(( i + 1 ))
done

# Наведение на Dual и плашку.
"$GUI" "$TARGET" move 585 99
sleep 0.4
shot 09_hover_dual
"$GUI" "$TARGET" move 585 57
sleep 0.2

# Classic: отсчёт, забег с попаданиями, пауза (только port), Game Over.
"$GUI" "$TARGET" click 585 57
sleep 1.2
shot 10_countdown
sleep 2.2
timeout 20 python3 "$ROOT/tools/helpers/ref_bot.py" play "$DISPLAY_OF" lmb 4 3 >/dev/null || true
shot 11_game
if [ "$TARGET" = port ]; then
    "$GUI" "$TARGET" key p
    sleep 0.3
    shot 12_pause
    "$GUI" "$TARGET" key p
fi
"$GUI" "$TARGET" key Escape
sleep 0.5
shot 13_game_over
"$GUI" "$TARGET" key space
sleep 0.5
shot 14_menu_after
echo "снимки — $OUT"
