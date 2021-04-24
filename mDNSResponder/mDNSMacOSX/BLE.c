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
#include "mDNSMacOSX.h"
#include "BLE.h"
#include <pthread.h>

#pragma mark - Browse and Registration Request Handling

// Disable use of BLE discovery APIs by default.
mDNSBool EnableBLEBasedDiscovery = mDNSfalse;

typedef struct matchingResponses 
{
    struct matchingResponses * next;
    void * response;
} matchingResponses_t;

// Initially used for both the browse and registration lists.
typedef struct requestList
{
    struct requestList  * next;
    unsigned int        refCount;
    domainname          name;
    mDNSu16             type;
    DNSServiceFlags     flags;
    mDNSInterfaceID     InterfaceID;

// TODO: Possibly restructure the following browse and registration specific 
// members as a union to save a bit of space.

    // The following fields are only used for browse requests currently
    serviceHash_t       browseHash;
    DNSQuestion         * question;
    mDNSu8              key[MAX_DOMAIN_LABEL];
    size_t              keySize;
    matchingResponses_t * ourResponses;

    // The following fields are only used for registration requests currently
    serviceHash_t       registeredHash;
    ServiceRecordSet    * serviceRecordSet; // service record set in the original request
    AuthRecType         savedARType;
    bool                triggeredOnAWDL;
} requestList_t;

// Lists for all DNSServiceBrowse() and DNSServiceRegister() requests using 
// BLE beacon based triggering.
static requestList_t* BLEBrowseListHead = NULL;
static requestList_t* BLERegistrationListHead = NULL;

#define isAutoTriggerRequest(ptr) ((ptr->InterfaceID == kDNSServiceInterfaceIndexAny) && (ptr->flags & kDNSServiceFlagsAutoTrigger))

#pragma mark - Manage list of responses that match this request.

mDNSlocal bool inResponseListForRequest(requestList_t *request, void * response)
{
    matchingResponses_t * rp;

    for (rp = request->ourResponses; rp; rp = rp->next)
        if (rp->response == response)
            break;
    
    return (rp != 0);
}

mDNSlocal void addToResponseListForRequest(requestList_t *request, void * response)
{
    matchingResponses_t *matchingResponse = calloc(1, sizeof(matchingResponses_t));

    if (matchingResponse == NULL)
    {
        LogMsg("addToResponseListForRequest: calloc() failed!");
        return;
    }
    matchingResponse->response = response;
    matchingResponse->next = request->ourResponses;
    request->ourResponses = matchingResponse;
}

// If response is currently in the list of responses, remove it and return true.
// Othewise, return false.
mDNSlocal bool removeFromResponseListForRequest(requestList_t *request, void * response)
{
    matchingResponses_t ** nextp;
    bool responseRemoved = false;

    for (nextp = & request->ourResponses; *nextp; nextp = & (*nextp)->next)
        if ((*nextp)->response == response)
            break;

    if (*nextp)
    {
        LogInfo("removeFromResponseListForRequest: response no longer matches for  %##s %s ", request->name.c, DNSTypeName(request->type));

        responseRemoved = true;
        matchingResponses_t *tmp = *nextp;
        *nextp = (*nextp)->next;
        free(tmp);
    }
    return responseRemoved;
}

// Free all current entries on the response list for this request.
mDNSlocal void freeResponseListEntriesForRequest(requestList_t *request)
{
    matchingResponses_t * ptr;

    ptr = request->ourResponses; 
    while (ptr)
    {
        matchingResponses_t * tmp;

        tmp = ptr;
        ptr = ptr->next;
        free(tmp);
    }
}

#pragma mark - Manage request lists

mDNSlocal requestList_t ** findInRequestList(requestList_t ** listHead, const domainname *const name, mDNSu16 type)
{
    requestList_t **ptr = listHead;

    for ( ; *ptr; ptr = &(*ptr)->next)
        if ((*ptr)->type == type && SameDomainName(&(*ptr)->name, name))
            break;

    return ptr;
}

mDNSlocal requestList_t * addToRequestList(requestList_t ** listHead, const domainname *const name, mDNSu16 type, DNSServiceFlags flags)
{
    requestList_t **ptr = findInRequestList(listHead, name, type);

    if (!*ptr)
    {
        *ptr = mDNSPlatformMemAllocate(sizeof(**ptr));
        mDNSPlatformMemZero(*ptr, sizeof(**ptr));
        (*ptr)->type = type;
        (*ptr)->flags = flags;
        AssignDomainName(&(*ptr)->name, name);
    }
    (*ptr)->refCount += 1;

    LogInfo("addToRequestList: %##s %s refcount now %u", (*ptr)->name.c, DNSTypeName((*ptr)->type), (*ptr)->refCount);

    return *ptr;
}

mDNSlocal void removeFromRequestList(requestList_t ** listHead, const domainname *const name, mDNSu16 type)
{
    requestList_t **ptr = findInRequestList(listHead, name, type);

    if (!*ptr) { LogMsg("removeFromRequestList: Didn't find %##s %s in list", name->c, DNSTypeName(type)); return; }

    (*ptr)->refCount -= 1;

    LogInfo("removeFromRequestList: %##s %s refcount now %u", (*ptr)->name.c, DNSTypeName((*ptr)->type), (*ptr)->refCount);

    if (!(*ptr)->refCount)
    {
        requestList_t *tmp = *ptr;
        *ptr = (*ptr)->next;
        freeResponseListEntriesForRequest(tmp);
        mDNSPlatformMemFree(tmp);
    }
}

#pragma mark - Hashing and beacon state 

// Simple string hash based on the Bernstein hash.

#define PRIME   31  // small prime number
#define MODULO  (sizeof(serviceHash_t) * 8)
#define CONVERT_TO_LOWER_CASE(x) (((x) <= 'Z' && (x) >= 'A') ? ((x) | 0x20) : (x))

mDNSlocal serviceHash_t BLELabelHash(const unsigned char *str, unsigned int length)
{
    serviceHash_t hash = 0;

    for (unsigned int i = 0; i < length; i++) {
        hash = PRIME * hash + CONVERT_TO_LOWER_CASE(*str);
        str++;
    }

    hash %= MODULO;
    LogInfo("BLELabelHash: %d characters hashed to %d", length, hash);

    return ((serviceHash_t)1 << hash);
}

// Hash just the service type not including the protocol or first "_" character initially.
mDNSlocal serviceHash_t BLEServiceHash(const domainname *const domain)
{
    const unsigned char *p = domain->c;
    unsigned int length = (unsigned int) *p;

    p++;
    if (*p != '_')
    {
        LogInfo("BLEServiceHash: browse type does not begin with a _");
        return 0;
    }
    p++;  // skip the '-"
    length--;

    if (length > MAX_DOMAIN_LABEL || length == 0)
    {
        LogInfo("BLEServiceHash: invalid browse type length: %d characters", length);
        return 0;
    }

    return BLELabelHash(p, length);
}

// Storage for the current Bonjour BLE beacon data;
typedef struct BLEBeacon
{
    serviceHash_t browseHash;
    serviceHash_t registeredHash;
} BLEBeacon_t;

BLEBeacon_t BLEBeacon;

mDNSlocal void addServiceToBeacon(serviceHash_t browseHash, serviceHash_t registeredHash)
{
    bool beaconUpdated = false;

    if (BLEBeacon.browseHash & browseHash)
    {
        LogInfo("addServiceToBeacon: Bit 0x%x already set in browsing services hash", browseHash);
    }
    else
    {
        BLEBeacon.browseHash |= browseHash;
        beaconUpdated = true;
    }

    if (BLEBeacon.registeredHash & registeredHash)
    {
        LogInfo("addServiceToBeacon: Bit 0x%x already set in advertising services hash", registeredHash);
    }
    else
    {
        BLEBeacon.registeredHash |= registeredHash;
        beaconUpdated = true;
    }

    if (beaconUpdated)
        updateBLEBeaconAndScan(BLEBeacon.browseHash, BLEBeacon.registeredHash);
}

// Go through all the existing browses and registrations to get the
// current hash values for the corresponding BLE beacon.
// We must do this when any hash bits are removed do accurately generate
// the correct combination of all currently set hash bits.
mDNSlocal void updateBeacon()
{
    requestList_t *ptr;

    BLEBeacon.browseHash = 0;
    BLEBeacon.registeredHash = 0;

    for (ptr = BLEBrowseListHead; ptr; ptr = ptr->next)
    {
        BLEBeacon.browseHash |= ptr->browseHash;
    }

    for (ptr = BLERegistrationListHead; ptr; ptr = ptr->next)
    {
        BLEBeacon.registeredHash |= ptr->registeredHash;
    }

    updateBLEBeaconAndScan(BLEBeacon.browseHash, BLEBeacon.registeredHash);
}

#pragma mark - Request start/stop

// Forward declarations for mDNSLocal functions that are called before they are defined.
mDNSlocal void checkForMatchingResponses(requestList_t *bp);
mDNSlocal void clearResponseLists();

void start_BLE_browse(DNSQuestion * q, const domainname *const domain, DNS_TypeValues type, DNSServiceFlags flags, mDNSu8 *key, size_t keySize)
{
    requestList_t * ptr; 

    if (!EnableBLEBasedDiscovery)
    {
        LogMsg("start_BLE_browse: EnableBLEBasedDiscovery disabled");
        return;
    }

    LogInfo("start_BLE_browse: Starting BLE browse for: %##s %s", domain->c, DNSTypeName(type));

    ptr = addToRequestList(&BLEBrowseListHead, domain, type, flags);

    // If equivalent BLE browse is already running, just return.
    if (ptr->refCount > 1)
    {
        LogInfo("start_BLE_browse: Dup of existing BLE browse.");
        return;
    }

    ptr->browseHash = BLEServiceHash(domain);
    ptr->question = q;

    if (ptr->browseHash == 0)
    {
        LogInfo("BLEServiceHash failed!");
        removeFromRequestList(&BLEBrowseListHead, domain, type);
        return;
    }

    // Save these for use in D2D plugin callback logic.
    memcpy(ptr->key, key, keySize);
    ptr->keySize = keySize;
    // Extract the interface ID for easier access in the requestList_t structure
    ptr->InterfaceID = q->InterfaceID;

    addServiceToBeacon(ptr->browseHash, 0);

    checkForMatchingResponses(ptr);
}

// Stop the browse.
// Return true if this is the last reference to the browse, false otherwise.
bool stop_BLE_browse(const domainname *const domain, DNS_TypeValues type, DNSServiceFlags flags)
{
    (void)  flags;   // not used initially
    requestList_t * ptr;
    bool    lastReference = false;

    if (!EnableBLEBasedDiscovery)
    {
        LogMsg("stop_BLE_browse: EnableBLEBasedDiscovery disabled");
        return lastReference;
    }

    LogInfo("stop_BLE_browse: Stopping BLE browse for: %##s %s", domain->c, DNSTypeName(type));

    ptr = *(findInRequestList(&BLEBrowseListHead, domain, type));
    if (ptr == 0)
    {
        LogInfo("stop_BLE_browse: No matching browse found.");
        return lastReference;
    }
    
    // If this is the last reference for this browse, update advertising and browsing bits set in
    // the beacon after removing this browse from the list.
    if (ptr->refCount == 1)
        lastReference = true;

    removeFromRequestList(&BLEBrowseListHead, domain, type);

    if (lastReference)
        updateBeacon();

    // If there are no active browse or registration requests, BLE scanning will be disabled.
    // Clear the list of responses received to remove any stale response state.
    if (BLEBrowseListHead == NULL && BLERegistrationListHead == 0)
        clearResponseLists();

    return lastReference;
}

extern void internal_start_browsing_for_service(mDNSInterfaceID InterfaceID, const domainname *const typeDomain, DNS_TypeValues qtype, DNSServiceFlags flags, DNSQuestion * q);
extern void internal_stop_browsing_for_service(mDNSInterfaceID InterfaceID, const domainname *const typeDomain, DNS_TypeValues qtype, DNSServiceFlags flags);

extern void internal_start_advertising_service(const ResourceRecord *const resourceRecord, DNSServiceFlags flags);
extern void internal_stop_advertising_service(const ResourceRecord *const resourceRecord, DNSServiceFlags flags);

void start_BLE_advertise(ServiceRecordSet * serviceRecordSet, const domainname *const domain, DNS_TypeValues type, DNSServiceFlags flags)
{
    requestList_t * ptr; 
    const domainname * instanceRemoved;

    if (!EnableBLEBasedDiscovery)
    {
        LogMsg("start_BLE_advertise: EnableBLEBasedDiscovery disabled");
        return;
    }

    // Just process the SRV record for each service registration.  The PTR
    // record already has the service type at the beginning of the domain, but 
    // we want to filter out reverse address PTR records at this point in time, so using
    // the SRV record instead.
    if (type != kDNSServiceType_SRV)
        return;

    if (serviceRecordSet == NULL)
    {
        LogInfo("start_BLE_advertise: NULL service record set for: %##s %s, returning", domain->c, DNSTypeName(type));
        return;
    }
    LogInfo("start_BLE_advertise: Starting BLE advertisement for: %##s %s", domain->c, DNSTypeName(type));

    instanceRemoved = SkipLeadingLabels(domain, 1);

    ptr = addToRequestList(&BLERegistrationListHead, instanceRemoved, type, flags);

    // If equivalent BLE registration is already running, just return.
    if (ptr->refCount > 1)
    {
        LogInfo("start_BLE_advertise: Dup of existing BLE advertisement.");
        return;
    }

    ptr->registeredHash = BLEServiceHash(instanceRemoved);
    if (ptr->registeredHash == 0)
    {
        LogInfo("BLEServiceHash failed!");
        removeFromRequestList(&BLERegistrationListHead, instanceRemoved, type);
        return;
    }
    ptr->serviceRecordSet = serviceRecordSet;
    // Extract the interface ID for easier access in the requestList_t structure
    ptr->InterfaceID = serviceRecordSet->RR_SRV.resrec.InterfaceID;

    addServiceToBeacon(0, ptr->registeredHash);
}

void stop_BLE_advertise(const domainname *const domain, DNS_TypeValues type, DNSServiceFlags flags)
{
    (void)  flags;   // not used initially
    requestList_t       * ptr;
    bool                lastReference = false;
    const domainname    * instanceRemoved;

    if (!EnableBLEBasedDiscovery)
    {
        LogMsg("stop_BLE_advertise: EnableBLEBasedDiscovery disabled");
        return;
    }

    // Just process the SRV record for each service registration.  The PTR
    // record already has the service type at the beginning of the domain, but 
    // we want to filter out reverse address PTR records at this point in time, so using
    // the SRV record instead.
    if (type != kDNSServiceType_SRV)
        return;

    LogInfo("stop_BLE_advertise: Stopping BLE advertisement for: %##s %s", domain->c, DNSTypeName(type));

    instanceRemoved = SkipLeadingLabels(domain, 1);

    // Get the request pointer from the indirect pointer returned.
    ptr =  *(findInRequestList(&BLERegistrationListHead, instanceRemoved, type));

    if (ptr == 0)
    {
        LogInfo("stop_BLE_advertise: No matching advertisement found.");
        return;
    }
    
    // If this is the last reference for this registration, update advertising and browsing bits set in
    // the beacon before removing this registration from the request list.
    if (ptr->refCount == 1)
    {
        lastReference = true;

        if (isAutoTriggerRequest(ptr) && ptr->triggeredOnAWDL)
        {
            // And remove the corresponding advertisements from the AWDL D2D plugin.
            // Do it directly here, since we do not set the kDNSServiceFlagsIncludeAWDL bit in the original client request structure
            // when we trigger the registration over AWDL, we just update the record ARType field, so our caller, external_stop_browsing_for_service()
            // would not call into the D2D plugin to remove the advertisements in this case.
            internal_stop_advertising_service(& ptr->serviceRecordSet->RR_PTR.resrec, (ptr->flags | kDNSServiceFlagsIncludeAWDL));
            internal_stop_advertising_service(& ptr->serviceRecordSet->RR_SRV.resrec, (ptr->flags | kDNSServiceFlagsIncludeAWDL));
            internal_stop_advertising_service(& ptr->serviceRecordSet->RR_TXT.resrec, (ptr->flags | kDNSServiceFlagsIncludeAWDL));
        }
    }
    removeFromRequestList(&BLERegistrationListHead, instanceRemoved, type);

    if (lastReference)
        updateBeacon();

    // If there are no active browse or registration requests, BLE scanning will be disabled.
    // Clear the list of responses received to remove any stale response state.
    if (BLEBrowseListHead == NULL && BLERegistrationListHead == 0)
        clearResponseLists();
}

#pragma mark - Response Handling

// Structure used to track the beacons received from various peers.
typedef struct responseList
{
    struct responseList * next;
    serviceHash_t       browseHash;
    serviceHash_t       registeredHash;
    mDNSEthAddr         senderMAC;
} responseList_t;

#define RESPONSE_LIST_NUMBER 8
static responseList_t* BLEResponseListHeads[RESPONSE_LIST_NUMBER];

mDNSlocal responseList_t ** findInResponseList(mDNSEthAddr * ptrToMAC)
{
    // Use the least significant byte of the MAC address as our hash index to find the list.
    responseList_t **ptr = & BLEResponseListHeads[ptrToMAC->b[5] % RESPONSE_LIST_NUMBER];

    for ( ; *ptr; ptr = &(*ptr)->next)
    {
        if (memcmp(&(*ptr)->senderMAC, ptrToMAC, sizeof(mDNSEthAddr)) == 0)
            break;
    }

    return ptr;
}


mDNSlocal responseList_t ** addToResponseList(serviceHash_t browseHash, serviceHash_t registeredHash, mDNSEthAddr * ptrToMAC)
{
    responseList_t **ptr = findInResponseList(ptrToMAC);

    if (!*ptr)
    {
        *ptr = mDNSPlatformMemAllocate(sizeof(**ptr));
        mDNSPlatformMemZero(*ptr, sizeof(**ptr));
        (*ptr)->browseHash = browseHash;
        (*ptr)->registeredHash = registeredHash;
        memcpy(& (*ptr)->senderMAC, ptrToMAC, sizeof(mDNSEthAddr));
    }

    return ptr;
}

mDNSlocal void removeFromResponseList(mDNSEthAddr * ptrToMAC)
{
    responseList_t **ptr = findInResponseList(ptrToMAC);

    if (!*ptr)
    {
        LogMsg("removeFromResponseList: did not find entry for MAC = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
                ptrToMAC->b[0], ptrToMAC->b[1], ptrToMAC->b[2], ptrToMAC->b[3], ptrToMAC->b[4], ptrToMAC->b[5]);
        return;
    }

    LogInfo("removeFromResponseList: removing entry for MAC = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
                ptrToMAC->b[0], ptrToMAC->b[1], ptrToMAC->b[2], ptrToMAC->b[3], ptrToMAC->b[4], ptrToMAC->b[5]);

    responseList_t *tmp = *ptr;
    *ptr = (*ptr)->next;
    mDNSPlatformMemFree(tmp);
}

// Free all current entries on the BLE response lists, removing all pointers
// to freed structures from the lists.
mDNSlocal void clearResponseLists()
{
    responseList_t **ptr;

    for (unsigned int i = 0; i < RESPONSE_LIST_NUMBER; i++)
    {
        ptr = & BLEResponseListHeads[i];
        while (*ptr)
        {
            responseList_t * tmp;
    
            tmp = *ptr;
            *ptr = (*ptr)->next;
            mDNSPlatformMemFree(tmp);
        }
    }
}

// Called from mDNS_Execute() when NextBLEServiceTime is reached
// to stop the BLE beacon a few seconds after the last request has
// been stopped.  This gives peers a chance to see that this device
// is no longer browsing for or advertising any services via the
// BLE beacon.  
void serviceBLE(void)
{
    mDNSStorage.NextBLEServiceTime = 0;
    if (BLEBrowseListHead || BLERegistrationListHead)
    {
        // We don't expect to be called if there are active requests.
        LogInfo("serviceBLE: called with active BLE requests ??");
        return;
    }
    stopBLEBeacon();
}

// Called from start_BLE_browse() on the mDNSResonder kqueue thread
mDNSlocal void checkForMatchingResponses(requestList_t *bp)
{
    responseList_t *ptr;

    for (unsigned int i = 0; i < RESPONSE_LIST_NUMBER; i++)
    {
        for (ptr = BLEResponseListHeads[i]; ptr; ptr = ptr->next)
        {
            if ((bp->browseHash & ptr->registeredHash) == bp->browseHash)
            {
                // Clear the registered services hash for the response.
                // The next beacon from this peer will update the hash and our 
                // newly started browse will get an add event if there is a match.
                ptr->registeredHash = 0;
            }
        }
    }
}

// Define a fixed name to use for the instance name denoting that one or more instances
// of a service are being advetised by peers in their BLE beacons.
// Name format is: length byte + bytes of name string + two byte pointer to the PTR record name.
// See compression_lhs definition in the D2D plugin code for backgound on 0xc027 DNS name compression pointer value.
static Byte  *BLEinstanceValue = (Byte *) "\x11ThresholdInstance\xc0\x27";
#define BLEValueSize  strlen((const char *)BLEinstanceValue)

void xD2DAddToCache(mDNS *const m, D2DStatus result, D2DServiceInstance instanceHandle, D2DTransportType transportType, const Byte *key, size_t keySize, const Byte *value, size_t valueSize);
void xD2DRemoveFromCache(mDNS *const m, D2DStatus result, D2DServiceInstance instanceHandle, D2DTransportType transportType, const Byte *key, size_t keySize, const Byte *value, size_t valueSize);

// Find each unique browse that matches the registered service hash in the BLE response.
// Called on the CFRunLoop thread while handling a callback from CoreBluetooth.
// Caller should hold  KQueueLock().
mDNSlocal void findMatchingBrowse(responseList_t *response)
{
    requestList_t *ptr;

    ptr = BLEBrowseListHead;
    for ( ; ptr; ptr = ptr->next)
    {
        if ((ptr->browseHash & response->registeredHash) == ptr->browseHash) 
        {

            LogInfo("findMatchingBrowse: Registration in response matched browse for: %##s", ptr->name.c);

            if (inResponseListForRequest(ptr, response))
            {
                LogInfo("findMatchingBrowse: Already on response list for browse: %##s", ptr->name.c);

                continue;
            }
            else
            {
                LogInfo("findMatchingBrowse: Adding to response list for browse: %##s", ptr->name.c);

                if (ptr->ourResponses == 0)
                {
                    if (isAutoTriggerRequest(ptr))
                    {
	                    LogInfo("findMatchingBrowse: First BLE response, triggering browse for %##s on AWDL", ptr->name.c);
	                    ptr->question->flags |= kDNSServiceFlagsIncludeAWDL;
	                    mDNSCoreRestartQuestion(& mDNSStorage, ptr->question);
	                    // register with the AWDL D2D plugin, 
	                    internal_start_browsing_for_service(ptr->question->InterfaceID, & ptr->name, ptr->type, ptr->question->flags, ptr->question);
                    }

	                // Browse on mDNSInterface_BLE is used to determine if there are one or more instances of the
	                // service type discoveryed over BLE.  If this is the first instance, add the psuedo instance defined by BLEinstanceValue.
	                if (ptr->question->InterfaceID == mDNSInterface_BLE)
	                {
	                    xD2DAddToCache(& mDNSStorage, kD2DSuccess, 0, D2DBLETransport, ptr->key, ptr->keySize, BLEinstanceValue, BLEValueSize);
	                }
                }
                addToResponseListForRequest(ptr, response);
            }
        }
        else
        {
            // If a previous response from this peer had matched the browse, remove that response from the
            // list now.  If this is the last matching response, remove the corresponding key from the AWDL D2D plugin
            if (removeFromResponseListForRequest(ptr, response) && (ptr->ourResponses == 0))
            {
                if (ptr->question->InterfaceID == mDNSInterface_BLE)
                {
                    xD2DRemoveFromCache(& mDNSStorage, kD2DSuccess, 0, D2DBLETransport, ptr->key, ptr->keySize, BLEinstanceValue, BLEValueSize);
                }

                if (isAutoTriggerRequest(ptr))
                {
                    LogInfo("findMatchingBrowse: Last BLE response, disabling browse for %##s on AWDL", ptr->name.c);
                    internal_stop_browsing_for_service(ptr->question->InterfaceID, & ptr->name, ptr->type, ptr->question->flags);
                }
            }
        }
    }
}

// Find each local registration that matches the service browse hash in the BLE response.
// Called on the CFRunLoop thread while handling a callback from CoreBluetooth.
// Caller should hold  KQueueLock().
mDNSlocal void findMatchingRegistration(responseList_t *response)
{
    requestList_t *ptr;

    ptr = BLERegistrationListHead;
    for ( ; ptr; ptr = ptr->next)
    {
        if ((ptr->registeredHash & response->browseHash) == ptr->registeredHash) 
        {

            LogInfo("findMatchingRegistration: Incoming browse matched registration for: %##s", ptr->name.c);

            if (inResponseListForRequest(ptr, response))
            {
                LogInfo("findMatchingRegistration: Already on response list for registration: %##s", ptr->name.c);

                continue;
            }
            else
            {
                LogInfo("findMatchingRegistration: Adding to response list for registration: %##s", ptr->name.c);

                // Also pass the registration to the AWDL D2D plugin if this is the first matching peer browse for
                // an auto triggered local registration.
                if ((ptr->ourResponses == 0) && isAutoTriggerRequest(ptr))
                {
                    AuthRecType newARType;

                    LogInfo("findMatchingRegistration: First BLE response, triggering registration for %##s on AWDL", ptr->name.c);
                    if (ptr->serviceRecordSet == 0)
                    {
                        LogInfo("findMatchingRegistration: serviceRecordSet pointer is NULL ??");
                        continue;
                    }
                    // Modify the PTR, TXT, and SRV records so that they now apply to AWDL and restart the registration.
                    // RR_ADV is not passed to the D2D plugins froma internal_start_advertising_helper(), so we don't do it here either.

                    if (ptr->flags & kDNSServiceFlagsIncludeAWDL)
                    {
                        LogInfo("findMatchingRegistration: registration for %##s already applies to AWDL, skipping", ptr->name.c);
                        continue;
                    }

                    // Save the current ARType value to restore when the promotion to use AWDL is stopped.
                    ptr->savedARType = ptr->serviceRecordSet->RR_PTR.ARType;

                    // Preserve P2P attribute if original registration was applied to P2P.
                    if (ptr->serviceRecordSet->RR_PTR.ARType == AuthRecordAnyIncludeP2P)
                        newARType = AuthRecordAnyIncludeAWDLandP2P;
                    else
                        newARType = AuthRecordAnyIncludeAWDL;

                    ptr->serviceRecordSet->RR_PTR.ARType = newARType;
                    ptr->serviceRecordSet->RR_SRV.ARType = newARType;
                    ptr->serviceRecordSet->RR_TXT.ARType = newARType;
                    mDNSCoreRestartRegistration(& mDNSStorage, & ptr->serviceRecordSet->RR_PTR, -1);
                    mDNSCoreRestartRegistration(& mDNSStorage, & ptr->serviceRecordSet->RR_SRV, -1);
                    mDNSCoreRestartRegistration(& mDNSStorage, & ptr->serviceRecordSet->RR_TXT, -1);

                    internal_start_advertising_service(& ptr->serviceRecordSet->RR_PTR.resrec, (ptr->flags | kDNSServiceFlagsIncludeAWDL));
                    internal_start_advertising_service(& ptr->serviceRecordSet->RR_SRV.resrec, (ptr->flags | kDNSServiceFlagsIncludeAWDL));
                    internal_start_advertising_service(& ptr->serviceRecordSet->RR_TXT.resrec, (ptr->flags | kDNSServiceFlagsIncludeAWDL));
                    // indicate the registration has been applied to the AWDL interface
                    ptr->triggeredOnAWDL = true;
                }

                addToResponseListForRequest(ptr, response);
            }
        }
        else
        {
            // If a previous response from this peer had matched the browse, remove that response from the
            // list now.  If this is the last matching response for a local auto triggered registration, 
            // remove the advertised key/value pairs from the AWDL D2D plugin.
            if (removeFromResponseListForRequest(ptr, response) && (ptr->ourResponses == 0) && isAutoTriggerRequest(ptr))
            {
                LogInfo("findMatchingRegistration: Last BLE response, disabling registration for %##s on AWDL", ptr->name.c);

                // Restore the saved ARType and call into the AWDL D2D plugin to stop the corresponding record advertisements over AWDL.
                ptr->serviceRecordSet->RR_PTR.ARType = ptr->savedARType;
                ptr->serviceRecordSet->RR_SRV.ARType = ptr->savedARType;
                ptr->serviceRecordSet->RR_TXT.ARType = ptr->savedARType;
                internal_stop_advertising_service(& ptr->serviceRecordSet->RR_PTR.resrec, (ptr->flags | kDNSServiceFlagsIncludeAWDL));
                internal_stop_advertising_service(& ptr->serviceRecordSet->RR_SRV.resrec, (ptr->flags | kDNSServiceFlagsIncludeAWDL));
                internal_stop_advertising_service(& ptr->serviceRecordSet->RR_TXT.resrec, (ptr->flags | kDNSServiceFlagsIncludeAWDL));
            }
        }
    }
}

// Called on CFRunLoop thread during CoreBluetooth beacon response processing.
// Thus, must call KQueueLock() prior to calling any core mDNSResponder routines to register records, etc.
void responseReceived(serviceHash_t browseHash, serviceHash_t registeredHash, mDNSEthAddr * ptrToMAC)
{
    responseList_t ** ptr;

    KQueueLock(& mDNSStorage);
    ptr = findInResponseList(ptrToMAC);
    if (*ptr == 0)
    {
        // Only add to list if peer is actively browsing or advertising.
        if (browseHash || registeredHash)
        {
            LogInfo("responseReceived: First beacon of this type, adding to list");
            LogInfo("responseReceived: browseHash = 0x%x, registeredHash = 0x%x",
                    browseHash, registeredHash);
            LogInfo("responseReceived: sender MAC = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
                    ptrToMAC->b[0], ptrToMAC->b[1], ptrToMAC->b[2], ptrToMAC->b[3], ptrToMAC->b[4], ptrToMAC->b[5]);

            ptr = addToResponseList(browseHash, registeredHash, ptrToMAC);       
            // See if we are browsing for any of the peers advertised services.
            findMatchingBrowse(*ptr);
            // See if we have a registration that matches the peer's browse.
            findMatchingRegistration(*ptr);
        }
    }
    else    // have entry from this MAC in the list
    {
        if (((*ptr)->browseHash == browseHash) && ((*ptr)->registeredHash == registeredHash))
        {
            // A duplicate of a current entry.
#if VERBOSE_BLE_DEBUG
            LogInfo("responseReceived: Duplicate of previous beacon, ignoring");
            LogInfo("responseReceived: sender MAC = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
                    ptrToMAC->b[0], ptrToMAC->b[1], ptrToMAC->b[2], ptrToMAC->b[3], ptrToMAC->b[4], ptrToMAC->b[5]);
#endif // VERBOSE_BLE_DEBUG
        }
        else
        {
            LogInfo("responseReceived: Update of previous beacon");
            LogInfo("responseReceived: browseHash = 0x%x, registeredHash = 0x%x",
                    browseHash, registeredHash);
            LogInfo("responseReceived: sender MAC = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
                    ptrToMAC->b[0], ptrToMAC->b[1], ptrToMAC->b[2], ptrToMAC->b[3], ptrToMAC->b[4], ptrToMAC->b[5]);

            (*ptr)->browseHash = browseHash;
            (*ptr)->registeredHash = registeredHash;

            findMatchingBrowse(*ptr);
            findMatchingRegistration(*ptr);
        }

        // If peer is no longer browsing or advertising, remove from list.
        if ((browseHash == 0) && (registeredHash == 0))
        {
            LogInfo("responseReceived: Removing peer entry from the list");

            removeFromResponseList(ptrToMAC);
        }
    }

    KQueueUnlock(& mDNSStorage, "BLE responseReceived");
}
