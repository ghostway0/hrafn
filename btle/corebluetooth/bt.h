#pragma once

#include <map>
#include <optional>

#include <absl/time/clock.h>
#include <absl/time/time.h>

#include "btle/types.h"
#include "utils/uuid.h"

enum CharacteristicProperties {
    CharacteristicPropertyBroadcast = 1,
    CharacteristicPropertyRead = 1 << 2,
    CharacteristicPropertyWriteWithoutResponse = 1 << 3,
    CharacteristicPropertyWrite = 1 << 4,
    CharacteristicPropertyNotify = 1 << 5,
    CharacteristicPropertyIndicate = 1 << 6,
    CharacteristicPropertyAuthenticatedSignedWrites = 1 << 7,
    CharacteristicPropertyExtendedProperties = 1 << 8,
    CharacteristicPropertyNotifyEncryptionRequired = 1 << 9,
    CharacteristicPropertyIndicateEncryptionRequired = 1 << 10,
    CharacteristicPropertyWriteSigned = 1 << 11,
    CharacteristicPropertyWriteSignedWithoutResponse = 1 << 12,
    CharacteristicPropertyWriteAuxiliaries = 1 << 13,
};

struct Permissions {
    bool read;
    bool write;

    int as_int() const { return (read ? 1 : 0) | (write ? 2 : 0); }
};

class Descriptor {
public:
    static std::optional<Descriptor> from(UUID uuid, std::vector<char> value);

    static Descriptor from_raw(void *raw) { return Descriptor{raw}; }

    std::vector<uint8_t> value();

    UUID uuid();

    void *repr() { return raw_; }

private:
    void *raw_;

    explicit Descriptor(void *raw) : raw_{raw} {}
};

class Characteristic {
public:
    static Characteristic from_raw(void *raw) { return Characteristic{raw}; }

    static std::optional<Characteristic> from(UUID uuid,
            CharacteristicProperties properties,
            Permissions permissions,
            std::vector<uint8_t> value = {});

    void *repr() { return raw_; }

    void set_descriptors(std::vector<Descriptor> descriptors);

    void set_value(std::vector<uint8_t> value);

    std::vector<uint8_t> value();

    std::vector<Descriptor> descriptors();

    UUID uuid();

private:
    void *raw_;

    explicit Characteristic(void *raw) : raw_{raw} {}
};

class Service {
public:
    static Service from_raw(void *raw) { return Service{raw}; }

    UUID uuid();

    std::vector<Characteristic> characteristics();

    bool is_primary();

    void *repr() { return raw_; }

    std::vector<Service> included_services();

    void set_characteristics(std::vector<Characteristic> services) {
        characteristics_ = std::move(services);
    }

private:
    void *raw_;

    std::vector<Characteristic> characteristics_;

    explicit Service(void *raw) : raw_{raw} {}
};

enum class PeripheralState {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
};

class Peripheral {
public:
    static Peripheral from_raw(void *raw) { return Peripheral{raw}; }

    void set_delegate();

    std::string name();

    UUID uuid();

    std::vector<Service> services();

    void discover_services(std::span<UUID> uuids);

    void discover_included_services(Service service, std::span<UUID> uuids);

    void discover_characteristics(Service service, std::span<UUID> uuids);

    void discover_descriptors(Characteristic characteristic);

    void read_characteristic(Characteristic characteristic);

    void read_descriptor(Descriptor descriptor);

    void write_characteristic(
            Characteristic characteristic, std::vector<char> value, int type);

    void write_descriptor(Descriptor descriptor, std::vector<char> value);

    size_t max_write_len(int type);

    void set_notify(bool enabled, Characteristic &characteristic);

    PeripheralState state();

    bool can_send_write_without_response();

    void read_rssi();

    void on_disconnection(std::function<void()> callback) {
        on_disconnected_ = std::move(callback);
    }

    void *repr() { return raw_; }

    // internal funcitons. this is horrible code.

    void clear_services() { services_.clear(); }

    void add_service(Service service) {
        services_.push_back(std::move(service));
    }

private:
    void *raw_;

    std::vector<Service> services_;

    std::function<void()> on_disconnected_;

    explicit Peripheral(void *raw) : raw_{raw} {}
};

struct ScanOptions {
    bool allow_dups;
    absl::Duration interval;
    absl::Duration window;
    std::vector<UUID> solicited_services;
};

struct ConnectOptions {
    bool notify_on_connection;
    bool notify_on_disconnection;
    bool notify_on_notification;
};

class CentralManager {
public:
    CentralManager();

    int state();

    void scan(std::span<UUID> service_uuids, ScanOptions const &opts);

    void stop_scan();

    bool is_scanning();

    void connect(Peripheral peripheral, ConnectOptions const &opts);

    void cancel_connect(Peripheral &peripheral);

    void set_discovered_callback(
            std::function<void(Peripheral &, AdvertisingData const &)>
                    callback) {
        on_discovered_ = std::move(callback);
    }

    void *repr() { return raw_; }

    void on_discovered(Peripheral &peripheral, AdvertisingData const &data) {
        if (on_discovered_) {
            on_discovered_(peripheral, data);
        }
    }

    std::optional<Peripheral> retreive_peripheral(UUID const &uuid);

private:
    void *raw_;

    std::function<void(Peripheral &, AdvertisingData const &)> on_discovered_;
};

class ManagedCharacteristic {
public:
    ManagedCharacteristic(UUID uuid,
            CharacteristicProperties properties,
            Permissions permissions,
            std::optional<std::vector<uint8_t>> value);

    UUID uuid();

    void set_descriptors(std::vector<Descriptor> descriptors);

    void set_value(std::vector<uint8_t> value);

    void *repr() { return raw_; }

private:
    void *raw_;

    friend class CharacteristicBuilder;
};

class ManagedService {
public:
    explicit ManagedService(UUID uuid, bool primary = true);

    void add_characteristic(ManagedCharacteristic characteristic);

    void *repr() { return raw_; }

private:
    void *raw_;
};
struct AdvertisingOptions {
    std::string local_name;
    bool include_tx_power_level;
    bool include_local_name;
    bool include_device_name;
    std::vector<UUID> service_uuids;
    std::vector<uint8_t> manufacturer_data;
    std::map<UUID, std::vector<uint8_t>> service_data;
    std::vector<UUID> overflow_service_uuids;
    std::vector<UUID> solicited_service_uuids;
};

class Central {
public:
    static Central from_raw(void *raw) { return Central{raw}; }

    UUID uuid();

    size_t maximum_write_length();

    void *repr() { return raw_; }

private:
    void *raw_;

    explicit Central(void *raw) : raw_{raw} {}
};

class PeripheralManager {
public:
    PeripheralManager();

    void add_service(ManagedService service);

    void start_advertising(AdvertisingOptions const &);

    void stop_advertising();

    bool is_advertising();

    void set_manufacturer_data(std::vector<uint8_t> data);

    void set_on_connect(std::function<void(Central)> &&callback) {
        on_connect_.swap(callback);
    }

    void set_on_disconnect(std::function<void(Central)> &&callback) {
        on_disconnect_.swap(callback);
    }

    void set_on_subscribe(
            std::function<void(Central, Characteristic)> &&callback) {
        on_subscribe_.swap(callback);
    }

    void set_on_unsubscribe(
            std::function<void(Central, Characteristic)> &&callback) {
        on_unsubscribe_.swap(callback);
    }

    void set_on_read(
            std::function<void(Central, Characteristic)> &&callback) {
        on_read_.swap(callback);
    }

    void set_on_write(std::function<void(
                    Central, Characteristic, std::vector<uint8_t>)>
                    &&callback) {
        on_write_.swap(callback);
    }

    void *repr() { return raw_; }

    // bad code.

    void on_connect(Central central) {
        if (on_connect_) {
            on_connect_(std::move(central));
        }
    }

    void on_disconnect(Central central) {
        if (on_disconnect_) {
            on_disconnect_(std::move(central));
        }
    }

    void on_subscribe(Central central, Characteristic characteristic) {
        if (on_subscribe_) {
            on_subscribe_(std::move(central), std::move(characteristic));
        }
    }

    void on_unsubscribe(Central central, Characteristic characteristic) {
        if (on_unsubscribe_) {
            on_unsubscribe_(std::move(central), std::move(characteristic));
        }
    }

    void on_read(Central central, Characteristic chr) {
        if (on_read_) {
            on_read_(std::move(central), std::move(chr));
        }
    }

    void on_write(Central central,
            Characteristic chr,
            std::vector<uint8_t> value) {
        if (on_write_) {
            on_write_(std::move(central), std::move(chr), std::move(value));
        }
    }

private:
    void *raw_;

    std::function<void(Central)> on_connect_;
    std::function<void(Central)> on_disconnect_;
    std::function<void(Central, Characteristic)> on_subscribe_;
    std::function<void(Central, Characteristic)> on_unsubscribe_;
    std::function<void(Central, Characteristic)> on_read_;
    std::function<void(Central, Characteristic, std::vector<uint8_t>)>
            on_write_;
};
