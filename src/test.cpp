#include "btle/corebluetooth/bt.h"
#include <spdlog/spdlog.h>

int main() {
    Adapter adapter{};
    absl::SleepFor(absl::Seconds(1));

    adapter.on_discovery([](Peripheral peripheral, AdvertisingData) {
        spdlog::info("Discovered peripheral with UUID: {}",
                peripheral.uuid().to_string());
    });

    adapter.start_scanning({});
    absl::SleepFor(absl::Seconds(30));
    adapter.stop_scanning();
}
