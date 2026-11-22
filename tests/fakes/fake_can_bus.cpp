#include "fakes/fake_can_bus.hpp"

namespace cvc::test {

CFakeCanBus::CFakeCanBus(core::IClock& clock) : clock_(clock) {}

comm::EOpenResult CFakeCanBus::open(std::string_view interfaceName, comm::EBusMode mode) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (open_) {
        return comm::EOpenResult::kAlreadyOpen;
    }
    if (interfaceName != kInterfaceName || interfaceLost_) {
        return comm::EOpenResult::kFailed;
    }
    open_ = true;
    mode_ = mode;
    received_.clear();
    dropped_.store(0U);
    return comm::EOpenResult::kOpened;
}

void CFakeCanBus::close() noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    open_ = false;
    received_.clear();
}

comm::ESendResult CFakeCanBus::send(const comm::SCanFrame& frame) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (!open_) {
        return comm::ESendResult::kNotOpen;
    }
    if (interfaceLost_) {
        return comm::ESendResult::kInterfaceLost;
    }
    if (frame.id > comm::kMaxStandardId || frame.dlc > comm::kMaxCanDlc) {
        return comm::ESendResult::kInvalidFrame;
    }
    if (mode_ == comm::EBusMode::kRealVehicle && !isObdRequestId(frame.id)) {
        return comm::ESendResult::kNotPermitted;
    }
    if (!transmitted_.push(frame)) {
        return comm::ESendResult::kBufferFull;
    }
    return comm::ESendResult::kAccepted;
}

comm::EReceiveResult CFakeCanBus::receive(comm::SReceivedFrame& received, core::TDuration timeout) {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (!open_) {
            return comm::EReceiveResult::kNotOpen;
        }
        if (interfaceLost_) {
            return comm::EReceiveResult::kInterfaceLost;
        }
        if (popReceived(received)) {
            return comm::EReceiveResult::kReceived;
        }
    }

    clock_.sleepUntil(clock_.now() + timeout);

    const std::lock_guard<std::mutex> lock(mutex_);
    if (!open_) {
        return comm::EReceiveResult::kNotOpen;
    }
    if (interfaceLost_) {
        return comm::EReceiveResult::kInterfaceLost;
    }
    if (popReceived(received)) {
        return comm::EReceiveResult::kReceived;
    }
    return comm::EReceiveResult::kTimeout;
}

std::uint64_t CFakeCanBus::droppedFrames() const noexcept {
    return dropped_.load();
}

void CFakeCanBus::deliver(const comm::SCanFrame& frame) {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (!open_ || interfaceLost_) {
        return;
    }
    if (!received_.push(frame)) {
        dropped_.fetch_add(1U);
    }
}

bool CFakeCanBus::takeTransmitted(comm::SCanFrame& frame) {
    const std::lock_guard<std::mutex> lock(mutex_);
    return transmitted_.pop(frame);
}

void CFakeCanBus::loseInterface() {
    const std::lock_guard<std::mutex> lock(mutex_);
    interfaceLost_ = true;
}

bool CFakeCanBus::popReceived(comm::SReceivedFrame& received) {
    if (!received_.pop(received.frame)) {
        return false;
    }
    received.timestamp = clock_.now();
    return true;
}

bool CFakeCanBus::isObdRequestId(std::uint32_t id) {
    constexpr std::uint32_t kFunctionalRequestId = 0x7DFU;
    constexpr std::uint32_t kFirstPhysicalRequestId = 0x7E0U;
    constexpr std::uint32_t kLastPhysicalRequestId = 0x7E7U;
    return id == kFunctionalRequestId || (id >= kFirstPhysicalRequestId && id <= kLastPhysicalRequestId);
}

}  // namespace cvc::test
