# Сборка DP AIM Trainer

- Версия в программе («Справка», свойства EXE): `Release2-60-g1f9c46d`.
- Исходники: коммит `1f9c46dbf73247330ddc3164cccda352da5642cc` ветки `ccr-130336a8-8m3ovp` репозитория
  `dpsmpx/AIM_Trainer_pas`; последняя правка кода — `1f9c46dbf73247330ddc3164cccda352da5642cc`.
- Собрано: 2026-10-17 17:45 UTC, `tools/publish_build.sh`,
  пресет `windows-cross` (Release).
- Набор: llvm-mingw 20261006 (D-17) — clang version 23.1.3 (https://github.com/llvm/llvm-project.git 0d261d1ca552c95a8f007e061c787ac7132fbcbc); SDL 3.4.18, SDL3_ttf
  3.2.2, FreeType 2.15.0 — статически (D-18, D-31): EXE нужны только
  системные DLL Windows. Шрифты DejaVu Sans Condensed и Sans Mono 2.37 —
  `fonts/` со своей лицензией.
- Суммы SHA-256 — `SHA256SUMS`.
