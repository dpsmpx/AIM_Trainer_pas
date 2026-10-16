#!/bin/bash
# Выкладка собранных замерщиков в ветку win_debug_builds (D-23):
#
#   tools/lib_probes/build.sh && tools/lib_probes/publish.sh
#
# Ветка без общей истории с main (orphan) и только со сборками: AimProbes/ —
# EXE, README.md, SHA256SUMS и BUILD.md — из какого коммита и чем собрано
# (D-23, D-34); программа — tools/publish_build.sh.
# Новая сборка — новый коммит в ветке. Сборка должна быть из закоммиченного
# кода: коммит записан в свойствах каждого EXE (probe.rc.in) и в BUILD.md.
# Строки в конец сообщения коммита (соавторство) — $PUBLISH_TRAILERS.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$ROOT/build/lib_probes/AimProbes"
BRANCH="win_debug_builds"
WT="$ROOT/build/$BRANCH"

[ -d "$SRC" ] || { echo "нет $SRC — сначала tools/lib_probes/build.sh" >&2; exit 1; }
if ! git -C "$ROOT" diff --quiet HEAD -- tools/lib_probes docs/LIB_PROBES.md; then
    echo "в tools/lib_probes или docs/LIB_PROBES.md есть незакоммиченные правки:" \
         "сначала коммит, потом build.sh и publish.sh" >&2
    exit 1
fi
REV="$(git -C "$ROOT" rev-parse HEAD)"
SOURCE_BRANCH="$(git -C "$ROOT" rev-parse --abbrev-ref HEAD)"
# Каждый EXE обязан быть собран из этого коммита (поле Comments в свойствах).
python3 - "$SRC" "${REV:0:12}" <<'EOF'
import pathlib, sys
src, rev = pathlib.Path(sys.argv[1]), sys.argv[2]
bad = [p.name for p in sorted(src.glob("*.exe")) if rev.encode("utf-16-le") not in p.read_bytes()]
if bad:
    sys.exit(f"собраны не из коммита {rev}: {', '.join(bad)} — перезапустить build.sh")
EOF

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
git -C "$ROOT" worktree remove --force "$WT" 2>/dev/null || rm -rf "$WT"
git -C "$ROOT" worktree prune
if git -C "$ROOT" rev-parse -q --verify "refs/remotes/origin/$BRANCH" >/dev/null; then
    git -C "$ROOT" worktree add -q -B "$BRANCH" "$WT" "origin/$BRANCH"
else
    git -C "$ROOT" branch -D "$BRANCH" 2>/dev/null || true
    git -C "$ROOT" worktree add -q --orphan -b "$BRANCH" "$WT"
fi

rm -rf "$WT/AimProbes"
cp -r "$SRC" "$WT/AimProbes"
(cd "$WT/AimProbes" && sha256sum -- *.exe > SHA256SUMS)
LLVM_MINGW="${LLVM_MINGW:-/opt/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64}"
CLANG="$("$LLVM_MINGW/bin/x86_64-w64-mingw32-clang" --version | head -1)"
# Корневой BUILD.md — оглавление сборок (D-34), описание замерщиков — в
# AimProbes/BUILD.md.
cp "$ROOT/tools/publish/BUILD.md" "$WT/BUILD.md"
cat > "$WT/AimProbes/BUILD.md" <<EOF
# Замерщики ввода

- Исходники: коммит \`$REV\` ветки \`$SOURCE_BRANCH\` репозитория
  \`dpsmpx/AIM_Trainer_pas\` — \`tools/lib_probes/\`.
- Собрано: $(date -u '+%Y-%m-%d %H:%M UTC'), \`tools/lib_probes/build.sh\`.
- Набор: llvm-mingw 20261006 (D-17) — $CLANG; всё статически, EXE нужны
  только системные DLL Windows.
- Как запустить и что прислать — \`AimProbes/README.md\`.
- Суммы SHA-256 — \`AimProbes/SHA256SUMS\`.
EOF

git -C "$WT" add -A
if git -C "$WT" diff --cached --quiet; then
    echo "в $BRANCH нечего менять: сборка та же"
else
    git -C "$WT" commit -q -F - <<EOF
Замерщики ввода: сборка из ${REV:0:12}

Восемь EXE замерщиков ввода (tools/lib_probes/) для прогона у автора на
Windows; порядок — AimProbes/README.md. Ветка только со сборками (D-23).
${PUBLISH_TRAILERS:+
$PUBLISH_TRAILERS}
EOF
    push
fi
git -C "$WT" log --oneline -1
git -C "$ROOT" worktree remove --force "$WT"
