#include "btle/corebluetooth/bt.h"
#include <string>

#ifdef __APPLE__

#include "btle/corebluetooth/cbtle.h"

#elifdef __LINUX__

#include "btle/bluez/bluez_btle.h"

#endif

#include <asio.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/experimental/channel.hpp>
#include <spdlog/spdlog.h>
#include <tbb/concurrent_map.h>

#include "btle/types.h"
#include "messages.pb.h"

#include "crypto/crypto.h"
#include "net/net.h"

struct Packet {
    Pubkey from;
    Signature signature;
    std::vector<uint8_t> data;
    uint64_t timestamp;
    uint64_t checksum;

    static Packet from_proto(std::vector<uint8_t> const &data) {
        hrafn::Packet proto_packet;
        proto_packet.ParseFromArray(data.data(), data.size());

        // FIXME: bad naming
        std::vector<uint8_t> data_bytes{
                proto_packet.data().begin(), proto_packet.data().end()};

        return Packet{
                .from = Pubkey::from_stringbytes(proto_packet.from()),
                .signature =
                        Signature::from_stringbytes(proto_packet.signature()),
                .data = data_bytes,
                .timestamp = proto_packet.timestamp(),
                .checksum = proto_packet.checksum(),
        };
    }
};

using StreamChannel = asio::experimental::channel<void(
        std::error_code, std::unique_ptr<Stream>)>;
using DataChannel = asio::experimental::channel<void(
        std::error_code, std::unique_ptr<Packet>)>;

class StreamMultiplexer {
public:
    explicit StreamMultiplexer(
            StreamChannel &stream_channel, asio::io_context &ctx)
        : central_adapter_{ctx}, streams_{}, stream_channel_{stream_channel} {}

    tbb::concurrent_map<Pubkey, DataChannel> &streams_channel() {
        return streams_;
    }

    asio::awaitable<void> run(asio::io_context &ctx) {
        central_adapter_.on_discovery(
                [this](Peripheral &peripheral,
                        AdvertisingData const &) -> asio::awaitable<void> {
                    central_adapter_.connect(peripheral, ConnectOptions{});
                    spdlog::info("discovered peripheral {}({})",
                            peripheral.name(),
                            peripheral.uuid());
                    // stream_channel_.try_send(asio::error_code{},
                    //         std::make_unique<Stream>(peripheral));
                    co_return;
                });

        // adapter_.on_data_received(
        //         [this](std::error_code ec, std::vector<uint8_t> data) {
        //             auto packet = Packet::from_proto(data);
        //
        //             streams_.at(packet.from).try_send(ec, packet);
        //         });

        // asio::co_spawn(ctx, [&ctx, this]() -> asio::awaitable<void> {
        //     while (true) {
        //         adapter_.start_scanning();
        //         asio::steady_timer timer{ctx, 30s};
        //         co_await timer.async_wait(asio::use_awaitable);
        //
        //         adapter_.stop_scanning();
        //     }
        // });

        peripheral_adapter_.start_advertising(
                AdvertisingOptions{.local_name = "hrafn"});

        // TODO: start to listen for commands
        // while (running) {}
        co_return;
    }

private:
    CentralAdapter central_adapter_;
    PeripheralAdapter peripheral_adapter_{};
    tbb::concurrent_map<Pubkey, DataChannel> streams_;
    StreamChannel &stream_channel_;
};

//
// CharacteristicProperties properties,
// Permissions permissions,
// std::vector<uint8_t> value);

void todo(std::string_view msg) {
    spdlog::error("TODO: {}", msg);
    std::terminate();
}

enum class Property {
    Read,
    Write,
    Notify,
    Indicate,
};

class CharacteristicBuilder {
public:
    explicit CharacteristicBuilder(UUID uuid) : uuid_{uuid} {}

    CharacteristicBuilder &set_permissions(Permissions permission) {
        permissions_ = permission;
        return *this;
    }

    CharacteristicBuilder &add_property(CharacteristicProperties property) {
        properties_ =
                static_cast<CharacteristicProperties>(properties_ | property);
        return *this;
    }

    CharacteristicBuilder &set_cached_value(std::vector<uint8_t> value) {
        todo("This is not implemented yet");

        value_ = std::move(value);
        return *this;
    }

    ManagedCharacteristic build() {
        return ManagedCharacteristic{uuid_, properties_, permissions_, value_};
    }

private:
    CharacteristicProperties properties_{};
    std::optional<std::vector<uint8_t>> value_ = std::nullopt;
    Permissions permissions_{};
    UUID uuid_;
};

class ServiceBuilder {
public:
    explicit ServiceBuilder(UUID uuid) : service_{uuid} {}

    void add_characteristic(ManagedCharacteristic characteristic) {
        service_.add_characteristic(std::move(characteristic));
    }

    ManagedService build() { return service_; }

private:
    ManagedService service_;
};
