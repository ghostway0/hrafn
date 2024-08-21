#pragma once

#include <optional>
#include <span>
#include <vector>

constexpr std::vector<uint8_t> encode_varuint(uint64_t val) {
    std::vector<uint8_t> result;
    while (val >= 0x80) {
        result.push_back((val & 0xFF) | 0x80);
        val >>= 7;
    }
    result.push_back(val & 0xFF);
    return result;
}

constexpr std::optional<std::tuple<uint64_t, size_t>> decode_varuint(
        std::span<uint8_t> bytes) {
    uint64_t result = 0;

    for (size_t read = 0; read < bytes.size(); read++) {
        uint8_t current = bytes[read];
        result |= static_cast<uint64_t>(current & ~0x80) << (read * 7);

        if ((current & 0x80) == 0) {
            return std::tuple<uint64_t, size_t>{result, read + 1};
        }
    }

    return std::nullopt;
}

