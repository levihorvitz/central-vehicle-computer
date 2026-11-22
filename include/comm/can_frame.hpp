#pragma once

#include <array>
#include <cstdint>

#include "core/clock.hpp"

namespace cvc::comm {

inline constexpr std::uint32_t kMaxStandardId = 0x7FFU;
inline constexpr std::uint8_t kMaxCanDlc = 8U;

// [SW_CAN_001]
struct SCanFrame {
    std::uint32_t id = 0U;
    std::uint8_t dlc = 0U;
    std::array<std::uint8_t, kMaxCanDlc> data{};
};

// [SW_CAN_012]
struct SReceivedFrame {
    SCanFrame frame{};
    core::TTimePoint timestamp{};
};

}  // namespace cvc::comm
