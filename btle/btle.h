#include <string>

#include "btle/types.h"

#ifdef __APPLE__

#include "btle/corebluetooth/cbtle.h"

#elifdef __LINUX__

#include "btle/bluez/bluez_btle.h"

#endif
