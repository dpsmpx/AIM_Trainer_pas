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
    // В «показ кадра» у raylib входит весь EndDrawing: вывод пакета рисования,
    // смена кадра и опрос ввода (src/rcore.c) — отдельной функции показа нет.
    info.renderer = "OpenGL (rlgl поверх GLFW); показ = EndDrawing: отрисовка, смена кадра, опрос ввода";
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
        ClearBackground(Color{probe::kBackground.r, probe::kBackground.g, probe::kBackground.b, 255});
        if (v.target)
            DrawCircleV(Vector2{v.x, v.y}, v.r,
                        Color{probe::kTarget.r, probe::kTarget.g, probe::kTarget.b, 255});
        DrawRectangleV(Vector2{0, 0}, Vector2{v.progress * static_cast<float>(probe::kWidth), v.bar_h},
                       Color{v.bar.r, v.bar.g, v.bar.b, 255});
        const std::string title = session.take_title();
        if (!title.empty()) SetWindowTitle(title.c_str());
        const probe::Ns call = probe::now();
        EndDrawing();  // показ кадра и опрос ввода
        session.presented(call, probe::now());
    }
    session.note(IsKeyPressed(KEY_ESCAPE) ? "выход: Esc" : "выход: WindowShouldClose");
    session.finish();
    CloseWindow();
    return 0;
}
