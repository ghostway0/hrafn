#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

#include <absl/random/random.h>
#include <absl/strings/escaping.h>
#include <sodium.h>
#include <vector>

#include "messages.pb.h"

constexpr uint32_t kPubkeySize = 32;
constexpr uint32_t kPrivkeySize = 32;
constexpr uint32_t kSignatureSize = 32;

class Pubkey {
public:
    explicit Pubkey(std::array<uint8_t, kPubkeySize> bytes) : bytes_{bytes} {}

    explicit Pubkey(doomday::Ed25519FieldPoint const &field_point);

    static std::optional<Pubkey> from_base64(std::string_view base64);

    std::array<uint8_t, kPubkeySize> const &data() const { return bytes_; }

    bool verify(absl::Span<uint8_t const> bytes,
            absl::Span<uint8_t const> signature) const;

    std::string to_string() const;

    std::string to_base64() const;

    std::vector<uint8_t> encrypt_to(std::span<uint8_t> message);

    bool operator==(Pubkey const &other) const {
        // not secret data
        return other.bytes_ == bytes_;
    }

    bool operator<=>(Pubkey const &other) const = delete;

private:
    std::array<uint8_t, kPubkeySize> bytes_;
};

struct PeerId {
    std::vector<uint8_t> bytes;

    static std::optional<PeerId> from_base64(std::string_view base64);

    static PeerId from_pubkey(Pubkey const &pubkey);

    std::string to_base64() const;

    std::string to_string() const;
};

class Privkey {
public:
    explicit Privkey(std::array<uint8_t, kPrivkeySize> &&bytes);

    explicit Privkey(doomday::Ed25519FieldPoint &&field_point);

    Privkey(Privkey &&other) noexcept;

    Privkey(Privkey const &) = delete;

    ~Privkey() { std::fill(bytes_.begin(), bytes_.end(), 0); }

    static std::optional<Privkey> from_base64(std::string_view base64);

    std::array<uint8_t, kPrivkeySize> const &data() const { return bytes_; }

    std::vector<uint8_t> sign(std::span<uint8_t> message) {
        std::vector<uint8_t> signature(crypto_sign_BYTES);
        crypto_sign_detached(signature.data(),
                nullptr,
                message.data(),
                message.size(),
                bytes_.data());
        return signature;
    }

    std::optional<std::vector<uint8_t>> decrypt(std::span<uint8_t> ciphertext);

    bool operator<=>(Privkey const &other) = delete;

private:
    std::array<uint8_t, kPrivkeySize> bytes_;
};

struct Keypair {
    Pubkey pubkey;
    Privkey privkey;

    static Keypair generate();
    Keypair generate_from(std::array<uint8_t, crypto_sign_SEEDBYTES>);
};
