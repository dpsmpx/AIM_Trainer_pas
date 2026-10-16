#!/bin/bash
# Установка инструментов C++-версии (D-17, D-18, D-19, D-25) и эталона
# (D-26) на Ubuntu 24.04: в облачной сессии её вызывает хук начала сессии,
# на своей машине — человек.
#
#   tools/setup_env.sh [--skip-apt] [--skip-ref]
#
# Ставит:
#
#   1. Системные пакеты: GCC и Clang 18, CMake, Ninja, GoogleTest 1.14.0,
#      clang-format и clang-tidy 18, ccache, shellcheck; заголовки, с которыми
#      собирается SDL3 (X11, Wayland, звук, ввод); Xvfb и xdotool — окно без
#      экрана; Wine — тесты Windows-сборки; Mono 6.8 с WinForms, libgdiplus,
#      .NET SDK 10 и ImageMagick — эталон и его снимки.
#   2. Набор llvm-mingw 20261006 (D-17) — кросс-сборка под Windows.
#   3. Архив SDL 3.4.18 (D-18) — чтобы каждый пресет не качал его заново.
#   4. Эталон (D-26): компилятор PascalABC.NET из исходников на закреплённом
#      коммите с заплаткой для Mono 6.8 и его стандартные модули (GraphABC).
#      Сбой здесь установку не валит: C++-версии эталон не нужен.
#
# Оба архива сверяются по SHA-256 из решений; несовпадение — остановка.
# Куда: $AIM_TOOLS_DIR, по умолчанию ~/.local/share/aim-trainer-tools; там же
# env.sh с путями — его подключают tools/check_cpp.sh, tools/gui.sh и хук.
# Повторный запуск ничего не качает и не собирает, если всё на месте.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_DIR="${AIM_TOOLS_DIR:-$HOME/.local/share/aim-trainer-tools}"
DEPS_DIR="$TOOLS_DIR/deps"

LLVM_MINGW_NAME="llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64"
LLVM_MINGW_URL="https://github.com/mstorsjo/llvm-mingw/releases/download/20261006/$LLVM_MINGW_NAME.tar.xz"
LLVM_MINGW_SHA256="5f9c6ed95b2d4bdb2869a488c5fd5857fbdabcf288a0aa3eb1da43f6a08d8ab4"
SDL3_ARCHIVE="SDL3-3.4.18.tar.gz"
SDL3_URL="https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/$SDL3_ARCHIVE"
SDL3_SHA256="9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3"

# Эталон (D-26): PascalABC.NET 4.0.0.3882. Заплатка проверена на этом коммите,
# более свежий может с ней разойтись.
PABCNET_URL="https://github.com/pascalabcnet/pascalabcnet"
PABCNET_REV="643514c2d9b843ffb4971345b117dd2f6df6369c"
PABCNET_DIR="$TOOLS_DIR/pascalabcnet"
PABCNET_PATCH="$ROOT/tools/pabcnet/mono68-generic-array-ctors.patch"
export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1

APT_PACKAGES=(
    # Сборка, тесты, анализ
    build-essential cmake ninja-build pkg-config ccache git curl xz-utils python3
    clang clang-format clang-tidy shellcheck libgtest-dev libgmock-dev googletest
    # SDL3: заголовки; сами библиотеки X11, Wayland и звука SDL грузит при запуске
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev
    libxtst-dev libxfixes-dev libxinerama-dev libxkbcommon-dev libwayland-dev
    wayland-protocols libdecor-0-dev libegl-dev libgl-dev libgles-dev libdrm-dev
    libgbm-dev libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev
    libudev-dev libibus-1.0-dev
    # Окно без экрана и тесты Windows-сборки
    xvfb xdotool wine wine64
    # Эталон (D-26): оригинал под Mono, WinForms поверх libgdiplus; компилятор
    # собирается .NET SDK; снимки окон — ImageMagick; Consolas под Mono
    # подменяется DejaVu Sans Mono (D-22)
    mono-complete libgdiplus dotnet-sdk-10.0 imagemagick fonts-dejavu-core
)

SKIP_APT=0
SKIP_REF=0
for arg in "$@"; do
    case "$arg" in
        --skip-apt) SKIP_APT=1 ;;
        --skip-ref) SKIP_REF=1 ;;
        -h|--help) sed -n '2,25p' "$0"; exit 0 ;;
        *) echo "неизвестный ключ: $arg" >&2; exit 2 ;;
    esac
done

log() { printf '[setup_env] %s\n' "$*" >&2; }

as_root() {
    if [ "$(id -u)" -eq 0 ]; then "$@"; else sudo "$@"; fi
}

install_packages() {
    local missing=() p
    for p in "${APT_PACKAGES[@]}"; do
        dpkg-query -W -f='${Status}' "$p" 2>/dev/null | grep -q "install ok installed" ||
            missing+=("$p")
    done
    if [ "${#missing[@]}" -eq 0 ]; then
        log "системные пакеты уже стоят"
        return
    fi
    log "ставлю пакеты: ${missing[*]}"
    as_root apt-get update -q
    as_root env DEBIAN_FRONTEND=noninteractive \
        apt-get install -y -q --no-install-recommends "${missing[@]}"
}

# fetch URL ФАЙЛ SHA256 — скачать, если файла нет, и сверить сумму.
fetch() {
    local url="$1" file="$2" sum="$3"
    if [ ! -f "$file" ]; then
        log "качаю $(basename "$file")"
        curl -fsSL --retry 4 --retry-delay 2 -o "$file.part" "$url"
        mv "$file.part" "$file"
    fi
    if ! echo "$sum  $file" | sha256sum -c --status; then
        log "SHA-256 не совпал: $file — удаляю; проверьте источник и решение"
        rm -f "$file"
        exit 1
    fi
}

install_llvm_mingw() {
    local dir="$TOOLS_DIR/$LLVM_MINGW_NAME"
    if [ -x "$dir/bin/x86_64-w64-mingw32-clang++" ]; then
        log "llvm-mingw уже стоит: $dir"
        return
    fi
    fetch "$LLVM_MINGW_URL" "$DEPS_DIR/$LLVM_MINGW_NAME.tar.xz" "$LLVM_MINGW_SHA256"
    log "распаковываю llvm-mingw в $TOOLS_DIR"
    tar -xJf "$DEPS_DIR/$LLVM_MINGW_NAME.tar.xz" -C "$TOOLS_DIR"
    rm -f "$DEPS_DIR/$LLVM_MINGW_NAME.tar.xz"
}

# Отпечаток сборки эталона: коммит и заплатка. Совпал — собирать незачем.
pabcnet_stamp() {
    printf '%s %s\n' "$PABCNET_REV" "$(sha256sum "$PABCNET_PATCH" | cut -d' ' -f1)"
}

# Компилятор PascalABC.NET из исходников (D-26), как у TrueJointTileMaker:
# dotnet собирает его под .NET Framework 4.7.2, запускается он под Mono.
build_pabcnet() {
    local stamp_file="$PABCNET_DIR/.aim-stamp"
    if [ -f "$PABCNET_DIR/bin/pabcnetc.exe" ] && [ -f "$PABCNET_DIR/bin/Lib/GraphABC.pcu" ] &&
       [ "$(cat "$stamp_file" 2>/dev/null)" = "$(pabcnet_stamp)" ]; then
        log "PascalABC.NET уже собран: $PABCNET_DIR"
        return
    fi
    # Функция зовётся из if: set -e в ней не действует, поэтому каждый шаг
    # проверяется сам.
    log "получаю PascalABC.NET ${PABCNET_REV:0:12}"
    mkdir -p "$PABCNET_DIR" || return 1
    if [ ! -d "$PABCNET_DIR/.git" ]; then
        git -C "$PABCNET_DIR" init -q &&
            git -C "$PABCNET_DIR" remote add origin "$PABCNET_URL" || return 1
    fi
    GIT_LFS_SKIP_SMUDGE=1 git -C "$PABCNET_DIR" fetch -q --depth 1 origin "$PABCNET_REV" &&
        git -C "$PABCNET_DIR" checkout -q --force FETCH_HEAD &&
        git -C "$PABCNET_DIR" clean -q -fdx &&
        git -C "$PABCNET_DIR" apply "$PABCNET_PATCH" || return 1

    log "собираю компилятор (dotnet, около минуты)"
    (
        cd "$PABCNET_DIR" &&
            dotnet build -c Release --no-incremental pabcnetc.sln \
                -p:PABCNET_LEGACY_ONLY=true -v:q -nologo -clp:ErrorsOnly
    ) || return 1
    log "собираю стандартные модули: PABCSystem, GraphABC и другие"
    (
        cd "$PABCNET_DIR/ReleaseGenerators" &&
            mono ../bin/pabcnetc.exe RebuildStandartModulesMono.pas /rebuild >/dev/null
    ) || return 1
    [ -f "$PABCNET_DIR/bin/Lib/GraphABC.pcu" ] || { log "модули не собрались"; return 1; }
    pabcnet_stamp >"$stamp_file"
    log "PascalABC.NET готов"
}

write_env() {
    cat >"$TOOLS_DIR/env.sh" <<EOF
# Создано tools/setup_env.sh: пути к инструментам C++-версии Aim Trainer.
export AIM_TOOLS_DIR="$TOOLS_DIR"
export AIM_DEPS_DIR="$DEPS_DIR"
export LLVM_MINGW="$TOOLS_DIR/$LLVM_MINGW_NAME"
export PABCNET_DIR="$PABCNET_DIR"
export DOTNET_CLI_TELEMETRY_OPTOUT=1
export DOTNET_NOLOGO=1
EOF
    log "пути — в $TOOLS_DIR/env.sh"
}

mkdir -p "$TOOLS_DIR" "$DEPS_DIR"
[ "$SKIP_APT" -eq 1 ] || install_packages
install_llvm_mingw
fetch "$SDL3_URL" "$DEPS_DIR/$SDL3_ARCHIVE" "$SDL3_SHA256"
write_env
if [ "$SKIP_REF" -eq 0 ] && ! build_pabcnet; then
    log "эталон не собран — tools/gui.sh ref не заработает; C++-версии он не нужен"
fi
log "готово"
