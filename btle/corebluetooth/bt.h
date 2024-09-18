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
            std::vector<uint8_t> value);

    void set_descriptors(std::vector<Descriptor> descriptors);

    void set_value(std::vector<uint8_t> value);

    void on_read(std::function<void(UUID)> callback);

    void on_write(
            std::function<void(UUID, std::vector<uint8_t> const &)> callback);

    void on_notify(std::function<void(UUID)> callback);

    void on_indicate(std::function<void(UUID)> callback);

    void on_subscribe(std::function<void(UUID)> callback);

    void on_unsubscribe(std::function<void(UUID)> callback);

    void *repr() { return raw_; }

private:
    void *raw_;

    std::function<void(UUID)> on_read_;
    std::function<void(UUID, std::vector<uint8_t> const &)> on_write_;
    std::function<void(UUID)> on_notify_;
    std::function<void(UUID)> on_indicate_;
    std::function<void(UUID)> on_subscribe_;
    std::function<void(UUID)> on_unsubscribe_;
};

class ManagedService {
public:
    explicit ManagedService(UUID uuid);

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

class PeripheralManager {
public:
    PeripheralManager();

    void add_service(ManagedService service);

    void start_advertising(AdvertisingOptions const &);

    void stop_advertising();

    bool is_advertising();

    void set_manufacturer_data(std::vector<uint8_t> data);

    void set_on_connect(std::function<void(Peripheral)> &&callback) {
        on_connect_.swap(callback);
    }

    void *repr() { return raw_; }

    void on_connect(Peripheral peripheral) {
        if (on_connect_) {
            on_connect_(std::move(peripheral));
        }
    }

    void on_subscribe(Peripheral peripheral, Characteristic characteristic) {
        if (on_subscribe_) {
            on_subscribe_(std::move(peripheral), std::move(characteristic));
        }
    }

    void on_unsubscribe(Peripheral peripheral, Characteristic characteristic) {
        if (on_unsubscribe_) {
            on_unsubscribe_(std::move(peripheral), std::move(characteristic));
        }
    }

    void on_read(Peripheral peripheral, Characteristic chr) {
        if (on_read_) {
            on_read_(std::move(peripheral), std::move(chr));
        }
    }

    void on_write(Peripheral peripheral,
            Characteristic chr,
            std::vector<char> value) {
        if (on_write_) {
            on_write_(std::move(peripheral), std::move(chr), std::move(value));
        }
    }

private:
    void *raw_;

    std::function<void(Peripheral)> on_connect_;
    std::function<void(Peripheral, Characteristic)> on_subscribe_;
    std::function<void(Peripheral, Characteristic)> on_unsubscribe_;
    std::function<void(Peripheral, Characteristic)> on_read_;
    std::function<void(Peripheral, Characteristic, std::vector<char>)>
            on_write_;
};

class Adapter {
public:
    Adapter() : central_manager_{}, peripheral_manager_{} {}

    ~Adapter() = default;

    void start_scanning(ScanOptions const &opts) {
        central_manager_.scan({}, opts);
    }

    void stop_scanning() { central_manager_.stop_scan(); }

    void disconnect(Peripheral &peripheral) {
        central_manager_.cancel_connect(peripheral);
    }

    void add_service(UUID service_uuid,
            std::vector<Characteristic> const &characteristics);

    void on_discovery(std::function<void(Peripheral, AdvertisingData)> callback) {
        central_manager_.set_discovered_callback(std::move(callback));
    }

    void on_connect(std::function<void(Peripheral)> callback) {
        peripheral_manager_.set_on_connect(std::move(callback));
    }

    void start_advertising(AdvertisingOptions const &opts) {
        peripheral_manager_.start_advertising(opts);
    }

    void stop_advertising() { peripheral_manager_.stop_advertising(); }

private:
    CentralManager central_manager_;
    PeripheralManager peripheral_manager_;
};
