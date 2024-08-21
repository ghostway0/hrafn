#include <algorithm>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <absl/strings/str_format.h>

#include "multiaddr.h"
#include "semantic_version.h"
#include "varint.h"

namespace {

std::map<std::string_view,
        std::function<std::optional<std::unique_ptr<Protocol>>(
                MultiaddrStringTokenizer &)>> const protocol_parsers = {
        {
                "btle",
                BluetoothAddress::parse_to_protocol,
        },
        // {
        //         "p2p",
        //         BluetoothAddress::parse_to_protocol,
        // },
};

std::map<uint64_t,
        std::function<std::optional<std::unique_ptr<Protocol>>(
                MultiaddrRawTokenizer &iter)>> const raw_protocol_parsers = {
        {
                150,
                BluetoothAddress::parse_raw_to_protocol,
        },
};

} // namespace

std::optional<Multiaddr> Multiaddr::parse(std::string_view str) {
    Multiaddr multiaddr{};
    MultiaddrStringTokenizer tokenizer{str};

    while (auto protocol_opt = tokenizer.next()) {
        std::string_view protocol = protocol_opt.value();

        if (protocol.empty()) {
            continue;
        }

        if (auto protocol_parser = protocol_parsers.find(protocol);
                protocol_parser != protocol_parsers.end()) {
            auto protocol_result =
                    try_unwrap_optional(protocol_parser->second(tokenizer));

            multiaddr.protocols.push_back(std::move(protocol_result));
        } else if (tokenizer.is_done()) {
            multiaddr.version =
                    try_unwrap_optional(SemanticVersion::parse(protocol));
            break;
        } else {
            return std::nullopt;
        }
    }

    return multiaddr;
}

std::optional<Multiaddr> Multiaddr::parse_raw(std::span<uint8_t> bytes) {
    Multiaddr multiaddr{};
    MultiaddrRawTokenizer tokenizer{bytes};

    while (auto protocol_opt = tokenizer.read_uleb128()) {
        uint64_t protocol = protocol_opt.value();

        if (auto protocol_parser = raw_protocol_parsers.find(protocol);
                protocol_parser != raw_protocol_parsers.end()) {
            auto protocol_result =
                    try_unwrap_optional(protocol_parser->second(tokenizer));

            multiaddr.protocols.push_back(std::move(protocol_result));
        } else {
            return std::nullopt;
        }
    }

    return multiaddr;
}

std::string Multiaddr::to_string() const {
    std::string result;
    for (auto const &protocol : protocols) {
        absl::StrAppendFormat(
                &result, "/%s/%s", protocol->name, protocol->to_string());
    }

    if (version.has_value()) {
        absl::StrAppendFormat(&result, "/%s", version.value().to_string());
    }

    return result;
}
