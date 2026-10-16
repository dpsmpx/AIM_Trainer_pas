# Кросс-сборка под Windows набором llvm-mingw 20261006 (D-17): C++-версия
# (пресет windows-cross, D-25) и замерщики ввода (tools/lib_probes/build.sh).
#
# Путь к набору — переменная окружения LLVM_MINGW (её пишет tools/setup_env.sh);
# без неё — места, куда набор ставят tools/setup_env.sh и прежние сессии.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(_aim_llvm_mingw_name "llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64")
set(LLVM_MINGW "$ENV{LLVM_MINGW}")
if(NOT LLVM_MINGW)
    foreach(_dir "$ENV{HOME}/.local/share/aim-trainer-tools/${_aim_llvm_mingw_name}"
                 "/opt/${_aim_llvm_mingw_name}")
        if(EXISTS "${_dir}/bin/x86_64-w64-mingw32-clang++")
            set(LLVM_MINGW "${_dir}")
            break()
        endif()
    endforeach()
endif()
if(NOT EXISTS "${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang++")
    message(FATAL_ERROR "Нет llvm-mingw 20261006 (D-17): поставьте tools/setup_env.sh "
                        "или укажите путь в LLVM_MINGW.")
endif()

set(CMAKE_C_COMPILER "${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang")
set(CMAKE_CXX_COMPILER "${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang++")
set(CMAKE_RC_COMPILER "${LLVM_MINGW}/bin/x86_64-w64-mingw32-windres")

set(CMAKE_FIND_ROOT_PATH "${LLVM_MINGW}/x86_64-w64-mingw32")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Тесты собранного под Windows CTest запускает под Wine, если он есть; без
# него тесты не заводятся, и tools/check_cpp.sh так и пишет в итоге.
find_program(AIM_WINE NAMES wine wine64)
if(AIM_WINE)
    set(CMAKE_CROSSCOMPILING_EMULATOR "${AIM_WINE}")
endif()
