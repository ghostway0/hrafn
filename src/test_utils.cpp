#include <expected>
#include <string>
#include <string_view>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "multiaddr.h"
#include "semantic_version.h"
#include "uuid.h"
// #include "crypto.h"

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

TEST_CASE("Multiaddr parse/to_string") {
    SUBCASE("Semver before protocol") {
        auto multiaddr = Multiaddr::parse(
                "/1.2.3/btu/123e4567-e89b-12d3-a456-426614174000");
        CHECK(!multiaddr.has_value());
    }

    SUBCASE("Semver between protocol and address") {
        auto multiaddr = Multiaddr::parse(
                "/btu/1.2.3/123e4567-e89b-12d3-a456-426614174000");
        CHECK(!multiaddr.has_value());
    }

    SUBCASE("Semver after address") {
        auto multiaddr = Multiaddr::parse(
                "/btu/123e4567-e89b-12d3-a456-426614174000/1.2.3");
        CHECK(multiaddr.has_value());
        CHECK_EQ(multiaddr.value().version, SemanticVersion{1, 2, 3});
        CHECK_EQ(multiaddr.value().to_string(),
                "/btu/123e4567-e89b-12d3-a456-426614174000/1.2.3");
    }

    SUBCASE("No semver present") {
        auto multiaddr =
                Multiaddr::parse("/btu/123e4567-e89b-12d3-a456-426614174000");
        CHECK(multiaddr.has_value());
        CHECK_EQ(multiaddr.value().to_string(),
                "/btu/123e4567-e89b-12d3-a456-426614174000");
    }
}

// TEST_CASE("Crypto") {
//     Keypair keypair = generate();

//     std::vector<uint8_t> message{0x13, 0x37};

//     std::vector<uint8_t> signature = keypair.privkey.sign(message);
//     CHECK(keypair.pubkey.verify(message, signature));
// }
