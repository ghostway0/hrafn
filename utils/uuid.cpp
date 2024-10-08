#include <absl/strings/escaping.h>
#include <absl/strings/str_format.h>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include <sodium/randombytes.h>

#include "uuid.h"

UUID UUID::generate_random() {
    UUID uuid{};
    randombytes_buf(uuid.bytes().data(), UUID::kSize);
    return uuid;
}

std::optional<UUID> UUID::parse(std::string_view str) {
    std::string clean{};
    for (char c : str) {
        if (std::isalnum(c)) {
            clean += c;
        }
    }

    if (clean.size() != 32) {
        return std::nullopt;
    }

    UUID uuid{};

    for (size_t i = 0; i < 16; ++i) {
        std::string byte_str = clean.substr(i * 2, 2);
        std::string byte_result;

        if (!absl::HexStringToBytes(byte_str, &byte_result)) {
            return std::nullopt;
        }

        assert(byte_result.size() == 1);
        uuid.bytes()[i] = static_cast<uint8_t>(byte_result[0]);
    }

    return uuid;
}

std::optional<UUID> UUID::parse_raw(std::span<uint8_t> bytes) {
    if (bytes.size() != kSize) {
        return std::nullopt;
    }

    UUID uuid{};
    std::copy(bytes.begin(), bytes.end(), uuid.bytes().begin());

    return uuid;
}

std::string UUID::to_string() const {
    std::string out{};
    for (size_t i = 0; i < bytes_.size(); i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            out += '-';
        }

        out += absl::StrFormat("%02x", bytes_[i]);
    }

    return out;
}
