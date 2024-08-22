#include "varint.h"
#include <cstdint>
#include <vector>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

struct TestCase {
    std::vector<uint8_t> bytes;
    uint64_t integer;
};

TEST_CASE("VarUint") {
    std::vector<TestCase> test_cases{
            {.bytes = {0x0}, .integer = 0x0},
    };

    for (TestCase const &c : test_cases) {
        auto [i, _] = decode_varuint(std::span(c.bytes)).value();
        CHECK_EQ(i, c.integer);
        CHECK_EQ(encode_varuint(c.integer), c.bytes);
    }
}
