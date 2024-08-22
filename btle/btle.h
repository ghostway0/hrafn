#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "bt.h"
#include "spdlog/spdlog.h"

void example() {
    bt_init();
    CBPeripheralManagerWrapper *w = create_peripheral_manager();
    start_advertising(w,
            AdvertisingData{.local_name = "kaki",
                    .service_uuids = {UUID::generate_random()}});

    spdlog::info("doing stuff");
    absl::SleepFor(absl::Seconds(20));
    spdlog::info("finished doing stuff");

    stop_advertising(w);
}
