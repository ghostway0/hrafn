#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

class UUID {
public:
    static constexpr size_t kSize = 16;

    static std::optional<UUID> parse(std::string_view str);
    static std::optional<UUID> parse_raw(std::span<uint8_t> bytes);
    static UUID generate_random();

    std::string to_string() const;
    std::span<uint8_t const> bytes() const { return bytes_; };
    std::span<uint8_t> bytes() { return bytes_; };

private:
    std::array<uint8_t, kSize> bytes_;
};
