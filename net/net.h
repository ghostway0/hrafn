#include <expected>
#include <span>

#include <asio.hpp>

/// a bidirectional stream of data
/// guarantees:
/// - the packets that _are_ received are correct and full
struct Stream {
    virtual ~Stream() = default;
    // probably should use asio's abstractions
    virtual asio::awaitable<std::expected<void, asio::error_code>> read(
            std::span<uint8_t>) = 0;
    virtual asio::awaitable<std::expected<void, asio::error_code>> write(
            std::span<uint8_t>) = 0;

    asio::awaitable<std::expected<void, asio::error_code>> write(
            auto const *obj) {
        std::vector<uint8_t> bytes(obj->ByteSizeLong());
        obj->SerializeToArray(bytes.data(), bytes.size());
        co_try_unwrap(co_await write(bytes));
        co_return std::expected<void, asio::error_code>{};
    }
};
