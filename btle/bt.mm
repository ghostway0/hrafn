#import "bt.h"
#include <cstdio>

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

namespace cb {

static DiscoveredPeripheralCallback discovered_peripheral_callback;
static ConnectionCallback connection_callback;
static SubscriptionCallback subscription_callback;
static DataReceivedCallback data_received_callback;

static dispatch_queue_t bt_queue;

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

void bt_init() {
  if (bt_queue == nullptr) {
    bt_queue = dispatch_queue_create("bt_queue", nullptr);
  }
}

void set_discovered_peripheral_callback(DiscoveredPeripheralCallback callback) {
  discovered_peripheral_callback = std::move(callback);
}

void set_connection_callback(ConnectionCallback callback) {
  connection_callback = std::move(callback);
}

void set_subscription_callback(SubscriptionCallback callback) {
  subscription_callback = std::move(callback);
}

void set_data_received_callback(DataReceivedCallback callback) {
  data_received_callback = std::move(callback);
}

@interface BluetoothDelegate
    : NSObject <CBCentralManagerDelegate, CBPeripheralManagerDelegate>
@end

@implementation BluetoothDelegate

- (void)centralManager:(CBCentralManager *)central
    didDiscoverPeripheral:(CBPeripheral *)peripheral
        advertisementData:(NSDictionary<NSString *, id> *)advertisementData
                     RSSI:(NSNumber *)RSSI {
  // Handle peripheral discovery
  UUID uuid = nsuuid_to_uuid(peripheral.identifier);
  std::string name = peripheral.name ? [peripheral.name UTF8String] : "";
  discovered_peripheral_callback(uuid, name);
}

- (void)centralManager:(CBCentralManager *)central
    didConnectPeripheral:(CBPeripheral *)peripheral {
  UUID uuid = nsuuid_to_uuid(peripheral.identifier);
  connection_callback(uuid, true);
}

- (void)centralManager:(CBCentralManager *)central
    didFailToConnectPeripheral:(CBPeripheral *)peripheral
                         error:(NSError *)error {
  UUID uuid = nsuuid_to_uuid(peripheral.identifier);
  connection_callback(uuid, false);
}

- (void)centralManager:(CBCentralManager *)central
    didDisconnectPeripheral:(CBPeripheral *)peripheral
                      error:(NSError *)error {
  UUID uuid = nsuuid_to_uuid(peripheral.identifier);
  connection_callback(uuid, false);
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral
                                       error:(NSError *)error {
  if (error) {
    NSLog(@"Error starting advertising: %@", error);
  } else {
    NSLog(@"Successfully started advertising.");
  }
}

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
                         central:(CBCentral *)central
    didSubscribeToCharacteristic:(CBCharacteristic *)characteristic {
  UUID central_uuid = nsuuid_to_uuid(central.identifier);
  UUID char_uuid = cbuuid_to_uuid(characteristic.UUID);
  subscription_callback(central_uuid, char_uuid);
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didReceiveWriteRequests:(NSArray<CBATTRequest *> *)requests {
  for (CBATTRequest *request in requests) {
    UUID uuid = cbuuid_to_uuid(request.characteristic.UUID);
    std::vector<uint8_t> data((uint8_t *)request.value.bytes,
                              (uint8_t *)request.value.bytes +
                                  request.value.length);
    data_received_callback(uuid, data);
    [peripheral respondToRequest:request withResult:CBATTErrorSuccess];
  }
}

@end

CBCentralManagerWrapper *create_central_manager() {
  CBCentralManagerWrapper *wrapper = new CBCentralManagerWrapper();
  wrapper->delegate =
      static_cast<void const *>([[BluetoothDelegate alloc] init]);
  wrapper->central_manager = static_cast<void const *>([[CBCentralManager alloc]
      initWithDelegate:(__bridge id<CBCentralManagerDelegate>)(wrapper
                                                                   ->delegate)
                 queue:bt_queue]);
  return wrapper;
}

void destroy_central_manager(CBCentralManagerWrapper *wrapper) {
  [(id)(wrapper->central_manager) release];
  [(id)(wrapper->delegate) release];
  delete wrapper;
}

CBPeripheralManagerWrapper *create_peripheral_manager() {
  CBPeripheralManagerWrapper *wrapper = new CBPeripheralManagerWrapper();
  wrapper->delegate =
      static_cast<void const *>([[BluetoothDelegate alloc] init]);
  wrapper->peripheral_manager =
      static_cast<void const *>([[CBPeripheralManager alloc]
          initWithDelegate:(__bridge id<
                               CBPeripheralManagerDelegate>)(wrapper->delegate)
                     queue:bt_queue]);
  return wrapper;
}

void destroy_peripheral_manager(CBPeripheralManagerWrapper *wrapper) {
  [(id)(wrapper->peripheral_manager) release];
  [(id)(wrapper->delegate) release];
  delete wrapper;
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

  NSDictionary *advertising_data = @{
    CBAdvertisementDataLocalNameKey :
        [NSString stringWithUTF8String:data.local_name.c_str()],
    CBAdvertisementDataServiceUUIDsKey : service_uuids,
    CBAdvertisementDataManufacturerDataKey :
        [NSData dataWithBytes:data.manufacturer_data.data()
                       length:data.manufacturer_data.size()]
  };
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
  }

  return [[CBMutableCharacteristic alloc] initWithType:uuid
                                            properties:properties
                                                 value:value
                                           permissions:permissions];
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

} // namespace cb
