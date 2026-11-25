#pragma once

#include <atomic>
#include <cstdint>

namespace cvc::comm {

enum class EInterfaceStatus : std::uint8_t {
    kAvailable,
    kLost,
};

class CInterfaceStatus final {
    static_assert(std::atomic<EInterfaceStatus>::is_always_lock_free,
                  "std::atomic<EInterfaceStatus> must be lock-free");

public:
    CInterfaceStatus() = default;

    CInterfaceStatus(const CInterfaceStatus&) = delete;
    CInterfaceStatus& operator=(const CInterfaceStatus&) = delete;
    CInterfaceStatus(CInterfaceStatus&&) = delete;
    CInterfaceStatus& operator=(CInterfaceStatus&&) = delete;
    ~CInterfaceStatus() = default;

    // [SW_CANRX_009]
    [[nodiscard]]
    EInterfaceStatus get() const noexcept;

    // [SW_CANRX_010] [SW_CANRX_011] [SW_CANRX_012]
    void markLost() noexcept;

private:
    std::atomic<EInterfaceStatus> status_{EInterfaceStatus::kAvailable};
};

}  // namespace cvc::comm
