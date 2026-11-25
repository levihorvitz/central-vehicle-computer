#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace cvc::core {

template <typename T, std::size_t kCapacity>
class CSpscRingBuffer final {
    static_assert(kCapacity > 0U, "kCapacity must be greater than zero");
    static_assert((kCapacity & (kCapacity - 1U)) == 0U, "kCapacity must be a power of two");
    static_assert(std::is_nothrow_copy_assignable_v<T>, "T must be nothrow copy assignable");
    static_assert(std::atomic<std::size_t>::is_always_lock_free, "std::atomic<std::size_t> must be lock-free");

public:
    CSpscRingBuffer() = default;

    CSpscRingBuffer(const CSpscRingBuffer&) = delete;
    CSpscRingBuffer& operator=(const CSpscRingBuffer&) = delete;
    CSpscRingBuffer(CSpscRingBuffer&&) = delete;
    CSpscRingBuffer& operator=(CSpscRingBuffer&&) = delete;
    ~CSpscRingBuffer() = default;

    // [SW_CANRX_004] [SW_CANRX_006] [SW_CANRX_007]
    [[nodiscard]]
    bool tryPush(const T& item) noexcept;

    // [SW_CANRX_002] [SW_CANRX_006] [SW_CANRX_007]
    [[nodiscard]]
    bool tryPop(T& item) noexcept;

    // [SW_CANRX_003]
    [[nodiscard]]
    static constexpr std::size_t capacity() noexcept {
        return kCapacity;
    }

private:
    static constexpr std::size_t kCacheLineSize = 64U;

    std::array<T, kCapacity> buffer_{};

    alignas(kCacheLineSize) std::atomic<std::size_t> writeIndex_{0U};
    alignas(kCacheLineSize) std::atomic<std::size_t> readIndex_{0U};
};

template <typename T, std::size_t kCapacity>
bool CSpscRingBuffer<T, kCapacity>::tryPush(const T& item) noexcept {
    const std::size_t writeIndex = writeIndex_.load(std::memory_order_relaxed);
    const std::size_t readIndex = readIndex_.load(std::memory_order_acquire);

    if (writeIndex - readIndex == kCapacity) {
        return false;
    }

    buffer_[writeIndex % kCapacity] = item;
    writeIndex_.store(writeIndex + 1U, std::memory_order_release);
    return true;
}

template <typename T, std::size_t kCapacity>
bool CSpscRingBuffer<T, kCapacity>::tryPop(T& item) noexcept {
    const std::size_t readIndex = readIndex_.load(std::memory_order_relaxed);
    const std::size_t writeIndex = writeIndex_.load(std::memory_order_acquire);

    if (writeIndex == readIndex) {
        return false;
    }

    item = buffer_[readIndex % kCapacity];
    readIndex_.store(readIndex + 1U, std::memory_order_release);
    return true;
}

}  // namespace cvc::core