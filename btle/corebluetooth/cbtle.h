#pragma once

#include <vector>

#include "btle/types.h"
#include "bt.h"

class CoreBluetoothAdapter {
public:
    explicit CoreBluetoothAdapter(std::string_view local_name)
        : local_name_{local_name}, cm_{cb::create_central_manager()},
          pm_{cb::create_peripheral_manager()} {}

    void add_service(UUID service_uuid,
            std::vector<Characteristic> const &characteristics) {
        cb::add_service(&pm_, service_uuid.to_string(), characteristics);
    }

    void start_advertising(AdvertisingData const &advertising_data);

private:
    std::string local_name_;
    cb::CBCentralManagerWrapper cm_;
    cb::CBPeripheralManagerWrapper pm_;
    std::vector<UUID> service_uuids_;
};
