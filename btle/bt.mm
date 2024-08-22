#import "bt.h"
#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#include <cstdio>

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

static dispatch_queue_t bt_queue;

@interface BluetoothDelegate
    : NSObject <CBCentralManagerDelegate, CBPeripheralManagerDelegate>
@end

@implementation BluetoothDelegate

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
}

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
  NSLog(@"state %ld", peripheral.state);
}

- (void)centralManager:(CBCentralManager *)central
    didDiscoverPeripheral:(CBPeripheral *)peripheral
        advertisementData:(NSDictionary<NSString *, id> *)advertisementData
                     RSSI:(NSNumber *)RSSI {
}

- (void)centralManager:(CBCentralManager *)central
    didConnectPeripheral:(CBPeripheral *)peripheral {
}

- (void)centralManager:(CBCentralManager *)central
    didFailToConnectPeripheral:(CBPeripheral *)peripheral
                         error:(NSError *)error {
}

- (void)centralManager:(CBCentralManager *)central
    didDisconnectPeripheral:(CBPeripheral *)peripheral
                      error:(NSError *)error {
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didReceiveWriteRequests:(NSArray<CBATTRequest *> *)requests {
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral
                                       error:(NSError *)error {
  if (error == nullptr) {
    NSLog(@"Successfully started advertising.");
  } else {
    NSLog(@"Error starting advertising: %@", error);
  }
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
                         central:(CBCentral *)central
    didSubscribeToCharacteristic:(CBCharacteristic *)characteristic {
}

@end

namespace cb {

void bt_init() {
  if (bt_queue == nullptr) {
    // bt_queue = dispatch_queue_create("bt_queue", nullptr);
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

CBCentralManagerWrapper create_central_manager() {
  CBCentralManagerWrapper wrapper = CBCentralManagerWrapper();

  BluetoothDelegate *bt_dlg = [[BluetoothDelegate alloc] init];
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
  CBPeripheralManagerWrapper wrapper = CBPeripheralManagerWrapper();

  BluetoothDelegate *bt_dlg = [[BluetoothDelegate alloc] init];
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

} // namespace cb
