# Сборка AIM Trainer

- Исходники: коммит `552b3518bc2e7eb5446bf1896b7e2344308e7323` ветки `claude/dazzling-ritchie-fn3gj8` репозитория
  `dpsmpx/AIM_Trainer_pas`; последняя правка кода — `5c080ccb8b6306ce66009c883f99afe67ed67f10`.
- Собрано: 2026-10-16 15:19 UTC, `tools/publish_build.sh`,
  пресет `windows-cross` (Release).
- Набор: llvm-mingw 20261006 (D-17) — clang version 23.1.3 (https://github.com/llvm/llvm-project.git 0d261d1ca552c95a8f007e061c787ac7132fbcbc); SDL 3.4.18, SDL3_ttf
  3.2.2, FreeType 2.15.0 — статически (D-18, D-31): EXE нужны только
  системные DLL Windows. Шрифт DejaVu Sans Mono 2.37 — `fonts/` со
  своей лицензией.
- Суммы SHA-256 — `SHA256SUMS`.
