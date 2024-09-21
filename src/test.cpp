#include <absl/time/time.h>
#include <asio.hpp>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include "btle/btle.h"

int main() {
    asio::io_context ctx{};

    {
        CentralAdapter adapter{ctx};
        absl::SleepFor(absl::Milliseconds(100));

        adapter.on_discovery(
                [&](Peripheral &peripheral,
                        AdvertisingData const &data) -> asio::awaitable<void> {
                    spdlog::info(
                            "Discovered peripheral with UUID: {} services [{}]",
                            peripheral.uuid(),
                            fmt::join(data.service_uuids, ", "));
                    adapter.connect(peripheral, {});

                    std::vector<Service> services =
                            co_await peripheral.discover_services();

                    for (auto const &service : services) {
                        spdlog::info("Service: {}", service.uuid());
                        std::vector<Characteristic> characteristics =
                                co_await peripheral.discover_characteristics(service);

                        for (auto const &characteristic : characteristics) {
                            spdlog::info("Characteristic: {}", characteristic.uuid());
                        }
                    }

                    co_return;
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
                        UUID::from_string(
                                "00002a00-0000-1000-8000-00805f9b34fb")}
                        .add_property(CharacteristicPropertyRead)
                        .build());

        adapter.add_service(service_builder.build());

        adapter.start_advertising({.local_name = "lol"});

        absl::SleepFor(absl::Seconds(5));
        adapter.stop_advertising();
    }
}
