#include "types.h"
#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#include <cstdio>

#import "bt.h"

// the delegate should hold these
static DiscoveredPeripheralCallback discovered_peripheral_callback;
static ConnectionCallback connection_callback;
static SubscriptionCallback subscription_callback;
static DataReceivedCallback data_received_callback;

NSUUID *uuid_to_nsuuid(const UUID &uuid) {
  uuid_t ns_uuid_bytes;
  memcpy(ns_uuid_bytes, uuid.bytes.data(), UUID::kSize);
  return [[NSUUID alloc] initWithUUIDBytes:ns_uuid_bytes];
}

UUID nsuuid_to_uuid(NSUUID *ns_uuid) {
  UUID uuid{};
  [ns_uuid getUUIDBytes:uuid.bytes.data()];
  return uuid;
}

static UUID cbuuid_to_uuid(CBUUID *cb_uuid) {
  UUID uuid = {}; // Zero-initialize the UUID array

  NSData *uuid_data = [cb_uuid data];
  const uint8_t *bytes = static_cast<const uint8_t *>([uuid_data bytes]);
  NSUInteger length = [uuid_data length];

  if (length == 2 || length == 4) {
    // For 16-bit or 32-bit UUIDs, map to the standard base UUID format
    static const uint8_t base_uuid[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x10, 0x00, 0x80, 0x00, 0x00, 0x80,
                                        0x5F, 0x9B, 0x34, 0xFB};
    std::copy(std::begin(base_uuid), std::end(base_uuid), uuid.bytes.begin());
    std::copy(bytes, bytes + length,
              uuid.bytes.begin() +
                  (4 - length)); // Adjust position for the length
  } else if (length == 16) {
    // For 128-bit UUIDs, directly copy the bytes
    std::copy(bytes, bytes + 16, uuid.bytes.begin());
  } else {
    assert(false && "Invalid CBUUID length");
  }

  return uuid;
}

AdvertisingData dict_to_advertisement_data(NSDictionary<NSString *, id> *dict) {
  AdvertisingData data{};
  id local_name = dict[CBAdvertisementDataLocalNameKey];
  if (local_name) {
    data.local_name = [local_name UTF8String];
  }

  id service_uuids = dict[CBAdvertisementDataServiceUUIDsKey];
  if (service_uuids) {
    for (CBUUID *uuid in service_uuids) {
      data.service_uuids.push_back(cbuuid_to_uuid(uuid));
    }
  }

  id manufacturer_data = dict[CBAdvertisementDataManufacturerDataKey];
  if (manufacturer_data) {
    data.manufacturer_data = std::vector<uint8_t>(
        static_cast<const uint8_t *>([manufacturer_data bytes]),
        static_cast<const uint8_t *>(
            static_cast<uint8_t const *>([manufacturer_data bytes]) +
            [manufacturer_data length]));
  }

  return data;
}

// make the wrapper have one
static dispatch_queue_t bt_queue;
@interface BluetoothDelegate
    : NSObject <CBCentralManagerDelegate, CBPeripheralManagerDelegate>

@property(nonatomic, copy)
    DiscoveredPeripheralCallback discoveredPeripheralCallback;
@property(nonatomic, copy) ConnectionCallback connectionCallback;
@property(nonatomic, copy) SubscriptionCallback subscriptionCallback;
@property(nonatomic, copy) DataReceivedCallback dataReceivedCallback;

@end

@implementation BluetoothDelegate

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
}

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
}

- (void)centralManager:(CBCentralManager *)central
    didDiscoverPeripheral:(CBPeripheral *)peripheral
        advertisementData:(NSDictionary<NSString *, id> *)advertisementData
                     RSSI:(NSNumber *)RSSI {
  if (self.discoveredPeripheralCallback) {
    UUID uuid = nsuuid_to_uuid(peripheral.identifier);
    AdvertisingData adv_data = dict_to_advertisement_data(advertisementData);
    self.discoveredPeripheralCallback(uuid, advertisementData);
  }
}

- (void)centralManager:(CBCentralManager *)central
    didConnectPeripheral:(CBPeripheral *)peripheral {
  if (self.connectionCallback) {
    UUID uuid = nsuuid_to_uuid(peripheral.identifier);
    self.connectionCallback(uuid, true);
  }
}

- (void)centralManager:(CBCentralManager *)central
    didFailToConnectPeripheral:(CBPeripheral *)peripheral
                         error:(NSError *)error {
  if (self.connectionCallback) {
    UUID uuid = nsuuid_to_uuid(peripheral.identifier);
    self.connectionCallback(uuid, false);
  }
}

- (void)centralManager:(CBCentralManager *)central
    didDisconnectPeripheral:(CBPeripheral *)peripheral
                      error:(NSError *)error {
  if (self.connectionCallback) {
    UUID uuid = nsuuid_to_uuid(peripheral.identifier);
    self.connectionCallback(uuid, false);
  }
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didReceiveWriteRequests:(NSArray<CBATTRequest *> *)requests {
  for (CBATTRequest *request in requests) {
    NSData *data = request.value;
    if (self.dataReceivedCallback) {
      UUID uuid = nsuuid_to_uuid(request.central.identifier);

      auto data_vec = std::vector<uint8_t>(
          static_cast<const uint8_t *>([data bytes]),
          static_cast<const uint8_t *>(
              static_cast<uint8_t const *>([data bytes]) + [data length]));
      bool should_respond = self.dataReceivedCallback(uuid, data_vec);

      if (should_respond) {
        [peripheral respondToRequest:request withResult:CBATTErrorSuccess];
      }
    }
  }
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral
                                       error:(NSError *)error {
  if (error != nullptr) {
    NSLog(@"While trying to start advertising, encountered: %@",
          error.localizedDescription);
  }
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
                         central:(CBCentral *)central
    didSubscribeToCharacteristic:(CBCharacteristic *)characteristic {
  if (self.subscriptionCallback) {
    UUID uuid = nsuuid_to_uuid(central.identifier);
    self.subscriptionCallback(uuid, cbuuid_to_uuid(characteristic.UUID));
  }
}

@end

namespace cb {

BluetoothDelegate *bt_dlg = nil;

void bt_init() {
  if (bt_queue == nullptr) {
    bt_queue = dispatch_queue_create("bt_queue", nullptr);
  }

  if (bt_dlg == nullptr) {
    bt_dlg = [[BluetoothDelegate alloc] init];
  }
}

void set_discovered_peripheral_callback(DiscoveredPeripheralCallback callback) {
  bt_dlg.discoveredPeripheralCallback = std::move(callback);
}

void set_connection_callback(ConnectionCallback callback) {
  bt_dlg.connectionCallback = std::move(callback);
}

void set_subscription_callback(SubscriptionCallback callback) {
  bt_dlg.subscriptionCallback = std::move(callback);
}

void set_data_received_callback(DataReceivedCallback callback) {
  bt_dlg.dataReceivedCallback = std::move(callback);
}

CBCentralManagerWrapper create_central_manager() {
  bt_init();

  CBCentralManagerWrapper wrapper = CBCentralManagerWrapper();

  CBCentralManager *cm =
      [[CBCentralManager alloc] initWithDelegate:bt_dlg queue:bt_queue];

  wrapper.delegate = static_cast<void const *>(bt_dlg);
  wrapper.central_manager = static_cast<void const *>(cm);

  [bt_dlg retain];
  [cm retain];
  return wrapper;
}

void destroy_central_manager(CBCentralManagerWrapper wrapper) {
  [(id)(wrapper.central_manager) release];
  [(id)(wrapper.delegate) release];
}

CBPeripheralManagerWrapper create_peripheral_manager() {
  bt_init();

  CBPeripheralManagerWrapper wrapper = CBPeripheralManagerWrapper();

  CBPeripheralManager *pmgr =
      [[CBPeripheralManager alloc] initWithDelegate:bt_dlg queue:bt_queue];

  wrapper.delegate = static_cast<void const *>(bt_dlg);
  wrapper.peripheral_manager = static_cast<void const *>(pmgr);

  [pmgr retain];
  [bt_dlg retain];
  return wrapper;
}

void destroy_peripheral_manager(CBPeripheralManagerWrapper wrapper) {
  [(id)(wrapper.peripheral_manager) release];
  [(id)(wrapper.delegate) release];
}

void start_scanning(CBCentralManagerWrapper *wrapper) {
  [(CBCentralManager *)(wrapper->central_manager)
      scanForPeripheralsWithServices:nil
                             options:nil];
}

void stop_scanning(CBCentralManagerWrapper *wrapper) {
  [(CBCentralManager *)(wrapper->central_manager) stopScan];
}

void start_advertising(CBPeripheralManagerWrapper *wrapper,
                       const AdvertisingData &data) {
  NSMutableArray *service_uuids =
      [NSMutableArray arrayWithCapacity:data.service_uuids.size()];
  for (const auto &uuid : data.service_uuids) {
    CBUUID *cbuuid = [CBUUID UUIDWithNSUUID:uuid_to_nsuuid(uuid)];
    [service_uuids addObject:cbuuid];
  }

  NSMutableDictionary *advertising_data = [[NSMutableDictionary alloc] init];
  [advertising_data autorelease];

  [advertising_data
      setObject:[NSString stringWithUTF8String:data.local_name.c_str()]
         forKey:CBAdvertisementDataLocalNameKey];
  [advertising_data setObject:service_uuids
                       forKey:CBAdvertisementDataServiceUUIDsKey];
  [advertising_data
      setObject:[NSData dataWithBytes:data.manufacturer_data.data()
                               length:data.manufacturer_data.size()]
         forKey:CBAdvertisementDataManufacturerDataKey];

  [(CBPeripheralManager *)(wrapper->peripheral_manager)
      startAdvertising:advertising_data];
}

void stop_advertising(CBPeripheralManagerWrapper *wrapper) {
  [(CBPeripheralManager *)(wrapper->peripheral_manager) stopAdvertising];
}

void connect_to_peripheral(CBCentralManagerWrapper *wrapper, const UUID &uuid) {
  NSUUID *ns_uuid = uuid_to_nsuuid(uuid);
  NSArray *peripherals = [(CBCentralManager *)(wrapper->central_manager)
      retrievePeripheralsWithIdentifiers:@[ ns_uuid ]];
  if ([peripherals count] > 0) {
    [(CBCentralManager *)(wrapper->central_manager)
        connectPeripheral:[peripherals firstObject]
                  options:nil];
  }
}

void disconnect_from_peripheral(CBCentralManagerWrapper *wrapper,
                                const UUID &uuid) {
  NSUUID *ns_uuid = uuid_to_nsuuid(uuid);
  NSArray *peripherals = [(CBCentralManager *)(wrapper->central_manager)
      retrievePeripheralsWithIdentifiers:@[ ns_uuid ]];
  if ([peripherals count] > 0) {
    [(CBCentralManager *)(wrapper->central_manager)
        cancelPeripheralConnection:[peripherals firstObject]];
  }
}

CBMutableCharacteristic *create_characteristic(CBUUID *uuid, NSData *value,
                                               bool is_readable,
                                               bool is_writable) {
  CBCharacteristicProperties properties = 0;
  CBAttributePermissions permissions = 0;

  if (is_readable) {
    properties |= CBCharacteristicPropertyRead;
    permissions |= CBAttributePermissionsReadable;
  }

  if (is_writable) {
    properties |= CBCharacteristicPropertyWrite;
    permissions |= CBAttributePermissionsWriteable;
    value = nullptr;
  }

  return [[CBMutableCharacteristic alloc]
      initWithType:uuid
        properties:properties
             value:(is_writable ? nil : value)permissions:permissions];
}

void add_service(CBPeripheralManagerWrapper *wrapper,
                 const std::string &service_uuid,
                 const std::vector<Characteristic> &characteristics) {
  CBUUID *cb_service_uuid = [CBUUID
      UUIDWithString:[NSString stringWithUTF8String:service_uuid.c_str()]];
  CBMutableService *service =
      [[CBMutableService alloc] initWithType:cb_service_uuid primary:YES];

  NSMutableArray *cb_characteristics =
      [NSMutableArray arrayWithCapacity:characteristics.size()];
  for (const auto &characteristic : characteristics) {
    CBUUID *char_uuid = [CBUUID
        UUIDWithString:[NSString
                           stringWithUTF8String:characteristic.uuid.to_string()
                                                    .c_str()]];
    NSData *char_value = [NSData dataWithBytes:characteristic.value.c_str()
                                        length:characteristic.value.size()];
    CBMutableCharacteristic *cb_characteristic =
        create_characteristic(char_uuid, char_value, characteristic.is_readable,
                              characteristic.is_writable);
    [cb_characteristics addObject:cb_characteristic];
  }

  service.characteristics = cb_characteristics;
  [(CBPeripheralManager *)(wrapper->peripheral_manager) addService:service];
}

void write_to_characteristic(CBPeripheralManagerWrapper *wrapper,
                             const UUID &peripheral_uuid,
                             const UUID &service_uuid,
                             const UUID &characteristic_uuid,
                             const std::vector<uint8_t> &data) {
  NSUUID *ns_peripheral_uuid = uuid_to_nsuuid(peripheral_uuid);
  NSArray *peripherals = [(CBPeripheralManager *)(wrapper->peripheral_manager)
      retrievePeripheralsWithIdentifiers:@[ ns_peripheral_uuid ]];
  if ([peripherals count] > 0) {
    CBMutableService *service = nil;
    for (CBMutableService *s in
         [(CBPeripheralManager *)(wrapper->peripheral_manager) services]) {
      if ([s.UUID isEqual:uuid_to_nsuuid(service_uuid)]) {
        service = s;
        break;
      }
    }

    if (service != nil) {
      CBMutableCharacteristic *characteristic = nil;
      for (CBMutableCharacteristic *c in service.characteristics) {
        if ([c.UUID isEqual:uuid_to_nsuuid(characteristic_uuid)]) {
          characteristic = c;
          break;
        }
      }

      if (characteristic != nil) {
        NSData *data_ns = [NSData dataWithBytes:data.data() length:data.size()];
        [(CBPeripheralManager *)(wrapper->peripheral_manager)
                     updateValue:data_ns
               forCharacteristic:characteristic
            onSubscribedCentrals:nil];
      }
    }
  }
}

} // namespace cb
