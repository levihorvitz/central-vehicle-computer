#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

#include <gtest/gtest.h>

#include "core/spsc_ring_buffer.hpp"
#include "support/allocation_counter.hpp"

using cvc::core::CSpscRingBuffer;
using cvc::test::allocationCount;

namespace {

constexpr std::uint32_t kSmallCapacity = 4U;
using TSmallQueue = CSpscRingBuffer<std::uint32_t, static_cast<std::size_t>(kSmallCapacity)>;

void fill(TSmallQueue& queue, std::uint32_t first) {
    for (std::uint32_t value = first; value < first + kSmallCapacity; ++value) {
        ASSERT_TRUE(queue.tryPush(value));
    }
}

void expectPops(TSmallQueue& queue, std::uint32_t first, std::uint32_t count) {
    for (std::uint32_t value = first; value < first + count; ++value) {
        SCOPED_TRACE(value);
        std::uint32_t popped = 0U;
        ASSERT_TRUE(queue.tryPop(popped));
        EXPECT_EQ(popped, value);
    }
}

}  // namespace

// --- Empty, content and order ---

// SW_CANRX_002
TEST(CSpscRingBufferTest, NewQueueIsEmpty) {
    TSmallQueue queue;
    std::uint32_t popped = 7U;

    EXPECT_FALSE(queue.tryPop(popped));
    EXPECT_EQ(popped, 7U);
}

// SW_CANRX_002
TEST(CSpscRingBufferTest, ItemsComeOutInPushOrder) {
    TSmallQueue queue;
    ASSERT_TRUE(queue.tryPush(11U));
    ASSERT_TRUE(queue.tryPush(22U));
    ASSERT_TRUE(queue.tryPush(33U));

    for (const std::uint32_t expected : {11U, 22U, 33U}) {
        SCOPED_TRACE(expected);
        std::uint32_t popped = 0U;
        ASSERT_TRUE(queue.tryPop(popped));
        EXPECT_EQ(popped, expected);
    }
    std::uint32_t popped = 0U;
    EXPECT_FALSE(queue.tryPop(popped));
}

// --- Capacity ---

// SW_CANRX_003
TEST(CSpscRingBufferTest, HoldsExactlyCapacityItems) {
    TSmallQueue queue;

    for (std::uint32_t value = 0U; value < kSmallCapacity; ++value) {
        SCOPED_TRACE(value);
        EXPECT_TRUE(queue.tryPush(value));
    }
    EXPECT_FALSE(queue.tryPush(99U));
}

// --- Full queue ---

// SW_CANRX_004
TEST(CSpscRingBufferTest, PushOnFullQueueFails) {
    TSmallQueue queue;
    fill(queue, 1U);

    EXPECT_FALSE(queue.tryPush(99U));
}

// SW_CANRX_004
TEST(CSpscRingBufferTest, PushOnFullQueueLeavesContentsUnchanged) {
    TSmallQueue queue;
    fill(queue, 1U);

    ASSERT_FALSE(queue.tryPush(99U));

    expectPops(queue, 1U, kSmallCapacity);
    std::uint32_t popped = 0U;
    EXPECT_FALSE(queue.tryPop(popped));
}

// SW_CANRX_004
TEST(CSpscRingBufferTest, PushSucceedsAfterPopFromFullQueue) {
    TSmallQueue queue;
    fill(queue, 1U);
    std::uint32_t popped = 0U;
    ASSERT_TRUE(queue.tryPop(popped));

    EXPECT_TRUE(queue.tryPush(5U));

    expectPops(queue, 2U, kSmallCapacity);
}

// --- Wrap-around ---

// SW_CANRX_002
TEST(CSpscRingBufferTest, KeepsOrderAcrossWrapAround) {
    TSmallQueue queue;
    constexpr std::uint32_t kInFlight = 3U;
    constexpr std::uint32_t kTotal = 1000U;
    for (std::uint32_t value = 0U; value < kInFlight; ++value) {
        ASSERT_TRUE(queue.tryPush(value));
    }

    for (std::uint32_t value = kInFlight; value < kTotal; ++value) {
        SCOPED_TRACE(value);
        ASSERT_TRUE(queue.tryPush(value));
        std::uint32_t popped = 0U;
        ASSERT_TRUE(queue.tryPop(popped));
        ASSERT_EQ(popped, value - kInFlight);
    }

    expectPops(queue, kTotal - kInFlight, kInFlight);
}

// SW_CANRX_002, SW_CANRX_003
TEST(CSpscRingBufferTest, FillAndDrainRepeatedly) {
    TSmallQueue queue;
    constexpr std::uint32_t kRounds = 10U;

    for (std::uint32_t round = 0U; round < kRounds; ++round) {
        SCOPED_TRACE(round);
        const std::uint32_t first = round * kSmallCapacity;
        fill(queue, first);
        EXPECT_FALSE(queue.tryPush(99U));
        expectPops(queue, first, kSmallCapacity);
        std::uint32_t popped = 0U;
        EXPECT_FALSE(queue.tryPop(popped));
    }
}

// --- Threads ---

// SW_CANRX_002, SW_CANRX_006
TEST(CSpscRingBufferTest, ConcurrentProducerAndConsumerDeliverAllInOrder) {
    constexpr std::uint32_t kItems = 100000U;
    constexpr auto kDeadline = std::chrono::seconds(10);
    CSpscRingBuffer<std::uint32_t, 64U> queue;
    std::atomic<bool> abort{false};

    std::thread producer([&queue, &abort] {
        for (std::uint32_t value = 0U; value < kItems; ++value) {
            while (!queue.tryPush(value)) {
                if (abort.load()) {
                    return;
                }
                std::this_thread::yield();
            }
        }
    });

    const auto deadline = std::chrono::steady_clock::now() + kDeadline;
    std::uint32_t expected = 0U;
    bool inOrder = true;
    while (expected < kItems && inOrder && std::chrono::steady_clock::now() < deadline) {
        std::uint32_t popped = 0U;
        if (!queue.tryPop(popped)) {
            std::this_thread::yield();
            continue;
        }
        inOrder = (popped == expected);
        ++expected;
    }
    abort.store(true);
    producer.join();

    EXPECT_TRUE(inOrder) << "item " << (expected - 1U) << " came out of order";
    EXPECT_EQ(expected, kItems);
}

// --- Resources ---

// SW_CANRX_007
TEST(CSpscRingBufferTest, PushAndPopDoNotAllocate) {
    TSmallQueue queue;
    std::uint32_t popped = 0U;

    const std::size_t before = allocationCount();
    const bool pushed = queue.tryPush(42U);
    const bool poppedOk = queue.tryPop(popped);
    const bool poppedEmpty = queue.tryPop(popped);
    const std::size_t after = allocationCount();

    EXPECT_EQ(after - before, 0U);
    EXPECT_TRUE(pushed);
    EXPECT_TRUE(poppedOk);
    EXPECT_EQ(popped, 42U);
    EXPECT_FALSE(poppedEmpty);
}
