#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include <absl/strings/str_split.h>
#include <vector>

#include "error.h"
#include "semantic_version.h"
#include "uuid.h"

constexpr uint32_t kUUIDSize = 32;

#define try_unwrap(x) \
    ({ \
        auto _x = x; \
        if (!_x.has_value()) { \
            return std::unexpected(_x.error()); \
        } \
        _x.value(); \
    })

struct Protocol {
    std::string name;
    bool needs_argument;
    std::optional<size_t> size;
    uint8_t code;

    Protocol(std::string &&name,
            bool needs_argument,
            std::optional<size_t> size,
            uint8_t code)
        : name{name}, needs_argument{needs_argument}, size{size}, code{code} {}

    virtual ~Protocol() = default;
    virtual std::string textual() const = 0;
    virtual std::span<uint8_t const> raw() const = 0;
};

struct Multiaddr {
    std::vector<std::shared_ptr<Protocol>> protocols;
    std::vector<uint8_t> identifier;
    SemanticVersion version;

    static std::expected<Multiaddr, ParseError> parse(std::string_view str);

    auto operator<=>(Multiaddr const &) const = default;
};

struct BluetoothAddress : Protocol {
    UUID address;

    explicit BluetoothAddress(UUID addr)
        : Protocol{"bluetooth", true, kUUIDSize, 'b'}, address{addr} {}

    std::string textual() const override { return address.to_string(); }

    std::span<uint8_t const> raw() const override {
        return {address.bytes.begin(), address.bytes.end()};
    }

    static std::expected<std::shared_ptr<Protocol>, ParseError>
    parse_to_protocol(std::string_view str) {
        BluetoothAddress address{try_unwrap(UUID::parse(str))};
        return std::shared_ptr(std::make_shared<BluetoothAddress>(address));
    }

    static std::expected<std::shared_ptr<Protocol>, ParseError>
    parse_raw_to_protocol(std::span<uint8_t> bytes) {
        BluetoothAddress address{try_unwrap(UUID::parse_raw(bytes))};
        return std::shared_ptr(std::make_shared<BluetoothAddress>(address));
    }
};
