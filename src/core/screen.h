#pragma once

// Размер окна — общий для программы и её проверок (AUD-22: константы не
// дублируются).

namespace aim::core {

/// Окно фиксированного размера: раскладка интерфейса — в пикселях под него (D-02).
constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;

}  // namespace aim::core
