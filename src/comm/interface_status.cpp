#include "comm/interface_status.hpp"

namespace cvc::comm {

EInterfaceStatus CInterfaceStatus::get() const noexcept {
    return EInterfaceStatus::kAvailable;
}

void CInterfaceStatus::markLost() noexcept {}

}  // namespace cvc::comm
