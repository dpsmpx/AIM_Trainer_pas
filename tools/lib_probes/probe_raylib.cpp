// Замерщик ввода на raylib (docs/LIB_PROBES.md). Времени события нет, а
// нажатие видно только как состояние кнопки: его обновляет PollInputEvents в
// конце EndDrawing прошлого кадра (src/rcore.c, src/platforms/rcore_desktop_glfw.c).
// Программа узнаёт о клике, когда в начале кадра спрашивает IsMouseButtonPressed.
#include "raylib.h"

#include <optional>
#include <string>

#include "probe_common.h"

namespace {

int to_probe_button(int b) {
    switch (b) {
        case MOUSE_BUTTON_LEFT: return 1;
        case MOUSE_BUTTON_RIGHT: return 2;
        case MOUSE_BUTTON_MIDDLE: return 3;
        default: return 0;
    }
}

}  // namespace

int main() {
    SetTraceLogLevel(LOG_WARNING);
    // Без FLAG_VSYNC_HINT и SetTargetFPS: кадры без ожидания и без ограничения.
    InitWindow(probe::kWidth, probe::kHeight, "raylib");

    probe::LibInfo info;
    info.name = "raylib";
    info.version = RAYLIB_VERSION;
    info.renderer = "OpenGL (rlgl поверх GLFW)";
    info.timestamp = "нет: IsMouseButtonPressed — состояние, обновлённое в EndDrawing прошлого кадра";
    probe::Session session(info, GetWindowHandle());

    while (!WindowShouldClose()) {  // Esc закрывает окно
        const probe::Ns t = probe::now();
        for (int b : {MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE})
            if (IsMouseButtonPressed(b))
                session.click(to_probe_button(b), static_cast<float>(GetMouseX()),
                              static_cast<float>(GetMouseY()), t, std::nullopt);

        const probe::View v = session.frame();
        BeginDrawing();
        ClearBackground(Color{28, 28, 32, 255});
        if (v.target) DrawCircleV(Vector2{v.x, v.y}, v.r, Color{97, 207, 255, 255});
        DrawRectangleV(Vector2{0, 0}, Vector2{v.progress * static_cast<float>(probe::kWidth), 6},
                       v.selftest ? Color{255, 200, 40, 255} : Color{97, 207, 255, 255});
        const std::string title = session.take_title();
        if (!title.empty()) SetWindowTitle(title.c_str());
        const probe::Ns call = probe::now();
        EndDrawing();  // показ кадра и опрос ввода
        session.presented(call, probe::now());
    }
    session.finish();
    CloseWindow();
    return 0;
}
