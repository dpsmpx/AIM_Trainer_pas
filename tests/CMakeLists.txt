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

add_executable(aim_core_tests core/click_time_test.cpp)
target_link_libraries(aim_core_tests PRIVATE aim_core GTest::gtest_main)
aim_own_code(aim_core_tests)
gtest_discover_tests(aim_core_tests DISCOVERY_MODE PRE_TEST
                     DISCOVERY_TIMEOUT ${_aim_discovery_timeout})

add_executable(aim_platform_tests platform/sdl_version_test.cpp platform/raw_mouse_test.cpp)
target_link_libraries(aim_platform_tests PRIVATE aim_platform GTest::gtest_main)
aim_own_code(aim_platform_tests)
gtest_discover_tests(aim_platform_tests DISCOVERY_MODE PRE_TEST
                     DISCOVERY_TIMEOUT ${_aim_discovery_timeout})

# Программа целиком: три кадра без экрана и снимок последнего; проверка
# снимка — отдельный тест после неё. Драйвер offscreen есть в любой сборке
# SDL3 и не требует X-сервера. Рендер — программный, кадровый буфер — без
# ускорения: иначе SDL поднимает EGL и Mesa, а их утечки LeakSanitizer
# приписывает программе (так же у TrueJointTileMaker).
set(_aim_frame "${CMAKE_CURRENT_BINARY_DIR}/app_frame.bmp")
add_test(NAME app_offscreen_frames
         COMMAND AimTrainer --frames 3 --screenshot "${_aim_frame}")
set_tests_properties(app_offscreen_frames PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_RENDER_DRIVER=software;SDL_FRAMEBUFFER_ACCELERATION=0"
    FIXTURES_SETUP app_frame)

add_executable(aim_frame_check app/frame_check.cpp)
target_link_libraries(aim_frame_check PRIVATE aim_core SDL3::SDL3)
aim_own_code(aim_frame_check)
add_test(NAME app_frame_is_background COMMAND aim_frame_check "${_aim_frame}")
set_tests_properties(app_frame_is_background PROPERTIES FIXTURES_REQUIRED app_frame)
