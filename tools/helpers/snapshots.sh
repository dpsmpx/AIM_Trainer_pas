#!/bin/bash
# Снимки всех экранов из теста обхода (tests/app/screens_test.cpp, AUD-49) и
# их сверка: правка отрисовки, которая не должна менять картинку (холст,
# AUD-46), сверяется попиксельно до и после (D-27).
#
#   tools/helpers/snapshots.sh shoot КАТАЛОГ [ПРЕСЕТ]   кадры обхода обоими шрифтами — BMP в КАТАЛОГ
#   tools/helpers/snapshots.sh diff ДО ПОСЛЕ            какие кадры отличаются и сколькими пикселями
#
# ПРЕСЕТ — сборка с тестами (по умолчанию linux-gcc; tools/check_cpp.sh или
# cmake --build --preset). Обход детерминирован: те же часы, те же случайные
# числа — те же кадры. diff: код выхода 1 — есть отличия; разница каждого
# кадра — ИМЯ.diff.png рядом с кадром в ПОСЛЕ.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

usage() { sed -n '2,13p' "$0"; exit 2; }

shoot() {
    local out="${1:?КАТАЛОГ}" preset="${2:-linux-gcc}"
    local tests="$ROOT/build/$preset/tests"
    [ -x "$tests/aim_app_tests" ] || { echo "нет $tests/aim_app_tests — собрать пресет $preset" >&2; exit 1; }
    rm -rf "$out"
    mkdir -p "$out"
    out="$(cd "$out" && pwd)"
    # Тест пишет свои файлы игрока в текущий каталог — каталог тестов.
    (cd "$tests" && env -u DISPLAY AIM_SNAPSHOT_DIR="$out" ./aim_app_tests \
        --gtest_filter='*ScreensTest*' >/dev/null)
    echo "кадров: $(find "$out" -name '*.bmp' | wc -l) — $out"
}

diff_dirs() {
    local before="${1:?ДО}" after="${2:?ПОСЛЕ}" differ=0 total=0 name n
    for f in "$before"/*.bmp; do
        name="$(basename "$f")"
        total=$((total + 1))
        if [ ! -f "$after/$name" ]; then
            echo "$name: нет в $after"
            differ=$((differ + 1))
            continue
        fi
        n="$(compare -metric AE "$f" "$after/$name" "$after/${name%.bmp}.diff.png" 2>&1 >/dev/null || true)"
        if [ "$n" = "0" ]; then
            rm -f "$after/${name%.bmp}.diff.png"
        else
            echo "$name: отличается пикселей — $n"
            differ=$((differ + 1))
        fi
    done
    echo "кадров: $total, отличаются: $differ"
    [ "$differ" -eq 0 ]
}

case "${1:-}" in
    shoot) shift; shoot "$@" ;;
    diff) shift; diff_dirs "$@" ;;
    *) usage ;;
esac
