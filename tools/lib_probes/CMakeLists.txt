# Замерщики ввода: по EXE на библиотеку, чтобы выбрать библиотеку по точности
# замера реакции (поручение автора 15 октября 2026, docs/LIB_PROBES.md).
#
# Одна библиотека — один каталог сборки (PROBE_LIB): raylib несёт свой GLFW, а
# SDL2 и SDL3 читают одни и те же настройки — в одной сборке они мешали бы
# друг другу. Все исходники библиотек закреплены: архивы — по SHA-256, клоны —
# по коммиту тега. Сборка всех сразу — tools/lib_probes/build.sh.
cmake_minimum_required(VERSION 3.25)
project(AimTrainerLibProbes LANGUAGES C CXX)

set(PROBE_LIB "" CACHE STRING "SDL3, SDL2, GLFW, raylib, SFML, Allegro или WinAPI")

if(NOT WIN32)
    message(FATAL_ERROR "Замерщики ввода собираются только под Windows: "
                        "tools/lib_probes/build.sh (кросс-сборка llvm-mingw, D-17).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

execute_process(COMMAND git -C "${CMAKE_CURRENT_SOURCE_DIR}" rev-parse --short=12 HEAD
                OUTPUT_VARIABLE PROBE_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT PROBE_REVISION)
    set(PROBE_REVISION "неизвестно")
endif()

include(FetchContent)

# Замерщик: свой код — со всеми предупреждениями как ошибками (D-19), всё
# статически — один EXE без DLL рядом (D-17), в свойствах файла — библиотека.
function(add_probe name source title)
    set(PROBE_NAME "${name}")
    set(PROBE_TITLE "${title}")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/probe.rc.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/${name}.rc" @ONLY)
    add_executable(probe_${name} WIN32 ${source} probe_common.cpp
                   "${CMAKE_CURRENT_BINARY_DIR}/${name}.rc")
    set_target_properties(probe_${name} PROPERTIES OUTPUT_NAME "AimProbe_${name}")
    target_compile_options(probe_${name} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion
                                  -Wsign-conversion -Werror>)
    target_link_options(probe_${name} PRIVATE -static)
    target_link_libraries(probe_${name} PRIVATE dwmapi)
endfunction()

if(PROBE_LIB STREQUAL "SDL3")
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(sdl3
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
        URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE SYSTEM)
    FetchContent_MakeAvailable(sdl3)
    add_probe(SDL3 probe_sdl3.cpp "SDL3 3.4.18")
    target_link_libraries(probe_SDL3 PRIVATE SDL3::SDL3-static)
    add_probe(SDL3_raw probe_sdl3.cpp "SDL3 3.4.18, относительный режим мыши")
    target_compile_definitions(probe_SDL3_raw PRIVATE PROBE_SDL3_RAW)
    target_link_libraries(probe_SDL3_raw PRIVATE SDL3::SDL3-static)

elseif(PROBE_LIB STREQUAL "SDL2")
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(sdl2
        URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz
        URL_HASH SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE SYSTEM)
    FetchContent_MakeAvailable(sdl2)
    add_probe(SDL2 probe_sdl2.cpp "SDL2 2.32.10")
    target_link_libraries(probe_SDL2 PRIVATE SDL2::SDL2main SDL2::SDL2-static)

elseif(PROBE_LIB STREQUAL "GLFW")
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        URL https://github.com/glfw/glfw/releases/download/3.5.1/glfw-3.5.1.zip
        URL_HASH SHA256=ea79bc5feffc254c87291980c2d0bce9acebb68c4983b79f961dcd2cb8a611a0
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE SYSTEM)
    FetchContent_MakeAvailable(glfw)
    add_probe(GLFW probe_glfw.cpp "GLFW 3.5.1")
    target_link_libraries(probe_GLFW PRIVATE glfw opengl32)

elseif(PROBE_LIB STREQUAL "raylib")
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(PLATFORM "Desktop" CACHE STRING "" FORCE)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG dbc56a87da87d973a9c5baa4e7438a9d20121d28  # 6.0
        SYSTEM)
    FetchContent_MakeAvailable(raylib)
    add_probe(raylib probe_raylib.cpp "raylib 6.0")
    target_link_libraries(probe_raylib PRIVATE raylib)

elseif(PROBE_LIB STREQUAL "SFML")
    set(SFML_BUILD_WINDOW ON CACHE BOOL "" FORCE)
    set(SFML_BUILD_GRAPHICS OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_DOC OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(sfml
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 8835b6b955c96fa1ad1278a8dae6f05c4143effc  # 3.1.0
        SYSTEM)
    FetchContent_MakeAvailable(sfml)
    add_probe(SFML probe_sfml.cpp "SFML 3.1.0 (окно и OpenGL)")
    target_link_libraries(probe_SFML PRIVATE SFML::Window opengl32)

elseif(PROBE_LIB STREQUAL "Allegro")
    set(SHARED OFF CACHE BOOL "" FORCE)
    foreach(opt WANT_DOCS WANT_EXAMPLES WANT_TESTS WANT_DEMO WANT_IMAGE WANT_FONT WANT_TTF
                WANT_AUDIO WANT_ACODEC WANT_COLOR WANT_MEMFILE WANT_PHYSFS WANT_NATIVE_DIALOG
                WANT_VIDEO WANT_SHADERS_D3D WANT_SHADERS_GL WANT_POPUP_EXAMPLES WANT_RELEASE_LOGGING)
        set(${opt} OFF CACHE BOOL "" FORCE)
    endforeach()
    set(WANT_PRIMITIVES ON CACHE BOOL "" FORCE)
    FetchContent_Declare(allegro
        GIT_REPOSITORY https://github.com/liballeg/allegro5.git
        GIT_TAG 9e58c5e03af16810e8cfecec719e69ce4a76b49b  # 5.2.11.3
        SYSTEM)
    FetchContent_MakeAvailable(allegro)
    add_probe(Allegro probe_allegro.cpp "Allegro 5.2.11.3")
    target_compile_definitions(probe_Allegro PRIVATE ALLEGRO_STATICLINK)
    target_include_directories(probe_Allegro SYSTEM PRIVATE
        "${allegro_SOURCE_DIR}/include" "${allegro_BINARY_DIR}/include"
        "${allegro_SOURCE_DIR}/addons/primitives")
    target_link_libraries(probe_Allegro PRIVATE allegro_primitives allegro)

elseif(PROBE_LIB STREQUAL "WinAPI")
    add_probe(WinAPI probe_winapi.cpp "Win32 и GDI, без библиотеки")
    target_link_libraries(probe_WinAPI PRIVATE gdi32)

else()
    message(FATAL_ERROR "PROBE_LIB: SDL3, SDL2, GLFW, raylib, SFML, Allegro или WinAPI")
endif()
