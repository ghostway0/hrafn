#pragma once

#include <absl/strings/str_split.h>
#include <__fwd/string_view.h>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <iterator>
#include <memory>
#include <span>

#include "error.h"
#include "semantic_version.h"
#include "uuid.h"

struct Multiaddr;
struct Protocol;

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
    uint8_t code;

    Protocol(std::string &&name, uint8_t code) : name{name}, code{code} {}

    virtual ~Protocol() = default;
    virtual std::string to_string() const = 0;
    virtual std::span<uint8_t const> raw() const = 0;
};

struct Multiaddr {
    std::vector<std::shared_ptr<Protocol>> protocols;
    std::vector<uint8_t> identifier;
    std::optional<SemanticVersion> version;

    static std::expected<Multiaddr, ParseError> parse(std::string_view str);

    std::string to_string() const;

    auto operator<=>(Multiaddr const &) const = default;
};

class MultiaddrStringTokenizer {
public:
    explicit MultiaddrStringTokenizer(std::string_view str)
        : tokens_{absl::StrSplit(str, '/')}, current_{tokens_.begin()} {}

    std::optional<std::string_view> next() {
        if (current_ != tokens_.end()) {
            return *current_++;
        }
        return std::nullopt;
    }

    bool is_done() {
        return current_ == tokens_.end()
                || std::next(current_) == tokens_.end();
    }

private:
    std::vector<std::string_view> tokens_;
    std::vector<std::string_view>::iterator current_;
};

class MultiaddrRawTokenizer {
public:
    explicit MultiaddrRawTokenizer(std::vector<uint8_t> str) {}

    std::optional<std::vector<uint8_t>> next() {
        // if (current_ != tokens_.end()) {
        //     return *current_++;
        // }
        return std::nullopt;
    }

private:
    // std::vector<std::uint8_t> tokens_;
    // std::vector<std::string_view>::iterator current_;
};

struct BluetoothAddress : Protocol {
    UUID address;

    explicit BluetoothAddress(UUID addr)
        : Protocol{"btu", 'b'}, address{addr} {}

    std::string to_string() const override { return address.to_string(); }

    std::span<uint8_t const> raw() const override {
        return {address.bytes.begin(), address.bytes.end()};
    }

    static std::expected<std::unique_ptr<Protocol>, ParseError>
    parse_to_protocol(MultiaddrStringTokenizer &iter) {
        BluetoothAddress address{try_unwrap(UUID::parse(iter.next().value()))};
        return std::unique_ptr(std::make_unique<BluetoothAddress>(address));
    }

    static std::expected<std::unique_ptr<Protocol>, ParseError>
    parse_raw_to_protocol(MultiaddrRawTokenizer &iter) {
        auto a = iter.next().value();
        BluetoothAddress address{try_unwrap(UUID::parse_raw(a))};
        return std::unique_ptr(std::make_unique<BluetoothAddress>(address));
    }
};
