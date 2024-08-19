#include <algorithm>
#include <expected>
#include <functional>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

#include <absl/strings/str_format.h>

#include "multiaddr.h"
#include "semantic_version.h"

struct VectorCompare {
    bool operator()(std::vector<uint8_t> const &lhs,
            std::vector<uint8_t> const &rhs) const {
        return std::lexicographical_compare(
                lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }
};

constexpr std::vector<uint8_t> encode_varint(uint64_t val) {
    std::vector<uint8_t> result;
    while (val >= 0x80) {
        result.push_back((val & 0xFF) | 0x80);
        val >>= 7;
    }
    result.push_back(val & 0xFF);
    return result;
}

std::map<std::string_view,
        std::function<std::expected<std::shared_ptr<Protocol>, ParseError>(
                MultiaddrStringTokenizer &)>> const protocol_parsers = {
        {
                "btu",
                BluetoothAddress::parse_to_protocol,
        },
        // {
        //         "p2p",
        //         BluetoothAddress::parse_to_protocol,
        // },
};

std::map<std::vector<uint8_t>,
        std::function<std::expected<std::unique_ptr<Protocol>, ParseError>(
                MultiaddrRawTokenizer &iter)>,
        VectorCompare> const raw_protocol_parsers = {
        {
                encode_varint(150),
                BluetoothAddress::parse_raw_to_protocol,
        },
};

std::expected<Multiaddr, ParseError> Multiaddr::parse(std::string_view str) {
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
                    try_unwrap(protocol_parser->second(tokenizer));
            multiaddr.protocols.push_back(protocol_result);
        } else if (tokenizer.is_done()) {
            multiaddr.version = try_unwrap(SemanticVersion::parse(protocol));
            break;
        } else {
            return std::unexpected(ParseError::InvalidFormat);
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
