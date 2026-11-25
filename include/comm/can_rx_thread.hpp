#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

#include "comm/can_bus.hpp"
#include "comm/can_rx_queue.hpp"
#include "comm/interface_status.hpp"
#include "core/clock.hpp"

namespace cvc::comm {

class CCanRxThread final {
public:
    static constexpr core::TDuration kReceiveTimeout = std::chrono::milliseconds(10);

    CCanRxThread(ICanBus& bus, TCanRxQueue& queue, CInterfaceStatus& status) noexcept;

    CCanRxThread(const CCanRxThread&) = delete;
    CCanRxThread& operator=(const CCanRxThread&) = delete;
    CCanRxThread(CCanRxThread&&) = delete;
    CCanRxThread& operator=(CCanRxThread&&) = delete;
    ~CCanRxThread();

    // [SW_CANRX_008]
    [[nodiscard]]
    bool start();

    // [SW_CANRX_008]
    void stop() noexcept;

    // [SW_CANRX_008] [SW_CANRX_010]
    [[nodiscard]]
    bool isRunning() const noexcept;

    // [SW_CANRX_004]
    [[nodiscard]]
    std::uint64_t discardedFrames() const noexcept;

private:
    void run() noexcept;

    ICanBus& bus_;
    TCanRxQueue& queue_;
    CInterfaceStatus& status_;
    std::thread thread_;
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> discarded_{0U};
};

}  // namespace cvc::comm
