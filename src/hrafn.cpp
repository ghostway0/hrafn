#include <algorithm>
#include <cstdint>
#include <ctime>
#include <cwchar>
#include <expected>
#include <memory>
#include <mutex>
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
#include <asio/error_code.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/experimental/channel.hpp>
#include <fmt/ranges.h>
#include <sodium/crypto_box.h>
#include <sodium/crypto_hash_sha256.h>
#include <sodium/crypto_sign.h>
#include <spdlog/spdlog.h>

#include "asio/co_spawn.hpp"
#include "asio/io_context.hpp"
#include "asio/steady_timer.hpp"
#include "asio/use_awaitable.hpp"
#include "btle/corebluetooth/mutable_characteristic.h"
#include "crypto/crypto.h"
#include "messages.pb.h"
#include "utils/error_utils.h"
#include "utils/multiaddr.h"
#include "utils/semantic_version.h"

using namespace std::chrono_literals;

constexpr SemanticVersion kVersion = {0, 0, 0};
constexpr uint32_t kHandshakeMessageMaxSize = 1024;
constexpr absl::Duration kSyncInterval = absl::Minutes(2);

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
    time_t last_sync;
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
    std::mutex mutex;
    Contact contact;

    static asio::awaitable<std::expected<Connection, HandshakeError>> negotiate(
            std::unique_ptr<Stream> stream, Pubkey const &pubkey);
};

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
        for (Message &message : messages_) {
            // FIXME: this does not work
            if (message.header.timestamp() < connection.contact.last_sync) {
                continue;
            }

            if (mode == SyncMode::Full) {
                co_await sync_one(connection, message);
                continue;
            }

            // NOLINTNEXTLINE
            if (std::find(message.recipients.begin(),
                        message.recipients.end(),
                        connection.contact.pubkey)
                    != message.recipients.end()) {
                co_await sync_one(connection, message);
            }
        }

        // weird.
        co_return std::expected<void, asio::error_code>{};
    }

private:
    // should be a db or lru
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
    explicit BluetoothDiscovery() = default;

    asio::awaitable<void> run() { co_return; }

private:
};

struct Context {
    asio::io_context &executor;
    Keypair keypair;
    std::vector<Contact> contact_list;
    Syncer syncer;
    std::atomic<bool> running{true};
    // error stack?
};

asio::awaitable<void> handle_messages(Connection &connection) {
    while (connection.stream->valid()) {
        auto header = co_await stream_read_type<hrafn::MessageHeader,
                hrafn::MessageHeader,
                1024>(connection.stream);

        if (!header.has_value()) {
            continue;
        }

        std::vector<uint8_t> data(header.value().size());
        co_await connection.stream->read(data);
    }
}

asio::awaitable<void> periodic_sync(Connection &connection, Context &ctx) {
    while (ctx.running.load(std::memory_order_relaxed)
            && connection.stream->valid()) {
        {
            std::scoped_lock lock(connection.mutex);
            co_await ctx.syncer.sync(connection, SyncMode::Full);
        }

        asio::steady_timer timer(
                ctx.executor, absl::ToChronoSeconds(kSyncInterval));
    }
}

asio::awaitable<void> start_connection(
        std::unique_ptr<Stream> stream, Context &ctx) {
    // if in contact list, set contact, and use the pubkey to negotiate
    // otherwise, use an ephemeral keypair to negotiate

    auto connection = co_await Connection::negotiate(
            std::move(stream), ctx.keypair.pubkey);

    if (!connection.has_value()) {
        // error
        co_return;
    }

    asio::co_spawn(
            ctx.executor, handle_messages(connection.value()), asio::detached);

    asio::co_spawn(ctx.executor,
            periodic_sync(connection.value(), ctx),
            asio::detached);

    while (ctx.running.load(std::memory_order_relaxed)
            && connection->stream->valid()) {
    }
}

// the multiplexer has a queue of commands (which type's given by a template?). It will send the command to the right connection handler.
// or maybe the multiplexer shouldn't know about commands, and instead knows about 'spans'
// so it would have a map of spans and it would choose one and send only events that are related to one span

// class Network: multiplexer


class ConnectionMultiplexer {
public:
    explicit ConnectionMultiplexer(Context &ctx)
        : incoming_streams_{ctx.executor}, ctx_{ctx} {}

    asio::awaitable<void> run() {
        while (ctx_.running.load(std::memory_order_relaxed)) {
            std::unique_ptr<Stream> stream =
                    co_await incoming_streams_.async_receive(
                            asio::use_awaitable);

            asio::co_spawn(ctx_.executor,
                    start_connection(std::move(stream), ctx_),
                    asio::detached);
        }
    }

private:
    asio::experimental::channel<void(std::error_code, std::unique_ptr<Stream>)>
            incoming_streams_;
    // peers to connection ids (should we use tbb or something?)
    std::unordered_map<PeerId, size_t> connection_ids_;
    std::mutex connections_mutex_;

    Context &ctx_;
};

asio::awaitable<void> bluetooth_service(Context &ctx) {
    BluetoothDiscovery discovery;
    asio::co_spawn(ctx.executor, discovery.run(), asio::detached);

    co_return;
}

int main() {
    Adapter adapter{};
    adapter.on_discovery([](UUID uuid, AdvertisingData) {
        spdlog::info("Discovered peripheral: {}", uuid.to_string());
    });

    asio::io_context ctx;

    Context app_ctx{
            .executor = ctx,
            .keypair = Keypair::generate(),
            .contact_list = {},
    };

    ctx.run();

    return 0;
}
