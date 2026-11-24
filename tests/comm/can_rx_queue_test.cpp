#include <chrono>
#include <cstddef>

#include <gtest/gtest.h>

#include "comm/can_frame.hpp"
#include "comm/can_rx_queue.hpp"
#include "core/clock.hpp"
#include "support/can_test_utils.hpp"

using cvc::comm::SReceivedFrame;
using cvc::comm::TCanRxQueue;
using cvc::core::TTimePoint;
using cvc::test::makeFrame;
using cvc::test::sameFrame;

namespace {

constexpr std::size_t kRequiredCapacity = 426U;

}  // namespace

// SW_CANRX_002
TEST(CCanRxQueueTest, PopReturnsPushedFrameUnchanged) {
    TCanRxQueue queue;
    SReceivedFrame pushed;
    pushed.frame = makeFrame(0x321U);
    pushed.timestamp = TTimePoint{} + std::chrono::milliseconds(1234);

    ASSERT_TRUE(queue.tryPush(pushed));

    SReceivedFrame popped;
    ASSERT_TRUE(queue.tryPop(popped));
    EXPECT_TRUE(sameFrame(popped.frame, pushed.frame));
    EXPECT_EQ(popped.timestamp, pushed.timestamp);
}

// SW_CANRX_003
TEST(CCanRxQueueTest, ReceiveQueueCapacityMeetsRequirement) {
    static_assert(TCanRxQueue::capacity() >= kRequiredCapacity);

    EXPECT_GE(TCanRxQueue::capacity(), kRequiredCapacity);
}
