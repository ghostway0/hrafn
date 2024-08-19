#include "crypto.h"

Keypair Keypair::generate() {
    std::array<uint8_t, kPrivkeySize> privkey{};
    std::array<uint8_t, kPubkeySize> pubkey{};

    crypto_sign_keypair(pubkey.data(), privkey.data());

    return Keypair{
            Pubkey{std::move(pubkey)},
            Privkey{std::move(privkey)},
    };
}

Keypair Keypair::generate_from(
        std::array<uint8_t, crypto_sign_SEEDBYTES> seed) {
    std::array<uint8_t, kPrivkeySize> privkey{};
    std::array<uint8_t, kPubkeySize> pubkey{};

    crypto_sign_seed_keypair(pubkey.data(), privkey.data(), seed.data());

    return Keypair{
            Pubkey{std::move(pubkey)},
            Privkey{std::move(privkey)},
    };
}

// Pubkey::Pubkey(doomday::Ed25519FieldPoint const &field_point) : bytes_{} {
//     std::copy(field_point.limbs().begin(),
//             field_point.limbs().end(),
//             bytes_.begin());
// }

std::optional<Pubkey> Pubkey::from_base64(std::string_view base64) {
    std::string decoded;
    if (!absl::Base64Unescape(base64, &decoded)
            || decoded.size() != kPubkeySize) {
        return std::nullopt;
    }

    std::array<uint8_t, kPubkeySize> bytes{};
    std::copy(decoded.begin(), decoded.end(), bytes.begin());

    return Pubkey{std::move(bytes)};
}

bool Pubkey::verify(absl::Span<uint8_t const> bytes,
        absl::Span<uint8_t const> signature) const {
    return crypto_sign_verify_detached(
                   signature.data(), bytes.data(), bytes.size(), bytes_.data())
            == 0;
}

std::string Pubkey::to_string() const {
    std::string buffer{};
    for (uint8_t byte : bytes_) {
        absl::StrAppendFormat(&buffer, "%02x", byte);
    }

    return buffer;
}

std::string Pubkey::to_base64() const {
    return absl::Base64Escape(absl::string_view{
            reinterpret_cast<char const *>(bytes_.data()), bytes_.size()});
}

std::vector<uint8_t> Pubkey::encrypt_to(std::span<uint8_t> message) {
    std::vector<uint8_t> ciphertext(message.size() + crypto_box_SEALBYTES);
    crypto_box_seal(
            ciphertext.data(), message.data(), message.size(), bytes_.data());
    return ciphertext;
}

std::optional<PeerId> PeerId::from_base64(std::string_view base64) {
    std::string decoded;
    if (!absl::Base64Unescape(base64, &decoded)) {
        return std::nullopt;
    }

    return PeerId{std::vector<uint8_t>(decoded.begin(), decoded.end())};
}

PeerId PeerId::from_pubkey(Pubkey const &pubkey) {
    std::vector<uint8_t> bytes(crypto_hash_sha256_BYTES);
    crypto_hash_sha256(
            bytes.data(), pubkey.data().data(), pubkey.data().size());

    return PeerId{bytes};
}

std::string PeerId::to_base64() const {
    return absl::Base64Escape(absl::string_view{
            reinterpret_cast<char const *>(bytes.data()), bytes.size()});
}

std::string PeerId::to_string() const {
    std::string buffer{};
    for (uint8_t byte : bytes) {
        absl::StrAppendFormat(&buffer, "%02x", byte);
    }

    return buffer;
}

Privkey::Privkey(std::array<uint8_t, kPrivkeySize> &&bytes) : bytes_{bytes} {
    std::fill(bytes.begin(), bytes.end(), 0);
}

// Privkey::Privkey(doomday::Ed25519FieldPoint &&field_point) : bytes_{} {
//     std::copy(field_point.limbs().begin(),
//             field_point.limbs().end(),
//             bytes_.begin());
//     // TODO(): zero out field_point
// }

Privkey::Privkey(Privkey &&other) noexcept : bytes_{} {
    std::copy(bytes_.begin(), bytes_.end(), bytes_.begin());
    std::fill(other.bytes_.begin(), other.bytes_.end(), 0);
}

std::optional<Privkey> Privkey::from_base64(std::string_view base64) {
    std::string decoded;
    if (!absl::Base64Unescape(base64, &decoded)
            || decoded.size() != kPrivkeySize) {
        return std::nullopt;
    }

    std::array<uint8_t, kPrivkeySize> bytes{};
    std::copy(decoded.begin(), decoded.end(), bytes.begin());
    std::fill(decoded.begin(), decoded.end(), 0);

    return Privkey{std::move(bytes)};
}

std::optional<std::vector<uint8_t>> Privkey::decrypt(
        std::span<uint8_t> ciphertext) const {
    std::vector<uint8_t> message(ciphertext.size() - crypto_box_SEALBYTES);
    if (crypto_box_seal_open(message.data(),
                ciphertext.data(),
                ciphertext.size(),
                bytes_.data(),
                bytes_.data())
            != 0) {
        return std::nullopt;
    }

    return message;
}

Signature Privkey::sign(std::span<uint8_t> message) const {
    std::array<uint8_t, kSignatureSize> signature{};
    crypto_sign_detached(signature.data(),
            nullptr,
            message.data(),
            message.size(),
            bytes_.data());
    return Signature{signature};
}
