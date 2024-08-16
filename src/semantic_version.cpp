#include <expected>

#include <absl/strings/str_split.h>
#include <absl/strings/numbers.h>

#include "semantic_version.h"

std::expected<SemanticVersion, ParseError> SemanticVersion::parse(
        std::string_view str) {
    std::vector<std::string_view> parts = absl::StrSplit(str, '.');
    if (parts.size() != 3) {
        return std::unexpected(ParseError::ParseInvalidFormat);
    }

    SemanticVersion version{};
    if (!absl::SimpleAtoi(parts[0], &version.major)
            || !absl::SimpleAtoi(parts[1], &version.minor)
            || !absl::SimpleAtoi(parts[2], &version.patch)) {
        return std::unexpected(ParseError::ParseInvalidFormat);
    }

    return version;
}
