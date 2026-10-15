# Кросс-сборка под Windows набором llvm-mingw 20261006 (D-17). Путь к набору —
# переменная окружения LLVM_MINGW; её же читают проверочные сборки CMake.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(LLVM_MINGW "$ENV{LLVM_MINGW}")
if(NOT LLVM_MINGW)
    set(LLVM_MINGW "/opt/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64")
endif()

set(CMAKE_C_COMPILER "${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang")
set(CMAKE_CXX_COMPILER "${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang++")
set(CMAKE_RC_COMPILER "${LLVM_MINGW}/bin/x86_64-w64-mingw32-windres")

set(CMAKE_FIND_ROOT_PATH "${LLVM_MINGW}/x86_64-w64-mingw32")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
