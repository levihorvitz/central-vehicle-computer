#include "comm/e2e_sender.hpp"

namespace cvc::comm {

CE2eSender::CE2eSender(TMessageId id) noexcept : id_(id) {}

void CE2eSender::protect(TPayload& payload) noexcept {
    payload[kCounterByte] = static_cast<std::uint8_t>(payload[kCounterByte] & ~kCounterMask);
    payload[kCounterByte] = static_cast<std::uint8_t>(payload[kCounterByte] | (kCounterMask & counter_));
    payload[kCrcByte] = e2eCrc(id_, payload);
    counter_ = static_cast<std::uint8_t>((counter_ + 1) % kCounterModulo);
}

}  // namespace cvc::comm
