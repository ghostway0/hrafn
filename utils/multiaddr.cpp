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
#include "varint.h"

namespace {

struct VectorCompare {
    bool operator()(std::vector<uint8_t> const &lhs,
            std::vector<uint8_t> const &rhs) const {
        return std::lexicographical_compare(
                lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }
};

std::map<std::string_view,
        std::function<std::optional<std::shared_ptr<Protocol>>(
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

std::map<std::vector<uint8_t>,
        std::function<std::optional<std::unique_ptr<Protocol>>(
                MultiaddrRawTokenizer &iter)>,
        VectorCompare> const raw_protocol_parsers = {
        {
                encode_varuint(150),
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
            multiaddr.protocols.push_back(protocol_result);
        } else if (tokenizer.is_done()) {
            multiaddr.version = try_unwrap_optional(SemanticVersion::parse(protocol));
            break;
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
