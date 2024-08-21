#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "multiaddr.h"

TEST_CASE("Multiaddr string representation") {
    SUBCASE("Semver before protocol") {
        auto multiaddr = Multiaddr::parse(
                "/1.2.3/btle/123e4567-e89b-12d3-a456-426614174000");
        CHECK(!multiaddr.has_value());
    }

    SUBCASE("Semver between protocol and address") {
        auto multiaddr = Multiaddr::parse(
                "/btle/1.2.3/123e4567-e89b-12d3-a456-426614174000");
        CHECK(!multiaddr.has_value());
    }

    SUBCASE("Semver after address") {
        auto multiaddr = Multiaddr::parse(
                "/btle/123e4567-e89b-12d3-a456-426614174000/1.2.3");
        CHECK(multiaddr.has_value());
        CHECK_EQ(multiaddr.value().version, SemanticVersion{1, 2, 3});
        CHECK_EQ(multiaddr.value().to_string(),
                "/btle/123e4567-e89b-12d3-a456-426614174000/1.2.3");
    }

    SUBCASE("No semver present") {
        auto multiaddr =
                Multiaddr::parse("/btle/123e4567-e89b-12d3-a456-426614174000");
        CHECK(multiaddr.has_value());
        CHECK_EQ(multiaddr.value().to_string(),
                "/btle/123e4567-e89b-12d3-a456-426614174000");
    }
}

TEST_CASE("Multiaddr packed representation") {}
