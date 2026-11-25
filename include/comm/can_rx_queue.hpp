#pragma once

#include <cstddef>

#include "comm/can_frame.hpp"
#include "core/spsc_ring_buffer.hpp"

namespace cvc::comm {

constexpr std::size_t kRxQueueCapacity = 512U;  // [SW_CANRX_003]

using TCanRxQueue = core::CSpscRingBuffer<SReceivedFrame, kRxQueueCapacity>;

}  // namespace cvc::comm
