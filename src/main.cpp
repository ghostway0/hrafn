#include <array>
#include <ctime>
#include <expected>
#include <optional>
#include <span>
#include <spdlog/spdlog.h>
#include <variant>
#include <vector>

#include <absl/strings/str_split.h>
#include <asio.hpp>
#include <asio/experimental/channel.hpp>
#include <sodium/crypto_sign.h>

#include "error.h"
#include "messages.pb.h"
#include "uuid.h"

constexpr uint8_t kVersion = 42;
constexpr uint32_t kHandshakeMessageMaxSize = 1024;

class Pubkey {
public:
    Pubkey() = default;

    explicit Pubkey(std::array<uint8_t, 32> bytes) : bytes_{bytes} {}

    std::array<uint8_t, 32> const &data() const { return bytes_; }

    bool verify(std::span<uint8_t> bytes, std::span<uint8_t> signature) {
        return crypto_sign_verify_detached(signature.data(),
                       bytes.data(),
                       bytes.size_bytes(),
                       bytes_.data())
                == 0;
    }

    bool operator==(Pubkey const &other) const {
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
    std::vector<uint8_t> bytes;
    obj->SerializeToArray(bytes.data(), bytes.size());
    return bytes;
}

template<typename T, typename S, size_t kSize>
asio::awaitable<std::expected<S, asio::error_code>> stream_read_type(
        Stream &stream) {
    std::vector<uint8_t> buffer(kSize);
    co_await stream.read(buffer);

    T root;
    if (!root.ParseFromArray(buffer.data(), buffer.size())) {
        co_return std::unexpected{asio::error::invalid_argument};
    }

    co_return root;
}

template<typename T, typename S>
asio::awaitable<std::expected<void, asio::error_code>> stream_write_type(
        Stream &stream, S val) {
    std::vector<uint8_t> bytes = serialize_to_bytes<T>(&val);
    co_return co_await stream.write(bytes);
}

struct SemanticVersion {
    size_t major;
    size_t minor;
    size_t patch;

    static std::expected<SemanticVersion, ParseError> parse(std::string_view str);

    auto operator<=>(SemanticVersion const &) const = default;
};

std::expected<SemanticVersion, ParseError> SemanticVersion::parse(
        std::string_view str) {
    std::vector<std::string_view> parts = absl::StrSplit(str, '.');
    if (parts.size() != 3) {
        return std::unexpected(ParseError::ParseInvalidFormat);
    }

    SemanticVersion version{};
    if (!absl::SimpleAtoi(parts[0], &version.major)
            || !absl::SimpleAtoi(parts[1], &version.minor)
            || !absl::SimpleAtoi(parts[2], &version.patch)) {
        return std::unexpected(ParseError::ParseInvalidFormat);
    }

    return version;
}

struct Protocol {
    std::string name;
    size_t size;
    uint8_t code;
    bool sized;

    virtual ~Protocol() = 0;
    virtual std::string transcode() const = 0;
};

Protocol::~Protocol() = default;

struct BluetoothAddress : Protocol {
    UUID address;

    std::string transcode() const override { return address.to_string(); }
};

struct Multiaddr {
    SemanticVersion version;

    static std::expected<Multiaddr, ParseError> parse(std::string_view str);

    auto operator<=>(Multiaddr const &) const = default;
};

std::expected<Multiaddr, ParseError> Multiaddr::parse(
        [[maybe_unused]] std::string_view str) {
    return {};
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

asio::awaitable<std::expected<Connection, int>> Connection::negotiate(
        Stream &stream, std::optional<Pubkey> pubkey) {
    auto handshake =
            HandshakeMessage{0, static_cast<uint64_t>(time(nullptr)), pubkey}
                    .pack();

    co_await stream_write_type<doomday::HandshakeMessage>(stream, handshake);

    Pubkey remote_pubkey{};
    auto result = co_await stream_read_type<doomday::HandshakeMessage,
            doomday::HandshakeMessage,
            kHandshakeMessageMaxSize>(stream);

    if (!result.has_value()) {
        co_return std::unexpected(0);
    }

    if (!remote_pubkey.verify({}, {})) {
        co_return std::unexpected(0);
    }

    // TODO(ghostway)

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

#define try_unwrap(x) \
    ({ \
        auto _x = x; \
        if (!_x.has_value()) { \
            return _x.error(); \
        } \
        _x.value(); \
    })

int main() {
    asio::io_context ctx;

    auto val = UUID::parse("00001101-0000-1000-8000-00805F9B34FB");
    if (!val.has_value()) {
        return 2;
    }

    spdlog::info("{}", val.value().to_string());

    Central central(ctx);
    BluetoothDiscovery discovery_service{central.events()};

    asio::co_spawn(ctx, central.run(), asio::detached);
    asio::co_spawn(ctx, discovery_service.run(), asio::detached);

    ctx.run();

    return 0;
}
