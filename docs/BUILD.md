# Сборка C++-версии

Как собрать C++-версию Aim Trainer у автора на Windows и в облачной сессии.
Решения: C++20 (D-16), llvm-mingw (D-17), SDL 3.4.18 (D-18), CMake, Ninja и
GoogleTest (D-19), раскладка и проверка (D-20, D-25), текст и шрифт (D-31). Pascal-версия —
образец в `archive/pascal/` (D-28) — собирается под Mono в сессии, §5.

## 1. На Windows — у автора

Готовая сборка — ветка `win_debug_builds`, каталог `AimTrainer/` (D-34):
«Code» → «Download ZIP». Своя — ниже.

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

Первая настройка качает SDL 3.4.18, SDL3_ttf 3.2.2, FreeType 2.15.0 и
шрифт DejaVu 2.37 и сверяет SHA-256 (D-18, D-31) — нужен интернет;
следующие — нет. Итог — `build\windows\AimTrainer.exe` без DLL рядом
(D-17) и каталог `build\windows\fonts\` со шрифтом (D-31): программу
переносить вместе с ним. Что проверить — сценарий 21
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

Сборку для автора — EXE с `fonts/` — по его запросу выкладывает
`tools/publish_build.sh` в ветку `win_debug_builds`, каталог `AimTrainer/`
(D-23, D-29, D-34): собирает пресет `windows-cross` из закоммиченного кода
и записывает коммит в `AimTrainer/BUILD.md`.

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
- `tests/app/` — файлы игрока (`player_files_test.cpp`) и программа
  целиком: три кадра на драйвере SDL `offscreen` без экрана и снимок
  последнего — 800×600 и цвет панели туториала в одной точке
  (`frame_check.cpp`). Остальная отрисовка тестами не проверяется
  (AUD-49).

Тесты Windows-сборки идут под Wine без экрана. Окно Windows-сборки в
сессии можно открыть под Xvfb: `DISPLAY=:99 wine
build/windows-cross/AimTrainer.exe`; клик — `xdotool`.

## 5. Эталон — оригинал под Mono

Оригинал на PascalABC.NET собирается и запускается в сессии (D-26):
компилятор 4.0.0.3882 из исходников с заплаткой для Mono 6.8 ставит
`tools/setup_env.sh`, сборкой и окном управляет `tools/gui.sh`:

```sh
tools/gui.sh ref build              # archive/pascal/*.pas → build/ref/AIM_Trainer.exe
tools/gui.sh ref start              # запуск в своём Xvfb (:97)
tools/gui.sh ref click 585 57       # ЛКМ по «Classic Mode»; 3-й аргумент — кнопка
tools/gui.sh ref key Escape space   # клавиши
tools/gui.sh ref shot menu.png      # снимок окна 800×600
tools/gui.sh ref stop
tools/gui.sh compare a.png b.png    # число несовпавших пикселей
```

То же с `port` вместо `ref` — C++-версия (пресет `linux-gcc`, экран `:96`).
Оригинал пишет файлы профиля в `build/ref` (AUD-17), C++-версия — в
`build/gui/port/data` (`AIM_TRAINER_DATA_DIR`, D-30), а не в папку
пользователя сессии; `tools/gui.sh ЦЕЛЬ dir` печатает папку. `tools/gui.sh
ref build` начинает с чистого каталога, `tools/gui.sh ЦЕЛЬ clean` — удаляет
только файлы игрока.

Вспомогательные скрипты сессий — `tools/helpers/` (D-27):

| скрипт | что делает |
|---|---|
| `ref_bot.py` | играет по снимкам окна: находит цели и бомбы, щёлкает или наводит курсор; правила — в шапке |
| `wine_clicks.sh` | щелчки по Windows-сборке под Wine: сколько получили время из сырого ввода (D-24) |
| `click_debug_log.patch` | временный журнал времени клика в `main.cpp` — для разбора, не для коммита |
| `profile_menu.sh` | замер кадра меню под Callgrind: инструкции своего кода, вызовы отрисовки и выделения памяти на кадр (AUD-39) |

## 6. Откуда зависимости

| что | откуда | как закреплено |
|---|---|---|
| SDL 3.4.18 | архив выпуска `libsdl-org/SDL`; в сессии — `$AIM_DEPS_DIR` | SHA-256 (D-18), `tools/cmake/dependencies.cmake` |
| SDL3_ttf 3.2.2, FreeType 2.15.0 | архивы выпусков `libsdl-org/SDL_ttf` и `download.savannah.gnu.org`; в сессии — `$AIM_DEPS_DIR` | SHA-256 и подписи авторов (D-31), `tools/cmake/dependencies.cmake` |
| шрифт DejaVu Sans Mono 2.37 | архив выпуска `dejavu-fonts/dejavu-fonts`; в сессии — `$AIM_DEPS_DIR` | SHA-256 (D-31); сборка кладёт файлы в `fonts/` рядом с программой |
| GoogleTest 1.14.0 | пакет Ubuntu 24.04; иначе — тег `v1.14.0` | версия пакета; коммит `f8d7d77c0693` |
| llvm-mingw 20261006 | выпуск `mstorsjo/llvm-mingw` | SHA-256 (D-17), `tools/setup_env.sh` и раздел 1 |

Происхождение и лицензии — `docs/PROVENANCE.md`.
