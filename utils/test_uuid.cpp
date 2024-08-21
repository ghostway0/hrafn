#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "uuid.h"

TEST_CASE("UUID parse/to_string") {
    SUBCASE("Empty") {
        CHECK(!UUID::parse("").has_value());
    }

    SUBCASE("Wrong length") {
        CHECK(!UUID::parse("123e4567-e89b-12d3-a456-4266141740").has_value());
    }

    SUBCASE("Not hex") {
        CHECK(!UUID::parse("123e4567-e89b-12d3-a45!-42661417400z").has_value());
    }

    SUBCASE("Equivalent representations") {
        auto a = UUID::parse("123e4567-e89b-12d3-a456-426614174000");
        auto b = UUID::parse("{123e4567-e89b-12d3-a456-426614174000}");
        auto c = UUID::parse("123e4567e89b12d3a456426614174000");

        CHECK(a.has_value());
        CHECK(b.has_value());
        CHECK(c.has_value());

        std::string s = a.value().to_string();

        CHECK_EQ(s, "123e4567-e89b-12d3-a456-426614174000");
        CHECK_EQ(b.value().to_string(), s);
        CHECK_EQ(c.value().to_string(), s);
    }
}

