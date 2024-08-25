#pragma once

#include <functional>
#include <vector>

#include <asio/experimental/channel.hpp>

#include "bt.h"
#include "btle/types.h"
#include "utils/uuid.h"
#include "net/net.h"

// Design:
// DirectStream : read/write
//
// DiscoveryService -> `DirectStream`s that are given to the Central
//
//
// can't do that. 
// need a ConnectionMultiplexer. every message here would be sent to the multiplexer and then to the correct stream, if not direct otherwise to the Central.
// need to have a map from PeerId to Stream (it will be verified there too).
// writing to a stream would be more direct: every BluetoothStream would have a reference to the Adapter.

class BluetoothStream : public Stream {
public:

private:
    // bidirectional channel (asio)
};

struct Delegate {
    DiscoveredPeripheralCallback discovered_peripheral_callback;
    ConnectionCallback connection_callback;
    SubscriptionCallback subscription_callback;
    DataReceivedCallback data_received_callback;
};

class Adapter {
public:
    using DiscoveredPeripheralCallback =
            std::function<void(UUID, AdvertisingData)>;
    using ConnectionCallback = std::function<void(UUID, bool)>;
    using SubscriptionCallback = std::function<void(UUID, UUID)>;
    using DataReceivedCallback =
            std::function<bool(UUID, std::vector<uint8_t>)>;

    explicit Adapter(Delegate const &delegate);
    ~Adapter();

    void start_scanning();
    void stop_scanning();

    void start_advertising(AdvertisingData const &data);
    void stop_advertising();

    void connect_to_peripheral(const UUID &uuid);

    void disconnect_from_peripheral(const UUID &uuid);

    void add_service(std::string const &service_uuid,
            std::vector<Characteristic> const &characteristics);

private:
    cb::CBCentralManagerWrapper central_manager_wrapper_;
    cb::CBPeripheralManagerWrapper peripheral_manager_wrapper_;
    // map from uuid -> stream
    // somehow need to subscribe to the channels and write with cb
};
