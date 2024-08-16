#pragma once

#include <cstddef>
#include <expected>
#include <string_view>

#include "error.h"

struct SemanticVersion {
    size_t major;
    size_t minor;
    size_t patch;

    static std::expected<SemanticVersion, ParseError> parse(
            std::string_view str);

    auto operator<=>(SemanticVersion const &) const = default;
};
