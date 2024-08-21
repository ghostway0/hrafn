#include <vector>

constexpr std::vector<uint8_t> encode_varint(uint64_t val) {
    std::vector<uint8_t> result;
    while (val >= 0x80) {
        result.push_back((val & 0xFF) | 0x80);
        val >>= 7;
    }
    result.push_back(val & 0xFF);
    return result;
}

