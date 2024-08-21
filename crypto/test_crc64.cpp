
#include <vector>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "crc64.h"

TEST_CASE("Check(\"123456789\"): 0xe9c6d914c4b8d9ca") {
    std::vector<uint8_t> bytes{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK_EQ(crypto::crc64(bytes), 0xe9c6d914c4b8d9ca);
}
