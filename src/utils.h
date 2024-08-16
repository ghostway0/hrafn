#include <expected>
#include <array>

enum class Error {
    ParseInvalidFormat,
    ParseConversionFailed,
};

struct UUID {
    std::array<uint8_t, 16> bytes;

    static std::expected<UUID, Error> parse(std::string_view str);
    std::string to_string() const;
};
