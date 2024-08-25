#include "crypto/kdf_chain.h"

std::array<uint8_t, 2> constexpr kKDFChainInput{0x13, 0x37};

std::array<uint8_t, crypto_kdf_hkdf_sha512_KEYBYTES> kdf_hmac(
        std::span<uint8_t const> salt, std::span<uint8_t const> input) {
    std::array<uint8_t, crypto_kdf_hkdf_sha512_KEYBYTES> prk{};
    crypto_kdf_hkdf_sha512_extract(
            prk.data(), salt.data(), salt.size(), input.data(), input.size());

    return prk;
}

KDFChain::KDFChain(std::span<uint8_t> seed) {
    root_key_.resize(seed.size());
    std::copy(seed.begin(), seed.end(), root_key_.begin());

    chain_key_.resize(seed.size());
    std::copy(seed.begin(), seed.end(), chain_key_.begin());
}

std::array<uint8_t, 32> KDFChain::next_key() {
    std::array<uint8_t, 32> message_key{};
    std::array<uint8_t, 64> hmac = kdf_hmac(chain_key_, kKDFChainInput);
    n_++;

    std::copy_n(hmac.begin(), 32, chain_key_.begin());
    std::copy_n(hmac.begin() + 32, 32, message_key.begin());

    return message_key;
}
