// Проверка снимка кадра программы (tests/CMakeLists.txt, app_frame_shows_tutorial):
// окно 800×600 (D-02); первый запуск — поверх меню туториал: затемнение и
// панель в центре (RenderTutorialOverlay образца). Частицы меню случайны, но
// под двумя слоями почти не видны — сверка с допуском.
//
//   aim_frame_check файл.bmp   → 0 — снимок верен, 1 — нет, 2 — неверный вызов

#include <SDL3/SDL.h>

#include <cstdlib>

#include "core/screen.h"

namespace {

constexpr int kWidth = aim::core::kWindowWidth;
constexpr int kHeight = aim::core::kWindowHeight;

// Панель туториала (45, 45, 50) с прозрачностью 240 поверх затемнения
// (15, 15, 18; 225) поверх фона тёмной темы.
constexpr int kPanelR = 43;
constexpr int kPanelG = 43;
constexpr int kPanelB = 48;
constexpr int kTolerance = 3;

bool pixel_near(SDL_Surface* frame, int x, int y, int er, int eg, int eb) {
    Uint8 r = 0, g = 0, b = 0, a = 0;
    if (!SDL_ReadSurfacePixel(frame, x, y, &r, &g, &b, &a)) return false;
    if (std::abs(r - er) <= kTolerance && std::abs(g - eg) <= kTolerance &&
        std::abs(b - eb) <= kTolerance)
        return true;
    SDL_Log("пиксель (%d, %d): %u, %u, %u вместо ~%d, %d, %d", x, y, r, g, b, er, eg, eb);
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
    // Левый верхний угол панели 480×220 в центре окна, внутри скругления.
    ok = ok && pixel_near(frame, (kWidth - 480) / 2 + 10, (kHeight - 220) / 2 + 10, kPanelR,
                          kPanelG, kPanelB);
    SDL_DestroySurface(frame);
    return ok ? 0 : 1;
}
