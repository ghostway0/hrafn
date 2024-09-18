#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <absl/strings/str_split.h>

#include "error_utils.h"
#include "semantic_version.h"
#include "uuid.h"
#include "varint.h"

struct Multiaddr;
struct Protocol;

constexpr uint32_t kUUIDSize = 32;

struct Protocol {
    std::string name;
    uint8_t code;

    Protocol(std::string &&name, uint8_t code) : name{name}, code{code} {}

    virtual ~Protocol() = default;
    virtual std::string to_string() const = 0;
    virtual std::span<uint8_t const> raw() const = 0;
};

struct Multiaddr {
    std::vector<std::unique_ptr<Protocol>> protocols;
    std::vector<uint8_t> identifier;
    // not sure if this is even standard
    std::optional<SemanticVersion> version;

    static std::optional<Multiaddr> parse(std::string_view str);
    static std::optional<Multiaddr> parse_raw(std::span<uint8_t> bytes);

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
    explicit MultiaddrRawTokenizer(std::vector<uint8_t> bytes)
        : bytes_{bytes} {}

    explicit MultiaddrRawTokenizer(std::span<uint8_t> bytes) : bytes_{bytes} {}

    template<typename T>
    std::optional<T> read()
    requires std::is_trivially_copyable_v<T>
    {
        if (current_ + sizeof(T) >= bytes_.size()) {
            return std::nullopt;
        }

        T *val = reinterpret_cast<T *>(&bytes_[current_]);
        current_ += sizeof(T);
        return *val;
    }

    std::optional<uint64_t> read_uleb128() {
        auto [value, read] = try_unwrap_optional(decode_varuint(
                {bytes_.begin() + current_, bytes_.size() - current_}));
        current_ += read;
        return value;
    }

    std::optional<std::span<uint8_t>> read_bytes(size_t count) {
        if (current_ + count > bytes_.size()) {
            return std::nullopt;
        }

        std::span<uint8_t> result = {bytes_.begin() + current_, count};
        current_ += count;
        return result;
    }

    std::optional<uint64_t> uint64() { return read<uint64_t>(); }
    std::optional<uint32_t> uint32() { return read<uint32_t>(); }
    std::optional<uint32_t> varuint32() {
        uint64_t value = try_unwrap_optional(read_uleb128());

        return value < UINT32_MAX
                ? std::make_optional(static_cast<uint32_t>(value))
                : std::nullopt;
    }

private:
    std::span<uint8_t> bytes_;
    size_t current_ = 0;
};

struct BluetoothAddress : Protocol {
    UUID address;

    explicit BluetoothAddress(UUID addr)
        : Protocol{"btle", 'b'}, address{addr} {}

    std::string to_string() const override { return address.to_string(); }

    std::span<uint8_t const> raw() const override {
        return {address.bytes().begin(), address.bytes().end()};
    }

    static std::optional<std::unique_ptr<Protocol>> parse_to_protocol(
            MultiaddrStringTokenizer &iter) {
        BluetoothAddress address{
                try_unwrap_optional(UUID::parse(iter.next().value()))};
        return std::unique_ptr(std::make_unique<BluetoothAddress>(address));
    }

    static std::optional<std::unique_ptr<Protocol>> parse_raw_to_protocol(
            MultiaddrRawTokenizer &tokenizer) {
        std::span<uint8_t> bytes =
                try_unwrap_optional(tokenizer.read_bytes(UUID::kSize));

        BluetoothAddress address{try_unwrap_optional(UUID::parse_raw(bytes))};

        return std::unique_ptr(std::make_unique<BluetoothAddress>(address));
    }
};
