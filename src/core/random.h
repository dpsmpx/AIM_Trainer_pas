#pragma once

// Случайные числа логики. Источник — интерфейс: программа берёт генератор
// стандартной библиотеки, тесты — заданную последовательность.
//
// Семантика — как у PascalABC.NET, на которой написан образец
// (docs/ARCHITECTURE.md, §11): Random — вещественное в [0, 1), Random(a, b) —
// целое в [a, b] включительно, при a > b границы меняются местами.

#include <cstdint>
#include <functional>
#include <memory>
#include <random>

namespace aim::core {

class Random {
public:
    Random() = default;
    Random(const Random&) = delete;
    Random& operator=(const Random&) = delete;
    Random(Random&&) = delete;
    Random& operator=(Random&&) = delete;
    virtual ~Random() = default;

    /// Вещественное в [0, 1) — Random без аргументов.
    virtual double unit() = 0;

    /// Целое в [lo, hi] включительно; при lo > hi границы меняются местами —
    /// Random(a, b).
    virtual int between(int lo, int hi) = 0;
};

/// mt19937_64 и свои отображения в числа: по одному началу — одни и те же
/// числа на всех системах (D-39).
class EngineRandom final : public Random {
public:
    explicit EngineRandom(std::uint64_t seed);

    double unit() override;
    int between(int lo, int hi) override;

private:
    std::mt19937_64 engine_;
};

/// Генератор случайных чисел замера по его началу.
using RandomFactory = std::function<std::unique_ptr<Random>(std::uint64_t seed)>;
/// EngineRandom(seed).
std::unique_ptr<Random> engine_random(std::uint64_t seed);

}  // namespace aim::core
