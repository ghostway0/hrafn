#include <absl/time/clock.h>
#include <absl/time/time.h>

#include "btle/btle.h"
#include "cbtle.h"
#include "corebluetooth/bt.h"

Adapter::Adapter(Delegate const &delegate)
    : central_manager_wrapper_{cb::create_central_manager()},
      peripheral_manager_wrapper_{cb::create_peripheral_manager()} {
    cb::set_discovered_peripheral_callback(
            delegate.discovered_peripheral_callback);
    cb::set_connection_callback(delegate.connection_callback);
    cb::set_subscription_callback(delegate.subscription_callback);
    cb::set_data_received_callback(delegate.data_received_callback);
}

Adapter::~Adapter() {
    destroy_central_manager(central_manager_wrapper_);
    destroy_peripheral_manager(peripheral_manager_wrapper_);
}

void Adapter::start_scanning() {
    cb::start_scanning(&central_manager_wrapper_);
}

void Adapter::stop_scanning() {
    cb::stop_scanning(&central_manager_wrapper_);
}

void Adapter::start_advertising(AdvertisingData const &data) {
    absl::SleepFor(absl::Milliseconds(100));
    cb::start_advertising(&peripheral_manager_wrapper_, data);
}

void Adapter::stop_advertising() {
    cb::stop_advertising(&peripheral_manager_wrapper_);
}

void Adapter::connect_to_peripheral(const UUID &uuid) {
    cb::connect_to_peripheral(&central_manager_wrapper_, uuid);
}

void Adapter::disconnect_from_peripheral(const UUID &uuid) {
    cb::disconnect_from_peripheral(&central_manager_wrapper_, uuid);
}

void Adapter::add_service(std::string const &service_uuid,
        std::vector<Characteristic> const &characteristics) {
    cb::add_service(
            &peripheral_manager_wrapper_, service_uuid, characteristics);
}

// adapter channel-> connection multiplexer (uuid->{pubkey, connection}:verify) -> connection
