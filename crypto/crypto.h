#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <absl/strings/escaping.h>
#include <absl/strings/str_format.h>
#include <absl/types/span.h>
#include <sodium.h>

constexpr uint32_t kPubkeySize = crypto_sign_PUBLICKEYBYTES;
constexpr uint32_t kPrivkeySize = crypto_sign_SECRETKEYBYTES;
constexpr uint32_t kSignatureSize = crypto_sign_BYTES;

#define chksum_t uint64_t

class Pubkey {
public:
    explicit Pubkey(std::array<uint8_t, kPubkeySize> bytes) : bytes_{bytes} {}

    static std::optional<Pubkey> from_base64(std::string_view base64);

    static Pubkey from_stringbytes(std::string_view stringbytes) {
        std::array<uint8_t, kPubkeySize> data{};
        auto const *bytes =
                reinterpret_cast<uint8_t const *>(stringbytes.data());
        std::copy(bytes, bytes + kPubkeySize, data.begin());

        return Pubkey{data};
    }

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

struct Signature {
    std::array<uint8_t, kSignatureSize> bytes;

    static Signature from_bytes(std::span<uint8_t> bytes) {
        Signature signature{};
        std::copy(bytes.begin(), bytes.end(), signature.bytes.begin());
        return signature;
    }

    static Signature from_stringbytes(std::string_view stringbytes) {
        Signature signature{};
        auto const *bytes =
                reinterpret_cast<uint8_t const *>(stringbytes.data());
        std::copy(bytes, bytes + kSignatureSize, signature.bytes.begin());

        return signature;
    }
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

    Privkey(Privkey &&other) noexcept;

    Privkey(Privkey const &) = delete;

    ~Privkey() { std::fill(bytes_.begin(), bytes_.end(), 0); }

    static std::optional<Privkey> from_base64(std::string_view base64);

    std::array<uint8_t, kPrivkeySize> const &data() const { return bytes_; }

    Signature sign(std::span<uint8_t> message) const;

    std::optional<std::vector<uint8_t>> decrypt(
            std::span<uint8_t> ciphertext) const;

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
