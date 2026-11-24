#pragma once

#include <cstdint>
#include <string_view>

#include "comm/can_frame.hpp"
#include "core/clock.hpp"

namespace cvc::comm {

enum class EBusMode : std::uint8_t {
    kSimulation,
    kRealVehicle,
};

enum class EOpenResult : std::uint8_t {
    kOpened,
    kAlreadyOpen,
    kFailed,
};

enum class ESendResult : std::uint8_t {
    kAccepted,
    kNotOpen,
    kInvalidFrame,
    kNotPermitted,
    kBufferFull,
    kInterfaceLost,
};

enum class EReceiveResult : std::uint8_t {
    kReceived,
    kTimeout,
    kNotOpen,
    kInterfaceLost,
};

class ICanBus {
public:
    virtual ~ICanBus() = default;

    // [SW_CAN_003]
    [[nodiscard]]
    virtual EOpenResult open(std::string_view interfaceName, EBusMode mode) = 0;

    // [SW_CAN_005]
    virtual void close() noexcept = 0;

    // [SW_CAN_004, SW_CAN_006, SW_CAN_007, SW_CAN_009, SW_CAN_016, SW_CAN_017]
    [[nodiscard]]
    virtual ESendResult send(const SCanFrame& frame) = 0;

    // [SW_CAN_004, SW_CAN_008, SW_CAN_010, SW_CAN_012, SW_CAN_017]
    [[nodiscard]]
    virtual EReceiveResult receive(SReceivedFrame& received, core::TDuration timeout) = 0;

    // [SW_CAN_013]
    [[nodiscard]]
    virtual std::uint64_t droppedFrames() const noexcept = 0;
};

}  // namespace cvc::comm
