#include <absl/time/clock.h>
#include <absl/time/time.h>

#include "btle/btle.h"
#include "cbtle.h"

void CoreBluetoothAdapter::start_advertising(
        AdvertisingData const &advertising_data) {
    absl::SleepFor(absl::Milliseconds(100));
    cb::start_advertising(&pm_, advertising_data);
}
