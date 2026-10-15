// Системы без своего потока сырого ввода (D-24): время клика — из события SDL.

#include "platform/raw_mouse.h"

namespace aim::platform {

struct RawMouse::Impl {
    std::string problem = "свой поток сырого ввода — только на Windows (D-24)";
};

RawMouse::RawMouse() : impl_(std::make_unique<Impl>()) {}

RawMouse::~RawMouse() = default;

bool RawMouse::active() const {
    return false;
}

const std::string& RawMouse::problem() const {
    return impl_->problem;
}

void RawMouse::drain(std::vector<core::RawPress>& /*out*/) {}

}  // namespace aim::platform
