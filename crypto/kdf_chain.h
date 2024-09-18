#include <span>
#include <vector>

#include <sodium.h>

class KDFChain {
public:
    explicit KDFChain(std::span<uint8_t> seed);

    std::array<uint8_t, 32> next_key();

    size_t n() const { return n_; }

private:
    std::vector<uint8_t> root_key_;
    std::vector<uint8_t> chain_key_;
    size_t n_ = 0;
};
