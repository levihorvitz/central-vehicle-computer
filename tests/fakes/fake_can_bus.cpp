#include "fakes/fake_can_bus.hpp"

namespace cvc::test {

CFakeCanBus::CFakeCanBus(core::IClock& clock) : clock_(clock) {}

comm::EOpenResult CFakeCanBus::open(std::string_view /*interfaceName*/, comm::EBusMode /*mode*/) {
    return comm::EOpenResult::kFailed;
}

void CFakeCanBus::close() noexcept {}

comm::ESendResult CFakeCanBus::send(const comm::SCanFrame& /*frame*/) {
    return comm::ESendResult::kNotOpen;
}

comm::EReceiveResult CFakeCanBus::receive(comm::SReceivedFrame& /*received*/, core::TDuration /*timeout*/) {
    return comm::EReceiveResult::kNotOpen;
}

std::uint64_t CFakeCanBus::droppedFrames() const noexcept {
    return 0U;
}

void CFakeCanBus::deliver(const comm::SCanFrame& /*frame*/) {}

bool CFakeCanBus::takeTransmitted(comm::SCanFrame& /*frame*/) {
    return false;
}

void CFakeCanBus::loseInterface() {}

}  // namespace cvc::test
