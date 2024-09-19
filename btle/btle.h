#include <string>

#ifdef __APPLE__

#include "btle/corebluetooth/cbtle.h"

#elifdef __LINUX__

#include "btle/bluez/bluez_btle.h"

#endif

#include <tbb/concurrent_map.h>
#include <asio.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/experimental/channel.hpp>

#include "btle/types.h"
#include "messages.pb.h"

#include "crypto/crypto.h"
#include "net/net.h"

Adapter get_default_adapter();

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
using DataChannel = asio::experimental::channel<void(std::error_code, std::unique_ptr<Packet>)>;

class StreamMultiplexer {
public:
    explicit StreamMultiplexer(StreamChannel &stream_channel)
        : adapter_{get_default_adapter()}, streams_{},
          stream_channel_{stream_channel} {}

    tbb::concurrent_map<Pubkey, DataChannel> &streams_channel() {
        return streams_;
    }

    asio::awaitable<void> run(asio::io_context &ctx) {
        adapter_.on_discovery(
                [this](Peripheral &peripheral, AdvertisingData const &) {
                    adapter_.connect(peripheral, ConnectOptions{});
                    // stream_channel_.try_send(asio::error_code{},
                    //         std::make_unique<Stream>(peripheral));
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

        adapter_.start_advertising(AdvertisingOptions{.local_name = "hrafn"});

        // TODO: start to listen for commands
        // while (running) {}
        co_return;
    }

private:
    Adapter adapter_;
    tbb::concurrent_map<Pubkey, DataChannel> streams_;
    StreamChannel &stream_channel_;
};
