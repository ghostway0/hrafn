#include <algorithm>
#include <cstdint>
#include <ctime>
#include <cwchar>
#include <expected>
#include <fmt/ranges.h>
#include <memory>
#include <optional>
#include <span>
#include <spdlog/spdlog.h>
#include <variant>
#include <vector>

#include <absl/crc/crc32c.h>
#include <absl/log/globals.h>
#include <absl/log/initialize.h>
#include <absl/log/log.h>
#include <absl/strings/escaping.h>
#include <absl/strings/str_split.h>
#include <asio.hpp>
#include <asio/experimental/channel.hpp>
#include <sodium/crypto_box.h>
#include <sodium/crypto_hash_sha256.h>
#include <sodium/crypto_sign.h>

#include "crypto.h"
#include "error_utils.h"
#include "messages.pb.h"
#include "multiaddr.h"

constexpr uint8_t kVersion = 42;
constexpr uint32_t kHandshakeMessageMaxSize = 1024;
constexpr time_t kHandshakeAcceptableTimeDrift = 32;

struct Stream {
    virtual ~Stream() = default;
    // probably should use asio's abstractions
    virtual asio::awaitable<std::expected<void, asio::error_code>> read(
            std::span<uint8_t>) = 0;
    virtual asio::awaitable<std::expected<void, asio::error_code>> write(
            std::span<uint8_t>) = 0;
};

template<typename T, typename S>
std::vector<uint8_t> serialize_to_bytes(S const *obj) {
    std::vector<uint8_t> bytes(obj->ByteSizeLong());
    obj->SerializeToArray(bytes.data(), bytes.size());
    return bytes;
}

template<typename T, typename S, size_t kSize>
asio::awaitable<std::expected<S, asio::error_code>> stream_read_type(
        std::shared_ptr<Stream> stream) {
    std::vector<uint8_t> buffer(kSize);
    co_await stream->read(buffer);

    T root;
    if (!root.ParseFromArray(buffer.data(), buffer.size())) {
        co_return std::unexpected{asio::error::invalid_argument};
    }

    co_return root;
}

// stream should outlive everything, but I'm not sure.

template<typename T, typename S>
asio::awaitable<std::expected<void, asio::error_code>> stream_write_type(
        std::shared_ptr<Stream> stream, S val) {
    std::vector<uint8_t> bytes = serialize_to_bytes<T>(&val);
    co_return co_await stream->write(bytes);
}

struct Contact {
    std::optional<std::string> name;
    std::vector<Multiaddr> known_addrs;
    Pubkey pubkey;
};

struct Identity {
    Pubkey pubkey;
    Privkey privkey;
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
    std::shared_ptr<Stream> stream;
    std::optional<Contact> contact = std::nullopt;

    static asio::awaitable<std::expected<Connection, HandshakeError>> negotiate(
            std::shared_ptr<Stream> stream, std::optional<Pubkey> pubkey);
};

struct HandshakeMessage {
    uint32_t flags;
    uint64_t timestamp;
    std::optional<Pubkey> pubkey;

    doomday::HandshakeMessage pack() const {
        doomday::HandshakeMessage handshake;

        handshake.set_flags(flags);
        handshake.set_timestamp(timestamp);
        handshake.set_checksum(0);
        handshake.set_version(kVersion);

        if (pubkey) {
            auto *data =
                    handshake.mutable_pubkey()->mutable_limbs()->mutable_data();
            std::copy(pubkey->data().begin(), pubkey->data().end(), data);
        }

        return handshake;
    }
};

std::optional<HandshakeError> validate_handshake(
        doomday::HandshakeMessage const &received_handshake,
        std::optional<Pubkey> const &pubkey,
        uint64_t timestamp) {
    if (received_handshake.version() != kVersion) {
        return HandshakeError::InvalidVersion;
    }

    std::string bytes = received_handshake.SerializeAsString();

    if (absl::crc32c_t{received_handshake.checksum()}
            != absl::ComputeCrc32c(bytes)) {
        return HandshakeError::InvalidChecksum;
    }

    auto received_timestamp = received_handshake.timestamp();
    if (std::abs(static_cast<int64_t>(received_timestamp - timestamp))
            > kHandshakeAcceptableTimeDrift) {
        return HandshakeError::InvalidTimestamp;
    }

    if (pubkey) {
        if (!received_handshake.has_pubkey()) {
            return HandshakeError::InvalidPubkey;
        }

        Pubkey remote_pubkey{received_handshake.pubkey()};
        std::string const &sig = received_handshake.signature();

        absl::Span<uint8_t const> bytes_span{
                reinterpret_cast<uint8_t const *>(bytes.data()), bytes.size()};
        absl::Span<uint8_t const> sig_span{
                reinterpret_cast<uint8_t const *>(sig.data()), sig.size()};

        if (!remote_pubkey.verify(bytes_span, sig_span)) {
            return HandshakeError::InvalidSignature;
        }
    }

    return std::nullopt;
}

asio::awaitable<std::expected<Connection, HandshakeError>>
Connection::negotiate(
        std::shared_ptr<Stream> stream, std::optional<Pubkey> pubkey) {
    co_await stream_write_type<doomday::HandshakeMessage>(stream,
            HandshakeMessage{0, static_cast<uint64_t>(time(nullptr)), pubkey}
                    .pack());

    auto received_handshake = ({
        auto handshake_or = co_await stream_read_type<doomday::HandshakeMessage,
                doomday::HandshakeMessage,
                kHandshakeMessageMaxSize>(stream);
        co_try_unwrap_or(handshake_or, HandshakeError::InvalidFormat);
    });

    time_t timestamp = time(nullptr);
    if (std::optional<HandshakeError> validation_error =
                    validate_handshake(received_handshake, pubkey, timestamp);
            validation_error.has_value()) {
        co_return std::unexpected(validation_error.value());
    }

    co_return Connection{.stream = stream};
}

struct Message {
    std::vector<uint8_t> data;
    // should use an internal header that packs into it
    doomday::MessageHeader header;
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
            Contact contact = connection.contact.value();

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
        std::vector<uint8_t> header_bytes =
                serialize_to_bytes<doomday::MessageHeader>(&message.header);

        co_await connection.stream->write(header_bytes);
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
    asio::io_context ctx;

    Central central(ctx);
    BluetoothDiscovery discovery_service{central.events()};

    asio::co_spawn(ctx, central.run(), asio::detached);
    asio::co_spawn(ctx, discovery_service.run(), asio::detached);

    ctx.run();

    return 0;
}
