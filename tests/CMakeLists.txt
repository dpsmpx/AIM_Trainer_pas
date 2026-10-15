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

add_executable(aim_core_tests core/click_time_test.cpp)
target_link_libraries(aim_core_tests PRIVATE aim_core GTest::gtest_main)
aim_own_code(aim_core_tests)
gtest_discover_tests(aim_core_tests DISCOVERY_MODE PRE_TEST)

add_executable(aim_platform_tests platform/sdl_version_test.cpp platform/raw_mouse_test.cpp)
target_link_libraries(aim_platform_tests PRIVATE aim_platform GTest::gtest_main)
aim_own_code(aim_platform_tests)
gtest_discover_tests(aim_platform_tests DISCOVERY_MODE PRE_TEST)

# Программа целиком: три кадра без экрана и снимок последнего; проверка
# снимка — отдельный тест после неё. Драйвер offscreen есть в любой сборке
# SDL3 и не требует X-сервера; ускорение кадрового буфера выключено, чтобы
# SDL не поднимал EGL (так же у TrueJointTileMaker).
set(_aim_frame "${CMAKE_CURRENT_BINARY_DIR}/app_frame.bmp")
add_test(NAME app_offscreen_frames
         COMMAND AimTrainer --frames 3 --screenshot "${_aim_frame}")
set_tests_properties(app_offscreen_frames PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_FRAMEBUFFER_ACCELERATION=0"
    FIXTURES_SETUP app_frame)

add_executable(aim_frame_check app/frame_check.cpp)
target_link_libraries(aim_frame_check PRIVATE SDL3::SDL3)
aim_own_code(aim_frame_check)
add_test(NAME app_frame_is_background COMMAND aim_frame_check "${_aim_frame}")
set_tests_properties(app_frame_is_background PROPERTIES FIXTURES_REQUIRED app_frame)
