
#pragma once

#include <cstdint>
#include <span>

namespace crypto {

uint64_t crc64(std::span<uint8_t const> data);

uint64_t crc64(uint64_t crc, std::span<uint8_t const> data);

} // namespace crypto
