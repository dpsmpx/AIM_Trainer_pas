# Сборка C++-версии

Как собрать C++-версию Aim Trainer у автора на Windows и в облачной сессии.
Решения: C++20 (D-16), llvm-mingw (D-17), SDL 3.4.18 (D-18), CMake, Ninja и
GoogleTest (D-19), раскладка и проверка (D-20, D-25). Pascal-версия
собирается в PascalABC.NET, как раньше (`docs/MANUAL_CHECKS.md`).

## 1. На Windows — у автора

Один раз:

1. **llvm-mingw 20261006** (D-17): архив
   `llvm-mingw-20261006-ucrt-x86_64.zip` со страницы выпуска
   <https://github.com/mstorsjo/llvm-mingw/releases/tag/20261006>. Сверить
   сумму в PowerShell:

   ```powershell
   Get-FileHash .\llvm-mingw-20261006-ucrt-x86_64.zip
   ```

   Должно быть `317492C456AA27EE607A5919F1D2D38DCDC1112516A24D0BF4B00D078F52D17A`.
   Распаковать, например, в `C:\llvm-mingw` и добавить `C:\llvm-mingw\bin`
   в `PATH` («Параметры → Система → О системе → Дополнительные параметры
   системы → Переменные среды»).
2. **CMake от 3.25 и Ninja** (D-19):

   ```powershell
   winget install Kitware.CMake
   winget install Ninja-build.Ninja
   ```

   или установщик с cmake.org и `ninja.exe` с
   <https://github.com/ninja-build/ninja/releases> в `PATH`.
3. Git не нужен: исходники — ветка на GitHub, «Code → Download ZIP».

Сборка — в новом окне PowerShell (чтобы подхватился `PATH`), в каталоге
проекта:

```powershell
cmake --preset windows
cmake --build --preset windows
```

Первая настройка качает SDL 3.4.18 с GitHub и сверяет SHA-256 (D-18) —
нужен интернет; следующие — нет. Итог — `build\windows\AimTrainer.exe`:
один файл без DLL рядом (D-17). Что проверить — сценарий 21
`docs/MANUAL_CHECKS.md`.

Не собирается — в PR или в сессию: вывод обеих команд и строки
`cmake --version`, `ninja --version`, `clang --version`.

## 2. В облачной сессии

Инструменты ставит хук начала сессии — `tools/setup_env.sh`: пакеты Ubuntu
24.04, llvm-mingw для Linux и архив SDL, оба со сверкой SHA-256; пути — в
`~/.local/share/aim-trainer-tools/env.sh`. На своей машине с Ubuntu 24.04 —
тот же скрипт.

Проверка — одной командой (D-25):

```sh
tools/check_cpp.sh                 # всё: документы, оформление, слои, скрипты, все пресеты
tools/check_cpp.sh linux-gcc       # только этот пресет (остальные проверки — всегда)
tools/check_cpp.sh --fix           # заодно поправить оформление clang-format
```

Итог — таблица в конце вывода: её — в описание PR. Журналы —
`build/check/`.

## 3. Пресеты

| пресет | где | что |
|---|---|---|
| `linux-gcc` | сессия | GCC, отладка, тесты |
| `linux-clang` | сессия | Clang, отладка, тесты |
| `linux-asan` | сессия | GCC с ASan и UBSan, тесты |
| `linux-tidy` | сессия | Clang, clang-tidy при сборке; замечание — ошибка |
| `windows-cross` | сессия | llvm-mingw, выпуск; тесты — под Wine |
| `windows` | Windows у автора | llvm-mingw, выпуск, без тестов |

В пресетах сессии предупреждения своего кода — ошибки (D-19); у автора —
нет: его Clang новее, чем в сессии (D-17). Каталог сборки —
`build/<пресет>`, в Git не попадает.

Вручную, без `tools/check_cpp.sh`:

```sh
cmake --preset linux-gcc && cmake --build --preset linux-gcc && ctest --preset linux-gcc
```

## 4. Тесты

- `tests/core/` — логика без окна, GoogleTest.
- `tests/platform/` — версия SDL (D-18) и поток сырого ввода (D-24); на
  Linux — что потока нет.
- `tests/app/` — программа целиком: три кадра на драйвере SDL `offscreen`
  без экрана и снимок последнего — 800×600, фон тёмной темы.

Тесты Windows-сборки идут под Wine без экрана. Окно Windows-сборки в
сессии можно открыть под Xvfb: `DISPLAY=:99 wine
build/windows-cross/AimTrainer.exe`; клик — `xdotool`.

## 5. Откуда зависимости

| что | откуда | как закреплено |
|---|---|---|
| SDL 3.4.18 | архив выпуска `libsdl-org/SDL`; в сессии — `$AIM_DEPS_DIR` | SHA-256 (D-18), `tools/cmake/dependencies.cmake` |
| GoogleTest 1.14.0 | пакет Ubuntu 24.04; иначе — тег `v1.14.0` | версия пакета; коммит `f8d7d77c0693` |
| llvm-mingw 20261006 | выпуск `mstorsjo/llvm-mingw` | SHA-256 (D-17), `tools/setup_env.sh` и раздел 1 |

Происхождение и лицензии — `docs/PROVENANCE.md`.
