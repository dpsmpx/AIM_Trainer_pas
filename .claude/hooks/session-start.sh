#!/bin/bash
# Хук начала веб-сессии Claude Code (по шаблону process-kit Galdhold,
# tools/dpv/kit; без DP Verify — D-01).
#
#   * инструменты C++-версии — tools/setup_env.sh (D-25), пути — в окружение
#     сессии;
#   * проверка документов — tools/check_docs.py;
#   * ветки с невлитой работой — с них продолжают, а не делают заново
#     (CLAUDE.md, «Не терять работу между сессиями»).
#
# Только в веб-сессии; рабочее дерево не меняет. Сбой установки хук не
# валит: о нём сообщается, а сессия остаётся с тем, что поставилось.
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
    exit 0
fi

PROJ="${CLAUDE_PROJECT_DIR:-$(cd "$(dirname "$0")/../.." && pwd)}"
MAIN=main

# ---- Инструменты C++-версии ----
TOOLS_DIR="${AIM_TOOLS_DIR:-$HOME/.local/share/aim-trainer-tools}"
mkdir -p "$TOOLS_DIR"
if "$PROJ/tools/setup_env.sh" >"$TOOLS_DIR/setup_env.log" 2>&1; then
    echo "session-start: инструменты C++ — на месте (tools/setup_env.sh)"
    if [ -n "${CLAUDE_ENV_FILE:-}" ] && [ -f "$TOOLS_DIR/env.sh" ]; then
        cat "$TOOLS_DIR/env.sh" >>"$CLAUDE_ENV_FILE"
    fi
else
    echo "session-start: инструменты C++ не поставились — $TOOLS_DIR/setup_env.log:"
    tail -15 "$TOOLS_DIR/setup_env.log" | sed 's/^/  /'
fi

# ---- Документы ----
if command -v python3 >/dev/null 2>&1; then
    if out="$(python3 "$PROJ/tools/check_docs.py" 2>&1)"; then
        echo "session-start: документы — чисто (tools/check_docs.py)"
    else
        echo "session-start: документы — есть находки (tools/check_docs.py):"
        printf '%s\n' "$out" | sed 's/^/  /'
    fi
fi

# ---- Невлитая работа прошлых сессий ----
{
    # refspec явно: клон с одной веткой (--single-branch, --depth) другие ветки не видит
    git -C "$PROJ" fetch -q origin '+refs/heads/*:refs/remotes/origin/*' 2>/dev/null || true
    echo "session-start: ветки с работой, не влитой в $MAIN (свежие первыми):"
    found=0
    while read -r ref; do
        n="$(git -C "$PROJ" cherry "origin/$MAIN" "$ref" 2>/dev/null | grep -c '^+' || true)"
        [ "${n:-0}" -gt 0 ] || continue
        found=1
        echo "  ${ref#origin/}: коммитов $n, последний — $(git -C "$PROJ" log -1 --format='%cr: %s' "$ref")"
    done < <(git -C "$PROJ" for-each-ref --sort=-committerdate --format='%(refname:short)' \
                 refs/remotes/origin | grep -v -e '^origin$' -e '^origin/HEAD$' -e "^origin/$MAIN\$" | head -8)
    [ "$found" -eq 1 ] || echo "  нет"
    echo "  Перед работой сверить с ними задачу: git log origin/$MAIN..origin/<ветка>."
} || true
