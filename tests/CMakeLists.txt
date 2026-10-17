# Тесты C++-версии (D-19, D-25): модульные — GoogleTest через CTest, по
# каталогам слоёв; программа целиком — несколько кадров без экрана.
#
# Собранное под Windows в сессии запускается под Wine (tools/cmake/llvm-mingw.cmake);
# без Wine тесты кросс-сборки не заводятся — tools/check_cpp.sh пишет это в итог.
if(CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
    message(STATUS "Тесты не заводятся: собранное под ${CMAKE_SYSTEM_NAME} здесь не запустить")
    return()
endif()

include(GoogleTest)

# Под Wine первый запуск заводит каталог Wine и идёт долго: список тестов
# ждём дольше, чем по умолчанию (5 с).
if(CMAKE_CROSSCOMPILING)
    set(_aim_discovery_timeout 120)
else()
    set(_aim_discovery_timeout 10)
endif()

add_executable(aim_core_tests
    core/click_time_test.cpp
    core/flow_test.cpp
    core/frame_budget_test.cpp
    core/game_test.cpp
    core/measure_test.cpp
    core/menu_test.cpp
    core/parse_fuzz_test.cpp
    core/particles_test.cpp
    core/preset_run_test.cpp
    core/profile_file_test.cpp
    core/profile_test.cpp
    core/random_test.cpp
    core/records_test.cpp
    core/replay_test.cpp
    core/runs_test.cpp
    core/target_test.cpp
    core/theme_test.cpp
    core/tone_test.cpp
    core/wrap_test.cpp)
target_link_libraries(aim_core_tests PRIVATE aim_core GTest::gtest_main)
aim_own_code(aim_core_tests)
gtest_discover_tests(aim_core_tests DISCOVERY_MODE PRE_TEST
                     DISCOVERY_TIMEOUT ${_aim_discovery_timeout})

add_executable(aim_platform_tests
    platform/raw_mouse_test.cpp
    platform/sdl_version_test.cpp
    platform/single_instance_test.cpp
    platform/sound_test.cpp
    platform/storage_test.cpp)
target_link_libraries(aim_platform_tests PRIVATE aim_platform GTest::gtest_main)
aim_own_code(aim_platform_tests)
gtest_discover_tests(aim_platform_tests DISCOVERY_MODE PRE_TEST
                     DISCOVERY_TIMEOUT ${_aim_discovery_timeout})

# Слой программы: файлы игрока и все экраны — обход на окне без экрана,
# обоими шрифтами (AUD-49); шрифт — рядом с тестами, как у программы.
add_executable(aim_app_tests app/player_files_test.cpp app/screens_test.cpp)
target_link_libraries(aim_app_tests PRIVATE aim_app GTest::gtest_main)
aim_own_code(aim_app_tests)
aim_fonts_next_to(aim_app_tests)
gtest_discover_tests(aim_app_tests DISCOVERY_MODE PRE_TEST
                     DISCOVERY_TIMEOUT ${_aim_discovery_timeout})

# Программа целиком: три кадра без экрана и снимок последнего; проверка
# снимка — отдельный тест после неё. Драйвер offscreen есть в любой сборке
# SDL3 и не требует X-сервера. Рендер — программный, кадровый буфер — без
# ускорения: иначе SDL поднимает EGL и Mesa, а их утечки LeakSanitizer
# приписывает программе (так же у TrueJointTileMaker). Папка игрока — своя
# и пустая (D-30): первый запуск, туториал. --text-check: надпись меню или
# туториала не поместилась — код выхода 3, тест красный (D-43).
set(_aim_frame "${CMAKE_CURRENT_BINARY_DIR}/app_frame.bmp")
add_test(NAME app_data_reset COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CMAKE_CURRENT_BINARY_DIR}/app_data")
set_tests_properties(app_data_reset PROPERTIES FIXTURES_SETUP app_data)
add_test(NAME app_offscreen_frames
         COMMAND AimTrainer --frames 3 --screenshot "${_aim_frame}" --text-check)
set_tests_properties(app_offscreen_frames PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_RENDER_DRIVER=software;SDL_FRAMEBUFFER_ACCELERATION=0;SDL_AUDIO_DRIVER=dummy;AIM_TRAINER_DATA_DIR=app_data"
    FIXTURES_REQUIRED app_data
    FIXTURES_SETUP app_frame)

add_executable(aim_frame_check app/frame_check.cpp)
target_link_libraries(aim_frame_check PRIVATE aim_core SDL3::SDL3)
aim_own_code(aim_frame_check)
add_test(NAME app_frame_shows_tutorial COMMAND aim_frame_check "${_aim_frame}")
set_tests_properties(app_frame_shows_tutorial PROPERTIES FIXTURES_REQUIRED app_frame)
