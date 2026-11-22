#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "comm/can_bus.hpp"
#include "comm/can_frame.hpp"
#include "core/clock.hpp"

namespace cvc::test {

class CFakeCanBus final : public comm::ICanBus {
public:
    static constexpr std::string_view kInterfaceName = "fake0";
    static constexpr std::size_t kReceiveCapacity = 32U;
    static constexpr std::size_t kTransmitCapacity = 32U;

    explicit CFakeCanBus(core::IClock& clock);

    CFakeCanBus(const CFakeCanBus&) = delete;
    CFakeCanBus& operator=(const CFakeCanBus&) = delete;
    CFakeCanBus(CFakeCanBus&&) = delete;
    CFakeCanBus& operator=(CFakeCanBus&&) = delete;
    ~CFakeCanBus() override = default;

    [[nodiscard]] comm::EOpenResult open(std::string_view interfaceName, comm::EBusMode mode) override;
    void close() noexcept override;
    [[nodiscard]] comm::ESendResult send(const comm::SCanFrame& frame) override;
    [[nodiscard]] comm::EReceiveResult receive(comm::SReceivedFrame& received, core::TDuration timeout) override;
    [[nodiscard]] std::uint64_t droppedFrames() const noexcept override;

    void deliver(const comm::SCanFrame& frame);
    [[nodiscard]] bool takeTransmitted(comm::SCanFrame& frame);
    void loseInterface();

private:
    core::IClock& clock_;
};

}  // namespace cvc::test
