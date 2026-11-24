#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
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

    [[nodiscard]]
    comm::EOpenResult open(std::string_view interfaceName, comm::EBusMode mode) override;
    void close() noexcept override;
    [[nodiscard]]
    comm::ESendResult send(const comm::SCanFrame& frame) override;
    [[nodiscard]]
    comm::EReceiveResult receive(comm::SReceivedFrame& received, core::TDuration timeout) override;
    [[nodiscard]]
    std::uint64_t droppedFrames() const noexcept override;

    void deliver(const comm::SCanFrame& frame);
    [[nodiscard]]
    bool takeTransmitted(comm::SCanFrame& frame);
    void loseInterface();

private:
    template <std::size_t Capacity>
    class CFrameRing {
    public:
        [[nodiscard]]
        bool push(const comm::SCanFrame& frame) {
            if (count_ == Capacity) {
                return false;
            }
            frames_[(head_ + count_) % Capacity] = frame;
            ++count_;
            return true;
        }

        [[nodiscard]]
        bool pop(comm::SCanFrame& frame) {
            if (count_ == 0U) {
                return false;
            }
            frame = frames_[head_];
            head_ = (head_ + 1U) % Capacity;
            --count_;
            return true;
        }

        void clear() {
            head_ = 0U;
            count_ = 0U;
        }

    private:
        std::array<comm::SCanFrame, Capacity> frames_{};
        std::size_t head_ = 0U;
        std::size_t count_ = 0U;
    };

    [[nodiscard]]
    bool popReceived(comm::SReceivedFrame& received);
    [[nodiscard]]
    static bool isObdRequestId(std::uint32_t id);

    core::IClock& clock_;
    mutable std::mutex mutex_;
    bool open_ = false;
    bool interfaceLost_ = false;
    comm::EBusMode mode_ = comm::EBusMode::kSimulation;
    CFrameRing<kReceiveCapacity> received_;
    CFrameRing<kTransmitCapacity> transmitted_;
    std::atomic<std::uint64_t> dropped_{0U};
};

}  // namespace cvc::test
