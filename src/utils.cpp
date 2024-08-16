#include <expected>

#include <absl/strings/str_format.h>
#include <absl/strings/str_split.h>

#include "utils.h"

// TODO(ghostway): maybe a separate file
// I hope this file won't become too big...

std::expected<UUID, Error> UUID::parse(std::string_view str) {
    std::string clean{};
    for (char c : str) {
        if (std::isalpha(c) || std::isdigit(c)) {
            clean += c;
        }
    }

    if (clean.size() != 32) {
        return std::unexpected(Error::ParseInvalidFormat);
    }

    UUID uuid{};
    std::uint8_t current = 0;

    for (size_t i = 0; i < clean.size(); i++) {
        uint8_t value = 0;
        if (clean[i] >= '0' && clean[i] <= '9') {
            value = clean[i] - '0';
        } else if (clean[i] >= 'A' && clean[i] <= 'F') {
            value = clean[i] - 'A' + 10;
        } else if (clean[i] >= 'a' && clean[i] <= 'f') {
            value = clean[i] - 'a' + 10;
        }

        current = current << 4 | value;

        if (i % 2 == 0) {
            continue;
        }

        uuid.bytes[i / 2] = current;
        current = 0;
    }

    return uuid;
}

std::string UUID::to_string() const {
    std::string out{};
    for (size_t i = 0; i < bytes.size(); i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            out += '-';
        }

        out += absl::StrFormat("%02x", bytes[i]);
    }

    return out;
}
