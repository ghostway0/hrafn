#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

struct UUID {
    static constexpr size_t kSize = 16;

    std::array<uint8_t, kSize> bytes;

    static std::optional<UUID> parse(std::string_view str);
    static std::optional<UUID> parse_raw(std::span<uint8_t> bytes);
    static UUID generate_random();

    std::string to_string() const;
};
