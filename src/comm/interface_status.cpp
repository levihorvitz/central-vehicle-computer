#include <atomic>

#include "comm/interface_status.hpp"

namespace cvc::comm {

EInterfaceStatus CInterfaceStatus::get() const noexcept {
    return status_.load(std::memory_order_relaxed);
}

void CInterfaceStatus::markLost() noexcept {
    status_.store(EInterfaceStatus::kLost, std::memory_order_relaxed);
}

}  // namespace cvc::comm
