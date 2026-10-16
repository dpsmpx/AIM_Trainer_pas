#!/bin/bash
# Замер стоимости кадра меню (аудит C++-версии, AUD-39; D-27): сборка
# RelWithDebInfo в build/prof, КАДРОВ кадров меню под Callgrind на рендере
# SDL software без экрана; итог — инструкции своего кода и число вызовов
# на кадр.
#
#   tools/helpers/profile_menu.sh [КАДРОВ]     по умолчанию 120
#
# Нужны valgrind и python3. Растеризация рендера software в итог не входит:
# считается то, что программа делает сама, — это сравнимо между версиями
# кода, но не время кадра на Windows. Файлы игрока — во временной папке:
# профиль с пройденным туториалом, чтобы мерилось меню.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
FRAMES="${1:-120}"
OUT="$ROOT/build/prof"
command -v valgrind >/dev/null || { echo "нет valgrind" >&2; exit 1; }

cmake -S "$ROOT" -B "$OUT" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DBUILD_TESTING=OFF >/dev/null
cmake --build "$OUT" --target AimTrainer >/dev/null

WORK="$(mktemp -d)"
trap 'rm -rf "${WORK:?}"' EXIT
printf '# DP AIM Trainer profile v1\ntutorial_seen=1\n' > "$WORK/profile1.ini"
(cd "$WORK" && AIM_TRAINER_DATA_DIR="$WORK" SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software \
    SDL_AUDIO_DRIVER=dummy valgrind --tool=callgrind --callgrind-out-file="$WORK/menu.cg" \
    "$OUT/AimTrainer" --frames "$FRAMES" >/dev/null 2>&1)

callgrind_annotate --inclusive=yes "$WORK/menu.cg" > "$WORK/inclusive.txt" 2>/dev/null
python3 -I - "$WORK/menu.cg" "$WORK/inclusive.txt" "$FRAMES" <<'EOF'
import collections
import re
import sys

raw, inclusive, frames = sys.argv[1], sys.argv[2], int(sys.argv[3])

# Инклюзивная стоимость функций своего кода — из callgrind_annotate.
cost = {}
for line in open(inclusive, encoding="utf-8", errors="replace"):
    m = re.match(r"^\s*([\d,]+) \([^)]*\)\s+\S*src/app/[^:\s]+:(\S+?)\(", line)
    if m and m.group(2) not in cost:
        cost[m.group(2)] = int(m.group(1).replace(",", ""))

# Число вызовов — из сырого файла: строки calls= после cfn=.
names, calls, callee = {}, collections.Counter(), None
for line in open(raw, encoding="utf-8", errors="replace"):
    m = re.match(r"^(fn|cfn)=\((\d+)\)(?: (.*))?", line)
    if m:
        kind, ident, name = m.groups()
        if name:
            names[ident] = name.strip()
        if kind == "cfn":
            callee = ident
        continue
    m = re.match(r"^calls=(\d+)", line)
    if m and callee:
        calls[names.get(callee, callee)] += int(m.group(1))

def per_frame(value):
    return f"{value / frames:,.0f}".replace(",", " ")

print(f"кадров: {frames}")
for fn in ("aim::app::Screens::render", "aim::app::MenuView::render_particles"):
    print(f"{fn}: инструкций на кадр — {per_frame(cost.get(fn, 0))}")
for label, pattern in (("SDL_RenderGeometry", "SDL_RenderGeometry_REAL"),
                       ("TTF_DrawRendererText", "TTF_DrawRendererText"),
                       ("TTF_CreateText (всего)", "TTF_CreateText"),
                       ("operator new", "operator new(unsigned long)")):
    total = sum(v for k, v in calls.items() if k.startswith(pattern))
    shown = str(total) if "всего" in label else per_frame(total)
    print(f"{label}: {'вызовов' if 'всего' in label else 'на кадр'} — {shown}")
EOF
