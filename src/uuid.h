#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

#include "error.h"

struct UUID {
    std::array<uint8_t, 16> bytes;

    static std::expected<UUID, ParseError> parse(std::string_view str);
    static std::expected<UUID, ParseError> parse_raw(std::span<uint8_t> bytes);
    std::string to_string() const;
};
