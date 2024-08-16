#pragma once

#include <cstddef>
#include <string_view>
#include <expected>

#include "error.h"
#include "semantic_version.h"

struct Multiaddr {
    SemanticVersion version;

    static std::expected<Multiaddr, ParseError> parse(std::string_view str);

    auto operator<=>(Multiaddr const &) const = default;
};
