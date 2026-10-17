#!/bin/bash
# Выкладка сборки программы под Windows в ветку win_debug_builds (D-23,
# D-34) — только по запросу автора (D-29):
#
#   tools/publish_build.sh
#
# Собирает пресет windows-cross (Release, всё статически) из закоммиченного
# кода и кладёт в ветку AimTrainer/: AimTrainer.exe, fonts/ (шрифт и его
# лицензия, D-31), README.md, SHA256SUMS и BUILD.md — из какого коммита и
# чем собрано. Корневой BUILD.md ветки — оглавление сборок. Новая сборка —
# новый коммит в ветке. Строки в конец сообщения коммита — $PUBLISH_TRAILERS.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="${AIM_TOOLS_DIR:-$HOME/.local/share/aim-trainer-tools}/env.sh"
# shellcheck source=/dev/null
[ -f "$ENV_FILE" ] && source "$ENV_FILE"
: "${LLVM_MINGW:?нет LLVM_MINGW — сначала tools/setup_env.sh}"
BRANCH="win_debug_builds"
WT="$ROOT/build/$BRANCH"
OUT="$ROOT/build/windows-cross"
TEMPLATES="$ROOT/tools/publish"
CODE=(src CMakeLists.txt CMakePresets.json tools/cmake)

if ! git -C "$ROOT" diff --quiet HEAD -- "${CODE[@]}" ||
   [ -n "$(git -C "$ROOT" ls-files --others --exclude-standard -- "${CODE[@]}")" ]; then
    echo "в коде есть незакоммиченные правки: сборка — только из коммита" >&2
    exit 1
fi
REV="$(git -C "$ROOT" rev-parse HEAD)"
CODE_REV="$(git -C "$ROOT" log -1 --format=%H -- "${CODE[@]}")"
SOURCE_BRANCH="$(git -C "$ROOT" rev-parse --abbrev-ref HEAD)"
# Версия в программе — та же строка, что даёт настройка CMake (D-49).
VERSION="$(git -C "$ROOT" describe --tags --always --dirty)"

(cd "$ROOT" && cmake --preset windows-cross >/dev/null && cmake --build --preset windows-cross) >&2
[ -f "$OUT/AimTrainer.exe" ] || { echo "нет $OUT/AimTrainer.exe" >&2; exit 1; }

push() {  # повтор при сбое сети: 2, 4, 8, 16 с
    local delay=2
    for _ in 1 2 3 4 5; do
        git -C "$WT" push -u origin "$BRANCH" && return 0
        sleep "$delay"
        delay=$((delay * 2))
    done
    return 1
}

git -C "$ROOT" fetch -q origin "+refs/heads/$BRANCH:refs/remotes/origin/$BRANCH" 2>/dev/null || true
git -C "$ROOT" worktree remove --force "$WT" 2>/dev/null || rm -rf "${WT:?}"
git -C "$ROOT" worktree prune
if git -C "$ROOT" rev-parse -q --verify "refs/remotes/origin/$BRANCH" >/dev/null; then
    git -C "$ROOT" worktree add -q -B "$BRANCH" "$WT" "origin/$BRANCH"
else
    git -C "$ROOT" branch -D "$BRANCH" 2>/dev/null || true
    git -C "$ROOT" worktree add -q --orphan -b "$BRANCH" "$WT"
fi

DEST="$WT/AimTrainer"
rm -rf "${DEST:?}"
mkdir -p "$DEST/fonts"
cp "$OUT/AimTrainer.exe" "$DEST/"
cp "$OUT"/fonts/* "$DEST/fonts/"
(cd "$DEST" && sha256sum -- AimTrainer.exe fonts/* > SHA256SUMS)
cp "$TEMPLATES/AimTrainer-README.md" "$DEST/README.md"
cp "$TEMPLATES/BUILD.md" "$WT/BUILD.md"
# Выкладки до D-34 держали описание замерщиков в корневом BUILD.md.
if [ -d "$WT/AimProbes" ] && [ ! -f "$WT/AimProbes/BUILD.md" ]; then
    git -C "$WT" show "HEAD:BUILD.md" > "$WT/AimProbes/BUILD.md"
fi

CLANG="$("$LLVM_MINGW/bin/x86_64-w64-mingw32-clang" --version | head -1)"
{
    echo "# Сборка DP AIM Trainer"
    echo
    echo "- Версия в программе («Справка», свойства EXE): \`$VERSION\`."
    echo "- Исходники: коммит \`$REV\` ветки \`$SOURCE_BRANCH\` репозитория"
    echo "  \`dpsmpx/AIM_Trainer_pas\`; последняя правка кода — \`$CODE_REV\`."
    echo "- Собрано: $(date -u '+%Y-%m-%d %H:%M UTC'), \`tools/publish_build.sh\`,"
    echo "  пресет \`windows-cross\` (Release)."
    echo "- Набор: llvm-mingw 20261006 (D-17) — $CLANG; SDL 3.4.18, SDL3_ttf"
    echo "  3.2.2, FreeType 2.15.0 — статически (D-18, D-31): EXE нужны только"
    echo "  системные DLL Windows. Шрифты DejaVu Sans Condensed и Sans Mono 2.37 —"
    echo "  \`fonts/\` со своей лицензией."
    echo "- Суммы SHA-256 — \`SHA256SUMS\`."
} > "$DEST/BUILD.md"

git -C "$WT" add -A
if git -C "$WT" diff --cached --quiet; then
    echo "в $BRANCH нечего менять: сборка та же"
else
    MESSAGE="Программа: сборка из ${REV:0:12}

AimTrainer.exe (пресет windows-cross, Release) со шрифтом в fonts/ для
проверки у автора на Windows; порядок — AimTrainer/README.md. Ветка только
со сборками (D-23, D-34)."
    [ -n "${PUBLISH_TRAILERS:-}" ] && MESSAGE="$MESSAGE

$PUBLISH_TRAILERS"
    git -C "$WT" commit -q -m "$MESSAGE"
    push
fi
git -C "$WT" log --oneline -1
git -C "$ROOT" worktree remove --force "$WT"
