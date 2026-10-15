// Замерщик ввода на SFML (docs/LIB_PROBES.md). Только модуль окна и OpenGL:
// событиям SFML время не ставит, pollEvent разбирает очередь окна
// (PeekMessageW в src/SFML/Window/Win32/WindowImplWin32.cpp).
#include <SFML/OpenGL.hpp>
#include <SFML/Window.hpp>

#include <cmath>
#include <format>
#include <numbers>
#include <optional>
#include <string>

#include "probe_common.h"

namespace {

int to_probe_button(sf::Mouse::Button b) {
    switch (b) {
        case sf::Mouse::Button::Left: return 1;
        case sf::Mouse::Button::Right: return 2;
        case sf::Mouse::Button::Middle: return 3;
        default: return 0;
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
    sf::Window window(sf::VideoMode({probe::kWidth, probe::kHeight}), "SFML",
                      sf::Style::Titlebar | sf::Style::Close);
    window.setVerticalSyncEnabled(false);
    if (!window.setActive(true)) return 1;

    probe::LibInfo info;
    info.name = "SFML";
    info.version = std::format("{}.{}.{}", SFML_VERSION_MAJOR, SFML_VERSION_MINOR,
                               SFML_VERSION_PATCH);
    info.renderer = std::string("OpenGL: ") + reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    info.timestamp = "нет: события из pollEvent (очередь окна), времени SFML не ставит";
    probe::Session session(info, window.getNativeHandle());

    bool running = true;
    while (running) {
        while (const std::optional event = window.pollEvent()) {
            const probe::Ns t = probe::now();
            if (event->is<sf::Event::Closed>()) {
                session.note("выход: окно закрыто");
                running = false;
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) {
                    session.note("выход: Esc");
                    running = false;
                }
            } else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                session.click(to_probe_button(mb->button), static_cast<float>(mb->position.x),
                              static_cast<float>(mb->position.y), t, std::nullopt);
            }
        }

        const probe::View v = session.frame();
        glViewport(0, 0, probe::kWidth, probe::kHeight);
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
        if (!title.empty()) window.setTitle(sf::String::fromUtf8(title.begin(), title.end()));
        const probe::Ns call = probe::now();
        window.display();
        session.presented(call, probe::now());
    }
    session.finish();
    return 0;
}
