#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

#include <gtest/gtest.h>

#include "comm/can_bus.hpp"
#include "comm/can_frame.hpp"
#include "comm/can_rx_queue.hpp"
#include "comm/can_rx_thread.hpp"
#include "comm/interface_status.hpp"
#include "core/clock.hpp"
#include "core/system_clock.hpp"
#include "fakes/fake_can_bus.hpp"
#include "support/allocation_counter.hpp"

namespace cvc::test {

using comm::CCanRxThread;
using comm::CInterfaceStatus;
using comm::EBusMode;
using comm::EInterfaceStatus;
using comm::EOpenResult;
using comm::SCanFrame;
using comm::SReceivedFrame;
using comm::TCanRxQueue;

namespace {

constexpr std::uint32_t kFirstId = 0x100U;
constexpr std::chrono::milliseconds kWaitLimit{2000};
constexpr std::chrono::milliseconds kPollPeriod{1};
constexpr std::chrono::milliseconds kStopLimit{100};
constexpr std::chrono::milliseconds kSettleTime{50};

[[nodiscard]]
SCanFrame makeNumberedFrame(std::uint32_t number) {
    SCanFrame frame{};
    frame.id = kFirstId + number;
    frame.dlc = 1U;
    frame.data[0] = static_cast<std::uint8_t>(number);
    return frame;
}

template <typename TCondition>
[[nodiscard]]
bool waitFor(TCondition condition) {
    const auto deadline = std::chrono::steady_clock::now() + kWaitLimit;
    while (!condition()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(kPollPeriod);
    }
    return true;
}

}  // namespace

class CCanRxThreadTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(bus_.open(CFakeCanBus::kInterfaceName, EBusMode::kSimulation), EOpenResult::kOpened);
    }

    void deliverFrames(std::uint32_t count) {
        for (std::uint32_t i = 0U; i < count; ++i) {
            bus_.deliver(makeNumberedFrame(i));
        }
    }

    core::CSystemClock clock_;
    CFakeCanBus bus_{clock_};
    TCanRxQueue queue_;
    CInterfaceStatus status_;
    CCanRxThread rxThread_{bus_, queue_, status_};
};

// SW_CANRX_001
TEST_F(CCanRxThreadTest, PutsEveryFrameIntoQueueWithBusTimestamp) {
    constexpr std::size_t kFrameCount = 20U;

    const core::TTimePoint before = clock_.now();
    deliverFrames(static_cast<std::uint32_t>(kFrameCount));
    ASSERT_TRUE(rxThread_.start());

    std::array<SReceivedFrame, kFrameCount> taken{};
    std::size_t count = 0U;
    const auto allTaken = [&] {
        while (count < taken.size() && queue_.tryPop(taken[count])) {
            ++count;
        }
        return count == taken.size();
    };
    ASSERT_TRUE(waitFor(allTaken));
    const core::TTimePoint after = clock_.now();

    for (std::size_t i = 0U; i < taken.size(); ++i) {
        const SCanFrame expected = makeNumberedFrame(static_cast<std::uint32_t>(i));
        EXPECT_EQ(taken[i].frame.id, expected.id);
        EXPECT_EQ(taken[i].frame.dlc, expected.dlc);
        EXPECT_EQ(taken[i].frame.data[0], expected.data[0]);
        EXPECT_LE(before, taken[i].timestamp);
        EXPECT_LE(taken[i].timestamp, after);
    }
}

// SW_CANRX_004
TEST_F(CCanRxThreadTest, DiscardsNewFrameWhenQueueIsFull) {
    constexpr std::uint32_t kExtraNumber = 600U;

    for (std::size_t i = 0U; i < TCanRxQueue::capacity(); ++i) {
        SReceivedFrame filler{};
        filler.frame = makeNumberedFrame(static_cast<std::uint32_t>(i));
        ASSERT_TRUE(queue_.tryPush(filler));
    }
    bus_.deliver(makeNumberedFrame(kExtraNumber));
    ASSERT_TRUE(rxThread_.start());

    const auto oneDiscarded = [&] {
        return rxThread_.discardedFrames() == 1U;
    };
    ASSERT_TRUE(waitFor(oneDiscarded));
    rxThread_.stop();

    for (std::size_t i = 0U; i < TCanRxQueue::capacity(); ++i) {
        SReceivedFrame taken;
        ASSERT_TRUE(queue_.tryPop(taken));
        EXPECT_EQ(taken.frame.id, kFirstId + static_cast<std::uint32_t>(i));
    }
    SReceivedFrame extra;
    EXPECT_FALSE(queue_.tryPop(extra));
    EXPECT_EQ(bus_.droppedFrames(), 0U);
}

// SW_CANRX_007
TEST_F(CCanRxThreadTest, DoesNotAllocateWhileRunning) {
    constexpr std::size_t kFrameCount = 20U;
    ASSERT_TRUE(rxThread_.start());

    const std::size_t before = allocationCount();
    deliverFrames(static_cast<std::uint32_t>(kFrameCount));
    std::size_t count = 0U;
    SReceivedFrame taken;
    const auto allTaken = [&] {
        while (queue_.tryPop(taken)) {
            ++count;
        }
        return count == kFrameCount;
    };
    const bool allArrived = waitFor(allTaken);
    const std::size_t after = allocationCount();

    ASSERT_TRUE(allArrived);
    EXPECT_EQ(after, before);
}

// SW_CANRX_008
TEST_F(CCanRxThreadTest, ReportsFailureWhenStartedTwice) {
    ASSERT_TRUE(rxThread_.start());
    EXPECT_FALSE(rxThread_.start());
    EXPECT_TRUE(rxThread_.isRunning());
}

// SW_CANRX_008
TEST_F(CCanRxThreadTest, StopsWithin100Milliseconds) {
    ASSERT_TRUE(rxThread_.start());

    const auto stopBegin = std::chrono::steady_clock::now();
    rxThread_.stop();
    const auto stopDuration = std::chrono::steady_clock::now() - stopBegin;

    EXPECT_FALSE(rxThread_.isRunning());
    EXPECT_LE(stopDuration, kStopLimit);
}

// SW_CANRX_008
TEST_F(CCanRxThreadTest, StopsReceivingWhenStopped) {
    ASSERT_TRUE(rxThread_.start());
    rxThread_.stop();

    deliverFrames(1U);
    std::this_thread::sleep_for(kSettleTime);

    SReceivedFrame taken;
    EXPECT_FALSE(queue_.tryPop(taken));
}

// SW_CANRX_008
TEST_F(CCanRxThreadTest, StopHasNoEffectWhenNotRunning) {
    rxThread_.stop();

    EXPECT_FALSE(rxThread_.isRunning());
    EXPECT_TRUE(rxThread_.start());
}

// SW_CANRX_010
TEST_F(CCanRxThreadTest, SetsStatusLostAndStopsWhenInterfaceLost) {
    ASSERT_TRUE(rxThread_.start());
    EXPECT_EQ(status_.get(), EInterfaceStatus::kAvailable);

    bus_.loseInterface();

    const auto stopped = [&] {
        return !rxThread_.isRunning();
    };
    ASSERT_TRUE(waitFor(stopped));
    EXPECT_EQ(status_.get(), EInterfaceStatus::kLost);
}

}  // namespace cvc::test
