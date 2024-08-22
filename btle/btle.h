#pragma once

#include <vector>

#include "utils/uuid.h"

struct Characteristic {
    UUID uuid;
    std::string value;
    bool is_readable;
    bool is_writable;
};

struct AdvertisingData {
    std::string local_name;
    std::vector<UUID> service_uuids;
    std::vector<uint8_t> manufacturer_data;
};
