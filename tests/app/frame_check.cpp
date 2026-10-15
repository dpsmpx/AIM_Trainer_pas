// Проверка снимка кадра программы (tests/CMakeLists.txt, app_frame_is_background):
// окно 800×600 (D-02), кадр залит фоном тёмной темы.
//
//   aim_frame_check файл.bmp   → 0 — снимок верен, 1 — нет, 2 — неверный вызов

#include <SDL3/SDL.h>

#include <initializer_list>

namespace {

constexpr int kWidth = 800;
constexpr int kHeight = 600;
constexpr Uint8 kR = 28, kG = 28, kB = 32;  // InitThemes в AIM_Trainer.pas, «Тёмная»

bool pixel_is_background(SDL_Surface* frame, int x, int y) {
    Uint8 r = 0, g = 0, b = 0, a = 0;
    if (!SDL_ReadSurfacePixel(frame, x, y, &r, &g, &b, &a)) return false;
    if (r == kR && g == kG && b == kB) return true;
    SDL_Log("пиксель (%d, %d): %u, %u, %u вместо %u, %u, %u", x, y, r, g, b, kR, kG, kB);
    return false;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        SDL_Log("использование: aim_frame_check файл.bmp");
        return 2;
    }
    SDL_Surface* frame = SDL_LoadBMP(argv[1]);
    if (frame == nullptr) {
        SDL_Log("снимок не прочитан: %s", SDL_GetError());
        return 1;
    }
    bool ok = frame->w == kWidth && frame->h == kHeight;
    if (!ok) SDL_Log("снимок %d×%d вместо %d×%d", frame->w, frame->h, kWidth, kHeight);
    for (const auto [x, y] : {SDL_Point{0, 0}, SDL_Point{kWidth - 1, kHeight - 1},
                              SDL_Point{kWidth / 2, kHeight / 2}}) {
        ok = ok && pixel_is_background(frame, x, y);
    }
    SDL_DestroySurface(frame);
    return ok ? 0 : 1;
}
