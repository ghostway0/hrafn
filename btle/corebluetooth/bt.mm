#include <Foundation/NSObjCRuntime.h>

#import <CoreBluetooth/CBAdvertisementData.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#include <map>
#include <optional>
#include <span>
#include <vector>

#include "helpers.h"
#include "btle/types.h"
#import "bt.h"

@interface PeripheralDelegate : NSObject <CBPeripheralDelegate> {
  Peripheral *parent_;
};
@end

@implementation PeripheralDelegate

- (void)peripheral:(CBPeripheral *)peripheral
    didDiscoverServices:(NSError *)error {
  parent_->clear_services();
  for (CBService *svc in peripheral.services) {
    parent_->add_service(Service::from_raw((void *)svc));
  }
}

- (void)peripheral:(CBPeripheral *)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(NSError *)error {
  Service svc = Service::from_raw((void *)service);
  std::vector<Characteristic> chrs;
  for (CBCharacteristic *chr in service.characteristics) {
    chrs.push_back(Characteristic::from_raw((void *)chr));
  }

  svc.set_characteristics(chrs);
}

- (void)peripheral:(CBPeripheral *)peripheral
    didDiscoverDescriptorsForCharacteristic:(CBCharacteristic *)characteristic
                                      error:(NSError *)error {
  Characteristic chr = Characteristic::from_raw((void *)characteristic);
  std::vector<Descriptor> dscs;
  for (CBDescriptor *dsc in characteristic.descriptors) {
    dscs.push_back(Descriptor::from_raw((void *)dsc));
  }

  chr.set_descriptors(dscs);
}

// is this needed?
- (void)peripheral:(CBPeripheral *)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
                              error:(NSError *)error {
  Characteristic chr = Characteristic::from_raw((void *)characteristic);
  std::vector<uint8_t> value = chr.value();
}

- (void)peripheral:(CBPeripheral *)peripheral
    didUpdateValueForDescriptor:(CBDescriptor *)descriptor
                          error:(NSError *)error {
  Descriptor dsc = Descriptor::from_raw((void *)descriptor);
  std::vector<uint8_t> value = dsc.value();
}

- (void)peripheral:(CBPeripheral *)peripheral
    didWriteValueForCharacteristic:(CBCharacteristic *)characteristic
                             error:(NSError *)error {
  Characteristic chr = Characteristic::from_raw((void *)characteristic);
}

- (void)peripheral:(CBPeripheral *)peripheral
    didWriteValueForDescriptor:(CBDescriptor *)descriptor
                         error:(NSError *)error {
  Descriptor dsc = Descriptor::from_raw((void *)descriptor);
}

- (void)peripheral:(CBPeripheral *)peripheral
    didUpdateNotificationStateForCharacteristic:
        (CBCharacteristic *)characteristic
                                          error:(NSError *)error {
  Characteristic chr = Characteristic::from_raw((void *)characteristic);
  // parent_->on_notify(chr);
}

@end

@interface CentralManagerDelegate : NSObject <CBCentralManagerDelegate> {
  CentralManager *parent;
  PeripheralDelegate *peripheral_delegate;
};
@end

@implementation CentralManagerDelegate

- (instancetype)init {
  NSLog(@"CentralManagerDelegate init");
  self = [super init];
  peripheral_delegate = [[PeripheralDelegate alloc] init];
  return self;
}

- (void)centralManager:(CBCentralManager *)central
    didDiscoverPeripheral:(CBPeripheral *)peripheral
        advertisementData:(NSDictionary<NSString *, id> *)advertisementData
                     RSSI:(NSNumber *)rssi {
  std::map<std::string, std::string> adv_dict;
  for (NSString *key in advertisementData) {
    id val = [advertisementData objectForKey:key];
    if ([val isKindOfClass:[NSData class]]) {
      auto *nsd = (NSData *)val;
      std::string str((const char *)nsd.bytes, nsd.length);
      adv_dict[[key UTF8String]] = str;
    } else if ([val isKindOfClass:[NSString class]]) {
      auto *nsstr = (NSString *)val;
      adv_dict[[key UTF8String]] = [nsstr UTF8String];
    }
  }
  
  NSLog(@"Discovered peripheral: %@", peripheral);

  Peripheral prph = Peripheral::from_raw((void *)peripheral);
  self->parent->on_discovered(prph, {});
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
}

@end

@interface CentralManagerBase : NSObject <CBCentralManagerDelegate> {
  CentralManager *parent;
  PeripheralDelegate *peripheral_delegate;
  dispatch_queue_t queue;
  CBCentralManager *cmgr;
};

@end

@implementation CentralManagerBase

- (instancetype)init:(CentralManager *)parent {
  self = [super init];
  queue = dispatch_queue_create("com.BULLSHIT", DISPATCH_QUEUE_SERIAL);
  cmgr = [[CBCentralManager alloc] initWithDelegate:self queue:queue];
  self->parent = parent;

  return self;
}

- (void)centralManager:(CBCentralManager *)central
    didDiscoverPeripheral:(CBPeripheral *)peripheral
        advertisementData:(NSDictionary<NSString *, id> *)raw_adv
                     RSSI:(NSNumber *)rssi {
  AdvertisingData advertised{};

  for (NSString *key in raw_adv) {
    id val = [raw_adv objectForKey:key];
  }

  Peripheral prph = Peripheral::from_raw((void *)peripheral);

  self->parent->on_discovered(prph, advertised);
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
}

- (CBCentralManager *)underlying {
  return self->cmgr;
}

- (PeripheralDelegate *)peripheral_delegate {
  return self->peripheral_delegate;
}

@end

CentralManager::CentralManager() {
  auto *delegate = [[CentralManagerBase alloc] init:this];

  raw_ = delegate;
}

int CentralManager::state() {
  auto *cmgr = [static_cast<CentralManagerBase *>(raw_) underlying];
  return [cmgr state];
}

void CentralManager::scan(std::span<UUID> service_uuids,
                          ScanOptions const &opts) {
  auto *cmgr = [static_cast<CentralManagerBase *>(raw_) underlying];
  NSArray *arr_svc_uuids = uuids_to_cbuuids(service_uuids);

  NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];

  if (opts.allow_dups) {
    [dict setObject:[NSNumber numberWithBool:YES]
             forKey:CBCentralManagerScanOptionAllowDuplicatesKey];
  }

  if (!opts.solicited_services.empty()) {
    NSMutableArray *arr_sol_svc_uuids =
        uuids_to_cbuuids(opts.solicited_services);
    [dict setObject:arr_sol_svc_uuids
             forKey:CBCentralManagerScanOptionSolicitedServiceUUIDsKey];
  }

  [cmgr scanForPeripheralsWithServices:arr_svc_uuids options:dict];
}

void CentralManager::stop_scan() {
  auto *cmgr = [static_cast<CentralManagerBase *>(raw_) underlying];
  [cmgr stopScan];
}

bool CentralManager::is_scanning() {
  auto *cmgr = [static_cast<CentralManagerBase *>(raw_) underlying];
  return [cmgr isScanning];
}

void CentralManager::connect(Peripheral peripheral,
                             ConnectOptions const &opts) {
  auto *cmgr = [static_cast<CentralManagerBase *>(raw_) underlying];
  auto *prph = static_cast<CBPeripheral *>(peripheral.repr());

  NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];

  if (opts.notify_on_connection) {
    [dict setObject:[NSNumber numberWithBool:YES]
             forKey:CBConnectPeripheralOptionNotifyOnConnectionKey];
  }

  if (opts.notify_on_disconnection) {
    [dict setObject:[NSNumber numberWithBool:YES]
             forKey:CBConnectPeripheralOptionNotifyOnDisconnectionKey];
  }

  if (opts.notify_on_notification) {
    [dict setObject:[NSNumber numberWithBool:YES]
             forKey:CBConnectPeripheralOptionNotifyOnNotificationKey];
  }

  [cmgr connectPeripheral:prph options:dict];
}

void CentralManager::cancel_connect(Peripheral &peripheral) {
  auto *cmgr = [static_cast<CentralManagerBase *>(raw_) underlying];
  auto *prph = static_cast<CBPeripheral *>(peripheral.repr());
  [cmgr cancelPeripheralConnection:prph];
}

@interface PeripheralManagerDelegate : NSObject <CBPeripheralManagerDelegate> {
  PeripheralManager *parent;
  CBPeripheralManager *mgr;
  dispatch_queue_t queue;
};

@end

@implementation PeripheralManagerDelegate

- (instancetype)init {
  self = [super init];
  queue = dispatch_queue_create("com.BULLSHIT", DISPATCH_QUEUE_SERIAL);
  mgr = [[CBPeripheralManager alloc] initWithDelegate:self queue:queue];
  return self;
}

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
            didAddService:(CBService *)service
                    error:(NSError *)error {
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
                         central:(CBCentral *)central
    didSubscribeToCharacteristic:(CBCharacteristic *)characteristic {
  parent->on_subscribe(Peripheral::from_raw((void *)central),
                       Characteristic::from_raw((void *)characteristic));
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
                             central:(CBCentral *)central
    didUnsubscribeFromCharacteristic:(CBCharacteristic *)characteristic {
  parent->on_unsubscribe(Peripheral::from_raw((void *)central),
                         Characteristic::from_raw((void *)characteristic));
}

- (void)peripheralManagerIsReadyToUpdateSubscribers:
    (CBPeripheralManager *)peripheral {
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didReceiveReadRequest:(CBATTRequest *)request {
  parent->on_read(Peripheral::from_raw((void *)request.central),
                  Characteristic::from_raw((void *)request.characteristic));
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didReceiveWriteRequests:(NSArray<CBATTRequest *> *)requests {
  for (CBATTRequest *req in requests) {
    parent->on_write(
        Peripheral::from_raw((void *)req.central),
        Characteristic::from_raw((void *)req.characteristic),
        {(char *)req.value.bytes, (char *)req.value.bytes + req.value.length});
  }
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
         willRestoreState:(NSDictionary<NSString *, id> *)dict {
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
      didStartAdvertising:(NSError *)error {
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
     didReceiveConnection:(CBPeripheral *)central
     didReceiveConnection:(CBPeripheral *)peripheral {
  parent->on_connect(Peripheral::from_raw((void *)peripheral));
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
      didOpenL2CAPChannel:(CBL2CAPChannel *)channel
                    error:(NSError *)error {
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didPublishL2CAPChannel:(CBL2CAPPSM)PSM
                     error:(NSError *)error {
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didUnpublishL2CAPChannel:(CBL2CAPPSM)PSM
                       error:(NSError *)error {
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
     didCloseL2CAPChannel:(CBL2CAPChannel *)channel
                    error:(NSError *)error {
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didUpdateValueForCharacteristic:(CBMutableCharacteristic *)characteristic
                              error:(NSError *)error {
}

- (CBPeripheralManager *)underlying {
  return self->mgr;
}

@end

PeripheralManager::PeripheralManager() {
  auto *delegate = [[PeripheralManagerDelegate alloc] init];
  raw_ = delegate;
}

void PeripheralManager::add_service(ManagedService service) {
  auto *mgr = [static_cast<PeripheralManagerDelegate *>(raw_) underlying];
  auto *svc = static_cast<CBMutableService *>(service.repr());
  [mgr addService:svc];
}

void PeripheralManager::start_advertising(AdvertisingOptions const &opts) {
  auto *mgr = [static_cast<PeripheralManagerDelegate *>(raw_) underlying];
  NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];

  if (opts.include_tx_power_level) {
    [dict setObject:[NSNumber numberWithBool:YES]
             forKey:CBAdvertisementDataTxPowerLevelKey];
  }

  if (opts.include_local_name) {
    [dict setObject:[NSNumber numberWithBool:YES]
             forKey:CBAdvertisementDataLocalNameKey];
  }

  if (opts.include_device_name) {
    [dict setObject:[NSNumber numberWithBool:YES]
             forKey:CBAdvertisementDataLocalNameKey];
  }

  if (!opts.service_uuids.empty()) {
    NSMutableArray *arr = uuids_to_cbuuids(opts.service_uuids);
    [dict setObject:arr forKey:CBAdvertisementDataServiceUUIDsKey];
  }

  if (!opts.manufacturer_data.empty()) {
    NSData *nsd = [NSData dataWithBytes:opts.manufacturer_data.data()
                                 length:opts.manufacturer_data.size()];
    [dict setObject:nsd forKey:CBAdvertisementDataManufacturerDataKey];
  }

  if (!opts.service_data.empty()) {
    for (const auto &[uuid, data] : opts.service_data) {
      CBUUID *cbuuid = str_to_cbuuid(uuid.to_string());
      NSData *nsd = [NSData dataWithBytes:data.data() length:data.size()];
      [dict setObject:nsd forKey:cbuuid];
    }
  }

  if (!opts.overflow_service_uuids.empty()) {
    NSMutableArray *arr = uuids_to_cbuuids(opts.overflow_service_uuids);
    [dict setObject:arr forKey:CBAdvertisementDataOverflowServiceUUIDsKey];
  }

  if (!opts.solicited_service_uuids.empty()) {
    NSMutableArray *arr = uuids_to_cbuuids(opts.solicited_service_uuids);
    [dict setObject:arr forKey:CBAdvertisementDataSolicitedServiceUUIDsKey];
  }

  if (!opts.local_name.empty()) {
    [dict setObject:[NSString stringWithUTF8String:opts.local_name.c_str()]
             forKey:CBAdvertisementDataLocalNameKey];
  }

  [mgr startAdvertising:dict];
}

void PeripheralManager::stop_advertising() {
  auto *mgr = [static_cast<PeripheralManagerDelegate *>(raw_) underlying];
  [mgr stopAdvertising];
}

bool PeripheralManager::is_advertising() {
  auto *mgr = [static_cast<PeripheralManagerDelegate *>(raw_) underlying];
  return [mgr isAdvertising];
}
