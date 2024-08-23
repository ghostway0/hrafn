#include <bitset>
#include <cmath>
#include <cstdint>
#include <functional>

struct Options {
    /// expected number of elements
    size_t n;
    /// false positive rate
    double fpr;
};

struct BloomFilterConfig {
    size_t m;
    size_t k;
};

constexpr BloomFilterConfig calculate_bloom_filter_config(Options const &opts) {
    auto n = static_cast<double>(opts.n);
    double ln_fpr = std::log(opts.fpr);
    auto m = static_cast<size_t>(-n * ln_fpr / std::pow(std::log(2.0), 2.0));
    auto k = static_cast<size_t>(-ln_fpr / std::log(2.0));
    return {m, k};
}

template<Options opts>
class StaticBloomFilter {
public:
    std::pair<uint64_t, uint64_t> get_halves(auto const &item) const {
        auto hash_value = std::hash<decltype(item)>{}(item);
        size_t bits = sizeof(hash_value) * 8;
        auto lower_bits = static_cast<uint64_t>(
                hash_value & std::numeric_limits<uint64_t>::max());
        auto upper_bits = static_cast<uint64_t>(hash_value >> (bits / 2));

        return {lower_bits, upper_bits};
    }

    void put(auto const &item) {
        auto [lower_bits, upper_bits] = get_halves(item);

        size_t index = lower_bits;
        for (size_t i = 0; i < k_; ++i) {
            bits_.set(index % bits_.size());
            index = (index + i * upper_bits) % bits_.size();
        }
    }

    bool might_contain(auto const &item) const {
        auto [lower_bits, upper_bits] = get_halves(item);

        size_t index = lower_bits;
        for (size_t i = 0; i < k_; ++i) {
            if (!bits_.test(index % bits_.size())) {
                return false;
            }
            index = (index + i * upper_bits) % bits_.size();
        }

        return true;
    }

    size_t estimated_n() const {
        auto m = static_cast<double>(bits_.size());
        auto x = static_cast<double>(bits_.count());

        double val = -m / static_cast<double>(k_) * log(1.0 - x / m);
        return static_cast<size_t>(val);
    }

private:
    static constexpr BloomFilterConfig kConfig =
            calculate_bloom_filter_config(opts);

    std::bitset<kConfig.m> bits_;
    size_t k_;
};
