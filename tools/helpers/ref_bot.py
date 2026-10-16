"""Игрок для сценариев docs/MANUAL_CHECKS.md на окне без экрана (D-26, D-27).

Снимает окно Aim Trainer (ImageMagick `import`), находит цели и бомбы по
цвету и щёлкает или наводит курсор через xdotool. Окно запускает
tools/gui.sh (ref — оригинал под Mono, port — C++-версия).

    python3 tools/helpers/ref_bot.py find СНИМОК.png
    python3 tools/helpers/ref_bot.py play DISPLAY ПРАВИЛО СЕКУНД [ДЕЙСТВИЙ] [КАТАЛОГ]

find печатает пятна «вид cx cy площадь r g b». play ведёт игру СЕКУНД секунд
или до ДЕЙСТВИЙ щелчков и печатает каждое действие; КАТАЛОГ — куда сложить
все снимки. DISPLAY — :97 у ref, :96 у port (tools/gui.sh).

ПРАВИЛО:
  lmb        ЛКМ по цели в стороне от центра (точность ниже 95 %: точное
             попадание роняет оригинал — эффект 4, сценарий 22)
  center     ЛКМ точно в центр цели
  dual       синие — ЛКМ, красные — ПКМ (Dual), в стороне от центра
  dualwrong  синие — ПКМ, красные — ЛКМ
  hover      держать курсор на цели (Tracking), не щёлкая
  bomb       ЛКМ по бомбе

Цель — пятно насыщенного цвета выше HUD; бомба — тёмно-красный круг с белым
крестом. Анимацию попадания («+15», частицы) около последнего щелчка бот
0,7 с не трогает.
"""
import math
import os
import subprocess
import sys
import tempfile
import time

import numpy as np
from PIL import Image

HUD_TOP = 565  # HUD внизу окна: в нём цели не ищутся


def _components(mask, min_area):
    """Связные пятна маски: списки точек (y, x)."""
    h, w = mask.shape
    seen = np.zeros_like(mask)
    ys, xs = np.nonzero(mask)
    for y0, x0 in zip(ys, xs):
        if seen[y0, x0]:
            continue
        stack = [(y0, x0)]
        seen[y0, x0] = True
        pts = []
        while stack:
            y, x = stack.pop()
            pts.append((y, x))
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                ny, nx = y + dy, x + dx
                if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not seen[ny, nx]:
                    seen[ny, nx] = True
                    stack.append((ny, nx))
        if len(pts) >= min_area:
            yield np.array(pts)


def find(path):
    """Пятна на снимке: (вид, cx, cy, площадь, r, g, b), крупные первыми."""
    img = np.asarray(Image.open(path).convert("RGB")).astype(int)[:HUD_TOP]
    hi, lo = img.max(axis=2), img.min(axis=2)
    r, g = img[:, :, 0], img[:, :, 1]
    out = []
    for pts in _components((hi - lo > 60) & (hi > 90), 12):
        cy, cx = pts.mean(axis=0)
        col = img[pts[:, 0], pts[:, 1]].mean(axis=0)
        kind = "blue" if col[2] > col[0] + 40 else "red" if col[0] > col[2] + 40 else "other"
        out.append((kind, int(cx), int(cy), len(pts), *map(int, col)))
    for pts in _components((r - g >= 12) & (hi < 80), 60):
        cy, cx = pts.mean(axis=0)
        col = img[pts[:, 0], pts[:, 1]].mean(axis=0)
        out.append(("bomb", int(cx), int(cy), len(pts), *map(int, col)))
    return sorted(out, key=lambda b: -b[3])


def play(display, policy, seconds, max_actions, shots_dir):
    env = dict(os.environ, DISPLAY=display)
    wid = subprocess.run(["xdotool", "search", "--name", "^Aim Trainer"], env=env,
                         capture_output=True, text=True, check=True).stdout.split()[0]
    shot = os.path.join(tempfile.mkdtemp(prefix="aim_bot_"), "shot.png")

    def act(x, y, btn=None):
        cmd = ["xdotool", "mousemove", "--window", wid, str(x), str(y)]
        if btn:
            cmd += ["click", str(btn)]
        subprocess.run(cmd, env=env, check=True)

    start = time.time()
    actions = n = 0
    last, last_t = None, 0.0
    while time.time() - start < seconds and actions < max_actions:
        # Окно пропало (программа упала или закрылась) — import ждал бы вечно.
        if subprocess.run(["xdotool", "getwindowname", wid], env=env,
                          capture_output=True).returncode != 0:
            print(round(time.time() - start, 2), "окна нет", flush=True)
            return 1
        subprocess.run(["import", "-window", wid, shot], env=env, timeout=5)
        n += 1
        if shots_dir:
            subprocess.run(["cp", shot, os.path.join(shots_dir, f"b{n:04d}.png")])
        found = find(shot)
        if last and time.time() - last_t < 0.7:
            found = [f for f in found
                     if abs(f[1] - last[1]) > 60 or abs(f[2] - last[2]) > 60]
        t = round(time.time() - start, 2)
        bombs = [f for f in found if f[0] == "bomb"]
        targets = [f for f in found if f[0] in ("blue", "red")]
        if policy == "bomb":
            pick, btn, off = (bombs[0] if bombs else None), 1, 0.0
        else:
            pick = targets[0] if targets else None
            off = 0.0 if policy in ("center", "hover") else 0.6
            kind = pick[0] if pick else None
            btn = {"lmb": 1, "center": 1, "hover": None,
                   "dual": 1 if kind == "blue" else 3,
                   "dualwrong": 3 if kind == "blue" else 1}.get(policy, "?")
            if btn == "?":
                sys.exit("неизвестное правило: " + policy)
        if pick is None:
            time.sleep(0.03)
            continue
        radius = math.sqrt(pick[3] / math.pi)
        x, y = pick[1] + int(off * radius), pick[2]
        if policy == "hover":
            act(x, y)
            print(t, "навёл", pick[:4], flush=True)
            continue
        act(x, y, btn)
        last, last_t = pick, time.time()
        actions += 1
        print(t, "щелчок", btn, (x, y), pick, flush=True)
        time.sleep(0.12)
    return 0


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "find":
        for b in find(sys.argv[2]):
            print(*b)
    elif len(sys.argv) >= 5 and sys.argv[1] == "play":
        sys.exit(play(sys.argv[2], sys.argv[3], float(sys.argv[4]),
                      int(sys.argv[5]) if len(sys.argv) > 5 else 10**9,
                      sys.argv[6] if len(sys.argv) > 6 else None))
    else:
        sys.exit(__doc__)
