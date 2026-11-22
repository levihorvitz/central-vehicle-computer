#include "comm/socket_can_bus.hpp"

namespace cvc::comm {

CSocketCanBus::CSocketCanBus(core::IClock& clock) : clock_(clock) {}

CSocketCanBus::~CSocketCanBus() {
    close();
}

EOpenResult CSocketCanBus::open(std::string_view /*interfaceName*/, EBusMode /*mode*/) {
    return EOpenResult::kFailed;
}

void CSocketCanBus::close() noexcept {}

ESendResult CSocketCanBus::send(const SCanFrame& /*frame*/) {
    return ESendResult::kNotOpen;
}

EReceiveResult CSocketCanBus::receive(SReceivedFrame& /*received*/, core::TDuration /*timeout*/) {
    return EReceiveResult::kNotOpen;
}

std::uint64_t CSocketCanBus::droppedFrames() const noexcept {
    return 0U;
}

}  // namespace cvc::comm
