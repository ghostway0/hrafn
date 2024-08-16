#pragma once

#include <array>
#include <expected>
#include <string_view>

#include "error.h"

struct UUID {
    std::array<uint8_t, 16> bytes;

    static std::expected<UUID, ParseError> parse(std::string_view str);
    std::string to_string() const;
};
