#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "semantic_version.h"

TEST_CASE("Semver parse/to_string") {
    SUBCASE("Empty") {
        CHECK(!SemanticVersion::parse("").has_value());
    }

    SUBCASE("Wrong") {
        CHECK(!SemanticVersion::parse("1.2.b").has_value());
    }

    auto semver = SemanticVersion::parse("1.2.3");
    CHECK(semver.has_value());

    CHECK_EQ(semver.value(), SemanticVersion{1, 2, 3});

    SUBCASE("Comparison") {
        CHECK(SemanticVersion{1, 2, 3} == SemanticVersion{1, 2, 3});
        CHECK(SemanticVersion{2, 2, 3} > SemanticVersion{1, 2, 3});
        CHECK(SemanticVersion{2, 2, 3} > SemanticVersion{1, 3, 4});
    }
}

