# Aim Trainer — C++-версия (перенос, D-08). Pascal-версия в корне — эталон и
# не правится (D-09). Раскладка — D-25, сборка у автора и в сессии —
# docs/BUILD.md:
#
#   cmake --preset linux-gcc && cmake --build --preset linux-gcc && ctest --preset linux-gcc
#   tools/check_cpp.sh            — всё, что проверяется в сессии, одной командой
cmake_minimum_required(VERSION 3.25)
project(AimTrainer LANGUAGES C CXX)

# C++20 без расширений компилятора (D-16).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AIM_WERROR "Предупреждения своего кода — ошибки (сборки сессии, D-19)" OFF)
option(AIM_SANITIZE "Собирать свой код с AddressSanitizer и UndefinedBehaviorSanitizer" OFF)
option(AIM_CLANG_TIDY "Проверять свой код clang-tidy при сборке" OFF)

# ccache ускоряет пересборку и SDL в каждом пресете; без него сборка та же.
find_program(AIM_CCACHE ccache)
if(AIM_CCACHE)
    set(CMAKE_C_COMPILER_LAUNCHER "${AIM_CCACHE}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${AIM_CCACHE}")
endif()

include(CTest)
include(tools/cmake/dependencies.cmake)

# Флаги своего кода; чужие библиотеки собираются своими (D-19).
add_library(aim_options INTERFACE)
target_compile_options(aim_options INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    $<$<BOOL:${AIM_WERROR}>:-Werror>)
if(AIM_SANITIZE)
    target_compile_options(aim_options INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
    target_link_options(aim_options INTERFACE -fsanitize=address,undefined)
endif()
if(MINGW)
    # Один EXE без DLL рядом: libc++, libunwind и winpthreads — внутри (D-17).
    target_link_options(aim_options INTERFACE -static)
endif()

if(AIM_CLANG_TIDY)
    find_program(AIM_CLANG_TIDY_EXE NAMES clang-tidy-18 clang-tidy REQUIRED)
endif()

# Свой код: флаги проекта и, по выбору, clang-tidy (настройки — .clang-tidy).
function(aim_own_code target)
    target_link_libraries(${target} PRIVATE aim_options)
    if(AIM_CLANG_TIDY)
        set_target_properties(${target} PROPERTIES
            CXX_CLANG_TIDY "${AIM_CLANG_TIDY_EXE};--warnings-as-errors=*")
    endif()
endfunction()

# Логика без окна: только стандартная библиотека, без SDL и ОС (D-20, D-25).
add_library(aim_core STATIC
    src/core/click_time.cpp)
target_include_directories(aim_core PUBLIC src)
aim_own_code(aim_core)

# SDL3 и системные вызовы (D-25); свой поток сырого ввода — только Windows (D-24).
if(WIN32)
    add_library(aim_platform STATIC src/platform/raw_mouse_win.cpp)
else()
    add_library(aim_platform STATIC src/platform/raw_mouse_none.cpp)
endif()
target_link_libraries(aim_platform PUBLIC aim_core SDL3::SDL3)
aim_own_code(aim_platform)

# Программа: окно и главный цикл. WIN32 — без окна консоли; WinMain даёт
# SDL_main.h.
add_executable(AimTrainer WIN32 src/app/main.cpp)
target_link_libraries(AimTrainer PRIVATE aim_platform)
aim_own_code(AimTrainer)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
