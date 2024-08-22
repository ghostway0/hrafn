#include "bt.h"
#include <absl/time/time.h>
#include <spdlog/spdlog.h>

void example() {
    cb::bt_init();
    cb::CBPeripheralManagerWrapper w = cb::create_peripheral_manager();

    UUID characteristic_uuid = UUID::generate_random();
    cb::Characteristic characteristic = {.uuid = characteristic_uuid,
            .value = "Initial Value",
            .is_readable = true,
            .is_writable = true};

    UUID service_uuid = UUID::generate_random();
    cb::add_service(&w, service_uuid.to_string(), {characteristic});

    absl::SleepFor(absl::Milliseconds(100));
    cb::start_advertising(&w,
            cb::AdvertisingData{
                    .local_name = "kaki", .service_uuids = {service_uuid}});

    spdlog::info("doing stuff");
    absl::SleepFor(absl::Seconds(20));
    spdlog::info("finished doing stuff");

    cb::stop_advertising(&w);
    cb::destroy_peripheral_manager(w);
}
