#!/usr/bin/env python3
"""Проверка документов Aim Trainer (D-01).

1. В корне — только точки входа и исходники C++-версии (`ROOT_ALLOW`).
2. Ссылка Markdown `[…](путь)` ведёт на существующий файл или каталог.
3. Путь в обратных кавычках вида `docs/…`, `ROADMAP/…`, `tools/…`,
   `archive/…`, `.claude/…`, `.github/…` существует. Голые имена файлов не
   сверяются: документы нарочно цитируют несуществующие (`TODO.md`,
   раздел «README»). Не сверяются и аудиты (`docs/audits/` — снимок на
   дату), CHANGELOG (история называет то, чего уже нет), README (D-06),
   пути с чужим проектом (`DPmeta:docs/…`).
4. Решение `### D-NN` в журнале: номера по порядку без пропусков и
   повторов, есть поля «Статус», «Решение», «Почему», «Проверка».
5. Ссылка `D-NN` в документах ведёт на существующее решение, `AUD-NN` —
   на находку аудита.
6. `TODO.md`: пункт верхнего уровня — «- `М` **Что** — …».
7. `ROADMAP/ROADMAP.md`: задача — `### <a id="task-…"></a>[ ] …` или
   `[+]` (якорь — номер этапа или имя: `task-2`, `task-cpp`), шаг —
   `- [ ]` или `- [+]`.
8. Чего по решениям быть не должно, того нет (`MUST_BE_ABSENT`).
9. Исходники Pascal-версии в `archive/pascal/` не изменены
   (`PASCAL_FROZEN`, D-09, D-28): хэш `git hash-object` каждого файла
   совпадает с записанным. Меняются эти хэши только вместе с новым
   решением.

Только стандартная библиотека, без сети. Коды возврата: 0 — чисто,
1 — есть находки.
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ROOT_ALLOW = {
    ".git", ".gitignore", ".github", ".claude",
    "README.md", "CLAUDE.md", "TODO.md", "LICENSE",
    "docs", "ROADMAP", "tools",
    # Pascal-версия — образец функционала, в архиве (D-28).
    "archive",
    # C++-версия (D-20); build/ — каталог сборки, в Git не попадает.
    "CMakeLists.txt", "CMakePresets.json", "src", "tests",
    ".clang-format", ".clang-tidy", "build",
}
DECISIONS = ROOT / "docs/direction/DECISIONS.md"
AUDITS = ROOT / "docs/audits"
TODO = ROOT / "TODO.md"
ROADMAP = ROOT / "ROADMAP/ROADMAP.md"
DECISION_FIELDS = ("**Статус.**", "**Решение.**", "**Почему.**", "**Проверка.**")
# По решению этого быть не должно; документы называют его, говоря, что его нет.
MUST_BE_ABSENT = {".github/workflows": "D-07"}
# Названо в документах как отклонённое, но не запрещено (D-01, «Отклонено»).
NAMED_ABSENT = {"tools/dpv"}
# Pascal-версия остаётся как есть (D-09), лежит в архиве (D-28): хэши blob
# на коммите cbf5c3c.
PASCAL_FROZEN = {
    "archive/pascal/AIM_Trainer.pas": "fab3b6383dbaf9909ae5da77f68295e21ec2376c",
    "archive/pascal/AimCircle.pas": "265704c2c59518759ed573b9d048afc5c2b2b382",
    "archive/pascal/Control_AI.pas": "33a228e5afa8853dff320c6afbe23140b799a94c",
    "archive/pascal/Base.pas": "41e8be2f9904fd4b4d164e7c39173c54207115f2",
}

LINK_RE = re.compile(r"\[[^\]]*\]\(([^)\s]+)\)")
CODE_RE = re.compile(r"`([^`\s]+)`")
PATH_RE = re.compile(r"^(?:docs|ROADMAP|tools|archive|\.claude|\.github)/[\w./-]*(?::\d+(?:-\d+)?)?$")
D_REF_RE = re.compile(r"(?<![\w:-])D-(\d{2})(?![\w-])")
AUD_REF_RE = re.compile(r"(?<![\w:-])AUD-(\d{2})(?![\w-])")
TODO_ITEM_RE = re.compile(r"^- `[МСТК]` \*\*[^*]+\*\*")
TASK_RE = re.compile(r'^### <a id="task-[a-z0-9-]+"></a>\[[ +]\] \S')
STEP_RE = re.compile(r"^- \[[ +]\] \S")


def md_files() -> list[Path]:
    out = subprocess.run(["git", "-C", str(ROOT), "ls-files", "-z", "--cached", "--others",
                          "--exclude-standard"], capture_output=True, text=True)
    rels = [p for p in out.stdout.split("\0") if p] if out.returncode == 0 else []
    return sorted(ROOT / r for r in rels if r.endswith(".md") and (ROOT / r).is_file())


def decisions(bad: list[str]) -> set[str]:
    if not DECISIONS.is_file():
        bad.append(f"нет журнала решений {DECISIONS.relative_to(ROOT)}")
        return set()
    text = DECISIONS.read_text(encoding="utf-8")
    parts = re.split(r"^### (D-\d{2})\b.*$", text, flags=re.M)
    ids = parts[1::2]
    for want, (rid, body) in enumerate(zip(ids, parts[2::2]), 1):
        if rid != f"D-{want:02d}":
            bad.append(f"DECISIONS: {rid} на месте D-{want:02d} — номера по порядку, без пропусков")
        for field in DECISION_FIELDS:
            if field not in body:
                bad.append(f"DECISIONS: у {rid} нет поля {field}")
    return set(ids)


def findings() -> set[str]:
    ids: set[str] = set()
    for f in sorted(AUDITS.glob("*.md")):
        ids.update(re.findall(r"^#{2,4} (AUD-\d{2})\b", f.read_text(encoding="utf-8"), re.M))
    return ids


def check_shape(path: Path, item_re: re.Pattern[str], what: str, bad: list[str],
                prefix: str) -> None:
    if not path.is_file():
        bad.append(f"нет {path.relative_to(ROOT)}")
        return
    for no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if line.startswith(prefix) and not item_re.match(line):
            bad.append(f"{path.relative_to(ROOT)}:{no}: {what} не по форме: {line[:60]}")


def main() -> int:
    bad: list[str] = []
    known_d = decisions(bad)
    known_aud = findings()

    for p in sorted(ROOT.iterdir()):
        if p.name not in ROOT_ALLOW:
            bad.append(f"в корне лишнее: {p.name} (CLAUDE.md, «Где лежат не-кодовые файлы»)")

    for f in md_files():
        rel = f.relative_to(ROOT)
        snapshot = rel.parts[:2] == ("docs", "audits") or rel.as_posix() in {
            "docs/CHANGELOG.md", "README.md"}
        for no, line in enumerate(f.read_text(encoding="utf-8").splitlines(), 1):
            for m in LINK_RE.finditer(line):
                target = m.group(1).split("#", 1)[0]
                if target and not re.match(r"[a-z]+:", target) and not (f.parent / target).exists():
                    bad.append(f"{rel}:{no}: ссылка на несуществующее {target}")
            if rel.as_posix() == "README.md":
                continue
            if not snapshot:
                for m in CODE_RE.finditer(line):
                    tok = m.group(1)
                    if not PATH_RE.match(tok):
                        continue
                    path = tok.split(":", 1)[0]
                    if path.rstrip("/") in MUST_BE_ABSENT.keys() | NAMED_ABSENT:
                        continue
                    if not (ROOT / path).exists():
                        bad.append(f"{rel}:{no}: путь не существует: {path}")
            for m in D_REF_RE.finditer(line):
                if f"D-{m.group(1)}" not in known_d:
                    bad.append(f"{rel}:{no}: решения D-{m.group(1)} нет в журнале")
            for m in AUD_REF_RE.finditer(line):
                if f"AUD-{m.group(1)}" not in known_aud:
                    bad.append(f"{rel}:{no}: находки AUD-{m.group(1)} нет в docs/audits/")

    for path, rule in MUST_BE_ABSENT.items():
        if (ROOT / path).exists():
            bad.append(f"есть {path} — против {rule}")

    for name, want in PASCAL_FROZEN.items():
        if not (ROOT / name).is_file():
            bad.append(f"нет {name} — Pascal-версия лежит в архиве как есть (D-09, D-28)")
            continue
        got = subprocess.run(["git", "-C", str(ROOT), "hash-object", name],
                             capture_output=True, text=True).stdout.strip()
        if got != want:
            bad.append(f"{name} изменён — Pascal-версия остаётся как есть (D-09)")

    check_shape(TODO, TODO_ITEM_RE, "пункт TODO", bad, "- ")
    check_shape(ROADMAP, TASK_RE, "задача ROADMAP", bad, "### ")
    check_shape(ROADMAP, STEP_RE, "шаг ROADMAP", bad, "- ")

    for b in bad:
        print(b)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
