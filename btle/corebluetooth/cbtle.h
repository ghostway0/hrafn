#pragma once

#include "bt.h"

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

    void on_discovery(
            std::function<void(Peripheral &, AdvertisingData const &)> callback) {
        central_manager_.set_discovered_callback(std::move(callback));
    }

    void on_connect(std::function<void(Peripheral)> callback) {
        peripheral_manager_.set_on_connect(std::move(callback));
    }

    void start_advertising(AdvertisingOptions const &opts) {
        peripheral_manager_.start_advertising(opts);
    }

    void stop_advertising() { peripheral_manager_.stop_advertising(); }

    void connect(Peripheral &peripheral, ConnectOptions const &opts) {
        central_manager_.connect(peripheral, opts);
    }

private:
    CentralManager central_manager_;
    PeripheralManager peripheral_manager_;
};
