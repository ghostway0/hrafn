#include <expected>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "utils.h"

TEST_CASE("UUID parse/to_string") {
    if (UUID::parse("").has_value()) {
        FAIL("Empty string should not be a valid UUID");
    }

    std::expected<UUID, Error> a = UUID::parse("123e4567-e89b-12d3-a456-426614174000");
    std::expected<UUID, Error> b = UUID::parse("{123e4567-e89b-12d3-a456-426614174000}");
    std::expected<UUID, Error> c = UUID::parse("123e4567e89b12d3a456426614174000");

    CHECK(a.has_value());
    CHECK(b.has_value());
    CHECK(c.has_value());

    std::string s = a.value().to_string();

    CHECK_EQ(s, "123e4567-e89b-12d3-a456-426614174000");
    CHECK_EQ(b.value().to_string(), s);
    CHECK_EQ(c.value().to_string(), s);
}
