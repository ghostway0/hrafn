#include <algorithm>
#include <cstdint>
#include <ctime>
#include <cwchar>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include <absl/crc/crc32c.h>
#include <absl/log/globals.h>
#include <absl/log/initialize.h>
#include <absl/log/log.h>
#include <absl/strings/escaping.h>
#include <absl/strings/str_split.h>
#include <absl/time/time.h>
#include <asio.hpp>
#include <asio/experimental/channel.hpp>
#include <fmt/ranges.h>
#include <sodium/crypto_box.h>
#include <sodium/crypto_hash_sha256.h>
#include <sodium/crypto_sign.h>
#include <spdlog/spdlog.h>

#include "asio/error_code.hpp"
#include "crypto/crypto.h"
#include "messages.pb.h"
#include "utils/error_utils.h"
#include "utils/multiaddr.h"
#include "utils/semantic_version.h"

#include "btle/btle.h"

constexpr SemanticVersion kVersion = {0, 0, 0};
constexpr uint32_t kHandshakeMessageMaxSize = 1024;

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
        co_return std::expected<void, asio::error_code>();
    }
};

template<typename T, typename S>
std::vector<uint8_t> serialize_to_bytes(S const *obj) {
    std::vector<uint8_t> bytes(obj->ByteSizeLong());
    obj->SerializeToArray(bytes.data(), bytes.size());
    return bytes;
}

template<typename T, typename S, size_t kSize>
asio::awaitable<std::expected<S, asio::error_code>> stream_read_type(
        std::unique_ptr<Stream> &stream) {
    std::vector<uint8_t> buffer(kSize);
    co_await stream->read(buffer);

    T root;
    if (!root.ParseFromArray(buffer.data(), buffer.size())) {
        co_return std::unexpected{asio::error::invalid_argument};
    }

    co_return root;
}

template<typename T, typename S>
asio::awaitable<std::expected<void, asio::error_code>> stream_write_type(
        std::unique_ptr<Stream> &stream, S val) {
    std::vector<uint8_t> bytes = serialize_to_bytes<T>(&val);
    co_return co_await stream->write(bytes);
}

struct Contact {
    std::optional<std::string> name;
    std::vector<Multiaddr> known_addrs;
    Pubkey pubkey;
};

enum class HandshakeError {
    InvalidFormat,
    InvalidVersion,
    InvalidChecksum,
    InvalidSignature,
    InvalidPubkey,
    InvalidTimestamp,
};

struct Connection {
    std::unique_ptr<Stream> stream;
    std::optional<Contact> contact = std::nullopt;

    static asio::awaitable<std::expected<Connection, HandshakeError>> negotiate(
            std::unique_ptr<Stream> stream, Pubkey const &pubkey);
};

// TODO(ghostway): I think I should have a SignedMessage message in the
// protobuf.

// protocol:
// 1. handshake:
// - peer id
// - timestamp
// - checksum
// - flags?
// 2. stream of messages:
// messages should be pretty much strip down as much as possible
// message header have:
// - timestamp
// - checksum
// - (bloom) filter?
// then bytes
// the inner messages structure:
// - sender
// - signature (OTR?) or should this be in an encrypted header?
// - ratchet slot?
// - associated id (full messages might be split across multiple small messages)

struct HandshakeMessage {
    uint32_t flags;
    PeerId peer_id;

    static HandshakeMessage generate(PeerId const &peer_id) {
        HandshakeMessage message{
                .flags = 0,
                .peer_id = peer_id,
        };

        return message;
    }

    hrafn::HandshakeMessage proto() const {
        hrafn::HandshakeMessage message;
        message.set_flags(flags);
        message.set_peer_id(peer_id.to_base64());
        return message;
    }
};

asio::awaitable<std::expected<Connection, HandshakeError>>
Connection::negotiate(std::unique_ptr<Stream> stream, Pubkey const &pubkey) {
    auto message =
            HandshakeMessage::generate(PeerId::from_pubkey(pubkey)).proto();
    co_await stream->write(&message);

    auto handshake = ({
        auto handshake_or = co_await stream_read_type<hrafn::HandshakeMessage,
                hrafn::HandshakeMessage,
                kHandshakeMessageMaxSize>(stream);
        co_try_unwrap_or(handshake_or, HandshakeError::InvalidFormat);
    });

    co_return Connection{.stream = std::move(stream)};
}

struct Message {
    std::vector<uint8_t> data;
    // should use an internal header that packs into it
    hrafn::MessageHeader header;
    std::vector<Pubkey> recipients;
};

enum class SyncMode : uint8_t {
    Full,
    Direct,
};

class Syncer {
public:
    Syncer() = default;

    void add_message(Message const &message) { messages_.push_back(message); }

    asio::awaitable<std::expected<void, asio::error_code>> sync(
            Connection &connection, SyncMode mode) {
        if (mode == SyncMode::Direct && !connection.contact.has_value()) {
            co_return std::unexpected(asio::error::invalid_argument);
        }

        for (Message &message : messages_) {
            if (mode == SyncMode::Full) {
                co_await sync_one(connection, message);
                continue;
            }

            // NOLINTNEXTLINE
            Contact contact = std::move(connection.contact.value());

            if (std::find(message.recipients.begin(),
                        message.recipients.end(),
                        contact.pubkey)
                    != message.recipients.end()) {
                co_await sync_one(connection, message);
            }
        }

        // weird.
        co_return std::expected<void, asio::error_code>{};
    }

private:
    std::vector<Message> messages_;

    asio::awaitable<void> sync_one(Connection &connection, Message message) {
        co_await connection.stream->write(&message.header);
        co_await connection.stream->write(message.data);
    }
};

using Event = std::variant<Message, Connection>;

using EventsChannel = asio::experimental::channel<void(std::error_code, Event)>;

class BluetoothDiscovery {
public:
    explicit BluetoothDiscovery(EventsChannel &events) : events_{events} {}

    asio::awaitable<void> run() {
        // TODO(ghostway)
        while (true) {
            // Connection connection = {};

            // co_await events_.async_send(asio::error_code{},
            //         std::move(connection),
            //         asio::use_awaitable);
        }
    }

private:
    EventsChannel &events_;
};

class Central {
public:
    explicit Central(asio::io_context &ctx) : events_{ctx} {}

    asio::awaitable<void> run() {
        while (true) {
            Event event = co_await events_.async_receive(asio::use_awaitable);
            co_await handle_event(event);
        }
    }

    asio::awaitable<void> handle_event(Event &event) {
        if (auto *message = std::get_if<Message>(&event)) {
            syncer_.add_message(*message);
        }

        if (auto *connection = std::get_if<Connection>(&event)) {
            co_await syncer_.sync(*connection, SyncMode::Full);
        }
    }

    EventsChannel &events() { return events_; }

private:
    EventsChannel events_;
    Syncer syncer_;
};

int main() {
    example();

    asio::io_context ctx;

    Central central(ctx);
    BluetoothDiscovery discovery_service{central.events()};

    asio::co_spawn(ctx, central.run(), asio::detached);
    asio::co_spawn(ctx, discovery_service.run(), asio::detached);

    ctx.run();

    return 0;
}
