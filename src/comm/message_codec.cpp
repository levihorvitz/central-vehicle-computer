// Generated from icd/central_vehicle_computer.dbc. Do not edit by hand.
#include "comm/message_codec.hpp"

namespace cvc::comm {

void decode(const TPayload& /*payload*/, SBrakeStatus& /*message*/) noexcept {}

EEncodeResult encode(const SBrakeStatus& /*message*/, TPayload& /*payload*/) noexcept {
    return EEncodeResult::kOk;
}

}  // namespace cvc::comm
