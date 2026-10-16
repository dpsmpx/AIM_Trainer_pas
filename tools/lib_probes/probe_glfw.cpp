// Замерщик ввода на GLFW (docs/LIB_PROBES.md). Времени события GLFW не даёт:
// колбэк кнопки вызывается из glfwPollEvents, который разбирает очередь окна
// (PeekMessage в src/win32_window.c), — момент вызова и есть «получено».
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <cmath>
#include <numbers>
#include <optional>
#include <string>

#include "probe_common.h"

namespace {

probe::Session* g_session = nullptr;

int to_probe_button(int b) {
    switch (b) {
        case GLFW_MOUSE_BUTTON_LEFT: return 1;
        case GLFW_MOUSE_BUTTON_RIGHT: return 2;
        case GLFW_MOUSE_BUTTON_MIDDLE: return 3;
        default: return 0;
    }
}

void on_button(GLFWwindow* w, int button, int action, int) {
    if (action != GLFW_PRESS || !g_session) return;
    const probe::Ns t = probe::now();
    double x = 0, y = 0;
    glfwGetCursorPos(w, &x, &y);
    g_session->click(to_probe_button(button), static_cast<float>(x), static_cast<float>(y), t,
                     std::nullopt);
}

bool g_escape = false;

void on_key(GLFWwindow* w, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        g_escape = true;
        glfwSetWindowShouldClose(w, GLFW_TRUE);
    }
}

void fill_circle(float cx, float cy, float r) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 64; ++i) {
        const float a = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / 64.0f;
        glVertex2f(cx + r * std::cos(a), cy + r * std::sin(a));
    }
    glEnd();
}

}  // namespace

int main() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(probe::kWidth, probe::kHeight, "GLFW", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);  // без ожидания обновления экрана

    probe::LibInfo info;
    info.name = "GLFW";
    info.version = glfwGetVersionString();
    info.renderer = std::string("OpenGL: ") + reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    info.timestamp = "нет: колбэк кнопки из glfwPollEvents, времени события GLFW не даёт";
    probe::Session session(info, glfwGetWin32Window(window));
    g_session = &session;
    glfwSetMouseButtonCallback(window, on_button);
    glfwSetKeyCallback(window, on_key);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        const probe::View v = session.frame();
        int fw = 0, fh = 0;
        glfwGetFramebufferSize(window, &fw, &fh);
        glViewport(0, 0, fw, fh);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, probe::kWidth, probe::kHeight, 0, -1, 1);
        glClearColor(probe::kBackground.r / 255.0f, probe::kBackground.g / 255.0f,
                     probe::kBackground.b / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (v.target) {
            glColor3ub(probe::kTarget.r, probe::kTarget.g, probe::kTarget.b);
            fill_circle(v.x, v.y, v.r);
        }
        glColor3ub(v.bar.r, v.bar.g, v.bar.b);
        glRectf(0, 0, v.progress * static_cast<float>(probe::kWidth), v.bar_h);
        const std::string title = session.take_title();
        if (!title.empty()) glfwSetWindowTitle(window, title.c_str());
        const probe::Ns call = probe::now();
        glfwSwapBuffers(window);
        session.presented(call, probe::now());
    }
    session.note(g_escape ? "выход: Esc" : "выход: окно закрыто");
    session.finish();
    g_session = nullptr;
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
