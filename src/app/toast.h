#pragma once

// Уведомление вверху окна — перенос RenderToast образца: 3 с, появление и
// угасание по 300 мс. У образца новое уведомление сменяет прежнее; здесь
// они ждут очереди — при запуске их бывает несколько (ошибки файлов, D-30).

#include <deque>
#include <string>

#include "app/canvas.h"
#include "core/time.h"

namespace aim::app {

class Toasts {
public:
    void push(std::string text);
    /// Нарисовать текущее уведомление; кончилось — взять следующее.
    void render(Canvas& canvas, core::Nanoseconds now);

private:
    std::deque<std::string> queue_;
    std::string text_;
    core::Nanoseconds start_ = 0;
    bool active_ = false;
};

}  // namespace aim::app
