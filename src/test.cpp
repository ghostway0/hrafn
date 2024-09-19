#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include "btle/btle.h"

int main() {
    Adapter adapter{};
    absl::SleepFor(absl::Milliseconds(100));

    adapter.on_discovery([](Peripheral peripheral, AdvertisingData data) {
        spdlog::info("Discovered peripheral with UUID: {} services [{}]",
                peripheral.uuid(), fmt::join(data.service_uuids, ", "));
    });

    adapter.start_scanning({});
    absl::SleepFor(absl::Seconds(5));
    adapter.stop_scanning();

    spdlog::info("Starting to advertise");

    adapter.start_advertising({.local_name = "gil buchbinder the corech of the sfarim"});
    absl::SleepFor(absl::Seconds(30));
    adapter.stop_advertising();
}
