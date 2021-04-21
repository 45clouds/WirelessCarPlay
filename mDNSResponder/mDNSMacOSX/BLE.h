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

#ifndef _BLE_H_
#define _BLE_H_

#include "dns_sd.h"
#include "dns_sd_private.h"

typedef unsigned int serviceHash_t;

void start_BLE_browse(DNSQuestion * q, const domainname *const typeDomain, DNS_TypeValues type, DNSServiceFlags flags,
                       mDNSu8 *key, size_t keySize);
bool stop_BLE_browse(const domainname *const typeDomain, DNS_TypeValues type, DNSServiceFlags flags);

void start_BLE_advertise(ServiceRecordSet * serviceRecordSet, const domainname *const domain, DNS_TypeValues type, DNSServiceFlags flags);
void stop_BLE_advertise(const domainname *const typeDomain, DNS_TypeValues type, DNSServiceFlags flags);

void responseReceived(serviceHash_t browseHash, serviceHash_t registeredHash, mDNSEthAddr *ptrToMAC);

void serviceBLE(void);

// C interfaces to Objective-C beacon management code.
void updateBLEBeaconAndScan(serviceHash_t browseHash, serviceHash_t registeredHash);
void stopBLEBeacon(void);

extern mDNS mDNSStorage;
extern mDNSBool EnableBLEBasedDiscovery;

// TODO: Add the following to a local D2D.h file
#include <DeviceToDeviceManager/DeviceToDeviceManager.h>

// Just define as the current max value for now.  
// TODO: Will need to define in DeviceToDeviceManager.framework if we convert this
// BLE discovery code to a D2D plugin.
#define D2DBLETransport D2DTransportMax

#define applyToBLE(interface, flags) ((interface == mDNSInterface_BLE) || ((interface == mDNSInterface_Any) && (flags & kDNSServiceFlagsAutoTrigger)))

#endif /* _BLE_H_ */
