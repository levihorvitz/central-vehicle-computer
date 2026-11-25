#pragma once

#include <cstddef>
#include <type_traits>

namespace cvc::core {

template <typename T, std::size_t kCapacity>
class CSpscRingBuffer final {
    static_assert(kCapacity > 0U, "kCapacity must be greater than zero");
    static_assert((kCapacity & (kCapacity - 1U)) == 0U, "kCapacity must be a power of two");
    static_assert(std::is_nothrow_copy_assignable_v<T>, "T must be nothrow copy assignable");

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
};

template <typename T, std::size_t kCapacity>
bool CSpscRingBuffer<T, kCapacity>::tryPush(const T& /*item*/) noexcept {
    return false;
}

template <typename T, std::size_t kCapacity>
bool CSpscRingBuffer<T, kCapacity>::tryPop(T& /*item*/) noexcept {
    return false;
}

}  // namespace cvc::core
