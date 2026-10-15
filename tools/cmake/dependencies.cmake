# Чужие библиотеки C++-версии — только закреплённые и сверенные.
#
#   SDL 3.4.18 (D-18) — архив выпуска со сверкой SHA-256, статически.
#   GoogleTest 1.14.0 (D-19) — на Linux пакет Ubuntu; без пакета — исходники
#   тега v1.14.0 по коммиту (в пресете windows-cross — исходники из пакета).
#
# Уже скачанный архив SDL берётся из каталога $AIM_DEPS_DIR (его заполняет
# tools/setup_env.sh): без сети и без повторной загрузки в каждый пресет.
include(FetchContent)

set(AIM_SDL3_VERSION "3.4.18")
set(AIM_SDL3_SHA256 "9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3")
set(AIM_DEPS_DIR "$ENV{AIM_DEPS_DIR}" CACHE PATH "Каталог со скачанными архивами зависимостей")

set(_aim_sdl3_archive "SDL3-${AIM_SDL3_VERSION}.tar.gz")
if(AIM_DEPS_DIR AND EXISTS "${AIM_DEPS_DIR}/${_aim_sdl3_archive}")
    set(_aim_sdl3_url "${AIM_DEPS_DIR}/${_aim_sdl3_archive}")
else()
    set(_aim_sdl3_url "https://github.com/libsdl-org/SDL/releases/download/release-${AIM_SDL3_VERSION}/${_aim_sdl3_archive}")
endif()

# Только статическая библиотека, без тестов, примеров и установки (D-18).
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    URL "${_aim_sdl3_url}"
    URL_HASH SHA256=${AIM_SDL3_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)
FetchContent_MakeAvailable(SDL3)

if(BUILD_TESTING)
    find_package(GTest 1.14.0 EXACT CONFIG QUIET)
    if(NOT GTest_FOUND)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG f8d7d77c06936315286eb55f8de22cd23c188571  # v1.14.0
            SYSTEM)
        FetchContent_MakeAvailable(googletest)
    endif()
endif()
