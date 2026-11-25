#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include <gtest/gtest.h>

#include "comm/can_bus.hpp"
#include "comm/can_frame.hpp"
#include "fakes/fake_can_bus.hpp"
#include "fakes/fake_clock.hpp"
#include "support/can_bus_contract.hpp"
#include "support/can_bus_harness.hpp"
#include "support/can_test_utils.hpp"

namespace cvc::test {
namespace {

using comm::EBusMode;
using comm::EOpenResult;
using comm::EReceiveResult;
using comm::ESendResult;
using comm::SCanFrame;
using comm::SReceivedFrame;

constexpr core::TDuration kShortTimeout = std::chrono::milliseconds(1);

class CFakeCanBusHarness final : public ICanBusHarness {
public:
    CFakeCanBusHarness() : bus_(clock_) {}

    [[nodiscard]]
    comm::ICanBus& bus() override {
        return bus_;
    }
    [[nodiscard]]
    std::string_view interfaceName() const override {
        return CFakeCanBus::kInterfaceName;
    }
    [[nodiscard]]
    std::string_view missingInterfaceName() const override {
        return "missing0";
    }
    [[nodiscard]]
    CFakeClock& clock() override {
        return clock_;
    }

    void deliver(const SCanFrame& frame) override { bus_.deliver(frame); }
    [[nodiscard]]
    bool takeTransmitted(SCanFrame& frame) override {
        return bus_.takeTransmitted(frame);
    }

private:
    CFakeClock clock_;
    CFakeCanBus bus_;
};

std::unique_ptr<ICanBusHarness> makeFakeCanBusHarness() {
    return std::make_unique<CFakeCanBusHarness>();
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(Fake, CCanBusContractTest, ::testing::Values(&makeFakeCanBusHarness));

// --- Behaviour that only the fake can produce on demand ---

// SW_CAN_013
TEST(FakeCanBusTest, CountsFramesBeyondReceiveCapacityAsDroppedAndKeepsTheOldest) {
    CFakeClock clock;
    CFakeCanBus bus(clock);
    ASSERT_EQ(bus.open(CFakeCanBus::kInterfaceName, EBusMode::kSimulation), EOpenResult::kOpened);

    for (std::size_t i = 0U; i < CFakeCanBus::kReceiveCapacity + 2U; ++i) {
        bus.deliver(makeFrame(static_cast<std::uint32_t>(i)));
    }

    EXPECT_EQ(bus.droppedFrames(), 2U);
    for (std::size_t i = 0U; i < CFakeCanBus::kReceiveCapacity; ++i) {
        SReceivedFrame received;
        ASSERT_EQ(bus.receive(received, kShortTimeout), EReceiveResult::kReceived);
        EXPECT_EQ(received.frame.id, static_cast<std::uint32_t>(i));
    }
    SReceivedFrame received;
    EXPECT_EQ(bus.receive(received, kShortTimeout), EReceiveResult::kTimeout);
}

// SW_CAN_013
TEST(FakeCanBusTest, ResetsDroppedCountWhenOpenedAgain) {
    CFakeClock clock;
    CFakeCanBus bus(clock);
    ASSERT_EQ(bus.open(CFakeCanBus::kInterfaceName, EBusMode::kSimulation), EOpenResult::kOpened);
    for (std::size_t i = 0U; i < CFakeCanBus::kReceiveCapacity + 1U; ++i) {
        bus.deliver(makeFrame(0x100U));
    }
    ASSERT_EQ(bus.droppedFrames(), 1U);

    bus.close();
    ASSERT_EQ(bus.open(CFakeCanBus::kInterfaceName, EBusMode::kSimulation), EOpenResult::kOpened);

    EXPECT_EQ(bus.droppedFrames(), 0U);
}

// SW_CAN_006, SW_CAN_016
TEST(FakeCanBusTest, ReportsBufferFullWithoutWaiting) {
    CFakeClock clock;
    CFakeCanBus bus(clock);
    ASSERT_EQ(bus.open(CFakeCanBus::kInterfaceName, EBusMode::kSimulation), EOpenResult::kOpened);
    for (std::size_t i = 0U; i < CFakeCanBus::kTransmitCapacity; ++i) {
        ASSERT_EQ(bus.send(makeFrame(0x100U)), ESendResult::kAccepted);
    }
    const core::TTimePoint before = clock.now();

    EXPECT_EQ(bus.send(makeFrame(0x100U)), ESendResult::kBufferFull);
    EXPECT_EQ(clock.now(), before);

    SCanFrame transmitted;
    ASSERT_TRUE(bus.takeTransmitted(transmitted));
    EXPECT_EQ(bus.send(makeFrame(0x100U)), ESendResult::kAccepted);
}

// SW_CAN_017
TEST(FakeCanBusTest, ReportsInterfaceLostOnSendAndReceive) {
    CFakeClock clock;
    CFakeCanBus bus(clock);
    ASSERT_EQ(bus.open(CFakeCanBus::kInterfaceName, EBusMode::kSimulation), EOpenResult::kOpened);

    bus.loseInterface();

    EXPECT_EQ(bus.send(makeFrame(0x100U)), ESendResult::kInterfaceLost);
    SReceivedFrame received;
    EXPECT_EQ(bus.receive(received, kShortTimeout), EReceiveResult::kInterfaceLost);
    SCanFrame transmitted;
    EXPECT_FALSE(bus.takeTransmitted(transmitted));
}

}  // namespace cvc::test
