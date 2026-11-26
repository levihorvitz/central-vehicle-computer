#include "comm/crc8.hpp"

namespace cvc::comm {

std::uint8_t crc8SaeJ1850(const std::uint8_t* data, std::size_t length) noexcept {
    std::uint8_t crc = 0xFF;
    for (std::size_t i = 0; i < length; ++i) {
        crc = static_cast<std::uint8_t>(crc ^ data[i]);
        for (int j = 0; j < 8; ++j) {
            if ((crc & 0x80U) != 0U) {
                crc = static_cast<std::uint8_t>(crc << 1U);
                crc = static_cast<std::uint8_t>(crc ^ 0x1DU);
            } else {
                crc = static_cast<std::uint8_t>(crc << 1U);
            }
        }
    }
    return static_cast<std::uint8_t>(crc ^ 0xFFU);
}

}  // namespace cvc::comm
