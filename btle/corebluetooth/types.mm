#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#include <cstdint>

#include <span>
#include <vector>

#include "bt.h"
#include "helpers.h"

// characteristic

void Characteristic::set_descriptors(std::vector<Descriptor> descriptors) {
  CBMutableCharacteristic *chr = static_cast<CBMutableCharacteristic *>(raw_);
  NSMutableArray *arr = [NSMutableArray arrayWithCapacity:descriptors.size()];

  for (Descriptor d : descriptors) {
    [arr addObject:(CBDescriptor *)d.repr()];
  }

  chr.descriptors = arr;
}

UUID Characteristic::uuid() const {
  CBCharacteristic *chr = static_cast<CBCharacteristic *>(raw_);
  return UUID::parse(chr.UUID.UUIDString.UTF8String).value();
}

UUID ManagedCharacteristic::uuid() const {
  CBCharacteristic *chr = static_cast<CBCharacteristic *>(raw_);
  return UUID::parse(chr.UUID.UUIDString.UTF8String).value();
}

std::vector<Descriptor> Characteristic::descriptors() {
  CBCharacteristic *chr = static_cast<CBCharacteristic *>(raw_);
  NSArray *arr = chr.descriptors;

  std::vector<Descriptor> objs;
  for (int i = 0; i < [arr count]; ++i) {
    objs.push_back(Descriptor::from_raw([arr objectAtIndex:i]));
  }

  return objs;
}

std::vector<uint8_t> Characteristic::value() {
  CBCharacteristic *chr = static_cast<CBCharacteristic *>(raw_);
  NSData *nsd = chr.value;
  return {(uint8_t *)nsd.bytes, (uint8_t *)nsd.bytes + nsd.length};
}

void Characteristic::set_value(std::vector<uint8_t> value) {
  CBMutableCharacteristic *chr = static_cast<CBMutableCharacteristic *>(raw_);
  NSData *nsd = [NSData dataWithBytes:value.data() length:value.size()];
  chr.value = nsd;
}

std::optional<Characteristic>
Characteristic::from(UUID uuid, CharacteristicProperties properties,
                     Permissions permissions, std::vector<uint8_t> value) {
  CBUUID *cbuuid = str_to_cbuuid(uuid.to_string());
  NSData *nsd = [NSData dataWithBytes:value.data() length:value.size()];

  CBMutableCharacteristic *chr = [[CBMutableCharacteristic alloc]
      initWithType:cbuuid
        properties:(CBCharacteristicProperties)properties
             value:nsd
       permissions:(CBAttributePermissions)permissions.as_int()];

  return Characteristic(chr);
}

// descriptor

std::optional<Descriptor> Descriptor::from(UUID uuid, std::vector<char> value) {
  CBUUID *cbuuid = str_to_cbuuid(uuid.to_string());
  NSData *nsd = [NSData dataWithBytes:value.data() length:value.size()];

  CBMutableDescriptor *dsc =
      [[CBMutableDescriptor alloc] initWithType:cbuuid value:nsd];

  return Descriptor::from_raw((void *)dsc);
}

std::vector<uint8_t> Descriptor::value() {
  CBDescriptor *dsc = static_cast<CBDescriptor *>(raw_);
  id val = dsc.value;

  if ([val isKindOfClass:[NSData class]]) {
    NSData *nsdata = (NSData *)val;
    return {(uint8_t *)nsdata.bytes, (uint8_t *)nsdata.bytes + nsdata.length};
  }

  if ([val isKindOfClass:[NSString class]]) {
    NSString *nsstring = (NSString *)val;
    NSData *nsd = [nsstring dataUsingEncoding:NSUTF8StringEncoding];
    return {(uint8_t *)nsd.bytes, (uint8_t *)nsd.bytes + nsd.length};
  }

  return {};
}

UUID Descriptor::uuid() {
  auto *dsc = static_cast<CBDescriptor *>(raw_);
  return UUID::parse(dsc.UUID.UUIDString.UTF8String).value();
}

// peripheral

UUID Peripheral::uuid() const {
  auto *prph = static_cast<CBPeripheral *>(raw_);
  NSString *str = prph.identifier.UUIDString;
  std::string cpp_str = [str UTF8String];
  return UUID::parse(cpp_str).value();
}

std::vector<Service> Peripheral::services() {
  auto *prph = static_cast<CBPeripheral *>(raw_);
  NSArray *arr = prph.services;

  std::vector<Service> objs;
  objs.reserve([arr count]);

  for (int i = 0; i < [arr count]; ++i) {
    objs.push_back(Service::from_raw([arr objectAtIndex:i]));
  }

  return objs;
}

asio::awaitable<std::vector<Service>>
Peripheral::discover_services(std::optional<std::span<UUID>> uuids) {

  completion_signal_ = new asio::steady_timer(co_await asio::this_coro::executor);

  auto *prph = static_cast<CBPeripheral *>(raw_);
  NSArray *arr = nil;

  if (uuids.has_value()) {
    arr = uuids_to_cbuuids(uuids.value());
  }

  [prph discoverServices:arr];

  assert(completion_signal_ != nullptr);
  co_await completion_signal_->async_wait(asio::use_awaitable);

  co_return services();
}

asio::awaitable<std::vector<Characteristic>>
Peripheral::discover_characteristics(Service service,
                                     std::optional<std::span<UUID>> uuids) {
  auto *prph = static_cast<CBPeripheral *>(raw_);

  auto *svc = static_cast<CBService *>(service.repr());
  NSArray *arr = nil;

  if (uuids.has_value()) {
    arr = uuids_to_cbuuids(uuids.value());
  }

  [prph discoverCharacteristics:arr forService:svc];

  completion_signal_ = new asio::steady_timer(co_await asio::this_coro::executor);

  co_return service.characteristics();
}

void Peripheral::discover_descriptors(Characteristic characteristic) {
  auto *prph = static_cast<CBPeripheral *>(raw_);
  auto *chr = static_cast<CBCharacteristic *>(characteristic.repr());
  [prph discoverDescriptorsForCharacteristic:chr];
}

void Peripheral::read_characteristic(Characteristic characteristic) {
  auto *prph = static_cast<CBPeripheral *>(raw_);
  auto *chr = static_cast<CBCharacteristic *>(characteristic.repr());
  [prph readValueForCharacteristic:chr];
}

void Peripheral::read_descriptor(Descriptor descriptor) {
  auto *prph = static_cast<CBPeripheral *>(raw_);
  auto *dsc = static_cast<CBDescriptor *>(descriptor.repr());
  [prph readValueForDescriptor:dsc];
}

void Peripheral::write_characteristic(Characteristic characteristic,
                                      std::vector<uint8_t> value, int type) {
  auto *prph = static_cast<CBPeripheral *>(raw_);
  auto *chr = static_cast<CBCharacteristic *>(characteristic.repr());

  NSData *nsd = [NSData dataWithBytes:value.data() length:value.size()];
  [prph writeValue:nsd
      forCharacteristic:chr
                   type:(CBCharacteristicWriteType)type];
}

void Peripheral::write_descriptor(Descriptor descriptor,
                                  std::vector<uint8_t> value) {
  auto *prph = static_cast<CBPeripheral *>(raw_);
  auto *dsc = static_cast<CBDescriptor *>(descriptor.repr());

  NSData *nsd = [NSData dataWithBytes:value.data() length:value.size()];
  [prph writeValue:nsd forDescriptor:dsc];
}

size_t Peripheral::max_write_len(int type) {
  auto *prph = static_cast<CBPeripheral *>(raw_);
  return [prph maximumWriteValueLengthForType:(CBCharacteristicWriteType)type];
}

void Peripheral::set_notify(bool enabled, Characteristic &characteristic) {
  auto *prph = static_cast<CBPeripheral *>(raw_);
  auto *chr = static_cast<CBCharacteristic *>(characteristic.repr());
  [prph setNotifyValue:enabled forCharacteristic:chr];
}

PeripheralState Peripheral::state() {
  auto *prph = static_cast<CBPeripheral *>(raw_);
  return (PeripheralState)[prph state];
}

bool Peripheral::can_send_write_without_response() {
  auto *prph = static_cast<CBPeripheral *>(raw_);
  return [prph canSendWriteWithoutResponse];
}

void Peripheral::read_rssi() {
  auto *prph = static_cast<CBPeripheral *>(raw_);
  [prph readRSSI];
}

UUID Central::uuid() {
  CBCentral *cntrl = static_cast<CBCentral *>(raw_);
  NSString *str = cntrl.identifier.UUIDString;
  std::string cpp_str = [str UTF8String];
  return UUID::from_string(cpp_str);
}

void Central::notify(ManagedCharacteristic from, std::vector<uint8_t> value) {
  auto *cntrl = static_cast<CBCentral *>(raw_);
  auto *chr = static_cast<CBMutableCharacteristic *>(from.repr());
  NSData *nsd = [NSData dataWithBytes:value.data() length:value.size()];
  [cntrl updateValue:nsd forCharacteristic:chr onSubscribedCentrals:nil];
}

size_t Central::maximum_write_length() {
  auto *cntrl = static_cast<CBCentral *>(raw_);
  return [cntrl maximumUpdateValueLength];
}

// service

UUID Service::uuid() const {
  auto *svc = static_cast<CBService *>(raw_);
  return UUID::parse(svc.UUID.UUIDString.UTF8String).value();
}

std::vector<Characteristic> Service::characteristics() {
  auto *svc = static_cast<CBService *>(raw_);
  NSArray *arr = svc.characteristics;

  std::vector<Characteristic> objs;
  for (int i = 0; i < [arr count]; ++i) {
    objs.push_back(Characteristic::from_raw([arr objectAtIndex:i]));
  }

  return objs;
}

bool Service::is_primary() {
  auto *svc = static_cast<CBService *>(raw_);
  return svc.isPrimary;
}

std::vector<Service> Service::included_services() {
  auto *svc = static_cast<CBService *>(raw_);
  NSArray *arr = svc.includedServices;

  std::vector<Service> objs;
  for (int i = 0; i < [arr count]; ++i) {
    objs.push_back(Service::from_raw([arr objectAtIndex:i]));
  }

  return objs;
}

ManagedCharacteristic::ManagedCharacteristic(
    UUID uuid, CharacteristicProperties properties, Permissions permissions,
    std::optional<std::vector<uint8_t>> value) {
  CBUUID *cbuuid = str_to_cbuuid(uuid.to_string());

  // FIXME: currently only works with on-demand values (when value is null)
  NSData *nsd = nil;
  if (value.has_value()) {
    nsd =
        [NSData dataWithBytes:value.value().data() length:value.value().size()];
  }

  CBMutableCharacteristic *chr = [[CBMutableCharacteristic alloc]
      initWithType:cbuuid
        properties:(CBCharacteristicProperties)properties
             value:nsd
       permissions:(CBAttributePermissions)permissions.as_int()];

  raw_ = chr;
}

void ManagedCharacteristic::set_descriptors(
    std::vector<Descriptor> descriptors) {
  auto *chr = static_cast<CBMutableCharacteristic *>(raw_);
  NSMutableArray *arr = [NSMutableArray arrayWithCapacity:descriptors.size()];

  for (Descriptor d : descriptors) {
    [arr addObject:(CBDescriptor *)d.repr()];
  }

  chr.descriptors = arr;
}

void ManagedCharacteristic::set_value(std::vector<uint8_t> value) {
  CBMutableCharacteristic *chr = static_cast<CBMutableCharacteristic *>(raw_);
  NSData *nsd = [NSData dataWithBytes:value.data() length:value.size()];
  chr.value = nsd;
}

ManagedService::ManagedService(UUID uuid, bool primary) {
  CBUUID *cbuuid = str_to_cbuuid(uuid.to_string());
  CBMutableService *svc =
      [[CBMutableService alloc] initWithType:cbuuid primary:primary];
  raw_ = svc;
}

void ManagedService::add_characteristic(ManagedCharacteristic characteristic) {
  auto *svc = static_cast<CBMutableService *>(raw_);
  NSMutableArray *chars = [NSMutableArray arrayWithArray:svc.characteristics];

  auto *chr = static_cast<CBMutableCharacteristic *>(characteristic.repr());
  [chars addObject:chr];

  svc.characteristics = chars;
}
