#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>

#include <gtest/gtest.h>

#include "comm/can_frame.hpp"
#include "comm/e2e.hpp"
#include "comm/message_codec.hpp"
#include "support/allocation_counter.hpp"

using cvc::comm::decode;
using cvc::comm::EEncodeResult;
using cvc::comm::encode;
using cvc::comm::kCounterByte;
using cvc::comm::kCrcByte;
using cvc::comm::SBrakeStatus;
using cvc::comm::TMessageId;
using cvc::comm::TPayload;
using cvc::test::allocationCount;

namespace cvc::comm {

void PrintTo(EEncodeResult result, std::ostream* os) {
    switch (result) {
        case EEncodeResult::kOk:
            *os << "kOk";
            return;
        case EEncodeResult::kInvalidValue:
            *os << "kInvalidValue";
            return;
    }
}

}  // namespace cvc::comm

namespace {

// Payloads computed with cantools from icd/central_vehicle_computer.dbc, with the alive counter and CRC set to 0.
// Brake pedal pressed, ABS fault, 123.4 bar.
constexpr TPayload kPayloadBraking{0x05, 0xD2, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
// Brake pedal pressed, ABS active, ABS fault, 250 bar (the top of the range).
constexpr TPayload kPayloadAtPressureLimit{0x07, 0xC4, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00};

constexpr SBrakeStatus kBraking{true, false, true, 123.4F};

void expectBraking(const SBrakeStatus& message) {
    EXPECT_TRUE(message.brakePedalPressed);
    EXPECT_FALSE(message.absActive);
    EXPECT_TRUE(message.absFault);
    ASSERT_TRUE(message.brakePressureBar.has_value());
    EXPECT_FLOAT_EQ(*message.brakePressureBar, 123.4F);
}

// Puts a raw BrakePressure value in bits 8-19, independently of the code under test.
TPayload withRawBrakePressure(std::uint16_t raw) {
    TPayload payload{};
    payload[1] = static_cast<std::uint8_t>(raw & 0xFFU);
    payload[2] = static_cast<std::uint8_t>(raw >> 8U);
    return payload;
}

TPayload filledWith(std::uint8_t value) {
    TPayload payload{};
    payload.fill(value);
    return payload;
}

}  // namespace

// SW_CODEC_001, SW_CODEC_002
TEST(MessageCodecTest, DecodesBrakeStatus) {
    SBrakeStatus message{};
    decode(kPayloadBraking, message);
    expectBraking(message);
}

// SW_CODEC_001, SW_CODEC_002
TEST(MessageCodecTest, EncodesBrakeStatus) {
    TPayload payload{};
    EXPECT_EQ(encode(kBraking, payload), EEncodeResult::kOk);
    EXPECT_EQ(payload, kPayloadBraking);
}

// SW_CODEC_001
TEST(MessageCodecTest, IgnoresTheAliveCounterAndCrcWhenDecoding) {
    TPayload payload = kPayloadBraking;
    payload[kCounterByte] = 0xFF;
    payload[kCrcByte] = 0xFF;
    SBrakeStatus message{};
    decode(payload, message);
    expectBraking(message);
}

// SW_CODEC_002
TEST(MessageCodecTest, RoundsBrakePressureToTheNearestRawValue) {
    TPayload payload{};
    ASSERT_EQ(encode(SBrakeStatus{false, false, false, 12.36F}, payload), EEncodeResult::kOk);
    EXPECT_EQ(payload[1], 0x7C);  // raw 124
    ASSERT_EQ(encode(SBrakeStatus{false, false, false, 12.34F}, payload), EEncodeResult::kOk);
    EXPECT_EQ(payload[1], 0x7B);  // raw 123
}

// SW_CODEC_003, SW_CODEC_004
TEST(MessageCodecTest, AcceptsBrakePressureAtTheTopOfTheRange) {
    SBrakeStatus message{};
    decode(kPayloadAtPressureLimit, message);
    ASSERT_TRUE(message.brakePressureBar.has_value());
    EXPECT_FLOAT_EQ(*message.brakePressureBar, 250.0F);

    TPayload payload{};
    EXPECT_EQ(encode(SBrakeStatus{true, true, true, 250.0F}, payload), EEncodeResult::kOk);
    EXPECT_EQ(payload, kPayloadAtPressureLimit);
}

// SW_CODEC_003
TEST(MessageCodecTest, ReportsBrakePressureAboveTheRangeAsInvalid) {
    const std::array<std::uint16_t, 2> raws{2501U, 4095U};
    for (const std::uint16_t raw : raws) {
        SCOPED_TRACE(raw);
        SBrakeStatus message{false, false, false, 5.0F};
        decode(withRawBrakePressure(raw), message);
        EXPECT_FALSE(message.brakePressureBar.has_value());
    }
}

// SW_CODEC_004
TEST(MessageCodecTest, RejectsBrakePressureOutsideTheRange) {
    // 250.04 and -0.04 both round to a valid raw value: the range applies to the physical value.
    const std::array<float, 3> values{250.04F, -0.04F, std::numeric_limits<float>::quiet_NaN()};
    for (const float value : values) {
        SCOPED_TRACE(value);
        TPayload payload = filledWith(0xA5);
        EXPECT_EQ(encode(SBrakeStatus{true, true, true, value}, payload), EEncodeResult::kInvalidValue);
        EXPECT_EQ(payload, filledWith(0xA5));
    }
}

// SW_CODEC_004
TEST(MessageCodecTest, RejectsMissingBrakePressure) {
    TPayload payload = filledWith(0xA5);
    EXPECT_EQ(encode(SBrakeStatus{true, false, false, std::nullopt}, payload), EEncodeResult::kInvalidValue);
    EXPECT_EQ(payload, filledWith(0xA5));
}

// SW_CODEC_005
TEST(MessageCodecTest, ClearsUnusedBitsAndKeepsTheAliveCounterAndCrc) {
    TPayload payload = filledWith(0xFF);
    EXPECT_EQ(encode(kBraking, payload), EEncodeResult::kOk);
    const TPayload expected{0x05, 0xD2, 0x04, 0x00, 0x00, 0x00, 0x0F, 0xFF};
    EXPECT_EQ(payload, expected);
}

// SW_CODEC_006
TEST(MessageCodecTest, BrakeStatusHasTheIdentifierFromTheDbc) {
    EXPECT_EQ(SBrakeStatus::kId, TMessageId{0x110});
}

// SW_CODEC_007
TEST(MessageCodecTest, DoesNotAllocate) {
    SBrakeStatus message{};
    TPayload payload{};
    const auto before = allocationCount();
    decode(kPayloadBraking, message);
    const EEncodeResult result = encode(kBraking, payload);
    EXPECT_EQ(allocationCount(), before);
    EXPECT_EQ(result, EEncodeResult::kOk);
}
