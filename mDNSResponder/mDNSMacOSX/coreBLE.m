/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2015-2016 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mDNSEmbeddedAPI.h"
#include "DNSCommon.h"

#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <CoreBluetooth/CoreBluetooth_Private.h>
#import "mDNSMacOSX.h"
#import "BLE.h"
#import "coreBLE.h"

static coreBLE * coreBLEptr;

// Call Bluetooth subsystem to start/stop the the Bonjour BLE beacon and
// beacon scanning based on the current browse and registration hashes.
void updateBLEBeaconAndScan(serviceHash_t browseHash, serviceHash_t registeredHash)
{
    if (coreBLEptr == 0)
        coreBLEptr = [[coreBLE alloc] init];

    LogInfo("updateBLEBeaconAndScan: browseHash = 0x%x, registeredHash = 0x%x", browseHash, registeredHash);

    [coreBLEptr advertiseBrowses:browseHash andRegistrations: registeredHash];
    [coreBLEptr updateScan:(browseHash || registeredHash)];
}

// Stop the current BLE beacon.
void stopBLEBeacon(void)
{
    if (coreBLEptr == 0)
    {
        LogInfo("stopBLEBeacon called before BLE scan initialized ??");
        return;
    }

    LogInfo("stopBLEBeacon Stopping beacon");
    [coreBLEptr stopBeacon];
}

@implementation coreBLE
{
    CBCentralManager     *_centralManager;
    CBPeripheralManager  *_peripheralManager;

    NSData               *_currentlyAdvertisedData;

    // [_centralManager isScanning] is only avilable on iOS and not OSX,
    // so track scanning state locally.
    BOOL                 _isScanning;
}

- (id)init
{
    self = [super init];

    if (self)
    {
        _centralManager     = [[CBCentralManager alloc] initWithDelegate:self queue:dispatch_get_main_queue()];
        _peripheralManager  = [[CBPeripheralManager alloc] initWithDelegate:self queue:dispatch_get_main_queue()];
        _currentlyAdvertisedData = nil;
        _isScanning = NO;

        if (_centralManager == nil || _peripheralManager == nil )
        {
            LogMsg("coreBLE initialization failed!");
        } 
        else
        {
            LogInfo("coreBLE initialised");
        }
    }

    return self;
}

#define ADVERTISEMENTDATALENGTH 28 // 31 - 3 (3 bytes for flags)

// TODO: 
// DBDeviceTypeBonjour should eventually be added to the DBDeviceType definitions in WirelessProximity
// The Bluetooth team recommended using a value < 32 for prototyping, since 32 is the number of
// beacon types they can track in their duplicate beacon filtering logic.
#define DBDeviceTypeBonjour     26

// Beacon flags and version byte
#define BonjourBLEVersion     1

extern mDNS mDNSStorage;
extern mDNSInterfaceID AWDLInterfaceID;

// Transmit the last beacon indicating we are no longer advertising or browsing any services for two seconds.
#define LastBeaconTime 2

- (void) advertiseBrowses:(serviceHash_t) browseHash andRegistrations:(serviceHash_t) registeredHash
{
    uint8_t advertisingData[ADVERTISEMENTDATALENGTH] = {0, 0xff, 0x4c, 0x00 };
    uint8_t advertisingLength = 4;

    // TODO: If we have been transmitting a beacon, we probably want to continue transmitting
    // for a few seconds after both hashes are zero so that that any devices scanning 
    // can see the beacon indicating we have stopped all browses and advertisements.

    // Stop the beacon if there is no data to advertise.
    if (browseHash == 0 && registeredHash == 0)
    {
        LogInfo("advertiseBrowses:andRegistrations Stopping beacon in %d seconds", LastBeaconTime);
        if (mDNSStorage.NextBLEServiceTime)
            LogInfo("advertiseBrowses:andRegistrations: NextBLEServiceTime already set ??");

        mDNSStorage.NextBLEServiceTime = NonZeroTime(mDNS_TimeNow_NoLock(& mDNSStorage) + LastBeaconTime * mDNSPlatformOneSecond);
    }
    else
    {
        mDNSStorage.NextBLEServiceTime = 0;
    }

    // The beacon type.
    advertisingData[advertisingLength++] = DBDeviceTypeBonjour;

    // Flags and Version field
    advertisingData[advertisingLength++] = BonjourBLEVersion;

    memcpy(& advertisingData[advertisingLength], & browseHash, sizeof(serviceHash_t));
    advertisingLength += sizeof(serviceHash_t);
    memcpy(& advertisingData[advertisingLength], & registeredHash, sizeof(serviceHash_t));
    advertisingLength += sizeof(serviceHash_t);


    // Add the MAC address of the awdl0 interface.  Don't cache it since
    // it can get updated periodically.
    if (AWDLInterfaceID)
    {
        NetworkInterfaceInfoOSX *intf = IfindexToInterfaceInfoOSX(& mDNSStorage, AWDLInterfaceID);
        if (intf)
            memcpy(& advertisingData[advertisingLength], & intf->ifinfo.MAC, sizeof(mDNSEthAddr));
        else 
            memset( & advertisingData[advertisingLength], 0, sizeof(mDNSEthAddr));
    }
    else
    {
        // just use zero if not avaiblable
       memset( & advertisingData[advertisingLength], 0, sizeof(mDNSEthAddr));
    }
    advertisingLength += sizeof(mDNSEthAddr);

    // Total length of data advertised, minus this lenght byte.
    advertisingData[0] = (advertisingLength - 1);

    LogInfo("advertiseBrowses:andRegistrations advertisingLength = %d", advertisingLength);

    NSData* data = [NSData dataWithBytes:advertisingData length:advertisingLength];

    if([_peripheralManager isAdvertising] && [data isEqualToData: _currentlyAdvertisedData])
    {
        // No need to restart the advertisement if it is already active with the same data.
        LogInfo("advertiseBrowses:andRegistrations: No change in advertised data");
    }
    else
    {
        _currentlyAdvertisedData = data;

        if ([_peripheralManager isAdvertising])
        {
            LogInfo("advertiseBrowses:andRegistrations: Stop current advertisement before restarting");
            [_peripheralManager stopAdvertising];
        }
        LogInfo("advertiseBrowses:andRegistrations: Starting beacon");

        [_peripheralManager startAdvertising:@{ CBAdvertisementDataAppleMfgData : _currentlyAdvertisedData, CBCentralManagerScanOptionIsPrivilegedDaemonKey : @YES, @"kCBAdvOptionUseFGInterval" : @YES }];
    }
}

- (void) stopBeacon
{
    [_peripheralManager stopAdvertising];
}

- (void) updateScan:(bool) start
{
    if (_isScanning)
    {
        if (!start)
        {
            LogInfo("updateScan: stopping scan");
            [_centralManager stopScan];
            _isScanning = NO;
        }
    }
    else
    {
        if (start)
        {
            LogInfo("updateScan: starting scan");

            _isScanning = YES;
            [_centralManager scanForPeripheralsWithServices:nil options:@{ CBCentralManagerScanOptionAllowDuplicatesKey : @NO }];
        }
    }
}

#pragma mark - CBCentralManagerDelegate protocol

- (void)centralManagerDidUpdateState:(CBCentralManager *)central
{
    switch (central.state) {
        case CBCentralManagerStateUnknown:
            LogInfo("centralManagerDidUpdateState: CBCentralManagerStateUnknown");
            break;

        case CBCentralManagerStateResetting:
            LogInfo("centralManagerDidUpdateState: CBCentralManagerStateResetting");
            break;

        case CBCentralManagerStateUnsupported:
            LogInfo("centralManagerDidUpdateState: CBCentralManagerStateUnsupported");
            break;

        case CBCentralManagerStateUnauthorized:
            LogInfo("centralManagerDidUpdateState: CBCentralManagerStateUnauthorized");
            break;

        case CBCentralManagerStatePoweredOff:
            LogInfo("centralManagerDidUpdateState: CBCentralManagerStatePoweredOff");
            break;

        case CBCentralManagerStatePoweredOn:
            LogInfo("centralManagerDidUpdateState: CBCentralManagerStatePoweredOn");
            break;

        default:
            LogInfo("centralManagerDidUpdateState: Unknown state ??");
            break;
    }
}

// offset of beacon type in recieved CBAdvertisementDataManufacturerDataKey bytes
#define beaconTypeByteIndex 2

- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary<NSString *, id> *)advertisementData RSSI:(NSNumber *)RSSI
{
    (void) central;
    (void) peripheral;
    (void) RSSI;

    NSData *data = [advertisementData objectForKey:CBAdvertisementDataManufacturerDataKey];
   
    if (!data) {
        return;
    }

    unsigned char *bytes = (unsigned char *)data.bytes;
    
    // Just parse the DBDeviceTypeBonjour beacons.
    if (bytes[beaconTypeByteIndex] == DBDeviceTypeBonjour)
    {
        serviceHash_t browseHash, registeredHash;
        mDNSEthAddr senderMAC;
        unsigned char flagsAndVersion;
        unsigned char *ptr;

#if VERBOSE_BLE_DEBUG
        LogInfo("didDiscoverPeripheral: received DBDeviceTypeBonjour beacon, length = %d", [data length]);
        LogInfo("didDiscoverPeripheral: central = 0x%x, peripheral = 0x%x", central, peripheral);
#endif // VERBOSE_BLE_DEBUG

        // The DBDeviceTypeBonjour beacon bytes will be:
        // x4C, 0x0, 0x2A, flags_and_version_byte,, browseHash, advertisingServices_hash_bytes,
        // 6_bytes_of_sender_AWDL_MAC_address

        ptr = & bytes[beaconTypeByteIndex + 1];
        flagsAndVersion = *ptr++;
        memcpy(& browseHash, ptr, sizeof(serviceHash_t));
        ptr += sizeof(serviceHash_t);
        memcpy(& registeredHash, ptr, sizeof(serviceHash_t));
        ptr += sizeof(serviceHash_t);
        memcpy(& senderMAC, ptr, sizeof(senderMAC));

#if VERBOSE_BLE_DEBUG
        LogInfo("didDiscoverPeripheral: version = 0x%x, browseHash = 0x%x, registeredHash = 0x%x",
                flagsAndVersion, browseHash, registeredHash);
        LogInfo("didDiscoverPeripheral: sender MAC = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
            senderMAC.b[0], senderMAC.b[1], senderMAC.b[2], senderMAC.b[3], senderMAC.b[4], senderMAC.b[5]);
#else
        (void)flagsAndVersion; // Unused
#endif  // VERBOSE_BLE_DEBUG

        responseReceived(browseHash, registeredHash, & senderMAC);

#if VERBOSE_BLE_DEBUG
        // Log every 4th package during debug
        static int pkgsIn = 0;

        if (((pkgsIn++) & 3) == 0)
        {
            LogInfo("0x%x 0x%x 0x%x 0x%x 0x%x", bytes[0], bytes[1], bytes[2], bytes[3], bytes[4]);
//            LogInfo("0x%x 0x%x 0x%x 0x%x 0x%x", bytes[5], bytes[6], bytes[7], bytes[9], bytes[9]);
//            LogInfo("0x%x 0x%x 0x%x 0x%x 0x%x", bytes[10], bytes[11], bytes[12], bytes[13], bytes[14]);
//            LogInfo("0x%x 0x%x 0x%x 0x%x 0x%x", bytes[15], bytes[16], bytes[17], bytes[18], bytes[19]);
        }
#endif  // VERBOSE_BLE_DEBUG

    }
}

#pragma mark - CBPeripheralManagerDelegate protocol

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral
{

    switch (peripheral.state) {
        case CBPeripheralManagerStateUnknown:
            LogInfo("peripheralManagerDidUpdateState: CBPeripheralManagerStateUnknown");
            break;

        case CBPeripheralManagerStateResetting:
            LogInfo("peripheralManagerDidUpdateState: CBPeripheralManagerStateResetting");
            break;

        case CBPeripheralManagerStateUnsupported:
            LogInfo("peripheralManagerDidUpdateState: CBPeripheralManagerStateUnsupported");
            break;

        case CBPeripheralManagerStateUnauthorized:
            LogInfo("peripheralManagerDidUpdateState: CBPeripheralManagerStateUnauthorized");
            break;

        case CBPeripheralManagerStatePoweredOff:
            LogInfo("peripheralManagerDidUpdateState: CBPeripheralManagerStatePoweredOff");
            break;

        case CBPeripheralManagerStatePoweredOn:
            LogInfo("peripheralManagerDidUpdateState: CBPeripheralManagerStatePoweredOn");
            break;

        default:
            LogInfo("peripheralManagerDidUpdateState: Unknown state ??");
            break;
    }
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral error:(nullable NSError *)error
{
    (void) peripheral;

    if (error)
    {
        const char * errorString = [[error localizedDescription] cStringUsingEncoding:NSASCIIStringEncoding];
        LogInfo("peripheralManagerDidStartAdvertising: error = %s", errorString ? errorString: "unknown");
    }
    else
    {
        LogInfo("peripheralManagerDidStartAdvertising:");
    }
}

@end
