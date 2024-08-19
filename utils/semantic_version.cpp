#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <expected>
#include <string_view>
#include <vector>

#include "semantic_version.h"

std::expected<SemanticVersion, ParseError> SemanticVersion::parse(
        std::string_view str) {
    std::vector<std::string_view> parts = absl::StrSplit(str, '.');
    if (parts.size() != 3) {
        return std::unexpected(ParseError::InvalidFormat);
    }

    SemanticVersion version{};
    if (!absl::SimpleAtoi(parts[0], &version.major)
            || !absl::SimpleAtoi(parts[1], &version.minor)
            || !absl::SimpleAtoi(parts[2], &version.patch)) {
        return std::unexpected(ParseError::InvalidFormat);
    }

    return version;
}

std::string SemanticVersion::to_string() const {
    return std::to_string(major) + "." + std::to_string(minor) + "."
            + std::to_string(patch);
}
