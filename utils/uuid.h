#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

struct UUID {
    std::array<uint8_t, 16> bytes;

    static std::optional<UUID> parse(std::string_view str);
    static std::optional<UUID> parse_raw(std::span<uint8_t> bytes);

    std::string to_string() const;
};
