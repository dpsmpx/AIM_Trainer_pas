#!/bin/bash
# Кросс-сборка замерщиков ввода под Windows (docs/LIB_PROBES.md):
#
#   tools/lib_probes/build.sh [библиотека …]   # по умолчанию — все
#
# Набор — llvm-mingw 20261006 (D-17): $LLVM_MINGW, по умолчанию
# /opt/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64. Каждая библиотека — свой
# каталог build/lib_probes/<библиотека>. Итог — build/lib_probes/AimProbes/ и
# архив build/lib_probes/AimProbes-win64.zip: все EXE и docs/LIB_PROBES.md.
#
# Исходники библиотек CMake качает сам и сверяет (SHA-256 архива или коммит).
# Уже скачанные и сверенные можно подложить без загрузки: $PROBE_SOURCES —
# каталог с sdl3-3.4.18, SDL2-2.32.10, glfw-3.5.1, raylib-6.0, SFML-3.1.0,
# allegro5-5.2.11.3.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
export LLVM_MINGW="${LLVM_MINGW:-/opt/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64}"
OUT="$ROOT/build/lib_probes"
mkdir -p "$OUT"
LIBS=("$@")
[ "${#LIBS[@]}" -gt 0 ] || LIBS=(SDL3 SDL2 GLFW raylib SFML Allegro WinAPI)

if [ ! -x "$LLVM_MINGW/bin/x86_64-w64-mingw32-clang++" ]; then
    echo "нет llvm-mingw в $LLVM_MINGW (D-17)" >&2
    exit 1
fi

source_dir() {  # имя FetchContent → подложенный каталог исходников
    local dir=""
    case "$1" in
        sdl3) dir="sdl3-3.4.18" ;;
        sdl2) dir="SDL2-2.32.10" ;;
        glfw) dir="glfw-3.5.1" ;;
        raylib) dir="raylib-6.0" ;;
        sfml) dir="SFML-3.1.0" ;;
        allegro) dir="allegro5-5.2.11.3" ;;
    esac
    if [ -n "${PROBE_SOURCES:-}" ] && [ -n "$dir" ] && [ -d "$PROBE_SOURCES/$dir" ]; then
        local name
        name="$(echo "$1" | tr '[:lower:]' '[:upper:]')"
        echo "-DFETCHCONTENT_SOURCE_DIR_${name}=$PROBE_SOURCES/$dir"
    fi
}

for lib in "${LIBS[@]}"; do
    dep="$(echo "$lib" | tr '[:upper:]' '[:lower:]')"
    args=()
    src="$(source_dir "$dep")"
    [ -z "$src" ] || args+=("$src")
    echo "== $lib"
    cmake -S "$ROOT/tools/lib_probes" -B "$OUT/$lib" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$ROOT/tools/lib_probes/llvm-mingw.cmake" \
        -DPROBE_LIB="$lib" "${args[@]}" >"$OUT-$lib-configure.log" 2>&1 ||
        { tail -30 "$OUT-$lib-configure.log"; exit 1; }
    cmake --build "$OUT/$lib" >"$OUT-$lib-build.log" 2>&1 ||
        { grep -E 'error|Error' "$OUT-$lib-build.log" | head -30; exit 1; }
done

rm -rf "$OUT/AimProbes"
mkdir -p "$OUT/AimProbes"
find "$OUT" -maxdepth 2 -name 'AimProbe_*.exe' -exec cp {} "$OUT/AimProbes/" \;
cp "$ROOT/docs/LIB_PROBES.md" "$OUT/AimProbes/README.md"
(cd "$OUT" && rm -f AimProbes-win64.zip && cmake -E tar cf AimProbes-win64.zip --format=zip AimProbes)
ls -l "$OUT/AimProbes"
sha256sum "$OUT/AimProbes"/*.exe
