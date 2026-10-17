# Чужие библиотеки C++-версии — только закреплённые и сверенные.
#
#   SDL 3.4.18 (D-18) — архив выпуска со сверкой SHA-256, статически.
#   SDL3_ttf 3.2.2 и FreeType 2.15.0 (D-31) — так же; FreeType — внутри
#   SDL3_ttf, его же сборкой.
#   DejaVu 2.37 (D-31, D-43): Sans Condensed и Sans Mono — архив выпуска со
#   сверкой SHA-256; файлы шрифтов кладёт рядом с программой CMakeLists.txt.
#   GoogleTest 1.14.0 (D-19) — на Linux пакет Ubuntu; без пакета — исходники
#   тега v1.14.0 по коммиту (в пресете windows-cross — исходники из пакета).
#
# Уже скачанные архивы берутся из каталога $AIM_DEPS_DIR (его заполняет
# tools/setup_env.sh): без сети и без повторной загрузки в каждый пресет.
include(FetchContent)

set(AIM_SDL3_VERSION "3.4.18")
set(AIM_SDL3_SHA256 "9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3")
set(AIM_SDL3_TTF_VERSION "3.2.2")
set(AIM_SDL3_TTF_SHA256 "63547d58d0185c833213885b635a2c0548201cc8f301e6587c0be1a67e1e045d")
set(AIM_FREETYPE_VERSION "2.15.0")
set(AIM_FREETYPE_SHA256 "93cd47eb719c325107f2d3cf087d06299e944c3b48a40b55537302304dea1dd5")
set(AIM_DEJAVU_VERSION "2.37")
set(AIM_DEJAVU_SHA256 "fa9ca4d13871dd122f61258a80d01751d603b4d3ee14095d65453b4e846e17d7")
set(AIM_DEPS_DIR "$ENV{AIM_DEPS_DIR}" CACHE PATH "Каталог со скачанными архивами зависимостей")

# aim_archive_url(VAR АРХИВ АДРЕС) — архив из $AIM_DEPS_DIR, если он там есть,
# иначе адрес выпуска.
function(aim_archive_url var archive url)
    if(AIM_DEPS_DIR AND EXISTS "${AIM_DEPS_DIR}/${archive}")
        set(${var} "${AIM_DEPS_DIR}/${archive}" PARENT_SCOPE)
    else()
        set(${var} "${url}/${archive}" PARENT_SCOPE)
    endif()
endfunction()

aim_archive_url(_aim_sdl3_url "SDL3-${AIM_SDL3_VERSION}.tar.gz"
    "https://github.com/libsdl-org/SDL/releases/download/release-${AIM_SDL3_VERSION}")
aim_archive_url(_aim_sdl3_ttf_url "SDL3_ttf-${AIM_SDL3_TTF_VERSION}.tar.gz"
    "https://github.com/libsdl-org/SDL_ttf/releases/download/release-${AIM_SDL3_TTF_VERSION}")
aim_archive_url(_aim_freetype_url "freetype-${AIM_FREETYPE_VERSION}.tar.xz"
    "https://download.savannah.gnu.org/releases/freetype")
string(REPLACE "." "_" _aim_dejavu_tag "version_${AIM_DEJAVU_VERSION}")
aim_archive_url(_aim_dejavu_url "dejavu-fonts-ttf-${AIM_DEJAVU_VERSION}.tar.bz2"
    "https://github.com/dejavu-fonts/dejavu-fonts/releases/download/${_aim_dejavu_tag}")

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

# SDL3_ttf со своим (vendored) FreeType: архив выпуска SDL3_ttf приходит без
# external/freetype — туда кладётся архив FreeType. SOURCE_SUBDIR без
# CMakeLists.txt — только распаковать; подключает SDL3_ttf add_subdirectory
# ниже, когда FreeType уже на месте.
FetchContent_Declare(sdl3_ttf
    URL "${_aim_sdl3_ttf_url}"
    URL_HASH SHA256=${AIM_SDL3_TTF_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR aim-unpack-only)
FetchContent_MakeAvailable(sdl3_ttf)
FetchContent_Declare(freetype
    URL "${_aim_freetype_url}"
    URL_HASH SHA256=${AIM_FREETYPE_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR "${sdl3_ttf_SOURCE_DIR}/external/freetype"
    SOURCE_SUBDIR aim-unpack-only)
FetchContent_MakeAvailable(freetype)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${sdl3_ttf_SOURCE_DIR}" "${sdl3_ttf_BINARY_DIR}" EXCLUDE_FROM_ALL SYSTEM)

# Шрифт (D-31): только распаковать; файлы — рядом с программой.
FetchContent_Declare(dejavu
    URL "${_aim_dejavu_url}"
    URL_HASH SHA256=${AIM_DEJAVU_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR aim-unpack-only)
FetchContent_MakeAvailable(dejavu)
set(AIM_FONT_FILES
    "${dejavu_SOURCE_DIR}/ttf/DejaVuSansCondensed.ttf"
    "${dejavu_SOURCE_DIR}/ttf/DejaVuSansCondensed-Bold.ttf"
    "${dejavu_SOURCE_DIR}/ttf/DejaVuSansMono.ttf"
    "${dejavu_SOURCE_DIR}/ttf/DejaVuSansMono-Bold.ttf"
    "${dejavu_SOURCE_DIR}/LICENSE")

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
