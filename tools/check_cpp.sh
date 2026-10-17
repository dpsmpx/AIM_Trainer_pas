#!/bin/bash
# Проверка C++-версии одной командой (D-19, D-25) — перед каждым коммитом с
# кодом C++; итог в конце — в описание PR (D-07).
#
#   tools/check_cpp.sh              документы, оформление, слои, скрипты;
#                                   сборка и тесты всех пресетов сессии
#   tools/check_cpp.sh --fix        то же, но оформление clang-format правится
#   tools/check_cpp.sh ПРЕСЕТ…      только эти пресеты (остальные проверки — всегда)
#
# Пресеты сессии: linux-gcc, linux-clang, linux-asan, linux-tidy (clang-tidy
# при сборке, замечание — ошибка), linux-tsan (ThreadSanitizer: поток звука,
# диалог папки), windows-cross (llvm-mingw, тесты под Wine). Покрытие —
# отдельно: tools/helpers/coverage.sh.
# Инструменты ставит tools/setup_env.sh; его env.sh подключается сам.
# Журналы — build/check/. Код возврата: 0 — всё зелёное, 1 — есть провал.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

ENV_FILE="${AIM_TOOLS_DIR:-$HOME/.local/share/aim-trainer-tools}/env.sh"
# shellcheck source=/dev/null
[ -f "$ENV_FILE" ] && source "$ENV_FILE"

ALL_PRESETS=(linux-gcc linux-clang linux-asan linux-tidy linux-tsan windows-cross)
FIX=0
PRESETS=()
for arg in "$@"; do
    case "$arg" in
        --fix) FIX=1 ;;
        -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
        -*) echo "неизвестный ключ: $arg" >&2; exit 2 ;;
        *)
            if [[ " ${ALL_PRESETS[*]} " != *" $arg "* ]]; then
                echo "неизвестный пресет: $arg (есть: ${ALL_PRESETS[*]})" >&2
                exit 2
            fi
            PRESETS+=("$arg")
            ;;
    esac
done
[ "${#PRESETS[@]}" -gt 0 ] || PRESETS=("${ALL_PRESETS[@]}")

LOGS="$ROOT/build/check"
mkdir -p "$LOGS"
RESULTS=()
FAILED=0

# result ИМЯ ИТОГ — строка итоговой таблицы; ИТОГ «зелёное» или причина провала.
result() {
    RESULTS+=("| $1 | $2 |")
    [ "$2" = "зелёное" ] || FAILED=1
    printf '== %s: %s\n' "$1" "$2"
}

tool() {  # первый найденный из списка
    local t
    for t in "$@"; do
        if command -v "$t" >/dev/null 2>&1; then echo "$t"; return 0; fi
    done
    return 1
}

# ---- Документы ----
if python3 tools/check_docs.py >"$LOGS/docs.log" 2>&1; then
    result "документы (tools/check_docs.py)" "зелёное"
else
    cat "$LOGS/docs.log"
    result "документы (tools/check_docs.py)" "есть находки"
fi

# ---- Оформление ----
mapfile -t SOURCES < <(git ls-files -co --exclude-standard -- src tests |
                       grep -E '\.(cpp|h)$' | sort)
if ! FORMAT="$(tool clang-format-18 clang-format)"; then
    result "оформление (clang-format)" "нет clang-format"
elif [ "${#SOURCES[@]}" -eq 0 ]; then
    result "оформление (clang-format)" "нет исходников"
elif [ "$FIX" -eq 1 ]; then
    "$FORMAT" -i "${SOURCES[@]}"
    result "оформление (clang-format --fix)" "зелёное"
elif "$FORMAT" --dry-run --Werror "${SOURCES[@]}" >"$LOGS/format.log" 2>&1; then
    result "оформление (clang-format)" "зелёное"
else
    head -40 "$LOGS/format.log"
    result "оформление (clang-format)" "не по .clang-format — tools/check_cpp.sh --fix"
fi

# ---- Слои (D-25): core — без SDL и ОС, platform — без app ----
layers_bad=0
if grep -rnE '#include[[:space:]]*[<"](SDL3/|windows\.h|platform/|app/)' src/core; then
    echo "src/core включает SDL, ОС или верхние слои" >&2
    layers_bad=1
fi
if grep -rnE '#include[[:space:]]*"app/' src/platform; then
    echo "src/platform включает слой app" >&2
    layers_bad=1
fi
if [ "$layers_bad" -eq 0 ]; then
    result "слои: core ← platform ← app" "зелёное"
else
    result "слои: core ← platform ← app" "нарушены"
fi

# ---- Скрипты ----
# Локаль UTF-8 обязательна: без неё shellcheck спотыкается о кириллицу.
if command -v shellcheck >/dev/null 2>&1; then
    if LC_ALL=C.UTF-8 shellcheck -x tools/*.sh tools/lib_probes/*.sh tools/helpers/*.sh .claude/hooks/*.sh \
        >"$LOGS/shellcheck.log" 2>&1; then
        result "скрипты (shellcheck)" "зелёное"
    else
        head -40 "$LOGS/shellcheck.log"
        result "скрипты (shellcheck)" "есть замечания"
    fi
else
    result "скрипты (shellcheck)" "нет shellcheck"
fi

# ---- Пресеты ----
for preset in "${PRESETS[@]}"; do
    name="пресет $preset"
    if ! cmake --preset "$preset" >"$LOGS/$preset-configure.log" 2>&1; then
        tail -20 "$LOGS/$preset-configure.log"
        result "$name" "не настраивается — build/check/$preset-configure.log"
        continue
    fi
    if ! cmake --build --preset "$preset" >"$LOGS/$preset-build.log" 2>&1; then
        grep -E 'error|warning:' "$LOGS/$preset-build.log" | grep -v '/_deps/' | head -40
        result "$name" "не собирается — build/check/$preset-build.log"
        continue
    fi
    # linux-tidy проверяет код при сборке; его тесты — те же, что у linux-clang.
    if [ "$preset" = "linux-tidy" ]; then
        result "$name: сборка с clang-tidy" "зелёное"
        continue
    fi
    if [ "$preset" = "windows-cross" ] && ! command -v wine >/dev/null 2>&1; then
        result "$name: сборка" "зелёное"
        result "$name: тесты" "не запускались — нет wine (tools/setup_env.sh)"
        continue
    fi
    # Тесты — без экрана: окно программы — на драйвере offscreen (tests/CMakeLists.txt).
    if env -u DISPLAY -u WAYLAND_DISPLAY ctest --preset "$preset" >"$LOGS/$preset-test.log" 2>&1; then
        summary="$(grep -E 'tests passed' "$LOGS/$preset-test.log" | tail -1)"
        result "$name: сборка и тесты (${summary:-?})" "зелёное"
    else
        grep -E 'Failed|FAILED|Error|\*\*\*' "$LOGS/$preset-test.log" | head -30
        result "$name: сборка и тесты" "тесты красные — build/check/$preset-test.log"
    fi
done

echo
echo "Итог tools/check_cpp.sh ($(git rev-parse --short HEAD 2>/dev/null || echo '?')$(git diff --quiet 2>/dev/null || echo ', с незакоммиченными правками')):"
echo
echo "| проверка | итог |"
echo "|---|---|"
printf '%s\n' "${RESULTS[@]}"
exit "$FAILED"
