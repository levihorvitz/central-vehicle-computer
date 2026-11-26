// Generated from icd/central_vehicle_computer.dbc. Do not edit by hand.
#pragma once

#include <cstdint>
#include <optional>

#include "comm/can_frame.hpp"

namespace cvc::comm {

enum class EEncodeResult : std::uint8_t {
    kOk,
    kInvalidValue,
};

// [SW_CODEC_001] [SW_CODEC_006]
struct SBrakeStatus {
    static constexpr TMessageId kId = 0x110U;
    bool brakePedalPressed{};
    bool absActive{};
    bool absFault{};
    std::optional<float> brakePressureBar{};
};

// [SW_CODEC_001] [SW_CODEC_002] [SW_CODEC_003] [SW_CODEC_007]
void decode(const TPayload& payload, SBrakeStatus& message) noexcept;

// [SW_CODEC_001] [SW_CODEC_002] [SW_CODEC_004] [SW_CODEC_005] [SW_CODEC_007]
[[nodiscard]]
EEncodeResult encode(const SBrakeStatus& message, TPayload& payload) noexcept;

}  // namespace cvc::comm
