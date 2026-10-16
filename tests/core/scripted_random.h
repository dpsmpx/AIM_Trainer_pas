#pragma once

// Источник случайных чисел для тестов: отдаёт заданные значения по порядку.
// Значение между вызовами between() проверяется на попадание в границы — так
// тест ловит, что логика просит не тот диапазон.

#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <initializer_list>
#include <utility>

#include "core/random.h"

namespace aim::core::testing {

class ScriptedRandom final : public Random {
public:
    /// Следующие значения unit(); кончились — 0,5.
    ScriptedRandom& units(std::initializer_list<double> values) {
        units_.insert(units_.end(), values);
        return *this;
    }
    /// Следующие значения between(); кончились — нижняя граница.
    ScriptedRandom& ints(std::initializer_list<int> values) {
        ints_.insert(ints_.end(), values);
        return *this;
    }

    double unit() override {
        if (units_.empty()) return 0.5;
        const double value = units_.front();
        units_.pop_front();
        return value;
    }

    int between(int lo, int hi) override {
        if (lo > hi) std::swap(lo, hi);
        if (ints_.empty()) return lo;
        const int value = ints_.front();
        ints_.pop_front();
        EXPECT_GE(value, lo) << "between(" << lo << ", " << hi << ")";
        EXPECT_LE(value, hi) << "between(" << lo << ", " << hi << ")";
        return std::clamp(value, lo, hi);
    }

private:
    std::deque<double> units_;
    std::deque<int> ints_;
};

}  // namespace aim::core::testing
