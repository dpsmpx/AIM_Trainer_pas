#!/bin/bash
# Окна оригинала и C++-версии без экрана: сборка, запуск, ввод и снимки
# (D-26) — для сценариев docs/MANUAL_CHECKS.md и сверки снимков с эталоном.
#
#   tools/gui.sh ref  build              собрать оригинал (archive/pascal) под Mono в build/ref
#   tools/gui.sh port build              собрать C++-версию, пресет linux-gcc
#   tools/gui.sh TARGET start            запустить в своём Xvfb
#   tools/gui.sh TARGET stop             закрыть программу и её Xvfb
#   tools/gui.sh TARGET clean            удалить файлы игрока
#   tools/gui.sh TARGET dir              папка файлов игрока
#   tools/gui.sh TARGET shot F.png       снимок окна 800×600
#   tools/gui.sh TARGET title            заголовок окна (_NET_WM_NAME; у port кириллица
#                                        до него не доходит — docs/ARCHITECTURE.md, §12)
#   tools/gui.sh TARGET key KEY...       клавиши: Escape, space, p, Return
#   tools/gui.sh TARGET move X Y         курсор в точку окна
#   tools/gui.sh TARGET click X Y [BTN]  щелчок: 1 — ЛКМ, 3 — ПКМ
#   tools/gui.sh compare A.png B.png [DIFF.png]  число несовпавших пикселей
#
# TARGET — ref (оригинал на PascalABC.NET, эталон) или port (C++-версия).
# Координаты — внутри окна, как в коде программы. У каждой цели свой
# виртуальный экран (ref — :97, port — :96): окна на общем легли бы друг на
# друга, и щелчок достался бы верхнему.
#
# Оригинал пишет файлы профиля в текущий каталог (AUD-17), поэтому работает
# в build/ref с копиями исходников из archive/pascal: файлы репозитория он
# не трогает (D-04, D-09, D-28). Сборка начинает с чистого каталога — как «Подготовка» сценариев.
# C++-версия пишет в build/gui/port/data (AIM_TRAINER_DATA_DIR, D-30), а не
# в папку пользователя сессии; своя AIM_TRAINER_DATA_DIR — главнее.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_DIR="${AIM_TOOLS_DIR:-$HOME/.local/share/aim-trainer-tools}"
# shellcheck source=/dev/null
[ -f "$TOOLS_DIR/env.sh" ] && . "$TOOLS_DIR/env.sh"
PABCNET_DIR="${PABCNET_DIR:-$TOOLS_DIR/pascalabcnet}"
REF_DIR="$ROOT/build/ref"
PORT_BIN="$ROOT/build/linux-gcc/AimTrainer"
TITLE="Aim Trainer"
# Шаблоны файлов игрока — для find -name: раскрывать их здесь, в текущем
# каталоге, нельзя.
# Обе программы: Pascal-версия — profileN_stats.ini и _sessions.csv,
# C++-версия — profileN.ini и profileN_runs.csv (D-38).
PLAYER_FILES=('profile*.ini' 'profile*_sessions.csv' 'profile*_runs.csv'
              'profile*_corrupt_backup.txt' 'export_profile*.csv' 'aim_trainer.ini')

die() { echo "gui.sh: $*" >&2; exit 1; }

usage() { sed -n '2,18p' "$0"; exit 2; }

# --- цели -------------------------------------------------------------------

build_ref() {
    local compiler="$PABCNET_DIR/bin/pabcnetc.exe"
    [ -f "$compiler" ] || die "нет $compiler — сначала tools/setup_env.sh"
    rm -rf "$REF_DIR"
    mkdir -p "$REF_DIR"
    cp "$ROOT"/archive/pascal/*.pas "$REF_DIR"/
    # Строки с номерами шагов ([12]BeginParsingFile...) — шум, остальное видно.
    { (cd "$REF_DIR" && mono "$compiler" AIM_Trainer.pas) | grep -v '^\[' >&2; } || true
    [ -f "$REF_DIR/AIM_Trainer.exe" ] || die "оригинал не собрался"
    echo "$REF_DIR/AIM_Trainer.exe"
}

build_port() {
    (cd "$ROOT" && cmake --preset linux-gcc >/dev/null && cmake --build --preset linux-gcc) >&2
    [ -x "$PORT_BIN" ] || die "C++-версия не собралась"
    echo "$PORT_BIN"
}

launch() {
    case "$TARGET" in
        ref)
            [ -f "$REF_DIR/AIM_Trainer.exe" ] || build_ref >/dev/null
            cd "$REF_DIR" && exec mono AIM_Trainer.exe ;;
        port)
            [ -x "$PORT_BIN" ] || build_port >/dev/null
            mkdir -p "$PORT_DATA"
            # Программный кадровый буфер: Xvfb всё равно рисует без GPU.
            # Звуковой карты в сессии нет: звук — в драйвер dummy, без шума
            # ALSA в журнале.
            AIM_TRAINER_DATA_DIR="$PORT_DATA" SDL_FRAMEBUFFER_ACCELERATION=0 \
                SDL_AUDIO_DRIVER="${SDL_AUDIO_DRIVER:-dummy}" exec "$PORT_BIN" ;;
    esac
}

# --- экран и окно -----------------------------------------------------------

ensure_display() {
    export DISPLAY="$TARGET_DISPLAY"
    if ! xdotool getmouselocation >/dev/null 2>&1; then
        setsid Xvfb "$DISPLAY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 &
        echo $! >"$STATE/xvfb.pid"
        for _ in $(seq 50); do
            xdotool getmouselocation >/dev/null 2>&1 && break
            sleep 0.1
        done
    fi
}

# Номер окна. Звать как wid="$(window_id)": тогда set -e остановит скрипт,
# если окна нет, — import с пустым номером ждал бы щелчка мышью.
window_id() {
    local wid
    wid="$(xdotool search --name "^$TITLE" 2>/dev/null | head -1 || true)"
    [ -n "$wid" ] || die "окно $TARGET не найдено: tools/gui.sh $TARGET start"
    echo "$wid"
}

# --- команды ----------------------------------------------------------------

cmd_start() {
    ensure_display
    # Своя сессия: сигналы Mono не задевают вызвавшую оболочку. Скрипт
    # запускает сам себя командой _launch, она заменяется программой — номер
    # процесса в app.pid и есть программа.
    setsid "$0" "$TARGET" _launch >"$STATE/run.log" 2>&1 &
    echo $! >"$STATE/app.pid"
    for _ in $(seq 100); do
        xdotool search --name "^$TITLE" >/dev/null 2>&1 && break
        sleep 0.1
    done
    local wid
    wid="$(window_id)"
    # Окно появляется раньше, чем нарисован первый кадр.
    sleep 1.5
    xdotool getwindowname "$wid" >/dev/null 2>&1 ||
        die "$TARGET закрылся сразу после запуска — $STATE/run.log"
    echo "$TARGET запущен на $DISPLAY, окно $wid"
}

cmd_stop() {
    local f
    for f in app xvfb; do
        if [ -f "$STATE/$f.pid" ]; then
            kill "$(cat "$STATE/$f.pid")" 2>/dev/null || true
            rm -f "$STATE/$f.pid"
        fi
    done
}

player_dir() {
    if [ "$TARGET" = ref ]; then echo "$REF_DIR"; else echo "$PORT_DATA"; fi
}

cmd_clean() {
    local dir
    dir="$(player_dir)"
    [ -d "$dir" ] || return 0
    # aim_trainer.ini — номер последнего профиля у C++-версии (D-30).
    local args=() p
    for p in "${PLAYER_FILES[@]}"; do args+=(-o -name "$p"); done
    find "$dir" -maxdepth 1 -type f \( "${args[@]:1}" \) -delete
}

cmd_shot() {
    [ $# -eq 1 ] || die "shot FILE.png"
    local wid
    wid="$(window_id)"
    import -window "$wid" "$1"
}

cmd_key() {
    local wid k
    wid="$(window_id)"
    # Фокус окну и нажатие через XTEST: синтетические события XSendEvent
    # WinForms под Mono может не принять.
    xdotool windowfocus "$wid" 2>/dev/null || true
    for k in "$@"; do
        xdotool key "$k"
        sleep 0.15
    done
}

cmd_move() {
    [ $# -eq 2 ] || die "move X Y"
    local wid
    wid="$(window_id)"
    xdotool mousemove --window "$wid" "$1" "$2"
    sleep 0.05
}

cmd_click() {
    [ $# -ge 2 ] || die "click X Y [BTN]"
    local wid
    wid="$(window_id)"
    xdotool mousemove --window "$wid" "$1" "$2" sleep 0.05 click "${3:-1}"
    sleep 0.15
}

cmd_compare() {
    [ $# -ge 2 ] || die "compare A.png B.png [DIFF.png]"
    # AE — сколько пикселей отличается; 0 — снимки совпали.
    local n
    n="$(compare -metric AE "$1" "$2" "${3:-null:}" 2>&1 >/dev/null || true)"
    echo "$n"
    [ "$n" = "0" ]
}

# --- разбор -----------------------------------------------------------------

[ $# -ge 1 ] || usage
if [ "$1" = "compare" ]; then
    shift
    cmd_compare "$@"
    exit $?
fi

TARGET="$1"
shift || true
case "$TARGET" in
    ref) TARGET_DISPLAY="${AIM_REF_DISPLAY:-:97}" ;;
    port) TARGET_DISPLAY="${AIM_PORT_DISPLAY:-:96}" ;;
    *) usage ;;
esac
STATE="$ROOT/build/gui/$TARGET"
mkdir -p "$STATE"
PORT_DATA="${AIM_TRAINER_DATA_DIR:-$ROOT/build/gui/port/data}"
export DISPLAY="$TARGET_DISPLAY"

cmd="${1:-}"
shift || true
case "$cmd" in
    build) if [ "$TARGET" = ref ]; then build_ref; else build_port; fi ;;
    start) cmd_start ;;
    _launch) launch ;;
    stop) cmd_stop ;;
    clean) cmd_clean ;;
    dir) player_dir ;;
    shot) cmd_shot "$@" ;;
    title) wid="$(window_id)" && xdotool getwindowname "$wid" ;;
    key) cmd_key "$@" ;;
    move) cmd_move "$@" ;;
    click) cmd_click "$@" ;;
    *) usage ;;
esac
