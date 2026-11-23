#pragma once

#include <cstddef>
#include <cstdint>

#include "comm/can_frame.hpp"

namespace cvc::test {

[[nodiscard]] inline comm::SCanFrame makeFrame(std::uint32_t id, std::uint8_t dlc = comm::kMaxCanDlc) {
    comm::SCanFrame frame;
    frame.id = id;
    frame.dlc = dlc;
    for (std::size_t i = 0U; i < frame.data.size(); ++i) {
        frame.data[i] = static_cast<std::uint8_t>((id + i) & 0xFFU);
    }
    return frame;
}

[[nodiscard]] inline bool sameFrame(const comm::SCanFrame& a, const comm::SCanFrame& b) {
    if (a.id != b.id || a.dlc != b.dlc || a.dlc > comm::kMaxCanDlc) {
        return false;
    }
    for (std::size_t i = 0U; i < a.dlc; ++i) {
        if (a.data[i] != b.data[i]) {
            return false;
        }
    }
    return true;
}

}  // namespace cvc::test
