#include <cassert>
#include <expected>
#include <functional>
#include <map>
#include <string_view>

#include "multiaddr.h"
#include "semantic_version.h"

std::map<std::string_view,
        std::function<std::expected<std::shared_ptr<Protocol>, ParseError>(
                std::string_view)>> const protocol_parsers = {
        {
                "bluetooth",
                BluetoothAddress::parse_to_protocol,
        },
};

std::map<std::string_view,
        std::function<std::expected<std::shared_ptr<Protocol>, ParseError>(
                std::span<uint8_t>)>> const raw_protocol_parsers = {
        {
                "bluetooth",
                BluetoothAddress::parse_raw_to_protocol,
        },
};

std::expected<Multiaddr, ParseError> Multiaddr::parse(std::string_view str) {
    Multiaddr multiaddr{};
    auto split = absl::StrSplit(str, '/');
    std::optional<std::string> protocol = std::nullopt;

    for (std::string_view part : split) {
        if (part.empty()) {
            continue;
        }

        if (!protocol.has_value()) {
            protocol = part;
            continue;
        }

        auto protocol_parser = protocol_parsers.find(protocol.value());
        if (protocol_parser == protocol_parsers.end()) {
            return std::unexpected(ParseError::InvalidFormat);
        }

        auto protocol_result = try_unwrap(protocol_parser->second(part));

        multiaddr.protocols.push_back(protocol_result);
        protocol = std::nullopt;
    }

    if (protocol.has_value()) {
        // the last part of the multiaddr is a semver
        // if not associated with a protocol
        multiaddr.version =
                try_unwrap(SemanticVersion::parse(protocol.value()));
    }

    return multiaddr;
}
