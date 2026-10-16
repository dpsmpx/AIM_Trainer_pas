# Отладочные сборки AIM Trainer

Ветка только со сборками для проверки у автора на Windows; в `main` не
вливается (решение D-23 в `docs/direction/DECISIONS.md` ветки `main`).

- Исходники: коммит `d642df41e440866ce0f1f66db2eba982bad94783` ветки `claude/youthful-darwin-t4qnge` репозитория
  `dpsmpx/AIM_Trainer_pas` — `tools/lib_probes/`.
- Собрано: 2026-10-15 17:33 UTC, `tools/lib_probes/build.sh`.
- Набор: llvm-mingw 20261006 (D-17) — clang version 23.1.3 (https://github.com/llvm/llvm-project.git 0d261d1ca552c95a8f007e061c787ac7132fbcbc); всё статически, EXE нужны
  только системные DLL Windows.
- Как запустить и что прислать — `AimProbes/README.md`.
- Суммы SHA-256 — `AimProbes/SHA256SUMS`.
