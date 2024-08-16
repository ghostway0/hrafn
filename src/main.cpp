#include <array>
#include <condition_variable>
#include <ctime>
#include <expected>
#include <optional>
#include <span>
#include <system_error>
#include <variant>
#include <vector>

#include <asio.hpp>
#include <asio/experimental/channel.hpp>
#include <sodium/crypto_sign.h>
#include <spdlog/spdlog.h>

#include "generated/messages_generated.h"

constexpr uint8_t kVersion = 42;
constexpr uint32_t kHandshakeMessageMaxSize = 1024;

class Pubkey {
public:
    Pubkey() = default;

    explicit Pubkey(std::array<uint8_t, 32> bytes) : bytes_{bytes} {}

    explicit Pubkey(Doomday::Ed25519FieldPoint const &point) : bytes_{} {
        std::copy(point.limbs()->begin(), point.limbs()->end(), bytes_.begin());
    }

    std::array<uint8_t, 32> const &data() const { return bytes_; }

    Doomday::Ed25519FieldPoint pack() const {
        Doomday::Ed25519FieldPoint point;
        std::copy(bytes_.begin(), bytes_.end(), point.mutable_limbs()->Data());
        return point;
    }

    bool verify(std::span<uint8_t> bytes, std::span<uint8_t> signature) {
        return crypto_sign_verify_detached(signature.data(),
                       bytes.data(),
                       bytes.size_bytes(),
                       bytes_.data())
                == 0;
    }

    bool operator==(const Pubkey& other) const {
        // not secret data
        return other.bytes_ == bytes_;
    }

private:
    std::array<uint8_t, 32> bytes_;
};

class Privkey {};

class Keypair {};

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
    flatbuffers::FlatBufferBuilder builder;

    auto offset = T::Pack(builder, obj);
    builder.Finish(offset);

    uint8_t *buf = builder.GetBufferPointer();
    size_t size = builder.GetSize();

    return {buf, buf + size};
}

template<typename T, typename S, size_t kSize>
asio::awaitable<std::expected<S, asio::error_code>> stream_read_type(
        Stream &stream) {
    std::vector<uint8_t> buffer(kSize);
    co_await stream.read(buffer);

    auto verifier = flatbuffers::Verifier(buffer.data(), buffer.size());

    if (!verifier.VerifyBuffer<T>()) {
        co_return std::unexpected{asio::error::invalid_argument};
    }

    T const *root = flatbuffers::GetRoot<T>(buffer.data());
    co_return *root->UnPack();
}

template<typename T, typename S>
asio::awaitable<std::expected<void, asio::error_code>> stream_write_type(
        Stream &stream, S val) {
    std::vector<uint8_t> bytes = serialize_to_bytes<T>(val);
    co_return co_await stream.write(bytes);
}

// TODO(ghostway)
struct Multiaddr {};

struct Contact {
    std::optional<std::string> name;
    std::vector<Multiaddr> known_addrs;
    Pubkey pubkey;
};

struct Identity {
    Pubkey pubkey;
    Privkey privkey;
};

struct Connection {
    Stream &stream;
    std::optional<Contact> contact = std::nullopt;

    static asio::awaitable<std::expected<Connection, int>> negotiate(
            Stream &stream, std::optional<Pubkey> pubkey);
};

struct HandshakeMessage {
    uint32_t flags;
    uint64_t timestamp;
    std::optional<Pubkey> pubkey;

    Doomday::HandshakeMessageT pack() const {
        Doomday::HandshakeMessageT handshake;

        handshake.flags = flags;
        handshake.timestamp = timestamp;
        handshake.checksum = 0;
        handshake.version = kVersion;

        if (pubkey) {
            handshake.pubkey = std::make_unique<Doomday::Ed25519FieldPoint>(
                    pubkey.value().pack());
        }

        return handshake;
    }
};

asio::awaitable<std::expected<Connection, int>> Connection::negotiate(
        Stream &stream, std::optional<Pubkey> pubkey) {
    auto handshake =
            HandshakeMessage{0, static_cast<uint64_t>(time(nullptr)), pubkey}
                    .pack();

    co_await stream_write_type<Doomday::HandshakeMessage>(stream, &handshake);

    Pubkey remote_pubkey{};
    co_await stream_read_type<Doomday::HandshakeMessage,
            Doomday::HandshakeMessageT,
            kHandshakeMessageMaxSize>(stream);

    if (!remote_pubkey.verify({}, {})) {
        co_return std::unexpected(0);
    }

    // TODO(ghostway)

    co_return Connection{.stream = stream};
}

struct Message {
    std::vector<uint8_t> data;
    // should use an internal header that packs into it
    Doomday::MessageHeaderT header;
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
                serialize_to_bytes<Doomday::MessageHeader>(&message.header);

        co_await connection.stream.write(header_bytes);
        co_await connection.stream.write(message.data);
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
