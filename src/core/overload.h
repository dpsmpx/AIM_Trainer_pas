#pragma once

// Перегрузка лямбд для std::visit: Overload{[](const A&) {…}, [](const B&) {…}}.

namespace aim::core {

template <class... F>
struct Overload : F... {
    using F::operator()...;
};
template <class... F>
Overload(F...) -> Overload<F...>;

}  // namespace aim::core
