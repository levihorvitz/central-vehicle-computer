#pragma once

#include <cstdint>
#include <string_view>

#include "comm/can_bus.hpp"
#include "comm/can_frame.hpp"
#include "core/clock.hpp"

namespace cvc::comm {

class CSocketCanBus final : public ICanBus {
public:
    explicit CSocketCanBus(core::IClock& clock);

    CSocketCanBus(const CSocketCanBus&) = delete;
    CSocketCanBus& operator=(const CSocketCanBus&) = delete;
    CSocketCanBus(CSocketCanBus&&) = delete;
    CSocketCanBus& operator=(CSocketCanBus&&) = delete;
    ~CSocketCanBus() override;

    [[nodiscard]] EOpenResult open(std::string_view interfaceName, EBusMode mode) override;
    void close() noexcept override;
    [[nodiscard]] ESendResult send(const SCanFrame& frame) override;
    [[nodiscard]] EReceiveResult receive(SReceivedFrame& received, core::TDuration timeout) override;
    [[nodiscard]] std::uint64_t droppedFrames() const noexcept override;

private:
    core::IClock& clock_;
};

}  // namespace cvc::comm
