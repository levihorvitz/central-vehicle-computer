#pragma once

#include <cstddef>
#include <cstdint>

#include "comm/can_frame.hpp"

namespace cvc::comm {

constexpr std::size_t kCounterByte = 6;
constexpr std::size_t kCrcByte = 7;
constexpr std::uint8_t kCounterMask = 0x0F;
constexpr std::uint8_t kCounterModulo = 16;

enum class EE2eResult {
    kOk,
    kLost,
    kRepeated,
    kCorrupted,
};

[[nodiscard]]
std::uint8_t e2eCrc(TMessageId id, const TPayload& payload) noexcept;

}  // namespace cvc::comm
