# Сборка DP AIM Trainer

- Исходники: коммит `e8078bd438e049d0d011ab318a40011e88f063ae` ветки `claude/dazzling-ritchie-fn3gj8` репозитория
  `dpsmpx/AIM_Trainer_pas`; последняя правка кода — `af5a042c045b19561cbf53d91e8136f9fd64a690`.
- Собрано: 2026-10-17 01:54 UTC, `tools/publish_build.sh`,
  пресет `windows-cross` (Release).
- Набор: llvm-mingw 20261006 (D-17) — clang version 23.1.3 (https://github.com/llvm/llvm-project.git 0d261d1ca552c95a8f007e061c787ac7132fbcbc); SDL 3.4.18, SDL3_ttf
  3.2.2, FreeType 2.15.0 — статически (D-18, D-31): EXE нужны только
  системные DLL Windows. Шрифты DejaVu Sans Condensed и Sans Mono 2.37 —
  `fonts/` со своей лицензией.
- Суммы SHA-256 — `SHA256SUMS`.
