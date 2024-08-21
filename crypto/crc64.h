
#pragma once

#include <span>
#include <cstdint>

namespace crypto {

uint64_t crc64(std::span<const uint8_t> data);

uint64_t crc64(uint64_t crc, std::span<const uint8_t> data);

} // namespace crypto
