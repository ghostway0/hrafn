#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include "absl/time/time.h"
#include "btle/btle.h"

int main() {
    {
        CentralAdapter adapter{};
        absl::SleepFor(absl::Milliseconds(100));

        adapter.on_discovery([](Peripheral peripheral, AdvertisingData data) {
            spdlog::info("Discovered peripheral with UUID: {} services [{}]",
                    peripheral.uuid(),
                    fmt::join(data.service_uuids, ", "));
        });

        adapter.start_scanning({});
        absl::SleepFor(absl::Seconds(5));
        adapter.stop_scanning();
    }

    {
        PeripheralAdapter adapter{};
        absl::SleepFor(absl::Milliseconds(100));

        spdlog::info("Starting to advertise");

        ServiceBuilder service_builder{
                UUID::parse("00001800-0000-1000-8000-00805f9b34fb").value()};

        service_builder.add_characteristic(
                CharacteristicBuilder{
                        UUID::parse("00002a00-0000-1000-8000-00805f9b34fb")
                                .value()}
                        .add_property(CharacteristicPropertyRead)
                        .build());

        adapter.add_service(service_builder.build());

        adapter.start_advertising({.local_name = "lol"});

        absl::SleepFor(absl::Seconds(5));
        adapter.stop_advertising();
    }
}
