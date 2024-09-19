#pragma once

#include "bt.h"

class CentralAdapter {
public:
    CentralAdapter() : central_manager_{} {}

    void start_scanning(ScanOptions const &opts) {
        central_manager_.scan({}, opts);
    }

    void stop_scanning() { central_manager_.stop_scan(); }

    void disconnect(Peripheral &peripheral) {
        central_manager_.cancel_connect(peripheral);
    }

    void add_service(UUID service_uuid,
            std::vector<Characteristic> const &characteristics);

    void on_discovery(std::function<void(Peripheral &, AdvertisingData const &)>
                    callback) {
        central_manager_.set_discovered_callback(std::move(callback));
    }

    void connect(Peripheral &peripheral, ConnectOptions const &opts) {
        central_manager_.connect(peripheral, opts);
    }

private:
    CentralManager central_manager_;
};

class PeripheralAdapter {
public:
    PeripheralAdapter() : peripheral_manager_{} {}

    void on_connect(std::function<void(Central)> callback) {
        peripheral_manager_.set_on_connect(std::move(callback));
    }

    void on_disconnect(std::function<void(Central)> callback) {
        peripheral_manager_.set_on_disconnect(std::move(callback));
    }

    void on_read_request(
            std::function<void(Central, Characteristic)> callback) {
        peripheral_manager_.set_on_read(std::move(callback));
    }

    void on_write_request(
            std::function<void(Central, Characteristic, std::vector<uint8_t>)>
                    callback) {
        peripheral_manager_.set_on_write(std::move(callback));
    }

    void start_advertising(AdvertisingOptions const &opts) {
        peripheral_manager_.start_advertising(opts);
    }

    void add_service(ManagedService &&service) {
        peripheral_manager_.add_service(service);
    }

    void stop_advertising() { peripheral_manager_.stop_advertising(); }

private:
    PeripheralManager peripheral_manager_;
};
