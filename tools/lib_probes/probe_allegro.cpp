// Замерщик ввода на Allegro 5 (docs/LIB_PROBES.md). Окно Allegro живёт в своём
// потоке (d3d_display_thread_proc в src/win/d3d_disp.cpp), и событие мыши
// получает метку al_get_time — QueryPerformanceCounter — в этом потоке, когда
// пришло WM_LBUTTONDOWN (src/win/wmouse.c), а не когда программа его забрала.
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_windows.h>

#include <format>
#include <string>

#include "probe_common.h"

int main(int, char*[]) {
    if (!al_init() || !al_install_mouse() || !al_install_keyboard() ||
        !al_init_primitives_addon())
        return 1;
    al_set_new_display_option(ALLEGRO_VSYNC, 2, ALLEGRO_SUGGEST);  // 2 — без ожидания экрана
    ALLEGRO_DISPLAY* display = al_create_display(probe::kWidth, probe::kHeight);
    if (!display) return 1;
    ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
    al_register_event_source(queue, al_get_display_event_source(display));
    al_register_event_source(queue, al_get_mouse_event_source());
    al_register_event_source(queue, al_get_keyboard_event_source());

    const uint32_t ver = al_get_allegro_version();
    probe::LibInfo info;
    info.name = "Allegro";
    info.version = std::format("{}.{}.{}.{}", ver >> 24, (ver >> 16) & 255, (ver >> 8) & 255,
                               ver & 255);
    info.renderer = (al_get_display_flags(display) & ALLEGRO_OPENGL) ? "OpenGL" : "Direct3D 9";
    info.timestamp = "с al_get_time (QueryPerformanceCounter) в потоке окна Allegro при WM_*BUTTONDOWN";
    probe::Session session(info, al_get_win_window_handle(display));

    bool running = true;
    while (running) {
        ALLEGRO_EVENT ev;
        while (al_get_next_event(queue, &ev)) {
            const double lib_now = al_get_time();
            const probe::Ns t = probe::now();
            switch (ev.type) {
                case ALLEGRO_EVENT_DISPLAY_CLOSE:
                    running = false;
                    break;
                case ALLEGRO_EVENT_KEY_DOWN:
                    if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) running = false;
                    break;
                case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN: {
                    const double age_s = lib_now - ev.mouse.timestamp;
                    const auto age = age_s > 0 ? static_cast<probe::Ns>(age_s * 1e9) : 0;
                    // Кнопки Allegro: 1 — левая, 2 — правая, 3 — средняя, как в ядре.
                    session.click(static_cast<int>(ev.mouse.button), static_cast<float>(ev.mouse.x),
                                  static_cast<float>(ev.mouse.y), t, probe::from_age(t, age));
                    break;
                }
                default:
                    break;
            }
        }

        const probe::View v = session.frame();
        al_clear_to_color(al_map_rgb(28, 28, 32));
        if (v.target) al_draw_filled_circle(v.x, v.y, v.r, al_map_rgb(97, 207, 255));
        al_draw_filled_rectangle(0, 0, v.progress * static_cast<float>(probe::kWidth), 6,
                                 v.selftest ? al_map_rgb(255, 200, 40) : al_map_rgb(97, 207, 255));
        const std::string title = session.take_title();
        if (!title.empty()) al_set_window_title(display, title.c_str());
        const probe::Ns call = probe::now();
        al_flip_display();
        session.presented(call, probe::now());
    }
    session.finish();
    al_destroy_event_queue(queue);
    al_destroy_display(display);
    return 0;
}
