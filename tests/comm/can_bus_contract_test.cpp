#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

#include <gtest/gtest.h>

#include "comm/can_bus.hpp"
#include "comm/can_frame.hpp"
#include "support/allocation_counter.hpp"
#include "support/can_bus_contract.hpp"
#include "support/can_test_utils.hpp"

namespace cvc::test {
namespace {

using comm::EBusMode;
using comm::EOpenResult;
using comm::EReceiveResult;
using comm::ESendResult;
using comm::SCanFrame;
using comm::SReceivedFrame;

// Where no frame is expected: keeps those tests fast.
constexpr core::TDuration kShortTimeout = std::chrono::milliseconds(1);
// Where a frame is expected: on a real socket it passes through the kernel first.
constexpr core::TDuration kFrameTimeout = std::chrono::milliseconds(100);

}  // namespace

// --- Open and close ---

// SW_CAN_003
TEST_P(CCanBusContractTest, OpensExistingInterface) {
    EXPECT_EQ(bus().open(harness_->interfaceName(), EBusMode::kSimulation), EOpenResult::kOpened);
}

// SW_CAN_003
TEST_P(CCanBusContractTest, FailsToOpenMissingInterface) {
    EXPECT_EQ(bus().open(harness_->missingInterfaceName(), EBusMode::kSimulation), EOpenResult::kFailed);
}

// SW_CAN_003, SW_CAN_009
TEST_P(CCanBusContractTest, SecondOpenFailsAndKeepsRealVehicleMode) {
    ASSERT_TRUE(openBus(EBusMode::kRealVehicle));

    EXPECT_EQ(bus().open(harness_->interfaceName(), EBusMode::kSimulation), EOpenResult::kAlreadyOpen);
    EXPECT_EQ(bus().send(makeFrame(0x100U)), ESendResult::kNotPermitted);
    EXPECT_TRUE(nothingTransmitted());
}

// SW_CAN_003
TEST_P(CCanBusContractTest, SecondOpenFailsAndKeepsSimulationMode) {
    ASSERT_TRUE(openBus(EBusMode::kSimulation));

    EXPECT_EQ(bus().open(harness_->interfaceName(), EBusMode::kRealVehicle), EOpenResult::kAlreadyOpen);
    EXPECT_EQ(bus().send(makeFrame(0x100U)), ESendResult::kAccepted);
}

// SW_CAN_003, SW_CAN_005
TEST_P(CCanBusContractTest, OpensAgainAfterClose) {
    ASSERT_TRUE(openBus());
    bus().close();

    EXPECT_EQ(bus().open(harness_->interfaceName(), EBusMode::kSimulation), EOpenResult::kOpened);
}

// SW_CAN_004
TEST_P(CCanBusContractTest, SendFailsBeforeOpen) {
    EXPECT_EQ(bus().send(makeFrame(0x100U)), ESendResult::kNotOpen);
    EXPECT_TRUE(nothingTransmitted());
}

// SW_CAN_004
TEST_P(CCanBusContractTest, ReceiveFailsBeforeOpen) {
    SReceivedFrame received;
    EXPECT_EQ(bus().receive(received, kShortTimeout), EReceiveResult::kNotOpen);
}

// SW_CAN_004, SW_CAN_005
TEST_P(CCanBusContractTest, SendFailsAfterClose) {
    ASSERT_TRUE(openBus());
    bus().close();

    EXPECT_EQ(bus().send(makeFrame(0x100U)), ESendResult::kNotOpen);
    EXPECT_TRUE(nothingTransmitted());
}

// SW_CAN_004, SW_CAN_005
TEST_P(CCanBusContractTest, ReceiveFailsAfterCloseEvenIfFramesArrived) {
    ASSERT_TRUE(openBus());
    harness_->deliver(makeFrame(0x100U));
    bus().close();

    SReceivedFrame received;
    EXPECT_EQ(bus().receive(received, kShortTimeout), EReceiveResult::kNotOpen);
}

// SW_CAN_005
TEST_P(CCanBusContractTest, CloseWithoutOpenHasNoEffect) {
    bus().close();

    EXPECT_EQ(bus().open(harness_->interfaceName(), EBusMode::kSimulation), EOpenResult::kOpened);
}

// SW_CAN_005
TEST_P(CCanBusContractTest, SecondCloseHasNoEffect) {
    ASSERT_TRUE(openBus());
    bus().close();
    bus().close();

    EXPECT_EQ(bus().open(harness_->interfaceName(), EBusMode::kSimulation), EOpenResult::kOpened);
}

// --- Send ---

// SW_CAN_001, SW_CAN_006
TEST_P(CCanBusContractTest, TransmitsFrameUnchanged) {
    ASSERT_TRUE(openBus());
    const SCanFrame sent = makeFrame(0x123U);

    EXPECT_EQ(bus().send(sent), ESendResult::kAccepted);

    SCanFrame transmitted;
    ASSERT_TRUE(harness_->takeTransmitted(transmitted));
    EXPECT_TRUE(sameFrame(transmitted, sent));
}

// SW_CAN_007
TEST_P(CCanBusContractTest, AcceptsLowestAndHighestStandardId) {
    ASSERT_TRUE(openBus());

    EXPECT_EQ(bus().send(makeFrame(0x000U)), ESendResult::kAccepted);
    EXPECT_EQ(bus().send(makeFrame(comm::kMaxStandardId)), ESendResult::kAccepted);

    SCanFrame transmitted;
    ASSERT_TRUE(harness_->takeTransmitted(transmitted));
    EXPECT_EQ(transmitted.id, 0x000U);
    ASSERT_TRUE(harness_->takeTransmitted(transmitted));
    EXPECT_EQ(transmitted.id, comm::kMaxStandardId);
}

// SW_CAN_007
TEST_P(CCanBusContractTest, RejectsIdAboveStandardRange) {
    ASSERT_TRUE(openBus());

    EXPECT_EQ(bus().send(makeFrame(comm::kMaxStandardId + 1U)), ESendResult::kInvalidFrame);
    EXPECT_TRUE(nothingTransmitted());
}

// SW_CAN_001, SW_CAN_007
TEST_P(CCanBusContractTest, AcceptsDlcZero) {
    ASSERT_TRUE(openBus());

    EXPECT_EQ(bus().send(makeFrame(0x100U, 0U)), ESendResult::kAccepted);

    SCanFrame transmitted;
    ASSERT_TRUE(harness_->takeTransmitted(transmitted));
    EXPECT_EQ(transmitted.dlc, 0U);
}

// SW_CAN_007
TEST_P(CCanBusContractTest, RejectsDlcAboveEight) {
    ASSERT_TRUE(openBus());

    EXPECT_EQ(bus().send(makeFrame(0x100U, comm::kMaxCanDlc + 1U)), ESendResult::kInvalidFrame);
    EXPECT_TRUE(nothingTransmitted());
}

// SW_CAN_009
TEST_P(CCanBusContractTest, RealVehicleModeTransmitsObdRequestIds) {
    ASSERT_TRUE(openBus(EBusMode::kRealVehicle));

    for (const std::uint32_t id : {0x7DFU, 0x7E0U, 0x7E7U}) {
        SCOPED_TRACE(id);
        EXPECT_EQ(bus().send(makeFrame(id)), ESendResult::kAccepted);

        SCanFrame transmitted;
        ASSERT_TRUE(harness_->takeTransmitted(transmitted));
        EXPECT_EQ(transmitted.id, id);
    }
}

// SW_CAN_009
TEST_P(CCanBusContractTest, RealVehicleModeRejectsOtherIds) {
    ASSERT_TRUE(openBus(EBusMode::kRealVehicle));

    for (const std::uint32_t id : {0x7DEU, 0x7E8U, 0x000U, 0x100U}) {
        SCOPED_TRACE(id);
        EXPECT_EQ(bus().send(makeFrame(id)), ESendResult::kNotPermitted);
    }
    EXPECT_TRUE(nothingTransmitted());
}

// SW_CAN_009
TEST_P(CCanBusContractTest, SimulationModeTransmitsAnyStandardId) {
    ASSERT_TRUE(openBus(EBusMode::kSimulation));

    EXPECT_EQ(bus().send(makeFrame(0x100U)), ESendResult::kAccepted);
    EXPECT_EQ(bus().send(makeFrame(0x7E8U)), ESendResult::kAccepted);
}

// --- Receive ---

// SW_CAN_001
TEST_P(CCanBusContractTest, ReceivesFrameUnchanged) {
    ASSERT_TRUE(openBus());
    const SCanFrame sent = makeFrame(0x321U);
    harness_->deliver(sent);

    SReceivedFrame received;
    ASSERT_EQ(bus().receive(received, kFrameTimeout), EReceiveResult::kReceived);
    EXPECT_TRUE(sameFrame(received.frame, sent));
}

// SW_CAN_008
TEST_P(CCanBusContractTest, ReportsTimeoutWhenNoFrameArrives) {
    ASSERT_TRUE(openBus());

    SReceivedFrame received;
    EXPECT_EQ(bus().receive(received, kShortTimeout), EReceiveResult::kTimeout);
}

// SW_CAN_010
TEST_P(CCanBusContractTest, DeliversFramesInOrderOfReception) {
    ASSERT_TRUE(openBus());
    for (const std::uint32_t id : {0x101U, 0x102U, 0x103U}) {
        harness_->deliver(makeFrame(id));
    }

    for (const std::uint32_t id : {0x101U, 0x102U, 0x103U}) {
        SCOPED_TRACE(id);
        SReceivedFrame received;
        ASSERT_EQ(bus().receive(received, kFrameTimeout), EReceiveResult::kReceived);
        EXPECT_EQ(received.frame.id, id);
    }
}

// SW_CAN_012
TEST_P(CCanBusContractTest, TimestampIsTimeOfReading) {
    ASSERT_TRUE(openBus());
    harness_->deliver(makeFrame(0x100U));
    harness_->clock().advance(std::chrono::milliseconds(5));
    const core::TTimePoint readTime = harness_->clock().now();

    SReceivedFrame received;
    ASSERT_EQ(bus().receive(received, kFrameTimeout), EReceiveResult::kReceived);
    EXPECT_EQ(received.timestamp, readTime);
}

// SW_CAN_013
TEST_P(CCanBusContractTest, DroppedCountIsZeroAfterOpen) {
    ASSERT_TRUE(openBus());

    EXPECT_EQ(bus().droppedFrames(), 0U);
}

// --- Resources and threads ---

// SW_CAN_014
TEST_P(CCanBusContractTest, DoesNotAllocateAfterOpen) {
    ASSERT_TRUE(openBus());
    harness_->deliver(makeFrame(0x100U));
    const SCanFrame frame = makeFrame(0x200U);
    SReceivedFrame received;

    const std::size_t before = allocationCount();
    const ESendResult sendResult = bus().send(frame);
    const EReceiveResult firstReceive = bus().receive(received, kFrameTimeout);
    const EReceiveResult secondReceive = bus().receive(received, kShortTimeout);
    const std::uint64_t dropped = bus().droppedFrames();
    const std::size_t after = allocationCount();

    EXPECT_EQ(after - before, 0U);
    EXPECT_EQ(sendResult, ESendResult::kAccepted);
    EXPECT_EQ(firstReceive, EReceiveResult::kReceived);
    EXPECT_EQ(secondReceive, EReceiveResult::kTimeout);
    EXPECT_EQ(dropped, 0U);
}

// SW_CAN_015
TEST_P(CCanBusContractTest, SendsAndReceivesConcurrently) {
    ASSERT_TRUE(openBus());
    constexpr std::size_t kFrames = 16U;
    std::size_t receivedCount = 0U;

    std::thread receiver([this, &receivedCount] {
        const auto giveUp = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (receivedCount < kFrames && std::chrono::steady_clock::now() < giveUp) {
            SReceivedFrame received;
            if (bus().receive(received, kShortTimeout) == EReceiveResult::kReceived) {
                ++receivedCount;
            }
        }
    });

    std::size_t acceptedCount = 0U;
    for (std::size_t i = 0U; i < kFrames; ++i) {
        harness_->deliver(makeFrame(0x200U + static_cast<std::uint32_t>(i)));
        if (bus().send(makeFrame(0x300U + static_cast<std::uint32_t>(i))) == ESendResult::kAccepted) {
            ++acceptedCount;
        }
    }
    receiver.join();

    EXPECT_EQ(acceptedCount, kFrames);
    EXPECT_EQ(receivedCount, kFrames);
}

}  // namespace cvc::test
