#!/bin/bash
# Покрытие строк своего кода тестами (AUD-49): пресет linux-coverage (GCC с
# --coverage), все тесты, затем gcov — доля исполненных строк по слоям
# src/core, src/platform, src/app (D-27).
#
#   tools/helpers/coverage.sh          собрать, прогнать тесты, напечатать таблицу
#   tools/helpers/coverage.sh --files  ещё и каждый файл
#
# Цель аудита: src/core — от 90 %, src/app — от 60 %.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="$ROOT/build/linux-coverage"
FILES=0
[ "${1:-}" = "--files" ] && FILES=1

cd "$ROOT"
cmake --preset linux-coverage >/dev/null
cmake --build --preset linux-coverage >/dev/null
find "$BUILD" -name '*.gcda' -delete
env -u DISPLAY -u WAYLAND_DISPLAY ctest --preset linux-coverage >/dev/null || {
    echo "тесты красные — покрытие не считается" >&2
    exit 1
}

python3 - "$ROOT" "$BUILD" "$FILES" <<'EOF'
import json, os, subprocess, sys
from collections import defaultdict

root, build, files = sys.argv[1], sys.argv[2], sys.argv[3] == "1"
lines = defaultdict(dict)  # файл -> строка -> исполнена ли
for dirpath, _, names in os.walk(build):
    if "_deps" in dirpath:
        continue
    for name in names:
        if not name.endswith(".gcda"):
            continue
        out = subprocess.run(["gcov", "--json-format", "--stdout", os.path.join(dirpath, name)],
                             capture_output=True, cwd=build).stdout
        for chunk in out.splitlines():
            if not chunk.strip():
                continue
            data = json.loads(chunk)
            for f in data.get("files", []):
                path = os.path.normpath(os.path.join(data.get("current_working_directory", build),
                                                     f["file"]))
                rel = os.path.relpath(path, root)
                if not rel.startswith("src/"):
                    continue
                for l in f["lines"]:
                    n = l["line_number"]
                    lines[rel][n] = lines[rel].get(n, False) or l["count"] > 0

layers = defaultdict(lambda: [0, 0])
for rel, ls in sorted(lines.items()):
    hit, total = sum(ls.values()), len(ls)
    layer = "/".join(rel.split("/")[:2])
    layers[layer][0] += hit
    layers[layer][1] += total
    if files:
        print(f"{100 * hit / total:5.1f} %  {hit:5}/{total:<5} {rel}")
print("| слой | строк исполнено | покрытие |")
print("|---|---|---|")
for layer, (hit, total) in sorted(layers.items()):
    print(f"| {layer} | {hit} из {total} | {100 * hit / total:.1f} % |")
EOF
