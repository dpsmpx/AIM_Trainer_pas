"""Значок программы — прицел в цветах темы «Тёмная» (D-49, AUD-52).

Рисует круг с перекрестием и красной точкой в центре на тёмном
скруглённом квадрате и пишет src/app/aim_trainer.ico (16…256 px). Значок —
свой, лицензия программы (docs/PROVENANCE.md). Запуск из корня:

    python3 tools/helpers/make_icon.py

Нужен Pillow; рисуется крупно и уменьшается со сглаживанием, так что
файл от запуска к запуску одинаков.
"""
from pathlib import Path

from PIL import Image, ImageDraw

BACKGROUND = (28, 28, 32, 255)  # фон темы «Тёмная» (core/palette.h)
RING = (97, 207, 255, 255)      # акцент и цель для ЛКМ
CROSS = (255, 255, 255, 255)    # текст
DOT = (255, 80, 80, 255)        # цель для ПКМ
SIZES = [16, 24, 32, 48, 64, 128, 256]
BIG = 1024


def draw() -> Image.Image:
    img = Image.new("RGBA", (BIG, BIG), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    pad = BIG // 32
    d.rounded_rectangle((pad, pad, BIG - pad, BIG - pad), radius=BIG // 6, fill=BACKGROUND)
    c = BIG // 2
    r = int(BIG * 0.30)
    ring = BIG // 14
    d.ellipse((c - r, c - r, c + r, c + r), outline=RING, width=ring)
    # Перекрестие: штрихи от края круга внутрь и наружу, с просветом в центре.
    line = BIG // 20
    gap = int(BIG * 0.10)
    reach = int(BIG * 0.42)
    for sx, sy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        x1, y1 = c + sx * gap, c + sy * gap
        x2, y2 = c + sx * reach, c + sy * reach
        box = (min(x1, x2) - (line // 2 if sx == 0 else 0), min(y1, y2) - (line // 2 if sy == 0 else 0),
               max(x1, x2) + (line // 2 if sx == 0 else 0), max(y1, y2) + (line // 2 if sy == 0 else 0))
        d.rectangle(box, fill=CROSS)
    dot = BIG // 18
    d.ellipse((c - dot, c - dot, c + dot, c + dot), fill=DOT)
    return img


def main() -> None:
    root = Path(__file__).resolve().parent.parent.parent
    out = root / "src/app/aim_trainer.ico"
    big = draw()
    big.resize((256, 256), Image.LANCZOS).save(out, format="ICO", sizes=[(s, s) for s in SIZES])
    print(f"{out.relative_to(root)}: {out.stat().st_size} байт")


if __name__ == "__main__":
    main()
