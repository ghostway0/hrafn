#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#include "utils/uuid.h"

#include <string_view>

inline CBUUID *str_to_cbuuid(std::string_view str) {
  return [CBUUID UUIDWithString:[NSString stringWithUTF8String:str.data()]];
}

inline NSMutableArray *uuids_to_cbuuids(std::span<const UUID> uuids) {
  NSMutableArray *arr = [NSMutableArray arrayWithCapacity:uuids.size()];

  for (UUID uuid : uuids) {
    std::string str = uuid.to_string();
    CBUUID *cbuuid =
        [CBUUID UUIDWithString:[NSString stringWithUTF8String:str.c_str()]];
    [arr addObject:cbuuid];
  }

  return arr;
}

