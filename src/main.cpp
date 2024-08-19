#include <algorithm>
#include <array>
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
#include <asio.hpp>
#include <asio/experimental/channel.hpp>
#include <fmt/ranges.h>
#include <sodium/crypto_box.h>
#include <sodium/crypto_hash_sha256.h>
#include <sodium/crypto_sign.h>
#include <spdlog/spdlog.h>

#include "utils/error_utils.h"
#include "crypto/crc64.h"
#include "crypto/crypto.h"
#include "utils/multiaddr.h"
#include "messages.pb.h"

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
        std::unique_ptr<Stream> &stream) {
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
        std::unique_ptr<Stream> &stream, S val) {
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
    std::unique_ptr<Stream> stream;
    std::optional<Contact> contact = std::nullopt;

    static asio::awaitable<std::expected<Connection, HandshakeError>> negotiate(
            std::unique_ptr<Stream> stream, std::optional<Pubkey> pubkey);
};


    // bool verify() const {
    //     std::vector<uint8_t> data_to_verify{};

    //     auto flags_bytes = std::span(&flags, 1);
    //     data_to_verify.insert(
    //             data_to_verify.end(), flags_bytes.begin(), flags_bytes.end());

    //     auto timestamp_bytes = std::span(&timestamp, 1);
    //     data_to_verify.insert(data_to_verify.end(),
    //             timestamp_bytes.begin(),
    //             timestamp_bytes.end());

    //     if (pubkey_and_signature) {
    //         Pubkey const &pubkey = pubkey_and_signature.value().pubkey;
    //         std::array<uint8_t, kSignatureSize> const &signature =
    //                 pubkey_and_signature.value().signature;

    //         auto const &pubkey_data =
    //                 pubkey_and_signature.value().pubkey.data();
    //         data_to_verify.insert(data_to_verify.end(),
    //                 pubkey_data.begin(),
    //                 pubkey_data.end());

    //         bool signature_valid = pubkey.verify(
    //                 std::span<uint8_t const>(data_to_verify), signature);

    //         if (!signature_valid) {
    //             return false;
    //         }
    //     }

    //     uint64_t computed_checksum =
    //             crc64(std::span<uint8_t const>(data_to_verify));

    //     return computed_checksum == checksum;
    // }

// TODO(ghostway): I think I should have a SignedMessage message in the protobuf.

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
    Pubkey pubkey;
    Signature signature;
    time_t timestamp;
    chksum_t checksum;

    static HandshakeMessage generate(Keypair const &keypair) {
        HandshakeMessage message{
                .flags = 0,
                .pubkey = keypair.pubkey,
                .timestamp = std::time(nullptr),
        };

        std::vector<uint8_t> data_to_sign{};
        auto flags_bytes = std::span(&message.flags, 1);
        data_to_sign.insert(
                data_to_sign.end(), flags_bytes.begin(), flags_bytes.end());

        auto timestamp_bytes = std::span(&message.timestamp, 1);
        data_to_sign.insert(data_to_sign.end(),
                timestamp_bytes.begin(),
                timestamp_bytes.end());

        auto const &pubkey_data = message.pubkey.data();
        data_to_sign.insert(
                data_to_sign.end(), pubkey_data.begin(), pubkey_data.end());

        message.signature = Signature{keypair.privkey.sign(data_to_sign)};

        message.checksum = crc64(std::span<uint8_t const>(data_to_sign));

        return message;
    }
};

std::optional<HandshakeError> validate_handshake(
        HandshakeMessage const &handshake, uint64_t timestamp) {
    // if (handshake.version != kVersion) {
    //     return HandshakeError::InvalidVersion;
    // }

    // if (absl::crc32c_t{handshake.checksum()}
    //         != absl::ComputeCrc32c()) {
    //     return HandshakeError::InvalidChecksum;
    // }

    if (std::abs(static_cast<int64_t>(handshake.timestamp - timestamp))
            > kHandshakeAcceptableTimeDrift) {
        return HandshakeError::InvalidTimestamp;
    }

    // TODO(ghostway): cleanup
    // if (!handshake.verify()) {
    //     return HandshakeError::InvalidChecksum;
    // }

    return std::nullopt;
}

asio::awaitable<std::expected<Connection, HandshakeError>>
Connection::negotiate(
        std::unique_ptr<Stream> stream, std::optional<Pubkey> pubkey) {
    // co_await stream_write_type<doomday::HandshakeMessage>(
    //         stream, HandshakeMessage{.flags = 0, .pubkey = pubkey}.pack());

    // auto handshake = ({
    //     auto handshake_or = co_await stream_read_type<doomday::HandshakeMessage,
    //             doomday::HandshakeMessage,
    //             kHandshakeMessageMaxSize>(stream);
    //     co_try_unwrap_or(handshake_or, HandshakeError::InvalidFormat);
    // });

    // if (std::optional<HandshakeError> validation_error = validate_handshake(
    //             HandshakeMessage::unpack(handshake).value(), time(nullptr));
    //         validation_error.has_value()) {
    //     co_return std::unexpected(validation_error.value());
    // }

    co_return Connection{.stream = std::move(stream)};
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
