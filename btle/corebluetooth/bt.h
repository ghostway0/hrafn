#pragma once

#include <functional>
#include <string>
#include <vector>

#include "btle/btle.h"
#include "utils/uuid.h"

using DiscoveredPeripheralCallback =
        std::function<void(const UUID &, std::string const &)>;
using ConnectionCallback = std::function<void(const UUID &, bool)>;
using SubscriptionCallback = std::function<void(
        const UUID &central_uuid, const UUID &characteristic_uuid)>;
using DataReceivedCallback =
        std::function<void(const UUID &, std::vector<uint8_t> const &)>;

namespace cb {

struct CBCentralManagerWrapper {
    void const *central_manager;
    void const *delegate;
};

struct CBPeripheralManagerWrapper {
    void const *peripheral_manager;
    void const *delegate;
};

void bt_init();

CBCentralManagerWrapper create_central_manager();
void destroy_central_manager(CBCentralManagerWrapper wrapper);

CBPeripheralManagerWrapper create_peripheral_manager();
void destroy_peripheral_manager(CBPeripheralManagerWrapper wrapper);

void start_scanning(CBCentralManagerWrapper *wrapper);
void stop_scanning(CBCentralManagerWrapper *wrapper);

void start_advertising(
        CBPeripheralManagerWrapper *wrapper, AdvertisingData const &data);
void stop_advertising(CBPeripheralManagerWrapper *wrapper);

void connect_to_peripheral(CBCentralManagerWrapper *wrapper, const UUID &uuid);
void disconnect_from_peripheral(
        CBCentralManagerWrapper *wrapper, const UUID &uuid);

void add_service(CBPeripheralManagerWrapper *wrapper,
        std::string const &service_uuid,
        std::vector<Characteristic> const &characteristics);

void set_discovered_peripheral_callback(DiscoveredPeripheralCallback callback);
void set_connection_callback(ConnectionCallback callback);
void set_subscription_callback(SubscriptionCallback callback);
void set_data_received_callback(DataReceivedCallback callback);

void enable_background_mode(bool enable);

} // namespace cb
