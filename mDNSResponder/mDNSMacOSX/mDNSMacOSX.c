/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2002-2016 Apple Inc. All rights reserved.
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

// ***************************************************************************
// mDNSMacOSX.c:
// Supporting routines to run mDNS on a CFRunLoop platform
// ***************************************************************************

// For debugging, set LIST_ALL_INTERFACES to 1 to display all found interfaces,
// including ones that mDNSResponder chooses not to use.
#define LIST_ALL_INTERFACES 0

#include "mDNSEmbeddedAPI.h"        // Defines the interface provided to the client layer above
#include "DNSCommon.h"
#include "uDNS.h"
#include "mDNSMacOSX.h"             // Defines the specific types needed to run mDNS on this platform
#include "dns_sd.h"                 // For mDNSInterface_LocalOnly etc.
#include "dns_sd_private.h"
#include "PlatformCommon.h"
#include "uds_daemon.h"
#include "CryptoSupport.h"

#include <stdio.h>
#include <stdarg.h>                 // For va_list support
#include <stdlib.h>                 // For arc4random
#include <net/if.h>
#include <net/if_types.h>           // For IFT_ETHER
#include <net/if_dl.h>
#include <net/bpf.h>                // For BIOCSETIF etc.
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/event.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>                   // platform support for UTC time
#include <arpa/inet.h>              // for inet_aton
#include <pthread.h>
#include <netdb.h>                  // for getaddrinfo
#include <sys/sockio.h>             // for SIOCGIFEFLAGS
#include <notify.h>
#include <netinet/in.h>             // For IP_RECVTTL
#ifndef IP_RECVTTL
#define IP_RECVTTL 24               // bool; receive reception TTL w/dgram
#endif

#include <netinet/in_systm.h>       // For n_long, required by <netinet/ip.h> below
#include <netinet/ip.h>             // For IPTOS_LOWDELAY etc.
#include <netinet6/in6_var.h>       // For IN6_IFF_TENTATIVE etc.

#include <netinet/tcp.h>

#include <DebugServices.h>
#include "dnsinfo.h"

#include <ifaddrs.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>

#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPowerSourcesPrivate.h>
#include <IOKit/ps/IOPSKeys.h>

#include <mach/mach_error.h>
#include <mach/mach_port.h>
#include <mach/mach_time.h>
#include "helper.h"
#include "P2PPacketFilter.h"

#include <asl.h>
#include <SystemConfiguration/SCPrivate.h>

#if TARGET_OS_IPHONE
// For WiFiManagerClientRef etc, declarations.
#include <MobileGestalt.h>
#include <MobileWiFi/WiFiManagerClient.h>
#include <dlfcn.h>
#endif // TARGET_OS_IPHONE

// Include definition of opaque_presence_indication for KEV_DL_NODE_PRESENCE handling logic.
#include <Kernel/IOKit/apple80211/apple80211_var.h>

#if APPLE_OSX_mDNSResponder
#include <DeviceToDeviceManager/DeviceToDeviceManager.h>
#include <AWACS.h>
#include <ne_session.h> // for ne_session_set_socket_attributes()
#if !NO_D2D
#include "BLE.h"

D2DStatus D2DInitialize(CFRunLoopRef runLoop, D2DServiceCallback serviceCallback, void* userData) __attribute__((weak_import));
D2DStatus D2DRetain(D2DServiceInstance instanceHandle, D2DTransportType transportType) __attribute__((weak_import));
D2DStatus D2DStopAdvertisingPairOnTransport(const Byte *key, const size_t keySize, const Byte *value, const size_t valueSize, D2DTransportType transport) __attribute__((weak_import));
D2DStatus D2DRelease(D2DServiceInstance instanceHandle, D2DTransportType transportType) __attribute__((weak_import));
D2DStatus D2DStartAdvertisingPairOnTransport(const Byte *key, const size_t keySize, const Byte *value, const size_t valueSize, D2DTransportType transport) __attribute__((weak_import));
D2DStatus D2DStartBrowsingForKeyOnTransport(const Byte *key, const size_t keySize, D2DTransportType transport) __attribute__((weak_import));
D2DStatus D2DStopBrowsingForKeyOnTransport(const Byte *key, const size_t keySize, D2DTransportType transport) __attribute__((weak_import));
void D2DStartResolvingPairOnTransport(const Byte *key, const size_t keySize, const Byte *value, const size_t valueSize, D2DTransportType transport) __attribute__((weak_import));
void D2DStopResolvingPairOnTransport(const Byte *key, const size_t keySize, const Byte *value, const size_t valueSize, D2DTransportType transport) __attribute__((weak_import));
D2DStatus D2DTerminate() __attribute__((weak_import));

void xD2DAddToCache(mDNS *const m, D2DStatus result, D2DServiceInstance instanceHandle, D2DTransportType transportType, const Byte *key, size_t keySize, const Byte *value, size_t valueSize);
void xD2DRemoveFromCache(mDNS *const m, D2DStatus result, D2DServiceInstance instanceHandle, D2DTransportType transportType, const Byte *key, size_t keySize, const Byte *value, size_t valueSize);

#endif // ! NO_D2D

#else
#define NO_D2D 1
#define NO_AWACS 1
#endif // APPLE_OSX_mDNSResponder

#if APPLE_OSX_mDNSResponder && !TARGET_OS_EMBEDDED
#include <IOKit/platform/IOPlatformSupportPrivate.h>
#endif // APPLE_OSX_mDNSResponder && !TARGET_OS_EMBEDDED

#define kInterfaceSpecificOption "interface="

#define mDNS_IOREG_KEY               "mDNS_KEY"
#define mDNS_IOREG_VALUE             "2009-07-30"
#define mDNS_IOREG_KA_KEY            "mDNS_Keepalive"
#define mDNS_USER_CLIENT_CREATE_TYPE 'mDNS'

#define DARK_WAKE_TIME 16 // Time we hold an idle sleep assertion for maintenance after a wake notification

// cache the InterfaceID of the AWDL interface 
mDNSInterfaceID AWDLInterfaceID;

// ***************************************************************************
// Globals

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - Globals
#endif

// By default we don't offer sleep proxy service
// If OfferSleepProxyService is set non-zero (typically via command-line switch),
// then we'll offer sleep proxy service on desktop Macs that are set to never sleep.
// We currently do not offer sleep proxy service on laptops, or on machines that are set to go to sleep.
mDNSexport int OfferSleepProxyService = 0;
mDNSexport int DisableSleepProxyClient = 0;
mDNSexport int UseInternalSleepProxy = 1;       // Set to non-zero to use internal (in-NIC) Sleep Proxy

mDNSexport int OSXVers, iOSVers;
mDNSexport int KQueueFD;

#ifndef NO_SECURITYFRAMEWORK
static CFArrayRef ServerCerts;
OSStatus SSLSetAllowAnonymousCiphers(SSLContextRef context, Boolean enable);
#endif /* NO_SECURITYFRAMEWORK */

static CFStringRef NetworkChangedKey_IPv4;
static CFStringRef NetworkChangedKey_IPv6;
static CFStringRef NetworkChangedKey_Hostnames;
static CFStringRef NetworkChangedKey_Computername;
static CFStringRef NetworkChangedKey_DNS;
static CFStringRef NetworkChangedKey_StateInterfacePrefix;
static CFStringRef NetworkChangedKey_DynamicDNS       = CFSTR("Setup:/Network/DynamicDNS");
static CFStringRef NetworkChangedKey_BackToMyMac      = CFSTR("Setup:/Network/BackToMyMac");
static CFStringRef NetworkChangedKey_BTMMConnectivity = CFSTR("State:/Network/Connectivity");
static CFStringRef NetworkChangedKey_PowerSettings    = CFSTR("State:/IOKit/PowerManagement/CurrentSettings");

static char HINFO_HWstring_buffer[32];
static char *HINFO_HWstring = "Device";
static int HINFO_HWstring_prefixlen = 6;

mDNSexport int WatchDogReportingThreshold = 250;

dispatch_queue_t SSLqueue;

#if TARGET_OS_EMBEDDED
#define kmDNSResponderManagedPrefsID CFSTR("/Library/Managed Preferences/mobile/com.apple.mDNSResponder.plist")
#endif

#if APPLE_OSX_mDNSResponder
static mDNSu8 SPMetricPortability   = 99;
static mDNSu8 SPMetricMarginalPower = 99;
static mDNSu8 SPMetricTotalPower    = 99;
static mDNSu8 SPMetricFeatures      = 1; /* The current version supports TCP Keep Alive Feature */
mDNSexport domainname ActiveDirectoryPrimaryDomain;
mDNSexport int ActiveDirectoryPrimaryDomainLabelCount;
mDNSexport mDNSAddr ActiveDirectoryPrimaryDomainServer;
#endif // APPLE_OSX_mDNSResponder

// Don't send triggers too often. We arbitrarily limit it to three minutes.
#define DNS_TRIGGER_INTERVAL (180 * mDNSPlatformOneSecond)

// Used by AutoTunnel
const char btmmprefix[] = "btmmdns:";
const char dnsprefix[] = "dns:";

// String Array used to write list of private domains to Dynamic Store
static CFArrayRef privateDnsArray = NULL;

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - D2D Support
#endif

#if !NO_D2D

mDNSexport void D2D_start_advertising_interface(NetworkInterfaceInfo *interface)
{
    // AWDL wants the address and reverse address PTR record communicated
    // via the D2D interface layer.
    if (interface->InterfaceID == AWDLInterfaceID)
    {
        // only log if we have a valid record to start advertising
        if (interface->RR_A.resrec.RecordType || interface->RR_PTR.resrec.RecordType)
            LogInfo("D2D_start_advertising_interface: %s", interface->ifname);

        if (interface->RR_A.resrec.RecordType)
            external_start_advertising_service(&interface->RR_A.resrec, 0);
        if (interface->RR_PTR.resrec.RecordType)
            external_start_advertising_service(&interface->RR_PTR.resrec, 0);
    }
}

mDNSexport void D2D_stop_advertising_interface(NetworkInterfaceInfo *interface)
{
    if (interface->InterfaceID == AWDLInterfaceID)
    {
        // only log if we have a valid record to stop advertising
        if (interface->RR_A.resrec.RecordType || interface->RR_PTR.resrec.RecordType)
            LogInfo("D2D_stop_advertising_interface: %s", interface->ifname);

        if (interface->RR_A.resrec.RecordType)
            external_stop_advertising_service(&interface->RR_A.resrec, 0);
        if (interface->RR_PTR.resrec.RecordType)
            external_stop_advertising_service(&interface->RR_PTR.resrec, 0);
    }
}

// If record would have been advertised to the D2D plugin layer, stop that advertisement.
mDNSexport void D2D_stop_advertising_record(AuthRecord *ar)
{
    DNSServiceFlags flags = deriveD2DFlagsFromAuthRecType(ar->ARType);
    if (callExternalHelpers(ar->resrec.InterfaceID, ar->resrec.name, flags))
    {
        external_stop_advertising_service(&ar->resrec, flags);
    }
}

// If record should be advertised to the D2D plugin layer, start that advertisement.
mDNSexport void D2D_start_advertising_record(AuthRecord *ar)
{
    DNSServiceFlags flags = deriveD2DFlagsFromAuthRecType(ar->ARType);
    if (callExternalHelpers(ar->resrec.InterfaceID, ar->resrec.name, flags))
    {
        external_start_advertising_service(&ar->resrec, flags);
    }
}

// Name compression items for fake packet version number 1
static const mDNSu8 compression_packet_v1 = 0x01;

static DNSMessage compression_base_msg = { { {{0}}, {{0}}, 2, 0, 0, 0 }, "\x04_tcp\x05local\x00\x00\x0C\x00\x01\x04_udp\xC0\x11\x00\x0C\x00\x01" };
static mDNSu8 *const compression_limit = (mDNSu8 *) &compression_base_msg + sizeof(DNSMessage);
static mDNSu8 *const compression_lhs = (mDNSu8 *const) compression_base_msg.data + 27;

mDNSlocal void FreeD2DARElemCallback(mDNS *const m, AuthRecord *const rr, mStatus result);
mDNSlocal void PrintHex(mDNSu8 *data, mDNSu16 len);

typedef struct D2DRecordListElem
{
    struct D2DRecordListElem *next;
    D2DServiceInstance       instanceHandle;
    D2DTransportType         transportType;
    AuthRecord               ar;    // must be last in the structure to accomodate extra space
                                    // allocated for large records.
} D2DRecordListElem;

static D2DRecordListElem *D2DRecords = NULL; // List of records returned with D2DServiceFound events

typedef struct D2DBrowseListElem
{
    struct D2DBrowseListElem *next;
    domainname name;
    mDNSu16 type;
    unsigned int refCount;
} D2DBrowseListElem;

D2DBrowseListElem* D2DBrowseList = NULL;

mDNSlocal mDNSu8 *putVal16(mDNSu8 *ptr, mDNSu16 val)
{
    ptr[0] = (mDNSu8)((val >> 8 ) & 0xFF);
    ptr[1] = (mDNSu8)((val      ) & 0xFF);
    return ptr + sizeof(mDNSu16);
}

mDNSlocal mDNSu8 *putVal32(mDNSu8 *ptr, mDNSu32 val)
{
    ptr[0] = (mDNSu8)((val >> 24) & 0xFF);
    ptr[1] = (mDNSu8)((val >> 16) & 0xFF);
    ptr[2] = (mDNSu8)((val >>  8) & 0xFF);
    ptr[3] = (mDNSu8)((val      ) & 0xFF);
    return ptr + sizeof(mDNSu32);
}

mDNSlocal void DomainnameToLower(const domainname * const in, domainname * const out)
{
    const mDNSu8 * const start = (const mDNSu8 * const)in;
    mDNSu8 *ptr = (mDNSu8*)start;
    while(*ptr)
    {
        mDNSu8 c = *ptr;
        out->c[ptr-start] = *ptr;
        ptr++;
        for (; c; c--,ptr++) out->c[ptr-start] = mDNSIsUpperCase(*ptr) ? (*ptr - 'A' + 'a') : *ptr;
    }
    out->c[ptr-start] = *ptr;
}

mDNSlocal mDNSu8 * DNSNameCompressionBuildLHS(const domainname* typeDomain, DNS_TypeValues qtype)
{
    mDNSu8 *ptr = putDomainNameAsLabels(&compression_base_msg, compression_lhs, compression_limit, typeDomain);
    if (!ptr) return ptr;
    *ptr = (qtype >> 8) & 0xff;
    ptr += 1;
    *ptr = qtype & 0xff;
    ptr += 1;
    *ptr = compression_packet_v1;
    return ptr + 1;
}

mDNSlocal mDNSu8 * DNSNameCompressionBuildRHS(mDNSu8 *start, const ResourceRecord *const resourceRecord)
{
    return putRData(&compression_base_msg, start, compression_limit, resourceRecord);
}

#define PRINT_DEBUG_BYTES_LIMIT 64  // set limit on number of record bytes printed for debugging

mDNSlocal void PrintHex(mDNSu8 *data, mDNSu16 len)
{
    mDNSu8 *end;
    char buffer[49] = {0};
    char *bufend = buffer + sizeof(buffer);

    if (len > PRINT_DEBUG_BYTES_LIMIT)
    {
        LogInfo(" (limiting debug output to %d bytes)", PRINT_DEBUG_BYTES_LIMIT);
        len = PRINT_DEBUG_BYTES_LIMIT;
    }
    end = data + len;

    while(data < end)
    {
        char *ptr = buffer;
        for(; data < end && ptr < bufend-1; ptr+=3,data++)
            mDNS_snprintf(ptr, bufend - ptr, "%02X ", *data);
        LogInfo("    %s", buffer);
    }
}

mDNSlocal void PrintHelper(const char *const tag, mDNSu8 *lhs, mDNSu16 lhs_len, mDNSu8 *rhs, mDNSu16 rhs_len)
{
    if (!mDNS_LoggingEnabled) return;

    LogInfo("%s:", tag);
    LogInfo("  LHS: (%d bytes)", lhs_len);
    PrintHex(lhs, lhs_len);

    if (!rhs) return;

    LogInfo("  RHS: (%d bytes)", rhs_len);
    PrintHex(rhs, rhs_len);
}

mDNSlocal void FreeD2DARElemCallback(mDNS *const m, AuthRecord *const rr, mStatus result)
{
    (void)m;  // unused
    if (result == mStatus_MemFree)
    {
        D2DRecordListElem **ptr = &D2DRecords;
        D2DRecordListElem *tmp;
        while (*ptr && &(*ptr)->ar != rr) ptr = &(*ptr)->next;
        if (!*ptr) { LogMsg("FreeD2DARElemCallback: Could not find in D2DRecords: %s", ARDisplayString(m, rr)); return; }
        LogInfo("FreeD2DARElemCallback: Found in D2DRecords: %s", ARDisplayString(m, rr));
        tmp = *ptr;
        *ptr = (*ptr)->next;
        // Just because we stoppped browsing, doesn't mean we should tear down the PAN connection.
        mDNSPlatformMemFree(tmp);
    }
}

mDNSexport void external_connection_release(const domainname *instance)
{
    (void) instance;
    D2DRecordListElem *ptr = D2DRecords;

    for ( ; ptr ; ptr = ptr->next)
    {
        if ((ptr->ar.resrec.rrtype == kDNSServiceType_PTR) &&
             SameDomainName(&ptr->ar.rdatastorage.u.name, instance))
        {
            LogInfo("external_connection_release: Calling D2DRelease(instanceHandle = %p, transportType = %d", 
                ptr->instanceHandle,  ptr->transportType);
            if (D2DRelease) D2DRelease(ptr->instanceHandle, ptr->transportType);
        }
    }
}

mDNSlocal void xD2DClearCache(const domainname *regType, DNS_TypeValues qtype)
{
    D2DRecordListElem *ptr = D2DRecords;
    for ( ; ptr ; ptr = ptr->next)
    {
        if ((ptr->ar.resrec.rrtype == qtype) && SameDomainName(&ptr->ar.namestorage, regType))
        {
            LogInfo("xD2DClearCache: Clearing cache record and deregistering %s", ARDisplayString(&mDNSStorage, &ptr->ar));
            mDNS_Deregister(&mDNSStorage, &ptr->ar);
        }
    }
}

mDNSlocal D2DBrowseListElem ** D2DFindInBrowseList(const domainname *const name, mDNSu16 type)
{
    D2DBrowseListElem **ptr = &D2DBrowseList;

    for ( ; *ptr; ptr = &(*ptr)->next)
        if ((*ptr)->type == type && SameDomainName(&(*ptr)->name, name))
            break;

    return ptr;
}

mDNSlocal unsigned int D2DBrowseListRefCount(const domainname *const name, mDNSu16 type)
{
    D2DBrowseListElem **ptr = D2DFindInBrowseList(name, type);
    return *ptr ? (*ptr)->refCount : 0;
}

mDNSlocal void D2DBrowseListRetain(const domainname *const name, mDNSu16 type)
{
    D2DBrowseListElem **ptr = D2DFindInBrowseList(name, type);

    if (!*ptr)
    {
        *ptr = mDNSPlatformMemAllocate(sizeof(**ptr));
        mDNSPlatformMemZero(*ptr, sizeof(**ptr));
        (*ptr)->type = type;
        AssignDomainName(&(*ptr)->name, name);
    }
    (*ptr)->refCount += 1;

    LogInfo("D2DBrowseListRetain: %##s %s refcount now %u", (*ptr)->name.c, DNSTypeName((*ptr)->type), (*ptr)->refCount);
}

// Returns true if found in list, false otherwise
mDNSlocal bool D2DBrowseListRelease(const domainname *const name, mDNSu16 type)
{
    D2DBrowseListElem **ptr = D2DFindInBrowseList(name, type);

    if (!*ptr) { LogMsg("D2DBrowseListRelease: Didn't find %##s %s in list", name->c, DNSTypeName(type)); return false; }

    (*ptr)->refCount -= 1;

    LogInfo("D2DBrowseListRelease: %##s %s refcount now %u", (*ptr)->name.c, DNSTypeName((*ptr)->type), (*ptr)->refCount);

    if (!(*ptr)->refCount)
    {
        D2DBrowseListElem *tmp = *ptr;
        *ptr = (*ptr)->next;
        mDNSPlatformMemFree(tmp);
    }
    return true;
}

mDNSlocal mStatus xD2DParse(mDNS *const m, const mDNSu8 * const lhs, const mDNSu16 lhs_len, const mDNSu8 * const rhs, const mDNSu16 rhs_len, D2DRecordListElem **D2DListp)
{
    // Sanity check that key array (lhs) has one domain name, followed by the record type and single byte D2D 
    // plugin protocol version number.
    // Note, we don't have a DNSMessage pointer at this point, so just pass in the lhs value as the lower bound
    // of the input bytes we are processing.  skipDomainName() does not try to follow name compression pointers,
    // so it is safe to pass it the key byte array since it will stop parsing the DNS name and return a pointer
    // to the byte after the first name compression pointer it encounters.
    const mDNSu8 *keyp = skipDomainName((const DNSMessage *const) lhs, lhs, lhs + lhs_len);

    // There should be 3 bytes remaining in a valid key,
    // two for the DNS record type, and one for the D2D protocol version number.
    if (keyp == NULL || (keyp + 3 != (lhs + lhs_len)))
    {
        LogInfo("xD2DParse: Could not parse DNS name in key");
        return mStatus_Incompatible;
    }
    keyp += 2;   // point to D2D compression packet format version byte
    if (*keyp != compression_packet_v1)
    {
        LogInfo("xD2DParse: Invalid D2D packet version: %d", *keyp);
        return mStatus_Incompatible;
    }

    if (mDNS_LoggingEnabled)
    {
        LogInfo("%s", __func__);
        LogInfo("  Static Bytes: (%d bytes)", compression_lhs - (mDNSu8*)&compression_base_msg);
        PrintHex((mDNSu8*)&compression_base_msg, compression_lhs - (mDNSu8*)&compression_base_msg);
    }

    mDNSu8 *ptr = compression_lhs; // pointer to the end of our fake packet

    // Check to make sure we're not going to go past the end of the DNSMessage data
    // 7 = 2 for CLASS (-1 for our version) + 4 for TTL + 2 for RDLENGTH
    if (ptr + lhs_len - 7 + rhs_len >= compression_limit) return mStatus_NoMemoryErr;

    // Copy the LHS onto our fake wire packet
    mDNSPlatformMemCopy(ptr, lhs, lhs_len);
    ptr += lhs_len - 1;

    // Check the 'fake packet' version number, to ensure that we know how to decompress this data
    if (*ptr != compression_packet_v1) return mStatus_Incompatible;

    // two bytes of CLASS
    ptr = putVal16(ptr, kDNSClass_IN | kDNSClass_UniqueRRSet);

    // four bytes of TTL
    ptr = putVal32(ptr, 120);

    // Copy the RHS length into the RDLENGTH of our fake wire packet
    ptr = putVal16(ptr, rhs_len);

    // Copy the RHS onto our fake wire packet
    mDNSPlatformMemCopy(ptr, rhs, rhs_len);
    ptr += rhs_len;

    if (mDNS_LoggingEnabled)
    {
        LogInfo("  Our Bytes (%d bytes): ", ptr - compression_lhs);
        PrintHex(compression_lhs, ptr - compression_lhs);
    }

    ptr = (mDNSu8 *) GetLargeResourceRecord(m, &compression_base_msg, compression_lhs, ptr, mDNSInterface_Any, kDNSRecordTypePacketAns, &m->rec);
    if (!ptr || m->rec.r.resrec.RecordType == kDNSRecordTypePacketNegative)
    {
        LogMsg("xD2DParse: failed to get large RR");
        m->rec.r.resrec.RecordType = 0;
        return mStatus_UnknownErr;
    }
    else
    {
        LogInfo("xD2DParse: got rr: %s", CRDisplayString(m, &m->rec.r));
    }

    *D2DListp = mDNSPlatformMemAllocate(sizeof(D2DRecordListElem) + (m->rec.r.resrec.rdlength <= sizeof(RDataBody) ? 0 : m->rec.r.resrec.rdlength - sizeof(RDataBody)));
    if (!*D2DListp) return mStatus_NoMemoryErr;

    AuthRecord *rr = &(*D2DListp)->ar;
    mDNS_SetupResourceRecord(rr, mDNSNULL, mDNSInterface_P2P, m->rec.r.resrec.rrtype, 7200, kDNSRecordTypeShared, AuthRecordP2P, FreeD2DARElemCallback, NULL);
    AssignDomainName(&rr->namestorage, &m->rec.namestorage);
    rr->resrec.rdlength = m->rec.r.resrec.rdlength;
    rr->resrec.rdata->MaxRDLength = m->rec.r.resrec.rdlength;
    mDNSPlatformMemCopy(rr->resrec.rdata->u.data, m->rec.r.resrec.rdata->u.data, m->rec.r.resrec.rdlength);
    rr->resrec.namehash = DomainNameHashValue(rr->resrec.name);
    SetNewRData(&rr->resrec, mDNSNULL, 0);  // Sets rr->rdatahash for us

    m->rec.r.resrec.RecordType = 0; // Mark m->rec as no longer in use

    return mStatus_NoError;
}

mDNSexport void xD2DAddToCache(mDNS *const m, D2DStatus result, D2DServiceInstance instanceHandle, D2DTransportType transportType, const Byte *key, size_t keySize, const Byte *value, size_t valueSize)
{
    if (result == kD2DSuccess)
    {
        if ( key == NULL || value == NULL || keySize == 0 || valueSize == 0) { LogMsg("xD2DAddToCache: NULL Byte * passed in or length == 0"); return; }

        mStatus err;
        D2DRecordListElem *ptr = NULL;

        err = xD2DParse(m, (const mDNSu8 * const)key, (const mDNSu16)keySize, (const mDNSu8 * const)value, (const mDNSu16)valueSize, &ptr);
        if (err)
        {
            LogMsg("xD2DAddToCache: xD2DParse returned error: %d", err);
            PrintHelper(__func__, (mDNSu8 *)key, (mDNSu16)keySize, (mDNSu8 *)value, (mDNSu16)valueSize);
            if (ptr)
                mDNSPlatformMemFree(ptr);
            return;
        }

        // If the record was created based on a BLE beacon, update the interface index to indicate
        // this and thus match BLE specific queries.
        if (transportType == D2DBLETransport)
            ptr->ar.resrec.InterfaceID = mDNSInterface_BLE;

        err = mDNS_Register(m, &ptr->ar);
        if (err)
        {
            LogMsg("xD2DAddToCache: mDNS_Register returned error %d for %s", err, ARDisplayString(m, &ptr->ar));
            mDNSPlatformMemFree(ptr);
            return;
        }

        LogInfo("xD2DAddToCache: mDNS_Register succeeded for %s", ARDisplayString(m, &ptr->ar));
        ptr->instanceHandle = instanceHandle;
        ptr->transportType = transportType;
        ptr->next = D2DRecords;
        D2DRecords = ptr;
    }
    else
        LogMsg("xD2DAddToCache: Unexpected result %d", result);
}

mDNSlocal D2DRecordListElem * xD2DFindInList(mDNS *const m, const Byte *const key, const size_t keySize, const Byte *const value, const size_t valueSize)
{
    D2DRecordListElem *ptr = D2DRecords;
    D2DRecordListElem *arptr = NULL;

    if ( key == NULL || value == NULL || keySize == 0 || valueSize == 0) { LogMsg("xD2DFindInList: NULL Byte * passed in or length == 0"); return NULL; }

    mStatus err = xD2DParse(m, (const mDNSu8 *const)key, (const mDNSu16)keySize, (const mDNSu8 *const)value, (const mDNSu16)valueSize, &arptr);
    if (err)
    {
        LogMsg("xD2DFindInList: xD2DParse returned error: %d", err);
        PrintHelper(__func__, (mDNSu8 *)key, (mDNSu16)keySize, (mDNSu8 *)value, (mDNSu16)valueSize);
        if (arptr)
            mDNSPlatformMemFree(arptr);
        return NULL;
    }

    while (ptr)
    {
        if (IdenticalResourceRecord(&arptr->ar.resrec, &ptr->ar.resrec)) break;
        ptr = ptr->next;
    }

    if (!ptr) LogMsg("xD2DFindInList: Could not find in D2DRecords: %s", ARDisplayString(m, &arptr->ar));
    mDNSPlatformMemFree(arptr);
    return ptr;
}

mDNSexport void xD2DRemoveFromCache(mDNS *const m, D2DStatus result, D2DServiceInstance instanceHandle, D2DTransportType transportType, const Byte *key, size_t keySize, const Byte *value, size_t valueSize)
{
    (void)transportType; // We don't care about this, yet.
    (void)instanceHandle; // We don't care about this, yet.

    if (result == kD2DSuccess)
    {
        D2DRecordListElem *ptr = xD2DFindInList(m, key, keySize, value, valueSize);
        if (ptr)
        {
            LogInfo("xD2DRemoveFromCache: Remove from cache: %s", ARDisplayString(m, &ptr->ar));
            mDNS_Deregister(m, &ptr->ar);
        }
    }
    else
        LogMsg("xD2DRemoveFromCache: Unexpected result %d", result);
}

mDNSlocal void xD2DServiceResolved(mDNS *const m, D2DStatus result, D2DServiceInstance instanceHandle, D2DTransportType transportType, const Byte *key, size_t keySize, const Byte *value, size_t valueSize)
{
    (void)m;
    (void)key;
    (void)keySize;
    (void)value;
    (void)valueSize;

    if (result == kD2DSuccess)
    {
        LogInfo("xD2DServiceResolved: Starting up PAN connection for %p", instanceHandle);
        if (D2DRetain) D2DRetain(instanceHandle, transportType);
    }
    else LogMsg("xD2DServiceResolved: Unexpected result %d", result);
}

mDNSlocal void xD2DRetainHappened(mDNS *const m, D2DStatus result, D2DServiceInstance instanceHandle, D2DTransportType transportType, const Byte *key, size_t keySize, const Byte *value, size_t valueSize)
{
    (void)m;
    (void)instanceHandle;
    (void)transportType;
    (void)key;
    (void)keySize;
    (void)value;
    (void)valueSize;

    if (result == kD2DSuccess) LogInfo("xD2DRetainHappened: Opening up PAN connection for %p", instanceHandle);
    else LogMsg("xD2DRetainHappened: Unexpected result %d", result);
}

mDNSlocal void xD2DReleaseHappened(mDNS *const m, D2DStatus result, D2DServiceInstance instanceHandle, D2DTransportType transportType, const Byte *key, size_t keySize, const Byte *value, size_t valueSize)
{
    (void)m;
    (void)instanceHandle;
    (void)transportType;
    (void)key;
    (void)keySize;
    (void)value;
    (void)valueSize;

    if (result == kD2DSuccess) LogInfo("xD2DReleaseHappened: Closing PAN connection for %p", instanceHandle);
    else LogMsg("xD2DReleaseHappened: Unexpected result %d", result);
}

mDNSlocal void xD2DServiceCallback(D2DServiceEvent event, D2DStatus result, D2DServiceInstance instanceHandle, D2DTransportType transportType, const Byte *key, size_t keySize, const Byte *value, size_t valueSize, void *userData)
{
    mDNS *m = (mDNS *) userData;
    const char *eventString = "unknown";

    KQueueLock(m);

    if (keySize   > 0xFFFF) LogMsg("xD2DServiceCallback: keySize too large: %u", keySize);
    if (valueSize > 0xFFFF) LogMsg("xD2DServiceCallback: valueSize too large: %u", valueSize);

    switch (event)
    {
    case D2DServiceFound:
        eventString = "D2DServiceFound";
        break;
    case D2DServiceLost:
        eventString = "D2DServiceLost";
        break;
    case D2DServiceResolved:
        eventString = "D2DServiceResolved";
        break;
    case D2DServiceRetained:
        eventString = "D2DServiceRetained";
        break;
    case D2DServiceReleased:
        eventString = "D2DServiceReleased";
        break;
    default:
        break;
    }

    LogInfo("xD2DServiceCallback: event=%s result=%d instanceHandle=%p transportType=%d LHS=%p (%u) RHS=%p (%u) userData=%p", eventString, result, instanceHandle, transportType, key, keySize, value, valueSize, userData);
    PrintHelper(__func__, (mDNSu8 *)key, (mDNSu16)keySize, (mDNSu8 *)value, (mDNSu16)valueSize);

    switch (event)
    {
    case D2DServiceFound:
        xD2DAddToCache(m, result, instanceHandle, transportType, key, keySize, value, valueSize);
        break;
    case D2DServiceLost:
        xD2DRemoveFromCache(m, result, instanceHandle, transportType, key, keySize, value, valueSize);
        break;
    case D2DServiceResolved:
        xD2DServiceResolved(m, result, instanceHandle, transportType, key, keySize, value, valueSize);
        break;
    case D2DServiceRetained:
        xD2DRetainHappened(m, result, instanceHandle, transportType, key, keySize, value, valueSize);
        break;
    case D2DServiceReleased:
        xD2DReleaseHappened(m, result, instanceHandle, transportType, key, keySize, value, valueSize);
        break;
    default:
        break;
    }

    // Need to tickle the main kqueue loop to potentially handle records we removed or added.
    KQueueUnlock(m, "xD2DServiceCallback");
}

// Map interface index and flags to a specific D2D transport type or D2DTransportMax if all plugins 
// should be called.
// When D2DTransportMax is returned, if a specific transport should not be called, *excludedTransportType 
// will be set to the excluded transport value, otherwise, it will be set to D2DTransportMax.
// If the return value is not D2DTransportMax, excludedTransportType is undefined.

mDNSlocal D2DTransportType xD2DInterfaceToTransportType(mDNSInterfaceID InterfaceID, DNSServiceFlags flags, D2DTransportType * excludedTransportType)
{
    NetworkInterfaceInfoOSX *info;

    // Default exludes the D2DAWDLTransport when D2DTransportMax is returned.
    *excludedTransportType = D2DAWDLTransport;

    // Call all D2D plugins when both kDNSServiceFlagsIncludeP2P and kDNSServiceFlagsIncludeAWDL are set.
    if ((flags & kDNSServiceFlagsIncludeP2P) && (flags & kDNSServiceFlagsIncludeAWDL))
    {
        LogInfo("xD2DInterfaceToTransportType: returning D2DTransportMax (including AWDL) since both kDNSServiceFlagsIncludeP2P and kDNSServiceFlagsIncludeAWDL are set");
        *excludedTransportType = D2DTransportMax;
        return D2DTransportMax;
    } 
    // Call all D2D plugins (exlcluding AWDL) when only kDNSServiceFlagsIncludeP2P is set.
    else if (flags & kDNSServiceFlagsIncludeP2P)
    {
        LogInfo("xD2DInterfaceToTransportType: returning D2DTransportMax (excluding AWDL) since only kDNSServiceFlagsIncludeP2P is set");
        return D2DTransportMax;
    }
    // Call AWDL D2D plugin when only kDNSServiceFlagsIncludeAWDL is set.
    else if (flags & kDNSServiceFlagsIncludeAWDL)
    {
        LogInfo("xD2DInterfaceToTransportType: returning D2DAWDLTransport since only kDNSServiceFlagsIncludeAWDL is set");
        return D2DAWDLTransport;
    }

    if (InterfaceID == mDNSInterface_P2P)
    {
        LogInfo("xD2DInterfaceToTransportType: returning D2DTransportMax (excluding AWDL) for interface index mDNSInterface_P2P");
        return D2DTransportMax; 
    }

    // Compare to cached AWDL interface ID.
    if (AWDLInterfaceID && (InterfaceID == AWDLInterfaceID))
    {
        LogInfo("xD2DInterfaceToTransportType: returning D2DAWDLTransport for interface index %d", InterfaceID);
        return D2DAWDLTransport;
    }

    info = IfindexToInterfaceInfoOSX(&mDNSStorage, InterfaceID);
    if (info == NULL)
    {
        LogInfo("xD2DInterfaceToTransportType: Invalid interface index %d", InterfaceID);
        return D2DTransportMax;
    }

    // Recognize AirDrop specific p2p* interface based on interface name.
    if (strncmp(info->ifinfo.ifname, "p2p", 3) == 0)
    {
        LogInfo("xD2DInterfaceToTransportType: returning D2DWifiPeerToPeerTransport for interface index %d", InterfaceID);
        return D2DWifiPeerToPeerTransport;
    }

    // Currently there is no way to identify Bluetooth interface by name,
    // since they use "en*" based name strings.

    LogInfo("xD2DInterfaceToTransportType: returning default D2DTransportMax for interface index %d", InterfaceID);
    return D2DTransportMax;
}

// Similar to callExternalHelpers(), but without the checks for the BLE specific interface or flags.
// It's assumed that the domain was already verified to be .local once we are at this level.
mDNSlocal mDNSBool callInternalHelpers(mDNSInterfaceID InterfaceID, DNSServiceFlags flags)
{
    if (   ((InterfaceID == mDNSInterface_Any) && (flags & (kDNSServiceFlagsIncludeP2P | kDNSServiceFlagsIncludeAWDL)))
        || mDNSPlatformInterfaceIsD2D(InterfaceID))
        return mDNStrue;
    else
        return mDNSfalse;
}

mDNSexport void external_start_browsing_for_service(mDNSInterfaceID InterfaceID, const domainname *const typeDomain, DNS_TypeValues qtype, DNSServiceFlags flags, DNSQuestion * q)
{
    // BLE support currently not handled by a D2D plugin
    if (applyToBLE(InterfaceID, flags))
    {
        domainname lower;

        DomainnameToLower(typeDomain, &lower);
        // pass in the key and keySize
        mDNSu8 *end = DNSNameCompressionBuildLHS(&lower, qtype);
        start_BLE_browse(q, &lower, qtype, flags, compression_lhs, end - compression_lhs);
    }

    if (callInternalHelpers(InterfaceID, flags))
        internal_start_browsing_for_service(InterfaceID, typeDomain, qtype, flags);
}

mDNSexport void internal_start_browsing_for_service(mDNSInterfaceID InterfaceID, const domainname *const typeDomain, DNS_TypeValues qtype, DNSServiceFlags flags)
{
    domainname lower;

    DomainnameToLower(typeDomain, &lower);

    if (!D2DBrowseListRefCount(&lower, qtype))
    {
        D2DTransportType transportType, excludedTransport;

        LogInfo("%s: Starting browse for: %##s %s", __func__, lower.c, DNSTypeName(qtype));
        mDNSu8 *end = DNSNameCompressionBuildLHS(&lower, qtype);
        PrintHelper(__func__, compression_lhs, end - compression_lhs, mDNSNULL, 0);

        transportType = xD2DInterfaceToTransportType(InterfaceID, flags, & excludedTransport);
        if (transportType == D2DTransportMax)
        {
            D2DTransportType i;
            for (i = 0; i < D2DTransportMax; i++)
            {
                if (i == excludedTransport) continue;
                if (D2DStartBrowsingForKeyOnTransport) D2DStartBrowsingForKeyOnTransport(compression_lhs, end - compression_lhs, i);
            }
        }
        else
        {
            if (D2DStartBrowsingForKeyOnTransport) D2DStartBrowsingForKeyOnTransport(compression_lhs, end - compression_lhs, transportType);
        }
    }
    D2DBrowseListRetain(&lower, qtype);
}

mDNSexport void external_stop_browsing_for_service(mDNSInterfaceID InterfaceID, const domainname *const typeDomain, DNS_TypeValues qtype, DNSServiceFlags flags)
{
    // BLE support currently not handled by a D2D plugin
    if (applyToBLE(InterfaceID, flags))
    {
        domainname lower;

        // If this is the last instance of this browse, clear any cached records recieved for it.
        // We are not guaranteed to get a D2DServiceLost event for all key, value pairs cached over BLE.
        DomainnameToLower(typeDomain, &lower);
        if (stop_BLE_browse(&lower, qtype, flags))
            xD2DClearCache(&lower, qtype);
    }

    if (callInternalHelpers(InterfaceID, flags))
        internal_stop_browsing_for_service(InterfaceID, typeDomain, qtype, flags);
}

mDNSexport void internal_stop_browsing_for_service(mDNSInterfaceID InterfaceID, const domainname *const typeDomain, DNS_TypeValues qtype, DNSServiceFlags flags)
{
    domainname lower;

    DomainnameToLower(typeDomain, &lower);

    // If found in list and this is the last reference to this browse, remove the key from the D2D plugins.
    if (D2DBrowseListRelease(&lower, qtype) && !D2DBrowseListRefCount(&lower, qtype))
    {
        D2DTransportType transportType, excludedTransport;

        LogInfo("%s: Stopping browse for: %##s %s", __func__, lower.c, DNSTypeName(qtype));
        mDNSu8 *end = DNSNameCompressionBuildLHS(&lower, qtype);
        PrintHelper(__func__, compression_lhs, end - compression_lhs, mDNSNULL, 0);

        transportType = xD2DInterfaceToTransportType(InterfaceID, flags, & excludedTransport);
        if (transportType == D2DTransportMax)
        {
            D2DTransportType i;
            for (i = 0; i < D2DTransportMax; i++)
            {
                if (i == excludedTransport) continue;
                if (D2DStopBrowsingForKeyOnTransport) D2DStopBrowsingForKeyOnTransport(compression_lhs, end - compression_lhs, i);
            }
        }
        else
        {
            if (D2DStopBrowsingForKeyOnTransport) D2DStopBrowsingForKeyOnTransport(compression_lhs, end - compression_lhs, transportType);
        }

        // The D2D driver may not generate the D2DServiceLost event for this key after
        // the D2DStopBrowsingForKey*() call above.  So, we flush the key from the D2D 
        // record cache now.
        xD2DClearCache(&lower, qtype);
    }
}

mDNSexport void external_start_advertising_service(const ResourceRecord *const resourceRecord, DNSServiceFlags flags)
{
    // Note, start_BLE_advertise() is currently called directly from external_start_advertising_helper() since
    // it needs to pass the ServiceRecordSet so that we can promote the record advertisements to AWDL 
    // when we see the corresponding browse indication over BLE.

    if (callInternalHelpers(resourceRecord->InterfaceID, flags))
        internal_start_advertising_service(resourceRecord, flags);
}

mDNSexport void internal_start_advertising_service(const ResourceRecord *const resourceRecord, DNSServiceFlags flags)
{
    domainname lower;
    mDNSu8 *rhs = NULL;
    mDNSu8 *end = NULL;
    D2DTransportType transportType, excludedTransport;
    DomainnameToLower(resourceRecord->name, &lower);

    LogInfo("%s: %s", __func__, RRDisplayString(&mDNSStorage, resourceRecord));

    // For SRV records, update packet filter if p2p interface already exists, otherwise,
    // if will be updated when we get the KEV_DL_IF_ATTACHED event for the interface.
    if (resourceRecord->rrtype == kDNSType_SRV)
        mDNSUpdatePacketFilter(NULL);

    rhs = DNSNameCompressionBuildLHS(&lower, resourceRecord->rrtype);
    end = DNSNameCompressionBuildRHS(rhs, resourceRecord);
    PrintHelper(__func__, compression_lhs, rhs - compression_lhs, rhs, end - rhs);

    transportType = xD2DInterfaceToTransportType(resourceRecord->InterfaceID, flags, & excludedTransport);
    if (transportType == D2DTransportMax)
    {
        D2DTransportType i;
        for (i = 0; i < D2DTransportMax; i++)
        {
            if (i == excludedTransport) continue;
            if (D2DStartAdvertisingPairOnTransport) D2DStartAdvertisingPairOnTransport(compression_lhs, rhs - compression_lhs, rhs, end - rhs, i);
        }
    }
    else
    {
        if (D2DStartAdvertisingPairOnTransport) D2DStartAdvertisingPairOnTransport(compression_lhs, rhs - compression_lhs, rhs, end - rhs, transportType);
    }
}

mDNSexport void external_stop_advertising_service(const ResourceRecord *const resourceRecord, DNSServiceFlags flags)
{
    // BLE support currently not handled by a D2D plugin
    if (applyToBLE(resourceRecord->InterfaceID, flags))
    {
        domainname lower;

        DomainnameToLower(resourceRecord->name, &lower);
        stop_BLE_advertise(&lower, resourceRecord->rrtype, flags);
    }

    if (callInternalHelpers(resourceRecord->InterfaceID, flags))
        internal_stop_advertising_service(resourceRecord, flags);
}

mDNSexport void internal_stop_advertising_service(const ResourceRecord *const resourceRecord, DNSServiceFlags flags)
{
    domainname lower;
    mDNSu8 *rhs = NULL;
    mDNSu8 *end = NULL;
    D2DTransportType transportType, excludedTransport;
    DomainnameToLower(resourceRecord->name, &lower);

    LogInfo("%s: %s", __func__, RRDisplayString(&mDNSStorage, resourceRecord));

    // For SRV records, update packet filter if p2p interface already exists, otherwise,
    // For SRV records, update packet filter to to remove this port from list
    if (resourceRecord->rrtype == kDNSType_SRV)
        mDNSUpdatePacketFilter(resourceRecord);

    rhs = DNSNameCompressionBuildLHS(&lower, resourceRecord->rrtype);
    end = DNSNameCompressionBuildRHS(rhs, resourceRecord);
    PrintHelper(__func__, compression_lhs, rhs - compression_lhs, rhs, end - rhs);

    transportType = xD2DInterfaceToTransportType(resourceRecord->InterfaceID, flags, & excludedTransport);
    if (transportType == D2DTransportMax)
    {
        D2DTransportType i;
        for (i = 0; i < D2DTransportMax; i++)
        {
            if (i == excludedTransport) continue;
            if (D2DStopAdvertisingPairOnTransport) D2DStopAdvertisingPairOnTransport(compression_lhs, rhs - compression_lhs, rhs, end - rhs, i);
        }
    }
    else
    {
        if (D2DStopAdvertisingPairOnTransport) D2DStopAdvertisingPairOnTransport(compression_lhs, rhs - compression_lhs, rhs, end - rhs, transportType);
    }
}

mDNSexport void external_start_resolving_service(mDNSInterfaceID InterfaceID, const domainname *const fqdn, DNSServiceFlags flags)
{
    domainname lower;
    mDNSu8 *rhs = NULL;
    mDNSu8 *end = NULL;
    mDNSBool AWDL_used = false;   // whether AWDL was used for this resolve
    D2DTransportType transportType, excludedTransport;
    DomainnameToLower(SkipLeadingLabels(fqdn, 1), &lower);

    LogInfo("external_start_resolving_service: %##s", fqdn->c);
    rhs = DNSNameCompressionBuildLHS(&lower, kDNSType_PTR);
    end = putDomainNameAsLabels(&compression_base_msg, rhs, compression_limit, fqdn);
    PrintHelper(__func__, compression_lhs, rhs - compression_lhs, rhs, end - rhs);

    transportType = xD2DInterfaceToTransportType(InterfaceID, flags, & excludedTransport);
    if (transportType == D2DTransportMax)
    {
        // Resolving over all the transports, except for excludedTransport if set.
        D2DTransportType i;
        for (i = 0; i < D2DTransportMax; i++)
        {
            if (i == excludedTransport) continue;
            if (D2DStartResolvingPairOnTransport) D2DStartResolvingPairOnTransport(compression_lhs, rhs - compression_lhs, rhs, end - rhs, i);

            if (i == D2DAWDLTransport)
                AWDL_used = true;
        }
    }
    else
    {
        // Resolving over one specific transport.
        if (D2DStartResolvingPairOnTransport) D2DStartResolvingPairOnTransport(compression_lhs, rhs - compression_lhs, rhs, end - rhs, transportType);

        if (transportType == D2DAWDLTransport)
            AWDL_used = true;
    }

    // AWDL wants the SRV and TXT record queries communicated over the D2D interface.
    // We only want these records going to AWDL, so use AWDLInterfaceID as the
    // interface and don't set any other flags.
    if (AWDL_used && AWDLInterfaceID)
    {
        LogInfo("external_start_resolving_service: browse for TXT and SRV over AWDL");
        external_start_browsing_for_service(AWDLInterfaceID, fqdn, kDNSType_TXT, 0, 0);
        external_start_browsing_for_service(AWDLInterfaceID, fqdn, kDNSType_SRV, 0, 0);
    }
}

mDNSexport void external_stop_resolving_service(mDNSInterfaceID InterfaceID, const domainname *const fqdn, DNSServiceFlags flags)
{
    domainname lower;
    mDNSu8 *rhs = NULL;
    mDNSu8 *end = NULL;
    mDNSBool AWDL_used = false;   // whether AWDL was used for this resolve
    D2DTransportType transportType, excludedTransport;
    DomainnameToLower(SkipLeadingLabels(fqdn, 1), &lower);

    LogInfo("external_stop_resolving_service: %##s", fqdn->c);
    rhs = DNSNameCompressionBuildLHS(&lower, kDNSType_PTR);
    end = putDomainNameAsLabels(&compression_base_msg, rhs, compression_limit, fqdn);
    PrintHelper(__func__, compression_lhs, rhs - compression_lhs, rhs, end - rhs);

    transportType = xD2DInterfaceToTransportType(InterfaceID, flags, & excludedTransport);
    if (transportType == D2DTransportMax)
    {
        D2DTransportType i;
        for (i = 0; i < D2DTransportMax; i++)
        {
            if (i == excludedTransport) continue;
            if (D2DStopResolvingPairOnTransport) D2DStopResolvingPairOnTransport(compression_lhs, rhs - compression_lhs, rhs, end - rhs, i);

            if (i == D2DAWDLTransport)
                AWDL_used = true;
        }
    }
    else
    {
        if (D2DStopResolvingPairOnTransport) D2DStopResolvingPairOnTransport(compression_lhs, rhs - compression_lhs, rhs, end - rhs, transportType);

        if (transportType == D2DAWDLTransport)
            AWDL_used = true;
    }

    // AWDL wants the SRV and TXT record queries communicated over the D2D interface.
    // We only want these records going to AWDL, so use AWDLInterfaceID as the
    // interface and don't set any other flags.
    if (AWDL_used && AWDLInterfaceID)
    {
        LogInfo("external_stop_resolving_service: stop browse for TXT and SRV on AWDL");
        external_stop_browsing_for_service(AWDLInterfaceID, fqdn, kDNSType_TXT, 0);
        external_stop_browsing_for_service(AWDLInterfaceID, fqdn, kDNSType_SRV, 0);
    }
}

#elif APPLE_OSX_mDNSResponder

mDNSexport void internal_start_browsing_for_service(mDNS *const m, const domainname *const type, DNS_TypeValues qtype, DNSServiceFlags flags) { (void)m; (void)type; (void)qtype; (void)flags }
mDNSexport void internal_stop_browsing_for_service(mDNS *const m, const domainname *const type, DNS_TypeValues qtype, DNSServiceFlags flags) { (void)m; (void)type; (void)qtype; (void)flags;}
mDNSexport void internal_start_advertising_service(const ResourceRecord *const resourceRecord, DNSServiceFlags flags) { (void)resourceRecord; (void)flags;}
mDNSexport void internal_stop_advertising_service(const ResourceRecord *const resourceRecord, DNSServiceFlags flags) { (void)resourceRecord; (void)flags;}

mDNSexport void external_start_browsing_for_service(mDNS *const m, const domainname *const type, DNS_TypeValues qtype, DNSServiceFlags flags) { (void)m; (void)type; (void)qtype; (void)flags; (void)q }
mDNSexport void external_stop_browsing_for_service(mDNS *const m, const domainname *const type, DNS_TypeValues qtype, DNSServiceFlags flags) { (void)m; (void)type; (void)qtype; (void)flags;}
mDNSexport void external_start_advertising_service(const ResourceRecord *const resourceRecord, DNSServiceFlags flags) { (void)resourceRecord; (void)flags;}
mDNSexport void external_stop_advertising_service(const ResourceRecord *const resourceRecord, DNSServiceFlags flags) { (void)resourceRecord; (void)flags;}
mDNSexport void external_start_resolving_service(const domainname *const fqdn, DNSServiceFlags flags)  { (void)fqdn; (void)flags;}
mDNSexport void external_stop_resolving_service(const domainname *const fqdn, DNSServiceFlags flags)  { (void)fqdn; (void)flags;}

#endif // ! NO_D2D

// ***************************************************************************
// Functions

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Utility Functions
#endif

// We only attempt to send and receive multicast packets on interfaces that are
// (a) flagged as multicast-capable
// (b) *not* flagged as point-to-point (e.g. modem)
// Typically point-to-point interfaces are modems (including mobile-phone pseudo-modems), and we don't want
// to run up the user's bill sending multicast traffic over a link where there's only a single device at the
// other end, and that device (e.g. a modem bank) is probably not answering Multicast DNS queries anyway.

#if BONJOUR_ON_DEMAND
#define MulticastInterface(i) ((i)->m->BonjourEnabled && ((i)->ifa_flags & IFF_MULTICAST) && !((i)->ifa_flags & IFF_POINTOPOINT))
#else
#define MulticastInterface(i) (((i)->ifa_flags & IFF_MULTICAST) && !((i)->ifa_flags & IFF_POINTOPOINT))
#endif
#define SPSInterface(i)       ((i)->ifinfo.McastTxRx && !((i)->ifa_flags & IFF_LOOPBACK) && !(i)->D2DInterface)

mDNSexport void NotifyOfElusiveBug(const char *title, const char *msg)  // Both strings are UTF-8 text
{
    // Unless ForceAlerts is defined, we only show these bug report alerts on machines that have a 17.x.x.x address
    #if !ForceAlerts
    {
        // Determine if we're at Apple (17.*.*.*)
        NetworkInterfaceInfoOSX *i;
        for (i = mDNSStorage.p->InterfaceList; i; i = i->next)
            if (i->ifinfo.ip.type == mDNSAddrType_IPv4 && i->ifinfo.ip.ip.v4.b[0] == 17)
                break;
        if (!i) 
            return; // If not at Apple, don't show the alert
    }
    #endif

    LogMsg("NotifyOfElusiveBug: %s", title);
    LogMsg("NotifyOfElusiveBug: %s", msg);

    // If we display our alert early in the boot process, then it vanishes once the desktop appears.
    // To avoid this, we don't try to display alerts in the first three minutes after boot.
    if ((mDNSu32)(mDNSPlatformRawTime()) < (mDNSu32)(mDNSPlatformOneSecond * 180))
    { 
        LogMsg("Suppressing notification early in boot: %d", mDNSPlatformRawTime()); 
        return; 
    }

#ifndef NO_CFUSERNOTIFICATION
    static int notifyCount = 0; // To guard against excessive display of warning notifications
    if (notifyCount < 5) 
    { 
        notifyCount++; 
        mDNSNotify(title, msg); 
    }
#endif /* NO_CFUSERNOTIFICATION */

}

// Write a syslog message and display an alert, then if ForceAlerts is set, generate a stack trace
#if APPLE_OSX_mDNSResponder && MACOSX_MDNS_MALLOC_DEBUGGING >= 1
mDNSexport void LogMemCorruption(const char *format, ...)
{
    char buffer[512];
    va_list ptr;
    va_start(ptr,format);
    buffer[mDNS_vsnprintf((char *)buffer, sizeof(buffer), format, ptr)] = 0;
    va_end(ptr);
    LogMsg("!!!! %s !!!!", buffer);
    NotifyOfElusiveBug("Memory Corruption", buffer);
#if ForceAlerts
    *(volatile long*)0 = 0;  // Trick to crash and get a stack trace right here, if that's what we want
#endif
}
#endif

// Like LogMemCorruption above, but only display the alert if ForceAlerts is set and we're going to generate a stack trace
#if APPLE_OSX_mDNSResponder
mDNSexport void LogFatalError(const char *format, ...)
{
    char buffer[512];
    va_list ptr;
    va_start(ptr,format);
    buffer[mDNS_vsnprintf((char *)buffer, sizeof(buffer), format, ptr)] = 0;
    va_end(ptr);
    LogMsg("!!!! %s !!!!", buffer);
#if ForceAlerts
    NotifyOfElusiveBug("Fatal Error. See /Library/Logs/DiagnosticReports", buffer);
    *(volatile long*)0 = 0;  // Trick to crash and get a stack trace right here, if that's what we want
#endif
}
#endif

// Returns true if it is an AppleTV based hardware running iOS, false otherwise
mDNSlocal mDNSBool IsAppleTV(void)
{
#if TARGET_OS_EMBEDDED
    static mDNSBool sInitialized = mDNSfalse;
    static mDNSBool sIsAppleTV   = mDNSfalse;
    CFStringRef deviceClass = NULL;

    if(!sInitialized)
    {
        deviceClass = (CFStringRef) MGCopyAnswer(kMGQDeviceClass, NULL);
        if(deviceClass)
        {
            if(CFEqual(deviceClass, kMGDeviceClassAppleTV))
                sIsAppleTV = mDNStrue;
            CFRelease(deviceClass);
        }
        sInitialized = mDNStrue;
    }
    return(sIsAppleTV);
#else 
    return mDNSfalse;
#endif // TARGET_OS_EMBEDDED
}

mDNSlocal struct ifaddrs *myGetIfAddrs(int refresh)
{
    static struct ifaddrs *ifa = NULL;

    if (refresh && ifa)
    {
        freeifaddrs(ifa);
        ifa = NULL;
    }

    if (ifa == NULL) 
        getifaddrs(&ifa);
    return ifa;
}

mDNSlocal void DynamicStoreWrite(int key, const char* subkey, uintptr_t value, signed long valueCnt)
{
    CFStringRef sckey       = NULL;
    Boolean release_sckey   = FALSE;
    CFDataRef bytes         = NULL;
    CFPropertyListRef plist = NULL;

    switch ((enum mDNSDynamicStoreSetConfigKey)key)
    {
        case kmDNSMulticastConfig:
            sckey = CFSTR("State:/Network/" kDNSServiceCompMulticastDNS);
            break;
        case kmDNSDynamicConfig:
            sckey = CFSTR("State:/Network/DynamicDNS");
            break;
        case kmDNSPrivateConfig:
            sckey = CFSTR("State:/Network/" kDNSServiceCompPrivateDNS);
            break;
        case kmDNSBackToMyMacConfig:
            sckey = CFSTR("State:/Network/BackToMyMac");
            break;
        case kmDNSSleepProxyServersState:
        {
            CFMutableStringRef tmp = CFStringCreateMutable(kCFAllocatorDefault, 0);
            CFStringAppend(tmp, CFSTR("State:/Network/Interface/"));
            CFStringAppendCString(tmp, subkey, kCFStringEncodingUTF8);
            CFStringAppend(tmp, CFSTR("/SleepProxyServers"));
            sckey = CFStringCreateCopy(kCFAllocatorDefault, tmp);
            release_sckey = TRUE;
            CFRelease(tmp);
            break;
        }
        case kmDNSDebugState:
            sckey = CFSTR("State:/Network/mDNSResponder/DebugState");
            break;
        default:
            LogMsg("unrecognized key %d", key);
            goto fin;
    }
    if (NULL == (bytes = CFDataCreateWithBytesNoCopy(NULL, (void *)value,
                                                     valueCnt, kCFAllocatorNull)))
    {
        LogMsg("CFDataCreateWithBytesNoCopy of value failed");
        goto fin;
    }
    if (NULL == (plist = CFPropertyListCreateWithData(NULL, bytes, kCFPropertyListImmutable, NULL, NULL)))
    {
        LogMsg("CFPropertyListCreateWithData of bytes failed");
        goto fin;
    }
    CFRelease(bytes);
    bytes = NULL;
    SCDynamicStoreSetValue(NULL, sckey, plist);

fin:
    if (NULL != bytes)
        CFRelease(bytes);
    if (NULL != plist)
        CFRelease(plist);
    if (release_sckey && sckey)
        CFRelease(sckey);
}

mDNSexport void mDNSDynamicStoreSetConfig(int key, const char *subkey, CFPropertyListRef value)
{
    CFPropertyListRef valueCopy;
    char *subkeyCopy  = NULL;
    if (!value)
        return;

    // We need to copy the key and value before we dispatch off the block below as the
    // caller will free the memory once we return from this function.
    valueCopy = CFPropertyListCreateDeepCopy(NULL, value, kCFPropertyListImmutable);
    if (!valueCopy)
    {   
        LogMsg("mDNSDynamicStoreSetConfig: ERROR valueCopy NULL");
        return;
    }
    if (subkey)
    {
        int len    = strlen(subkey);
        subkeyCopy = mDNSPlatformMemAllocate(len + 1);
        if (!subkeyCopy)
        {
            LogMsg("mDNSDynamicStoreSetConfig: ERROR subkeyCopy NULL");
            CFRelease(valueCopy);
            return;
        }
        mDNSPlatformMemCopy(subkeyCopy, subkey, len);
        subkeyCopy[len] = 0;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        CFWriteStreamRef stream = NULL;
        CFDataRef bytes = NULL;
        CFIndex ret;
        KQueueLock(&mDNSStorage);

        if (NULL == (stream = CFWriteStreamCreateWithAllocatedBuffers(NULL, NULL)))
        {
            LogMsg("mDNSDynamicStoreSetConfig : CFWriteStreamCreateWithAllocatedBuffers failed (Object creation failed)");
            goto END;
        }
        CFWriteStreamOpen(stream);
        ret = CFPropertyListWrite(valueCopy, stream, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
        if (ret == 0)
        {
            LogMsg("mDNSDynamicStoreSetConfig : CFPropertyListWriteToStream failed (Could not write property list to stream)");
            goto END;
        }
        if (NULL == (bytes = CFWriteStreamCopyProperty(stream, kCFStreamPropertyDataWritten)))
        {
            LogMsg("mDNSDynamicStoreSetConfig : CFWriteStreamCopyProperty failed (Object creation failed) ");
            goto END;
        }
        CFWriteStreamClose(stream);
        CFRelease(stream);
        stream = NULL;
        DynamicStoreWrite(key, subkeyCopy ? subkeyCopy : "", (uintptr_t)CFDataGetBytePtr(bytes), CFDataGetLength(bytes));

    END:
        CFRelease(valueCopy);
        if (NULL != stream)
        {
            CFWriteStreamClose(stream);
            CFRelease(stream);
        }
        if (NULL != bytes)
            CFRelease(bytes); 
        if (subkeyCopy)
            mDNSPlatformMemFree(subkeyCopy);

        KQueueUnlock(&mDNSStorage, "mDNSDynamicStoreSetConfig");
    });
}

// To match *either* a v4 or v6 instance of this interface name, pass AF_UNSPEC for type
mDNSlocal NetworkInterfaceInfoOSX *SearchForInterfaceByName(mDNS *const m, const char *ifname, int type)
{
    NetworkInterfaceInfoOSX *i;
    for (i = m->p->InterfaceList; i; i = i->next)
        if (i->Exists && !strcmp(i->ifinfo.ifname, ifname) &&
            ((type == AF_UNSPEC                                         ) ||
             (type == AF_INET  && i->ifinfo.ip.type == mDNSAddrType_IPv4) ||
             (type == AF_INET6 && i->ifinfo.ip.type == mDNSAddrType_IPv6))) return(i);
    return(NULL);
}

mDNSlocal int myIfIndexToName(u_short ifindex, char *name)
{
    struct ifaddrs *ifa;
    for (ifa = myGetIfAddrs(0); ifa; ifa = ifa->ifa_next)
        if (ifa->ifa_addr->sa_family == AF_LINK)
            if (((struct sockaddr_dl*)ifa->ifa_addr)->sdl_index == ifindex)
            { strlcpy(name, ifa->ifa_name, IF_NAMESIZE); return 0; }
    return -1;
}

mDNSexport NetworkInterfaceInfoOSX *IfindexToInterfaceInfoOSX(const mDNS *const m, mDNSInterfaceID ifindex)
{
    mDNSu32 scope_id = (mDNSu32)(uintptr_t)ifindex;
    NetworkInterfaceInfoOSX *i;

    // Don't get tricked by inactive interfaces
    for (i = m->p->InterfaceList; i; i = i->next)
        if (i->Registered && i->scope_id == scope_id) return(i);

    return mDNSNULL;
}

mDNSexport mDNSInterfaceID mDNSPlatformInterfaceIDfromInterfaceIndex(mDNS *const m, mDNSu32 ifindex)
{
    if (ifindex == kDNSServiceInterfaceIndexLocalOnly) return(mDNSInterface_LocalOnly);
    if (ifindex == kDNSServiceInterfaceIndexP2P      ) return(mDNSInterface_P2P);
    if (ifindex == kDNSServiceInterfaceIndexBLE      ) return(mDNSInterface_BLE);
    if (ifindex == kDNSServiceInterfaceIndexAny      ) return(mDNSNULL);

    NetworkInterfaceInfoOSX* ifi = IfindexToInterfaceInfoOSX(m, (mDNSInterfaceID)(uintptr_t)ifindex);
    if (!ifi)
    {
        // Not found. Make sure our interface list is up to date, then try again.
        LogInfo("mDNSPlatformInterfaceIDfromInterfaceIndex: InterfaceID for interface index %d not found; Updating interface list", ifindex);
        mDNSMacOSXNetworkChanged(m);
        ifi = IfindexToInterfaceInfoOSX(m, (mDNSInterfaceID)(uintptr_t)ifindex);
    }

    if (!ifi) return(mDNSNULL);

    return(ifi->ifinfo.InterfaceID);
}


mDNSexport mDNSu32 mDNSPlatformInterfaceIndexfromInterfaceID(mDNS *const m, mDNSInterfaceID id, mDNSBool suppressNetworkChange)
{
    NetworkInterfaceInfoOSX *i;
    if (id == mDNSInterface_Any      ) return(0);
    if (id == mDNSInterface_LocalOnly) return(kDNSServiceInterfaceIndexLocalOnly);
    if (id == mDNSInterface_Unicast  ) return(0);
    if (id == mDNSInterface_P2P      ) return(kDNSServiceInterfaceIndexP2P);
    if (id == mDNSInterface_BLE      ) return(kDNSServiceInterfaceIndexBLE);

    mDNSu32 scope_id = (mDNSu32)(uintptr_t)id;

    // Don't use i->Registered here, because we DO want to find inactive interfaces, which have no Registered set
    for (i = m->p->InterfaceList; i; i = i->next)
        if (i->scope_id == scope_id) return(i->scope_id);

    // If we are supposed to suppress network change, return "id" back
    if (suppressNetworkChange) return scope_id;

    // Not found. Make sure our interface list is up to date, then try again.
    LogInfo("Interface index for InterfaceID %p not found; Updating interface list", id);
    mDNSMacOSXNetworkChanged(m);
    for (i = m->p->InterfaceList; i; i = i->next)
        if (i->scope_id == scope_id) return(i->scope_id);

    return(0);
}

#if APPLE_OSX_mDNSResponder
mDNSexport void mDNSASLLog(uuid_t *uuid, const char *subdomain, const char *result, const char *signature, const char *fmt, ...)
{
    if (iOSVers) 
        return; // No ASL on iOS

    static char buffer[512];
    aslmsg asl_msg = asl_new(ASL_TYPE_MSG);

    if (!asl_msg)   { LogMsg("mDNSASLLog: asl_new failed"); return; }
    if (uuid)
    {
        char uuidStr[37];
        uuid_unparse(*uuid, uuidStr);
        asl_set     (asl_msg, "com.apple.message.uuid", uuidStr);
    }

    static char domainBase[] = "com.apple.mDNSResponder.%s";
    mDNS_snprintf   (buffer, sizeof(buffer), domainBase, subdomain);
    asl_set         (asl_msg, "com.apple.message.domain", buffer);

    if (result) asl_set(asl_msg, "com.apple.message.result", result);
    if (signature) asl_set(asl_msg, "com.apple.message.signature", signature);

    va_list ptr;
    va_start(ptr,fmt);
    mDNS_vsnprintf(buffer, sizeof(buffer), fmt, ptr);
    va_end(ptr);

    int old_filter = asl_set_filter(NULL,ASL_FILTER_MASK_UPTO(ASL_LEVEL_DEBUG));
    asl_log(NULL, asl_msg, ASL_LEVEL_DEBUG, "%s", buffer);
    asl_set_filter(NULL, old_filter);
    asl_free(asl_msg);
}


mDNSlocal void mDNSLogDNSSECStatistics(mDNS *const m)
{
    char    buffer[16];

    aslmsg  aslmsg = asl_new(ASL_TYPE_MSG);

    // If we failed to allocate an aslmsg structure, keep accumulating
    // the statistics and try again at the next log interval.
    if (!aslmsg)
    {
        LogMsg("mDNSLogDNSSECStatistics: asl_new() failed!");
        return;
    }

    asl_set(aslmsg,"com.apple.message.domain", "com.apple.mDNSResponder.DNSSECstatistics");

    if (m->rrcache_totalused_unicast)
    {
        mDNS_snprintf(buffer, sizeof(buffer), "%u", (mDNSu32) ((unsigned long)(m->DNSSECStats.TotalMemUsed * 100))/m->rrcache_totalused_unicast);
    }
    else
    {
        LogMsg("mDNSLogDNSSECStatistics: unicast is zero");
        buffer[0] = 0;
    }
    asl_set(aslmsg,"com.apple.message.MemUsage", buffer);

    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.Latency0);
    asl_set(aslmsg,"com.apple.message.Latency0", buffer);
    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.Latency10);
    asl_set(aslmsg,"com.apple.message.Latency10", buffer);
    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.Latency20);
    asl_set(aslmsg,"com.apple.message.Latency20", buffer);
    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.Latency50);
    asl_set(aslmsg,"com.apple.message.Latency50", buffer);
    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.Latency100);
    asl_set(aslmsg,"com.apple.message.Latency100", buffer);

    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.ExtraPackets0);
    asl_set(aslmsg,"com.apple.message.ExtraPackets0", buffer);
    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.ExtraPackets3);
    asl_set(aslmsg,"com.apple.message.ExtraPackets3", buffer);
    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.ExtraPackets7);
    asl_set(aslmsg,"com.apple.message.ExtraPackets7", buffer);
    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.ExtraPackets10);
    asl_set(aslmsg,"com.apple.message.ExtraPackets10", buffer);

    // Ignore IndeterminateStatus as we don't log them
    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.SecureStatus);
    asl_set(aslmsg,"com.apple.message.SecureStatus", buffer);
    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.InsecureStatus);
    asl_set(aslmsg,"com.apple.message.InsecureStatus", buffer);
    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.BogusStatus);
    asl_set(aslmsg,"com.apple.message.BogusStatus", buffer);
    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.NoResponseStatus);
    asl_set(aslmsg,"com.apple.message.NoResponseStatus", buffer);

    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.NumProbesSent);
    asl_set(aslmsg,"com.apple.message.NumProbesSent", buffer);
    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.MsgSize0);
    asl_set(aslmsg,"com.apple.message.MsgSize0", buffer);
    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.MsgSize1);
    asl_set(aslmsg,"com.apple.message.MsgSize1", buffer);
    mDNS_snprintf(buffer, sizeof(buffer), "%u", m->DNSSECStats.MsgSize2);
    asl_set(aslmsg,"com.apple.message.MsgSize2", buffer);

    asl_log(NULL, aslmsg, ASL_LEVEL_NOTICE, "");
    asl_free(aslmsg);
}

// Calculate packets per hour given total packet count and interval in seconds.
// Cast one term of multiplication to (long) to use 64-bit arithmetic 
// and avoid a potential 32-bit overflow prior to the division.
#define ONE_HOUR    3600
#define PACKET_RATE(PACKETS, INTERVAL) (int)(((long) (PACKETS) * ONE_HOUR)/(INTERVAL))

// Put packet rate data in discrete buckets.
mDNSlocal int mDNSBucketData(int inputData, int interval)
{
    if (!interval)
    {
        LogMsg("mDNSBucketData: interval is zero!");
        return 0;
    }

    int ratePerHour = PACKET_RATE(inputData, interval);
    int bucket;

    if (ratePerHour == 0)
        bucket = 0;
    else if (ratePerHour <= 10)
        bucket = 10;
    else if (ratePerHour <= 100)
        bucket = 100;
    else if (ratePerHour <= 1000)
        bucket = 1000;
    else if (ratePerHour <= 5000)
        bucket = 5000;
    else if (ratePerHour <= 10000)
        bucket = 10000;
    else if (ratePerHour <= 50000)
        bucket = 50000;
    else if (ratePerHour <= 100000)
        bucket = 100000;
    else if (ratePerHour <= 250000)
        bucket = 250000;
    else if (ratePerHour <= 500000)
        bucket = 500000;
    else
        bucket = 1000000;

    return bucket;
}

mDNSlocal void mDNSLogBonjourStatistics(mDNS *const m)
{
    static mDNSs32 last_PktNum, last_MPktNum;
    static mDNSs32 last_UnicastPacketsSent, last_MulticastPacketsSent;
    static mDNSs32 last_RemoteSubnet;

    mDNSs32 interval;
    char    buffer[16];
    mDNSs32 inMulticast = m->MPktNum - last_MPktNum;
    mDNSs32 inUnicast   = m->PktNum - last_PktNum - inMulticast;
    mDNSs32 outUnicast  = m->UnicastPacketsSent - last_UnicastPacketsSent;
    mDNSs32 outMulticast = m->MulticastPacketsSent - last_MulticastPacketsSent;
    mDNSs32 remoteSubnet = m->RemoteSubnet - last_RemoteSubnet;

    
    // save starting values for new interval
    last_PktNum = m->PktNum;
    last_MPktNum = m->MPktNum;
    last_UnicastPacketsSent = m->UnicastPacketsSent;
    last_MulticastPacketsSent = m->MulticastPacketsSent;
    last_RemoteSubnet = m->RemoteSubnet;

    // Need a non-zero active time interval.
    if (!m->ActiveStatTime)
        return;

    // Round interval time to nearest hour boundary. Less then 30 minutes rounds to zero.
    interval = (m->ActiveStatTime + ONE_HOUR/2)/ONE_HOUR; 

    // Use a minimum of 30 minutes of awake time to calculate average packet rates.
    // The rounded awake interval should not be greater than the rounded reporting
    // interval.
    if ((interval == 0) || (interval > (kDefaultNextStatsticsLogTime + ONE_HOUR/2)/ONE_HOUR))
        return;

    aslmsg  aslmsg = asl_new(ASL_TYPE_MSG);

    if (!aslmsg)
    {
        LogMsg("mDNSLogBonjourStatistics: asl_new() failed!");
        return;
    }
    // log in MessageTracer format
    asl_set(aslmsg,"com.apple.message.domain", "com.apple.mDNSResponder.statistics");

    snprintf(buffer, sizeof(buffer), "%d", interval);
    asl_set(aslmsg,"com.apple.message.interval", buffer);

    // log the packet rates as packets per hour
    snprintf(buffer, sizeof(buffer), "%d",
            mDNSBucketData(inUnicast, m->ActiveStatTime)); 
    asl_set(aslmsg,"com.apple.message.UnicastIn", buffer);

    snprintf(buffer, sizeof(buffer), "%d",
            mDNSBucketData(inMulticast, m->ActiveStatTime));
    asl_set(aslmsg,"com.apple.message.MulticastIn", buffer);

    snprintf(buffer, sizeof(buffer), "%d",
            mDNSBucketData(outUnicast, m->ActiveStatTime));
    asl_set(aslmsg,"com.apple.message.UnicastOut", buffer);

    snprintf(buffer, sizeof(buffer), "%d",
            mDNSBucketData(outMulticast, m->ActiveStatTime));
    asl_set(aslmsg,"com.apple.message.MulticastOut", buffer);

    snprintf(buffer, sizeof(buffer), "%d",
            mDNSBucketData(remoteSubnet, m->ActiveStatTime));
    asl_set(aslmsg,"com.apple.message.RemoteSubnet", buffer);

    asl_log(NULL, aslmsg, ASL_LEVEL_NOTICE, "");

    asl_free(aslmsg);
}

// Log multicast and unicast traffic statistics to MessageTracer on OSX
mDNSexport void mDNSLogStatistics(mDNS *const m)
{
    // MessageTracer only available on OSX
    if (iOSVers)
        return;

    mDNSs32 currentUTC = mDNSPlatformUTC();

    // log runtime statistics
    if ((currentUTC - m->NextStatLogTime) >= 0)
    {
        m->NextStatLogTime = currentUTC + kDefaultNextStatsticsLogTime;
        // If StatStartTime is zero, it hasn't been reinitialized yet
        // in the wakeup code path.
        if (m->StatStartTime)
        {
            m->ActiveStatTime += currentUTC - m->StatStartTime;
        }

        // Only log statistics if we have recorded some active time during
        // this statistics interval.
        if (m->ActiveStatTime)
        {
            mDNSLogBonjourStatistics(m);
            mDNSLogDNSSECStatistics(m);
        }
 
        // Start a new statistics gathering interval.
        m->StatStartTime = currentUTC;
        m->ActiveStatTime = 0;
    }
}

#endif // APPLE_OSX_mDNSResponder

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - UDP & TCP send & receive
#endif

mDNSlocal mDNSBool AddrRequiresPPPConnection(const struct sockaddr *addr)
{
    mDNSBool result = mDNSfalse;
    SCNetworkConnectionFlags flags;
    CFDataRef remote_addr;
    CFMutableDictionaryRef options;
    SCNetworkReachabilityRef ReachRef = NULL;

    options = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    remote_addr = CFDataCreate(NULL, (const UInt8 *)addr, addr->sa_len);
    CFDictionarySetValue(options, kSCNetworkReachabilityOptionRemoteAddress, remote_addr);
    CFDictionarySetValue(options, kSCNetworkReachabilityOptionServerBypass, kCFBooleanTrue);
    ReachRef = SCNetworkReachabilityCreateWithOptions(kCFAllocatorDefault, options);
    CFRelease(options);
    CFRelease(remote_addr);

    if (!ReachRef) 
    { 
        LogMsg("ERROR: RequiresConnection - SCNetworkReachabilityCreateWithOptions"); 
        goto end; 
    }
    if (!SCNetworkReachabilityGetFlags(ReachRef, &flags)) 
    { 
        LogMsg("ERROR: AddrRequiresPPPConnection - SCNetworkReachabilityGetFlags"); 
        goto end; 
    }
    result = flags & kSCNetworkFlagsConnectionRequired;

end:
    if (ReachRef) 
        CFRelease(ReachRef);
    return result;
}

// Set traffic class for socket
mDNSlocal void setTrafficClass(int socketfd, mDNSBool useBackgroundTrafficClass)
{
    int traffic_class;

    if (useBackgroundTrafficClass)
        traffic_class = SO_TC_BK_SYS;
    else
        traffic_class = SO_TC_CTL;

    (void) setsockopt(socketfd, SOL_SOCKET, SO_TRAFFIC_CLASS, (void *)&traffic_class, sizeof(traffic_class));
}

mDNSlocal int mDNSPlatformGetSocktFd(void *sockCxt, mDNSTransport_Type transType, mDNSAddr_Type addrType)
{
    if (transType == mDNSTransport_UDP)
    {
        UDPSocket* sock = (UDPSocket*) sockCxt;
        return (addrType == mDNSAddrType_IPv4) ? sock->ss.sktv4 : sock->ss.sktv6;
    }
    else if (transType == mDNSTransport_TCP)
    {
        TCPSocket* sock = (TCPSocket*) sockCxt;
        return (addrType == mDNSAddrType_IPv4) ? sock->ss.sktv4 : sock->ss.sktv6;
    }
    else
    {
        LogInfo("mDNSPlatformGetSocktFd: invalid transport %d", transType);
        return kInvalidSocketRef;
    }
}

mDNSexport void mDNSPlatformSetSocktOpt(void *sockCxt, mDNSTransport_Type transType, mDNSAddr_Type addrType, DNSQuestion *q)
{
    int sockfd;
    char unenc_name[MAX_ESCAPED_DOMAIN_NAME];

    // verify passed-in arguments exist and that sockfd is valid
    if (q == mDNSNULL || sockCxt == mDNSNULL || (sockfd = mDNSPlatformGetSocktFd(sockCxt, transType, addrType)) < 0)
        return;

    if (q->pid)
    {
        if (setsockopt(sockfd, SOL_SOCKET, SO_DELEGATED, &q->pid, sizeof(q->pid)) == -1)
            LogMsg("mDNSPlatformSetSocktOpt: Delegate PID failed %s for PID %d", strerror(errno), q->pid);
    }
    else
    {
        if (setsockopt(sockfd, SOL_SOCKET, SO_DELEGATED_UUID, &q->uuid, sizeof(q->uuid)) == -1)
            LogMsg("mDNSPlatformSetSocktOpt: Delegate UUID failed %s", strerror(errno));
    }

    // set the domain on the socket
    ConvertDomainNameToCString(&q->qname, unenc_name);
    if (!(ne_session_set_socket_attributes(sockfd, unenc_name, NULL)))
        LogInfo("mDNSPlatformSetSocktOpt: ne_session_set_socket_attributes()-> setting domain failed for %s", unenc_name);

    int nowake = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_NOWAKEFROMSLEEP, &nowake, sizeof(nowake)) == -1)
        LogInfo("mDNSPlatformSetSocktOpt: SO_NOWAKEFROMSLEEP failed %s", strerror(errno));

    if ((q->flags & kDNSServiceFlagsDenyCellular) || (q->flags & kDNSServiceFlagsDenyExpensive))
    {
#if defined(SO_RESTRICT_DENY_CELLULAR)
        if (q->flags & kDNSServiceFlagsDenyCellular)
        {
            int restrictions = 0;
            restrictions = SO_RESTRICT_DENY_CELLULAR;
            if (setsockopt(sockfd, SOL_SOCKET, SO_RESTRICTIONS, &restrictions, sizeof(restrictions)) == -1)
                LogMsg("mDNSPlatformSetSocktOpt: SO_RESTRICT_DENY_CELLULAR failed %s", strerror(errno));
        }
#endif
#if defined(SO_RESTRICT_DENY_EXPENSIVE)
        if (q->flags & kDNSServiceFlagsDenyExpensive)
        {
            int restrictions = 0;
            restrictions = SO_RESTRICT_DENY_EXPENSIVE;
            if (setsockopt(sockfd, SOL_SOCKET, SO_RESTRICTIONS, &restrictions, sizeof(restrictions)) == -1)
                LogMsg("mDNSPlatformSetSocktOpt: SO_RESTRICT_DENY_EXPENSIVE failed %s", strerror(errno));
        }
#endif
    }
}

// Note: If InterfaceID is NULL, it means, "send this packet through our anonymous unicast socket"
// Note: If InterfaceID is non-NULL it means, "send this packet through our port 5353 socket on the specified interface"
// OR send via our primary v4 unicast socket
// UPDATE: The UDPSocket *src parameter now allows the caller to specify the source socket
mDNSexport mStatus mDNSPlatformSendUDP(const mDNS *const m, const void *const msg, const mDNSu8 *const end,
                                       mDNSInterfaceID InterfaceID, UDPSocket *src, const mDNSAddr *dst, 
                                       mDNSIPPort dstPort, mDNSBool useBackgroundTrafficClass)
{
    NetworkInterfaceInfoOSX *info = mDNSNULL;
    struct sockaddr_storage to;
    int s = -1, err;
    mStatus result = mStatus_NoError;

    if (InterfaceID)
    {
        info = IfindexToInterfaceInfoOSX(m, InterfaceID);
        if (info == NULL)
        {
            // We may not have registered interfaces with the "core" as we may not have
            // seen any interface notifications yet. This typically happens during wakeup
            // where we might try to send DNS requests (non-SuppressUnusable questions internal
            // to mDNSResponder) before we receive network notifications.
            LogInfo("mDNSPlatformSendUDP: Invalid interface index %p", InterfaceID);
            return mStatus_BadParamErr;
        }
    }

    char *ifa_name = InterfaceID ? info->ifinfo.ifname : "unicast";

    if (dst->type == mDNSAddrType_IPv4)
    {
        struct sockaddr_in *sin_to = (struct sockaddr_in*)&to;
        sin_to->sin_len            = sizeof(*sin_to);
        sin_to->sin_family         = AF_INET;
        sin_to->sin_port           = dstPort.NotAnInteger;
        sin_to->sin_addr.s_addr    = dst->ip.v4.NotAnInteger;
        s = (src ? src->ss : m->p->permanentsockets).sktv4;

        if (info)   // Specify outgoing interface
        {
            if (!mDNSAddrIsDNSMulticast(dst))
            {
                #ifdef IP_BOUND_IF
                if (info->scope_id == 0)
                    LogInfo("IP_BOUND_IF socket option not set -- info %p (%s) scope_id is zero", info, ifa_name);
                else
                    setsockopt(s, IPPROTO_IP, IP_BOUND_IF, &info->scope_id, sizeof(info->scope_id));
                #else
                {
                    static int displayed = 0;
                    if (displayed < 1000)
                    {
                        displayed++;
                        LogInfo("IP_BOUND_IF socket option not defined -- cannot specify interface for unicast packets");
                    }
                }
                #endif
            }
            else
                #ifdef IP_MULTICAST_IFINDEX
            {
                err = setsockopt(s, IPPROTO_IP, IP_MULTICAST_IFINDEX, &info->scope_id, sizeof(info->scope_id));
                // We get an error when we compile on a machine that supports this option and run the binary on
                // a different machine that does not support it
                if (err < 0)
                {
                    if (errno != ENOPROTOOPT) LogInfo("mDNSPlatformSendUDP: setsockopt: IP_MUTLTICAST_IFINDEX returned %d", errno);
                    err = setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, &info->ifa_v4addr, sizeof(info->ifa_v4addr));
                    if (err < 0 && !m->NetworkChanged)
                        LogMsg("setsockopt - IP_MULTICAST_IF error %.4a %d errno %d (%s)", &info->ifa_v4addr, err, errno, strerror(errno));
                }
            }
                #else
            {
                err = setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, &info->ifa_v4addr, sizeof(info->ifa_v4addr));
                if (err < 0 && !m->NetworkChanged)
                    LogMsg("setsockopt - IP_MULTICAST_IF error %.4a %d errno %d (%s)", &info->ifa_v4addr, err, errno, strerror(errno));

            }
                #endif
        }
    }

    else if (dst->type == mDNSAddrType_IPv6)
    {
        struct sockaddr_in6 *sin6_to = (struct sockaddr_in6*)&to;
        sin6_to->sin6_len            = sizeof(*sin6_to);
        sin6_to->sin6_family         = AF_INET6;
        sin6_to->sin6_port           = dstPort.NotAnInteger;
        sin6_to->sin6_flowinfo       = 0;
        sin6_to->sin6_addr           = *(struct in6_addr*)&dst->ip.v6;
        sin6_to->sin6_scope_id       = info ? info->scope_id : 0;
        s = (src ? src->ss : m->p->permanentsockets).sktv6;
        if (info && mDNSAddrIsDNSMulticast(dst))    // Specify outgoing interface
        {
            err = setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_IF, &info->scope_id, sizeof(info->scope_id));
            if (err < 0)
            {
                char name[IFNAMSIZ];
                if (if_indextoname(info->scope_id, name) != NULL)
                    LogMsg("setsockopt - IPV6_MULTICAST_IF error %d errno %d (%s)", err, errno, strerror(errno));
                else
                    LogInfo("setsockopt - IPV6_MUTLICAST_IF scopeid %d, not a valid interface", info->scope_id);
            }
        }
#ifdef IPV6_BOUND_IF
        if (info)   // Specify outgoing interface for non-multicast destination
        {
            if (!mDNSAddrIsDNSMulticast(dst))
            {
                if (info->scope_id == 0)
                    LogInfo("IPV6_BOUND_IF socket option not set -- info %p (%s) scope_id is zero", info, ifa_name);
                else
                    setsockopt(s, IPPROTO_IPV6, IPV6_BOUND_IF, &info->scope_id, sizeof(info->scope_id));
            }
        }
#endif
    }

    else
    {
        LogFatalError("mDNSPlatformSendUDP: dst is not an IPv4 or IPv6 address!");
        return mStatus_BadParamErr;
    }

    if (s >= 0)
        verbosedebugf("mDNSPlatformSendUDP: sending on InterfaceID %p %5s/%ld to %#a:%d skt %d",
                      InterfaceID, ifa_name, dst->type, dst, mDNSVal16(dstPort), s);
    else
        verbosedebugf("mDNSPlatformSendUDP: NOT sending on InterfaceID %p %5s/%ld (socket of this type not available)",
                      InterfaceID, ifa_name, dst->type, dst, mDNSVal16(dstPort));

    // Note: When sending, mDNSCore may often ask us to send both a v4 multicast packet and then a v6 multicast packet
    // If we don't have the corresponding type of socket available, then return mStatus_Invalid
    if (s < 0) return(mStatus_Invalid);

    // switch to background traffic class for this message if requested
    if (useBackgroundTrafficClass)
        setTrafficClass(s, useBackgroundTrafficClass);

    err = sendto(s, msg, (UInt8*)end - (UInt8*)msg, 0, (struct sockaddr *)&to, to.ss_len);

    // set traffic class back to default value
    if (useBackgroundTrafficClass)
        setTrafficClass(s, mDNSfalse);

    if (err < 0)
    {
        static int MessageCount = 0;
        LogInfo("mDNSPlatformSendUDP -> sendto(%d) failed to send packet on InterfaceID %p %5s/%d to %#a:%d skt %d error %d errno %d (%s) %lu",
                s, InterfaceID, ifa_name, dst->type, dst, mDNSVal16(dstPort), s, err, errno, strerror(errno), (mDNSu32)(m->timenow));
        if (!mDNSAddressIsAllDNSLinkGroup(dst))
        {
            if (errno == EHOSTUNREACH) return(mStatus_HostUnreachErr);
            if (errno == EHOSTDOWN || errno == ENETDOWN || errno == ENETUNREACH) return(mStatus_TransientErr);
        }
        // Don't report EHOSTUNREACH in the first three minutes after boot
        // This is because mDNSResponder intentionally starts up early in the boot process (See <rdar://problem/3409090>)
        // but this means that sometimes it starts before configd has finished setting up the multicast routing entries.
        if (errno == EHOSTUNREACH && (mDNSu32)(mDNSPlatformRawTime()) < (mDNSu32)(mDNSPlatformOneSecond * 180)) return(mStatus_TransientErr);
        // Don't report EADDRNOTAVAIL ("Can't assign requested address") if we're in the middle of a network configuration change
        if (errno == EADDRNOTAVAIL && m->NetworkChanged) return(mStatus_TransientErr);
        if (errno == EHOSTUNREACH || errno == EADDRNOTAVAIL || errno == ENETDOWN)
            LogInfo("mDNSPlatformSendUDP sendto(%d) failed to send packet on InterfaceID %p %5s/%d to %#a:%d skt %d error %d errno %d (%s) %lu",
                    s, InterfaceID, ifa_name, dst->type, dst, mDNSVal16(dstPort), s, err, errno, strerror(errno), (mDNSu32)(m->timenow));
        else
        {
            MessageCount++;
            if (MessageCount < 50)  // Cap and ensure NO spamming of LogMsgs
                LogMsg("mDNSPlatformSendUDP: sendto(%d) failed to send packet on InterfaceID %p %5s/%d to %#a:%d skt %d error %d errno %d (%s) %lu MessageCount is %d",
                       s, InterfaceID, ifa_name, dst->type, dst, mDNSVal16(dstPort), s, err, errno, strerror(errno), (mDNSu32)(m->timenow), MessageCount);
            else  // If logging is enabled, remove the cap and log aggressively
                LogInfo("mDNSPlatformSendUDP: sendto(%d) failed to send packet on InterfaceID %p %5s/%d to %#a:%d skt %d error %d errno %d (%s) %lu MessageCount is %d",
                        s, InterfaceID, ifa_name, dst->type, dst, mDNSVal16(dstPort), s, err, errno, strerror(errno), (mDNSu32)(m->timenow), MessageCount);
        }

        result = mStatus_UnknownErr;
    }

    return(result);
}

mDNSexport ssize_t myrecvfrom(const int s, void *const buffer, const size_t max,
                             struct sockaddr *const from, size_t *const fromlen, mDNSAddr *dstaddr, char ifname[IF_NAMESIZE], mDNSu8 *ttl)
{
    static unsigned int numLogMessages = 0;
    struct iovec databuffers = { (char *)buffer, max };
    struct msghdr msg;
    ssize_t n;
    struct cmsghdr *cmPtr;
    char ancillary[1024];

    *ttl = 255;  // If kernel fails to provide TTL data (e.g. Jaguar doesn't) then assume the TTL was 255 as it should be

    // Set up the message
    msg.msg_name       = (caddr_t)from;
    msg.msg_namelen    = *fromlen;
    msg.msg_iov        = &databuffers;
    msg.msg_iovlen     = 1;
    msg.msg_control    = (caddr_t)&ancillary;
    msg.msg_controllen = sizeof(ancillary);
    msg.msg_flags      = 0;

    // Receive the data
    n = recvmsg(s, &msg, 0);
    if (n<0)
    {
        if (errno != EWOULDBLOCK && numLogMessages++ < 100) LogMsg("mDNSMacOSX.c: recvmsg(%d) returned error %d errno %d", s, n, errno);
        return(-1);
    }
    if (msg.msg_controllen < (int)sizeof(struct cmsghdr))
    {
        if (numLogMessages++ < 100) LogMsg("mDNSMacOSX.c: recvmsg(%d) returned %d msg.msg_controllen %d < sizeof(struct cmsghdr) %lu, errno %d",
                                           s, n, msg.msg_controllen, sizeof(struct cmsghdr), errno);
        return(-1);
    }
    if (msg.msg_flags & MSG_CTRUNC)
    {
        if (numLogMessages++ < 100) LogMsg("mDNSMacOSX.c: recvmsg(%d) msg.msg_flags & MSG_CTRUNC", s);
        return(-1);
    }

    *fromlen = msg.msg_namelen;

    // Parse each option out of the ancillary data.
    for (cmPtr = CMSG_FIRSTHDR(&msg); cmPtr; cmPtr = CMSG_NXTHDR(&msg, cmPtr))
    {
        // debugf("myrecvfrom cmsg_level %d cmsg_type %d", cmPtr->cmsg_level, cmPtr->cmsg_type);
        if (cmPtr->cmsg_level == IPPROTO_IP && cmPtr->cmsg_type == IP_RECVDSTADDR)
        {
            dstaddr->type = mDNSAddrType_IPv4;
            dstaddr->ip.v4 = *(mDNSv4Addr*)CMSG_DATA(cmPtr);
            //LogMsg("mDNSMacOSX.c: recvmsg IP_RECVDSTADDR %.4a", &dstaddr->ip.v4);
        }
        if (cmPtr->cmsg_level == IPPROTO_IP && cmPtr->cmsg_type == IP_RECVIF)
        {
            struct sockaddr_dl *sdl = (struct sockaddr_dl *)CMSG_DATA(cmPtr);
            if (sdl->sdl_nlen < IF_NAMESIZE)
            {
                mDNSPlatformMemCopy(ifname, sdl->sdl_data, sdl->sdl_nlen);
                ifname[sdl->sdl_nlen] = 0;
                // debugf("IP_RECVIF sdl_index %d, sdl_data %s len %d", sdl->sdl_index, ifname, sdl->sdl_nlen);
            }
        }
        if (cmPtr->cmsg_level == IPPROTO_IP && cmPtr->cmsg_type == IP_RECVTTL)
            *ttl = *(u_char*)CMSG_DATA(cmPtr);
        if (cmPtr->cmsg_level == IPPROTO_IPV6 && cmPtr->cmsg_type == IPV6_PKTINFO)
        {
            struct in6_pktinfo *ip6_info = (struct in6_pktinfo*)CMSG_DATA(cmPtr);
            dstaddr->type = mDNSAddrType_IPv6;
            dstaddr->ip.v6 = *(mDNSv6Addr*)&ip6_info->ipi6_addr;
            myIfIndexToName(ip6_info->ipi6_ifindex, ifname);
        }
        if (cmPtr->cmsg_level == IPPROTO_IPV6 && cmPtr->cmsg_type == IPV6_HOPLIMIT)
            *ttl = *(int*)CMSG_DATA(cmPtr);
    }

    return(n);
}

// What is this for, and why does it use xor instead of a simple quality check? -- SC
mDNSlocal mDNSInterfaceID FindMyInterface(mDNS *const m, const mDNSAddr *addr)
{
    NetworkInterfaceInfo *intf;

    if (addr->type == mDNSAddrType_IPv4)
    {
        for (intf = m->HostInterfaces; intf; intf = intf->next)
        {
            if (intf->ip.type == addr->type && intf->McastTxRx)
            {
                if ((intf->ip.ip.v4.NotAnInteger ^ addr->ip.v4.NotAnInteger) == 0)
                {
                    return(intf->InterfaceID);
                }
            }
        }
    }

    if (addr->type == mDNSAddrType_IPv6)
    {
        for (intf = m->HostInterfaces; intf; intf = intf->next)
        {
            if (intf->ip.type == addr->type && intf->McastTxRx)
            {
                if (((intf->ip.ip.v6.l[0] ^ addr->ip.v6.l[0]) == 0) &&
                    ((intf->ip.ip.v6.l[1] ^ addr->ip.v6.l[1]) == 0) &&
                    ((intf->ip.ip.v6.l[2] ^ addr->ip.v6.l[2]) == 0) &&
                    (((intf->ip.ip.v6.l[3] ^ addr->ip.v6.l[3]) == 0)))
                    {
                        return(intf->InterfaceID);
                    }
            }
        }
    }
    return(mDNSInterface_Any);
}

mDNSexport void myKQSocketCallBack(int s1, short filter, void *context)
{
    KQSocketSet *const ss = (KQSocketSet *)context;
    mDNS *const m = ss->m;
    int err = 0, count = 0, closed = 0;

    if (filter != EVFILT_READ)
        LogMsg("myKQSocketCallBack: Why is filter %d not EVFILT_READ (%d)?", filter, EVFILT_READ);

    if (s1 != ss->sktv4 && s1 != ss->sktv6)
    {
        LogMsg("myKQSocketCallBack: native socket %d", s1);
        LogMsg("myKQSocketCallBack: sktv4 %d sktv6 %d", ss->sktv4, ss->sktv6);
    }

    while (!closed)
    {
        mDNSAddr senderAddr, destAddr = zeroAddr;
        mDNSIPPort senderPort;
        struct sockaddr_storage from;
        size_t fromlen = sizeof(from);
        char packetifname[IF_NAMESIZE] = "";
        mDNSu8 ttl;
        err = myrecvfrom(s1, &m->imsg, sizeof(m->imsg), (struct sockaddr *)&from, &fromlen, &destAddr, packetifname, &ttl);
        if (err < 0) break;

        if ((destAddr.type == mDNSAddrType_IPv4 && (destAddr.ip.v4.b[0] & 0xF0) == 0xE0) ||
            (destAddr.type == mDNSAddrType_IPv6 && (destAddr.ip.v6.b[0]         == 0xFF))) m->p->num_mcasts++;

        count++;
        if (from.ss_family == AF_INET)
        {
            struct sockaddr_in *s = (struct sockaddr_in*)&from;
            senderAddr.type = mDNSAddrType_IPv4;
            senderAddr.ip.v4.NotAnInteger = s->sin_addr.s_addr;
            senderPort.NotAnInteger = s->sin_port;
            //LogInfo("myKQSocketCallBack received IPv4 packet from %#-15a to %#-15a on skt %d %s", &senderAddr, &destAddr, s1, packetifname);
        }
        else if (from.ss_family == AF_INET6)
        {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)&from;
            senderAddr.type = mDNSAddrType_IPv6;
            senderAddr.ip.v6 = *(mDNSv6Addr*)&sin6->sin6_addr;
            senderPort.NotAnInteger = sin6->sin6_port;
            //LogInfo("myKQSocketCallBack received IPv6 packet from %#-15a to %#-15a on skt %d %s", &senderAddr, &destAddr, s1, packetifname);
        }
        else
        {
            LogMsg("myKQSocketCallBack from is unknown address family %d", from.ss_family);
            return;
        }

        // Note: When handling multiple packets in a batch, MUST reset InterfaceID before handling each packet
        mDNSInterfaceID InterfaceID = mDNSNULL;
        NetworkInterfaceInfoOSX *intf = m->p->InterfaceList;
        while (intf) 
        {
            if (intf->Exists && !strcmp(intf->ifinfo.ifname, packetifname))
                break;
            intf = intf->next;
        }

        // When going to sleep we deregister all our interfaces, but if the machine
        // takes a few seconds to sleep we may continue to receive multicasts
        // during that time, which would confuse mDNSCoreReceive, because as far
        // as it's concerned, we should have no active interfaces any more.
        // Hence we ignore multicasts for which we can find no matching InterfaceID.
        if (intf)
            InterfaceID = intf->ifinfo.InterfaceID;
        else if (mDNSAddrIsDNSMulticast(&destAddr))
            continue;

        if (!InterfaceID)
        {
            InterfaceID = FindMyInterface(m, &destAddr);
        }

//		LogMsg("myKQSocketCallBack got packet from %#a to %#a on interface %#a/%s",
//			&senderAddr, &destAddr, &ss->info->ifinfo.ip, ss->info->ifinfo.ifname);

        // mDNSCoreReceive may close the socket we're reading from.  We must break out of our
        // loop when that happens, or we may try to read from an invalid FD.  We do this by
        // setting the closeFlag pointer in the socketset, so CloseSocketSet can inform us
        // if it closes the socketset.
        ss->closeFlag = &closed;

        if (ss->proxy)
        {
            m->p->UDPProxyCallback(m, &m->p->UDPProxy, (unsigned char *)&m->imsg, (unsigned char*)&m->imsg + err, &senderAddr,
                senderPort, &destAddr, ss->port, InterfaceID, NULL);
        }
        else
        {
            mDNSCoreReceive(m, &m->imsg, (unsigned char*)&m->imsg + err, &senderAddr, senderPort, &destAddr, ss->port, InterfaceID);
        }

        // if we didn't close, we can safely dereference the socketset, and should to
        // reset the closeFlag, since it points to something on the stack
        if (!closed) ss->closeFlag = mDNSNULL;
    }

    // If a client application is put in the background, it's socket to us can go defunct and
    // we'll get an ENOTCONN error on that connection.  Just close the socket in that case.
    if (err < 0 && errno == ENOTCONN)
    {
        LogInfo("myKQSocketCallBack: ENOTCONN, closing socket");
        close(s1);
        return;
    }

    if (err < 0 && (errno != EWOULDBLOCK || count == 0))
    {
        // Something is busted here.
        // kqueue says there is a packet, but myrecvfrom says there is not.
        // Try calling select() to get another opinion.
        // Find out about other socket parameter that can help understand why select() says the socket is ready for read
        // All of this is racy, as data may have arrived after the call to select()
        static unsigned int numLogMessages = 0;
        int save_errno = errno;
        int so_error = -1;
        int so_nread = -1;
        int fionread = -1;
        socklen_t solen = sizeof(int);
        fd_set readfds;
        struct timeval timeout;
        int selectresult;
        FD_ZERO(&readfds);
        FD_SET(s1, &readfds);
        timeout.tv_sec  = 0;
        timeout.tv_usec = 0;
        selectresult = select(s1+1, &readfds, NULL, NULL, &timeout);
        if (getsockopt(s1, SOL_SOCKET, SO_ERROR, &so_error, &solen) == -1)
            LogMsg("myKQSocketCallBack getsockopt(SO_ERROR) error %d", errno);
        if (getsockopt(s1, SOL_SOCKET, SO_NREAD, &so_nread, &solen) == -1)
            LogMsg("myKQSocketCallBack getsockopt(SO_NREAD) error %d", errno);
        if (ioctl(s1, FIONREAD, &fionread) == -1)
            LogMsg("myKQSocketCallBack ioctl(FIONREAD) error %d", errno);
        if (numLogMessages++ < 100)
            LogMsg("myKQSocketCallBack recvfrom skt %d error %d errno %d (%s) select %d (%spackets waiting) so_error %d so_nread %d fionread %d count %d",
                   s1, err, save_errno, strerror(save_errno), selectresult, FD_ISSET(s1, &readfds) ? "" : "*NO* ", so_error, so_nread, fionread, count);
        if (numLogMessages > 5)
            NotifyOfElusiveBug("Flaw in Kernel (select/recvfrom mismatch)",
                               "Congratulations, you've reproduced an elusive bug.\r"
                               "Please contact the current assignee of <rdar://problem/3375328>.\r"
                               "Alternatively, you can send email to radar-3387020@group.apple.com. (Note number is different.)\r"
                               "If possible, please leave your machine undisturbed so that someone can come to investigate the problem.");

        sleep(1);       // After logging this error, rate limit so we don't flood syslog
    }
}

mDNSlocal void doTcpSocketCallback(TCPSocket *sock)
{
    mDNSBool c = !sock->connected;
    sock->connected = mDNStrue;
    sock->callback(sock, sock->context, c, sock->err);
    // Note: the callback may call CloseConnection here, which frees the context structure!
}

#ifndef NO_SECURITYFRAMEWORK

mDNSlocal OSStatus tlsWriteSock(SSLConnectionRef connection, const void *data, size_t *dataLength)
{
    int ret = send(((TCPSocket *)connection)->fd, data, *dataLength, 0);
    if (ret >= 0 && (size_t)ret < *dataLength) { *dataLength = ret; return(errSSLWouldBlock); }
    if (ret >= 0)                              { *dataLength = ret; return(noErr); }
    *dataLength = 0;
    if (errno == EAGAIN                      ) return(errSSLWouldBlock);
    if (errno == ENOENT                      ) return(errSSLClosedGraceful);
    if (errno == EPIPE || errno == ECONNRESET) return(errSSLClosedAbort);
    LogMsg("ERROR: tlsWriteSock: %d error %d (%s)\n", ((TCPSocket *)connection)->fd, errno, strerror(errno));
    return(errSSLClosedAbort);
}

mDNSlocal OSStatus tlsReadSock(SSLConnectionRef connection, void *data, size_t *dataLength)
{
    int ret = recv(((TCPSocket *)connection)->fd, data, *dataLength, 0);
    if (ret > 0 && (size_t)ret < *dataLength) { *dataLength = ret; return(errSSLWouldBlock); }
    if (ret > 0)                              { *dataLength = ret; return(noErr); }
    *dataLength = 0;
    if (ret == 0 || errno == ENOENT    ) return(errSSLClosedGraceful);
    if (            errno == EAGAIN    ) return(errSSLWouldBlock);
    if (            errno == ECONNRESET) return(errSSLClosedAbort);
    LogMsg("ERROR: tlsSockRead: error %d (%s)\n", errno, strerror(errno));
    return(errSSLClosedAbort);
}

mDNSlocal OSStatus tlsSetupSock(TCPSocket *sock, SSLProtocolSide pside, SSLConnectionType ctype)
{
    char domname_cstr[MAX_ESCAPED_DOMAIN_NAME];

    sock->tlsContext = SSLCreateContext(kCFAllocatorDefault, pside, ctype);
    if (!sock->tlsContext) 
    { 
        LogMsg("ERROR: tlsSetupSock: SSLCreateContext failed"); 
        return(mStatus_UnknownErr); 
    }

    mStatus err = SSLSetIOFuncs(sock->tlsContext, tlsReadSock, tlsWriteSock);
    if (err) 
    { 
        LogMsg("ERROR: tlsSetupSock: SSLSetIOFuncs failed with error code: %d", err); 
        goto fail; 
    }

    err = SSLSetConnection(sock->tlsContext, (SSLConnectionRef) sock);
    if (err) 
    { 
        LogMsg("ERROR: tlsSetupSock: SSLSetConnection failed with error code: %d", err); 
        goto fail; 
    }

    // Instead of listing all the acceptable ciphers, we just disable the bad ciphers. It does not disable
    // all the bad ciphers like RC4_MD5, but it assumes that the servers don't offer them.
    err = SSLSetAllowAnonymousCiphers(sock->tlsContext, 0);
    if (err) 
    { 
        LogMsg("ERROR: tlsSetupSock: SSLSetAllowAnonymousCiphers failed with error code: %d", err); 
        goto fail; 
    }

    // We already checked for NULL in hostname and this should never happen. Hence, returning -1
    // (error not in OSStatus space) is okay.
    if (!sock->hostname.c[0]) 
    {
        LogMsg("ERROR: tlsSetupSock: hostname NULL"); 
        err = -1;
        goto fail;
    }

    ConvertDomainNameToCString(&sock->hostname, domname_cstr);
    err = SSLSetPeerDomainName(sock->tlsContext, domname_cstr, strlen(domname_cstr));
    if (err) 
    { 
        LogMsg("ERROR: tlsSetupSock: SSLSetPeerDomainname: %s failed with error code: %d", domname_cstr, err); 
        goto fail; 
    }

    return(err);

fail:
    if (sock->tlsContext)
        CFRelease(sock->tlsContext);
    return(err);
}

#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
mDNSlocal void doSSLHandshake(TCPSocket *sock)
{
    mStatus err = SSLHandshake(sock->tlsContext);

    //Can't have multiple threads in mDNS core. When MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM is
    //defined, KQueueLock is a noop. Hence we need to serialize here
    //
    //NOTE: We just can't serialize doTcpSocketCallback alone on the main queue.
    //We need the rest of the logic also. Otherwise, we can enable the READ
    //events below, dispatch a doTcpSocketCallback on the main queue. Assume it is
    //ConnFailed which means we are going to free the tcpInfo. While it
    //is waiting to be dispatched, another read event can come into tcpKQSocketCallback
    //and potentially call doTCPCallback with error which can close the fd and free the
    //tcpInfo. Later when the thread gets dispatched it will crash because the tcpInfo
    //is already freed.

    dispatch_async(dispatch_get_main_queue(), ^{

                       LogInfo("doSSLHandshake %p: got lock", sock); // Log *after* we get the lock

                       if (sock->handshake == handshake_to_be_closed)
                       {
                           LogInfo("SSLHandshake completed after close");
                           mDNSPlatformTCPCloseConnection(sock);
                       }
                       else
                       {
                           if (sock->fd != -1) KQueueSet(sock->fd, EV_ADD, EVFILT_READ, sock->kqEntry);
                           else LogMsg("doSSLHandshake: sock->fd is -1");

                           if (err == errSSLWouldBlock)
                               sock->handshake = handshake_required;
                           else
                           {
                               if (err)
                               {
                                   LogMsg("SSLHandshake failed: %d%s", err, err == errSSLPeerInternalError ? " (server busy)" : "");
                                   CFRelease(sock->tlsContext);
                                   sock->tlsContext = NULL;
                               }

                               sock->err = err ? mStatus_ConnFailed : 0;
                               sock->handshake = handshake_completed;

                               LogInfo("doSSLHandshake: %p calling doTcpSocketCallback fd %d", sock, sock->fd);
                               doTcpSocketCallback(sock);
                           }
                       }

                       LogInfo("SSLHandshake %p: dropping lock for fd %d", sock, sock->fd);
                       return;
                   });
}
#else // MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
mDNSlocal void *doSSLHandshake(TCPSocket *sock)
{
    // Warning: Touching sock without the kqueue lock!
    // We're protected because sock->handshake == handshake_in_progress
    mDNS * const m = sock->m; // Get m now, as we may free sock if marked to be closed while we're waiting on SSLHandshake
    mStatus err = SSLHandshake(sock->tlsContext);

    KQueueLock(m);
    debugf("doSSLHandshake %p: got lock", sock); // Log *after* we get the lock

    if (sock->handshake == handshake_to_be_closed)
    {
        LogInfo("SSLHandshake completed after close");
        mDNSPlatformTCPCloseConnection(sock);
    }
    else
    {
        if (sock->fd != -1) KQueueSet(sock->fd, EV_ADD, EVFILT_READ, sock->kqEntry);
        else LogMsg("doSSLHandshake: sock->fd is -1");

        if (err == errSSLWouldBlock)
            sock->handshake = handshake_required;
        else
        {
            if (err)
            {
                LogMsg("SSLHandshake failed: %d%s", err, err == errSSLPeerInternalError ? " (server busy)" : "");
                CFRelease(sock->tlsContext);
                sock->tlsContext = NULL;
            }

            sock->err = err ? mStatus_ConnFailed : 0;
            sock->handshake = handshake_completed;

            debugf("doSSLHandshake: %p calling doTcpSocketCallback fd %d", sock, sock->fd);
            doTcpSocketCallback(sock);
        }
    }

    debugf("SSLHandshake %p: dropping lock for fd %d", sock, sock->fd);
    KQueueUnlock(m, "doSSLHandshake");
    return NULL;
}
#endif // MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM

mDNSlocal void spawnSSLHandshake(TCPSocket* sock)
{
    debugf("spawnSSLHandshake %p: entry", sock);

    if (sock->handshake != handshake_required) LogMsg("spawnSSLHandshake: handshake status not required: %d", sock->handshake);
    sock->handshake = handshake_in_progress;
    KQueueSet(sock->fd, EV_DELETE, EVFILT_READ, sock->kqEntry);

    // Dispatch it on a separate queue to help avoid blocking other threads/queues, and
    // to limit the number of threads used for SSLHandshake
    dispatch_async(SSLqueue, ^{doSSLHandshake(sock);});

    debugf("spawnSSLHandshake %p: done for %d", sock, sock->fd);
}

#endif /* NO_SECURITYFRAMEWORK */

mDNSlocal void tcpKQSocketCallback(__unused int fd, short filter, void *context)
{
    TCPSocket *sock = context;
    sock->err = mStatus_NoError;

    //if (filter == EVFILT_READ ) LogMsg("myKQSocketCallBack: tcpKQSocketCallback %d is EVFILT_READ", filter);
    //if (filter == EVFILT_WRITE) LogMsg("myKQSocketCallBack: tcpKQSocketCallback %d is EVFILT_WRITE", filter);
    // EV_ONESHOT doesn't seem to work, so we add the filter with EV_ADD, and explicitly delete it here with EV_DELETE
    if (filter == EVFILT_WRITE) 
        KQueueSet(sock->fd, EV_DELETE, EVFILT_WRITE, sock->kqEntry);

    if (sock->flags & kTCPSocketFlags_UseTLS)
    {
#ifndef NO_SECURITYFRAMEWORK
        if (!sock->setup) 
        { 
            sock->setup = mDNStrue; 
            sock->err = tlsSetupSock(sock, kSSLClientSide, kSSLStreamType);
            if (sock->err)
            {
                LogMsg("ERROR: tcpKQSocketCallback: tlsSetupSock failed with error code: %d", sock->err);
                return;
            }
        }
        if (sock->handshake == handshake_required) 
        { 
            spawnSSLHandshake(sock); 
            return;
        }
        else if (sock->handshake == handshake_in_progress || sock->handshake == handshake_to_be_closed)
        {
            return;
        }
        else if (sock->handshake != handshake_completed)
        {
            if (!sock->err) 
                sock->err = mStatus_UnknownErr;
            LogMsg("tcpKQSocketCallback called with unexpected SSLHandshake status: %d", sock->handshake);
        }
#else  /* NO_SECURITYFRAMEWORK */ 
        sock->err = mStatus_UnsupportedErr;
#endif /* NO_SECURITYFRAMEWORK */
    }

    doTcpSocketCallback(sock);
}

#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
mDNSexport int KQueueSet(int fd, u_short flags, short filter, KQueueEntry *const entryRef)
{
    dispatch_queue_t queue = dispatch_get_main_queue();
    dispatch_source_t source;
    if (flags == EV_DELETE)
    {
        if (filter == EVFILT_READ)
        {
            dispatch_source_cancel(entryRef->readSource);
            dispatch_release(entryRef->readSource);
            entryRef->readSource = mDNSNULL;
            debugf("KQueueSet: source cancel for read %p, %p", entryRef->readSource, entryRef->writeSource);
        }
        else if (filter == EVFILT_WRITE)
        {
            dispatch_source_cancel(entryRef->writeSource);
            dispatch_release(entryRef->writeSource);
            entryRef->writeSource = mDNSNULL;
            debugf("KQueueSet: source cancel for write %p, %p", entryRef->readSource, entryRef->writeSource);
        }
        else
            LogMsg("KQueueSet: ERROR: Wrong filter value %d for EV_DELETE", filter);
        return 0;
    }
    if (flags != EV_ADD) LogMsg("KQueueSet: Invalid flags %d", flags);

    if (filter == EVFILT_READ)
    {
        source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, fd, 0, queue);
    }
    else if (filter == EVFILT_WRITE)
    {
        source = dispatch_source_create(DISPATCH_SOURCE_TYPE_WRITE, fd, 0, queue);
    }
    else
    {
        LogMsg("KQueueSet: ERROR: Wrong filter value %d for EV_ADD", filter);
        return -1;
    }
    if (!source) return -1;
    dispatch_source_set_event_handler(source, ^{

                                          mDNSs32 stime = mDNSPlatformRawTime();
                                          entryRef->KQcallback(fd, filter, entryRef->KQcontext);
                                          mDNSs32 etime = mDNSPlatformRawTime();
                                          if (etime - stime >= WatchDogReportingThreshold)
                                              LogInfo("KQEntryCallback Block: WARNING: took %dms to complete", etime - stime);

                                          // Trigger the event delivery to the application. Even though we trigger the
                                          // event completion after handling every event source, these all will hopefully
                                          // get merged
                                          TriggerEventCompletion();

                                      });
    dispatch_source_set_cancel_handler(source, ^{
                                           if (entryRef->fdClosed)
                                           {
                                               //LogMsg("CancelHandler: closing fd %d", fd);
                                               close(fd);
                                           }
                                       });
    dispatch_resume(source);
    if (filter == EVFILT_READ)
        entryRef->readSource = source;
    else
        entryRef->writeSource = source;

    return 0;
}

mDNSexport void KQueueLock(mDNS *const m)
{
    (void)m; //unused
}
mDNSexport void KQueueUnlock(mDNS *const m, const char const *task)
{
    (void)m; //unused
    (void)task; //unused
}
#else
mDNSexport int KQueueSet(int fd, u_short flags, short filter, const KQueueEntry *const entryRef)
{
    struct kevent new_event;
    EV_SET(&new_event, fd, filter, flags, 0, 0, (void*)entryRef);
    return (kevent(KQueueFD, &new_event, 1, NULL, 0, NULL) < 0) ? errno : 0;
}

mDNSexport void KQueueLock(mDNS *const m)
{
    pthread_mutex_lock(&m->p->BigMutex);
    m->p->BigMutexStartTime = mDNSPlatformRawTime();
}

mDNSexport void KQueueUnlock(mDNS *const m, const char* task)
{
    mDNSs32 end = mDNSPlatformRawTime();
    (void)task;
    if (end - m->p->BigMutexStartTime >= WatchDogReportingThreshold)
        LogInfo("WARNING: %s took %dms to complete", task, end - m->p->BigMutexStartTime);

    pthread_mutex_unlock(&m->p->BigMutex);

    char wake = 1;
    if (send(m->p->WakeKQueueLoopFD, &wake, sizeof(wake), 0) == -1)
        LogMsg("ERROR: KQueueWake: send failed with error code: %d (%s)", errno, strerror(errno));
}
#endif

mDNSexport void mDNSPlatformCloseFD(KQueueEntry *kq, int fd)
{
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
        (void) fd; //unused
    if (kq->readSource)
    {
        dispatch_source_cancel(kq->readSource);
        kq->readSource = mDNSNULL;
    }
    if (kq->writeSource)
    {
        dispatch_source_cancel(kq->writeSource);
        kq->writeSource = mDNSNULL;
    }
    // Close happens in the cancellation handler
    debugf("mDNSPlatformCloseFD: resetting sources for %d", fd);
    kq->fdClosed = mDNStrue;
#else
    (void)kq; //unused
    close(fd);
#endif
}

mDNSlocal mStatus SetupTCPSocket(TCPSocket *sock, u_short sa_family, mDNSIPPort *port, mDNSBool useBackgroundTrafficClass)
{
    KQSocketSet *cp = &sock->ss;
    int         *s        = (sa_family == AF_INET) ? &cp->sktv4 : &cp->sktv6;
    KQueueEntry *k        = (sa_family == AF_INET) ? &cp->kqsv4 : &cp->kqsv6;
    const int on = 1;  // "on" for setsockopt
    mStatus err;

    int skt = socket(sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (skt < 3) { if (errno != EAFNOSUPPORT) LogMsg("SetupTCPSocket: socket error %d errno %d (%s)", skt, errno, strerror(errno));return(skt); }

    // for TCP sockets, the traffic class is set once and not changed
    setTrafficClass(skt, useBackgroundTrafficClass);

    if (sa_family == AF_INET)
    {
        // Bind it
        struct sockaddr_in addr;
        mDNSPlatformMemZero(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = port->NotAnInteger;
        err = bind(skt, (struct sockaddr*) &addr, sizeof(addr));
        if (err < 0) { LogMsg("ERROR: bind %s", strerror(errno)); close(skt); return err; }

        // Receive interface identifiers
        err = setsockopt(skt, IPPROTO_IP, IP_RECVIF, &on, sizeof(on));
        if (err < 0) { LogMsg("setsockopt IP_RECVIF - %s", strerror(errno)); close(skt); return err; }

        mDNSPlatformMemZero(&addr, sizeof(addr));
        socklen_t len = sizeof(addr);
        err = getsockname(skt, (struct sockaddr*) &addr, &len);
        if (err < 0) { LogMsg("getsockname - %s", strerror(errno)); close(skt); return err; }

        port->NotAnInteger = addr.sin_port;
    }
    else
    {
        // Bind it
        struct sockaddr_in6 addr6;
        mDNSPlatformMemZero(&addr6, sizeof(addr6));
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = port->NotAnInteger;
        err = bind(skt, (struct sockaddr*) &addr6, sizeof(addr6));
        if (err < 0) { LogMsg("ERROR: bind6 %s", strerror(errno)); close(skt); return err; }

        // We want to receive destination addresses and receive interface identifiers
        err = setsockopt(skt, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
        if (err < 0) { LogMsg("ERROR: setsockopt IPV6_RECVPKTINFO %s", strerror(errno)); close(skt); return err; }

        mDNSPlatformMemZero(&addr6, sizeof(addr6));
        socklen_t len = sizeof(addr6);
        err = getsockname(skt, (struct sockaddr *) &addr6, &len);
        if (err < 0) { LogMsg("getsockname6 - %s", strerror(errno)); close(skt); return err; }

        port->NotAnInteger = addr6.sin6_port;

    }
    *s = skt;
    k->KQcallback = tcpKQSocketCallback;
    k->KQcontext  = sock;
    k->KQtask     = "mDNSPlatformTCPSocket";
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
    k->readSource = mDNSNULL;
    k->writeSource = mDNSNULL;
    k->fdClosed = mDNSfalse;
#endif
    return mStatus_NoError;
}

mDNSexport TCPSocket *mDNSPlatformTCPSocket(mDNS *const m, TCPSocketFlags flags, mDNSIPPort *port, mDNSBool useBackgroundTrafficClass)
{
    mStatus err;
    (void) m;

    TCPSocket *sock = mallocL("TCPSocket/mDNSPlatformTCPSocket", sizeof(TCPSocket));
    if (!sock) { LogMsg("mDNSPlatformTCPSocket: memory allocation failure"); return(mDNSNULL); }

    mDNSPlatformMemZero(sock, sizeof(TCPSocket));

    sock->ss.m     = m;
    sock->ss.sktv4 = -1;
    sock->ss.sktv6 = -1;
    err = SetupTCPSocket(sock, AF_INET, port, useBackgroundTrafficClass);

    if (!err)
    {
        err = SetupTCPSocket(sock, AF_INET6, port, useBackgroundTrafficClass);
        if (err) { mDNSPlatformCloseFD(&sock->ss.kqsv4, sock->ss.sktv4); sock->ss.sktv4 = -1; }
    }
    if (err)
    {
        LogMsg("mDNSPlatformTCPSocket: socket error %d errno %d (%s)", sock->fd, errno, strerror(errno));
        freeL("TCPSocket/mDNSPlatformTCPSocket", sock);
        return(mDNSNULL);
    }
    // sock->fd is used as the default fd  if the caller does not call mDNSPlatformTCPConnect
    sock->fd                = sock->ss.sktv4;
    sock->callback          = mDNSNULL;
    sock->flags             = flags;
    sock->context           = mDNSNULL;
    sock->setup             = mDNSfalse;
    sock->connected         = mDNSfalse;
    sock->handshake         = handshake_required;
    sock->m                 = m;
    sock->err               = mStatus_NoError;

    return sock;
}

mDNSexport mStatus mDNSPlatformTCPConnect(TCPSocket *sock, const mDNSAddr *dst, mDNSOpaque16 dstport, domainname *hostname, mDNSInterfaceID InterfaceID, TCPConnectionCallback callback, void *context)
{
    KQSocketSet *cp = &sock->ss;
    int         *s        = (dst->type == mDNSAddrType_IPv4) ? &cp->sktv4 : &cp->sktv6;
    KQueueEntry *k        = (dst->type == mDNSAddrType_IPv4) ? &cp->kqsv4 : &cp->kqsv6;
    mStatus err = mStatus_NoError;
    struct sockaddr_storage ss;

    sock->callback          = callback;
    sock->context           = context;
    sock->setup             = mDNSfalse;
    sock->connected         = mDNSfalse;
    sock->handshake         = handshake_required;
    sock->err               = mStatus_NoError;

    if (hostname) { debugf("mDNSPlatformTCPConnect: hostname %##s", hostname->c); AssignDomainName(&sock->hostname, hostname); }

    if (dst->type == mDNSAddrType_IPv4)
    {
        struct sockaddr_in *saddr = (struct sockaddr_in *)&ss;
        mDNSPlatformMemZero(saddr, sizeof(*saddr));
        saddr->sin_family      = AF_INET;
        saddr->sin_port        = dstport.NotAnInteger;
        saddr->sin_len         = sizeof(*saddr);
        saddr->sin_addr.s_addr = dst->ip.v4.NotAnInteger;
    }
    else
    {
        struct sockaddr_in6 *saddr6 = (struct sockaddr_in6 *)&ss;
        mDNSPlatformMemZero(saddr6, sizeof(*saddr6));
        saddr6->sin6_family      = AF_INET6;
        saddr6->sin6_port        = dstport.NotAnInteger;
        saddr6->sin6_len         = sizeof(*saddr6);
        saddr6->sin6_addr        = *(struct in6_addr *)&dst->ip.v6;
    }

    // Watch for connect complete (write is ready)
    // EV_ONESHOT doesn't seem to work, so we add the filter with EV_ADD, and explicitly delete it in tcpKQSocketCallback using EV_DELETE
    if (KQueueSet(*s, EV_ADD /* | EV_ONESHOT */, EVFILT_WRITE, k))
    {
        LogMsg("ERROR: mDNSPlatformTCPConnect - KQueueSet failed");
        return errno;
    }

    // Watch for incoming data
    if (KQueueSet(*s, EV_ADD, EVFILT_READ, k))
    {
        LogMsg("ERROR: mDNSPlatformTCPConnect - KQueueSet failed");
        return errno;
    }

    if (fcntl(*s, F_SETFL, fcntl(*s, F_GETFL, 0) | O_NONBLOCK) < 0) // set non-blocking
    {
        LogMsg("ERROR: setsockopt O_NONBLOCK - %s", strerror(errno));
        return mStatus_UnknownErr;
    }

    // We bind to the interface and all subsequent packets including the SYN will be sent out
    // on this interface
    //
    // Note: If we are in Active Directory domain, we may try TCP (if the response can't fit in
    // UDP). mDNSInterface_Unicast indicates this case and not a valid interface.
    if (InterfaceID && InterfaceID != mDNSInterface_Unicast)
    {
        NetworkInterfaceInfoOSX *info = IfindexToInterfaceInfoOSX(&mDNSStorage, InterfaceID);
        if (dst->type == mDNSAddrType_IPv4)
        {
        #ifdef IP_BOUND_IF
            if (info) setsockopt(*s, IPPROTO_IP, IP_BOUND_IF, &info->scope_id, sizeof(info->scope_id));
            else { LogMsg("mDNSPlatformTCPConnect: Invalid interface index %p", InterfaceID); return mStatus_BadParamErr; }
        #else
            (void)InterfaceID; // Unused
            (void)info; // Unused
        #endif
        }
        else
        {
        #ifdef IPV6_BOUND_IF
            if (info) setsockopt(*s, IPPROTO_IPV6, IPV6_BOUND_IF, &info->scope_id, sizeof(info->scope_id));
            else { LogMsg("mDNSPlatformTCPConnect: Invalid interface index %p", InterfaceID); return mStatus_BadParamErr; }
        #else
            (void)InterfaceID; // Unused
            (void)info; // Unused
        #endif
        }
    }

    // mDNSPlatformReadTCP/WriteTCP (unlike the UDP counterpart) does not provide the destination address
    // from which we can infer the destination address family. Hence we need to remember that here.
    // Instead of remembering the address family, we remember the right fd.
    sock->fd = *s;
    sock->kqEntry = k;
    // initiate connection wth peer
    if (connect(*s, (struct sockaddr *)&ss, ss.ss_len) < 0)
    {
        if (errno == EINPROGRESS) return mStatus_ConnPending;
        if (errno == EHOSTUNREACH || errno == EADDRNOTAVAIL || errno == ENETDOWN)
            LogInfo("ERROR: mDNSPlatformTCPConnect - connect failed: socket %d: Error %d (%s)", sock->fd, errno, strerror(errno));
        else
            LogMsg("ERROR: mDNSPlatformTCPConnect - connect failed: socket %d: Error %d (%s) length %d", sock->fd, errno, strerror(errno), ss.ss_len);
        return mStatus_ConnFailed;
    }

    LogMsg("NOTE: mDNSPlatformTCPConnect completed synchronously");
    // kQueue should notify us, but this LogMsg is to help track down if it doesn't
    return err;
}

// Why doesn't mDNSPlatformTCPAccept actually call accept() ?
mDNSexport TCPSocket *mDNSPlatformTCPAccept(TCPSocketFlags flags, int fd)
{
    mStatus err = mStatus_NoError;

    TCPSocket *sock = mallocL("TCPSocket/mDNSPlatformTCPAccept", sizeof(TCPSocket));
    if (!sock) return(mDNSNULL);

    mDNSPlatformMemZero(sock, sizeof(*sock));
    sock->fd = fd;
    sock->flags = flags;

    if (flags & kTCPSocketFlags_UseTLS)
    {
#ifndef NO_SECURITYFRAMEWORK
        if (!ServerCerts) { LogMsg("ERROR: mDNSPlatformTCPAccept: unable to find TLS certificates"); err = mStatus_UnknownErr; goto exit; }

        err = tlsSetupSock(sock, kSSLServerSide, kSSLStreamType);
        if (err) { LogMsg("ERROR: mDNSPlatformTCPAccept: tlsSetupSock failed with error code: %d", err); goto exit; }

        err = SSLSetCertificate(sock->tlsContext, ServerCerts);
        if (err) { LogMsg("ERROR: mDNSPlatformTCPAccept: SSLSetCertificate failed with error code: %d", err); goto exit; }
#else
        err = mStatus_UnsupportedErr;
#endif /* NO_SECURITYFRAMEWORK */
    }
#ifndef NO_SECURITYFRAMEWORK
exit:
#endif

    if (err) { freeL("TCPSocket/mDNSPlatformTCPAccept", sock); return(mDNSNULL); }
    return(sock);
}

mDNSexport mDNSu16 mDNSPlatformGetUDPPort(UDPSocket *sock)
{
    mDNSu16 port;

    port = -1;
    if (sock)
    {
        port = sock->ss.port.NotAnInteger;
    }
    return port;
}

mDNSlocal void CloseSocketSet(KQSocketSet *ss)
{
    if (ss->sktv4 != -1)
    {
        mDNSPlatformCloseFD(&ss->kqsv4,  ss->sktv4);
        ss->sktv4 = -1;
    }
    if (ss->sktv6 != -1)
    {
        mDNSPlatformCloseFD(&ss->kqsv6,  ss->sktv6);
        ss->sktv6 = -1;
    }
    if (ss->closeFlag) *ss->closeFlag = 1;
}

mDNSexport void mDNSPlatformTCPCloseConnection(TCPSocket *sock)
{
    if (sock)
    {
#ifndef NO_SECURITYFRAMEWORK
        if (sock->tlsContext)
        {
            if (sock->handshake == handshake_in_progress) // SSLHandshake thread using this sock (esp. tlsContext)
            {
                LogInfo("mDNSPlatformTCPCloseConnection: called while handshake in progress");
                // When we come back from SSLHandshake, we will notice that a close was here and
                // call this function again which will do the cleanup then.
                sock->handshake = handshake_to_be_closed;
                return;
            }

            SSLClose(sock->tlsContext);
            CFRelease(sock->tlsContext);
            sock->tlsContext = NULL;
        }
#endif /* NO_SECURITYFRAMEWORK */
        if (sock->ss.sktv4 != -1) 
            shutdown(sock->ss.sktv4, 2);
        if (sock->ss.sktv6 != -1) 
            shutdown(sock->ss.sktv6, 2);
        CloseSocketSet(&sock->ss);
        sock->fd = -1;

        freeL("TCPSocket/mDNSPlatformTCPCloseConnection", sock);
    }
}

mDNSexport long mDNSPlatformReadTCP(TCPSocket *sock, void *buf, unsigned long buflen, mDNSBool *closed)
{
    ssize_t nread = 0;
    *closed = mDNSfalse;

    if (sock->flags & kTCPSocketFlags_UseTLS)
    {
#ifndef NO_SECURITYFRAMEWORK
        if (sock->handshake == handshake_required) { LogMsg("mDNSPlatformReadTCP called while handshake required"); return 0; }
        else if (sock->handshake == handshake_in_progress) return 0;
        else if (sock->handshake != handshake_completed) LogMsg("mDNSPlatformReadTCP called with unexpected SSLHandshake status: %d", sock->handshake);

        //LogMsg("Starting SSLRead %d %X", sock->fd, fcntl(sock->fd, F_GETFL, 0));
        mStatus err = SSLRead(sock->tlsContext, buf, buflen, (size_t *)&nread);
        //LogMsg("SSLRead returned %d (%d) nread %d buflen %d", err, errSSLWouldBlock, nread, buflen);
        if (err == errSSLClosedGraceful) { nread = 0; *closed = mDNStrue; }
        else if (err && err != errSSLWouldBlock)
        { LogMsg("ERROR: mDNSPlatformReadTCP - SSLRead: %d", err); nread = -1; *closed = mDNStrue; }
#else
        nread = -1;
        *closed = mDNStrue;
#endif /* NO_SECURITYFRAMEWORK */
    }
    else
    {
        static int CLOSEDcount = 0;
        static int EAGAINcount = 0;
        nread = recv(sock->fd, buf, buflen, 0);

        if (nread > 0) 
        { 
            CLOSEDcount = 0; 
            EAGAINcount = 0; 
        } // On success, clear our error counters
        else if (nread == 0)
        {
            *closed = mDNStrue;
            if ((++CLOSEDcount % 1000) == 0) 
            { 
                LogMsg("ERROR: mDNSPlatformReadTCP - recv %d got CLOSED %d times", sock->fd, CLOSEDcount); 
                assert(CLOSEDcount < 1000);
                // Recovery Mechanism to bail mDNSResponder out of trouble: Instead of logging the same error msg multiple times,
                // crash mDNSResponder using assert() and restart fresh. See advantages below:
                // 1.Better User Experience 
                // 2.CrashLogs frequency can be monitored 
                // 3.StackTrace can be used for more info
            }
        }
        // else nread is negative -- see what kind of error we got
        else if (errno == ECONNRESET) { nread = 0; *closed = mDNStrue; }
        else if (errno != EAGAIN) { LogMsg("ERROR: mDNSPlatformReadTCP - recv: %d (%s)", errno, strerror(errno)); nread = -1; }
        else // errno is EAGAIN (EWOULDBLOCK) -- no data available
        {
            nread = 0;
            if ((++EAGAINcount % 1000) == 0) { LogMsg("ERROR: mDNSPlatformReadTCP - recv %d got EAGAIN %d times", sock->fd, EAGAINcount); sleep(1); }
        }
    }

    return nread;
}

mDNSexport long mDNSPlatformWriteTCP(TCPSocket *sock, const char *msg, unsigned long len)
{
    int nsent;

    if (sock->flags & kTCPSocketFlags_UseTLS)
    {
#ifndef NO_SECURITYFRAMEWORK
        size_t processed;
        if (sock->handshake == handshake_required) { LogMsg("mDNSPlatformWriteTCP called while handshake required"); return 0; }
        if (sock->handshake == handshake_in_progress) return 0;
        else if (sock->handshake != handshake_completed) LogMsg("mDNSPlatformWriteTCP called with unexpected SSLHandshake status: %d", sock->handshake);

        mStatus err = SSLWrite(sock->tlsContext, msg, len, &processed);

        if (!err) nsent = (int) processed;
        else if (err == errSSLWouldBlock) nsent = 0;
        else { LogMsg("ERROR: mDNSPlatformWriteTCP - SSLWrite returned %d", err); nsent = -1; }
#else
        nsent = -1;
#endif /* NO_SECURITYFRAMEWORK */
    }
    else
    {
        nsent = send(sock->fd, msg, len, 0);
        if (nsent < 0)
        {
            if (errno == EAGAIN) nsent = 0;
            else { LogMsg("ERROR: mDNSPlatformWriteTCP - send %s", strerror(errno)); nsent = -1; }
        }
    }

    return nsent;
}

mDNSexport int mDNSPlatformTCPGetFD(TCPSocket *sock)
{
    return sock->fd;
}

// If mDNSIPPort port is non-zero, then it's a multicast socket on the specified interface
// If mDNSIPPort port is zero, then it's a randomly assigned port number, used for sending unicast queries
mDNSlocal mStatus SetupSocket(KQSocketSet *cp, const mDNSIPPort port, u_short sa_family, mDNSIPPort *const outport)
{
    int         *s        = (sa_family == AF_INET) ? &cp->sktv4 : &cp->sktv6;
    KQueueEntry *k        = (sa_family == AF_INET) ? &cp->kqsv4 : &cp->kqsv6;
    const int on = 1;
    const int twofivefive = 255;
    mStatus err = mStatus_NoError;
    char *errstr = mDNSNULL;
    const int mtu = 0;

    cp->closeFlag = mDNSNULL;

    int skt = socket(sa_family, SOCK_DGRAM, IPPROTO_UDP);
    if (skt < 3) { if (errno != EAFNOSUPPORT) LogMsg("SetupSocket: socket error %d errno %d (%s)", skt, errno, strerror(errno));return(skt); }

    // set default traffic class
    setTrafficClass(skt, mDNSfalse);

#ifdef SO_RECV_ANYIF
    // Enable inbound packets on IFEF_AWDL interface.
    // Only done for multicast sockets, since we don't expect unicast socket operations
    // on the IFEF_AWDL interface. Operation is a no-op for other interface types.
    if (mDNSSameIPPort(port, MulticastDNSPort)) 
    {
        err = setsockopt(skt, SOL_SOCKET, SO_RECV_ANYIF, &on, sizeof(on));
        if (err < 0) { errstr = "setsockopt - SO_RECV_ANYIF"; goto fail; }
    }
#endif // SO_RECV_ANYIF

    // ... with a shared UDP port, if it's for multicast receiving
    if (mDNSSameIPPort(port, MulticastDNSPort) || mDNSSameIPPort(port, NATPMPAnnouncementPort))
    {
        err = setsockopt(skt, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
        if (err < 0) { errstr = "setsockopt - SO_REUSEPORT"; goto fail; }
    }

    // Don't want to wake from sleep for inbound packets on the mDNS sockets
    if (mDNSSameIPPort(port, MulticastDNSPort)) 
    {
        int nowake = 1;
        if (setsockopt(skt, SOL_SOCKET, SO_NOWAKEFROMSLEEP, &nowake, sizeof(nowake)) == -1)
            LogInfo("SetupSocket: SO_NOWAKEFROMSLEEP failed %s", strerror(errno));
    }

    if (sa_family == AF_INET)
    {
        // We want to receive destination addresses
        err = setsockopt(skt, IPPROTO_IP, IP_RECVDSTADDR, &on, sizeof(on));
        if (err < 0) { errstr = "setsockopt - IP_RECVDSTADDR"; goto fail; }

        // We want to receive interface identifiers
        err = setsockopt(skt, IPPROTO_IP, IP_RECVIF, &on, sizeof(on));
        if (err < 0) { errstr = "setsockopt - IP_RECVIF"; goto fail; }

        // We want to receive packet TTL value so we can check it
        err = setsockopt(skt, IPPROTO_IP, IP_RECVTTL, &on, sizeof(on));
        if (err < 0) { errstr = "setsockopt - IP_RECVTTL"; goto fail; }

        // Send unicast packets with TTL 255
        err = setsockopt(skt, IPPROTO_IP, IP_TTL, &twofivefive, sizeof(twofivefive));
        if (err < 0) { errstr = "setsockopt - IP_TTL"; goto fail; }

        // And multicast packets with TTL 255 too
        err = setsockopt(skt, IPPROTO_IP, IP_MULTICAST_TTL, &twofivefive, sizeof(twofivefive));
        if (err < 0) { errstr = "setsockopt - IP_MULTICAST_TTL"; goto fail; }

        // And start listening for packets
        struct sockaddr_in listening_sockaddr;
        listening_sockaddr.sin_family      = AF_INET;
        listening_sockaddr.sin_port        = port.NotAnInteger;     // Pass in opaque ID without any byte swapping
        listening_sockaddr.sin_addr.s_addr = mDNSSameIPPort(port, NATPMPAnnouncementPort) ? AllHosts_v4.NotAnInteger : 0;
        err = bind(skt, (struct sockaddr *) &listening_sockaddr, sizeof(listening_sockaddr));
        if (err) { errstr = "bind"; goto fail; }
        if (outport) outport->NotAnInteger = listening_sockaddr.sin_port;
    }
    else if (sa_family == AF_INET6)
    {
        // NAT-PMP Announcements make no sense on IPv6, and we don't support IPv6 for PCP, so bail early w/o error
        if (mDNSSameIPPort(port, NATPMPAnnouncementPort)) { if (outport) *outport = zeroIPPort; close(skt); return mStatus_NoError; }

        // We want to receive destination addresses and receive interface identifiers
        err = setsockopt(skt, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
        if (err < 0) { errstr = "setsockopt - IPV6_RECVPKTINFO"; goto fail; }

        // We want to receive packet hop count value so we can check it
        err = setsockopt(skt, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on, sizeof(on));
        if (err < 0) { errstr = "setsockopt - IPV6_RECVHOPLIMIT"; goto fail; }

        // We want to receive only IPv6 packets. Without this option we get IPv4 packets too,
        // with mapped addresses of the form 0:0:0:0:0:FFFF:xxxx:xxxx, where xxxx:xxxx is the IPv4 address
        err = setsockopt(skt, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
        if (err < 0) { errstr = "setsockopt - IPV6_V6ONLY"; goto fail; }

        // Send unicast packets with TTL 255
        err = setsockopt(skt, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &twofivefive, sizeof(twofivefive));
        if (err < 0) { errstr = "setsockopt - IPV6_UNICAST_HOPS"; goto fail; }

        // And multicast packets with TTL 255 too
        err = setsockopt(skt, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &twofivefive, sizeof(twofivefive));
        if (err < 0) { errstr = "setsockopt - IPV6_MULTICAST_HOPS"; goto fail; }

        // Want to receive our own packets
        err = setsockopt(skt, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &on, sizeof(on));
        if (err < 0) { errstr = "setsockopt - IPV6_MULTICAST_LOOP"; goto fail; }

        // Disable default option to send mDNSv6 packets at min IPv6 MTU: RFC 3542, Sec 11
        err = setsockopt(skt, IPPROTO_IPV6, IPV6_USE_MIN_MTU, &mtu, sizeof(mtu));
        if (err < 0) // Since it is an optimization if we fail just log the err, no need to close the skt
            LogMsg("SetupSocket: setsockopt - IPV6_USE_MIN_MTU: IP6PO_MINMTU_DISABLE socket %d err %d errno %d (%s)", 
                    skt, err, errno, strerror(errno));
        
        // And start listening for packets
        struct sockaddr_in6 listening_sockaddr6;
        mDNSPlatformMemZero(&listening_sockaddr6, sizeof(listening_sockaddr6));
        listening_sockaddr6.sin6_len         = sizeof(listening_sockaddr6);
        listening_sockaddr6.sin6_family      = AF_INET6;
        listening_sockaddr6.sin6_port        = port.NotAnInteger;       // Pass in opaque ID without any byte swapping
        listening_sockaddr6.sin6_flowinfo    = 0;
        listening_sockaddr6.sin6_addr        = in6addr_any; // Want to receive multicasts AND unicasts on this socket
        listening_sockaddr6.sin6_scope_id    = 0;
        err = bind(skt, (struct sockaddr *) &listening_sockaddr6, sizeof(listening_sockaddr6));
        if (err) { errstr = "bind"; goto fail; }
        if (outport) outport->NotAnInteger = listening_sockaddr6.sin6_port;
    }

    fcntl(skt, F_SETFL, fcntl(skt, F_GETFL, 0) | O_NONBLOCK); // set non-blocking
    fcntl(skt, F_SETFD, 1); // set close-on-exec
    *s = skt;
    k->KQcallback = myKQSocketCallBack;
    k->KQcontext  = cp;
    k->KQtask     = "UDP packet reception";
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
    k->readSource = mDNSNULL;
    k->writeSource = mDNSNULL;
    k->fdClosed = mDNSfalse;
#endif
    KQueueSet(*s, EV_ADD, EVFILT_READ, k);

    return(mStatus_NoError);

fail:
    // For "bind" failures, only write log messages for our shared mDNS port, or for binding to zero
    if (strcmp(errstr, "bind") || mDNSSameIPPort(port, MulticastDNSPort) || mDNSIPPortIsZero(port))
        LogMsg("%s skt %d port %d error %d errno %d (%s)", errstr, skt, mDNSVal16(port), err, errno, strerror(errno));

    // If we got a "bind" failure of EADDRINUSE, inform the caller as it might need to try another random port
    if (!strcmp(errstr, "bind") && errno == EADDRINUSE)
    {
        err = EADDRINUSE;
        if (mDNSSameIPPort(port, MulticastDNSPort))
            NotifyOfElusiveBug("Setsockopt SO_REUSEPORT failed",
                               "Congratulations, you've reproduced an elusive bug.\r"
                               "Please contact the current assignee of <rdar://problem/3814904>.\r"
                               "Alternatively, you can send email to radar-3387020@group.apple.com. (Note number is different.)\r"
                               "If possible, please leave your machine undisturbed so that someone can come to investigate the problem.");
    }

    mDNSPlatformCloseFD(k, skt);
    return(err);
}

mDNSexport UDPSocket *mDNSPlatformUDPSocket(mDNS *const m, const mDNSIPPort requestedport)
{
    mStatus err;
    mDNSIPPort port = requestedport;
    mDNSBool randomizePort = mDNSIPPortIsZero(requestedport);
    int i = 10000; // Try at most 10000 times to get a unique random port
    UDPSocket *p = mallocL("UDPSocket", sizeof(UDPSocket));
    if (!p) { LogMsg("mDNSPlatformUDPSocket: memory exhausted"); return(mDNSNULL); }
    mDNSPlatformMemZero(p, sizeof(UDPSocket));
    p->ss.port  = zeroIPPort;
    p->ss.m     = m;
    p->ss.sktv4 = -1;
    p->ss.sktv6 = -1;
    p->ss.proxy = mDNSfalse;

    do
    {
        // The kernel doesn't do cryptographically strong random port allocation, so we do it ourselves here
        if (randomizePort) port = mDNSOpaque16fromIntVal(0xC000 + mDNSRandom(0x3FFF));
        err = SetupSocket(&p->ss, port, AF_INET, &p->ss.port);
        if (!err)
        {
            err = SetupSocket(&p->ss, port, AF_INET6, &p->ss.port);
            if (err) { mDNSPlatformCloseFD(&p->ss.kqsv4, p->ss.sktv4); p->ss.sktv4 = -1; }
        }
        i--;
    } while (err == EADDRINUSE && randomizePort && i);

    if (err)
    {
        // In customer builds we don't want to log failures with port 5351, because this is a known issue
        // of failing to bind to this port when Internet Sharing has already bound to it
        // We also don't want to log about port 5350, due to a known bug when some other
        // process is bound to it.
        if (mDNSSameIPPort(requestedport, NATPMPPort) || mDNSSameIPPort(requestedport, NATPMPAnnouncementPort))
            LogInfo("mDNSPlatformUDPSocket: SetupSocket %d failed error %d errno %d (%s)", mDNSVal16(requestedport), err, errno, strerror(errno));
        else LogMsg("mDNSPlatformUDPSocket: SetupSocket %d failed error %d errno %d (%s)", mDNSVal16(requestedport), err, errno, strerror(errno));
        freeL("UDPSocket", p);
        return(mDNSNULL);
    }
    return(p);
}

mDNSexport void mDNSPlatformUDPClose(UDPSocket *sock)
{
    CloseSocketSet(&sock->ss);
    freeL("UDPSocket", sock);
}

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - BPF Raw packet sending/receiving
#endif

#if APPLE_OSX_mDNSResponder

mDNSexport void mDNSPlatformSendRawPacket(const void *const msg, const mDNSu8 *const end, mDNSInterfaceID InterfaceID)
{
    if (!InterfaceID) { LogMsg("mDNSPlatformSendRawPacket: No InterfaceID specified"); return; }
    NetworkInterfaceInfoOSX *info;

    info = IfindexToInterfaceInfoOSX(&mDNSStorage, InterfaceID);
    if (info == NULL)
    {
        LogMsg("mDNSPlatformSendUDP: Invalid interface index %p", InterfaceID);
        return;
    }
    if (info->BPF_fd < 0)
        LogMsg("mDNSPlatformSendRawPacket: %s BPF_fd %d not ready", info->ifinfo.ifname, info->BPF_fd);
    else
    {
        //LogMsg("mDNSPlatformSendRawPacket %d bytes on %s", end - (mDNSu8 *)msg, info->ifinfo.ifname);
        if (write(info->BPF_fd, msg, end - (mDNSu8 *)msg) < 0)
            LogMsg("mDNSPlatformSendRawPacket: BPF write(%d) failed %d (%s)", info->BPF_fd, errno, strerror(errno));
    }
}

mDNSexport void mDNSPlatformSetLocalAddressCacheEntry(mDNS *const m, const mDNSAddr *const tpa, const mDNSEthAddr *const tha, mDNSInterfaceID InterfaceID)
{
    if (!InterfaceID) { LogMsg("mDNSPlatformSetLocalAddressCacheEntry: No InterfaceID specified"); return; }
    NetworkInterfaceInfoOSX *info;
    info = IfindexToInterfaceInfoOSX(m, InterfaceID);
    if (info == NULL) { LogMsg("mDNSPlatformSetLocalAddressCacheEntry: Invalid interface index %p", InterfaceID); return; }
    // Manually inject an entry into our local ARP cache.
    // (We can't do this by sending an ARP broadcast, because the kernel only pays attention to incoming ARP packets, not outgoing.)
    if (!mDNS_AddressIsLocalSubnet(m, InterfaceID, tpa))
        LogSPS("Don't need address cache entry for %s %#a %.6a",            info->ifinfo.ifname, tpa, tha);
    else
    {
        int result = mDNSSetLocalAddressCacheEntry(info->scope_id, tpa->type, tpa->ip.v6.b, tha->b);
        if (result) LogMsg("Set local address cache entry for %s %#a %.6a failed: %d", info->ifinfo.ifname, tpa, tha, result);
        else LogSPS("Set local address cache entry for %s %#a %.6a",            info->ifinfo.ifname, tpa, tha);
    }
}

mDNSlocal void CloseBPF(NetworkInterfaceInfoOSX *const i)
{
    LogSPS("%s closing BPF fd %d", i->ifinfo.ifname, i->BPF_fd);
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
    // close will happen in the cancel handler
    dispatch_source_cancel(i->BPF_source);
#else

    // Note: MUST NOT close() the underlying native BSD sockets.
    // CFSocketInvalidate() will do that for us, in its own good time, which may not necessarily be immediately, because
    // it first has to unhook the sockets from its select() call on its other thread, before it can safely close them.
    CFRunLoopRemoveSource(CFRunLoopGetMain(), i->BPF_rls, kCFRunLoopDefaultMode);
    CFRelease(i->BPF_rls);
    CFSocketInvalidate(i->BPF_cfs);
    CFRelease(i->BPF_cfs);
#endif
    i->BPF_fd = -1;
    if (i->BPF_mcfd >= 0) { close(i->BPF_mcfd); i->BPF_mcfd = -1; }
}

mDNSlocal void bpf_callback_common(NetworkInterfaceInfoOSX *info)
{
    KQueueLock(info->m);

    // Now we've got the lock, make sure the kqueue thread didn't close the fd out from under us (will not be a problem once the OS X
    // kernel has a mechanism for dispatching all events to a single thread, but for now we have to guard against this race condition).
    if (info->BPF_fd < 0) goto exit;

    ssize_t n = read(info->BPF_fd, &info->m->imsg, info->BPF_len);
    const mDNSu8 *ptr = (const mDNSu8 *)&info->m->imsg;
    const mDNSu8 *end = (const mDNSu8 *)&info->m->imsg + n;
    debugf("%3d: bpf_callback got %d bytes on %s", info->BPF_fd, n, info->ifinfo.ifname);

    if (n<0)
    {
        /* <rdar://problem/10287386>
         * sometimes there can be a race condition btw when the bpf socket
         * gets data and the callback get scheduled and when we call BIOCSETF (which
         * clears the socket).  this can cause the read to hang for a really long time
         * and effectively prevent us from responding to requests for long periods of time.
         * to prevent this make the socket non blocking and just bail if we dont get anything
         */
        if (errno == EAGAIN)
        {
            LogMsg("bpf_callback got EAGAIN bailing");
            goto exit;
        }
        LogMsg("Closing %s BPF fd %d due to error %d (%s)", info->ifinfo.ifname, info->BPF_fd, errno, strerror(errno));
        CloseBPF(info);
        goto exit;
    }

    while (ptr < end)
    {
        const struct bpf_hdr *const bh = (const struct bpf_hdr *)ptr;
        debugf("%3d: bpf_callback ptr %p bh_hdrlen %d data %p bh_caplen %4d bh_datalen %4d next %p remaining %4d",
               info->BPF_fd, ptr, bh->bh_hdrlen, ptr + bh->bh_hdrlen, bh->bh_caplen, bh->bh_datalen,
               ptr + BPF_WORDALIGN(bh->bh_hdrlen + bh->bh_caplen), end - (ptr + BPF_WORDALIGN(bh->bh_hdrlen + bh->bh_caplen)));
        // Note that BPF guarantees that the NETWORK LAYER header will be word aligned, not the link-layer header.
        // Given that An Ethernet header is 14 bytes, this means that if the network layer header (e.g. IP header,
        // ARP message, etc.) is 4-byte aligned, then necessarily the Ethernet header will be NOT be 4-byte aligned.
        mDNSCoreReceiveRawPacket(info->m, ptr + bh->bh_hdrlen, ptr + bh->bh_hdrlen + bh->bh_caplen, info->ifinfo.InterfaceID);
        ptr += BPF_WORDALIGN(bh->bh_hdrlen + bh->bh_caplen);
    }
exit:
    KQueueUnlock(info->m, "bpf_callback");
}
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
mDNSlocal void bpf_callback_dispatch(NetworkInterfaceInfoOSX *const info)
{
    bpf_callback_common(info);
}
#else
mDNSlocal void bpf_callback(const CFSocketRef cfs, const CFSocketCallBackType CallBackType, const CFDataRef address, const void *const data, void *const context)
{
    (void)cfs;
    (void)CallBackType;
    (void)address;
    (void)data;
    bpf_callback_common((NetworkInterfaceInfoOSX *)context);
}
#endif

mDNSexport void mDNSPlatformSendKeepalive(mDNSAddr *sadd, mDNSAddr *dadd, mDNSIPPort *lport, mDNSIPPort *rport, mDNSu32 seq, mDNSu32 ack, mDNSu16 win)
{
    LogMsg("mDNSPlatformSendKeepalive called\n");
    mDNSSendKeepalive(sadd->ip.v6.b, dadd->ip.v6.b, lport->NotAnInteger, rport->NotAnInteger, seq, ack, win);
}

mDNSexport mStatus mDNSPlatformClearSPSData(void)
{
    CFStringRef  spsAddress  = NULL;
    CFStringRef  ownerOPTRec = NULL;

    if ((spsAddress = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s%s%s"), "State:/Network/Interface/", "[^/]", "/BonjourSleepProxyAddress")))
    {
        if (SCDynamicStoreRemoveValue(NULL, spsAddress) == false)
            LogSPS("mDNSPlatformClearSPSData: Unable to remove sleep proxy address key");
    }

    if((ownerOPTRec = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s%s%s"), "State:/Network/Interface/", "[^/]", "/BonjourSleepProxyOPTRecord")))
    {
        if (SCDynamicStoreRemoveValue(NULL, ownerOPTRec) == false)
            LogSPS("mDNSPlatformClearSPSData: Unable to remove sleep proxy owner option record key");
    }

    if (spsAddress)  CFRelease(spsAddress);
    if (ownerOPTRec) CFRelease(ownerOPTRec);
    return KERN_SUCCESS;
}

mDNSlocal int getMACAddress(int family, v6addr_t raddr, v6addr_t gaddr, int *gfamily, ethaddr_t eth)
{
    struct
    {
        struct rt_msghdr m_rtm;
        char   m_space[512];
    } m_rtmsg;
    
    struct rt_msghdr *rtm = &(m_rtmsg.m_rtm);
    char  *cp  = m_rtmsg.m_space;
    int    seq = 6367, sock, rlen, i;
    struct sockaddr_in      *sin  = NULL;
    struct sockaddr_in6     *sin6 = NULL;
    struct sockaddr_dl      *sdl  = NULL;
    struct sockaddr_storage  sins;
    struct sockaddr_dl       sdl_m;
    
#define NEXTADDR(w, s, len)         \
if (rtm->rtm_addrs & (w))       \
{                               \
bcopy((char *)s, cp, len);  \
cp += len;                  \
}
    
    bzero(&sins,  sizeof(struct sockaddr_storage));
    bzero(&sdl_m, sizeof(struct sockaddr_dl));
    bzero((char *)&m_rtmsg, sizeof(m_rtmsg));
    
    sock = socket(PF_ROUTE, SOCK_RAW, 0);
    if (sock < 0)
    {
        LogMsg("getMACAddress: Can not open the socket - %s", strerror(errno));
        return errno;
    }
    
    rtm->rtm_addrs   |= RTA_DST | RTA_GATEWAY;
    rtm->rtm_type     = RTM_GET;
    rtm->rtm_flags    = 0;
    rtm->rtm_version  = RTM_VERSION;
    rtm->rtm_seq      = ++seq;
    
    sdl_m.sdl_len     = sizeof(sdl_m);
    sdl_m.sdl_family  = AF_LINK;
    if (family == AF_INET)
    {
        sin = (struct sockaddr_in*)&sins;
        sin->sin_family = AF_INET;
        sin->sin_len    = sizeof(struct sockaddr_in);
        memcpy(&sin->sin_addr, raddr, sizeof(struct in_addr));
        NEXTADDR(RTA_DST, sin, sin->sin_len);
    }
    else if (family == AF_INET6)
    {
        sin6 = (struct sockaddr_in6 *)&sins;
        sin6->sin6_len    = sizeof(struct sockaddr_in6);
        sin6->sin6_family = AF_INET6;
        memcpy(&sin6->sin6_addr, raddr, sizeof(struct in6_addr));
        NEXTADDR(RTA_DST, sin6, sin6->sin6_len);
    }
    NEXTADDR(RTA_GATEWAY, &sdl_m, sdl_m.sdl_len);
    rtm->rtm_msglen = rlen = cp - (char *)&m_rtmsg;
    
    if (write(sock, (char *)&m_rtmsg, rlen) < 0)
    {
        LogMsg("getMACAddress: writing to routing socket: %s", strerror(errno));
        close(sock);
        return errno;
    }
    
    do
    {
        rlen = read(sock, (char *)&m_rtmsg, sizeof(m_rtmsg));
    }
    while (rlen > 0 && (rtm->rtm_seq != seq || rtm->rtm_pid != getpid()));
    
    if (rlen < 0)
        LogMsg("getMACAddress: Read from routing socket failed");
    
    if (family == AF_INET)
    {
        sin = (struct sockaddr_in *) (rtm + 1);
        sdl = (struct sockaddr_dl *) (sin->sin_len + (char *) sin);
    }
    else if (family == AF_INET6)
    {
        sin6 = (struct sockaddr_in6 *) (rtm +1);
        sdl  = (struct sockaddr_dl  *) (sin6->sin6_len + (char *) sin6);
    }
    
    if (!sdl)
    {
        LogMsg("getMACAddress: sdl is NULL for family %d", family);
        close(sock);
        return -1;
    }
    
    // If the address is not on the local net, we get the IP address of the gateway.
    // We would have to repeat the process to get the MAC address of the gateway
    *gfamily = sdl->sdl_family;
    if (sdl->sdl_family == AF_INET)
    {
        if (sin)
        {
            struct sockaddr_in *new_sin = (struct sockaddr_in *)(sin->sin_len +(char*) sin);
            memcpy(gaddr, &new_sin->sin_addr, sizeof(struct in_addr));
        }
        else
        {
            LogMsg("getMACAddress: sin is NULL");
        }
        close(sock);
        return -1;
    }
    else if (sdl->sdl_family == AF_INET6)
    {
        if (sin6)
        {
            struct sockaddr_in6 *new_sin6 = (struct sockaddr_in6 *)(sin6->sin6_len +(char*) sin6);
            memcpy(gaddr, &new_sin6->sin6_addr, sizeof(struct in6_addr));
        }
        else
        {
            LogMsg("getMACAddress: sin6 is NULL");
        }
        close(sock);
        return -1;
    }
    
    unsigned char *ptr = (unsigned char *)LLADDR(sdl);
    for (i = 0; i < ETHER_ADDR_LEN; i++)
        (eth)[i] = *(ptr +i);
    
    close(sock);
    
    return KERN_SUCCESS;
}

mDNSlocal int GetRemoteMacinternal(int family, v6addr_t raddr, ethaddr_t eth)
{
    int      ret = 0;
    v6addr_t gateway;
    int      gfamily = 0;
    int      count = 0;

    do
    {
        ret = getMACAddress(family, raddr, gateway, &gfamily, eth);
        if (ret == -1)
        {
            memcpy(raddr, gateway, sizeof(family));
            family = gfamily;
            count++;
        }
    }
    while ((ret == -1) && (count < 5));
    return ret;
}

mDNSlocal int StoreSPSMACAddressinternal(int family, v6addr_t spsaddr, const char *ifname)
{
    ethaddr_t              eth;
    char                   spsip[INET6_ADDRSTRLEN];
    int                    ret        = 0;
    CFStringRef            sckey      = NULL;
    SCDynamicStoreRef      store      = SCDynamicStoreCreate(NULL, CFSTR("mDNSResponder:StoreSPSMACAddress"), NULL, NULL);
    SCDynamicStoreRef      ipstore    = SCDynamicStoreCreate(NULL, CFSTR("mDNSResponder:GetIPv6Addresses"), NULL, NULL);
    CFMutableDictionaryRef dict       = NULL;
    CFStringRef            entityname = NULL;
    CFDictionaryRef        ipdict     = NULL;
    CFArrayRef             addrs      = NULL;
    
    if ((store == NULL) || (ipstore == NULL))
    {
        LogMsg("StoreSPSMACAddressinternal: Unable to accesss SC Dynamic Store");
        ret = -1;
        goto fin;
    }
    
    // Get the MAC address of the Sleep Proxy Server
    memset(eth, 0, sizeof(eth));
    ret = GetRemoteMacinternal(family, spsaddr, eth);
    if (ret !=  0)
    {
        LogMsg("StoreSPSMACAddressinternal: Failed to determine the MAC address");
        goto fin;
    }
    
    // Create/Update the dynamic store entry for the specified interface
    sckey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s%s%s"), "State:/Network/Interface/", ifname, "/BonjourSleepProxyAddress");
    dict  = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!dict)
    {
        LogMsg("StoreSPSMACAddressinternal: SPSCreateDict() Could not create CFDictionary dict");
        ret = -1;
        goto fin;
    }
    
    CFStringRef macaddr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%02x:%02x:%02x:%02x:%02x:%02x"), eth[0], eth[1], eth[2], eth[3], eth[4], eth[5]);
    CFDictionarySetValue(dict, CFSTR("MACAddress"), macaddr);
    if (NULL != macaddr)
        CFRelease(macaddr);
    
    if( NULL == inet_ntop(family, (void *)spsaddr, spsip, sizeof(spsip)))
    {
        LogMsg("StoreSPSMACAddressinternal: inet_ntop failed: %s", strerror(errno));
        ret = -1;
        goto fin;
    }
    
    CFStringRef ipaddr = CFStringCreateWithCString(NULL, spsip, kCFStringEncodingUTF8);
    CFDictionarySetValue(dict, CFSTR("IPAddress"), ipaddr);
    if (NULL != ipaddr)
        CFRelease(ipaddr);
    
    // Get the current IPv6 addresses on this interface and store them so NAs can be sent on wakeup
    if ((entityname = CFStringCreateWithFormat(NULL, NULL, CFSTR("State:/Network/Interface/%s/IPv6"), ifname)) != NULL)
    {
        if ((ipdict = SCDynamicStoreCopyValue(ipstore, entityname)) != NULL)
        {
            if((addrs = CFDictionaryGetValue(ipdict, CFSTR("Addresses"))) != NULL)
            {
                addrs = CFRetain(addrs);
                CFDictionarySetValue(dict, CFSTR("RegisteredAddresses"), addrs);
            }
        }
    }
    SCDynamicStoreSetValue(store, sckey, dict);
    
fin:
    if (store)      CFRelease(store);
    if (ipstore)    CFRelease(ipstore);
    if (sckey)      CFRelease(sckey);
    if (dict)       CFRelease(dict);
    if (ipdict)     CFRelease(ipdict);
    if (entityname) CFRelease(entityname);
    if (addrs)      CFRelease(addrs);
    
    return ret;
}

mDNSlocal void mDNSStoreSPSMACAddress(int family, v6addr_t spsaddr, char *ifname)
{
    struct
    {
        v6addr_t saddr;
    } addr;
    int err = 0;
    
    mDNSPlatformMemCopy(addr.saddr, spsaddr, sizeof(v6addr_t));
    
    err = StoreSPSMACAddressinternal(family, (uint8_t *)addr.saddr, ifname);
    if (err != 0)
        LogMsg("mDNSStoreSPSMACAddress : failed");
}

mDNSexport mStatus mDNSPlatformStoreSPSMACAddr(mDNSAddr *spsaddr, char *ifname)
{
    int family = (spsaddr->type == mDNSAddrType_IPv4) ? AF_INET : AF_INET6;
    
    LogInfo("mDNSPlatformStoreSPSMACAddr : Storing %#a on interface %s", spsaddr, ifname);
    mDNSStoreSPSMACAddress(family, spsaddr->ip.v6.b, ifname);
    
    return KERN_SUCCESS;
}


mDNSexport mStatus mDNSPlatformStoreOwnerOptRecord(char *ifname, DNSMessage* msg, int length)
{
    int                    ret   = 0;
    CFStringRef            sckey = NULL;
    SCDynamicStoreRef      store = SCDynamicStoreCreate(NULL, CFSTR("mDNSResponder:StoreOwnerOPTRecord"), NULL, NULL);
    CFMutableDictionaryRef dict  = NULL;

    if (store == NULL)
    {
        LogMsg("mDNSPlatformStoreOwnerOptRecord: Unable to accesss SC Dynamic Store");
        ret = -1;
        goto fin;
    }

    // Create/Update the dynamic store entry for the specified interface
    sckey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s%s%s"), "State:/Network/Interface/", ifname, "/BonjourSleepProxyOPTRecord");
    dict  = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!dict)
    {
        LogMsg("mDNSPlatformStoreOwnerOptRecord: Could not create CFDictionary dictionary to store OPT Record");       
        ret =-1;
        goto fin;
    }

    CFDataRef optRec = NULL;
    optRec = CFDataCreate(NULL, (const uint8_t *)msg, (CFIndex)length);
    CFDictionarySetValue(dict, CFSTR("OwnerOPTRecord"), optRec);
    if (NULL != optRec) CFRelease(optRec);

    SCDynamicStoreSetValue(store, sckey, dict);

fin:
    if (NULL != store)  CFRelease(store);
    if (NULL != sckey)  CFRelease(sckey);
    if (NULL != dict)   CFRelease(dict);
    return ret;
}

mDNSlocal void mDNSGet_RemoteMAC(mDNS *const m, int family, v6addr_t raddr)
{
    ethaddr_t            eth;
    IPAddressMACMapping *addrMapping;
    int kr = KERN_FAILURE;
    struct
    {
        v6addr_t addr;
    } dst;
    
    mDNSPlatformMemCopy(dst.addr, raddr, sizeof(v6addr_t));
    
    kr = GetRemoteMacinternal(family, (uint8_t *)dst.addr, eth);

    // If the call to get the remote MAC address succeeds, allocate and copy
    // the values and schedule a task to update the MAC address in the TCP Keepalive record.
    if (kr == 0)
    {
        addrMapping = mDNSPlatformMemAllocate(sizeof(IPAddressMACMapping));
        snprintf(addrMapping->ethaddr, sizeof(addrMapping->ethaddr), "%02x:%02x:%02x:%02x:%02x:%02x",
                     eth[0], eth[1], eth[2], eth[3], eth[4], eth[5]);
        if (family == AF_INET)
        {
            addrMapping->ipaddr.type = mDNSAddrType_IPv4;
            mDNSPlatformMemCopy(addrMapping->ipaddr.ip.v4.b, raddr, sizeof(v6addr_t));
        }
        else
        {
            addrMapping->ipaddr.type = mDNSAddrType_IPv6;
            mDNSPlatformMemCopy(addrMapping->ipaddr.ip.v6.b, raddr, sizeof(v6addr_t));
        }
        UpdateRMAC(m, addrMapping);
    }
}

mDNSexport mStatus mDNSPlatformGetRemoteMacAddr(mDNS *const m, mDNSAddr *raddr)
{
    int family = (raddr->type == mDNSAddrType_IPv4) ? AF_INET : AF_INET6;
    
    LogInfo("mDNSPlatformGetRemoteMacAddr calling mDNSGet_RemoteMAC");
    mDNSGet_RemoteMAC(m, family, raddr->ip.v6.b);
    
    return KERN_SUCCESS;
}

mDNSexport mStatus mDNSPlatformRetrieveTCPInfo(mDNS *const m, mDNSAddr *laddr, mDNSIPPort *lport, mDNSAddr *raddr, mDNSIPPort *rport, mDNSTCPInfo *mti)
{
    mDNSs32 intfid;
    mDNSs32 error  = 0;
    int     family = (laddr->type == mDNSAddrType_IPv4) ? AF_INET : AF_INET6;

    error = mDNSRetrieveTCPInfo(family, laddr->ip.v6.b, lport->NotAnInteger, raddr->ip.v6.b, rport->NotAnInteger, (uint32_t *)&(mti->seq), (uint32_t *)&(mti->ack), (uint16_t *)&(mti->window), (int32_t*)&intfid);
    if (error != KERN_SUCCESS)
    {
        LogMsg("%s: mDNSRetrieveTCPInfo returned : %d", __func__, error);
        return error;
    }
    mti->IntfId = mDNSPlatformInterfaceIDfromInterfaceIndex(m, intfid);
    return error;
}

#define BPF_SetOffset(from, cond, to) (from)->cond = (to) - 1 - (from)

mDNSlocal int CountProxyTargets(mDNS *const m, NetworkInterfaceInfoOSX *x, int *p4, int *p6)
{
    int numv4 = 0, numv6 = 0;
    AuthRecord *rr;

    for (rr = m->ResourceRecords; rr; rr=rr->next)
        if (rr->resrec.InterfaceID == x->ifinfo.InterfaceID && rr->AddressProxy.type == mDNSAddrType_IPv4)
        {
            if (p4) LogSPS("CountProxyTargets: fd %d %-7s IP%2d %.4a", x->BPF_fd, x->ifinfo.ifname, numv4, &rr->AddressProxy.ip.v4);
            numv4++;
        }

    for (rr = m->ResourceRecords; rr; rr=rr->next)
        if (rr->resrec.InterfaceID == x->ifinfo.InterfaceID && rr->AddressProxy.type == mDNSAddrType_IPv6)
        {
            if (p6) LogSPS("CountProxyTargets: fd %d %-7s IP%2d %.16a", x->BPF_fd, x->ifinfo.ifname, numv6, &rr->AddressProxy.ip.v6);
            numv6++;
        }

    if (p4) *p4 = numv4;
    if (p6) *p6 = numv6;
    return(numv4 + numv6);
}

mDNSexport void mDNSPlatformUpdateProxyList(mDNS *const m, const mDNSInterfaceID InterfaceID)
{
    NetworkInterfaceInfoOSX *x;

    // Note: We can't use IfIndexToInterfaceInfoOSX because that looks for Registered also.
    for (x = m->p->InterfaceList; x; x = x->next) if ((x->ifinfo.InterfaceID == InterfaceID) && (x->BPF_fd >= 0)) break;

    if (!x) { LogMsg("mDNSPlatformUpdateProxyList: ERROR InterfaceID %p not found", InterfaceID); return; }

    #define MAX_BPF_ADDRS 250
    int numv4 = 0, numv6 = 0;

    if (CountProxyTargets(m, x, &numv4, &numv6) > MAX_BPF_ADDRS)
    {
        LogMsg("mDNSPlatformUpdateProxyList: ERROR Too many address proxy records v4 %d v6 %d", numv4, numv6);
        if (numv4 > MAX_BPF_ADDRS) numv4 = MAX_BPF_ADDRS;
        numv6 = MAX_BPF_ADDRS - numv4;
    }

    LogSPS("mDNSPlatformUpdateProxyList: fd %d %-7s MAC  %.6a %d v4 %d v6", x->BPF_fd, x->ifinfo.ifname, &x->ifinfo.MAC, numv4, numv6);

    // Caution: This is a static structure, so we need to be careful that any modifications we make to it
    // are done in such a way that they work correctly when mDNSPlatformUpdateProxyList is called multiple times
    static struct bpf_insn filter[17 + MAX_BPF_ADDRS] =
    {
        BPF_STMT(BPF_LD  + BPF_H   + BPF_ABS, 12),              // 0 Read Ethertype (bytes 12,13)

        BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 0x0806, 0, 1),      // 1 If Ethertype == ARP goto next, else 3
        BPF_STMT(BPF_RET + BPF_K,             42),              // 2 Return 42-byte ARP

        BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 0x0800, 4, 0),      // 3 If Ethertype == IPv4 goto 8 (IPv4 address list check) else next

        BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 0x86DD, 0, 9),      // 4 If Ethertype == IPv6 goto next, else exit
        BPF_STMT(BPF_LD  + BPF_H   + BPF_ABS, 20),              // 5 Read Protocol and Hop Limit (bytes 20,21)
        BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 0x3AFF, 0, 9),      // 6 If (Prot,TTL) == (3A,FF) goto next, else IPv6 address list check
        BPF_STMT(BPF_RET + BPF_K,             86),              // 7 Return 86-byte ND

        // Is IPv4 packet; check if it's addressed to any IPv4 address we're proxying for
        BPF_STMT(BPF_LD  + BPF_W   + BPF_ABS, 30),              // 8 Read IPv4 Dst (bytes 30,31,32,33)
    };

    struct bpf_insn *pc   = &filter[9];
    struct bpf_insn *chk6 = pc   + numv4 + 1;   // numv4 address checks, plus a "return 0"
    struct bpf_insn *fail = chk6 + 1 + numv6;   // Get v6 Dst LSW, plus numv6 address checks
    struct bpf_insn *ret4 = fail + 1;
    struct bpf_insn *ret6 = ret4 + 4;

    static const struct bpf_insn rf  = BPF_STMT(BPF_RET + BPF_K, 0);                // No match: Return nothing

    static const struct bpf_insn g6  = BPF_STMT(BPF_LD  + BPF_W   + BPF_ABS, 50);   // Read IPv6 Dst LSW (bytes 50,51,52,53)

    static const struct bpf_insn r4a = BPF_STMT(BPF_LDX + BPF_B   + BPF_MSH, 14);   // Get IP Header length (normally 20)
    static const struct bpf_insn r4b = BPF_STMT(BPF_LD  + BPF_IMM,           54);   // A = 54 (14-byte Ethernet plus 20-byte TCP + 20 bytes spare)
    static const struct bpf_insn r4c = BPF_STMT(BPF_ALU + BPF_ADD + BPF_X,    0);   // A += IP Header length
    static const struct bpf_insn r4d = BPF_STMT(BPF_RET + BPF_A, 0);                // Success: Return Ethernet + IP + TCP + 20 bytes spare (normally 74)

    static const struct bpf_insn r6a = BPF_STMT(BPF_RET + BPF_K, 94);               // Success: Return Eth + IPv6 + TCP + 20 bytes spare

    BPF_SetOffset(&filter[4], jf, fail);    // If Ethertype not ARP, IPv4, or IPv6, fail
    BPF_SetOffset(&filter[6], jf, chk6);    // If IPv6 but not ICMPv6, go to IPv6 address list check

    // BPF Byte-Order Note
    // The BPF API designers apparently thought that programmers would not be smart enough to use htons
    // and htonl correctly to convert numeric values to network byte order on little-endian machines,
    // so instead they chose to make the API implicitly byte-swap *ALL* values, even literal byte strings
    // that shouldn't be byte-swapped, like ASCII text, Ethernet addresses, IP addresses, etc.
    // As a result, if we put Ethernet addresses and IP addresses in the right byte order, the BPF API
    // will byte-swap and make them backwards, and then our filter won't work. So, we have to arrange
    // that on little-endian machines we deliberately put addresses in memory with the bytes backwards,
    // so that when the BPF API goes through and swaps them all, they end up back as they should be.
    // In summary, if we byte-swap all the non-numeric fields that shouldn't be swapped, and we *don't*
    // swap any of the numeric values that *should* be byte-swapped, then the filter will work correctly.

    // IPSEC capture size notes:
    //  8 bytes UDP header
    //  4 bytes Non-ESP Marker
    // 28 bytes IKE Header
    // --
    // 40 Total. Capturing TCP Header + 20 gets us enough bytes to receive the IKE Header in a UDP-encapsulated IKE packet.

    AuthRecord *rr;
    for (rr = m->ResourceRecords; rr; rr=rr->next)
        if (rr->resrec.InterfaceID == InterfaceID && rr->AddressProxy.type == mDNSAddrType_IPv4)
        {
            mDNSv4Addr a = rr->AddressProxy.ip.v4;
            pc->code = BPF_JMP + BPF_JEQ + BPF_K;
            BPF_SetOffset(pc, jt, ret4);
            pc->jf   = 0;
            pc->k    = (bpf_u_int32)a.b[0] << 24 | (bpf_u_int32)a.b[1] << 16 | (bpf_u_int32)a.b[2] << 8 | (bpf_u_int32)a.b[3];
            pc++;
        }
    *pc++ = rf;

    if (pc != chk6) LogMsg("mDNSPlatformUpdateProxyList: pc %p != chk6 %p", pc, chk6);
    *pc++ = g6; // chk6 points here

    // First cancel any previous ND group memberships we had, then create a fresh socket
    if (x->BPF_mcfd >= 0) close(x->BPF_mcfd);
    x->BPF_mcfd = socket(AF_INET6, SOCK_DGRAM, 0);

    for (rr = m->ResourceRecords; rr; rr=rr->next)
        if (rr->resrec.InterfaceID == InterfaceID && rr->AddressProxy.type == mDNSAddrType_IPv6)
        {
            const mDNSv6Addr *const a = &rr->AddressProxy.ip.v6;
            pc->code = BPF_JMP + BPF_JEQ + BPF_K;
            BPF_SetOffset(pc, jt, ret6);
            pc->jf   = 0;
            pc->k    = (bpf_u_int32)a->b[0x0C] << 24 | (bpf_u_int32)a->b[0x0D] << 16 | (bpf_u_int32)a->b[0x0E] << 8 | (bpf_u_int32)a->b[0x0F];
            pc++;

            struct ipv6_mreq i6mr;
            i6mr.ipv6mr_interface = x->scope_id;
            i6mr.ipv6mr_multiaddr = *(const struct in6_addr*)&NDP_prefix;
            i6mr.ipv6mr_multiaddr.s6_addr[0xD] = a->b[0xD];
            i6mr.ipv6mr_multiaddr.s6_addr[0xE] = a->b[0xE];
            i6mr.ipv6mr_multiaddr.s6_addr[0xF] = a->b[0xF];

            // Do precautionary IPV6_LEAVE_GROUP first, necessary to clear stale kernel state
            mStatus err = setsockopt(x->BPF_mcfd, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &i6mr, sizeof(i6mr));
            if (err < 0 && (errno != EADDRNOTAVAIL))
                LogMsg("mDNSPlatformUpdateProxyList: IPV6_LEAVE_GROUP error %d errno %d (%s) group %.16a on %u", err, errno, strerror(errno), &i6mr.ipv6mr_multiaddr, i6mr.ipv6mr_interface);

            err = setsockopt(x->BPF_mcfd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &i6mr, sizeof(i6mr));
            if (err < 0 && (errno != EADDRINUSE))   // Joining same group twice can give "Address already in use" error -- no need to report that
                LogMsg("mDNSPlatformUpdateProxyList: IPV6_JOIN_GROUP error %d errno %d (%s) group %.16a on %u", err, errno, strerror(errno), &i6mr.ipv6mr_multiaddr, i6mr.ipv6mr_interface);

            LogSPS("Joined IPv6 ND multicast group %.16a for %.16a", &i6mr.ipv6mr_multiaddr, a);
        }

    if (pc != fail) LogMsg("mDNSPlatformUpdateProxyList: pc %p != fail %p", pc, fail);
    *pc++ = rf;     // fail points here

    if (pc != ret4) LogMsg("mDNSPlatformUpdateProxyList: pc %p != ret4 %p", pc, ret4);
    *pc++ = r4a;    // ret4 points here
    *pc++ = r4b;
    *pc++ = r4c;
    *pc++ = r4d;

    if (pc != ret6) LogMsg("mDNSPlatformUpdateProxyList: pc %p != ret6 %p", pc, ret6);
    *pc++ = r6a;    // ret6 points here

    struct bpf_program prog = { pc - filter, filter };

#if 0
    // For debugging BPF filter program
    unsigned int q;
    for (q=0; q<prog.bf_len; q++)
        LogSPS("mDNSPlatformUpdateProxyList: %2d { 0x%02x, %d, %d, 0x%08x },", q, prog.bf_insns[q].code, prog.bf_insns[q].jt, prog.bf_insns[q].jf, prog.bf_insns[q].k);
#endif

    if (!numv4 && !numv6)
    {
        LogSPS("mDNSPlatformUpdateProxyList: No need for filter");
        if (m->timenow == 0) LogMsg("mDNSPlatformUpdateProxyList: m->timenow == 0");
        // Schedule check to see if we can close this BPF_fd now
        if (!m->NetworkChanged) m->NetworkChanged = NonZeroTime(m->timenow + mDNSPlatformOneSecond * 2);
        // prog.bf_len = 0; This seems to panic the kernel
        if (x->BPF_fd < 0) return;      // If we've already closed our BPF_fd, no need to generate an error message below
    }
    
    if (ioctl(x->BPF_fd, BIOCSETFNR, &prog) < 0) LogMsg("mDNSPlatformUpdateProxyList: BIOCSETFNR(%d) failed %d (%s)", prog.bf_len, errno, strerror(errno));
    else LogSPS("mDNSPlatformUpdateProxyList: BIOCSETFNR(%d) successful", prog.bf_len);
}

mDNSexport void mDNSPlatformReceiveBPF_fd(mDNS *const m, int fd)
{
    mDNS_Lock(m);

    NetworkInterfaceInfoOSX *i;
    for (i = m->p->InterfaceList; i; i = i->next) if (i->BPF_fd == -2) break;
    if (!i) { LogSPS("mDNSPlatformReceiveBPF_fd: No Interfaces awaiting BPF fd %d; closing", fd); close(fd); }
    else
    {
        LogSPS("%s using   BPF fd %d", i->ifinfo.ifname, fd);

        struct bpf_version v;
        if (ioctl(fd, BIOCVERSION, &v) < 0)
            LogMsg("mDNSPlatformReceiveBPF_fd: %d %s BIOCVERSION failed %d (%s)", fd, i->ifinfo.ifname, errno, strerror(errno));
        else if (BPF_MAJOR_VERSION != v.bv_major || BPF_MINOR_VERSION != v.bv_minor)
            LogMsg("mDNSPlatformReceiveBPF_fd: %d %s BIOCVERSION header %d.%d kernel %d.%d",
                   fd, i->ifinfo.ifname, BPF_MAJOR_VERSION, BPF_MINOR_VERSION, v.bv_major, v.bv_minor);

        if (ioctl(fd, BIOCGBLEN, &i->BPF_len) < 0)
            LogMsg("mDNSPlatformReceiveBPF_fd: %d %s BIOCGBLEN failed %d (%s)", fd, i->ifinfo.ifname, errno, strerror(errno));

        if (i->BPF_len > sizeof(m->imsg))
        {
            i->BPF_len = sizeof(m->imsg);
            if (ioctl(fd, BIOCSBLEN, &i->BPF_len) < 0)
                LogMsg("mDNSPlatformReceiveBPF_fd: %d %s BIOCSBLEN failed %d (%s)", fd, i->ifinfo.ifname, errno, strerror(errno));
            else
                LogSPS("mDNSPlatformReceiveBPF_fd: %d %s BIOCSBLEN %d", fd, i->ifinfo.ifname, i->BPF_len);
        }

        static const u_int opt_one = 1;
        if (ioctl(fd, BIOCIMMEDIATE, &opt_one) < 0)
            LogMsg("mDNSPlatformReceiveBPF_fd: %d %s BIOCIMMEDIATE failed %d (%s)", fd, i->ifinfo.ifname, errno, strerror(errno));

        //if (ioctl(fd, BIOCPROMISC, &opt_one) < 0)
        //	LogMsg("mDNSPlatformReceiveBPF_fd: %d %s BIOCPROMISC failed %d (%s)", fd, i->ifinfo.ifname, errno, strerror(errno));

        //if (ioctl(fd, BIOCSHDRCMPLT, &opt_one) < 0)
        //	LogMsg("mDNSPlatformReceiveBPF_fd: %d %s BIOCSHDRCMPLT failed %d (%s)", fd, i->ifinfo.ifname, errno, strerror(errno));

        /*  <rdar://problem/10287386>
         *  make socket non blocking see comments in bpf_callback_common for more info
         */
        if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) < 0) // set non-blocking
        {
            LogMsg("mDNSPlatformReceiveBPF_fd: %d %s O_NONBLOCK failed %d (%s)", fd, i->ifinfo.ifname, errno, strerror(errno));
        }

        struct ifreq ifr;
        mDNSPlatformMemZero(&ifr, sizeof(ifr));
        strlcpy(ifr.ifr_name, i->ifinfo.ifname, sizeof(ifr.ifr_name));
        if (ioctl(fd, BIOCSETIF, &ifr) < 0)
        { LogMsg("mDNSPlatformReceiveBPF_fd: %d %s BIOCSETIF failed %d (%s)", fd, i->ifinfo.ifname, errno, strerror(errno)); i->BPF_fd = -3; }
        else
        {
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
            i->BPF_fd  = fd;
            i->BPF_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, fd, 0, dispatch_get_main_queue());
            if (!i->BPF_source) {LogMsg("mDNSPlatformReceiveBPF_fd: dispatch source create failed"); return;}
            dispatch_source_set_event_handler(i->BPF_source, ^{bpf_callback_dispatch(i);});
            dispatch_source_set_cancel_handler(i->BPF_source, ^{close(fd);});
            dispatch_resume(i->BPF_source);
#else
            CFSocketContext myCFSocketContext = { 0, i, NULL, NULL, NULL };
            i->BPF_fd  = fd;
            i->BPF_cfs = CFSocketCreateWithNative(kCFAllocatorDefault, fd, kCFSocketReadCallBack, bpf_callback, &myCFSocketContext);
            i->BPF_rls = CFSocketCreateRunLoopSource(kCFAllocatorDefault, i->BPF_cfs, 0);
            CFRunLoopAddSource(CFRunLoopGetMain(), i->BPF_rls, kCFRunLoopDefaultMode);
#endif
            mDNSPlatformUpdateProxyList(m, i->ifinfo.InterfaceID);
        }
    }

    mDNS_Unlock(m);
}

#endif // APPLE_OSX_mDNSResponder

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Key Management
#endif

#ifndef NO_SECURITYFRAMEWORK
mDNSlocal CFArrayRef CopyCertChain(SecIdentityRef identity)
{
    CFMutableArrayRef certChain = NULL;
    if (!identity) { LogMsg("CopyCertChain: identity is NULL"); return(NULL); }
    SecCertificateRef cert;
    OSStatus err = SecIdentityCopyCertificate(identity, &cert);
    if (err || !cert) LogMsg("CopyCertChain: SecIdentityCopyCertificate() returned %d", (int) err);
    else
    {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        SecPolicySearchRef searchRef;
        err = SecPolicySearchCreate(CSSM_CERT_X_509v3, &CSSMOID_APPLE_X509_BASIC, NULL, &searchRef);
        if (err || !searchRef) LogMsg("CopyCertChain: SecPolicySearchCreate() returned %d", (int) err);
        else
        {
            SecPolicyRef policy;
            err = SecPolicySearchCopyNext(searchRef, &policy);
            if (err || !policy) LogMsg("CopyCertChain: SecPolicySearchCopyNext() returned %d", (int) err);
            else
            {
                CFArrayRef wrappedCert = CFArrayCreate(NULL, (const void**) &cert, 1, &kCFTypeArrayCallBacks);
                if (!wrappedCert) LogMsg("CopyCertChain: wrappedCert is NULL");
                else
                {
                    SecTrustRef trust;
                    err = SecTrustCreateWithCertificates(wrappedCert, policy, &trust);
                    if (err || !trust) LogMsg("CopyCertChain: SecTrustCreateWithCertificates() returned %d", (int) err);
                    else
                    {
                        err = SecTrustEvaluate(trust, NULL);
                        if (err) LogMsg("CopyCertChain: SecTrustEvaluate() returned %d", (int) err);
                        else
                        {
                            CFArrayRef rawCertChain;
                            CSSM_TP_APPLE_EVIDENCE_INFO *statusChain = NULL;
                            err = SecTrustGetResult(trust, NULL, &rawCertChain, &statusChain);
                            if (err || !rawCertChain || !statusChain) LogMsg("CopyCertChain: SecTrustGetResult() returned %d", (int) err);
                            else
                            {
                                certChain = CFArrayCreateMutableCopy(NULL, 0, rawCertChain);
                                if (!certChain) LogMsg("CopyCertChain: certChain is NULL");
                                else
                                {
                                    // Replace the SecCertificateRef at certChain[0] with a SecIdentityRef per documentation for SSLSetCertificate:
                                    // <http://devworld.apple.com/documentation/Security/Reference/secureTransportRef/index.html>
                                    CFArraySetValueAtIndex(certChain, 0, identity);
                                    // Remove root from cert chain, but keep any and all intermediate certificates that have been signed by the root certificate
                                    if (CFArrayGetCount(certChain) > 1) CFArrayRemoveValueAtIndex(certChain, CFArrayGetCount(certChain) - 1);
                                }
                                CFRelease(rawCertChain);
                                // Do not free statusChain:
                                // <http://developer.apple.com/documentation/Security/Reference/certifkeytrustservices/Reference/reference.html> says:
                                // certChain: Call the CFRelease function to release this object when you are finished with it.
                                // statusChain: Do not attempt to free this pointer; it remains valid until the trust management object is released...
                            }
                        }
                        CFRelease(trust);
                    }
                    CFRelease(wrappedCert);
                }
                CFRelease(policy);
            }
            CFRelease(searchRef);
        }
#pragma clang diagnostic pop
        CFRelease(cert);
    }
    return certChain;
}
#endif /* NO_SECURITYFRAMEWORK */

mDNSexport mStatus mDNSPlatformTLSSetupCerts(void)
{
#ifdef NO_SECURITYFRAMEWORK
    return mStatus_UnsupportedErr;
#else
    SecIdentityRef identity = nil;
    SecIdentitySearchRef srchRef = nil;
    OSStatus err;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // search for "any" identity matching specified key use
    // In this app, we expect there to be exactly one
    err = SecIdentitySearchCreate(NULL, CSSM_KEYUSE_DECRYPT, &srchRef);
    if (err) { LogMsg("ERROR: mDNSPlatformTLSSetupCerts: SecIdentitySearchCreate returned %d", (int) err); return err; }

    err = SecIdentitySearchCopyNext(srchRef, &identity);
    if (err) { LogMsg("ERROR: mDNSPlatformTLSSetupCerts: SecIdentitySearchCopyNext returned %d", (int) err); return err; }
#pragma clang diagnostic pop

    if (CFGetTypeID(identity) != SecIdentityGetTypeID())
    { LogMsg("ERROR: mDNSPlatformTLSSetupCerts: SecIdentitySearchCopyNext CFTypeID failure"); return mStatus_UnknownErr; }

    // Found one. Call CopyCertChain to create the correct certificate chain.
    ServerCerts = CopyCertChain(identity);
    if (ServerCerts == nil) { LogMsg("ERROR: mDNSPlatformTLSSetupCerts: CopyCertChain error"); return mStatus_UnknownErr; }

    return mStatus_NoError;
#endif /* NO_SECURITYFRAMEWORK */
}

mDNSexport void  mDNSPlatformTLSTearDownCerts(void)
{
#ifndef NO_SECURITYFRAMEWORK
    if (ServerCerts) { CFRelease(ServerCerts); ServerCerts = NULL; }
#endif /* NO_SECURITYFRAMEWORK */
}

// This gets the text of the field currently labelled "Computer Name" in the Sharing Prefs Control Panel
mDNSlocal void GetUserSpecifiedFriendlyComputerName(domainlabel *const namelabel)
{
    CFStringEncoding encoding = kCFStringEncodingUTF8;
    CFStringRef cfs = SCDynamicStoreCopyComputerName(NULL, &encoding);
    if (cfs)
    {
        CFStringGetPascalString(cfs, namelabel->c, sizeof(*namelabel), kCFStringEncodingUTF8);
        CFRelease(cfs);
    }
}

// This gets the text of the field currently labelled "Local Hostname" in the Sharing Prefs Control Panel
mDNSlocal void GetUserSpecifiedLocalHostName(domainlabel *const namelabel)
{
    CFStringRef cfs = SCDynamicStoreCopyLocalHostName(NULL);
    if (cfs)
    {
        CFStringGetPascalString(cfs, namelabel->c, sizeof(*namelabel), kCFStringEncodingUTF8);
        CFRelease(cfs);
    }
}

mDNSexport mDNSBool DictionaryIsEnabled(CFDictionaryRef dict)
{
    mDNSs32 val;
    CFNumberRef state = (CFNumberRef)CFDictionaryGetValue(dict, CFSTR("Enabled"));
    if (!state) return mDNSfalse;
    if (!CFNumberGetValue(state, kCFNumberSInt32Type, &val))
    { LogMsg("ERROR: DictionaryIsEnabled - CFNumberGetValue"); return mDNSfalse; }
    return val ? mDNStrue : mDNSfalse;
}

mDNSlocal mStatus SetupAddr(mDNSAddr *ip, const struct sockaddr *const sa)
{
    if (!sa) { LogMsg("SetupAddr ERROR: NULL sockaddr"); return(mStatus_Invalid); }

    if (sa->sa_family == AF_INET)
    {
        struct sockaddr_in *ifa_addr = (struct sockaddr_in *)sa;
        ip->type = mDNSAddrType_IPv4;
        ip->ip.v4.NotAnInteger = ifa_addr->sin_addr.s_addr;
        return(mStatus_NoError);
    }

    if (sa->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *ifa_addr = (struct sockaddr_in6 *)sa;
        // Inside the BSD kernel they use a hack where they stuff the sin6->sin6_scope_id
        // value into the second word of the IPv6 link-local address, so they can just
        // pass around IPv6 address structures instead of full sockaddr_in6 structures.
        // Those hacked IPv6 addresses aren't supposed to escape the kernel in that form, but they do.
        // To work around this we always whack the second word of any IPv6 link-local address back to zero.
        if (IN6_IS_ADDR_LINKLOCAL(&ifa_addr->sin6_addr)) ifa_addr->sin6_addr.__u6_addr.__u6_addr16[1] = 0;
        ip->type = mDNSAddrType_IPv6;
        ip->ip.v6 = *(mDNSv6Addr*)&ifa_addr->sin6_addr;
        return(mStatus_NoError);
    }

    LogMsg("SetupAddr invalid sa_family %d", sa->sa_family);
    return(mStatus_Invalid);
}

mDNSlocal mDNSEthAddr GetBSSID(char *ifa_name)
{
    mDNSEthAddr eth = zeroEthAddr;

    CFStringRef entityname = CFStringCreateWithFormat(NULL, NULL, CFSTR("State:/Network/Interface/%s/AirPort"), ifa_name);
    if (entityname)
    {
        CFDictionaryRef dict = SCDynamicStoreCopyValue(NULL, entityname);
        if (dict)
        {
            CFRange range = { 0, 6 };       // Offset, length
            CFDataRef data = CFDictionaryGetValue(dict, CFSTR("BSSID"));
            if (data && CFDataGetLength(data) == 6)
                CFDataGetBytes(data, range, eth.b);
            CFRelease(dict);
        }
        CFRelease(entityname);
    }

    return(eth);
}

mDNSlocal int GetMAC(mDNSEthAddr *eth, u_short ifindex)
{
    struct ifaddrs *ifa;
    for (ifa = myGetIfAddrs(0); ifa; ifa = ifa->ifa_next)
        if (ifa->ifa_addr->sa_family == AF_LINK)
        {
            const struct sockaddr_dl *const sdl = (const struct sockaddr_dl *)ifa->ifa_addr;
            if (sdl->sdl_index == ifindex)
            { mDNSPlatformMemCopy(eth->b, sdl->sdl_data + sdl->sdl_nlen, 6); return 0; }
        }
    *eth = zeroEthAddr;
    return -1;
}

#ifndef SIOCGIFWAKEFLAGS
#define SIOCGIFWAKEFLAGS _IOWR('i', 136, struct ifreq) /* get interface wake property flags */
#endif

#ifndef IF_WAKE_ON_MAGIC_PACKET
#define IF_WAKE_ON_MAGIC_PACKET 0x01
#endif

#ifndef ifr_wake_flags
#define ifr_wake_flags ifr_ifru.ifru_intval
#endif

mDNSlocal mDNSBool  CheckInterfaceSupport(NetworkInterfaceInfo *const intf, const char *key)
{
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, IOBSDNameMatching(kIOMasterPortDefault, 0, intf->ifname));
    if (!service)
    {
        LogSPS("CheckInterfaceSupport: No service for interface %s", intf->ifname);
        return mDNSfalse;
    }

    io_name_t n1, n2;
    IOObjectGetClass(service, n1);
    io_object_t parent;
    mDNSBool    ret = mDNSfalse;
    kern_return_t kr = IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent);
    if (kr == KERN_SUCCESS)
    {
        CFStringRef keystr =  CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
        IOObjectGetClass(parent, n2);
        LogSPS("CheckInterfaceSupport: Interface %s service %s parent %s", intf->ifname, n1, n2);
        const CFTypeRef ref = IORegistryEntryCreateCFProperty(parent, keystr, kCFAllocatorDefault, mDNSNULL);
        if (!ref)
        {
            LogSPS("CheckInterfaceSupport: No mDNS_IOREG_KEY for interface %s/%s/%s", intf->ifname, n1, n2);
            ret = mDNSfalse;
        }
        else
        {
            ret = mDNStrue;
            CFRelease(ref);
        }
        IOObjectRelease(parent);
        CFRelease(keystr);
    }
    else
    {
        LogSPS("CheckInterfaceSupport: IORegistryEntryGetParentEntry for %s/%s failed %d", intf->ifname, n1, kr);
        ret = mDNSfalse;
    }
    IOObjectRelease(service);
    return ret;
}


mDNSlocal  mDNSBool InterfaceSupportsKeepAlive(NetworkInterfaceInfo *const intf)
{
    return CheckInterfaceSupport(intf, mDNS_IOREG_KA_KEY);
}

mDNSlocal mDNSBool NetWakeInterface(NetworkInterfaceInfoOSX *i)
{
    // We only use Sleep Proxy Service on multicast-capable interfaces, except loopback and D2D.
    if (!SPSInterface(i)) return(mDNSfalse);

    // If the interface supports TCPKeepalive, it is capable of waking up for a magic packet
    // This check is needed since the SIOCGIFWAKEFLAGS ioctl returns wrong values for WOMP capability
    // when the power source is not AC Power.
    if (InterfaceSupportsKeepAlive(&i->ifinfo))
    {
        LogSPS("NetWakeInterface: %s supports TCP Keepalive returning true", i->ifinfo.ifname);
        return mDNStrue;
    }

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) { LogMsg("NetWakeInterface socket failed %s error %d errno %d (%s)", i->ifinfo.ifname, s, errno, strerror(errno)); return(mDNSfalse); }

    struct ifreq ifr;
    strlcpy(ifr.ifr_name, i->ifinfo.ifname, sizeof(ifr.ifr_name));
    if (ioctl(s, SIOCGIFWAKEFLAGS, &ifr) < 0)
    {
        // For some strange reason, in /usr/include/sys/errno.h, EOPNOTSUPP is defined to be
        // 102 when compiling kernel code, and 45 when compiling user-level code. Since this
        // error code is being returned from the kernel, we need to use the kernel version.
        #define KERNEL_EOPNOTSUPP 102
        if (errno != KERNEL_EOPNOTSUPP) // "Operation not supported on socket", the expected result on Leopard and earlier
            LogMsg("NetWakeInterface SIOCGIFWAKEFLAGS %s errno %d (%s)", i->ifinfo.ifname, errno, strerror(errno));
        // If on Leopard or earlier, we get EOPNOTSUPP, so in that case
        // we enable WOL if this interface is not AirPort and "Wake for Network access" is turned on.
        ifr.ifr_wake_flags = (errno == KERNEL_EOPNOTSUPP && !(i)->BSSID.l[0] && i->m->SystemWakeOnLANEnabled) ? IF_WAKE_ON_MAGIC_PACKET : 0;
    }

    close(s);

    // ifr.ifr_wake_flags = IF_WAKE_ON_MAGIC_PACKET;	// For testing with MacBook Air, using a USB dongle that doesn't actually support Wake-On-LAN

    LogSPS("%-6s %#-14a %s WOMP", i->ifinfo.ifname, &i->ifinfo.ip, (ifr.ifr_wake_flags & IF_WAKE_ON_MAGIC_PACKET) ? "supports" : "no");

    return((ifr.ifr_wake_flags & IF_WAKE_ON_MAGIC_PACKET) != 0);
}

mDNSlocal u_int64_t getExtendedFlags(char * ifa_name)
{
    int sockFD;
    struct ifreq ifr;

    sockFD = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockFD < 0)
    {
        LogMsg("getExtendedFlags: socket() call failed, errno = %d (%s)", errno, strerror(errno));
        return 0;
    }

    ifr.ifr_addr.sa_family = AF_INET;
    strlcpy(ifr.ifr_name, ifa_name, sizeof(ifr.ifr_name));

    if (ioctl(sockFD, SIOCGIFEFLAGS, (caddr_t)&ifr) == -1)
    {
        LogMsg("getExtendedFlags: SIOCGIFEFLAGS failed, errno = %d (%s)", errno, strerror(errno));
        ifr.ifr_eflags = 0;
    }
    LogInfo("getExtendedFlags: %s ifr_eflags = 0x%x", ifa_name, ifr.ifr_eflags);

    close(sockFD);
    return ifr.ifr_eflags;
}

#if TARGET_OS_IPHONE

// Function pointers for the routines we use in the MobileWiFi framework.
static WiFiManagerClientRef (*WiFiManagerClientCreate_p)(CFAllocatorRef allocator, WiFiClientType type) = mDNSNULL;
static CFArrayRef (*WiFiManagerClientCopyDevices_p)(WiFiManagerClientRef manager) = mDNSNULL;
static WiFiNetworkRef (*WiFiDeviceClientCopyCurrentNetwork_p)(WiFiDeviceClientRef device) = mDNSNULL;
static bool (*WiFiNetworkIsCarPlay_p)(WiFiNetworkRef network) = mDNSNULL;

mDNSlocal mDNSBool MobileWiFiLibLoad(void)
{
    static mDNSBool isInitialized = mDNSfalse;
    static void *MobileWiFiLib_p = mDNSNULL;
    static const char path[] = "/System/Library/PrivateFrameworks/MobileWiFi.framework/MobileWiFi";

    if (!isInitialized)
    {
        if (!MobileWiFiLib_p)
        {
            MobileWiFiLib_p = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
            if (!MobileWiFiLib_p)
            {
                LogInfo("MobileWiFiLibLoad: dlopen() failed.");
                goto exit;
            }
        }

        if (!WiFiManagerClientCreate_p)
        {
            WiFiManagerClientCreate_p = dlsym(MobileWiFiLib_p, "WiFiManagerClientCreate");
            if (!WiFiManagerClientCreate_p)
            {
                LogInfo("MobileWiFiLibLoad: load of WiFiManagerClientCreate symbol failed.");
                goto exit;
            }
        }

        if (!WiFiManagerClientCopyDevices_p)
        {
            WiFiManagerClientCopyDevices_p = dlsym(MobileWiFiLib_p, "WiFiManagerClientCopyDevices");
            if (!WiFiManagerClientCopyDevices_p)
            {
                LogInfo("MobileWiFiLibLoad: load of WiFiManagerClientCopyDevices symbol failed.");
                goto exit;
            }
        }

        if (!WiFiDeviceClientCopyCurrentNetwork_p)
        {
            WiFiDeviceClientCopyCurrentNetwork_p = dlsym(MobileWiFiLib_p, "WiFiDeviceClientCopyCurrentNetwork");
            if (!WiFiDeviceClientCopyCurrentNetwork_p)
            {
                LogInfo("MobileWiFiLibLoad: load of WiFiDeviceClientCopyCurrentNetwork symbol failed.");
                goto exit;
            }
        }

        if (!WiFiNetworkIsCarPlay_p)
        {
            WiFiNetworkIsCarPlay_p = dlsym(MobileWiFiLib_p, "WiFiNetworkIsCarPlay");
            if (!WiFiNetworkIsCarPlay_p)
            {
                LogInfo("MobileWiFiLibLoad: load of WiFiNetworkIsCarPlay symbol failed.");
                goto exit;
            }
        }

        isInitialized = mDNStrue;
    }

exit:
    return isInitialized;
}

// Return true if the interface is associate to a CarPlay hosted SSID.
mDNSlocal mDNSBool IsCarPlaySSID(char *ifa_name)
{
    static WiFiManagerClientRef manager = NULL;
    mDNSBool rvalue = mDNSfalse;

    if (!MobileWiFiLibLoad())
        return mDNSfalse;

    // If we have associated with a CarPlay hosted SSID, then use the same 
    // optimizations that are used if an interface has the IFEF_DIRECTLINK flag set.

    // Get one WiFiManagerClientRef to use for all calls.
    if (manager == NULL)
        manager = WiFiManagerClientCreate_p(NULL, kWiFiClientTypeNormal);

    if (manager == NULL)
    {
        LogInfo("IsCarPlaySSID: WiFiManagerClientCreate() failed!");
    }
    else
    {
        CFArrayRef      devices;

        devices = WiFiManagerClientCopyDevices_p(manager);
        if (devices != NULL)
        {
            WiFiDeviceClientRef     device;
            WiFiNetworkRef          network;

            device = (WiFiDeviceClientRef)CFArrayGetValueAtIndex(devices, 0);
            network = WiFiDeviceClientCopyCurrentNetwork_p(device);
            if (network != NULL)
            {
                if (WiFiNetworkIsCarPlay_p(network))
                {
                    LogInfo("%s is CarPlay hosted", ifa_name);
                    rvalue = mDNStrue;
                }
                CFRelease(network);
            }
            CFRelease(devices);
        }
    }

    return rvalue;
}

#else   // TARGET_OS_IPHONE

mDNSlocal mDNSBool IsCarPlaySSID(char *ifa_name)
{
    (void)ifa_name;  // unused

    // OSX WifiManager currently does not implement WiFiNetworkIsCarPlay()
    return mDNSfalse;;
}

#endif  // TARGET_OS_IPHONE

// Returns pointer to newly created NetworkInterfaceInfoOSX object, or
// pointer to already-existing NetworkInterfaceInfoOSX object found in list, or
// may return NULL if out of memory (unlikely) or parameters are invalid for some reason
// (e.g. sa_family not AF_INET or AF_INET6)
mDNSlocal NetworkInterfaceInfoOSX *AddInterfaceToList(mDNS *const m, struct ifaddrs *ifa, mDNSs32 utc)
{
    mDNSu32 scope_id  = if_nametoindex(ifa->ifa_name);
    mDNSEthAddr bssid = GetBSSID(ifa->ifa_name);
    u_int64_t   eflags = getExtendedFlags(ifa->ifa_name);

    mDNSAddr ip, mask;
    if (SetupAddr(&ip,   ifa->ifa_addr   ) != mStatus_NoError) return(NULL);
    if (SetupAddr(&mask, ifa->ifa_netmask) != mStatus_NoError) return(NULL);

    NetworkInterfaceInfoOSX **p;
    for (p = &m->p->InterfaceList; *p; p = &(*p)->next)
        if (scope_id == (*p)->scope_id &&
            mDNSSameAddress(&ip, &(*p)->ifinfo.ip) &&
            mDNSSameEthAddress(&bssid, &(*p)->BSSID))
        {
            debugf("AddInterfaceToList: Found existing interface %lu %.6a with address %#a at %p, ifname before %s, after %s", scope_id, &bssid, &ip, *p, (*p)->ifinfo.ifname, ifa->ifa_name);
            // The name should be updated to the new name so that we don't report a wrong name in our SIGINFO output.
            // When interfaces are created with same MAC address, kernel resurrects the old interface.
            // Even though the interface index is the same (which should be sufficient), when we receive a UDP packet
            // we get the corresponding name for the interface index on which the packet was received and check against
            // the InterfaceList for a matching name. So, keep the name in sync
            strlcpy((*p)->ifinfo.ifname, ifa->ifa_name, sizeof((*p)->ifinfo.ifname));
            (*p)->Exists = mDNStrue;
            // If interface was not in getifaddrs list last time we looked, but it is now, update 'AppearanceTime' for this record
            if ((*p)->LastSeen != utc) (*p)->AppearanceTime = utc;

            // If Wake-on-LAN capability of this interface has changed (e.g. because power cable on laptop has been disconnected)
            // we may need to start or stop or sleep proxy browse operation
            const mDNSBool NetWake = NetWakeInterface(*p);
            if ((*p)->ifinfo.NetWake != NetWake)
            {
                (*p)->ifinfo.NetWake = NetWake;
                // If this interface is already registered with mDNSCore, then we need to start or stop its NetWake browse on-the-fly.
                // If this interface is not already registered (i.e. it's a dormant interface we had in our list
                // from when we previously saw it) then we mustn't do that, because mDNSCore doesn't know about it yet.
                // In this case, the mDNS_RegisterInterface() call will take care of starting the NetWake browse if necessary.
                if ((*p)->Registered)
                {
                    mDNS_Lock(m);
                    if (NetWake) mDNS_ActivateNetWake_internal  (m, &(*p)->ifinfo);
                    else         mDNS_DeactivateNetWake_internal(m, &(*p)->ifinfo);
                    mDNS_Unlock(m);
                }
            }
            // Reset the flag if it has changed this time.
            (*p)->ifinfo.IgnoreIPv4LL = ((eflags & IFEF_ARPLL) != 0) ? mDNSfalse : mDNStrue;

            return(*p);
        }

    NetworkInterfaceInfoOSX *i = (NetworkInterfaceInfoOSX *)mallocL("NetworkInterfaceInfoOSX", sizeof(*i));
    debugf("AddInterfaceToList: Making   new   interface %lu %.6a with address %#a at %p", scope_id, &bssid, &ip, i);
    if (!i) return(mDNSNULL);
    mDNSPlatformMemZero(i, sizeof(NetworkInterfaceInfoOSX));
    i->ifinfo.InterfaceID = (mDNSInterfaceID)(uintptr_t)scope_id;
    i->ifinfo.ip          = ip;
    i->ifinfo.mask        = mask;
    strlcpy(i->ifinfo.ifname, ifa->ifa_name, sizeof(i->ifinfo.ifname));
    i->ifinfo.ifname[sizeof(i->ifinfo.ifname)-1] = 0;
    // We can be configured to disable multicast advertisement, but we want to to support
    // local-only services, which need a loopback address record.
    i->ifinfo.Advertise   = m->DivertMulticastAdvertisements ? ((ifa->ifa_flags & IFF_LOOPBACK) ? mDNStrue : mDNSfalse) : m->AdvertiseLocalAddresses;
    i->ifinfo.McastTxRx   = mDNSfalse; // For now; will be set up later at the end of UpdateInterfaceList
    i->ifinfo.Loopback    = ((ifa->ifa_flags & IFF_LOOPBACK) != 0) ? mDNStrue : mDNSfalse;
    i->ifinfo.IgnoreIPv4LL = ((eflags & IFEF_ARPLL) != 0) ? mDNSfalse : mDNStrue;

    // Setting DirectLink indicates we can do the optimization of skipping the probe phase 
    // for the interface address records since they should be unique.
    if (eflags & IFEF_DIRECTLINK)
        i->ifinfo.DirectLink  = mDNStrue;
    else
        i->ifinfo.DirectLink  = IsCarPlaySSID(ifa->ifa_name);

    i->next            = mDNSNULL;
    i->m               = m;
    i->Exists          = mDNStrue;
    i->Flashing        = mDNSfalse;
    i->Occulting       = mDNSfalse;
    i->D2DInterface    = (eflags & IFEF_LOCALNET_PRIVATE) ? mDNStrue: mDNSfalse;
    if (eflags & IFEF_AWDL)
    {
        // Set SupportsUnicastMDNSResponse false for the AWDL interface since unicast reserves
        // limited AWDL resources so we don't set the kDNSQClass_UnicastResponse bit in
        // Bonjour requests over the AWDL interface.
        i->ifinfo.SupportsUnicastMDNSResponse = mDNSfalse;
        AWDLInterfaceID = i->ifinfo.InterfaceID;
        i->ifinfo.DirectLink  = mDNStrue;
        LogInfo("AddInterfaceToList: AWDLInterfaceID = %d", (int) AWDLInterfaceID);
    }
    else
    {
        i->ifinfo.SupportsUnicastMDNSResponse = mDNStrue;
    }
    i->AppearanceTime  = utc;       // Brand new interface; AppearanceTime is now
    i->LastSeen        = utc;
    i->ifa_flags       = ifa->ifa_flags;
    i->scope_id        = scope_id;
    i->BSSID           = bssid;
    i->sa_family       = ifa->ifa_addr->sa_family;
    i->BPF_fd          = -1;
    i->BPF_mcfd        = -1;
    i->BPF_len         = 0;
    i->Registered      = mDNSNULL;

    // Do this AFTER i->BSSID has been set up
    i->ifinfo.NetWake  = (eflags & IFEF_EXPENSIVE)? mDNSfalse :  NetWakeInterface(i);
    GetMAC(&i->ifinfo.MAC, scope_id);
    if (i->ifinfo.NetWake && !i->ifinfo.MAC.l[0])
        LogMsg("AddInterfaceToList: Bad MAC address %.6a for %d %s %#a", &i->ifinfo.MAC, scope_id, i->ifinfo.ifname, &ip);

    *p = i;
    return(i);
}

#if APPLE_OSX_mDNSResponder

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - AutoTunnel
#endif

#define kRacoonPort 4500

static DomainAuthInfo* AnonymousRacoonConfig = mDNSNULL;

#ifndef NO_SECURITYFRAMEWORK

static CFMutableDictionaryRef domainStatusDict = NULL;

mDNSlocal mStatus CheckQuestionForStatus(const DNSQuestion *const q)
{
    if (q->LongLived)
    {
        if (q->servAddr.type == mDNSAddrType_IPv4 && mDNSIPv4AddressIsOnes(q->servAddr.ip.v4))
            return mStatus_NoSuchRecord;
        else if (q->state == LLQ_Poll)
            return mStatus_PollingMode;
        else if (q->state != LLQ_Established && !q->DuplicateOf)
            return mStatus_TransientErr;
    }

    return mStatus_NoError;
}

mDNSlocal mStatus UpdateLLQStatus(const mDNS *const m, char *buffer, int bufsz, const DomainAuthInfo *const info)
{
    mStatus status = mStatus_NoError;
    DNSQuestion* q, *worst_q = mDNSNULL;
    for (q = m->Questions; q; q=q->next)
        if (q->AuthInfo == info)
        {
            mStatus newStatus = CheckQuestionForStatus(q);
            if      (newStatus == mStatus_NoSuchRecord) { status = newStatus; worst_q = q; break; }
            else if (newStatus == mStatus_PollingMode)  { status = newStatus; worst_q = q; }
            else if (newStatus == mStatus_TransientErr && status == mStatus_NoError) { status = newStatus; worst_q = q; }
        }

    if      (status == mStatus_NoError) mDNS_snprintf(buffer, bufsz, "Success");
    else if (status == mStatus_NoSuchRecord) mDNS_snprintf(buffer, bufsz, "GetZoneData %s: %##s", worst_q->nta ? "not yet complete" : "failed", worst_q->qname.c);
    else if (status == mStatus_PollingMode) mDNS_snprintf(buffer, bufsz, "Query polling %##s", worst_q->qname.c);
    else if (status == mStatus_TransientErr) mDNS_snprintf(buffer, bufsz, "Query not yet established %##s", worst_q->qname.c);
    return status;
}

mDNSlocal mStatus UpdateRRStatus(const mDNS *const m, char *buffer, int bufsz, const DomainAuthInfo *const info)
{
    AuthRecord *r;

    if (info->deltime) return mStatus_NoError;
    for (r = m->ResourceRecords; r; r = r->next)
    {
        // This function is called from UpdateAutoTunnelDomainStatus which in turn may be called from
        // a callback e.g., CheckNATMappings. GetAuthInfoFor_internal does not like that (reentrancy being 1),
        // hence we inline the code here. We just need the lock to walk the list of AuthInfos which the caller
        // has already checked
        const domainname *n = r->resrec.name;
        while (n->c[0])
        {
            DomainAuthInfo *ptr;
            for (ptr = m->AuthInfoList; ptr; ptr = ptr->next)
                if (SameDomainName(&ptr->domain, n))
                {
                    if (ptr == info && (r->updateError == mStatus_BadSig || r->updateError == mStatus_BadKey || r->updateError == mStatus_BadTime))
                    {
                        mDNS_snprintf(buffer, bufsz, "Resource record update failed for %##s", r->resrec.name);
                        return r->updateError;
                    }
                }
            n = (const domainname *)(n->c + 1 + n->c[0]);
        }
    }
    return mStatus_NoError;
}

#endif // ndef NO_SECURITYFRAMEWORK

// MUST be called with lock held
mDNSlocal void UpdateAutoTunnelDomainStatus(const mDNS *const m, const DomainAuthInfo *const info)
{
#ifdef NO_SECURITYFRAMEWORK
        (void) m;
    (void)info;
#else
    // Note that in the LLQNAT, the clientCallback being non-zero means it's in use,
    // whereas in the AutoTunnelNAT, the clientContext being non-zero means it's in use
    const NATTraversalInfo *const llq = m->LLQNAT.clientCallback ? &m->LLQNAT : mDNSNULL;
    const NATTraversalInfo *const tun = m->AutoTunnelNAT.clientContext ? &m->AutoTunnelNAT : mDNSNULL;
    char buffer[1024];
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFStringRef domain = NULL;
    CFStringRef tmp = NULL;
    CFNumberRef num = NULL;
    mStatus status = mStatus_NoError;
    mStatus llqStatus = mStatus_NoError;
    char llqBuffer[1024];

    mDNS_CheckLock(m);

    if (!domainStatusDict)
    {
        domainStatusDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (!domainStatusDict) { LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFDictionary domainStatusDict"); return; }
    }

    if (!dict) { LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFDictionary dict"); return; }

    mDNS_snprintf(buffer, sizeof(buffer), "%##s", info->domain.c);
    domain = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
    if (!domain) { LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFString domain"); return; }

    if (info->deltime)
    {
        if (CFDictionaryContainsKey(domainStatusDict, domain))
        {
            CFDictionaryRemoveValue(domainStatusDict, domain);
            if (!m->ShutdownTime) mDNSDynamicStoreSetConfig(kmDNSBackToMyMacConfig, mDNSNULL, domainStatusDict);
        }
        CFRelease(domain);
        CFRelease(dict);

        return;
    }

    mDNS_snprintf(buffer, sizeof(buffer), "%#a", &m->Router);
    tmp = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
    if (!tmp)
        LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFString RouterAddress");
    else
    {
        CFDictionarySetValue(dict, CFSTR("RouterAddress"), tmp);
        CFRelease(tmp);
    }

    if (llq)
    {
        mDNSu32 port = mDNSVal16(llq->ExternalPort);

        num = CFNumberCreate(NULL, kCFNumberSInt32Type, &port);
        if (!num)
            LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFNumber LLQExternalPort");
        else
        {
            CFDictionarySetValue(dict, CFSTR("LLQExternalPort"), num);
            CFRelease(num);
        }

        if (llq->Result)
        {
            num = CFNumberCreate(NULL, kCFNumberSInt32Type, &llq->Result);
            if (!num)
                LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFNumber LLQNPMStatus");
            else
            {
                CFDictionarySetValue(dict, CFSTR("LLQNPMStatus"), num);
                CFRelease(num);
            }
        }
    }

    if (tun)
    {
        mDNSu32 port = mDNSVal16(tun->ExternalPort);

        num = CFNumberCreate(NULL, kCFNumberSInt32Type, &port);
        if (!num)
            LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFNumber AutoTunnelExternalPort");
        else
        {
            CFDictionarySetValue(dict, CFSTR("AutoTunnelExternalPort"), num);
            CFRelease(num);
        }

        mDNS_snprintf(buffer, sizeof(buffer), "%.4a", &tun->ExternalAddress);
        tmp = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
        if (!tmp)
            LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFString ExternalAddress");
        else
        {
            CFDictionarySetValue(dict, CFSTR("ExternalAddress"), tmp);
            CFRelease(tmp);
        }

        if (tun->Result)
        {
            num = CFNumberCreate(NULL, kCFNumberSInt32Type, &tun->Result);
            if (!num)
                LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFNumber AutoTunnelNPMStatus");
            else
            {
                CFDictionarySetValue(dict, CFSTR("AutoTunnelNPMStatus"), num);
                CFRelease(num);
            }
        }
    }
    if (tun || llq)
    {
        mDNSu32 code = m->LastNATMapResultCode;

        num = CFNumberCreate(NULL, kCFNumberSInt32Type, &code);
        if (!num)
            LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFNumber LastNATMapResultCode");
        else
        {
            CFDictionarySetValue(dict, CFSTR("LastNATMapResultCode"), num);
            CFRelease(num);
        }
    }

    mDNS_snprintf(buffer, sizeof(buffer), "Success");
    llqStatus = UpdateLLQStatus(m, llqBuffer, sizeof(llqBuffer), info);
    status = UpdateRRStatus(m, buffer, sizeof(buffer), info);

    // If we have a bad signature error updating a RR, it overrides any error as it needs to be
    // reported so that it can be fixed automatically (or the user needs to be notified)
    if (status != mStatus_NoError)
    {
        LogInfo("UpdateAutoTunnelDomainStatus: RR Status %d, %s", status, buffer);
    }
    else if (m->Router.type == mDNSAddrType_None)
    {
        status = mStatus_NoRouter;
        mDNS_snprintf(buffer, sizeof(buffer), "No network connection - none");
    }
    else if (m->Router.type == mDNSAddrType_IPv4 && mDNSIPv4AddressIsZero(m->Router.ip.v4))
    {
        status = mStatus_NoRouter;
        mDNS_snprintf(buffer, sizeof(buffer), "No network connection - v4 zero");
    }
    else if (mDNSIPv6AddressIsZero(info->AutoTunnelInnerAddress))
    {
        status = mStatus_ServiceNotRunning;
        mDNS_snprintf(buffer, sizeof(buffer), "No inner address");
    }
    else if (!llq && !tun)
    {
        status = mStatus_NotInitializedErr;
        mDNS_snprintf(buffer, sizeof(buffer), "Neither LLQ nor AutoTunnel NAT port mapping is currently active");
    }
    else if (llqStatus == mStatus_NoSuchRecord)
    {
        status = llqStatus;
        mDNS_snprintf(buffer, sizeof(buffer), "%s", llqBuffer);
    }
    else if ((llq && llq->Result == mStatus_DoubleNAT) || (tun && tun->Result == mStatus_DoubleNAT))
    {
        status = mStatus_DoubleNAT;
        mDNS_snprintf(buffer, sizeof(buffer), "Double NAT: Router is reporting a private address");
    }
    else if ((llq && llq->Result == mStatus_NATPortMappingDisabled) ||
             (tun && tun->Result == mStatus_NATPortMappingDisabled) ||
             (m->LastNATMapResultCode == NATErr_Refused && ((llq && !llq->Result && mDNSIPPortIsZero(llq->ExternalPort)) || (tun && !tun->Result && mDNSIPPortIsZero(tun->ExternalPort)))))
    {
        status = mStatus_NATPortMappingDisabled;
        mDNS_snprintf(buffer, sizeof(buffer), "PCP/NAT-PMP is disabled on the router");
    }
    else if ((llq && llq->Result) || (tun && tun->Result))
    {
        status = mStatus_NATTraversal;
        mDNS_snprintf(buffer, sizeof(buffer), "Error obtaining NAT port mapping from router");
    }
    else if ((llq && mDNSIPPortIsZero(llq->ExternalPort)) || (tun && mDNSIPPortIsZero(tun->ExternalPort)))
    {
        status = mStatus_NATTraversal;
        mDNS_snprintf(buffer, sizeof(buffer), "Unable to obtain NAT port mapping from router");
    }
    else
    {
        status = llqStatus;
        mDNS_snprintf(buffer, sizeof(buffer), "%s", llqBuffer);
        LogInfo("UpdateAutoTunnelDomainStatus: LLQ Status %d, %s", status, buffer);
    }

    num = CFNumberCreate(NULL, kCFNumberSInt32Type, &status);
    if (!num)
        LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFNumber StatusCode");
    else
    {
        CFDictionarySetValue(dict, CFSTR("StatusCode"), num);
        CFRelease(num);
    }

    tmp = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
    if (!tmp)
        LogMsg("UpdateAutoTunnelDomainStatus: Could not create CFString StatusMessage");
    else
    {
        CFDictionarySetValue(dict, CFSTR("StatusMessage"), tmp);
        CFRelease(tmp);
    }

    if (!CFDictionaryContainsKey(domainStatusDict, domain) ||
        !CFEqual(dict, (CFMutableDictionaryRef)CFDictionaryGetValue(domainStatusDict, domain)))
    {
        CFDictionarySetValue(domainStatusDict, domain, dict);
        if (!m->ShutdownTime)
        {
            static char statusBuf[16];
            mDNS_snprintf(statusBuf, sizeof(statusBuf), "%d", (int)status);
            mDNSASLLog((uuid_t *)&m->asl_uuid, "autotunnel.domainstatus", status ? "failure" : "success", statusBuf, "");
            mDNSDynamicStoreSetConfig(kmDNSBackToMyMacConfig, mDNSNULL, domainStatusDict);
        }
    }

    CFRelease(domain);
    CFRelease(dict);

    debugf("UpdateAutoTunnelDomainStatus: %s", buffer);
#endif // def NO_SECURITYFRAMEWORK
}

// MUST be called with lock held
mDNSexport void UpdateAutoTunnelDomainStatuses(const mDNS *const m)
{
#ifdef NO_SECURITYFRAMEWORK
        (void) m;
#else
    mDNS_CheckLock(m);
    DomainAuthInfo* info;
    for (info = m->AuthInfoList; info; info = info->next)
        if (info->AutoTunnel && !info->deltime)
            UpdateAutoTunnelDomainStatus(m, info);
#endif // def NO_SECURITYFRAMEWORK
}

mDNSlocal void UpdateAnonymousRacoonConfig(mDNS *m)     // Determine whether we need racoon to accept incoming connections
{
    DomainAuthInfo *info;

    for (info = m->AuthInfoList; info; info = info->next)
        if (info->AutoTunnel && !info->deltime && (!mDNSIPPortIsZero(m->AutoTunnelNAT.ExternalPort) || !mDNSIPv6AddressIsZero(m->AutoTunnelRelayAddr)))
            break;

    if (info != AnonymousRacoonConfig)
    {
        AnonymousRacoonConfig = info;
        LogInfo("UpdateAnonymousRacoonConfig need not be done in mDNSResponder");
    }
}

mDNSlocal void AutoTunnelRecordCallback(mDNS *const m, AuthRecord *const rr, mStatus result);

// Caller must hold the lock
mDNSlocal mDNSBool DeregisterAutoTunnelRecord(mDNS *m, DomainAuthInfo *info, AuthRecord* record)
{
    mDNS_CheckLock(m);

    LogInfo("DeregisterAutoTunnelRecord %##s %##s", &info->domain.c, record->namestorage.c);

    if (record->resrec.RecordType > kDNSRecordTypeDeregistering)
    {
        mStatus err = mDNS_Deregister_internal(m, record, mDNS_Dereg_normal);
        if (err)
        {
            record->resrec.RecordType = kDNSRecordTypeUnregistered;
            LogMsg("DeregisterAutoTunnelRecord error %d deregistering %##s %##s", err, info->domain.c, record->namestorage.c);
            return mDNSfalse;
        }
        else LogInfo("DeregisterAutoTunnelRecord: Deregistered");
    }
    else LogInfo("DeregisterAutoTunnelRecord: Not deregistering, state:%d", record->resrec.RecordType);

    return mDNStrue;
}

// Caller must hold the lock
mDNSlocal void DeregisterAutoTunnelHostRecord(mDNS *m, DomainAuthInfo *info)
{
    if (!DeregisterAutoTunnelRecord(m, info, &info->AutoTunnelHostRecord))
    {
        info->AutoTunnelHostRecord.namestorage.c[0] = 0;
        m->NextSRVUpdate = NonZeroTime(m->timenow);
    }
}

// Caller must hold the lock
mDNSlocal void UpdateAutoTunnelHostRecord(mDNS *m, DomainAuthInfo *info)
{
    mStatus err;
    mDNSBool NATProblem = mDNSIPPortIsZero(m->AutoTunnelNAT.ExternalPort) || m->AutoTunnelNAT.Result;

    mDNS_CheckLock(m);

    if (!info->AutoTunnelServiceStarted || info->deltime || m->ShutdownTime || mDNSIPv6AddressIsZero(info->AutoTunnelInnerAddress) || (m->SleepState != SleepState_Awake && NATProblem))
    {
        LogInfo("UpdateAutoTunnelHostRecord: Dereg %##s : AutoTunnelServiceStarted(%d) deltime(%d) address(%.16a) sleepstate(%d)",
                info->domain.c, info->AutoTunnelServiceStarted, info->deltime, &info->AutoTunnelInnerAddress, m->SleepState);
        DeregisterAutoTunnelHostRecord(m, info);
    }
    else if (info->AutoTunnelHostRecord.resrec.RecordType == kDNSRecordTypeUnregistered)
    {
        mDNS_SetupResourceRecord(&info->AutoTunnelHostRecord, mDNSNULL, mDNSInterface_Any, kDNSType_AAAA, kHostNameTTL,
                                 kDNSRecordTypeUnregistered, AuthRecordAny, AutoTunnelRecordCallback, info);
        info->AutoTunnelHostRecord.namestorage.c[0] = 0;
        AppendDomainLabel(&info->AutoTunnelHostRecord.namestorage, &m->hostlabel);
        AppendDomainName (&info->AutoTunnelHostRecord.namestorage, &info->domain);
        info->AutoTunnelHostRecord.resrec.rdata->u.ipv6 = info->AutoTunnelInnerAddress;
        info->AutoTunnelHostRecord.resrec.RecordType = kDNSRecordTypeKnownUnique;

        err = mDNS_Register_internal(m, &info->AutoTunnelHostRecord);
        if (err) LogMsg("UpdateAutoTunnelHostRecord error %d registering %##s", err, info->AutoTunnelHostRecord.namestorage.c);
        else
        {
            // Make sure we trigger the registration of all SRV records in regState_NoTarget again
            m->NextSRVUpdate = NonZeroTime(m->timenow);
            LogInfo("UpdateAutoTunnelHostRecord registering %##s", info->AutoTunnelHostRecord.namestorage.c);
        }
    }
    else LogInfo("UpdateAutoTunnelHostRecord: Type %d", info->AutoTunnelHostRecord.resrec.RecordType);
}

// Caller must hold the lock
mDNSlocal void DeregisterAutoTunnelServiceRecords(mDNS *m, DomainAuthInfo *info)
{
    LogInfo("DeregisterAutoTunnelServiceRecords %##s", info->domain.c);

    DeregisterAutoTunnelRecord(m, info, &info->AutoTunnelTarget);
    DeregisterAutoTunnelRecord(m, info, &info->AutoTunnelService);
    UpdateAutoTunnelHostRecord(m, info);
}

// Caller must hold the lock
mDNSlocal void UpdateAutoTunnelServiceRecords(mDNS *m, DomainAuthInfo *info)
{
    mDNS_CheckLock(m);

    if (!info->AutoTunnelServiceStarted || info->deltime || m->ShutdownTime || mDNSIPPortIsZero(m->AutoTunnelNAT.ExternalPort) || m->AutoTunnelNAT.Result)
    {
        LogInfo("UpdateAutoTunnelServiceRecords: Dereg %##s : AutoTunnelServiceStarted(%d) deltime(%d) ExtPort(%d) NATResult(%d)", info->domain.c, info->AutoTunnelServiceStarted, info->deltime, mDNSVal16(m->AutoTunnelNAT.ExternalPort), m->AutoTunnelNAT.Result);
        DeregisterAutoTunnelServiceRecords(m, info);
    }
    else
    {
        if (info->AutoTunnelTarget.resrec.RecordType == kDNSRecordTypeUnregistered)
        {
            // 1. Set up our address record for the external tunnel address
            // (Constructed name, not generally user-visible, used as target in IKE tunnel's SRV record)
            mDNS_SetupResourceRecord(&info->AutoTunnelTarget, mDNSNULL, mDNSInterface_Any, kDNSType_A, kHostNameTTL,
                                     kDNSRecordTypeUnregistered, AuthRecordAny, AutoTunnelRecordCallback, info);
            AssignDomainName (&info->AutoTunnelTarget.namestorage, (const domainname*) "\x0B" "_autotunnel");
            AppendDomainLabel(&info->AutoTunnelTarget.namestorage, &m->hostlabel);
            AppendDomainName (&info->AutoTunnelTarget.namestorage, &info->domain);
            info->AutoTunnelTarget.resrec.rdata->u.ipv4 = m->AutoTunnelNAT.ExternalAddress;
            info->AutoTunnelTarget.resrec.RecordType = kDNSRecordTypeKnownUnique;

            mStatus err = mDNS_Register_internal(m, &info->AutoTunnelTarget);
            if (err) LogMsg("UpdateAutoTunnelServiceRecords error %d registering %##s", err, info->AutoTunnelTarget.namestorage.c);
            else LogInfo("UpdateAutoTunnelServiceRecords registering %##s", info->AutoTunnelTarget.namestorage.c);
        }
        else LogInfo("UpdateAutoTunnelServiceRecords: NOOP Target state(%d)", info->AutoTunnelTarget.resrec.RecordType);

        if (info->AutoTunnelService.resrec.RecordType == kDNSRecordTypeUnregistered)
        {
            // 2. Set up IKE tunnel's SRV record: _autotunnel._udp.AutoTunnelHost SRV 0 0 port AutoTunnelTarget
            mDNS_SetupResourceRecord(&info->AutoTunnelService, mDNSNULL, mDNSInterface_Any, kDNSType_SRV,  kHostNameTTL,
                                     kDNSRecordTypeUnregistered, AuthRecordAny, AutoTunnelRecordCallback, info);
            AssignDomainName (&info->AutoTunnelService.namestorage, (const domainname*) "\x0B" "_autotunnel" "\x04" "_udp");
            AppendDomainLabel(&info->AutoTunnelService.namestorage, &m->hostlabel);
            AppendDomainName (&info->AutoTunnelService.namestorage, &info->domain);
            info->AutoTunnelService.resrec.rdata->u.srv.priority = 0;
            info->AutoTunnelService.resrec.rdata->u.srv.weight   = 0;
            info->AutoTunnelService.resrec.rdata->u.srv.port     = m->AutoTunnelNAT.ExternalPort;
            AssignDomainName(&info->AutoTunnelService.resrec.rdata->u.srv.target, &info->AutoTunnelTarget.namestorage);
            info->AutoTunnelService.resrec.RecordType = kDNSRecordTypeKnownUnique;

            mStatus err = mDNS_Register_internal(m, &info->AutoTunnelService);
            if (err) LogMsg("UpdateAutoTunnelServiceRecords error %d registering %##s", err, info->AutoTunnelService.namestorage.c);
            else LogInfo("UpdateAutoTunnelServiceRecords registering %##s", info->AutoTunnelService.namestorage.c);
        }
        else LogInfo("UpdateAutoTunnelServiceRecords: NOOP Service state(%d)", info->AutoTunnelService.resrec.RecordType);

        UpdateAutoTunnelHostRecord(m, info);

        LogInfo("AutoTunnel server listening for connections on %##s[%.4a]:%d:%##s[%.16a]",
                info->AutoTunnelTarget.namestorage.c,     &m->AdvertisedV4.ip.v4, mDNSVal16(m->AutoTunnelNAT.IntPort),
                info->AutoTunnelHostRecord.namestorage.c, &info->AutoTunnelInnerAddress);

    }
}

// Caller must hold the lock
mDNSlocal void DeregisterAutoTunnelDeviceInfoRecord(mDNS *m, DomainAuthInfo *info)
{
    DeregisterAutoTunnelRecord(m, info, &info->AutoTunnelDeviceInfo);
}

// Caller must hold the lock
mDNSlocal void UpdateAutoTunnelDeviceInfoRecord(mDNS *m, DomainAuthInfo *info)
{
    mDNS_CheckLock(m);

    if (!info->AutoTunnelServiceStarted || info->deltime || m->ShutdownTime)
        DeregisterAutoTunnelDeviceInfoRecord(m, info);
    else if (info->AutoTunnelDeviceInfo.resrec.RecordType == kDNSRecordTypeUnregistered)
    {
        mDNS_SetupResourceRecord(&info->AutoTunnelDeviceInfo, mDNSNULL, mDNSInterface_Any, kDNSType_TXT,  kStandardTTL, kDNSRecordTypeUnregistered, AuthRecordAny, AutoTunnelRecordCallback, info);
        ConstructServiceName(&info->AutoTunnelDeviceInfo.namestorage, &m->nicelabel, &DeviceInfoName, &info->domain);

        info->AutoTunnelDeviceInfo.resrec.rdlength = initializeDeviceInfoTXT(m, info->AutoTunnelDeviceInfo.resrec.rdata->u.data);
        info->AutoTunnelDeviceInfo.resrec.RecordType = kDNSRecordTypeKnownUnique;

        mStatus err = mDNS_Register_internal(m, &info->AutoTunnelDeviceInfo);
        if (err) LogMsg("UpdateAutoTunnelDeviceInfoRecord error %d registering %##s", err, info->AutoTunnelDeviceInfo.namestorage.c);
        else LogInfo("UpdateAutoTunnelDeviceInfoRecord registering %##s", info->AutoTunnelDeviceInfo.namestorage.c);
    }
    else
        LogInfo("UpdateAutoTunnelDeviceInfoRecord: not in Unregistered state: %d",info->AutoTunnelDeviceInfo.resrec.RecordType);
}

// Caller must hold the lock
mDNSlocal void DeregisterAutoTunnel6Record(mDNS *m, DomainAuthInfo *info)
{
    LogInfo("DeregisterAutoTunnel6Record %##s", info->domain.c);

    DeregisterAutoTunnelRecord(m, info, &info->AutoTunnel6Record);
    UpdateAutoTunnelHostRecord(m, info);
    UpdateAutoTunnelDomainStatus(m, info);
}

// Caller must hold the lock
mDNSlocal void UpdateAutoTunnel6Record(mDNS *m, DomainAuthInfo *info)
{
    mDNS_CheckLock(m);

    if (!info->AutoTunnelServiceStarted || info->deltime || m->ShutdownTime || mDNSIPv6AddressIsZero(m->AutoTunnelRelayAddr) || m->SleepState != SleepState_Awake)
        DeregisterAutoTunnel6Record(m, info);
    else if (info->AutoTunnel6Record.resrec.RecordType == kDNSRecordTypeUnregistered)
    {
        mDNS_SetupResourceRecord(&info->AutoTunnel6Record, mDNSNULL, mDNSInterface_Any, kDNSType_AAAA, kHostNameTTL,
                                 kDNSRecordTypeUnregistered, AuthRecordAny, AutoTunnelRecordCallback, info);
        AssignDomainName (&info->AutoTunnel6Record.namestorage, (const domainname*) "\x0C" "_autotunnel6");
        AppendDomainLabel(&info->AutoTunnel6Record.namestorage, &m->hostlabel);
        AppendDomainName (&info->AutoTunnel6Record.namestorage, &info->domain);
        info->AutoTunnel6Record.resrec.rdata->u.ipv6 = m->AutoTunnelRelayAddr;
        info->AutoTunnel6Record.resrec.RecordType = kDNSRecordTypeKnownUnique;

        mStatus err = mDNS_Register_internal(m, &info->AutoTunnel6Record);
        if (err) LogMsg("UpdateAutoTunnel6Record error %d registering %##s", err, info->AutoTunnel6Record.namestorage.c);
        else LogInfo("UpdateAutoTunnel6Record registering %##s", info->AutoTunnel6Record.namestorage.c);

        UpdateAutoTunnelHostRecord(m, info);

        LogInfo("AutoTunnel6 server listening for connections on %##s[%.16a] :%##s[%.16a]",
                info->AutoTunnel6Record.namestorage.c,    &m->AutoTunnelRelayAddr,
                info->AutoTunnelHostRecord.namestorage.c, &info->AutoTunnelInnerAddress);

    }
    else LogInfo("UpdateAutoTunnel6Record NOOP state(%d)",info->AutoTunnel6Record.resrec.RecordType);
}

mDNSlocal void AutoTunnelRecordCallback(mDNS *const m, AuthRecord *const rr, mStatus result)
{
    DomainAuthInfo *info = (DomainAuthInfo *)rr->RecordContext;
    if (result == mStatus_MemFree)
    {
        LogInfo("AutoTunnelRecordCallback MemFree %s", ARDisplayString(m, rr));
        
        mDNS_Lock(m);
        
        // Reset the host record namestorage to force high-level PTR/SRV/TXT to deregister
        if (rr == &info->AutoTunnelHostRecord)
        {
            rr->namestorage.c[0] = 0;
            m->NextSRVUpdate = NonZeroTime(m->timenow);
            LogInfo("AutoTunnelRecordCallback: NextSRVUpdate in %d %d", m->NextSRVUpdate - m->timenow, m->timenow);
        }
        if (m->ShutdownTime)
        {
            LogInfo("AutoTunnelRecordCallback: Shutdown, returning");
            mDNS_Unlock(m);        
            return;
        }
        if (rr == &info->AutoTunnelHostRecord)
        {
            LogInfo("AutoTunnelRecordCallback: calling UpdateAutoTunnelHostRecord");
            UpdateAutoTunnelHostRecord(m,info);
        }
        else if (rr == &info->AutoTunnelDeviceInfo)
        {
            LogInfo("AutoTunnelRecordCallback: Calling UpdateAutoTunnelDeviceInfoRecord");
            UpdateAutoTunnelDeviceInfoRecord(m,info);
        }
        else if (rr == &info->AutoTunnelService || rr == &info->AutoTunnelTarget)
        {
            LogInfo("AutoTunnelRecordCallback: Calling UpdateAutoTunnelServiceRecords");
            UpdateAutoTunnelServiceRecords(m,info);
        }
        else if (rr == &info->AutoTunnel6Record)
        {
            LogInfo("AutoTunnelRecordCallback: Calling UpdateAutoTunnel6Record");
            UpdateAutoTunnel6Record(m,info);
        }

        mDNS_Unlock(m);        
    }
}

mDNSlocal void AutoTunnelNATCallback(mDNS *m, NATTraversalInfo *n)
{
    DomainAuthInfo *info;

    LogInfo("AutoTunnelNATCallback Result %d %.4a Internal %d External %d",
            n->Result, &n->ExternalAddress, mDNSVal16(n->IntPort), mDNSVal16(n->ExternalPort));

    mDNS_Lock(m);
    
    m->NextSRVUpdate = NonZeroTime(m->timenow);
    LogInfo("AutoTunnelNATCallback: NextSRVUpdate in %d %d", m->NextSRVUpdate - m->timenow, m->timenow);

    for (info = m->AuthInfoList; info; info = info->next)
        if (info->AutoTunnel)
            UpdateAutoTunnelServiceRecords(m, info);

    UpdateAnonymousRacoonConfig(m);     // Determine whether we need racoon to accept incoming connections

    UpdateAutoTunnelDomainStatuses(m);

    mDNS_Unlock(m);
}

mDNSlocal void AutoTunnelHostNameChanged(mDNS *m, DomainAuthInfo *info)
{
    LogInfo("AutoTunnelHostNameChanged %#s.%##s", m->hostlabel.c, info->domain.c);

    mDNS_Lock(m);
    // We forcibly deregister the records that are based on the hostname.
    // When deregistration of each completes, the MemFree callback will make the
    // appropriate Update* call to use the new name to reregister.
    DeregisterAutoTunnelHostRecord(m, info);
    DeregisterAutoTunnelDeviceInfoRecord(m, info);
    DeregisterAutoTunnelServiceRecords(m, info);
    DeregisterAutoTunnel6Record(m, info);
    m->NextSRVUpdate = NonZeroTime(m->timenow);
    mDNS_Unlock(m);
}

// Must be called with the lock held
mDNSexport void StartServerTunnel(mDNS *const m, DomainAuthInfo *const info)
{
    if (info->deltime) return;
    
    if (info->AutoTunnelServiceStarted)
    {
        // On wake from sleep, this function will be called when determining SRV targets,
        // and needs to re-register the host record for the target to be set correctly
        UpdateAutoTunnelHostRecord(m, info);
        return;
    }
    
    info->AutoTunnelServiceStarted = mDNStrue;

    // Now that we have a service in this domain, we need to try to register the
    // AutoTunnel records, because the relay connection & NAT-T may have already been
    // started for another domain. If the relay connection is not up or the NAT-T has not
    // yet succeeded, the Update* functions are smart enough to not register the records.
    // Note: This should be done after we set AutoTunnelServiceStarted, as that variable is used to
    // decide whether to register the AutoTunnel records in the calls below.
    UpdateAutoTunnelServiceRecords(m, info);
    UpdateAutoTunnel6Record(m, info);
    UpdateAutoTunnelDeviceInfoRecord(m, info);
    UpdateAutoTunnelHostRecord(m, info);

    // If the global AutoTunnel NAT-T is not yet started, start it.
    if (!m->AutoTunnelNAT.clientContext)
    {
        m->AutoTunnelNAT.clientCallback   = AutoTunnelNATCallback;
        m->AutoTunnelNAT.clientContext    = (void*)1; // Means AutoTunnelNAT Traversal is active;
        m->AutoTunnelNAT.Protocol         = NATOp_MapUDP;
        m->AutoTunnelNAT.IntPort          = IPSECPort;
        m->AutoTunnelNAT.RequestedPort    = IPSECPort;
        m->AutoTunnelNAT.NATLease         = 0;
        mStatus err = mDNS_StartNATOperation_internal(m, &m->AutoTunnelNAT);
        if (err) LogMsg("StartServerTunnel: error %d starting NAT mapping", err);
    }
}

mDNSlocal mStatus AutoTunnelSetKeys(ClientTunnel *tun, mDNSBool AddNew)
{
    mDNSv6Addr loc_outer6;
    mDNSv6Addr rmt_outer6;

    // When we are tunneling over IPv6 Relay address, the port number is zero
    if (mDNSIPPortIsZero(tun->rmt_outer_port))
    {
        loc_outer6 = tun->loc_outer6;
        rmt_outer6 = tun->rmt_outer6;
    }
    else
    {
        loc_outer6 = zerov6Addr;
        loc_outer6.b[0] = tun->loc_outer.b[0];
        loc_outer6.b[1] = tun->loc_outer.b[1];
        loc_outer6.b[2] = tun->loc_outer.b[2];
        loc_outer6.b[3] = tun->loc_outer.b[3];

        rmt_outer6 = zerov6Addr;
        rmt_outer6.b[0] = tun->rmt_outer.b[0];
        rmt_outer6.b[1] = tun->rmt_outer.b[1];
        rmt_outer6.b[2] = tun->rmt_outer.b[2];
        rmt_outer6.b[3] = tun->rmt_outer.b[3];
    }

    return(mDNSAutoTunnelSetKeys(AddNew ? kmDNSAutoTunnelSetKeysReplace : kmDNSAutoTunnelSetKeysDelete, tun->loc_inner.b, loc_outer6.b, kRacoonPort, tun->rmt_inner.b, rmt_outer6.b, mDNSVal16(tun->rmt_outer_port), btmmprefix, SkipLeadingLabels(&tun->dstname, 1)));
}

// If the EUI-64 part of the IPv6 ULA matches, then that means the two addresses point to the same machine
#define mDNSSameClientTunnel(A,B) ((A)->l[2] == (B)->l[2] && (A)->l[3] == (B)->l[3])

mDNSlocal void ReissueBlockedQuestionWithType(mDNS *const m, domainname *d, mDNSBool success, mDNSu16 qtype)
{
    DNSQuestion *q = m->Questions;
    while (q)
    {
        if (q->NoAnswer == NoAnswer_Suspended && q->qtype == qtype && q->AuthInfo && q->AuthInfo->AutoTunnel && SameDomainName(&q->qname, d))
        {
            LogInfo("Restart %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
            mDNSQuestionCallback *tmp = q->QuestionCallback;
            q->QuestionCallback = AutoTunnelCallback;   // Set QuestionCallback to suppress another call back to AddNewClientTunnel
            mDNS_StopQuery(m, q);
            mDNS_StartQuery(m, q);
            q->QuestionCallback = tmp;                  // Restore QuestionCallback back to the real value
            if (!success) q->NoAnswer = NoAnswer_Fail;
            // When we call mDNS_StopQuery, it's possible for other subordinate questions like the GetZoneData query to be cancelled too.
            // In general we have to assume that the question list might have changed in arbitrary ways.
            // This code is itself called from a question callback, so the m->CurrentQuestion mechanism is
            // already in use. The safest solution is just to go back to the start of the list and start again.
            // In principle this sounds like an n^2 algorithm, but in practice we almost always activate
            // just one suspended question, so it's really a 2n algorithm.
            q = m->Questions;
        }
        else
            q = q->next;
    }
}

mDNSlocal void ReissueBlockedQuestions(mDNS *const m, domainname *d, mDNSBool success)
{
    // 1. We deliberately restart AAAA queries before A queries, because in the common case where a BTTM host has
    //    a v6 address but no v4 address, we prefer the caller to get the positive AAAA response before the A NXDOMAIN.
    // 2. In the case of AAAA queries, if our tunnel setup failed, then we return a deliberate failure indication to the caller --
    //    even if the name does have a valid AAAA record, we don't want clients trying to connect to it without a properly encrypted tunnel.
    // 3. For A queries we never fabricate failures -- if a BTTM service is really using raw IPv4, then it doesn't need the IPv6 tunnel.
    ReissueBlockedQuestionWithType(m, d, success, kDNSType_AAAA);
    ReissueBlockedQuestionWithType(m, d, mDNStrue, kDNSType_A);
}

mDNSlocal void UnlinkAndReissueBlockedQuestions(mDNS *const m, ClientTunnel *tun, mDNSBool success)
{
    ClientTunnel **p = &m->TunnelClients;
    while (*p != tun && *p) p = &(*p)->next;
    if (*p) *p = tun->next;
    ReissueBlockedQuestions(m, &tun->dstname, success);
    LogInfo("UnlinkAndReissueBlockedQuestions: Disposing ClientTunnel %p", tun);
    freeL("ClientTunnel", tun);
}

mDNSlocal mDNSBool TunnelClientDeleteMatching(mDNS *const m, ClientTunnel *tun, mDNSBool v6Tunnel)
{
    ClientTunnel **p;
    mDNSBool needSetKeys = mDNStrue;

    p = &tun->next;
    while (*p)
    {
        // Is this a tunnel to the same host that we are trying to setup now?
        if (!mDNSSameClientTunnel(&(*p)->rmt_inner, &tun->rmt_inner)) p = &(*p)->next;
        else
        {
            ClientTunnel *old = *p;
            if (v6Tunnel)
            {
                if (!mDNSIPPortIsZero(old->rmt_outer_port)) { p = &old->next; continue; }
                LogInfo("TunnelClientDeleteMatching: Found existing IPv6 AutoTunnel for %##s %.16a", old->dstname.c, &old->rmt_inner);
                if (old->q.ThisQInterval >= 0)
                {
                    LogInfo("TunnelClientDeleteMatching: Stopping query on IPv6 AutoTunnel for %##s %.16a", old->dstname.c, &old->rmt_inner);
                    mDNS_StopQuery(m, &old->q);
                }
                else if (!mDNSSameIPv6Address((*p)->rmt_inner, tun->rmt_inner) ||
                         !mDNSSameIPv6Address(old->loc_inner, tun->loc_inner)   ||
                         !mDNSSameIPv6Address(old->loc_outer6, tun->loc_outer6) ||
                         !mDNSSameIPv6Address(old->rmt_outer6, tun->rmt_outer6))
                {
                    // Delete the old tunnel if the current tunnel to the same host does not have the same ULA or
                    // the other parameters of the tunnel are different
                    LogInfo("TunnelClientDeleteMatching: Deleting existing IPv6 AutoTunnel for %##s %.16a", old->dstname.c, &old->rmt_inner);
                    AutoTunnelSetKeys(old, mDNSfalse);
                }
                else
                {
                    // Reusing the existing tunnel means that we reuse the IPsec SAs and the policies. We delete the old
                    // as "tun" and "old" are identical
                    LogInfo("TunnelClientDeleteMatching: Reusing the existing IPv6 AutoTunnel for %##s %.16a", old->dstname.c,
                            &old->rmt_inner);
                    needSetKeys = mDNSfalse;
                }
            }
            else
            {
                if (mDNSIPPortIsZero(old->rmt_outer_port)) { p = &old->next; continue; }
                LogInfo("TunnelClientDeleteMatching: Found existing IPv4 AutoTunnel for %##s %.16a", old->dstname.c, &old->rmt_inner);
                if (old->q.ThisQInterval >= 0)
                {
                    LogInfo("TunnelClientDeleteMatching: Stopping query on IPv4 AutoTunnel for %##s %.16a", old->dstname.c, &old->rmt_inner);
                    mDNS_StopQuery(m, &old->q);
                }
                else if (!mDNSSameIPv6Address((*p)->rmt_inner, tun->rmt_inner) ||
                         !mDNSSameIPv6Address(old->loc_inner, tun->loc_inner)   ||
                         !mDNSSameIPv4Address(old->loc_outer, tun->loc_outer)   ||
                         !mDNSSameIPv4Address(old->rmt_outer, tun->rmt_outer)   ||
                         !mDNSSameIPPort(old->rmt_outer_port, tun->rmt_outer_port))
                {
                    // Delete the old tunnel if the current tunnel to the same host does not have the same ULA or
                    // the other parameters of the tunnel are different
                    LogInfo("TunnelClientDeleteMatching: Deleting existing IPv4 AutoTunnel for %##s %.16a", old->dstname.c, &old->rmt_inner);
                    AutoTunnelSetKeys(old, mDNSfalse);
                }
                else
                {
                    // Reusing the existing tunnel means that we reuse the IPsec SAs and the policies. We delete the old
                    // as "tun" and "old" are identical
                    LogInfo("TunnelClientDeleteMatching: Reusing the existing IPv4 AutoTunnel for %##s %.16a", old->dstname.c,
                            &old->rmt_inner);
                    needSetKeys = mDNSfalse;
                }
            }

            *p = old->next;
            LogInfo("TunnelClientDeleteMatching: Disposing ClientTunnel %p", old);
            freeL("ClientTunnel", old);
        }
    }
    return needSetKeys;
}

// v6Tunnel indicates whether to delete a tunnel whose outer header is IPv6. If false, outer IPv4
// tunnel will be deleted
mDNSlocal void TunnelClientDeleteAny(mDNS *const m, ClientTunnel *tun, mDNSBool v6Tunnel)
{
    ClientTunnel **p;

    p = &tun->next;
    while (*p)
    {
        // If there is more than one client tunnel to the same host, delete all of them.
        // We do this by just checking against the EUI64 rather than the full address
        if (!mDNSSameClientTunnel(&(*p)->rmt_inner, &tun->rmt_inner)) p = &(*p)->next;
        else
        {
            ClientTunnel *old = *p;
            if (v6Tunnel)
            {
                if (!mDNSIPPortIsZero(old->rmt_outer_port)) { p = &old->next; continue;}
                LogInfo("TunnelClientDeleteAny: Found existing IPv6 AutoTunnel for %##s %.16a", old->dstname.c, &old->rmt_inner);
            }
            else
            {
                if (mDNSIPPortIsZero(old->rmt_outer_port)) { p = &old->next; continue;}
                LogInfo("TunnelClientDeleteAny: Found existing IPv4 AutoTunnel for %##s %.16a", old->dstname.c, &old->rmt_inner);
            }
            if (old->q.ThisQInterval >= 0)
            {
                LogInfo("TunnelClientDeleteAny: Stopping query on AutoTunnel for %##s %.16a", old->dstname.c, &old->rmt_inner);
                mDNS_StopQuery(m, &old->q);
            }
            else
            {
                LogInfo("TunnelClientDeleteAny: Deleting existing AutoTunnel for %##s %.16a", old->dstname.c, &old->rmt_inner);
                AutoTunnelSetKeys(old, mDNSfalse);
            }
            *p = old->next;
            LogInfo("TunnelClientDeleteAny: Disposing ClientTunnel %p", old);
            freeL("ClientTunnel", old);
        }
    }
}

mDNSlocal void TunnelClientFinish(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer)
{
    mDNSBool needSetKeys = mDNStrue;
    ClientTunnel *tun = (ClientTunnel *)question->QuestionContext;
    mDNSBool v6Tunnel = mDNSfalse;
    DomainAuthInfo *info;

    // If the port is zero, then we have a relay address of the peer
    if (mDNSIPPortIsZero(tun->rmt_outer_port))
        v6Tunnel = mDNStrue;

    if (v6Tunnel)
    {
        LogInfo("TunnelClientFinish: Relay address %.16a", &answer->rdata->u.ipv6);
        tun->rmt_outer6 = answer->rdata->u.ipv6;
        tun->loc_outer6 = m->AutoTunnelRelayAddr;
    }
    else
    {
        LogInfo("TunnelClientFinish: SRV target address %.4a", &answer->rdata->u.ipv4);
        tun->rmt_outer = answer->rdata->u.ipv4;
        mDNSAddr tmpDst = { mDNSAddrType_IPv4, {{{0}}} };
        tmpDst.ip.v4 = tun->rmt_outer;
        mDNSAddr tmpSrc = zeroAddr;
        mDNSPlatformSourceAddrForDest(&tmpSrc, &tmpDst);
        if (tmpSrc.type == mDNSAddrType_IPv4) tun->loc_outer = tmpSrc.ip.v4;
        else tun->loc_outer = m->AdvertisedV4.ip.v4;
    }

    question->ThisQInterval = -1;       // So we know this tunnel setup has completed

    info = GetAuthInfoForName(m, &tun->dstname);
    if (!info)
    {
        LogMsg("TunnelClientFinish: Could not get AuthInfo for %##s", tun->dstname.c);
        ReissueBlockedQuestions(m, &tun->dstname, mDNSfalse);
        return;
    }
    
    tun->loc_inner = info->AutoTunnelInnerAddress;

    // If we found a v6Relay address for our peer, delete all the v4Tunnels for our peer and
    // look for existing tunnels to see whether they have the same information for our peer.
    // If not, delete them and need to create a new tunnel. If they are same, just use the
    // same tunnel. Do the similar thing if we found a v4Tunnel end point for our peer.
    TunnelClientDeleteAny(m, tun, !v6Tunnel);
    needSetKeys = TunnelClientDeleteMatching(m, tun, v6Tunnel);

    if (needSetKeys) LogInfo("TunnelClientFinish: New %s AutoTunnel for %##s %.16a", (v6Tunnel ? "IPv6" : "IPv4"), tun->dstname.c, &tun->rmt_inner);
    else LogInfo("TunnelClientFinish: Reusing exiting %s AutoTunnel for %##s %.16a", (v6Tunnel ? "IPv6" : "IPv4"), tun->dstname.c, &tun->rmt_inner);

    mStatus result = needSetKeys ? AutoTunnelSetKeys(tun, mDNStrue) : mStatus_NoError;
    static char msgbuf[32];
    mDNS_snprintf(msgbuf, sizeof(msgbuf), "Tunnel setup - %d", result);
    mDNSASLLog((uuid_t *)&m->asl_uuid, "autotunnel.config", result ? "failure" : "success", msgbuf, "");
    // Kick off any questions that were held pending this tunnel setup
    ReissueBlockedQuestions(m, &tun->dstname, (result == mStatus_NoError) ? mDNStrue : mDNSfalse);
}

mDNSexport void AutoTunnelCallback(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
{
    ClientTunnel *tun = (ClientTunnel *)question->QuestionContext;
    DomainAuthInfo *info;

    LogInfo("AutoTunnelCallback tun %p AddRecord %d rdlength %d qtype %d", tun, AddRecord, answer->rdlength, question->qtype);

    if (!AddRecord) return;
    mDNS_StopQuery(m, question);

    // If we are looking up the AAAA record for _autotunnel6, don't consider it as failure.
    // The code below will look for _autotunnel._udp SRV record followed by A record
    if (tun->tc_state != TC_STATE_AAAA_PEER_RELAY && !answer->rdlength)
    {
        LogInfo("AutoTunnelCallback NXDOMAIN %##s (%s)", question->qname.c, DNSTypeName(question->qtype));
        static char msgbuf[16];
        mDNS_snprintf(msgbuf, sizeof(msgbuf), "%s lookup", DNSTypeName(question->qtype));
        mDNSASLLog((uuid_t *)&m->asl_uuid, "autotunnel.config", "failure", msgbuf, "");
        UnlinkAndReissueBlockedQuestions(m, tun, mDNSfalse);
        return;
    }

    switch (tun->tc_state)
    {
    case TC_STATE_AAAA_PEER:
        if (question->qtype != kDNSType_AAAA)
        {
            LogMsg("AutoTunnelCallback: Bad question type %d in TC_STATE_AAAA_PEER", question->qtype);
        }
        info = GetAuthInfoForName(m, &tun->dstname);
        if (!info)
        {
            LogMsg("AutoTunnelCallback: Could not get AuthInfo for %##s", tun->dstname.c);
            UnlinkAndReissueBlockedQuestions(m, tun, mDNStrue);
            return;
        }
        if (mDNSSameIPv6Address(answer->rdata->u.ipv6, info->AutoTunnelInnerAddress))
        {
            LogInfo("AutoTunnelCallback: suppressing tunnel to self %.16a", &answer->rdata->u.ipv6);
            UnlinkAndReissueBlockedQuestions(m, tun, mDNStrue);
            return;
        }
        if (info && mDNSSameIPv6NetworkPart(answer->rdata->u.ipv6, info->AutoTunnelInnerAddress))
        {
            LogInfo("AutoTunnelCallback: suppressing tunnel to peer %.16a", &answer->rdata->u.ipv6);
            UnlinkAndReissueBlockedQuestions(m, tun, mDNStrue);
            return;
        }
        tun->rmt_inner = answer->rdata->u.ipv6;
        LogInfo("AutoTunnelCallback:TC_STATE_AAAA_PEER: dst host %.16a", &tun->rmt_inner);
        if (!mDNSIPv6AddressIsZero(m->AutoTunnelRelayAddr))
        {
            LogInfo("AutoTunnelCallback: Looking up _autotunnel6 AAAA");
            tun->tc_state = TC_STATE_AAAA_PEER_RELAY;
            question->qtype = kDNSType_AAAA;
            AssignDomainName(&question->qname, (const domainname*) "\x0C" "_autotunnel6");
        }
        else
        {
            LogInfo("AutoTunnelCallback: Looking up _autotunnel._udp SRV");
            tun->tc_state = TC_STATE_SRV_PEER;
            question->qtype = kDNSType_SRV;
            AssignDomainName(&question->qname, (const domainname*) "\x0B" "_autotunnel" "\x04" "_udp");
        }
        AppendDomainName(&question->qname, &tun->dstname);
        mDNS_StartQuery(m, &tun->q);
        return;
    case TC_STATE_AAAA_PEER_RELAY:
        if (question->qtype != kDNSType_AAAA)
        {
            LogMsg("AutoTunnelCallback: Bad question type %d in TC_STATE_AAAA_PEER_RELAY", question->qtype);
        }
        // If it failed, look for the SRV record.
        if (!answer->rdlength)
        {
            LogInfo("AutoTunnelCallback: Looking up _autotunnel6 AAAA failed, trying SRV");
            tun->tc_state = TC_STATE_SRV_PEER;
            AssignDomainName(&question->qname, (const domainname*) "\x0B" "_autotunnel" "\x04" "_udp");
            AppendDomainName(&question->qname, &tun->dstname);
            question->qtype = kDNSType_SRV;
            mDNS_StartQuery(m, &tun->q);
            return;
        }
        TunnelClientFinish(m, question, answer);
        return;
    case TC_STATE_SRV_PEER:
        if (question->qtype != kDNSType_SRV)
        {
            LogMsg("AutoTunnelCallback: Bad question type %d in TC_STATE_SRV_PEER", question->qtype);
        }
        LogInfo("AutoTunnelCallback: SRV target name %##s", answer->rdata->u.srv.target.c);
        tun->tc_state = TC_STATE_ADDR_PEER;
        AssignDomainName(&tun->q.qname, &answer->rdata->u.srv.target);
        tun->rmt_outer_port = answer->rdata->u.srv.port;
        question->qtype = kDNSType_A;
        mDNS_StartQuery(m, &tun->q);
        return;
    case TC_STATE_ADDR_PEER:
        if (question->qtype != kDNSType_A)
        {
            LogMsg("AutoTunnelCallback: Bad question type %d in TC_STATE_ADDR_PEER", question->qtype);
        }
        TunnelClientFinish(m, question, answer);
        return;
    default:
        LogMsg("AutoTunnelCallback: Unknown question %p", question);
    }
}

// Must be called with the lock held
mDNSexport void AddNewClientTunnel(mDNS *const m, DNSQuestion *const q)
{
    ClientTunnel *p = mallocL("ClientTunnel", sizeof(ClientTunnel));
    if (!p) return;
    AssignDomainName(&p->dstname, &q->qname);
    p->MarkedForDeletion = mDNSfalse;
    p->loc_inner      = zerov6Addr;
    p->loc_outer      = zerov4Addr;
    p->loc_outer6     = zerov6Addr;
    p->rmt_inner      = zerov6Addr;
    p->rmt_outer      = zerov4Addr;
    p->rmt_outer6     = zerov6Addr;
    p->rmt_outer_port = zeroIPPort;
    p->tc_state = TC_STATE_AAAA_PEER;
    p->next = m->TunnelClients;
    m->TunnelClients = p;       // We intentionally build list in reverse order

    p->q.InterfaceID      = mDNSInterface_Any;
    p->q.flags            = 0;
    p->q.Target           = zeroAddr;
    AssignDomainName(&p->q.qname, &q->qname);
    p->q.qtype            = kDNSType_AAAA;
    p->q.qclass           = kDNSClass_IN;
    p->q.LongLived        = mDNSfalse;
    p->q.ExpectUnique     = mDNStrue;
    p->q.ForceMCast       = mDNSfalse;
    p->q.ReturnIntermed   = mDNStrue;
    p->q.SuppressUnusable = mDNSfalse;
    p->q.SearchListIndex  = 0;
    p->q.AppendSearchDomains = 0;
    p->q.RetryWithSearchDomains = mDNSfalse;
    p->q.TimeoutQuestion  = 0;
    p->q.WakeOnResolve    = 0;
    p->q.UseBackgroundTrafficClass = mDNSfalse;
    p->q.ValidationRequired = 0;
    p->q.ValidatingResponse = 0;
    p->q.ProxyQuestion      = 0;
    p->q.qnameOrig        = mDNSNULL;
    p->q.AnonInfo         = mDNSNULL;
    p->q.pid              = mDNSPlatformGetPID();
    p->q.euid             = 0;
    p->q.QuestionCallback = AutoTunnelCallback;
    p->q.QuestionContext  = p;

    LogInfo("AddNewClientTunnel start tun %p %##s (%s)%s", p, &q->qname.c, DNSTypeName(q->qtype), q->LongLived ? " LongLived" : "");
    mDNS_StartQuery_internal(m, &p->q);
}

#endif // APPLE_OSX_mDNSResponder

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Power State & Configuration Change Management
#endif

mDNSlocal mStatus UpdateInterfaceList(mDNS *const m, mDNSs32 utc)
{
    mDNSBool foundav4           = mDNSfalse;
    mDNSBool foundav6           = mDNSfalse;
    struct ifaddrs *ifa         = myGetIfAddrs(0);
    struct ifaddrs *v4Loopback  = NULL;
    struct ifaddrs *v6Loopback  = NULL;
    char defaultname[64];
    int InfoSocket              = socket(AF_INET6, SOCK_DGRAM, 0);
    if (InfoSocket < 3 && errno != EAFNOSUPPORT) 
        LogMsg("UpdateInterfaceList: InfoSocket error %d errno %d (%s)", InfoSocket, errno, strerror(errno));

    if (m->SleepState == SleepState_Sleeping) ifa = NULL;

    while (ifa)
    {
#if LIST_ALL_INTERFACES
        if (ifa->ifa_addr->sa_family == AF_APPLETALK)
            LogMsg("UpdateInterfaceList: %5s(%d) Flags %04X Family %2d is AF_APPLETALK",
                   ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family);
        else if (ifa->ifa_addr->sa_family == AF_LINK)
            LogMsg("UpdateInterfaceList: %5s(%d) Flags %04X Family %2d is AF_LINK",
                   ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family);
        else if (ifa->ifa_addr->sa_family != AF_INET && ifa->ifa_addr->sa_family != AF_INET6)
            LogMsg("UpdateInterfaceList: %5s(%d) Flags %04X Family %2d not AF_INET (2) or AF_INET6 (30)",
                   ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family);
        if (!(ifa->ifa_flags & IFF_UP))
            LogMsg("UpdateInterfaceList: %5s(%d) Flags %04X Family %2d Interface not IFF_UP",
                   ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family);
        if (!(ifa->ifa_flags & IFF_MULTICAST))
            LogMsg("UpdateInterfaceList: %5s(%d) Flags %04X Family %2d Interface not IFF_MULTICAST",
                   ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family);
        if (ifa->ifa_flags & IFF_POINTOPOINT)
            LogMsg("UpdateInterfaceList: %5s(%d) Flags %04X Family %2d Interface IFF_POINTOPOINT",
                   ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family);
        if (ifa->ifa_flags & IFF_LOOPBACK)
            LogMsg("UpdateInterfaceList: %5s(%d) Flags %04X Family %2d Interface IFF_LOOPBACK",
                   ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family);
#endif

        if (ifa->ifa_addr->sa_family == AF_LINK)
        {
            struct sockaddr_dl *sdl = (struct sockaddr_dl *)ifa->ifa_addr;
            if (sdl->sdl_type == IFT_ETHER && sdl->sdl_alen == sizeof(m->PrimaryMAC) && mDNSSameEthAddress(&m->PrimaryMAC, &zeroEthAddr))
                mDNSPlatformMemCopy(m->PrimaryMAC.b, sdl->sdl_data + sdl->sdl_nlen, 6);
        }

        if (ifa->ifa_flags & IFF_UP && ifa->ifa_addr)
            if (ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6)
            {
                if (!ifa->ifa_netmask)
                {
                    mDNSAddr ip;
                    SetupAddr(&ip, ifa->ifa_addr);
                    LogMsg("getifaddrs: ifa_netmask is NULL for %5s(%d) Flags %04X Family %2d %#a",
                           ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family, &ip);
                }
                // Apparently it's normal for the sa_family of an ifa_netmask to sometimes be zero, so we don't complain about that
                // <rdar://problem/5492035> getifaddrs is returning invalid netmask family for fw0 and vmnet
                else if (ifa->ifa_netmask->sa_family != ifa->ifa_addr->sa_family && ifa->ifa_netmask->sa_family != 0)
                {
                    mDNSAddr ip;
                    SetupAddr(&ip, ifa->ifa_addr);
                    LogMsg("getifaddrs ifa_netmask for %5s(%d) Flags %04X Family %2d %#a has different family: %d",
                           ifa->ifa_name, if_nametoindex(ifa->ifa_name), ifa->ifa_flags, ifa->ifa_addr->sa_family, &ip, ifa->ifa_netmask->sa_family);
                }
                // Currently we use a few internal ones like mDNSInterfaceID_LocalOnly etc. that are negative values (0, -1, -2).
                else if ((int)if_nametoindex(ifa->ifa_name) <= 0)
                {
                    LogMsg("UpdateInterfaceList: if_nametoindex returned zero/negative value for %5s(%d)", ifa->ifa_name, if_nametoindex(ifa->ifa_name));
                }
                else
                {
                    // Make sure ifa_netmask->sa_family is set correctly
                    // <rdar://problem/5492035> getifaddrs is returning invalid netmask family for fw0 and vmnet
                    ifa->ifa_netmask->sa_family = ifa->ifa_addr->sa_family;
                    int ifru_flags6 = 0;

                    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
                    if (ifa->ifa_addr->sa_family == AF_INET6 && InfoSocket >= 0)
                    {
                        struct in6_ifreq ifr6;
                        mDNSPlatformMemZero((char *)&ifr6, sizeof(ifr6));
                        strlcpy(ifr6.ifr_name, ifa->ifa_name, sizeof(ifr6.ifr_name));
                        ifr6.ifr_addr = *sin6;
                        if (ioctl(InfoSocket, SIOCGIFAFLAG_IN6, &ifr6) != -1)
                            ifru_flags6 = ifr6.ifr_ifru.ifru_flags6;
                        verbosedebugf("%s %.16a %04X %04X", ifa->ifa_name, &sin6->sin6_addr, ifa->ifa_flags, ifru_flags6);
                    }

                    if (!(ifru_flags6 & (IN6_IFF_TENTATIVE | IN6_IFF_DETACHED | IN6_IFF_DEPRECATED | IN6_IFF_TEMPORARY)))
                    {
                        if (ifa->ifa_flags & IFF_LOOPBACK)
                        {
                            if (ifa->ifa_addr->sa_family == AF_INET) 
                                v4Loopback = ifa;
                            else if (sin6->sin6_addr.s6_addr[0] != 0xFD) 
                                v6Loopback = ifa;
                        }
                        else
                        {
                            NetworkInterfaceInfoOSX *i = AddInterfaceToList(m, ifa, utc);
                            if (i && MulticastInterface(i) && i->ifinfo.Advertise)
                            {
                                if (ifa->ifa_addr->sa_family == AF_INET) 
                                    foundav4 = mDNStrue;
                                else 
                                    foundav6 = mDNStrue;
                            }
                        }
                    }
                }
            }
        ifa = ifa->ifa_next;
    }

    // For efficiency, we don't register a loopback interface when other interfaces of that family are available and advertising
    if (!foundav4 && v4Loopback) AddInterfaceToList(m, v4Loopback, utc);
    if (!foundav6 && v6Loopback) AddInterfaceToList(m, v6Loopback, utc);

    // Now the list is complete, set the McastTxRx setting for each interface.
    NetworkInterfaceInfoOSX *i;
    for (i = m->p->InterfaceList; i; i = i->next)
        if (i->Exists)
        {
            mDNSBool txrx = MulticastInterface(i);
            if (i->ifinfo.McastTxRx != txrx)
            {
                i->ifinfo.McastTxRx = txrx;
                i->Exists = 2; // State change; need to deregister and reregister this interface
            }
        }

    if (InfoSocket >= 0) 
        close(InfoSocket);

    mDNS_snprintf(defaultname, sizeof(defaultname), "%.*s-%02X%02X%02X%02X%02X%02X", HINFO_HWstring_prefixlen, HINFO_HWstring,
                  m->PrimaryMAC.b[0], m->PrimaryMAC.b[1], m->PrimaryMAC.b[2], m->PrimaryMAC.b[3], m->PrimaryMAC.b[4], m->PrimaryMAC.b[5]);

    // Set up the nice label
    domainlabel nicelabel;
    nicelabel.c[0] = 0;
    GetUserSpecifiedFriendlyComputerName(&nicelabel);
    if (nicelabel.c[0] == 0)
    {
        debugf("Couldn’t read user-specified Computer Name; using default “%s” instead", defaultname);
        MakeDomainLabelFromLiteralString(&nicelabel, defaultname);
    }

    // Set up the RFC 1034-compliant label
    domainlabel hostlabel;
    hostlabel.c[0] = 0;
    GetUserSpecifiedLocalHostName(&hostlabel);
    if (hostlabel.c[0] == 0)
    {
        debugf("Couldn’t read user-specified Local Hostname; using default “%s.local” instead", defaultname);
        MakeDomainLabelFromLiteralString(&hostlabel, defaultname);
    }

    mDNSBool namechange = mDNSfalse;

    // We use a case-sensitive comparison here because even though changing the capitalization
    // of the name alone is not significant to DNS, it's still a change from the user's point of view
    if (SameDomainLabelCS(m->p->usernicelabel.c, nicelabel.c))
        debugf("Usernicelabel (%#s) unchanged since last time; not changing m->nicelabel (%#s)", m->p->usernicelabel.c, m->nicelabel.c);
    else
    {
        if (m->p->usernicelabel.c[0])   // Don't show message first time through, when we first read name from prefs on boot
            LogMsg("User updated Computer Name from “%#s” to “%#s”", m->p->usernicelabel.c, nicelabel.c);
        m->p->usernicelabel = m->nicelabel = nicelabel;
        namechange = mDNStrue;
    }

    if (SameDomainLabelCS(m->p->userhostlabel.c, hostlabel.c))
        debugf("Userhostlabel (%#s) unchanged since last time; not changing m->hostlabel (%#s)", m->p->userhostlabel.c, m->hostlabel.c);
    else
    {
        if (m->p->userhostlabel.c[0])   // Don't show message first time through, when we first read name from prefs on boot
            LogMsg("User updated Local Hostname from “%#s” to “%#s”", m->p->userhostlabel.c, hostlabel.c);
        m->p->userhostlabel = m->hostlabel = hostlabel;
        mDNS_SetFQDN(m);
        namechange = mDNStrue;
    }

    if (namechange)     // If either name has changed, we need to tickle our AutoTunnel state machine to update its registered records
    {
#if APPLE_OSX_mDNSResponder
        DomainAuthInfo *info;
        for (info = m->AuthInfoList; info; info = info->next)
            if (info->AutoTunnel) AutoTunnelHostNameChanged(m, info);
#endif // APPLE_OSX_mDNSResponder
    }

    return(mStatus_NoError);
}

// Returns number of leading one-bits in mask: 0-32 for IPv4, 0-128 for IPv6
// Returns -1 if all the one-bits are not contiguous
mDNSlocal int CountMaskBits(mDNSAddr *mask)
{
    int i = 0, bits = 0;
    int bytes = mask->type == mDNSAddrType_IPv4 ? 4 : mask->type == mDNSAddrType_IPv6 ? 16 : 0;
    while (i < bytes)
    {
        mDNSu8 b = mask->ip.v6.b[i++];
        while (b & 0x80) { bits++; b <<= 1; }
        if (b) return(-1);
    }
    while (i < bytes) if (mask->ip.v6.b[i++]) return(-1);
    return(bits);
}

// Returns count of non-link local V4 addresses registered (why? -- SC)
mDNSlocal int SetupActiveInterfaces(mDNS *const m, mDNSs32 utc)
{
    NetworkInterfaceInfoOSX *i;
    int count = 0;

    // Recalculate SuppressProbes time based on the current set of active interfaces.
    m->SuppressProbes = 0;
    for (i = m->p->InterfaceList; i; i = i->next)
        if (i->Exists)
        {
            NetworkInterfaceInfo *const n = &i->ifinfo;
            NetworkInterfaceInfoOSX *primary = SearchForInterfaceByName(m, i->ifinfo.ifname, AF_UNSPEC);
            if (!primary) LogMsg("SetupActiveInterfaces ERROR! SearchForInterfaceByName didn't find %s", i->ifinfo.ifname);

            if (i->Registered && i->Registered != primary)  // Sanity check
            {
                LogMsg("SetupActiveInterfaces ERROR! n->Registered %p != primary %p", i->Registered, primary);
                i->Registered = mDNSNULL;
            }

            if (!i->Registered)
            {
                // Note: If i->Registered is set, that means we've called mDNS_RegisterInterface() for this interface,
                // so we need to make sure we call mDNS_DeregisterInterface() before disposing it.
                // If i->Registered is NOT set, then we haven't registered it and we should not try to deregister it.
                i->Registered = primary;

                // If i->LastSeen == utc, then this is a brand-new interface, just created, or an interface that never went away.
                // If i->LastSeen != utc, then this is an old interface, previously seen, that went away for (utc - i->LastSeen) seconds.
                // If the interface is an old one that went away and came back in less than a minute, then we're in a flapping scenario.
                i->Occulting = !(i->ifa_flags & IFF_LOOPBACK) && (utc - i->LastSeen > 0 && utc - i->LastSeen < 60);

                // Temporary fix to handle P2P flapping. P2P reuses the scope-id, mac address and the IP address
                // every time it creates a new interface. We think it is a duplicate and hence consider it
                // as flashing and occulting, that is, flapping. If an interface is marked as flapping,
                // mDNS_RegisterInterface() changes the probe delay from 1/2 second to 5 seconds and
                // logs a warning message to system.log noting frequent interface transitions.
                // Same logic applies when IFEF_DIRECTLINK flag is set on the interface.
                if ((strncmp(i->ifinfo.ifname, "p2p", 3) == 0) || i->ifinfo.DirectLink)
                {
                    LogInfo("SetupActiveInterfaces: %s interface registering %s %s", i->ifinfo.ifname,
                            i->Flashing               ? " (Flashing)"  : "",
                            i->Occulting              ? " (Occulting)" : "");
                    mDNS_RegisterInterface(m, n, 0);
                }
                else
                {
                    mDNS_RegisterInterface(m, n, i->Flashing && i->Occulting);
                }

                if (!mDNSAddressIsLinkLocal(&n->ip)) count++;
                LogInfo("SetupActiveInterfaces:   Registered    %5s(%lu) %.6a InterfaceID %p(%p), primary %p, %#a/%d%s%s%s",
                        i->ifinfo.ifname, i->scope_id, &i->BSSID, i->ifinfo.InterfaceID, i, primary, &n->ip, CountMaskBits(&n->mask),
                        i->Flashing        ? " (Flashing)"  : "",
                        i->Occulting       ? " (Occulting)" : "",
                        n->InterfaceActive ? " (Primary)"   : "");

                if (!n->McastTxRx)
                {
                    debugf("SetupActiveInterfaces:   No Tx/Rx on   %5s(%lu) %.6a InterfaceID %p %#a", i->ifinfo.ifname, i->scope_id, &i->BSSID, i->ifinfo.InterfaceID, &n->ip);
#if TARGET_OS_EMBEDDED
                    // We join the Bonjour multicast group on Apple embedded platforms ONLY when a client request is active,
                    // so we leave the multicast group here to clear any residual group membership.
                    if (i->sa_family == AF_INET)
                    {
                        struct ip_mreq imr;
                        primary->ifa_v4addr.s_addr = n->ip.ip.v4.NotAnInteger;
                        imr.imr_multiaddr.s_addr = AllDNSLinkGroup_v4.ip.v4.NotAnInteger;
                        imr.imr_interface        = primary->ifa_v4addr;

                        if (SearchForInterfaceByName(m, i->ifinfo.ifname, AF_INET) == i)
                        {
                            LogInfo("SetupActiveInterfaces: %5s(%lu) Doing IP_DROP_MEMBERSHIP for %.4a on %.4a", i->ifinfo.ifname, i->scope_id, &imr.imr_multiaddr, &imr.imr_interface);
                            mStatus err = setsockopt(m->p->permanentsockets.sktv4, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr, sizeof(imr));
                            if (err < 0 && (errno != EADDRNOTAVAIL))
                                LogMsg("setsockopt - IP_DROP_MEMBERSHIP error %d errno %d (%s)", err, errno, strerror(errno));
                        }
                    }
                    if (i->sa_family == AF_INET6)
                    {
                        struct ipv6_mreq i6mr;
                        i6mr.ipv6mr_interface = primary->scope_id;
                        i6mr.ipv6mr_multiaddr = *(struct in6_addr*)&AllDNSLinkGroup_v6.ip.v6;

                        if (SearchForInterfaceByName(m, i->ifinfo.ifname, AF_INET6) == i)
                        {
                            LogInfo("SetupActiveInterfaces: %5s(%lu) Doing IPV6_LEAVE_GROUP for %.16a on %u", i->ifinfo.ifname, i->scope_id, &i6mr.ipv6mr_multiaddr, i6mr.ipv6mr_interface);
                            mStatus err = setsockopt(m->p->permanentsockets.sktv6, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &i6mr, sizeof(i6mr));
                            if (err < 0 && (errno != EADDRNOTAVAIL))
                                LogMsg("setsockopt - IPV6_LEAVE_GROUP error %d errno %d (%s) group %.16a on %u", err, errno, strerror(errno), &i6mr.ipv6mr_multiaddr, i6mr.ipv6mr_interface);
                        }
                    }
#endif // TARGET_OS_EMBEDDED
                }
                else
                {
                    if (i->sa_family == AF_INET)
                    {
                        struct ip_mreq imr;
                        primary->ifa_v4addr.s_addr = n->ip.ip.v4.NotAnInteger;
                        imr.imr_multiaddr.s_addr = AllDNSLinkGroup_v4.ip.v4.NotAnInteger;
                        imr.imr_interface        = primary->ifa_v4addr;

                        // If this is our *first* IPv4 instance for this interface name, we need to do a IP_DROP_MEMBERSHIP first,
                        // before trying to join the group, to clear out stale kernel state which may be lingering.
                        // In particular, this happens with removable network interfaces like USB Ethernet adapters -- the kernel has stale state
                        // from the last time the USB Ethernet adapter was connected, and part of the kernel thinks we've already joined the group
                        // on that interface (so we get EADDRINUSE when we try to join again) but a different part of the kernel thinks we haven't
                        // joined the group (so we receive no multicasts). Doing an IP_DROP_MEMBERSHIP before joining seems to flush the stale state.
                        // Also, trying to make the code leave the group when the adapter is removed doesn't work either,
                        // because by the time we get the configuration change notification, the interface is already gone,
                        // so attempts to unsubscribe fail with EADDRNOTAVAIL (errno 49 "Can't assign requested address").
                        // <rdar://problem/5585972> IP_ADD_MEMBERSHIP fails for previously-connected removable interfaces
                        if (SearchForInterfaceByName(m, i->ifinfo.ifname, AF_INET) == i)
                        {
                            LogInfo("SetupActiveInterfaces: %5s(%lu) Doing precautionary IP_DROP_MEMBERSHIP for %.4a on %.4a", i->ifinfo.ifname, i->scope_id, &imr.imr_multiaddr, &imr.imr_interface);
                            mStatus err = setsockopt(m->p->permanentsockets.sktv4, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr, sizeof(imr));
                            if (err < 0 && (errno != EADDRNOTAVAIL))
                                LogMsg("setsockopt - IP_DROP_MEMBERSHIP error %d errno %d (%s)", err, errno, strerror(errno));
                        }

                        LogInfo("SetupActiveInterfaces: %5s(%lu) joining IPv4 mcast group %.4a on %.4a", i->ifinfo.ifname, i->scope_id, &imr.imr_multiaddr, &imr.imr_interface);
                        mStatus err = setsockopt(m->p->permanentsockets.sktv4, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(imr));
                        // Joining same group twice can give "Address already in use" error -- no need to report that
                        if (err < 0 && (errno != EADDRINUSE))
                            LogMsg("setsockopt - IP_ADD_MEMBERSHIP error %d errno %d (%s) group %.4a on %.4a", err, errno, strerror(errno), &imr.imr_multiaddr, &imr.imr_interface);
                    }
                    if (i->sa_family == AF_INET6)
                    {
                        struct ipv6_mreq i6mr;
                        i6mr.ipv6mr_interface = primary->scope_id;
                        i6mr.ipv6mr_multiaddr = *(struct in6_addr*)&AllDNSLinkGroup_v6.ip.v6;

                        if (SearchForInterfaceByName(m, i->ifinfo.ifname, AF_INET6) == i)
                        {
                            LogInfo("SetupActiveInterfaces: %5s(%lu) Doing precautionary IPV6_LEAVE_GROUP for %.16a on %u", i->ifinfo.ifname, i->scope_id, &i6mr.ipv6mr_multiaddr, i6mr.ipv6mr_interface);
                            mStatus err = setsockopt(m->p->permanentsockets.sktv6, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &i6mr, sizeof(i6mr));
                            if (err < 0 && (errno != EADDRNOTAVAIL))
                                LogMsg("setsockopt - IPV6_LEAVE_GROUP error %d errno %d (%s) group %.16a on %u", err, errno, strerror(errno), &i6mr.ipv6mr_multiaddr, i6mr.ipv6mr_interface);
                        }

                        LogInfo("SetupActiveInterfaces: %5s(%lu) joining IPv6 mcast group %.16a on %u", i->ifinfo.ifname, i->scope_id, &i6mr.ipv6mr_multiaddr, i6mr.ipv6mr_interface);
                        mStatus err = setsockopt(m->p->permanentsockets.sktv6, IPPROTO_IPV6, IPV6_JOIN_GROUP, &i6mr, sizeof(i6mr));
                        // Joining same group twice can give "Address already in use" error -- no need to report that
                        if (err < 0 && (errno != EADDRINUSE))
                            LogMsg("setsockopt - IPV6_JOIN_GROUP error %d errno %d (%s) group %.16a on %u", err, errno, strerror(errno), &i6mr.ipv6mr_multiaddr, i6mr.ipv6mr_interface);
                    }
                }
            }
        }

    return count;
}

mDNSlocal void MarkAllInterfacesInactive(mDNS *const m, mDNSs32 utc)
{
    NetworkInterfaceInfoOSX *i;
    for (i = m->p->InterfaceList; i; i = i->next)
    {
        if (i->Exists) i->LastSeen = utc;
        i->Exists = mDNSfalse;
    }
}

// Returns count of non-link local V4 addresses deregistered (why? -- SC)
mDNSlocal int ClearInactiveInterfaces(mDNS *const m, mDNSs32 utc)
{
    // First pass:
    // If an interface is going away, then deregister this from the mDNSCore.
    // We also have to deregister it if the primary interface that it's using for its InterfaceID is going away.
    // We have to do this because mDNSCore will use that InterfaceID when sending packets, and if the memory
    // it refers to has gone away we'll crash.
    NetworkInterfaceInfoOSX *i;
    int count = 0;
    for (i = m->p->InterfaceList; i; i = i->next)
    {
        // If this interface is no longer active, or its InterfaceID is changing, deregister it
        NetworkInterfaceInfoOSX *primary = SearchForInterfaceByName(m, i->ifinfo.ifname, AF_UNSPEC);
        if (i->Registered)
            if (i->Exists == 0 || i->Exists == 2 || i->Registered != primary)
            {
                i->Flashing = !(i->ifa_flags & IFF_LOOPBACK) && (utc - i->AppearanceTime < 60);
                LogInfo("ClearInactiveInterfaces: Deregistering %5s(%lu) %.6a InterfaceID %p(%p), primary %p, %#a/%d%s%s%s",
                        i->ifinfo.ifname, i->scope_id, &i->BSSID, i->ifinfo.InterfaceID, i, primary,
                        &i->ifinfo.ip, CountMaskBits(&i->ifinfo.mask),
                        i->Flashing               ? " (Flashing)"  : "",
                        i->Occulting              ? " (Occulting)" : "",
                        i->ifinfo.InterfaceActive ? " (Primary)"   : "");

                // Temporary fix to handle P2P flapping. P2P reuses the scope-id, mac address and the IP address
                // every time it creates a new interface. We think it is a duplicate and hence consider it
                // as flashing and occulting. The "core" does not flush the cache for this case. This leads to
                // stale data returned to the application even after the interface is removed. The application
                // then starts to send data but the new interface is not yet created.
                // Same logic applies when IFEF_DIRECTLINK flag is set on the interface.
                if ((strncmp(i->ifinfo.ifname, "p2p", 3) == 0) || i->ifinfo.DirectLink)
                {
                    LogInfo("ClearInactiveInterfaces: %s interface deregistering %s %s", i->ifinfo.ifname,
                            i->Flashing               ? " (Flashing)"  : "",
                            i->Occulting              ? " (Occulting)" : "");
                    mDNS_DeregisterInterface(m, &i->ifinfo, 0);
                }
                else
                {
                    mDNS_DeregisterInterface(m, &i->ifinfo, i->Flashing && i->Occulting);
                }
                if (!mDNSAddressIsLinkLocal(&i->ifinfo.ip)) count++;
                i->Registered = mDNSNULL;
                // Note: If i->Registered is set, that means we've called mDNS_RegisterInterface() for this interface,
                // so we need to make sure we call mDNS_DeregisterInterface() before disposing it.
                // If i->Registered is NOT set, then it's not registered and we should not call mDNS_DeregisterInterface() on it.

                // Caution: If we ever decide to add code here to leave the multicast group, we need to make sure that this
                // is the LAST representative of this physical interface, or we'll unsubscribe from the group prematurely.
            }
    }

    // Second pass:
    // Now that everything that's going to deregister has done so, we can clean up and free the memory
    NetworkInterfaceInfoOSX **p = &m->p->InterfaceList;
    while (*p)
    {
        i = *p;
        // If no longer active, delete interface from list and free memory
        if (!i->Exists)
        {
            if (i->LastSeen == utc) i->LastSeen = utc - 1;
            mDNSBool delete = (NumCacheRecordsForInterfaceID(m, i->ifinfo.InterfaceID) == 0) && (utc - i->LastSeen >= 60);
            LogInfo("ClearInactiveInterfaces: %-13s %5s(%lu) %.6a InterfaceID %p(%p) %#a/%d Age %d%s", delete ? "Deleting" : "Holding",
                    i->ifinfo.ifname, i->scope_id, &i->BSSID, i->ifinfo.InterfaceID, i,
                    &i->ifinfo.ip, CountMaskBits(&i->ifinfo.mask), utc - i->LastSeen,
                    i->ifinfo.InterfaceActive ? " (Primary)" : "");
#if APPLE_OSX_mDNSResponder
            if (i->BPF_fd >= 0) CloseBPF(i);
#endif // APPLE_OSX_mDNSResponder
            if (delete)
            {
                *p = i->next;
                freeL("NetworkInterfaceInfoOSX", i);
                continue;   // After deleting this object, don't want to do the "p = &i->next;" thing at the end of the loop
            }
        }
        p = &i->next;
    }
    return count;
}

mDNSlocal void AppendDNameListElem(DNameListElem ***List, mDNSu32 uid, domainname *name)
{
    DNameListElem *dnle = (DNameListElem*) mallocL("DNameListElem/AppendDNameListElem", sizeof(DNameListElem));
    if (!dnle) LogMsg("ERROR: AppendDNameListElem: memory exhausted");
    else
    {
        dnle->next = mDNSNULL;
        dnle->uid  = uid;
        AssignDomainName(&dnle->name, name);
        **List = dnle;
        *List = &dnle->next;
    }
}

mDNSlocal int compare_dns_configs(const void *aa, const void *bb)
{
    dns_resolver_t *a = *(dns_resolver_t**)aa;
    dns_resolver_t *b = *(dns_resolver_t**)bb;

    return (a->search_order < b->search_order) ? -1 : (a->search_order == b->search_order) ? 0 : 1;
}

mDNSlocal void UpdateSearchDomainHash(mDNS *const m, MD5_CTX *sdc, char *domain, mDNSInterfaceID InterfaceID)
{
    char *buf = ".";
    mDNSu32 scopeid = 0;
    char ifid_buf[16];

    if (domain)
        buf = domain;
    //
    // Hash the search domain name followed by the InterfaceID.
    // As we have scoped search domains, we also included InterfaceID. If either of them change,
    // we will detect it. Even if the order of them change, we will detect it.
    //
    // Note: We have to handle a few of these tricky cases.
    //
    // 1) Current: com, apple.com Changing to: comapple.com
    // 2) Current: a.com,b.com Changing to a.comb.com
    // 3) Current: a.com,b.com (ifid 8), Changing to a.com8b.com (ifid 8)
    // 4) Current: a.com (ifid 12), Changing to a.com1 (ifid: 2)
    //
    // There are more variants of the above. The key thing is if we include the null in each case
    // at the end of name and the InterfaceID, it will prevent a new name (which can't include
    // NULL as part of the name) to be mistakenly thought of as a old name.

    scopeid = mDNSPlatformInterfaceIndexfromInterfaceID(m, InterfaceID, mDNStrue);
    // mDNS_snprintf always null terminates
    if (mDNS_snprintf(ifid_buf, sizeof(ifid_buf), "%u", scopeid) >= sizeof(ifid_buf))
        LogMsg("UpdateSearchDomainHash: mDNS_snprintf failed for scopeid %u", scopeid);

    LogInfo("UpdateSearchDomainHash: buf %s, ifid_buf %s", buf, ifid_buf);
    MD5_Update(sdc, buf, strlen(buf) + 1);
    MD5_Update(sdc, ifid_buf, strlen(ifid_buf) + 1);
}

mDNSlocal void FinalizeSearchDomainHash(mDNS *const m, MD5_CTX *sdc)
{
    mDNSu8 md5_hash[MD5_LEN];

    MD5_Final(md5_hash, sdc);

    if (memcmp(md5_hash, m->SearchDomainsHash, MD5_LEN))
    {
        // If the hash is different, either the search domains have changed or
        // the ordering between them has changed. Restart the questions that
        // would be affected by this.
        LogInfo("FinalizeSearchDomains: The hash is different");
        memcpy(m->SearchDomainsHash, md5_hash, MD5_LEN);
        RetrySearchDomainQuestions(m);
    }
    else { LogInfo("FinalizeSearchDomains: The hash is same"); }
}

mDNSexport const char *DNSScopeToString(mDNSu32 scope)
{
    switch (scope)
    {
        case kScopeNone:
            return "Unscoped";
        case kScopeInterfaceID:
            return "InterfaceScoped";
        case kScopeServiceID:
            return "ServiceScoped";
        default:
            return "Unknown";
    }
}

mDNSlocal void ConfigSearchDomains(mDNS *const m, dns_resolver_t *resolver, mDNSInterfaceID interfaceId, mDNSu32 scope,  MD5_CTX *sdc, uint64_t generation)
{
    const char *scopeString = DNSScopeToString(scope);
    int j;
    domainname d;

    if (scope == kScopeNone)
        interfaceId = mDNSInterface_Any;

    if (scope == kScopeNone || scope == kScopeInterfaceID)
    {
        for (j = 0; j < resolver->n_search; j++)
        {
            if (MakeDomainNameFromDNSNameString(&d, resolver->search[j]) != NULL)
            {
                static char interface_buf[32];
                mDNS_snprintf(interface_buf, sizeof(interface_buf), "for interface %s", InterfaceNameForID(m, interfaceId));
                LogInfo("ConfigSearchDomains: (%s) configuring search domain %s %s (generation= %llu)", scopeString,
                        resolver->search[j], (interfaceId == mDNSInterface_Any) ? "" : interface_buf, generation);
                UpdateSearchDomainHash(m, sdc, resolver->search[j], interfaceId);
                mDNS_AddSearchDomain_CString(resolver->search[j], interfaceId);
            }
            else
            {
                LogInfo("ConfigSearchDomains: An invalid search domain was detected for %s domain %s n_nameserver %d, (generation= %llu)",
                        DNSScopeToString(scope), resolver->domain, resolver->n_nameserver, generation);
            }
        }
    }
    else
    {
        LogInfo("ConfigSearchDomains: (%s) Ignoring search domain for interface %s", scopeString, InterfaceNameForID(m,interfaceId));
    }
}

mDNSlocal mDNSInterfaceID ConfigParseInterfaceID(mDNS *const m, mDNSu32 ifindex)
{
    NetworkInterfaceInfoOSX *ni;
    mDNSInterfaceID interface;

    for (ni = m->p->InterfaceList; ni; ni = ni->next)
    {
        if (ni->ifinfo.InterfaceID && ni->scope_id == ifindex) 
            break;
    }
    if (ni != NULL) 
    {
        interface = ni->ifinfo.InterfaceID;
    }
    else
    {
        // In rare circumstances, we could potentially hit this case where we cannot parse the InterfaceID
        // (see <rdar://problem/13214785>). At this point, we still accept the DNS Config from configd 
        // Note: We currently ack the whole dns configuration and not individual resolvers or DNS servers. 
        // As the caller is going to ack the configuration always, we have to add all the DNS servers 
        // in the configuration. Otherwise, we won't have any DNS servers up until the network change.

        LogMsg("ConfigParseInterfaceID: interface specific index %d not found (interface may not be UP)",ifindex);

        // Set the correct interface from configd before passing this to mDNS_AddDNSServer() below
        interface = (mDNSInterfaceID)(unsigned long)ifindex;
    }
    return interface;
}

mDNSlocal void ConfigNonUnicastResolver(mDNS *const m, dns_resolver_t *r)
{
    char *opt = r->options;
    domainname d; 

    if (opt && !strncmp(opt, "mdns", strlen(opt)))
    {
        if (!MakeDomainNameFromDNSNameString(&d, r->domain))
        { 
            LogMsg("ConfigNonUnicastResolver: config->resolver bad domain %s", r->domain); 
            return;
        }
        mDNS_AddMcastResolver(m, &d, mDNSInterface_Any, r->timeout);
    }
}

mDNSlocal void ConfigDNSServers(mDNS *const m, dns_resolver_t *r, mDNSInterfaceID interface, mDNSu32 scope, mDNSu16 resGroupID)
{
    int n;
    domainname d;
    int serviceID = 0;
    mDNSBool cellIntf = mDNSfalse;
    mDNSBool reqA, reqAAAA;

    if (!r->domain || !*r->domain) 
    {
        d.c[0] = 0;
    }
    else if (!MakeDomainNameFromDNSNameString(&d, r->domain))
    { 
        LogMsg("ConfigDNSServers: bad domain %s", r->domain); 
        return;
    }
    // Parse the resolver specific attributes that affects all the DNS servers.
    if (scope == kScopeServiceID)
    {
        serviceID = r->service_identifier;
    }

#if TARGET_OS_IPHONE
    cellIntf = (r->reach_flags & kSCNetworkReachabilityFlagsIsWWAN) ? mDNStrue : mDNSfalse;
#endif
    reqA = (r->flags & DNS_RESOLVER_FLAGS_REQUEST_A_RECORDS ? mDNStrue : mDNSfalse);
    reqAAAA = (r->flags & DNS_RESOLVER_FLAGS_REQUEST_AAAA_RECORDS ? mDNStrue : mDNSfalse);

    for (n = 0; n < r->n_nameserver; n++)
    {
        mDNSAddr saddr;
        DNSServer *s;

        if (r->nameserver[n]->sa_family != AF_INET && r->nameserver[n]->sa_family != AF_INET6)
            continue;
        
        if (SetupAddr(&saddr, r->nameserver[n]))
        {
            LogMsg("ConfigDNSServers: Bad address");
            continue;
        }
        
        // The timeout value is for all the DNS servers in a given resolver, hence we pass
        // the timeout value only for the first DNSServer. If we don't have a value in the
        // resolver, then use the core's default value
        //
        // Note: this assumes that when the core picks a list of DNSServers for a question,
        // it takes the sum of all the timeout values for all DNS servers. By doing this, it
        // tries all the DNS servers in a specified timeout
        s = mDNS_AddDNSServer(m, &d, interface, serviceID, &saddr, r->port ? mDNSOpaque16fromIntVal(r->port) : UnicastDNSPort, scope,
                              (n == 0 ? (r->timeout ? r->timeout : DEFAULT_UDNS_TIMEOUT) : 0), cellIntf, resGroupID, reqA, reqAAAA, mDNStrue);
        if (s)
        {
            LogInfo("ConfigDNSServers(%s): DNS server %#a:%d for domain %##s", DNSScopeToString(scope), &s->addr, mDNSVal16(s->port), d.c);
        }
    }
}

// ConfigResolvers is called for different types of resolvers: Unscoped resolver, Interface scope resolver and
// Service scope resolvers. This is indicated by the scope argument.
//
// "resolver" has entries that should only be used for unscoped questions.
//
// "scoped_resolver" has entries that should only be used for Interface scoped question i.e., questions that specify an
// interface index (q->InterfaceID)
//
// "service_specific_resolver" has entries that should be used for Service scoped question i.e., questions that specify
// a service identifier (q->ServiceID)
//
mDNSlocal void ConfigResolvers(mDNS *const m, dns_config_t *config, mDNSu32 scope, mDNSBool setsearch, mDNSBool setservers, MD5_CTX *sdc, mDNSu16 resGroupID)
{
    int i;
    dns_resolver_t **resolver;
    int nresolvers;
    const char *scopeString = DNSScopeToString(scope);
    mDNSInterfaceID interface;

    switch (scope)
    {
        case kScopeNone:
            resolver = config->resolver;
            nresolvers = config->n_resolver;
            break;
        case kScopeInterfaceID:
            resolver = config->scoped_resolver;
            nresolvers = config->n_scoped_resolver;
            break;
        case kScopeServiceID:
            resolver = config->service_specific_resolver;
            nresolvers = config->n_service_specific_resolver;
            break;
        default:
            return;
    }
    qsort(resolver, nresolvers, sizeof(dns_resolver_t*), compare_dns_configs);

    for (i = 0; i < nresolvers; i++)
    {
        dns_resolver_t *r = resolver[i];

        LogInfo("ConfigResolvers: %s resolver[%d] domain %s n_nameserver %d", scopeString, i, r->domain, r->n_nameserver);

        interface = mDNSInterface_Any;

        // Parse the interface index 
        if (r->if_index != 0)
        {
            interface = ConfigParseInterfaceID(m, r->if_index);
        }

        if (setsearch)
        {
            ConfigSearchDomains(m, resolver[i], interface, scope, sdc, config->generation);
            
            // Parse other scoped resolvers for search lists
            if (!setservers) 
                continue;
        }

        if (r->port == 5353 || r->n_nameserver == 0)
        {
            ConfigNonUnicastResolver(m, r);
        }
        else
        {
            // Each scoped resolver gets its own ID (i.e., they are in their own group) so that responses from the
            // scoped resolver are not used by other non-scoped or scoped resolvers.
            if (scope != kScopeNone) 
                resGroupID++;

            ConfigDNSServers(m, r, interface, scope, resGroupID);
        }
    }
}

#if APPLE_OSX_mDNSResponder
mDNSlocal mDNSBool QuestionValidForDNSTrigger(DNSQuestion *q)
{
    if (QuerySuppressed(q))
    {
        debugf("QuestionValidForDNSTrigger: Suppressed: %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
        return mDNSfalse;
    }
    if (mDNSOpaque16IsZero(q->TargetQID))
    {
        debugf("QuestionValidForDNSTrigger: Multicast: %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
        return mDNSfalse;
    }
    // If we answered using LocalOnly records e.g., /etc/hosts, don't consider that a valid response
    // for trigger.
    if (q->LOAddressAnswers)
    {
        debugf("QuestionValidForDNSTrigger: LocalOnly answers: %##s (%s)", q->qname.c, DNSTypeName(q->qtype));
        return mDNSfalse;
    }
    return mDNStrue;
}
#endif

// This function is called if we are not delivering unicast answers to "A" or "AAAA" questions.
// We set our state appropriately so that if we start receiving answers, trigger the
// upper layer to retry DNS questions.
#if APPLE_OSX_mDNSResponder
mDNSexport void mDNSPlatformUpdateDNSStatus(mDNS *const m, DNSQuestion *q)
{
    if (!QuestionValidForDNSTrigger(q))
        return;

    // Ignore applications that start and stop queries for no reason before we ever talk
    // to any DNS server.
    if (!q->triedAllServersOnce)
    {
        LogInfo("QuestionValidForDNSTrigger: question %##s (%s) stopped too soon", q->qname.c, DNSTypeName(q->qtype));
        return;
    }
    if (q->qtype == kDNSType_A)
        m->p->v4answers = 0;
    if (q->qtype == kDNSType_AAAA)
        m->p->v6answers = 0;
    if (!m->p->v4answers || !m->p->v6answers)
    {
        LogInfo("mDNSPlatformUpdateDNSStatus: Trigger needed v4 %d, v6 %d, quesiton %##s (%s)", m->p->v4answers, m->p->v6answers, q->qname.c,
            DNSTypeName(q->qtype));
    }
}
#endif

mDNSlocal void AckConfigd(mDNS *const m, dns_config_t *config)
{
    mDNS_CheckLock(m);

    // Acking the configuration triggers configd to reissue the reachability queries
    m->p->DNSTrigger = NonZeroTime(m->timenow);
    _dns_configuration_ack(config, "com.apple.mDNSResponder");
}

// If v4q is non-NULL, it means we have received some answers for "A" type questions
// If v6q is non-NULL, it means we have received some answers for "AAAA" type questions
#if APPLE_OSX_mDNSResponder
mDNSexport void mDNSPlatformTriggerDNSRetry(mDNS *const m, DNSQuestion *v4q, DNSQuestion *v6q)
{
    mDNSBool trigger = mDNSfalse;
    mDNSs32 timenow;

    // Don't send triggers too often.
    // If we have started delivering answers to questions, we should send a trigger
    // if the time permits. If we are delivering answers, we should set the state
    // of v4answers/v6answers to 1 and avoid sending a trigger.  But, we don't know
    // whether the answers that are being delivered currently is for configd or some
    // other application. If we set the v4answers/v6answers to 1 and not deliver a trigger,
    // then we won't deliver the trigger later when it is okay to send one as the
    // "answers" are already set to 1. Hence, don't affect the state of v4answers and
    // v6answers if we are not delivering triggers.
    mDNS_Lock(m);
    timenow = m->timenow;
    if (m->p->DNSTrigger && (timenow - m->p->DNSTrigger) < DNS_TRIGGER_INTERVAL)
    {
        if (!m->p->v4answers || !m->p->v6answers)
        {
            debugf("mDNSPlatformTriggerDNSRetry: not triggering, time since last trigger %d ms, v4ans %d, v6ans %d",
                (timenow - m->p->DNSTrigger), m->p->v4answers, m->p->v6answers);
        }
        mDNS_Unlock(m);
        return;
    }
    mDNS_Unlock(m);
    if (v4q != NULL && QuestionValidForDNSTrigger(v4q))
    {
        int old = m->p->v4answers;

        m->p->v4answers = 1;

        // If there are IPv4 answers now and previously we did not have
        // any answers, trigger a DNS change so that reachability
        // can retry the queries again.
        if (!old)
        {
            LogInfo("mDNSPlatformTriggerDNSRetry: Triggering because of IPv4, last trigger %d ms, %##s (%s)", (timenow - m->p->DNSTrigger),
                v4q->qname.c, DNSTypeName(v4q->qtype));
            trigger = mDNStrue;
        }
    }
    if (v6q != NULL && QuestionValidForDNSTrigger(v6q))
    {
        int old = m->p->v6answers;

        m->p->v6answers = 1;
        // If there are IPv6 answers now and previously we did not have
        // any answers, trigger a DNS change so that reachability
        // can retry the queries again.
        if (!old)
        {
            LogInfo("mDNSPlatformTriggerDNSRetry: Triggering because of IPv6, last trigger %d ms, %##s (%s)", (timenow - m->p->DNSTrigger),
                v6q->qname.c, DNSTypeName(v6q->qtype));
            trigger = mDNStrue;
        }
    }
    if (trigger)
    {
        dns_config_t *config = dns_configuration_copy();
        if (config)
        {
            mDNS_Lock(m);
            AckConfigd(m, config);
            mDNS_Unlock(m);
            dns_configuration_free(config);
        }
        else
        {
            LogMsg("mDNSPlatformTriggerDNSRetry: ERROR!! configd did not return config");
        }
    }
}

mDNSlocal void SetupActiveDirectoryDomain(dns_config_t *config)
{
    // Record the so-called "primary" domain, which we use as a hint to tell if the user is on a network set up
    // by someone using Microsoft Active Directory using "local" as a private internal top-level domain
    if (config->n_resolver && config->resolver[0]->domain && config->resolver[0]->n_nameserver &&
        config->resolver[0]->nameserver[0])
    {
        MakeDomainNameFromDNSNameString(&ActiveDirectoryPrimaryDomain, config->resolver[0]->domain);
    }
    else
    {
         ActiveDirectoryPrimaryDomain.c[0] = 0;
    }

    //MakeDomainNameFromDNSNameString(&ActiveDirectoryPrimaryDomain, "test.local");
    ActiveDirectoryPrimaryDomainLabelCount = CountLabels(&ActiveDirectoryPrimaryDomain);
    if (config->n_resolver && config->resolver[0]->n_nameserver &&
        SameDomainName(SkipLeadingLabels(&ActiveDirectoryPrimaryDomain, ActiveDirectoryPrimaryDomainLabelCount - 1), &localdomain))
    {
        SetupAddr(&ActiveDirectoryPrimaryDomainServer, config->resolver[0]->nameserver[0]);
    }
    else
    {
        AssignDomainName(&ActiveDirectoryPrimaryDomain, (const domainname *)"");
        ActiveDirectoryPrimaryDomainLabelCount = 0;
        ActiveDirectoryPrimaryDomainServer = zeroAddr;
    }
}
#endif

mDNSlocal void SetupDDNSDomains(domainname *const fqdn, DNameListElem **RegDomains, DNameListElem **BrowseDomains)
{
    int i;
    char buf[MAX_ESCAPED_DOMAIN_NAME];  // Max legal C-string name, including terminating NULL
    domainname d;

    CFDictionaryRef ddnsdict = SCDynamicStoreCopyValue(NULL, NetworkChangedKey_DynamicDNS);
    if (ddnsdict)
    {
        if (fqdn)
        {
            CFArrayRef fqdnArray = CFDictionaryGetValue(ddnsdict, CFSTR("HostNames"));
            if (fqdnArray && CFArrayGetCount(fqdnArray) > 0)
            {
                // for now, we only look at the first array element.  if we ever support multiple configurations, we will walk the list
                CFDictionaryRef fqdnDict = CFArrayGetValueAtIndex(fqdnArray, 0);
                if (fqdnDict && DictionaryIsEnabled(fqdnDict))
                {
                    CFStringRef name = CFDictionaryGetValue(fqdnDict, CFSTR("Domain"));
                    if (name)
                    {
                        if (!CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8) ||
                            !MakeDomainNameFromDNSNameString(fqdn, buf) || !fqdn->c[0])
                            LogMsg("GetUserSpecifiedDDNSConfig SCDynamicStore bad DDNS host name: %s", buf[0] ? buf : "(unknown)");
                        else 
                            debugf("GetUserSpecifiedDDNSConfig SCDynamicStore DDNS host name: %s", buf);
                    }
                }
            }
        }
        if (RegDomains)
        {
            CFArrayRef regArray = CFDictionaryGetValue(ddnsdict, CFSTR("RegistrationDomains"));
            if (regArray && CFArrayGetCount(regArray) > 0)
            {
                CFDictionaryRef regDict = CFArrayGetValueAtIndex(regArray, 0);
                if (regDict && DictionaryIsEnabled(regDict))
                {
                    CFStringRef name = CFDictionaryGetValue(regDict, CFSTR("Domain"));
                    if (name)
                    {
                        if (!CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8) ||
                            !MakeDomainNameFromDNSNameString(&d, buf) || !d.c[0])
                            LogMsg("GetUserSpecifiedDDNSConfig SCDynamicStore bad DDNS registration domain: %s", buf[0] ? buf : "(unknown)");
                        else
                        {
                            debugf("GetUserSpecifiedDDNSConfig SCDynamicStore DDNS registration domain: %s", buf);
                            AppendDNameListElem(&RegDomains, 0, &d);
                        }
                    }
                }
            }
        }
        if (BrowseDomains)
        {
            CFArrayRef browseArray = CFDictionaryGetValue(ddnsdict, CFSTR("BrowseDomains"));
            if (browseArray)
            {
                for (i = 0; i < CFArrayGetCount(browseArray); i++)
                {
                    CFDictionaryRef browseDict = CFArrayGetValueAtIndex(browseArray, i);
                    if (browseDict && DictionaryIsEnabled(browseDict))
                    {
                        CFStringRef name = CFDictionaryGetValue(browseDict, CFSTR("Domain"));
                        if (name)
                        {
                            if (!CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8) ||
                                !MakeDomainNameFromDNSNameString(&d, buf) || !d.c[0])
                                LogMsg("GetUserSpecifiedDDNSConfig SCDynamicStore bad DDNS browsing domain: %s", buf[0] ? buf : "(unknown)");
                            else
                            {
                                debugf("GetUserSpecifiedDDNSConfig SCDynamicStore DDNS browsing domain: %s", buf);
                                AppendDNameListElem(&BrowseDomains, 0, &d);
                            }
                        }
                    }
                }
            }
        }
        CFRelease(ddnsdict);
    }
    if (RegDomains)
    {
        CFDictionaryRef btmm = SCDynamicStoreCopyValue(NULL, NetworkChangedKey_BackToMyMac);
        if (btmm)
        {
            CFIndex size = CFDictionaryGetCount(btmm);
            const void *key[size];
            const void *val[size];
            CFDictionaryGetKeysAndValues(btmm, key, val);
            for (i = 0; i < size; i++)
            {
                LogInfo("BackToMyMac %d", i);
                if (!CFStringGetCString(key[i], buf, sizeof(buf), kCFStringEncodingUTF8))
                    LogMsg("Can't read BackToMyMac %d key %s", i, buf);
                else
                {
                    mDNSu32 uid = atoi(buf);
                    if (!CFStringGetCString(val[i], buf, sizeof(buf), kCFStringEncodingUTF8))
                        LogMsg("Can't read BackToMyMac %d val %s", i, buf);
                    else if (MakeDomainNameFromDNSNameString(&d, buf) && d.c[0])
                    {
                        LogInfo("BackToMyMac %d %d %##s", i, uid, d.c);
                        AppendDNameListElem(&RegDomains, uid, &d);
                    }
                }
            }
            CFRelease(btmm);
        }
    }

}

// Returns mDNSfalse, if it does not set the configuration i.e., if the DNS configuration did not change
mDNSexport mDNSBool mDNSPlatformSetDNSConfig(mDNS *const m, mDNSBool setservers, mDNSBool setsearch, domainname *const fqdn,
    DNameListElem **RegDomains, DNameListElem **BrowseDomains, mDNSBool ackConfig)
{
    MD5_CTX sdc;    // search domain context
    static mDNSu16 resolverGroupID = 0;

    // Need to set these here because we need to do this even if SCDynamicStoreCreate() or SCDynamicStoreCopyValue() below don't succeed
    if (fqdn         ) fqdn->c[0]      = 0;
    if (RegDomains   ) *RegDomains     = NULL;
    if (BrowseDomains) *BrowseDomains  = NULL;

    LogInfo("mDNSPlatformSetDNSConfig:%s%s%s%s%s",
            setservers    ? " setservers"    : "",
            setsearch     ? " setsearch"     : "",
            fqdn          ? " fqdn"          : "",
            RegDomains    ? " RegDomains"    : "",
            BrowseDomains ? " BrowseDomains" : "");

    if (setsearch) MD5_Init(&sdc);

    // Add the inferred address-based configuration discovery domains
    // (should really be in core code I think, not platform-specific)
    if (setsearch)
    {
        struct ifaddrs *ifa = mDNSNULL;
        struct sockaddr_in saddr;
        mDNSPlatformMemZero(&saddr, sizeof(saddr));
        saddr.sin_len = sizeof(saddr);
        saddr.sin_family = AF_INET;
        saddr.sin_port = 0;
        saddr.sin_addr.s_addr = *(in_addr_t *)&m->Router.ip.v4;

        // Don't add any reverse-IP search domains if doing the WAB bootstrap queries would cause dial-on-demand connection initiation
        if (!AddrRequiresPPPConnection((struct sockaddr *)&saddr)) ifa =  myGetIfAddrs(1);

        while (ifa)
        {
            mDNSAddr a, n;
            char buf[64];

            if (ifa->ifa_addr->sa_family == AF_INET &&
                ifa->ifa_netmask                    &&
                !(ifa->ifa_flags & IFF_LOOPBACK)    &&
                !SetupAddr(&a, ifa->ifa_addr)       &&
                !mDNSv4AddressIsLinkLocal(&a.ip.v4)  )
            {
                // Apparently it's normal for the sa_family of an ifa_netmask to sometimes be incorrect, so we explicitly fix it here before calling SetupAddr
                // <rdar://problem/5492035> getifaddrs is returning invalid netmask family for fw0 and vmnet
                ifa->ifa_netmask->sa_family = ifa->ifa_addr->sa_family;     // Make sure ifa_netmask->sa_family is set correctly
                SetupAddr(&n, ifa->ifa_netmask);
                // Note: This is reverse order compared to a normal dotted-decimal IP address, so we can't use our customary "%.4a" format code
                mDNS_snprintf(buf, sizeof(buf), "%d.%d.%d.%d.in-addr.arpa.", a.ip.v4.b[3] & n.ip.v4.b[3],
                              a.ip.v4.b[2] & n.ip.v4.b[2],
                              a.ip.v4.b[1] & n.ip.v4.b[1],
                              a.ip.v4.b[0] & n.ip.v4.b[0]);
                UpdateSearchDomainHash(m, &sdc, buf, NULL);
                mDNS_AddSearchDomain_CString(buf, mDNSNULL);
            }
            ifa = ifa->ifa_next;
        }
    }

#ifndef MDNS_NO_DNSINFO
    if (setservers || setsearch)
    {
        dns_config_t *config = dns_configuration_copy();
        if (!config)
        {
            // On 10.4, calls to dns_configuration_copy() early in the boot process often fail.
            // Apparently this is expected behaviour -- "not a bug".
            // Accordingly, we suppress syslog messages for the first three minutes after boot.
            // If we are still getting failures after three minutes, then we log them.
            if ((mDNSu32)mDNSPlatformRawTime() > (mDNSu32)(mDNSPlatformOneSecond * 180))
                LogMsg("mDNSPlatformSetDNSConfig: Error: dns_configuration_copy returned NULL");
        }
        else
        {
            LogInfo("mDNSPlatformSetDNSConfig: config->n_resolver = %d, generation %llu, last %llu", config->n_resolver, config->generation, m->p->LastConfigGeneration);
            if (m->p->LastConfigGeneration == config->generation)
            {
                LogInfo("mDNSPlatformSetDNSConfig: generation number %llu same, not processing", config->generation);
                dns_configuration_free(config);
                SetupDDNSDomains(fqdn, RegDomains, BrowseDomains);
                return mDNSfalse;
            }
#if APPLE_OSX_mDNSResponder
            SetupActiveDirectoryDomain(config);
#endif

            // With scoped DNS, we don't want to answer a non-scoped question using a scoped cache entry
            // and vice-versa. As we compare resolverGroupID for matching cache entry with question, we need
            // to make sure that they don't match. We ensure this by always bumping up resolverGroupID between
            // the two calls to ConfigResolvers DNSServers for scoped and non-scoped can never have the
            // same resolverGroupID.
            //
            // All non-scoped resolvers use the same resolverGroupID i.e, we treat them all equally.
            ConfigResolvers(m, config, kScopeNone, setsearch, setservers, &sdc, ++resolverGroupID);
            resolverGroupID += config->n_resolver;

            ConfigResolvers(m, config, kScopeInterfaceID, setsearch, setservers, &sdc, resolverGroupID);
            resolverGroupID += config->n_scoped_resolver;

            ConfigResolvers(m, config, kScopeServiceID, setsearch, setservers, &sdc, resolverGroupID);

            // Acking provides a hint that we processed this current configuration and
            // we will use that from now on, assuming we don't get another one immediately
            // after we return from here.
            if (ackConfig)
            {
                // Note: We have to set the generation number here when we are acking.
                // For every DNS configuration change, we do the following:
                //
                // 1) Copy dns configuration, handle search domains change
                // 2) Copy dns configuration, handle dns server change
                //
                // If we update the generation number at step (1), we won't process the
                // DNS servers the second time because generation number would be the same.
                // As we ack only when we process dns servers, we set the generation number
                // during acking.
                m->p->LastConfigGeneration = config->generation;
                LogInfo("mDNSPlatformSetDNSConfig: Acking configuration setservers %d, setsearch %d", setservers, setsearch);
                AckConfigd(m, config);
            }
            dns_configuration_free(config);
            if (setsearch) FinalizeSearchDomainHash(m, &sdc);
        }
    }
#endif // MDNS_NO_DNSINFO
    SetupDDNSDomains(fqdn, RegDomains, BrowseDomains);
    return mDNStrue;
}


mDNSexport mStatus mDNSPlatformGetPrimaryInterface(mDNS *const m, mDNSAddr *v4, mDNSAddr *v6, mDNSAddr *r)
{
    char buf[256];
    (void)m; // Unused
	
    CFDictionaryRef dict = SCDynamicStoreCopyValue(NULL, NetworkChangedKey_IPv4);
    if (dict)
    {
        r->type  = mDNSAddrType_IPv4;
        r->ip.v4 = zerov4Addr;
        CFStringRef string = CFDictionaryGetValue(dict, kSCPropNetIPv4Router);
        if (string)
        {
            if (!CFStringGetCString(string, buf, 256, kCFStringEncodingUTF8))
                LogMsg("Could not convert router to CString");
            else
            {
                struct sockaddr_in saddr;
                saddr.sin_len = sizeof(saddr);
                saddr.sin_family = AF_INET;
                saddr.sin_port = 0;
                inet_aton(buf, &saddr.sin_addr);
                *(in_addr_t *)&r->ip.v4 = saddr.sin_addr.s_addr;
            }
        }
        string = CFDictionaryGetValue(dict, kSCDynamicStorePropNetPrimaryInterface);
        if (string)
        {
            mDNSBool HavePrimaryGlobalv6 = mDNSfalse;  // does the primary interface have a global v6 address?
            struct ifaddrs *ifa = myGetIfAddrs(1);
            *v4 = *v6 = zeroAddr;

            if (!CFStringGetCString(string, buf, 256, kCFStringEncodingUTF8)) 
            { 
                LogMsg("Could not convert router to CString"); 
                goto exit; 
            }
            // find primary interface in list
            while (ifa && (mDNSIPv4AddressIsZero(v4->ip.v4) || mDNSv4AddressIsLinkLocal(&v4->ip.v4) || !HavePrimaryGlobalv6))
            {
                mDNSAddr tmp6 = zeroAddr;
                if (!strcmp(buf, ifa->ifa_name))
                {
                    if (ifa->ifa_addr->sa_family == AF_INET)
                    {
                        if (mDNSIPv4AddressIsZero(v4->ip.v4) || mDNSv4AddressIsLinkLocal(&v4->ip.v4)) 
                            SetupAddr(v4, ifa->ifa_addr);
                    }
                    else if (ifa->ifa_addr->sa_family == AF_INET6)
                    {
                        SetupAddr(&tmp6, ifa->ifa_addr);
                        if (tmp6.ip.v6.b[0] >> 5 == 1)   // global prefix: 001
                        { 
                            HavePrimaryGlobalv6 = mDNStrue; 
                            *v6 = tmp6; 
                        }
                    }
                }
                else
                {
                    // We'll take a V6 address from the non-primary interface if the primary interface doesn't have a global V6 address
                    if (!HavePrimaryGlobalv6 && ifa->ifa_addr->sa_family == AF_INET6 && !v6->ip.v6.b[0])
                    {
                        SetupAddr(&tmp6, ifa->ifa_addr);
                        if (tmp6.ip.v6.b[0] >> 5 == 1) 
                            *v6 = tmp6;
                    }
                }
                ifa = ifa->ifa_next;
            }
            // Note that while we advertise v6, we still require v4 (possibly NAT'd, but not link-local) because we must use
            // V4 to communicate w/ our DNS server
        }

exit:
        CFRelease(dict);
    }
    return mStatus_NoError;
}

mDNSexport void mDNSPlatformDynDNSHostNameStatusChanged(const domainname *const dname, const mStatus status)
{
    LogInfo("mDNSPlatformDynDNSHostNameStatusChanged %d %##s", status, dname->c);
    char uname[MAX_ESCAPED_DOMAIN_NAME];    // Max legal C-string name, including terminating NUL
    ConvertDomainNameToCString(dname, uname);

    char *p = uname;
    while (*p)
    {
        *p = tolower(*p);
        if (!(*(p+1)) && *p == '.') *p = 0; // if last character, strip trailing dot
        p++;
    }

    // We need to make a CFDictionary called "State:/Network/DynamicDNS" containing (at present) a single entity.
    // That single entity is a CFDictionary with name "HostNames".
    // The "HostNames" CFDictionary contains a set of name/value pairs, where the each name is the FQDN
    // in question, and the corresponding value is a CFDictionary giving the state for that FQDN.
    // (At present we only support a single FQDN, so this dictionary holds just a single name/value pair.)
    // The CFDictionary for each FQDN holds (at present) a single name/value pair,
    // where the name is "Status" and the value is a CFNumber giving an errror code (with zero meaning success).

    const CFStringRef StateKeys [1] = { CFSTR("HostNames") };
    const CFStringRef HostKeys  [1] = { CFStringCreateWithCString(NULL, uname, kCFStringEncodingUTF8) };
    const CFStringRef StatusKeys[1] = { CFSTR("Status") };
    if (!HostKeys[0]) LogMsg("SetDDNSNameStatus: CFStringCreateWithCString(%s) failed", uname);
    else
    {
        const CFNumberRef StatusVals[1] = { CFNumberCreate(NULL, kCFNumberSInt32Type, &status) };
        if (!StatusVals[0]) LogMsg("SetDDNSNameStatus: CFNumberCreate(%d) failed", status);
        else
        {
            const CFDictionaryRef HostVals[1] = { CFDictionaryCreate(NULL, (void*)StatusKeys, (void*)StatusVals, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks) };
            if (HostVals[0])
            {
                const CFDictionaryRef StateVals[1] = { CFDictionaryCreate(NULL, (void*)HostKeys, (void*)HostVals, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks) };
                if (StateVals[0])
                {
                    CFDictionaryRef StateDict = CFDictionaryCreate(NULL, (void*)StateKeys, (void*)StateVals, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                    if (StateDict)
                    {
                        mDNSDynamicStoreSetConfig(kmDNSDynamicConfig, mDNSNULL, StateDict);
                        CFRelease(StateDict);
                    }
                    CFRelease(StateVals[0]);
                }
                CFRelease(HostVals[0]);
            }
            CFRelease(StatusVals[0]);
        }
        CFRelease(HostKeys[0]);
    }
}

#if APPLE_OSX_mDNSResponder
#if !NO_AWACS

// checks whether a domain is present in Setup:/Network/BackToMyMac. Just because there is a key in the
// keychain for a domain, it does not become a valid BTMM domain. If things get inconsistent, this will
// help catch it
mDNSlocal mDNSBool IsBTMMDomain(domainname *d)
{
    SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("mDNSResponder:IsBTMMDomain"), NULL, NULL);
    if (!store)
    {
        LogMsg("IsBTMMDomain: SCDynamicStoreCreate failed: %s", SCErrorString(SCError()));
        return mDNSfalse;
    }
    CFDictionaryRef btmm = SCDynamicStoreCopyValue(store, NetworkChangedKey_BackToMyMac);
    if (btmm)
    {
        CFIndex size = CFDictionaryGetCount(btmm);
        char buf[MAX_ESCAPED_DOMAIN_NAME];  // Max legal C-string name, including terminating NUL
        const void *key[size];
        const void *val[size];
        domainname dom;
        int i;
        CFDictionaryGetKeysAndValues(btmm, key, val);
        for (i = 0; i < size; i++)
        {
            LogInfo("BackToMyMac %d", i);
            if (!CFStringGetCString(key[i], buf, sizeof(buf), kCFStringEncodingUTF8))
                LogMsg("IsBTMMDomain: ERROR!! Can't read BackToMyMac %d key %s", i, buf);
            else
            {
                mDNSu32 uid = atoi(buf);
                if (!CFStringGetCString(val[i], buf, sizeof(buf), kCFStringEncodingUTF8))
                    LogMsg("IsBTMMDomain: Can't read BackToMyMac %d val %s", i, buf);
                else if (MakeDomainNameFromDNSNameString(&dom, buf) && dom.c[0])
                {
                    if (SameDomainName(&dom, d))
                    {
                        LogInfo("IsBTMMDomain: Domain %##s is a btmm domain, uid %u", d->c, uid);
                        CFRelease(btmm);
                        CFRelease(store);
                        return mDNStrue;
                    }
                }
            }
        }
        CFRelease(btmm);
    }
    CFRelease(store);
    LogInfo("IsBTMMDomain: Domain %##s not a btmm domain", d->c);
    return mDNSfalse;
}

// Appends data to the buffer
mDNSlocal int AddOneItem(char *buf, int bufsz, char *data, int *currlen)
{
    int len;

    len = strlcpy(buf + *currlen, data, bufsz - *currlen);
    if (len >= (bufsz - *currlen))
    {
        // if we have exceeded the space in buf, it has already been NULL terminated
        // and we have nothing more to do. Set currlen to the last byte so that the caller
        // knows to do the right thing
        LogMsg("AddOneItem: Exceeded the max buffer size currlen %d, len %d", *currlen, len);
        *currlen = bufsz - 1;
        return -1;
    }
    else { (*currlen) += len; }

    buf[*currlen] = ',';
    if (*currlen >= bufsz)
    {
        LogMsg("AddOneItem: ERROR!! How can currlen be %d", *currlen);
        *currlen = bufsz - 1;
        buf[*currlen] = 0;
        return -1;
    }
    // if we have filled up the buffer exactly, then there is no more work to do
    if (*currlen == bufsz - 1) { buf[*currlen] = 0; return -1; }
    (*currlen)++;
    return *currlen;
}

// If we have at least one BTMM domain, then trigger the connection to the relay. If we have no
// BTMM domains, then bring down the connection to the relay.
mDNSlocal void UpdateBTMMRelayConnection(mDNS *const m)
{
    DomainAuthInfo *BTMMDomain = mDNSNULL;
    DomainAuthInfo *FoundInList;
    static mDNSBool AWACSDConnected = mDNSfalse;
    char AllUsers[1024];    // maximum size of mach message
    char AllPass[1024];     // maximum size of mach message
    char username[MAX_DOMAIN_LABEL + 1];
    int currulen = 0;
    int currplen = 0;

    // if a domain is being deleted, we want to send a disconnect. If we send a disconnect now,
    // we may not be able to send the dns queries over the relay connection which may be needed
    // for sending the deregistrations. Hence, we need to delay sending the disconnect. But we
    // need to make sure that we send the disconnect before attempting the next connect as the
    // awacs connections are redirected based on usernames.
    //
    // For now we send a disconnect immediately. When we start sending dns queries over the relay
    // connection, we will need to fix this.

    for (FoundInList = m->AuthInfoList; FoundInList; FoundInList = FoundInList->next)
        if (!FoundInList->deltime && FoundInList->AutoTunnel && IsBTMMDomain(&FoundInList->domain))
        {
            // We need the passwd from the first domain.
            BTMMDomain = FoundInList;
            ConvertDomainLabelToCString_unescaped((domainlabel *)BTMMDomain->domain.c, username);
            LogInfo("UpdateBTMMRelayConnection: user %s for domain %##s", username, BTMMDomain->domain.c);
            if (AddOneItem(AllUsers, sizeof(AllUsers), username, &currulen) == -1) break;
            if (AddOneItem(AllPass, sizeof(AllPass), BTMMDomain->b64keydata, &currplen) == -1) break;
        }

    if (BTMMDomain)
    {
        // In the normal case (where we neither exceed the buffer size nor write bytes that
        // fit exactly into the buffer), currulen/currplen should be a different size than
        // (AllUsers - 1) / (AllPass - 1). In that case, we need to override the "," with a NULL byte.

        if (currulen != (int)(sizeof(AllUsers) - 1)) AllUsers[currulen - 1] = 0;
        if (currplen != (int)(sizeof(AllPass) - 1)) AllPass[currplen - 1] = 0;

        LogInfo("UpdateBTMMRelayConnection: AWS_Connect for user %s", AllUsers);
        AWACS_Connect(AllUsers, AllPass, "hello.connectivity.me.com");
        AWACSDConnected = mDNStrue;
    }
    else
    {
        // Disconnect only if we connected previously
        if (AWACSDConnected)
        {
            LogInfo("UpdateBTMMRelayConnection: AWS_Disconnect");
            AWACS_Disconnect();
            AWACSDConnected = mDNSfalse;
        }
        else LogInfo("UpdateBTMMRelayConnection: Not calling AWS_Disconnect");
    }
}
#elif !TARGET_OS_EMBEDDED
mDNSlocal void UpdateBTMMRelayConnection(mDNS *const m)
{
    (void) m; // Unused
    LogInfo("UpdateBTMMRelayConnection: AWACS connection not started, no AWACS library");
}
#endif // ! NO_AWACS

#if !TARGET_OS_EMBEDDED
mDNSlocal void ProcessConndConfigChanges(mDNS *const m);
#endif

#endif // APPLE_OSX_mDNSResponder

// MUST be called holding the lock
mDNSlocal void SetDomainSecrets_internal(mDNS *m)
{
#ifdef NO_SECURITYFRAMEWORK
        (void) m;
    LogMsg("Note: SetDomainSecrets: no keychain support");
#else
    mDNSBool haveAutoTunnels = mDNSfalse;

    LogInfo("SetDomainSecrets");

    // Rather than immediately deleting all keys now, we mark them for deletion in ten seconds.
    // In the case where the user simultaneously removes their DDNS host name and the key
    // for it, this gives mDNSResponder ten seconds to gracefully delete the name from the
    // server before it loses access to the necessary key. Otherwise, we'd leave orphaned
    // address records behind that we no longer have permission to delete.
    DomainAuthInfo *ptr;
    for (ptr = m->AuthInfoList; ptr; ptr = ptr->next)
        ptr->deltime = NonZeroTime(m->timenow + mDNSPlatformOneSecond*10);

#if APPLE_OSX_mDNSResponder
    {
        // Mark all TunnelClients for deletion
        ClientTunnel *client;
        for (client = m->TunnelClients; client; client = client->next)
        {
            LogInfo("SetDomainSecrets: tunnel to %##s marked for deletion", client->dstname.c);
            client->MarkedForDeletion = mDNStrue;
        }
    }
#endif // APPLE_OSX_mDNSResponder

    // String Array used to write list of private domains to Dynamic Store
    CFMutableArrayRef sa = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (!sa) { LogMsg("SetDomainSecrets: CFArrayCreateMutable failed"); return; }
    CFIndex i;
    CFDataRef data = NULL;
    const int itemsPerEntry = 4; // domain name, key name, key value, Name value
    CFArrayRef secrets = NULL;
    int err = mDNSKeychainGetSecrets(&secrets);
    if (err || !secrets)
        LogMsg("SetDomainSecrets: mDNSKeychainGetSecrets failed error %d CFArrayRef %p", err, secrets);
    else
    {
        CFIndex ArrayCount = CFArrayGetCount(secrets);
        // Iterate through the secrets
        for (i = 0; i < ArrayCount; ++i)
        {
            mDNSBool AutoTunnel;
            int j, offset;
            CFArrayRef entry = CFArrayGetValueAtIndex(secrets, i);
            if (CFArrayGetTypeID() != CFGetTypeID(entry) || itemsPerEntry != CFArrayGetCount(entry))
            { LogMsg("SetDomainSecrets: malformed entry %d, itemsPerEntry %d", i, itemsPerEntry); continue; }
            for (j = 0; j < CFArrayGetCount(entry); ++j)
                if (CFDataGetTypeID() != CFGetTypeID(CFArrayGetValueAtIndex(entry, j)))
                { LogMsg("SetDomainSecrets: malformed entry item %d", j); continue; }

            // The names have already been vetted by the helper, but checking them again here helps humans and automated tools verify correctness

            // Max legal domainname as C-string, including space for btmmprefix and terminating NUL
            // Get DNS domain this key is for (kmDNSKcWhere)
            char stringbuf[MAX_ESCAPED_DOMAIN_NAME + sizeof(btmmprefix)];
            data = CFArrayGetValueAtIndex(entry, kmDNSKcWhere);
            if (CFDataGetLength(data) >= (int)sizeof(stringbuf))
            { LogMsg("SetDomainSecrets: Bad kSecServiceItemAttr length %d", CFDataGetLength(data)); continue; }
            CFDataGetBytes(data, CFRangeMake(0, CFDataGetLength(data)), (UInt8 *)stringbuf);
            stringbuf[CFDataGetLength(data)] = '\0';

            AutoTunnel = mDNSfalse;
            offset = 0;
            if (!strncmp(stringbuf, dnsprefix, strlen(dnsprefix)))
                offset = strlen(dnsprefix);
            else if (!strncmp(stringbuf, btmmprefix, strlen(btmmprefix)))
            {
                AutoTunnel = mDNStrue;
                offset = strlen(btmmprefix);
            }
            domainname domain;
            if (!MakeDomainNameFromDNSNameString(&domain, stringbuf + offset)) { LogMsg("SetDomainSecrets: bad key domain %s", stringbuf); continue; }

            // Get key name (kmDNSKcAccount)
            data = CFArrayGetValueAtIndex(entry, kmDNSKcAccount);
            if (CFDataGetLength(data) >= (int)sizeof(stringbuf))
            { LogMsg("SetDomainSecrets: Bad kSecAccountItemAttr length %d", CFDataGetLength(data)); continue; }
            CFDataGetBytes(data, CFRangeMake(0,CFDataGetLength(data)), (UInt8 *)stringbuf);
            stringbuf[CFDataGetLength(data)] = '\0';

            domainname keyname;
            if (!MakeDomainNameFromDNSNameString(&keyname, stringbuf)) { LogMsg("SetDomainSecrets: bad key name %s", stringbuf); continue; }

            // Get key data (kmDNSKcKey)
            data = CFArrayGetValueAtIndex(entry, kmDNSKcKey);
            if (CFDataGetLength(data) >= (int)sizeof(stringbuf))
            { 
                LogMsg("SetDomainSecrets: Shared secret too long: %d", CFDataGetLength(data));
                continue;
            }
            CFDataGetBytes(data, CFRangeMake(0, CFDataGetLength(data)), (UInt8 *)stringbuf);
            stringbuf[CFDataGetLength(data)] = '\0';    // mDNS_SetSecretForDomain requires NULL-terminated C string for key

            // Get the Name of the keychain entry (kmDNSKcName) host or host:port
            // The hostname also has the port number and ":". It should take a maximum of 6 bytes.
            char hostbuf[MAX_ESCAPED_DOMAIN_NAME + 6];  // Max legal domainname as C-string, including terminating NUL
            data = CFArrayGetValueAtIndex(entry, kmDNSKcName);
            if (CFDataGetLength(data) >= (int)sizeof(hostbuf))
            { 
                LogMsg("SetDomainSecrets: host:port data too long: %d", CFDataGetLength(data));
                continue;
            }
            CFDataGetBytes(data, CFRangeMake(0,CFDataGetLength(data)), (UInt8 *)hostbuf);
            hostbuf[CFDataGetLength(data)] = '\0';

            domainname hostname;
            mDNSIPPort port;
            char *hptr;
            hptr = strchr(hostbuf, ':');

            port.NotAnInteger = 0;
            if (hptr)
            {
                mDNSu8 *p;
                mDNSu16 val = 0;

                *hptr++ = '\0';
                while(hptr && *hptr != 0)
                {
                    if (*hptr < '0' || *hptr > '9')
                    { LogMsg("SetDomainSecrets: Malformed Port number %d, val %d", *hptr, val); val = 0; break;}
                    val = val * 10 + *hptr - '0';
                    hptr++;
                }
                if (!val) continue;
                p = (mDNSu8 *)&val;
                port.NotAnInteger = p[0] << 8 | p[1];
            }
            // The hostbuf is of the format dsid@hostname:port. We don't care about the dsid.
            hptr = strchr(hostbuf, '@');
            if (hptr)
                hptr++;
            else
                hptr = hostbuf;
            if (!MakeDomainNameFromDNSNameString(&hostname, hptr)) { LogMsg("SetDomainSecrets: bad host name %s", hptr); continue; }

            DomainAuthInfo *FoundInList;
            for (FoundInList = m->AuthInfoList; FoundInList; FoundInList = FoundInList->next)
                if (SameDomainName(&FoundInList->domain, &domain)) break;

#if APPLE_OSX_mDNSResponder
            if (FoundInList)
            {
                // If any client tunnel destination is in this domain, set deletion flag to false
                ClientTunnel *client;
                for (client = m->TunnelClients; client; client = client->next)
                    if (FoundInList == GetAuthInfoForName_internal(m, &client->dstname))
                    {
                        LogInfo("SetDomainSecrets: tunnel to %##s no longer marked for deletion", client->dstname.c);
                        client->MarkedForDeletion = mDNSfalse;
                    }
            }

#endif // APPLE_OSX_mDNSResponder

            // Uncomment the line below to view the keys as they're read out of the system keychain
            // DO NOT SHIP CODE THIS WAY OR YOU'LL LEAK SECRET DATA INTO A PUBLICLY READABLE FILE!
            //LogInfo("SetDomainSecrets: domain %##s keyname %##s key %s hostname %##s port %d", &domain.c, &keyname.c, stringbuf, hostname.c, (port.b[0] << 8 | port.b[1]));
            LogInfo("SetDomainSecrets: domain %##s keyname %##s hostname %##s port %d", &domain.c, &keyname.c, hostname.c, (port.b[0] << 8 | port.b[1]));

            // If didn't find desired domain in the list, make a new entry
            ptr = FoundInList;
            if (FoundInList && FoundInList->AutoTunnel && haveAutoTunnels == mDNSfalse) haveAutoTunnels = mDNStrue;
            if (!FoundInList)
            {
                ptr = (DomainAuthInfo*)mallocL("DomainAuthInfo", sizeof(*ptr));
                if (!ptr) { LogMsg("SetDomainSecrets: No memory"); continue; }
            }

            //LogInfo("SetDomainSecrets: %d of %d %##s", i, ArrayCount, &domain);

            // It is an AutoTunnel if the keychains tells us so (with btmm prefix) or if it is a TunnelModeDomain
            if (mDNS_SetSecretForDomain(m, ptr, &domain, &keyname, stringbuf, &hostname, &port, AutoTunnel) == mStatus_BadParamErr)
            {
                if (!FoundInList) mDNSPlatformMemFree(ptr);     // If we made a new DomainAuthInfo here, and it turned out bad, dispose it immediately
                continue;
            }

            ConvertDomainNameToCString(&domain, stringbuf);
            CFStringRef cfs = CFStringCreateWithCString(NULL, stringbuf, kCFStringEncodingUTF8);
            if (cfs) { CFArrayAppendValue(sa, cfs); CFRelease(cfs); }
        }
        CFRelease(secrets);
    }

    if (!privateDnsArray || !CFEqual(privateDnsArray, sa))
    {
        if (privateDnsArray)
            CFRelease(privateDnsArray);
        
        privateDnsArray = sa;
        CFRetain(privateDnsArray);
        mDNSDynamicStoreSetConfig(kmDNSPrivateConfig, mDNSNULL, privateDnsArray);
    }
    CFRelease(sa);

#if APPLE_OSX_mDNSResponder
    {
        // clean up ClientTunnels
        ClientTunnel **pp = &m->TunnelClients;
        while (*pp)
        {
            if ((*pp)->MarkedForDeletion)
            {
                ClientTunnel *cur = *pp;
                LogInfo("SetDomainSecrets: removing client %p %##s from list", cur, cur->dstname.c);
                if (cur->q.ThisQInterval >= 0) mDNS_StopQuery(m, &cur->q);
                AutoTunnelSetKeys(cur, mDNSfalse);
                *pp = cur->next;
                freeL("ClientTunnel", cur);
            }
            else
                pp = &(*pp)->next;
        }

        mDNSBool needAutoTunnelNAT = mDNSfalse;
        DomainAuthInfo *info;
        for (info = m->AuthInfoList; info; info = info->next)
        {
            if (info->AutoTunnel)
            {
                UpdateAutoTunnelDeviceInfoRecord(m, info);
                UpdateAutoTunnelHostRecord(m, info);
                UpdateAutoTunnelServiceRecords(m, info);
                UpdateAutoTunnel6Record(m, info);
                if (info->deltime)
                {
                    if (info->AutoTunnelServiceStarted) info->AutoTunnelServiceStarted = mDNSfalse;
                }
                else if (info->AutoTunnelServiceStarted)
                    needAutoTunnelNAT = true;

	            UpdateAutoTunnelDomainStatus(m, info);
            }
        }

        // If the AutoTunnel NAT-T is no longer needed (& is currently running), stop it
        if (!needAutoTunnelNAT && m->AutoTunnelNAT.clientContext)
        {
            // stop the NAT operation, reset port, cleanup state
            mDNS_StopNATOperation_internal(m, &m->AutoTunnelNAT);
            m->AutoTunnelNAT.ExternalAddress = zerov4Addr;
            m->AutoTunnelNAT.NewAddress      = zerov4Addr;
            m->AutoTunnelNAT.ExternalPort    = zeroIPPort;
            m->AutoTunnelNAT.RequestedPort   = zeroIPPort;
            m->AutoTunnelNAT.Lifetime        = 0;
            m->AutoTunnelNAT.Result          = mStatus_NoError;
            m->AutoTunnelNAT.clientContext   = mDNSNULL;
        }

        UpdateAnonymousRacoonConfig(m);     // Determine whether we need racoon to accept incoming connections
        ProcessConndConfigChanges(m);       // Update AutoTunnelInnerAddress values and default ipsec policies as necessary
    }
#endif // APPLE_OSX_mDNSResponder

    CheckSuppressUnusableQuestions(m);

#endif /* NO_SECURITYFRAMEWORK */
}

mDNSexport void SetDomainSecrets(mDNS *m)
{
#if DEBUG
    // Don't get secrets for BTMM if running in debug mode
    if (!IsDebugSocketInUse())
#endif
    SetDomainSecrets_internal(m);
}

mDNSlocal void SetLocalDomains(void)
{
    CFMutableArrayRef sa = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (!sa) { LogMsg("SetLocalDomains: CFArrayCreateMutable failed"); return; }

    CFArrayAppendValue(sa, CFSTR("local"));
    CFArrayAppendValue(sa, CFSTR("254.169.in-addr.arpa"));
    CFArrayAppendValue(sa, CFSTR("8.e.f.ip6.arpa"));
    CFArrayAppendValue(sa, CFSTR("9.e.f.ip6.arpa"));
    CFArrayAppendValue(sa, CFSTR("a.e.f.ip6.arpa"));
    CFArrayAppendValue(sa, CFSTR("b.e.f.ip6.arpa"));

    mDNSDynamicStoreSetConfig(kmDNSMulticastConfig, mDNSNULL, sa);
    CFRelease(sa);
}

mDNSlocal void GetCurrentPMSetting(const CFStringRef name, mDNSs32 *val)
{
	
    CFDictionaryRef dict = SCDynamicStoreCopyValue(NULL, NetworkChangedKey_PowerSettings);
    if (!dict)
    {
        LogSPS("GetCurrentPMSetting: Could not get IOPM CurrentSettings dict");
    }
    else
    {
        CFNumberRef number = CFDictionaryGetValue(dict, name);
        if (!number || CFGetTypeID(number) != CFNumberGetTypeID() || !CFNumberGetValue(number, kCFNumberSInt32Type, val))
            *val = 0;
        CFRelease(dict);
    }
	
}

#if APPLE_OSX_mDNSResponder

static CFMutableDictionaryRef spsStatusDict = NULL;
static const CFStringRef kMetricRef = CFSTR("Metric");

mDNSlocal void SPSStatusPutNumber(CFMutableDictionaryRef dict, const mDNSu8* const ptr, CFStringRef key)
{
    mDNSu8 tmp = (ptr[0] - '0') * 10 + ptr[1] - '0';
    CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt8Type, &tmp);
    if (!num)
        LogMsg("SPSStatusPutNumber: Could not create CFNumber");
    else
    {
        CFDictionarySetValue(dict, key, num);
        CFRelease(num);
    }
}

mDNSlocal CFMutableDictionaryRef SPSCreateDict(const mDNSu8* const ptr)
{
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!dict) { LogMsg("SPSCreateDict: Could not create CFDictionary dict"); return dict; }

    char buffer[1024];
    buffer[mDNS_snprintf(buffer, sizeof(buffer), "%##s", ptr) - 1] = 0;
    CFStringRef spsname = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
    if (!spsname) { LogMsg("SPSCreateDict: Could not create CFString spsname full"); CFRelease(dict); return NULL; }
    CFDictionarySetValue(dict, CFSTR("FullName"), spsname);
    CFRelease(spsname);

    if (ptr[0] >=  2) SPSStatusPutNumber(dict, ptr + 1, CFSTR("Type"));
    if (ptr[0] >=  5) SPSStatusPutNumber(dict, ptr + 4, CFSTR("Portability"));
    if (ptr[0] >=  8) SPSStatusPutNumber(dict, ptr + 7, CFSTR("MarginalPower"));
    if (ptr[0] >= 11) SPSStatusPutNumber(dict, ptr +10, CFSTR("TotalPower"));

    mDNSu32 tmp = SPSMetric(ptr);
    CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt32Type, &tmp);
    if (!num)
        LogMsg("SPSCreateDict: Could not create CFNumber");
    else
    {
        CFDictionarySetValue(dict, kMetricRef, num);
        CFRelease(num);
    }

    if (ptr[0] >= 12)
    {
        memcpy(buffer, ptr + 13, ptr[0] - 12);
        buffer[ptr[0] - 12] = 0;
        spsname = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
        if (!spsname) { LogMsg("SPSCreateDict: Could not create CFString spsname"); CFRelease(dict); return NULL; }
        else
        {
            CFDictionarySetValue(dict, CFSTR("PrettyName"), spsname);
            CFRelease(spsname);
        }
    }

    return dict;
}

mDNSlocal CFComparisonResult CompareSPSEntries(const void *val1, const void *val2, void *context)
{
    (void)context;
    return CFNumberCompare((CFNumberRef)CFDictionaryGetValue((CFDictionaryRef)val1, kMetricRef),
                           (CFNumberRef)CFDictionaryGetValue((CFDictionaryRef)val2, kMetricRef),
                           NULL);
}

mDNSlocal void UpdateSPSStatus(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
{
    NetworkInterfaceInfo* info = (NetworkInterfaceInfo*)question->QuestionContext;
    debugf("UpdateSPSStatus: %s %##s %s %s", info->ifname, question->qname.c, AddRecord ? "Add" : "Rmv", answer ? RRDisplayString(m, answer) : "<null>");

    mDNS_Lock(m);
    mDNS_UpdateAllowSleep(m);
    mDNS_Unlock(m);

    if (answer && SPSMetric(answer->rdata->u.name.c) > 999999) return;  // Ignore instances with invalid names

    if (!spsStatusDict)
    {
        spsStatusDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (!spsStatusDict) { LogMsg("UpdateSPSStatus: Could not create CFDictionary spsStatusDict"); return; }
    }

    CFStringRef ifname = CFStringCreateWithCString(NULL, info->ifname, kCFStringEncodingUTF8);
    if (!ifname) { LogMsg("UpdateSPSStatus: Could not create CFString ifname"); return; }

    CFMutableArrayRef array = NULL;

    if (!CFDictionaryGetValueIfPresent(spsStatusDict, ifname, (const void**) &array))
    {
        array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        if (!array) { LogMsg("UpdateSPSStatus: Could not create CFMutableArray"); CFRelease(ifname); return; }
        CFDictionarySetValue(spsStatusDict, ifname, array);
        CFRelease(array); // let go of our reference, now that the dict has one
    }
    else
    if (!array) { LogMsg("UpdateSPSStatus: Could not get CFMutableArray for %s", info->ifname); CFRelease(ifname); return; }

    if (!answer) // special call that means the question has been stopped (because the interface is going away)
        CFArrayRemoveAllValues(array);
    else
    {
        CFMutableDictionaryRef dict = SPSCreateDict(answer->rdata->u.name.c);
        if (!dict) { CFRelease(ifname); return; }

        if (AddRecord)
        {
            if (!CFArrayContainsValue(array, CFRangeMake(0, CFArrayGetCount(array)), dict))
            {
                int i=0;
                for (i=0; i<CFArrayGetCount(array); i++)
                    if (CompareSPSEntries(CFArrayGetValueAtIndex(array, i), dict, NULL) != kCFCompareLessThan)
                        break;
                CFArrayInsertValueAtIndex(array, i, dict);
            }
            else LogMsg("UpdateSPSStatus: %s array already contains %##s", info->ifname, answer->rdata->u.name.c);
        }
        else
        {
            CFIndex i = CFArrayGetFirstIndexOfValue(array, CFRangeMake(0, CFArrayGetCount(array)), dict);
            if (i != -1) CFArrayRemoveValueAtIndex(array, i);
            else LogMsg("UpdateSPSStatus: %s array does not contain %##s", info->ifname, answer->rdata->u.name.c);
        }

        CFRelease(dict);
    }

    if (!m->ShutdownTime) mDNSDynamicStoreSetConfig(kmDNSSleepProxyServersState, info->ifname, array);

    CFRelease(ifname);
}

mDNSlocal mDNSs32 GetSystemSleepTimerSetting(void)
{
    mDNSs32 val = -1;
    SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("mDNSResponder:GetSystemSleepTimerSetting"), NULL, NULL);
    if (!store)
        LogMsg("GetSystemSleepTimerSetting: SCDynamicStoreCreate failed: %s", SCErrorString(SCError()));
    else
    {
        CFDictionaryRef dict = SCDynamicStoreCopyValue(store, NetworkChangedKey_PowerSettings);
        if (dict)
        {
            CFNumberRef number = CFDictionaryGetValue(dict, CFSTR("System Sleep Timer"));
            if (number) CFNumberGetValue(number, kCFNumberSInt32Type, &val);
            CFRelease(dict);
        }
        CFRelease(store);
    }
    return val;
}

mDNSlocal void SetSPS(mDNS *const m)
{
    
    // If we ever want to know InternetSharing status in the future, use DNSXEnableProxy()
    mDNSu8 sps = (OfferSleepProxyService && GetSystemSleepTimerSetting() == 0) ? mDNSSleepProxyMetric_IncidentalSoftware : 0;

    // For devices that are not running NAT, but are set to never sleep, we may choose to act
    // as a Sleep Proxy, but only for non-portable Macs (Portability > 35 means nominal weight < 3kg)
    //if (sps > mDNSSleepProxyMetric_PrimarySoftware && SPMetricPortability > 35) sps = 0;

    // If we decide to let laptops act as Sleep Proxy, we should do it only when running on AC power, not on battery

    // For devices that are unable to sleep at all to save power, or save 1W or less by sleeping,
    // it makes sense for them to offer low-priority Sleep Proxy service on the network.
    // We rate such a device as metric 70 ("Incidentally Available Hardware")
    if (SPMetricMarginalPower <= 60 && !sps) sps = mDNSSleepProxyMetric_IncidentalHardware;

    // If the launchd plist specifies an explicit value for the Intent Metric, then use that instead of the
    // computed value (currently 40 "Primary Network Infrastructure Software" or 80 "Incidentally Available Software")
    if (sps && OfferSleepProxyService && OfferSleepProxyService < 100) sps = OfferSleepProxyService;

#ifdef NO_APPLETV_SLEEP_PROXY_ON_WIFI
    // AppleTVs are not reliable sleep proxy servers on WiFi. Do not offer to be a BSP if the WiFi interface is active.
    if (IsAppleTV())
    {
        NetworkInterfaceInfo *intf  = mDNSNULL;
        mDNSEthAddr           bssid = zeroEthAddr;
        for (intf = GetFirstActiveInterface(m->HostInterfaces); intf; intf = GetFirstActiveInterface(intf->next))
        {
            if (intf->InterfaceID == AWDLInterfaceID) continue;
            bssid = GetBSSID(intf->ifname);
            if (!mDNSSameEthAddress(&bssid, &zeroEthAddr))
            {
                LogMsg("SetSPS: AppleTV on WiFi - not advertising BSP services");
                sps = 0;
                break;
            }
        }
    }
#endif  //  NO_APPLETV_SLEEP_PROXY_ON_WIFI

    mDNSCoreBeSleepProxyServer(m, sps, SPMetricPortability, SPMetricMarginalPower, SPMetricTotalPower, SPMetricFeatures);
}

// The definitions below should eventually come from some externally-supplied header file.
// However, since these definitions can't really be changed without breaking binary compatibility,
// they should never change, so in practice it should not be a big problem to have them defined here.

enum
{                               // commands from the daemon to the driver
    cmd_mDNSOffloadRR = 21,     // give the mdns update buffer to the driver
};

typedef union { void *ptr; mDNSOpaque64 sixtyfourbits; } FatPtr;

typedef struct
{                                       // cmd_mDNSOffloadRR structure
    uint32_t command;                 // set to OffloadRR
    uint32_t rrBufferSize;            // number of bytes of RR records
    uint32_t numUDPPorts;             // number of SRV UDP ports
    uint32_t numTCPPorts;             // number of SRV TCP ports
    uint32_t numRRRecords;            // number of RR records
    uint32_t compression;             // rrRecords - compression is base for compressed strings
    FatPtr rrRecords;                 // address of array of pointers to the rr records
    FatPtr udpPorts;                  // address of udp port list (SRV)
    FatPtr tcpPorts;                  // address of tcp port list (SRV)
} mDNSOffloadCmd;

#include <IOKit/IOKitLib.h>
#include <dns_util.h>

mDNSlocal mDNSu16 GetPortArray(mDNS *const m, int trans, mDNSIPPort *portarray)
{
    const domainlabel *const tp = (trans == mDNSTransport_UDP) ? (const domainlabel *)"\x4_udp" : (const domainlabel *)"\x4_tcp";
    int   count = 0;

    AuthRecord *rr;
    for (rr = m->ResourceRecords; rr; rr=rr->next)
    {
        if (rr->resrec.rrtype == kDNSType_SRV && SameDomainLabel(ThirdLabel(rr->resrec.name)->c, tp->c))
        {
            if (!portarray)
                count++;
            else
            {
                int i;
                for (i = 0; i < count; i++)
                    if (mDNSSameIPPort(portarray[i], rr->resrec.rdata->u.srv.port))
                        break;

                // Add it into the port list only if it not already present in the list
                if (i >= count)
                    portarray[count++] = rr->resrec.rdata->u.srv.port;
            }
        }
    }

    // If Back to My Mac is on, also wake for packets to the IPSEC UDP port (4500)
    if (trans == mDNSTransport_UDP && m->AutoTunnelNAT.clientContext)
    {
        LogSPS("GetPortArray Back to My Mac at %d", count);
        if (portarray) portarray[count] = IPSECPort;
        count++;
    }
    return(count);
}

#if APPLE_OSX_mDNSResponder && !TARGET_OS_EMBEDDED
mDNSlocal mDNSBool SupportsTCPKeepAlive()
{
    IOReturn  ret      = kIOReturnSuccess;
    CFTypeRef obj      = NULL;
    mDNSBool  supports = mDNSfalse;

    ret = IOPlatformCopyFeatureActive(CFSTR("TCPKeepAliveDuringSleep"), &obj);
    if ((kIOReturnSuccess == ret) && (obj != NULL))
    {
        supports = (obj ==  kCFBooleanTrue)? mDNStrue : mDNSfalse;
        CFRelease(obj);
    }
    LogSPS("%s: The hardware %s TCP Keep Alive", __func__, (supports ? "supports" : "does not support"));
    return supports;
}

mDNSlocal mDNSBool OnBattery(void)
{
    CFTypeRef powerInfo = IOPSCopyPowerSourcesInfo();
    CFTypeRef powerSrc  = IOPSGetProvidingPowerSourceType(powerInfo);
    mDNSBool  result    = mDNSfalse;

    if (powerInfo != NULL)
    {
        result = CFEqual(CFSTR(kIOPSBatteryPowerValue), powerSrc);
        CFRelease(powerInfo);
    }
    LogSPS("%s: The system is on %s", __func__, (result)? "Battery" : "AC Power");
    return result;
}

#endif // !TARGET_OS_EMBEDDED

#define TfrRecordToNIC(RR) \
    ((!(RR)->resrec.InterfaceID && ((RR)->ForceMCast || IsLocalDomain((RR)->resrec.name))))

mDNSlocal mDNSu32 CountProxyRecords(mDNS *const m, uint32_t *const numbytes, NetworkInterfaceInfo *const intf, mDNSBool TCPKAOnly, mDNSBool supportsTCPKA)
{
    *numbytes = 0;
    int count = 0;

    AuthRecord *rr;

    for (rr = m->ResourceRecords; rr; rr=rr->next)
    {
        if (!(rr->AuthFlags & AuthFlagsWakeOnly) && rr->resrec.RecordType > kDNSRecordTypeDeregistering)
        {
#if APPLE_OSX_mDNSResponder && !TARGET_OS_EMBEDDED
            mDNSBool   isKeepAliveRecord = mDNS_KeepaliveRecord(&rr->resrec);
            // Skip over all other records if we are registering TCP KeepAlive records only
            // Skip over TCP KeepAlive records if the policy prohibits it or if the interface does not support TCP Keepalive.
            if ((TCPKAOnly && !isKeepAliveRecord) || (isKeepAliveRecord && !supportsTCPKA))
                continue;

            // Update the record before calculating the number of bytes required
            // We offload the TCP Keepalive record even if the update fails. When the driver gets the record, it will
            // attempt to update the record again.
            if (isKeepAliveRecord && (UpdateKeepaliveRData(m, rr, intf, mDNSfalse, mDNSNULL) != mStatus_NoError))
                LogSPS("CountProxyRecords: Failed to update keepalive record - %s", ARDisplayString(m, rr));

			// Offload only Valid Keepalive records
			if (isKeepAliveRecord && !mDNSValidKeepAliveRecord(rr))
				continue;
#else
            (void) TCPKAOnly;     // unused
            (void) supportsTCPKA; // unused
            (void) intf;          // unused
#endif // APPLE_OSX_mDNSResponder
            if (TfrRecordToNIC(rr))
            {
                *numbytes += DomainNameLength(rr->resrec.name) + 10 + rr->resrec.rdestimate;
                LogSPS("CountProxyRecords: %3d size %5d total %5d %s",
                       count, DomainNameLength(rr->resrec.name) + 10 + rr->resrec.rdestimate, *numbytes, ARDisplayString(m,rr));
                count++;
            }
        }
    }
    return(count);
}

mDNSlocal void GetProxyRecords(mDNS *const m, DNSMessage *const msg, uint32_t *const numbytes, FatPtr *const records, mDNSBool TCPKAOnly, mDNSBool supportsTCPKA)
{
    mDNSu8 *p = msg->data;
    const mDNSu8 *const limit = p + *numbytes;
    InitializeDNSMessage(&msg->h, zeroID, zeroID);

    int count = 0;
    AuthRecord *rr;

    for (rr = m->ResourceRecords; rr; rr=rr->next)
    {
        if (!(rr->AuthFlags & AuthFlagsWakeOnly) && rr->resrec.RecordType > kDNSRecordTypeDeregistering)
        {
#if APPLE_OSX_mDNSResponder && !TARGET_OS_EMBEDDED
            mDNSBool   isKeepAliveRecord = mDNS_KeepaliveRecord(&rr->resrec);

            // Skip over all other records if we are registering TCP KeepAlive records only
            // Skip over TCP KeepAlive records if the policy prohibits it or if the interface does not support TCP Keepalive
            if ((TCPKAOnly && !isKeepAliveRecord) || (isKeepAliveRecord && !supportsTCPKA))
                continue;

			// Offload only Valid Keepalive records
			if (isKeepAliveRecord && !mDNSValidKeepAliveRecord(rr))
				continue;
#else
            (void) TCPKAOnly;     // unused
            (void) supportsTCPKA; // unused
#endif // APPLE_OSX_mDNSResponder

            if (TfrRecordToNIC(rr))
            {
                records[count].sixtyfourbits = zeroOpaque64;
                records[count].ptr = p;
                if (rr->resrec.RecordType & kDNSRecordTypeUniqueMask)
                    rr->resrec.rrclass |= kDNSClass_UniqueRRSet;    // Temporarily set the 'unique' bit so PutResourceRecord will set it
                p = PutResourceRecordTTLWithLimit(msg, p, &msg->h.mDNS_numUpdates, &rr->resrec, rr->resrec.rroriginalttl, limit);
                rr->resrec.rrclass &= ~kDNSClass_UniqueRRSet;       // Make sure to clear 'unique' bit back to normal state
                LogSPS("GetProxyRecords: %3d start %p end %p size %5d total %5d %s",
                       count, records[count].ptr, p, p - (mDNSu8 *)records[count].ptr, p - msg->data, ARDisplayString(m,rr));
                count++;
            }
        }
    }
    *numbytes = p - msg->data;
}

mDNSexport mDNSBool SupportsInNICProxy(NetworkInterfaceInfo *const intf)
{
    if(!UseInternalSleepProxy)
    {
        LogMsg("SupportsInNICProxy: Internal Sleep Proxy is disabled");
        return mDNSfalse;
    }
    return CheckInterfaceSupport(intf, mDNS_IOREG_KEY);
}

mDNSexport mStatus ActivateLocalProxy(mDNS *const m, NetworkInterfaceInfo *const intf, mDNSBool *keepaliveOnly)  // Called with the lock held
{
    mStatus      result        = mStatus_UnknownErr;
    mDNSBool     TCPKAOnly     = mDNSfalse;
    mDNSBool     supportsTCPKA = mDNSfalse;
    mDNSBool     onbattery     = mDNSfalse;
    io_service_t service       = IOServiceGetMatchingService(kIOMasterPortDefault, IOBSDNameMatching(kIOMasterPortDefault, 0, intf->ifname));

#if APPLE_OSX_mDNSResponder && !TARGET_OS_EMBEDDED
    onbattery = OnBattery();
    // Check if the interface supports TCP Keepalives and the system policy says it is ok to offload TCP Keepalive records
    supportsTCPKA = (InterfaceSupportsKeepAlive(intf) && SupportsTCPKeepAlive());

    // Only TCP Keepalive records are to be offloaded if
    // - The system is on battery
    // - OR wake for network access is not set but powernap is enabled
    TCPKAOnly     = supportsTCPKA && ((m->SystemWakeOnLANEnabled == mDNS_WakeOnBattery) || onbattery);
#else
    (void) onbattery; // unused;
#endif
    if (!service) { LogMsg("ActivateLocalProxy: No service for interface %s", intf->ifname); return(mStatus_UnknownErr); }

    io_name_t n1, n2;
    IOObjectGetClass(service, n1);
    io_object_t parent;
    kern_return_t kr = IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent);
    if (kr != KERN_SUCCESS) LogMsg("ActivateLocalProxy: IORegistryEntryGetParentEntry for %s/%s failed %d", intf->ifname, n1, kr);
    else
    {
        IOObjectGetClass(parent, n2);
        LogSPS("ActivateLocalProxy: Interface %s service %s parent %s", intf->ifname, n1, n2);
        const CFTypeRef ref = IORegistryEntryCreateCFProperty(parent, CFSTR(mDNS_IOREG_KEY), kCFAllocatorDefault, mDNSNULL);
        if (!ref) LogSPS("ActivateLocalProxy: No mDNS_IOREG_KEY for interface %s/%s/%s", intf->ifname, n1, n2);
        else
        {
            if (CFGetTypeID(ref) != CFStringGetTypeID() || !CFEqual(ref, CFSTR(mDNS_IOREG_VALUE)))
                LogMsg("ActivateLocalProxy: mDNS_IOREG_KEY for interface %s/%s/%s value %s != %s",
                       intf->ifname, n1, n2, CFStringGetCStringPtr(ref, mDNSNULL), mDNS_IOREG_VALUE);
            else if (!UseInternalSleepProxy)
                LogSPS("ActivateLocalProxy: Not using internal (NIC) sleep proxy for interface %s", intf->ifname);
            else
            {
                io_connect_t conObj;
                kr = IOServiceOpen(parent, mach_task_self(), mDNS_USER_CLIENT_CREATE_TYPE, &conObj);
                if (kr != KERN_SUCCESS) LogMsg("ActivateLocalProxy: IOServiceOpen for %s/%s/%s failed %d", intf->ifname, n1, n2, kr);
                else
                {
                    mDNSOffloadCmd cmd;
                    mDNSPlatformMemZero(&cmd, sizeof(cmd)); // When compiling 32-bit, make sure top 32 bits of 64-bit pointers get initialized to zero
                    cmd.command       = cmd_mDNSOffloadRR;
                    cmd.numUDPPorts   = GetPortArray(m, mDNSTransport_UDP, mDNSNULL);
                    cmd.numTCPPorts   = GetPortArray(m, mDNSTransport_TCP, mDNSNULL);
                    cmd.numRRRecords  = CountProxyRecords(m, &cmd.rrBufferSize, intf, TCPKAOnly, supportsTCPKA);
                    cmd.compression   = sizeof(DNSMessageHeader);

                    DNSMessage *msg   = (DNSMessage *)mallocL("mDNSOffloadCmd msg", sizeof(DNSMessageHeader) + cmd.rrBufferSize);
                    cmd.rrRecords.ptr = cmd.numRRRecords ? mallocL("mDNSOffloadCmd rrRecords", cmd.numRRRecords * sizeof(FatPtr))     : NULL;
                    cmd.udpPorts.ptr  = cmd.numUDPPorts  ? mallocL("mDNSOffloadCmd udpPorts" , cmd.numUDPPorts  * sizeof(mDNSIPPort)) : NULL;
                    cmd.tcpPorts.ptr  = cmd.numTCPPorts  ? mallocL("mDNSOffloadCmd tcpPorts" , cmd.numTCPPorts  * sizeof(mDNSIPPort)) : NULL;

                    LogSPS("ActivateLocalProxy: msg %p %d RR %p %d, UDP %p %d, TCP %p %d",
                           msg, cmd.rrBufferSize,
                           cmd.rrRecords.ptr, cmd.numRRRecords,
                           cmd.udpPorts.ptr, cmd.numUDPPorts,
                           cmd.tcpPorts.ptr, cmd.numTCPPorts);

                    if (msg && cmd.rrRecords.ptr) GetProxyRecords(m, msg, &cmd.rrBufferSize, cmd.rrRecords.ptr, TCPKAOnly, supportsTCPKA);
                    if (cmd.udpPorts.ptr) cmd.numUDPPorts = GetPortArray(m, mDNSTransport_UDP, cmd.udpPorts.ptr);
                    if (cmd.tcpPorts.ptr) cmd.numTCPPorts = GetPortArray(m, mDNSTransport_TCP, cmd.tcpPorts.ptr);

                    char outputData[2];
                    size_t outputDataSize = sizeof(outputData);
                    kr = IOConnectCallStructMethod(conObj, 0, &cmd, sizeof(cmd), outputData, &outputDataSize);
                    LogSPS("ActivateLocalProxy: IOConnectCallStructMethod for %s/%s/%s %d", intf->ifname, n1, n2, kr);
                    if (kr == KERN_SUCCESS) result = mStatus_NoError;

                    if (cmd.tcpPorts.ptr) freeL("mDNSOffloadCmd udpPorts",  cmd.tcpPorts.ptr);
                    if (cmd.udpPorts.ptr) freeL("mDNSOffloadCmd tcpPorts",  cmd.udpPorts.ptr);
                    if (cmd.rrRecords.ptr) freeL("mDNSOffloadCmd rrRecords", cmd.rrRecords.ptr);
                    if (msg) freeL("mDNSOffloadCmd msg",       msg);
                    IOServiceClose(conObj);
                }
            }
            CFRelease(ref);
        }
        IOObjectRelease(parent);
    }
    IOObjectRelease(service);
    *keepaliveOnly = TCPKAOnly;
    return result;
}

#endif // APPLE_OSX_mDNSResponder

mDNSlocal mDNSu8 SystemWakeForNetworkAccess(void)
{
    mDNSs32 val = 0;
    mDNSu8  ret = (mDNSu8)mDNS_NoWake;

#if TARGET_OS_IOS
    LogSPS("SystemWakeForNetworkAccess: Sleep Proxy Client disabled by command-line option");
    return ret;
#endif

    if (DisableSleepProxyClient)
    {
       LogSPS("SystemWakeForNetworkAccess: Sleep Proxy Client disabled by command-line option");
       return ret;
    }

    GetCurrentPMSetting(CFSTR("Wake On LAN"), &val);

    ret = (mDNSu8)(val != 0) ? mDNS_WakeOnAC : mDNS_NoWake;

#if APPLE_OSX_mDNSResponder && !TARGET_OS_EMBEDDED
    // If we have TCP Keepalive support, system is capable of registering for TCP Keepalives.
    // Further policy decisions on whether to offload the records is handled during sleep processing.
    if ((ret == mDNS_NoWake) && SupportsTCPKeepAlive())
        ret = (mDNSu8)mDNS_WakeOnBattery;
#endif // APPLE_OSX_mDNSResponder

    LogSPS("SystemWakeForNetworkAccess: Wake On LAN: %d", ret);
    return ret;
}

mDNSlocal mDNSBool SystemSleepOnlyIfWakeOnLAN(void)
{
    mDNSs32 val = 0;
    // PrioritizeNetworkReachabilityOverSleep has been deprecated.
    // GetCurrentPMSetting(CFSTR("PrioritizeNetworkReachabilityOverSleep"), &val);
    // Statically set the PrioritizeNetworkReachabilityOverSleep value to 1 for AppleTV
    if (IsAppleTV())
        val = 1;
    return val != 0 ? mDNStrue : mDNSfalse;
}


#if APPLE_OSX_mDNSResponder
// When sleeping, we always ensure that the _autotunnel6 record (if connected to RR relay)
// gets deregistered, so that older peers are forced to connect over direct UDP instead of
// the RR relay.
//
// When sleeping w/o a successful AutoTunnel NAT Mapping, we ensure that all our BTMM
// service records are deregistered, so they do not appear in peers' Finder sidebars.
// We do this by checking for the (non-autotunnel) SRV records, as the PTR and TXT records
// depend on their associated SRV record and therefore will be deregistered together in a
// single update with the SRV record.
//
// Also, the per-zone _kerberos TXT record is always there, including while sleeping, so
// its presence shouldn't delay sleep.
//
// Note that the order of record deregistration is: first _autotunnel6 (if connected to RR
// relay) and host records get deregistered, then SRV (UpdateAllSrvRecords), PTR and TXT.
//
// Also note that returning false here will not delay sleep past the maximum of 10 seconds.
mDNSexport mDNSBool RecordReadyForSleep(mDNS *const m, AuthRecord *rr)
{
    if (!AuthRecord_uDNS(rr)) return mDNStrue;
    
    if ((rr->resrec.rrtype == kDNSType_AAAA) && SameDomainLabel(rr->namestorage.c, (const mDNSu8 *)"\x0c_autotunnel6"))
    {
        LogInfo("RecordReadyForSleep: %s not ready for sleep", ARDisplayString(m, rr));
        return mDNSfalse;
    }
    
    if ((mDNSIPPortIsZero(m->AutoTunnelNAT.ExternalPort) || m->AutoTunnelNAT.Result))
    {
        if (rr->resrec.rrtype == kDNSType_SRV && rr->state != regState_NoTarget && rr->zone
            && !SameDomainLabel(rr->namestorage.c, (const mDNSu8 *)"\x0b_autotunnel"))
        {
            DomainAuthInfo *info = GetAuthInfoForName_internal(m, rr->zone);
            if (info && info->AutoTunnel)
            {
                LogInfo("RecordReadyForSleep: %s not ready for sleep", ARDisplayString(m, rr));
                return mDNSfalse;
            }
        }
    }
    
    return mDNStrue;
}

// Caller must hold the lock
mDNSexport void RemoveAutoTunnel6Record(mDNS *const m)
{
    DomainAuthInfo *info;
    // Set the address to zero before calling UpdateAutoTunnel6Record, so that it will
    // deregister the record, and the MemFree callback won't re-register.
    m->AutoTunnelRelayAddr = zerov6Addr;
    for (info = m->AuthInfoList; info; info = info->next)
        if (info->AutoTunnel)
            UpdateAutoTunnel6Record(m, info);
}

#if !TARGET_OS_EMBEDDED
mDNSlocal mDNSBool IPv6AddressIsOnInterface(mDNSv6Addr ipv6Addr, char *ifname)
{
    struct ifaddrs  *ifa;
    struct ifaddrs  *ifaddrs;
    mDNSAddr addr;

    if (if_nametoindex(ifname) == 0) {LogInfo("IPv6AddressIsOnInterface: Invalid name %s", ifname); return mDNSfalse;}

    if (getifaddrs(&ifaddrs) < 0) {LogInfo("IPv6AddressIsOnInterface: getifaddrs failed"); return mDNSfalse;}

    for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (strncmp(ifa->ifa_name, ifname, IFNAMSIZ) != 0)
            continue;
        if ((ifa->ifa_flags & IFF_UP) == 0 || !ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET6)
            continue;
        if (SetupAddr(&addr, ifa->ifa_addr) != mStatus_NoError)
        {
            LogInfo("IPv6AddressIsOnInterface: SetupAddr error, continuing to the next address");
            continue;
        }
        if (mDNSSameIPv6Address(ipv6Addr, *(mDNSv6Addr*)&addr.ip.v6))
        {
            LogInfo("IPv6AddressIsOnInterface: found %.16a", &ipv6Addr);
            break;
        }
    }
    freeifaddrs(ifaddrs);
    return ifa != NULL;
}

mDNSlocal mDNSv6Addr IPv6AddressFromString(char* buf)
{
    mDNSv6Addr retVal;
    struct addrinfo hints;
    struct addrinfo *res0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_flags = AI_NUMERICHOST;

    int err = getaddrinfo(buf, NULL, &hints, &res0);
    if (err)
        return zerov6Addr;

    retVal = *(mDNSv6Addr*)&((struct sockaddr_in6*)res0->ai_addr)->sin6_addr;

    freeaddrinfo(res0);

    return retVal;
}

mDNSlocal CFDictionaryRef CopyConnectivityBackToMyMacDict()
{
    CFDictionaryRef connd = NULL;
    CFDictionaryRef BTMMDict = NULL;

    connd = SCDynamicStoreCopyValue(NULL, NetworkChangedKey_BTMMConnectivity);
    if (!connd)
    {
        LogInfo("CopyConnectivityBackToMyMacDict: SCDynamicStoreCopyValue failed: %s", SCErrorString(SCError()));
        goto end;
    }

    BTMMDict = CFDictionaryGetValue(connd, CFSTR("BackToMyMac"));
    if (!BTMMDict)
    {
        LogInfo("CopyConnectivityBackToMyMacDict: CFDictionaryGetValue: No value for BackToMyMac");
        goto end;
    }

    // Non-dictionary is treated as non-existent dictionary
    if (CFGetTypeID(BTMMDict) != CFDictionaryGetTypeID())
    {
        BTMMDict = NULL;
        LogMsg("CopyConnectivityBackToMyMacDict: BackToMyMac not a dictionary");
        goto end;
    }

    CFRetain(BTMMDict);

end:
    if (connd) CFRelease(connd);

    return BTMMDict;
}

#define MAX_IPV6_TEXTUAL 40

mDNSlocal mDNSv6Addr ParseBackToMyMacAddr(CFDictionaryRef BTMMDict, CFStringRef ifKey, CFStringRef addrKey)
{
    mDNSv6Addr retVal = zerov6Addr;
    CFTypeRef string = NULL;
    char ifname[IFNAMSIZ];
    char address[MAX_IPV6_TEXTUAL];

    if (!BTMMDict)
        return zerov6Addr;

    if (!CFDictionaryGetValueIfPresent(BTMMDict, ifKey, &string))
    {
        LogInfo("ParseBackToMyMacAddr: interface key does not exist");
        return zerov6Addr;
    }

    if (!CFStringGetCString(string, ifname, IFNAMSIZ, kCFStringEncodingUTF8))
    {
        LogMsg("ParseBackToMyMacAddr: Could not convert interface to CString");
        return zerov6Addr;
    }

    if (!CFDictionaryGetValueIfPresent(BTMMDict, addrKey, &string))
    {
        LogMsg("ParseBackToMyMacAddr: address key does not exist, but interface key does");
        return zerov6Addr;
    }

    if (!CFStringGetCString(string, address, sizeof(address), kCFStringEncodingUTF8))
    {
        LogMsg("ParseBackToMyMacAddr: Could not convert address to CString");
        return zerov6Addr;
    }

    retVal = IPv6AddressFromString(address);
    LogInfo("ParseBackToMyMacAddr: %s (%s) %.16a", ifname, address, &retVal);

    if (mDNSIPv6AddressIsZero(retVal))
        return zerov6Addr;

    if (!IPv6AddressIsOnInterface(retVal, ifname))
    {
        LogMsg("ParseBackToMyMacAddr: %.16a is not on %s", &retVal, ifname);
        return zerov6Addr;
    }

    return retVal;
}

mDNSlocal CFDictionaryRef GetBackToMyMacZones(CFDictionaryRef BTMMDict)
{
    CFTypeRef zones = NULL;

    if (!BTMMDict)
        return NULL;

    if (!CFDictionaryGetValueIfPresent(BTMMDict, CFSTR("Zones"), &zones))
    {
        LogInfo("CopyBTMMZones: Zones key does not exist");
        return NULL;
    }

    return zones;
}

mDNSlocal mDNSv6Addr ParseBackToMyMacZone(CFDictionaryRef zones, DomainAuthInfo* info)
{
    mDNSv6Addr addr = zerov6Addr;
    char buffer[MAX_ESCAPED_DOMAIN_NAME];
    CFStringRef domain = NULL;
    CFTypeRef theZone = NULL;

    if (!zones)
        return addr;

    ConvertDomainNameToCString(&info->domain, buffer);
    domain = CFStringCreateWithCString(NULL, buffer, kCFStringEncodingUTF8);
    if (!domain)
        return addr;

    if (CFDictionaryGetValueIfPresent(zones, domain, &theZone))
        addr = ParseBackToMyMacAddr(theZone, CFSTR("Interface"), CFSTR("Address"));

    CFRelease(domain);

    return addr;
}

mDNSlocal void SetupBackToMyMacInnerAddresses(mDNS *const m, CFDictionaryRef BTMMDict)
{
    DomainAuthInfo* info;
    CFDictionaryRef zones = GetBackToMyMacZones(BTMMDict);
    mDNSv6Addr newAddr;

    for (info = m->AuthInfoList; info; info = info->next)
    {
        if (!info->AutoTunnel)
            continue;

        newAddr = ParseBackToMyMacZone(zones, info);

        if (mDNSSameIPv6Address(newAddr, info->AutoTunnelInnerAddress))
            continue;

        info->AutoTunnelInnerAddress = newAddr;
        DeregisterAutoTunnelHostRecord(m, info);
        UpdateAutoTunnelHostRecord(m, info);
        UpdateAutoTunnelDomainStatus(m, info);
    }
}

// MUST be called holding the lock
mDNSlocal void ProcessConndConfigChanges(mDNS *const m)
{
    CFDictionaryRef dict = CopyConnectivityBackToMyMacDict();
    if (!dict)
        LogInfo("ProcessConndConfigChanges: No BTMM dictionary");
    mDNSv6Addr relayAddr = ParseBackToMyMacAddr(dict, CFSTR("RelayInterface"), CFSTR("RelayAddress"));

    LogInfo("ProcessConndConfigChanges: relay %.16a", &relayAddr);

    SetupBackToMyMacInnerAddresses(m, dict);

    if (dict) CFRelease(dict);

    if (!mDNSSameIPv6Address(relayAddr, m->AutoTunnelRelayAddr))
    {
        m->AutoTunnelRelayAddr = relayAddr;

        DomainAuthInfo* info;
        for (info = m->AuthInfoList; info; info = info->next)
            if (info->AutoTunnel)
            {
                DeregisterAutoTunnel6Record(m, info);
                UpdateAutoTunnel6Record(m, info);
                UpdateAutoTunnelDomainStatus(m, info);
            }

        // Determine whether we need racoon to accept incoming connections
        UpdateAnonymousRacoonConfig(m);
    }

    // If awacsd crashes or exits for some reason, restart it
    UpdateBTMMRelayConnection(m);
}
#endif // !TARGET_OS_EMBEDDED
#endif /* APPLE_OSX_mDNSResponder */

mDNSlocal mDNSBool IsAppleNetwork(mDNS *const m)
{
    DNSServer *s;
    // Determine if we're on AppleNW based on DNSServer having 17.x.y.z IPv4 addr
    for (s = m->DNSServers; s; s = s->next)
    {
        if (s->addr.ip.v4.b[0] == 17)
        {     
            LogInfo("IsAppleNetwork: Found 17.x.y.z DNSServer concluding that we are on AppleNW: %##s %#a", s->domain.c, &s->addr);
            return mDNStrue;
        }     
    }
    return mDNSfalse;
}

// Called with KQueueLock & mDNS lock
// SetNetworkChanged is allowed to shorten (but not extend) the pause while we wait for configuration changes to settle
mDNSlocal void SetNetworkChanged(mDNS *const m, mDNSs32 delay)
{
    mDNS_CheckLock(m);
    if (!m->NetworkChanged || m->NetworkChanged - NonZeroTime(m->timenow + delay) > 0)
    {
        m->NetworkChanged = NonZeroTime(m->timenow + delay);
        LogInfo("SetNetworkChanged: Scheduling in %d ticks", delay);
    }
    else
        LogInfo("SetNetworkChanged: *NOT* increasing delay from %d to %d", m->NetworkChanged - m->timenow, delay);
}

// Called with KQueueLock & mDNS lock
mDNSlocal void SetKeyChainTimer(mDNS *const m, mDNSs32 delay)
{
    // If it's not set or it needs to happen sooner than when it's currently set
    if (!m->p->KeyChainTimer || m->p->KeyChainTimer - NonZeroTime(m->timenow + delay) > 0)
    {
        m->p->KeyChainTimer = NonZeroTime(m->timenow + delay);
        LogInfo("SetKeyChainTimer: %d", delay);
    }
}

mDNSexport void mDNSMacOSXNetworkChanged(mDNS *const m)
{
    LogInfo("***   Network Configuration Change   ***  %d ticks late%s",
            m->NetworkChanged ? mDNS_TimeNow(m) - m->NetworkChanged : 0,
            m->NetworkChanged ? "" : " (no scheduled configuration change)");
    m->NetworkChanged = 0;       // If we received a network change event and deferred processing, we're now dealing with it

    // If we have *any* TENTATIVE IPv6 addresses, wait until they've finished configuring
    int InfoSocket = socket(AF_INET6, SOCK_DGRAM, 0);
    if (InfoSocket > 0)
    {
        mDNSBool tentative = mDNSfalse;
        struct ifaddrs *ifa = myGetIfAddrs(1);
        while (ifa)
        {
            if (ifa->ifa_addr->sa_family == AF_INET6)
            {
                struct in6_ifreq ifr6;
                mDNSPlatformMemZero((char *)&ifr6, sizeof(ifr6));
                strlcpy(ifr6.ifr_name, ifa->ifa_name, sizeof(ifr6.ifr_name));
                ifr6.ifr_addr = *(struct sockaddr_in6 *)ifa->ifa_addr;
                // We need to check for IN6_IFF_TENTATIVE here, not IN6_IFF_NOTREADY, because
                // IN6_IFF_NOTREADY includes both IN6_IFF_TENTATIVE and IN6_IFF_DUPLICATED addresses.
                // We can expect that an IN6_IFF_TENTATIVE address will shortly become ready,
                // but an IN6_IFF_DUPLICATED address may not.
                if (ioctl(InfoSocket, SIOCGIFAFLAG_IN6, &ifr6) != -1)
                {
                    if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_TENTATIVE)
                    {
                        LogInfo("***   Network Configuration Change   ***  IPv6 address %.16a TENTATIVE, will retry", &ifr6.ifr_addr.sin6_addr);
                        tentative = mDNStrue;
                        // no need to check other interfaces if we already found out that one interface is TENTATIVE
                        break;
                    }
                }
            }
            ifa = ifa->ifa_next;
        }
        close(InfoSocket);
        if (tentative)
        {
            mDNS_Lock(m);
            SetNetworkChanged(m, mDNSPlatformOneSecond / 2);
            mDNS_Unlock(m);
            return;
        }
        LogInfo("***   Network Configuration Change   ***  No IPv6 address TENTATIVE, will continue");
    }

    mDNSs32 utc = mDNSPlatformUTC();
    m->SystemWakeOnLANEnabled = SystemWakeForNetworkAccess();
    m->SystemSleepOnlyIfWakeOnLAN = SystemSleepOnlyIfWakeOnLAN();
    MarkAllInterfacesInactive(m, utc);
    UpdateInterfaceList(m, utc);
    ClearInactiveInterfaces(m, utc);
    SetupActiveInterfaces(m, utc);

#if APPLE_OSX_mDNSResponder
#if !TARGET_OS_EMBEDDED
    mDNS_Lock(m);
    ProcessConndConfigChanges(m);
    mDNS_Unlock(m);

    // Scan to find client tunnels whose questions have completed,
    // but whose local inner/outer addresses have changed since the tunnel was set up
    ClientTunnel *p;
    for (p = m->TunnelClients; p; p = p->next)
        if (p->q.ThisQInterval < 0)
        {
            DomainAuthInfo* info = GetAuthInfoForName(m, &p->dstname);
            if (!info)
            {
                LogMsg("mDNSMacOSXNetworkChanged: Could not get AuthInfo for %##s, removing tunnel keys", p->dstname.c);
                AutoTunnelSetKeys(p, mDNSfalse);
            }
            else
            {
                mDNSv6Addr inner = info->AutoTunnelInnerAddress;

                if (!mDNSIPPortIsZero(p->rmt_outer_port))
                {
                    mDNSAddr tmpSrc = zeroAddr;
                    mDNSAddr tmpDst = { mDNSAddrType_IPv4, {{{0}}} };
                    tmpDst.ip.v4 = p->rmt_outer;
                    mDNSPlatformSourceAddrForDest(&tmpSrc, &tmpDst);
                    if (!mDNSSameIPv6Address(p->loc_inner, inner) ||
                        !mDNSSameIPv4Address(p->loc_outer, tmpSrc.ip.v4))
                    {
                        AutoTunnelSetKeys(p, mDNSfalse);
                        p->loc_inner = inner;
                        p->loc_outer = tmpSrc.ip.v4;
                        AutoTunnelSetKeys(p, mDNStrue);
                    }
                }
                else
                {
                    if (!mDNSSameIPv6Address(p->loc_inner, inner) ||
                        !mDNSSameIPv6Address(p->loc_outer6, m->AutoTunnelRelayAddr))
                    {
                        AutoTunnelSetKeys(p, mDNSfalse);
                        p->loc_inner = inner;
                        p->loc_outer6 = m->AutoTunnelRelayAddr;
                        AutoTunnelSetKeys(p, mDNStrue);
                    }
                }
            }
        }
#endif //!TARGET_OS_EMBEDDED

    SetSPS(m);

    NetworkInterfaceInfoOSX *i;
    for (i = m->p->InterfaceList; i; i = i->next)
    {
        if (!m->SPSSocket) // Not being Sleep Proxy Server; close any open BPF fds
        {
            if (i->BPF_fd >= 0 && CountProxyTargets(m, i, mDNSNULL, mDNSNULL) == 0)
                CloseBPF(i);
        }
        else // else, we're Sleep Proxy Server; open BPF fds
        {
            if (i->Exists && (i->Registered == i) && SPSInterface(i) && i->BPF_fd == -1)
            {
                LogMsg("%s mDNSMacOSXNetworkChanged: requesting BPF", i->ifinfo.ifname);
                i->BPF_fd = -2;
                mDNSRequestBPF();
            }
        }
    }

#endif // APPLE_OSX_mDNSResponder

    uDNS_SetupDNSConfig(m);
    mDNS_ConfigChanged(m);

    if (IsAppleNetwork(m) != mDNS_McastTracingEnabled)
    {
        mDNS_McastTracingEnabled = mDNS_McastTracingEnabled ? mDNSfalse : mDNStrue; 
        LogInfo("mDNSMacOSXNetworkChanged: Multicast Tracing %s", mDNS_McastTracingEnabled ? "Enabled" : "Disabled");
        UpdateDebugState();
    }

}

// Copy the fourth slash-delimited element from either:
//   State:/Network/Interface/<bsdname>/IPv4
// or
//   Setup:/Network/Service/<servicename>/Interface
mDNSlocal CFStringRef CopyNameFromKey(CFStringRef key)
{
    CFArrayRef a;
    CFStringRef name = NULL;

    a = CFStringCreateArrayBySeparatingStrings(NULL, key, CFSTR("/"));
    if (a && CFArrayGetCount(a) == 5) name = CFRetain(CFArrayGetValueAtIndex(a, 3));
    if (a != NULL) CFRelease(a);

    return name;
}

// Whether a key from a network change notification corresponds to
// an IP service that is explicitly configured for IPv4 Link Local
mDNSlocal int ChangedKeysHaveIPv4LL(CFArrayRef inkeys)
{
    CFDictionaryRef dict = NULL;
    CFMutableArrayRef a;
    const void **keys = NULL, **vals = NULL;
    CFStringRef pattern = NULL;
    int i, ic, j, jc;
    int found = 0;

    jc = CFArrayGetCount(inkeys);
    if (!jc) goto done;

    a = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (a == NULL) goto done;

    // Setup:/Network/Service/[^/]+/Interface
    pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainSetup, kSCCompAnyRegex, kSCEntNetInterface);
    if (pattern == NULL) goto done;
    CFArrayAppendValue(a, pattern);
    CFRelease(pattern);

    // Setup:/Network/Service/[^/]+/IPv4
    pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainSetup, kSCCompAnyRegex, kSCEntNetIPv4);
    if (pattern == NULL) goto done;
    CFArrayAppendValue(a, pattern);
    CFRelease(pattern);

    dict = SCDynamicStoreCopyMultiple(NULL, NULL, a);
    CFRelease(a);

    if (!dict)
    {
        LogMsg("ChangedKeysHaveIPv4LL: Empty dictionary");
        goto done;
    }

    ic = CFDictionaryGetCount(dict);
    vals = mDNSPlatformMemAllocate(sizeof (void *) * ic);
    keys = mDNSPlatformMemAllocate(sizeof (void *) * ic);
    CFDictionaryGetKeysAndValues(dict, keys, vals);

    // For each key we were given...
    for (j = 0; j < jc; j++)
    {
        CFStringRef key = CFArrayGetValueAtIndex(inkeys, j);
        CFStringRef ifname = NULL;

        char buf[256];

        // It would be nice to use a regex here
        if (!CFStringHasPrefix(key, CFSTR("State:/Network/Interface/")) || !CFStringHasSuffix(key, kSCEntNetIPv4)) continue;

        if ((ifname = CopyNameFromKey(key)) == NULL) continue;
        if (mDNS_LoggingEnabled)
        {
            if (!CFStringGetCString(ifname, buf, sizeof(buf), kCFStringEncodingUTF8)) buf[0] = 0;
            LogInfo("ChangedKeysHaveIPv4LL: potential ifname %s", buf);
        }

        // Loop over the interfaces to find matching the ifname, and see if that one has kSCValNetIPv4ConfigMethodLinkLocal
        for (i = 0; i < ic; i++)
        {
            CFDictionaryRef ipv4dict;
            CFStringRef name;
            CFStringRef serviceid;
            CFStringRef configmethod;

            if (!CFStringHasSuffix(keys[i], kSCEntNetInterface)) continue;

            if (CFDictionaryGetTypeID() != CFGetTypeID(vals[i])) continue;

            if ((name = CFDictionaryGetValue(vals[i], kSCPropNetInterfaceDeviceName)) == NULL) continue;

            if (!CFEqual(ifname, name)) continue;

            if ((serviceid = CopyNameFromKey(keys[i])) == NULL) continue;
            if (mDNS_LoggingEnabled)
            {
                if (!CFStringGetCString(serviceid, buf, sizeof(buf), kCFStringEncodingUTF8)) buf[0] = 0;
                LogInfo("ChangedKeysHaveIPv4LL: found serviceid %s", buf);
            }

            pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainSetup, serviceid, kSCEntNetIPv4);
            CFRelease(serviceid);
            if (pattern == NULL) continue;

            ipv4dict = CFDictionaryGetValue(dict, pattern);
            CFRelease(pattern);
            if (!ipv4dict || CFDictionaryGetTypeID() != CFGetTypeID(ipv4dict)) continue;

            configmethod = CFDictionaryGetValue(ipv4dict, kSCPropNetIPv4ConfigMethod);
            if (!configmethod) continue;

            if (mDNS_LoggingEnabled)
            {
                if (!CFStringGetCString(configmethod, buf, sizeof(buf), kCFStringEncodingUTF8)) buf[0] = 0;
                LogInfo("ChangedKeysHaveIPv4LL: configmethod %s", buf);
            }

            if (CFEqual(configmethod, kSCValNetIPv4ConfigMethodLinkLocal)) { found++; break; }
        }

        CFRelease(ifname);
    }

done:
    if (vals != NULL) mDNSPlatformMemFree(vals);
    if (keys != NULL) mDNSPlatformMemFree(keys);
    if (dict != NULL) CFRelease(dict);

    return found;
}

mDNSlocal void NetworkChanged(SCDynamicStoreRef store, CFArrayRef changedKeys, void *context)
{
    (void)store;        // Parameter not used
    mDNS *const m = (mDNS *const)context;
    KQueueLock(m);
    mDNS_Lock(m);

    //mDNSs32 delay = mDNSPlatformOneSecond * 2;                // Start off assuming a two-second delay
    const mDNSs32 delay = (mDNSPlatformOneSecond + 39) / 40;	// 25 ms delay

    int c = CFArrayGetCount(changedKeys);                   // Count changes
    CFRange range = { 0, c };
    int c_host = (CFArrayContainsValue(changedKeys, range, NetworkChangedKey_Hostnames   ) != 0);
    int c_comp = (CFArrayContainsValue(changedKeys, range, NetworkChangedKey_Computername) != 0);
    int c_udns = (CFArrayContainsValue(changedKeys, range, NetworkChangedKey_DNS         ) != 0);
    int c_ddns = (CFArrayContainsValue(changedKeys, range, NetworkChangedKey_DynamicDNS  ) != 0);
    int c_btmm = (CFArrayContainsValue(changedKeys, range, NetworkChangedKey_BackToMyMac ) != 0);
    int c_v4ll = ChangedKeysHaveIPv4LL(changedKeys);
    int c_fast = 0;
    
    // Do immediate network changed processing for "p2p*" interfaces and
    // for interfaces with the IFEF_DIRECTLINK flag set or association with a CarPlay
    // hosted SSID.
    {
        CFArrayRef  labels;
        CFIndex     n;
        for (int i = 0; i < c; i++)
        {
            CFStringRef key = CFArrayGetValueAtIndex(changedKeys, i);

            // Only look at keys with prefix "State:/Network/Interface/"
            if (!CFStringHasPrefix(key, NetworkChangedKey_StateInterfacePrefix))
                continue;

            // And suffix "IPv6" or "IPv4".
            if (!CFStringHasSuffix(key, kSCEntNetIPv6) && !CFStringHasSuffix(key, kSCEntNetIPv4))
                continue;

            labels = CFStringCreateArrayBySeparatingStrings(NULL, key, CFSTR("/"));
            if (labels == NULL)
                break;
            n = CFArrayGetCount(labels);

            // Interface changes will have keys of the form: 
            //     State:/Network/Interface/<interfaceName>/IPv6
            // Thus five '/' seperated fields, the 4th one being the <interfaceName> string.
            if (n == 5)
            {
                char buf[256];

                // The 4th label (index = 3) should be the interface name.
                if (CFStringGetCString(CFArrayGetValueAtIndex(labels, 3), buf, sizeof(buf), kCFStringEncodingUTF8)
                    && (strstr(buf, "p2p") || (getExtendedFlags(buf) & IFEF_DIRECTLINK) || IsCarPlaySSID(buf)))
                {
                    LogInfo("NetworkChanged: interface %s qualifies for reduced change handling delay", buf);
                    c_fast++;
                    CFRelease(labels);
                    break;
                }
            }
            CFRelease(labels);
        }
    }

    //if (c && c - c_host - c_comp - c_udns - c_ddns - c_btmm - c_v4ll - c_fast == 0)
    //    delay = mDNSPlatformOneSecond/10;  // If these were the only changes, shorten delay

    if (mDNS_LoggingEnabled)
    {
        int i;
        for (i=0; i<c; i++)
        {
            char buf[256];
            if (!CFStringGetCString(CFArrayGetValueAtIndex(changedKeys, i), buf, sizeof(buf), kCFStringEncodingUTF8)) buf[0] = 0;
            LogInfo("***   Network Configuration Change   *** SC key: %s", buf);
        }
        LogInfo("***   Network Configuration Change   *** %d change%s %s%s%s%s%s%s%sdelay %d%s",
                c, c>1 ? "s" : "",
                c_host ? "(Local Hostname) " : "",
                c_comp ? "(Computer Name) "  : "",
                c_udns ? "(DNS) "            : "",
                c_ddns ? "(DynamicDNS) "     : "",
                c_btmm ? "(BTMM) "           : "",
                c_v4ll ? "(kSCValNetIPv4ConfigMethodLinkLocal) " : "",
                c_fast ? "(P2P/IFEF_DIRECTLINK/IsCarPlaySSID) "  : "",
                delay,
                (c_ddns || c_btmm) ? " + SetKeyChainTimer" : "");
    }

    SetNetworkChanged(m, delay);

    // Other software might pick up these changes to register or browse in WAB or BTMM domains,
    // so in order for secure updates to be made to the server, make sure to read the keychain and
    // setup the DomainAuthInfo before handing the network change.
    // If we don't, then we will first try to register services in the clear, then later setup the
    // DomainAuthInfo, which is incorrect.
    if (c_ddns || c_btmm)
        SetKeyChainTimer(m, delay);

    // Don't try to call mDNSMacOSXNetworkChanged() here -- we're running on the wrong thread

    mDNS_Unlock(m);
    KQueueUnlock(m, "NetworkChanged");
}

#if APPLE_OSX_mDNSResponder
mDNSlocal void RefreshSPSStatus(const void *key, const void *value, void *context)
{
    (void)context;
    char buf[IFNAMSIZ];

    CFStringRef ifnameStr = (CFStringRef)key;
    CFArrayRef array = (CFArrayRef)value;
    if (!CFStringGetCString(ifnameStr, buf, sizeof(buf), kCFStringEncodingUTF8)) 
        buf[0] = 0;

    LogInfo("RefreshSPSStatus: Updating SPS state for key %s, array count %d", buf, CFArrayGetCount(array));
    mDNSDynamicStoreSetConfig(kmDNSSleepProxyServersState, buf, value);
}
#endif

mDNSlocal void DynamicStoreReconnected(SCDynamicStoreRef store, void *info)
{
    mDNS *const m = (mDNS *const)info;
    (void)store;

    KQueueLock(m);   // serialize with KQueueLoop()

    LogInfo("DynamicStoreReconnected: Reconnected");

    // State:/Network/MulticastDNS
    SetLocalDomains();

    // State:/Network/DynamicDNS
    if (m->FQDN.c[0])
        mDNSPlatformDynDNSHostNameStatusChanged(&m->FQDN, 1);

    // Note: PrivateDNS and BackToMyMac are automatically populated when configd is restarted
    // as we receive network change notifications and thus not necessary. But we leave it here
    // so that if things are done differently in the future, this code still works.

    // State:/Network/PrivateDNS
    if (privateDnsArray)
        mDNSDynamicStoreSetConfig(kmDNSPrivateConfig, mDNSNULL, privateDnsArray);

#if APPLE_OSX_mDNSResponder
    // State:/Network/BackToMyMac
    UpdateAutoTunnelDomainStatuses(m);

    // State:/Network/Interface/en0/SleepProxyServers
    if (spsStatusDict) 
        CFDictionaryApplyFunction(spsStatusDict, RefreshSPSStatus, NULL);
#endif
    KQueueUnlock(m, "DynamicStoreReconnected");
}

mDNSlocal mStatus WatchForNetworkChanges(mDNS *const m)
{
    mStatus err = -1;
    SCDynamicStoreContext context = { 0, m, NULL, NULL, NULL };
    SCDynamicStoreRef store    = SCDynamicStoreCreate(NULL, CFSTR("mDNSResponder:WatchForNetworkChanges"), NetworkChanged, &context);
    CFMutableArrayRef keys     = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFStringRef pattern1 = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetIPv4);
    CFStringRef pattern2 = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetIPv6);
    CFMutableArrayRef patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    if (!store) { LogMsg("SCDynamicStoreCreate failed: %s", SCErrorString(SCError())); goto error; }
    if (!keys || !pattern1 || !pattern2 || !patterns) goto error;

    CFArrayAppendValue(keys, NetworkChangedKey_IPv4);
    CFArrayAppendValue(keys, NetworkChangedKey_IPv6);
    CFArrayAppendValue(keys, NetworkChangedKey_Hostnames);
    CFArrayAppendValue(keys, NetworkChangedKey_Computername);
    CFArrayAppendValue(keys, NetworkChangedKey_DNS);
    CFArrayAppendValue(keys, NetworkChangedKey_DynamicDNS);
    CFArrayAppendValue(keys, NetworkChangedKey_BackToMyMac);
    CFArrayAppendValue(keys, NetworkChangedKey_PowerSettings);
    CFArrayAppendValue(keys, NetworkChangedKey_BTMMConnectivity);
    CFArrayAppendValue(patterns, pattern1);
    CFArrayAppendValue(patterns, pattern2);
    CFArrayAppendValue(patterns, CFSTR("State:/Network/Interface/[^/]+/AirPort"));
    if (!SCDynamicStoreSetNotificationKeys(store, keys, patterns))
    { LogMsg("SCDynamicStoreSetNotificationKeys failed: %s", SCErrorString(SCError())); goto error; }

#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
    if (!SCDynamicStoreSetDispatchQueue(store, dispatch_get_main_queue()))
    { LogMsg("SCDynamicStoreCreateRunLoopSource failed: %s", SCErrorString(SCError())); goto error; }
#else
    m->p->StoreRLS = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
    if (!m->p->StoreRLS) { LogMsg("SCDynamicStoreCreateRunLoopSource failed: %s", SCErrorString(SCError())); goto error; }
    CFRunLoopAddSource(CFRunLoopGetMain(), m->p->StoreRLS, kCFRunLoopDefaultMode);
#endif
    SCDynamicStoreSetDisconnectCallBack(store, DynamicStoreReconnected);
    m->p->Store = store;
    err = 0;
    goto exit;

error:
    if (store) CFRelease(store);

exit:
    if (patterns) CFRelease(patterns);
    if (pattern2) CFRelease(pattern2);
    if (pattern1) CFRelease(pattern1);
    if (keys) CFRelease(keys);

    return(err);
}

#if !TARGET_OS_EMBEDDED     // don't setup packet filter rules on embedded

mDNSlocal void mDNSSetPacketFilterRules(mDNS *const m, char * ifname, const ResourceRecord *const excludeRecord)
{
    AuthRecord  *rr;
    pfArray_t portArray;
    pfArray_t protocolArray;
    uint32_t count = 0;

    for (rr = m->ResourceRecords; rr; rr=rr->next)
    {
        if ((rr->resrec.rrtype == kDNSServiceType_SRV) 
            && ((rr->ARType == AuthRecordAnyIncludeP2P) || (rr->ARType == AuthRecordAnyIncludeAWDLandP2P)))
        {
            const mDNSu8    *p;

            if (count >= PFPortArraySize)
            {
                LogMsg("mDNSSetPacketFilterRules: %d service limit, skipping %s", PFPortArraySize, ARDisplayString(m, rr));
                continue;
            }

            if (excludeRecord && IdenticalResourceRecord(&rr->resrec, excludeRecord))
            {
                LogInfo("mDNSSetPacketFilterRules: record being removed, skipping %s", ARDisplayString(m, rr));
                continue;
            }

            LogMsg("mDNSSetPacketFilterRules: found %s", ARDisplayString(m, rr));

            portArray[count] = rr->resrec.rdata->u.srv.port.NotAnInteger;

            // Assume <Service Instance>.<App Protocol>.<Transport Protocol>.<Name>
            p = rr->resrec.name->c;

            // Skip to App Protocol
            if (p[0])
                p += 1 + p[0];

            // Skip to Transport Protocol
            if (p[0])
                p += 1 + p[0];

            if      (SameDomainLabel(p, (mDNSu8 *)"\x4" "_tcp"))
            {
                protocolArray[count] = IPPROTO_TCP;
            }
            else if (SameDomainLabel(p, (mDNSu8 *)"\x4" "_udp"))
            {
                protocolArray[count] = IPPROTO_UDP;
            }
            else
            {
                LogMsg("mDNSSetPacketFilterRules: could not determine transport protocol of service");
                LogMsg("mDNSSetPacketFilterRules: %s", ARDisplayString(m, rr));
                return;
            }
            count++;
        }
    }
    mDNSPacketFilterControl(PF_SET_RULES, ifname, count, portArray, protocolArray);
}

// If the p2p interface already exists, update the Bonjour packet filter rules for it.
mDNSexport void mDNSUpdatePacketFilter(const ResourceRecord *const excludeRecord)
{
    mDNS *const m = &mDNSStorage;

    NetworkInterfaceInfo *intf = GetFirstActiveInterface(m->HostInterfaces);
    while (intf)
    {
        if (strncmp(intf->ifname, "p2p", 3) == 0)
        {
            LogInfo("mDNSInitPacketFilter: Setting rules for ifname %s", intf->ifname);
            mDNSSetPacketFilterRules(m, intf->ifname, excludeRecord);
            break;
        }
        intf = GetFirstActiveInterface(intf->next);
    }
}

#else // !TARGET_OS_EMBEDDED

// Currently no packet filter setup required on embedded platforms.
mDNSexport void mDNSUpdatePacketFilter(const ResourceRecord *const excludeRecord)
{
    (void) excludeRecord; // unused
}

#endif // !TARGET_OS_EMBEDDED

// Handle AWDL KEV_DL_MASTER_ELECTED event by restarting queries and advertisements
// marked to include the AWDL interface.
mDNSlocal void newMasterElected(mDNS *const m, struct net_event_data * ptr)
{
    char        ifname[IFNAMSIZ];
    mDNSu32     interfaceIndex;
    DNSQuestion *q;
    AuthRecord  *rr;
    NetworkInterfaceInfoOSX *infoOSX;
    mDNSInterfaceID InterfaceID;

    snprintf(ifname, IFNAMSIZ, "%s%d", ptr->if_name, ptr->if_unit);
    interfaceIndex  = if_nametoindex(ifname);

    if (!interfaceIndex)
    {
        LogMsg("newMasterElected: if_nametoindex(%s) failed", ifname);
        return;
    }

    LogInfo("newMasterElected: ifname = %s, interfaceIndex = %d", ifname, interfaceIndex);
    infoOSX = IfindexToInterfaceInfoOSX(m, (mDNSInterfaceID)(uintptr_t)interfaceIndex);
    if (!infoOSX)
    {
        LogInfo("newMasterElected: interface %s not yet active", ifname);
        return;
    }
    InterfaceID = infoOSX->ifinfo.InterfaceID;

    for (q = m->Questions; q; q=q->next)
    {
        if ((!q->InterfaceID && (q->flags & kDNSServiceFlagsIncludeAWDL))
            || q->InterfaceID == InterfaceID)
        {
            LogInfo("newMasterElected: restarting %s query for %##s", DNSTypeName(q->qtype), q->qname.c);
            mDNSCoreRestartQuestion(m, q);
        }
    }

    for (rr = m->ResourceRecords; rr; rr=rr->next)
    {
        if ((!rr->resrec.InterfaceID 
            && ((rr->ARType == AuthRecordAnyIncludeAWDL) || ((rr->ARType == AuthRecordAnyIncludeAWDLandP2P))))
           || rr->resrec.InterfaceID == InterfaceID)
        {
            LogInfo("newMasterElected: restarting %s announcements for %##s", DNSTypeName(rr->resrec.rrtype), rr->namestorage.c);
            mDNSCoreRestartRegistration(m, rr, -1);
        }
    }
}

// An ssth array of all zeroes indicates the peer has no services registered.
mDNSlocal mDNSBool allZeroSSTH(struct opaque_presence_indication *op)
{
    int i;
    int *intp = (int *) op->ssth;

    // MAX_SSTH_SIZE should always be a multiple of sizeof(int), if
    // it's not, print an error message and return false so that
    // corresponding peer records are not flushed when KEV_DL_NODE_PRESENCE event
    // is received.
    if (MAX_SSTH_SIZE % sizeof(int))
    {
        LogInfo("allZeroSSTH: MAX_SSTH_SIZE = %d not a multiple of sizeof(int)", MAX_SSTH_SIZE);
        return mDNSfalse;
    }

    for (i = 0; i < (int)(MAX_SSTH_SIZE / sizeof(int)); i++, intp++)
    {
        if (*intp)
            return mDNSfalse;
    }
    return mDNStrue;
}

// Mark records from this peer for deletion from the cache.
mDNSlocal void removeCachedPeerRecords(mDNS *const m, mDNSu32 ifindex, mDNSAddr *ap, bool purgeNow)
{
    mDNSu32     slot;
    CacheGroup  *cg;
    CacheRecord *cr;
    NetworkInterfaceInfoOSX *infoOSX;
    mDNSInterfaceID InterfaceID;

    // Using mDNSPlatformInterfaceIDfromInterfaceIndex() would lead to recursive
    // locking issues, see: <rdar://problem/21332983>
    infoOSX = IfindexToInterfaceInfoOSX(m, (mDNSInterfaceID)(uintptr_t)ifindex);
    if (!infoOSX)
    {
        LogInfo("removeCachedPeerRecords: interface %d not yet active", ifindex);
        return;
    }
    InterfaceID = infoOSX->ifinfo.InterfaceID;

    FORALL_CACHERECORDS(slot, cg, cr)
    {
        if ((InterfaceID == cr->resrec.InterfaceID) && mDNSSameAddress(ap, & cr->sourceAddress))
        {
            LogInfo("removeCachedPeerRecords: %s %##s marking for deletion",
                 DNSTypeName(cr->resrec.rrtype), cr->resrec.name->c);

            if (purgeNow)
                mDNS_PurgeCacheResourceRecord(m, cr);
            else
                mDNS_Reconfirm_internal(m, cr, 0);  // use default minimum reconfirm time 
        }
    }
}

// Handle KEV_DL_NODE_PRESENCE event.
mDNSlocal void nodePresence(mDNS *const m, struct kev_dl_node_presence * p)
{
    char buf[INET6_ADDRSTRLEN];
    struct opaque_presence_indication *op = (struct opaque_presence_indication *) p->node_service_info;

    if (inet_ntop(AF_INET6, & p->sin6_node_address.sin6_addr, buf, sizeof(buf)))
        LogInfo("nodePresence:  IPv6 address: %s, SUI %d", buf, op->SUI);
    else
        LogInfo("nodePresence:  inet_ntop() error");
 
    // AWDL will generate a KEV_DL_NODE_PRESENCE event with SSTH field of
    // all zeroes when a node is present and has no services registered.
    if (allZeroSSTH(op))
    {
        mDNSAddr    peerAddr;

        peerAddr.type = mDNSAddrType_IPv6;
        peerAddr.ip.v6 = *(mDNSv6Addr*)&p->sin6_node_address.sin6_addr;

        LogInfo("nodePresence: ssth is all zeroes, reconfirm cached records for this peer");
        removeCachedPeerRecords(m, p->sdl_node_address.sdl_index, & peerAddr, false);
    }
}

// Handle KEV_DL_NODE_ABSENCE event.
mDNSlocal void nodeAbsence(mDNS *const m, struct kev_dl_node_absence * p)
{
    mDNSAddr    peerAddr;
    char buf[INET6_ADDRSTRLEN];

    if (inet_ntop(AF_INET6, & p->sin6_node_address.sin6_addr, buf, sizeof(buf)))
        LogInfo("nodeAbsence:  IPv6 address: %s", buf);
    else
        LogInfo("nodeAbsence:  inet_ntop() error");

    peerAddr.type = mDNSAddrType_IPv6;
    peerAddr.ip.v6 = *(mDNSv6Addr*)&p->sin6_node_address.sin6_addr;

    LogInfo("nodeAbsence: immediately purge cached records from this peer");
    removeCachedPeerRecords(m, p->sdl_node_address.sdl_index, & peerAddr, true);
}

mDNSlocal void SysEventCallBack(int s1, short __unused filter, void *context)
{
    mDNS *const m = (mDNS *const)context;

    mDNS_Lock(m);

    struct { struct kern_event_msg k; char extra[256]; } msg;
    int bytes = recv(s1, &msg, sizeof(msg), 0);
    if (bytes < 0)
        LogMsg("SysEventCallBack: recv error %d errno %d (%s)", bytes, errno, strerror(errno));
    else
    {
        LogInfo("SysEventCallBack got %d bytes size %d %X %s %X %s %X %s id %d code %d %s",
                bytes, msg.k.total_size,
                msg.k.vendor_code, msg.k.vendor_code  == KEV_VENDOR_APPLE  ? "KEV_VENDOR_APPLE"  : "?",
                msg.k.kev_class, msg.k.kev_class    == KEV_NETWORK_CLASS ? "KEV_NETWORK_CLASS" : "?",
                msg.k.kev_subclass, msg.k.kev_subclass == KEV_DL_SUBCLASS   ? "KEV_DL_SUBCLASS"   : "?",
                msg.k.id, msg.k.event_code,
                msg.k.event_code == KEV_DL_SIFFLAGS             ? "KEV_DL_SIFFLAGS"             :
                msg.k.event_code == KEV_DL_SIFMETRICS           ? "KEV_DL_SIFMETRICS"           :
                msg.k.event_code == KEV_DL_SIFMTU               ? "KEV_DL_SIFMTU"               :
                msg.k.event_code == KEV_DL_SIFPHYS              ? "KEV_DL_SIFPHYS"              :
                msg.k.event_code == KEV_DL_SIFMEDIA             ? "KEV_DL_SIFMEDIA"             :
                msg.k.event_code == KEV_DL_SIFGENERIC           ? "KEV_DL_SIFGENERIC"           :
                msg.k.event_code == KEV_DL_ADDMULTI             ? "KEV_DL_ADDMULTI"             :
                msg.k.event_code == KEV_DL_DELMULTI             ? "KEV_DL_DELMULTI"             :
                msg.k.event_code == KEV_DL_IF_ATTACHED          ? "KEV_DL_IF_ATTACHED"          :
                msg.k.event_code == KEV_DL_IF_DETACHING         ? "KEV_DL_IF_DETACHING"         :
                msg.k.event_code == KEV_DL_IF_DETACHED          ? "KEV_DL_IF_DETACHED"          :
                msg.k.event_code == KEV_DL_LINK_OFF             ? "KEV_DL_LINK_OFF"             :
                msg.k.event_code == KEV_DL_LINK_ON              ? "KEV_DL_LINK_ON"              :
                msg.k.event_code == KEV_DL_PROTO_ATTACHED       ? "KEV_DL_PROTO_ATTACHED"       :
                msg.k.event_code == KEV_DL_PROTO_DETACHED       ? "KEV_DL_PROTO_DETACHED"       :
                msg.k.event_code == KEV_DL_LINK_ADDRESS_CHANGED ? "KEV_DL_LINK_ADDRESS_CHANGED" :
                msg.k.event_code == KEV_DL_WAKEFLAGS_CHANGED    ? "KEV_DL_WAKEFLAGS_CHANGED"    :
                msg.k.event_code == KEV_DL_IF_IDLE_ROUTE_REFCNT ? "KEV_DL_IF_IDLE_ROUTE_REFCNT" :
                msg.k.event_code == KEV_DL_IFCAP_CHANGED        ? "KEV_DL_IFCAP_CHANGED"        :
                msg.k.event_code == KEV_DL_LINK_QUALITY_METRIC_CHANGED    ? "KEV_DL_LINK_QUALITY_METRIC_CHANGED"    :
                msg.k.event_code == KEV_DL_NODE_PRESENCE        ? "KEV_DL_NODE_PRESENCE"        :
                msg.k.event_code == KEV_DL_NODE_ABSENCE         ? "KEV_DL_NODE_ABSENCE"         :
                msg.k.event_code == KEV_DL_MASTER_ELECTED       ? "KEV_DL_MASTER_ELECTED"       :
                 "?");

        if (msg.k.event_code == KEV_DL_NODE_PRESENCE)
            nodePresence(m, (struct kev_dl_node_presence *) &msg.k.event_data);

        if (msg.k.event_code == KEV_DL_NODE_ABSENCE)
            nodeAbsence(m, (struct kev_dl_node_absence *) &msg.k.event_data);

        if (msg.k.event_code == KEV_DL_MASTER_ELECTED)
            newMasterElected(m, (struct net_event_data *) &msg.k.event_data);

        // We receive network change notifications both through configd and through SYSPROTO_EVENT socket.
        // Configd may not generate network change events for manually configured interfaces (i.e., non-DHCP)
        // always during sleep/wakeup due to some race conditions (See radar:8666757). At the same time, if
        // "Wake on Network Access" is not turned on, the notification will not have KEV_DL_WAKEFLAGS_CHANGED.
        // Hence, during wake up, if we see a KEV_DL_LINK_ON (i.e., link is UP), we trigger a network change.

        if (msg.k.event_code == KEV_DL_WAKEFLAGS_CHANGED || msg.k.event_code == KEV_DL_LINK_ON)
            SetNetworkChanged(m, mDNSPlatformOneSecond * 2);

#if !TARGET_OS_EMBEDDED     // don't setup packet filter rules on embedded

        // For p2p interfaces, need to open the advertised service port in the firewall.
        if (msg.k.event_code == KEV_DL_IF_ATTACHED)
        {
            struct net_event_data   * p;
            p = (struct net_event_data *) &msg.k.event_data;

            if (strncmp(p->if_name, "p2p", 3) == 0)
            {
                char ifname[IFNAMSIZ];
                snprintf(ifname, IFNAMSIZ, "%s%d", p->if_name, p->if_unit);

                LogInfo("SysEventCallBack: KEV_DL_IF_ATTACHED if_family = %d, if_unit = %d, if_name = %s", p->if_family, p->if_unit, p->if_name);

                mDNSSetPacketFilterRules(m, ifname, NULL);
            }
        }

        // For p2p interfaces, need to clear the firewall rules on interface detach
        if (msg.k.event_code == KEV_DL_IF_DETACHED)
        {
            struct net_event_data   * p;
            p = (struct net_event_data *) &msg.k.event_data;

            if (strncmp(p->if_name, "p2p", 3) == 0)
            {
                pfArray_t portArray, protocolArray; // not initialized since count is 0 for PF_CLEAR_RULES
                char ifname[IFNAMSIZ];
                snprintf(ifname, IFNAMSIZ, "%s%d", p->if_name, p->if_unit);

                LogInfo("SysEventCallBack: KEV_DL_IF_DETACHED if_family = %d, if_unit = %d, if_name = %s", p->if_family, p->if_unit, p->if_name);

                mDNSPacketFilterControl(PF_CLEAR_RULES, ifname, 0, portArray, protocolArray);
            }
        }
#endif // !TARGET_OS_EMBEDDED

    }

    mDNS_Unlock(m);
}

mDNSlocal mStatus WatchForSysEvents(mDNS *const m)
{
    m->p->SysEventNotifier = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT);
    if (m->p->SysEventNotifier < 0)
    { LogMsg("WatchForSysEvents: socket failed error %d errno %d (%s)", m->p->SysEventNotifier, errno, strerror(errno)); return(mStatus_NoMemoryErr); }

    struct kev_request kev_req = { KEV_VENDOR_APPLE, KEV_NETWORK_CLASS, KEV_DL_SUBCLASS };
    int err = ioctl(m->p->SysEventNotifier, SIOCSKEVFILT, &kev_req);
    if (err < 0)
    {
        LogMsg("WatchForSysEvents: SIOCSKEVFILT failed error %d errno %d (%s)", err, errno, strerror(errno));
        close(m->p->SysEventNotifier);
        m->p->SysEventNotifier = -1;
        return(mStatus_UnknownErr);
    }

    m->p->SysEventKQueue.KQcallback = SysEventCallBack;
    m->p->SysEventKQueue.KQcontext  = m;
    m->p->SysEventKQueue.KQtask     = "System Event Notifier";
    KQueueSet(m->p->SysEventNotifier, EV_ADD, EVFILT_READ, &m->p->SysEventKQueue);

    return(mStatus_NoError);
}

#ifndef NO_SECURITYFRAMEWORK
mDNSlocal OSStatus KeychainChanged(SecKeychainEvent keychainEvent, SecKeychainCallbackInfo *info, void *context)
{
    LogInfo("***   Keychain Changed   ***");
    mDNS *const m = (mDNS *const)context;
    SecKeychainRef skc;
    OSStatus err = SecKeychainCopyDefault(&skc);
    if (!err)
    {
        if (info->keychain == skc)
        {
            // For delete events, attempt to verify what item was deleted fail because the item is already gone, so we just assume they may be relevant
            mDNSBool relevant = (keychainEvent == kSecDeleteEvent);
            if (!relevant)
            {
                UInt32 tags[3] = { kSecTypeItemAttr, kSecServiceItemAttr, kSecAccountItemAttr };
                SecKeychainAttributeInfo attrInfo = { 3, tags, NULL };  // Count, array of tags, array of formats
                SecKeychainAttributeList *a = NULL;
                err = SecKeychainItemCopyAttributesAndData(info->item, &attrInfo, NULL, &a, NULL, NULL);
                if (!err)
                {
                    relevant = ((a->attr[0].length == 4 && (!strncasecmp(a->attr[0].data, "ddns", 4) || !strncasecmp(a->attr[0].data, "sndd", 4))) ||
                                (a->attr[1].length >= mDNSPlatformStrLen(dnsprefix) && (!strncasecmp(a->attr[1].data, dnsprefix, mDNSPlatformStrLen(dnsprefix)))) ||
                                (a->attr[1].length >= mDNSPlatformStrLen(btmmprefix) && (!strncasecmp(a->attr[1].data, btmmprefix, mDNSPlatformStrLen(btmmprefix)))));
                    SecKeychainItemFreeAttributesAndData(a, NULL);
                }
            }
            if (relevant)
            {
                LogInfo("***   Keychain Changed   *** KeychainEvent=%d %s",
                        keychainEvent,
                        keychainEvent == kSecAddEvent    ? "kSecAddEvent"    :
                        keychainEvent == kSecDeleteEvent ? "kSecDeleteEvent" :
                        keychainEvent == kSecUpdateEvent ? "kSecUpdateEvent" : "<Unknown>");
                // We're running on the CFRunLoop (Mach port) thread, not the kqueue thread, so we need to grab the KQueueLock before proceeding
                KQueueLock(m);
                mDNS_Lock(m);

                // To not read the keychain twice: when BTMM is enabled, changes happen to the keychain
                // then the BTMM DynStore dictionary, so delay reading the keychain for a second.
                // NetworkChanged() will reset the keychain timer to fire immediately when the DynStore changes.
                //
                // In the "fixup" case where the BTMM DNS servers aren't accepting the key mDNSResponder has,
                // the DynStore dictionary won't change (because the BTMM zone won't change).  In that case,
                // a one second delay is ok, as we'll still converge to correctness, and there's no race
                // condition between the RegistrationDomain and the DomainAuthInfo.
                //
                // Lastly, non-BTMM WAB cases can use the keychain but not the DynStore, so we need to set
                // the timer here, as it will not get set by NetworkChanged().
                SetKeyChainTimer(m, mDNSPlatformOneSecond);

                mDNS_Unlock(m);
                KQueueUnlock(m, "KeychainChanged");
            }
        }
        CFRelease(skc);
    }

    return 0;
}
#endif

mDNSlocal void PowerOn(mDNS *const m)
{
    mDNSCoreMachineSleep(m, false);     // Will set m->SleepState = SleepState_Awake;

    if (m->p->WakeAtUTC)
    {
        long utc = mDNSPlatformUTC();
        mDNSPowerRequest(-1,-1);        // Need to explicitly clear any previous power requests -- they're not cleared automatically on wake
        if (m->p->WakeAtUTC - utc > 30)
        {
            LogSPS("PowerChanged PowerOn %d seconds early, assuming not maintenance wake", m->p->WakeAtUTC - utc);
        }
        else if (utc - m->p->WakeAtUTC > 30)
        {
            LogSPS("PowerChanged PowerOn %d seconds late, assuming not maintenance wake", utc - m->p->WakeAtUTC);
        }
        else if (IsAppleTV())
        {
            LogSPS("PowerChanged PowerOn %d seconds late, device is an AppleTV running iOS so not re-sleeping", utc - m->p->WakeAtUTC);
        }
        else
        {
            LogSPS("PowerChanged: Waking for network maintenance operations %d seconds early; re-sleeping in 20 seconds", m->p->WakeAtUTC - utc);
            m->p->RequestReSleep = mDNS_TimeNow(m) + 20 * mDNSPlatformOneSecond;
        }
    }

	// Hold on to a sleep assertion to allow mDNSResponder to perform its maintenance activities.
	// This allows for the network link to come up, DHCP to get an address, mDNS to issue queries etc.
	// We will clear this assertion as soon as we think the mainenance activities are done.
	mDNSPlatformPreventSleep(m, DARK_WAKE_TIME, "mDNSResponder:maintenance");

}

mDNSlocal void PowerChanged(void *refcon, io_service_t service, natural_t messageType, void *messageArgument)
{
    mDNS *const m = (mDNS *const)refcon;
    KQueueLock(m);
    (void)service;    // Parameter not used
    debugf("PowerChanged %X %lX", messageType, messageArgument);

    // Make sure our m->SystemWakeOnLANEnabled value correctly reflects the current system setting
    m->SystemWakeOnLANEnabled = SystemWakeForNetworkAccess();

    switch(messageType)
    {
    case kIOMessageCanSystemPowerOff:       LogSPS("PowerChanged kIOMessageCanSystemPowerOff     (no action)"); break;          // E0000240
    case kIOMessageSystemWillPowerOff:      LogSPS("PowerChanged kIOMessageSystemWillPowerOff");                                // E0000250
        mDNSCoreMachineSleep(m, true);
        if (m->SleepState == SleepState_Sleeping) mDNSMacOSXNetworkChanged(m);
        break;
    case kIOMessageSystemWillNotPowerOff:   LogSPS("PowerChanged kIOMessageSystemWillNotPowerOff (no action)"); break;          // E0000260
    case kIOMessageCanSystemSleep:          LogSPS("PowerChanged kIOMessageCanSystemSleep");                    break;          // E0000270
    case kIOMessageSystemWillSleep:         LogSPS("PowerChanged kIOMessageSystemWillSleep");                                   // E0000280
        mDNSCoreMachineSleep(m, true);
        break;
    case kIOMessageSystemWillNotSleep:      LogSPS("PowerChanged kIOMessageSystemWillNotSleep    (no action)"); break;          // E0000290
    case kIOMessageSystemHasPoweredOn:      LogSPS("PowerChanged kIOMessageSystemHasPoweredOn");                                // E0000300
        // If still sleeping (didn't get 'WillPowerOn' message for some reason?) wake now
        if (m->SleepState)
        {
            LogMsg("PowerChanged kIOMessageSystemHasPoweredOn: ERROR m->SleepState %d", m->SleepState);
            PowerOn(m);
        }
        // Just to be safe, schedule a mDNSMacOSXNetworkChanged(), in case we never received
        // the System Configuration Framework "network changed" event that we expect
        // to receive some time shortly after the kIOMessageSystemWillPowerOn message
        mDNS_Lock(m);
        SetNetworkChanged(m, mDNSPlatformOneSecond * 2);
        mDNS_Unlock(m);

        break;
    case kIOMessageSystemWillRestart:       LogSPS("PowerChanged kIOMessageSystemWillRestart     (no action)"); break;          // E0000310
    case kIOMessageSystemWillPowerOn:       LogSPS("PowerChanged kIOMessageSystemWillPowerOn");                                 // E0000320

        // Make sure our interface list is cleared to the empty state, then tell mDNSCore to wake
        if (m->SleepState != SleepState_Sleeping)
        {
            LogMsg("kIOMessageSystemWillPowerOn: ERROR m->SleepState %d", m->SleepState);
            m->SleepState = SleepState_Sleeping;
            mDNSMacOSXNetworkChanged(m);
        }
        PowerOn(m);
        break;
    default:                                LogSPS("PowerChanged unknown message %X", messageType); break;
    }

    if (messageType == kIOMessageSystemWillSleep)
        m->p->SleepCookie = (long)messageArgument;
    else if (messageType == kIOMessageCanSystemSleep)
        IOAllowPowerChange(m->p->PowerConnection, (long)messageArgument);

    KQueueUnlock(m, "PowerChanged Sleep/Wake");
}

// iPhone OS doesn't currently have SnowLeopard's IO Power Management
// but it does define kIOPMAcknowledgmentOptionSystemCapabilityRequirements
#if defined(kIOPMAcknowledgmentOptionSystemCapabilityRequirements) && !TARGET_OS_EMBEDDED
mDNSlocal void SnowLeopardPowerChanged(void *refcon, IOPMConnection connection, IOPMConnectionMessageToken token, IOPMSystemPowerStateCapabilities eventDescriptor)
{
    mDNS *const m = (mDNS *const)refcon;
    KQueueLock(m);
    LogSPS("SnowLeopardPowerChanged %X %X %X%s%s%s%s%s",
           connection, token, eventDescriptor,
           eventDescriptor & kIOPMSystemPowerStateCapabilityCPU     ? " CPU"     : "",
           eventDescriptor & kIOPMSystemPowerStateCapabilityVideo   ? " Video"   : "",
           eventDescriptor & kIOPMSystemPowerStateCapabilityAudio   ? " Audio"   : "",
           eventDescriptor & kIOPMSystemPowerStateCapabilityNetwork ? " Network" : "",
           eventDescriptor & kIOPMSystemPowerStateCapabilityDisk    ? " Disk"    : "");

    // Make sure our m->SystemWakeOnLANEnabled value correctly reflects the current system setting
    m->SystemWakeOnLANEnabled = SystemWakeForNetworkAccess();

    if (eventDescriptor & kIOPMSystemPowerStateCapabilityCPU)
    {
        // We might be in Sleeping or Transferring state. When we go from "wakeup" to "sleep" state, we don't
        // go directly to sleep state, but transfer in to the sleep state during which SleepState is set to
        // SleepState_Transferring. During that time, we might get another wakeup before we transition to Sleeping
        // state. In that case, we need to acknowledge the previous "sleep" before we acknowledge the wakeup.
        if (m->SleepLimit)
        {
            LogSPS("SnowLeopardPowerChanged: Waking up, Acking old Sleep, SleepLimit %d SleepState %d", m->SleepLimit, m->SleepState);
            IOPMConnectionAcknowledgeEvent(connection, m->p->SleepCookie);
            m->SleepLimit = 0;
        }
        LogSPS("SnowLeopardPowerChanged: Waking up, Acking Wakeup, SleepLimit %d SleepState %d", m->SleepLimit, m->SleepState);
        // CPU Waking. Note: Can get this message repeatedly, as other subsystems power up or down.
        if (m->SleepState != SleepState_Awake)
        {
			PowerOn(m);
			// If the network notifications have already come before we got the wakeup, we ignored them and
			// in case we get no more, we need to trigger one.
			mDNS_Lock(m);
			SetNetworkChanged(m, mDNSPlatformOneSecond * 2);
			mDNS_Unlock(m);
        }
        IOPMConnectionAcknowledgeEvent(connection, token);
    }
    else
    {
        // CPU sleeping. Should not get this repeatedly -- once we're told that the CPU is halting
        // we should hear nothing more until we're told that the CPU has started executing again.
        if (m->SleepState) LogMsg("SnowLeopardPowerChanged: Sleep Error %X m->SleepState %d", eventDescriptor, m->SleepState);
        //sleep(5);
        //mDNSMacOSXNetworkChanged(m);
        mDNSCoreMachineSleep(m, true);
        //if (m->SleepState == SleepState_Sleeping) mDNSMacOSXNetworkChanged(m);
        m->p->SleepCookie = token;
    }

    KQueueUnlock(m, "SnowLeopardPowerChanged Sleep/Wake");
}
#endif

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - /etc/hosts support
#endif

// Implementation Notes
//
// As /etc/hosts file can be huge (1000s of entries - when this comment was written, the test file had about
// 23000 entries with about 4000 duplicates), we can't use a linked list to store these entries. So, we parse
// them into a hash table. The implementation need to be able to do the following things efficiently
//
// 1. Detect duplicates e.g., two entries with "1.2.3.4 foo"
// 2. Detect whether /etc/hosts has changed and what has changed since the last read from the disk
// 3. Ability to support multiple addresses per name e.g., "1.2.3.4 foo, 2.3.4.5 foo". To support this, we
//    need to be able set the RRSet of a resource record to the first one in the list and also update when
//    one of them go away. This is needed so that the core thinks that they are all part of the same RRSet and
//    not a duplicate
// 4. Don't maintain any local state about any records registered with the core to detect changes to /etc/hosts
//
// CFDictionary is not a suitable candidate because it does not support duplicates and even if we use a custom
// "hash" function to solve this, the others are hard to solve. Hence, we share the hash (AuthHash) implementation
// of the core layer which does all of the above very efficiently

#define ETCHOSTS_BUFSIZE    1024    // Buffer size to parse a single line in /etc/hosts

mDNSexport void FreeEtcHosts(mDNS *const m, AuthRecord *const rr, mStatus result)
{
    (void)m;  // unused
    (void)rr;
    (void)result;
    if (result == mStatus_MemFree)
    {
        LogInfo("FreeEtcHosts: %s", ARDisplayString(m, rr));
        freeL("etchosts", rr);
    }
}

// Returns true on success and false on failure
mDNSlocal mDNSBool mDNSMacOSXCreateEtcHostsEntry(mDNS *const m, const domainname *domain, const struct sockaddr *sa, const domainname *cname, char *ifname, AuthHash *auth)
{
    AuthRecord *rr;
    mDNSu32 slot;
    mDNSu32 namehash;
    AuthGroup *ag;
    mDNSInterfaceID InterfaceID = mDNSInterface_LocalOnly;
    mDNSu16 rrtype;

    if (!domain)
    {
        LogMsg("mDNSMacOSXCreateEtcHostsEntry: ERROR!! name NULL");
        return mDNSfalse;
    }
    if (!sa && !cname)
    {
        LogMsg("mDNSMacOSXCreateEtcHostsEntry: ERROR!! sa and cname both NULL");
        return mDNSfalse;
    }

    if (sa && sa->sa_family != AF_INET && sa->sa_family != AF_INET6)
    {
        LogMsg("mDNSMacOSXCreateEtcHostsEntry: ERROR!! sa with bad family %d", sa->sa_family);
        return mDNSfalse;
    }


    if (ifname)
    {
        mDNSu32 ifindex = if_nametoindex(ifname);
        if (!ifindex)
        {
            LogMsg("mDNSMacOSXCreateEtcHostsEntry: hosts entry %##s with invalid ifname %s", domain->c, ifname);
            return mDNSfalse;
        }
        InterfaceID = (mDNSInterfaceID)(uintptr_t)ifindex;
    }

    if (sa)
        rrtype = (sa->sa_family == AF_INET ? kDNSType_A : kDNSType_AAAA);
    else
        rrtype = kDNSType_CNAME;

    // Check for duplicates. See whether we parsed an entry before like this ?
    slot = AuthHashSlot(domain);
    namehash = DomainNameHashValue(domain);
    ag = AuthGroupForName(auth, slot, namehash, domain);
    if (ag)
    {
        rr = ag->members;
        while (rr)
        {
            if (rr->resrec.rrtype == rrtype)
            {
                if (rrtype == kDNSType_A)
                {
                    mDNSv4Addr ip;
                    ip.NotAnInteger = ((struct sockaddr_in*)sa)->sin_addr.s_addr;
                    if (mDNSSameIPv4Address(rr->resrec.rdata->u.ipv4, ip))
                    {
                        LogInfo("mDNSMacOSXCreateEtcHostsEntry: Same IPv4 address for name %##s", domain->c);
                        return mDNSfalse;
                    }
                }
                else if (rrtype == kDNSType_AAAA)
                {
                    mDNSv6Addr ip6;
                    ip6.l[0] = ((struct sockaddr_in6*)sa)->sin6_addr.__u6_addr.__u6_addr32[0];
                    ip6.l[1] = ((struct sockaddr_in6*)sa)->sin6_addr.__u6_addr.__u6_addr32[1];
                    ip6.l[2] = ((struct sockaddr_in6*)sa)->sin6_addr.__u6_addr.__u6_addr32[2];
                    ip6.l[3] = ((struct sockaddr_in6*)sa)->sin6_addr.__u6_addr.__u6_addr32[3];
                    if (mDNSSameIPv6Address(rr->resrec.rdata->u.ipv6, ip6))
                    {
                        LogInfo("mDNSMacOSXCreateEtcHostsEntry: Same IPv6 address for name %##s", domain->c);
                        return mDNSfalse;
                    }
                }
                else if (rrtype == kDNSType_CNAME)
                {
                    if (SameDomainName(&rr->resrec.rdata->u.name, cname))
                    {
                        LogInfo("mDNSMacOSXCreateEtcHostsEntry: Same cname %##s for name %##s", cname->c, domain->c);
                        return mDNSfalse;
                    }
                }
            }
            rr = rr->next;
        }
    }
    rr= mallocL("etchosts", sizeof(*rr));
    if (rr == NULL) return mDNSfalse;
    mDNSPlatformMemZero(rr, sizeof(*rr));
    mDNS_SetupResourceRecord(rr, NULL, InterfaceID, rrtype, 1, kDNSRecordTypeKnownUnique, AuthRecordLocalOnly, FreeEtcHosts, NULL);
    AssignDomainName(&rr->namestorage, domain);

    if (sa)
    {
        rr->resrec.rdlength = sa->sa_family == AF_INET ? sizeof(mDNSv4Addr) : sizeof(mDNSv6Addr);
        if (sa->sa_family == AF_INET)
            rr->resrec.rdata->u.ipv4.NotAnInteger = ((struct sockaddr_in*)sa)->sin_addr.s_addr;
        else
        {
            rr->resrec.rdata->u.ipv6.l[0] = ((struct sockaddr_in6*)sa)->sin6_addr.__u6_addr.__u6_addr32[0];
            rr->resrec.rdata->u.ipv6.l[1] = ((struct sockaddr_in6*)sa)->sin6_addr.__u6_addr.__u6_addr32[1];
            rr->resrec.rdata->u.ipv6.l[2] = ((struct sockaddr_in6*)sa)->sin6_addr.__u6_addr.__u6_addr32[2];
            rr->resrec.rdata->u.ipv6.l[3] = ((struct sockaddr_in6*)sa)->sin6_addr.__u6_addr.__u6_addr32[3];
        }
    }
    else
    {
        rr->resrec.rdlength = DomainNameLength(cname);
        rr->resrec.rdata->u.name.c[0] = 0;
        AssignDomainName(&rr->resrec.rdata->u.name, cname);
    }
    rr->resrec.namehash = DomainNameHashValue(rr->resrec.name);
    SetNewRData(&rr->resrec, mDNSNULL, 0);  // Sets rr->rdatahash for us
    LogInfo("mDNSMacOSXCreateEtcHostsEntry: Adding resource record %s", ARDisplayString(m, rr));
    InsertAuthRecord(m, auth, rr);
    return mDNStrue;
}

mDNSlocal int EtcHostsParseOneName(int start, int length, char *buffer, char **name)
{
    int i;

    *name = NULL;
    for (i = start; i < length; i++)
    {
        if (buffer[i] == '#')
            return -1;
        if (buffer[i] != ' ' && buffer[i] != ',' && buffer[i] != '\t')
        {
            *name = &buffer[i];

            // Found the start of a name, find the end and null terminate
            for (i++; i < length; i++)
            {
                if (buffer[i] == ' ' || buffer[i] == ',' || buffer[i] == '\t')
                {
                    buffer[i] = 0;
                    break;
                }
            }
            return i;
        }
    }
    return -1;
}

mDNSlocal void mDNSMacOSXParseEtcHostsLine(mDNS *const m, char *buffer, ssize_t length, AuthHash *auth)
{
    int i;
    int ifStart = 0;
    char *ifname = NULL;
    domainname name1d;
    domainname name2d;
    char *name1;
    char *name2;
    int aliasIndex;

    //Ignore leading whitespaces and tabs
    while (*buffer == ' ' || *buffer == '\t')
    {
        buffer++;
        length--;
    }

    // Find the end of the address string
    for (i = 0; i < length; i++)
    {
        if (buffer[i] == ' ' || buffer[i] == ',' || buffer[i] == '\t' || buffer[i] == '%')
        {
            if (buffer[i] == '%')
                ifStart = i + 1;
            buffer[i] = 0;
            break;
        }
    }

    // Convert the address string to an address
    struct addrinfo hints;
    bzero(&hints, sizeof(hints));
    hints.ai_flags = AI_NUMERICHOST;
    struct addrinfo *gairesults = NULL;
    if (getaddrinfo(buffer, NULL, &hints, &gairesults) != 0)
    {
        LogInfo("mDNSMacOSXParseEtcHostsLine: getaddrinfo returning null");
        return;
    }

    if (ifStart)
    {
        // Parse the interface
        ifname = &buffer[ifStart];
        for (i = ifStart + 1; i < length; i++)
        {
            if (buffer[i] == ' ' || buffer[i] == ',' || buffer[i] == '\t')
            {
                buffer[i] = 0;
                break;
            }
        }
    }

    i = EtcHostsParseOneName(i + 1, length, buffer, &name1);
    if (i == length)
    {
        // Common case (no aliases) : The entry is of the form "1.2.3.4 somehost" with no trailing white spaces/tabs etc.
        if (!MakeDomainNameFromDNSNameString(&name1d, name1))
        {
            LogMsg("mDNSMacOSXParseEtcHostsLine: ERROR!! cannot convert to domain name %s", name1);
            freeaddrinfo(gairesults);
            return;
        }
        mDNSMacOSXCreateEtcHostsEntry(m, &name1d, gairesults->ai_addr, mDNSNULL, ifname, auth);
    }
    else if (i != -1)
    {
        domainname first;
        // We might have some extra white spaces at the end for the common case of "1.2.3.4 somehost".
        // When we parse again below, EtchHostsParseOneName would return -1 and we will end up
        // doing the right thing.
        
        if (!MakeDomainNameFromDNSNameString(&first, name1))
        {
            LogMsg("mDNSMacOSXParseEtcHostsLine: ERROR!! cannot convert to domain name %s", name1);
            freeaddrinfo(gairesults);
            return;
        }
        mDNSMacOSXCreateEtcHostsEntry(m, &first, gairesults->ai_addr, mDNSNULL, ifname, auth);
        
        // /etc/hosts alias discussion:
        //
        // If the /etc/hosts has an entry like this
        //
        //  ip_address cname [aliases...]
        //  1.2.3.4    sun    star    bright
        //
        // star and bright are aliases (gethostbyname h_alias should point to these) and sun is the canonical
        // name (getaddrinfo ai_cannonname and gethostbyname h_name points to "sun")
        //
        // To achieve this, we need to add the entry like this:
        //
        // sun A 1.2.3.4
        // star CNAME sun
        // bright CNAME sun
        //
        // We store the first name we parsed in "first" and add the address (A/AAAA) record.
        // Then we parse additional names adding CNAME records till we reach the end.
        
        aliasIndex = 0;
        while (i < length)
        {
            // Continue to parse additional aliases until we reach end of the line and
            // for each "alias" parsed, add a CNAME record where "alias" points to the first "name".
            // See also /etc/hosts alias discussion above
            
            i = EtcHostsParseOneName(i + 1, length, buffer, &name2);
            
            if (name2)
            {
                if ((aliasIndex) && (*buffer == *name2))
                    break; // break out of the loop if we wrap around
                
                if (!MakeDomainNameFromDNSNameString(&name2d, name2))
                {
                    LogMsg("mDNSMacOSXParseEtcHostsLine: ERROR!! cannot convert to domain name %s", name2);
                    freeaddrinfo(gairesults);
                    return;
                }
                // Ignore if it points to itself
                if (!SameDomainName(&first, &name2d))
                {
                    if (!mDNSMacOSXCreateEtcHostsEntry(m, &name2d, mDNSNULL, &first, ifname, auth))
                    {
                        freeaddrinfo(gairesults);
                        return;
                    }
                }
                else
                {
                    LogInfo("mDNSMacOSXParseEtcHostsLine: Ignoring entry with same names first %##s, name2 %##s", first.c, name2d.c);
                }
                aliasIndex++;
            }
            else if (!aliasIndex)
            {
                // We have never parsed any aliases. This case happens if there
                // is just one name and some extra white spaces at the end.
                LogInfo("mDNSMacOSXParseEtcHostsLine: White space at the end of %##s", first.c);
                break;
            }
        }
    }
    freeaddrinfo(gairesults);
}

mDNSlocal void mDNSMacOSXParseEtcHosts(mDNS *const m, int fd, AuthHash *auth)
{
    mDNSBool good;
    char buf[ETCHOSTS_BUFSIZE];
    ssize_t len;
    FILE *fp;

    if (fd == -1) { LogInfo("mDNSMacOSXParseEtcHosts: fd is -1"); return; }

    fp = fopen("/etc/hosts", "r");
    if (!fp) { LogInfo("mDNSMacOSXParseEtcHosts: fp is NULL"); return; }

    while (1)
    {
        good = (fgets(buf, ETCHOSTS_BUFSIZE, fp) != NULL);
        if (!good) break;

        // skip comment and empty lines
        if (buf[0] == '#' || buf[0] == '\r' || buf[0] == '\n')
            continue;

        len = strlen(buf);
        if (!len) break;    // sanity check
        //Check for end of line code(mostly only \n but pre-OS X Macs could have only \r)
        if (buf[len - 1] == '\r' || buf[len - 1] == '\n')
        {
            buf[len - 1] = '\0';
            len = len - 1;
        }
        // fgets always null terminates and hence even if we have no
        // newline at the end, it is null terminated. The callee
        // (mDNSMacOSXParseEtcHostsLine) expects the length to be such that
        // buf[length] is zero and hence we decrement len to reflect that.
        if (len)
        {
            //Additional check when end of line code is 2 chars ie\r\n(DOS, other old OSes)
            //here we need to check for just \r but taking extra caution.
            if (buf[len - 1] == '\r' || buf[len - 1] == '\n')
            {
                buf[len - 1] = '\0';
                len = len - 1;
            }
        }
        if (!len) //Sanity Check: len should never be zero
        {
            LogMsg("mDNSMacOSXParseEtcHosts: Length is zero!");
            continue;
        }
        mDNSMacOSXParseEtcHostsLine(m, buf, len, auth);
    }
    fclose(fp);
}

mDNSlocal void mDNSMacOSXUpdateEtcHosts(mDNS *const m);

mDNSlocal int mDNSMacOSXGetEtcHostsFD(mDNS *const m)
{
#ifdef __DISPATCH_GROUP__
    // Can't do this stuff to be notified of changes in /etc/hosts if we don't have libdispatch
    static dispatch_queue_t etcq     = 0;
    static dispatch_source_t etcsrc   = 0;
    static dispatch_source_t hostssrc = 0;

    // First time through? just schedule ourselves on the main queue and we'll do the work later
    if (!etcq)
    {
        etcq = dispatch_get_main_queue();
        if (etcq)
        {
            // Do this work on the queue, not here - solves potential synchronization issues
            dispatch_async(etcq, ^{mDNSMacOSXUpdateEtcHosts(m);});
        }
        return -1;
    }

    if (hostssrc) return dispatch_source_get_handle(hostssrc);
#endif

    int fd = open("/etc/hosts", O_RDONLY);

#ifdef __DISPATCH_GROUP__
    // Can't do this stuff to be notified of changes in /etc/hosts if we don't have libdispatch
    if (fd == -1)
    {
        // If the open failed and we're already watching /etc, we're done
        if (etcsrc) { LogInfo("mDNSMacOSXGetEtcHostsFD: Returning etcfd because no etchosts"); return fd; }

        // we aren't watching /etc, we should be
        fd = open("/etc", O_RDONLY);
        if (fd == -1) { LogInfo("mDNSMacOSXGetEtcHostsFD: etc does not exist"); return -1; }
        etcsrc = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, fd, DISPATCH_VNODE_DELETE | DISPATCH_VNODE_WRITE | DISPATCH_VNODE_RENAME, etcq);
        if (etcsrc == NULL)
        {
            close(fd);
            return -1;
        }
        dispatch_source_set_event_handler(etcsrc,
                                          ^{
                                              u_int32_t flags = dispatch_source_get_data(etcsrc);
                                              LogMsg("mDNSMacOSXGetEtcHostsFD: /etc changed 0x%x", flags);
                                              if ((flags & (DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME)) != 0)
                                              {
                                                  dispatch_source_cancel(etcsrc);
                                                  dispatch_release(etcsrc);
                                                  etcsrc = NULL;
                                                  dispatch_async(etcq, ^{mDNSMacOSXUpdateEtcHosts(m);});
                                                  return;
                                              }
                                              if ((flags & DISPATCH_VNODE_WRITE) != 0 && hostssrc == NULL)
                                              {
                                                  mDNSMacOSXUpdateEtcHosts(m);
                                              }
                                          });
        dispatch_source_set_cancel_handler(etcsrc, ^{close(fd);});
        dispatch_resume(etcsrc);

        // Try and open /etc/hosts once more now that we're watching /etc, in case we missed the creation
        fd = open("/etc/hosts", O_RDONLY | O_EVTONLY);
        if (fd == -1) { LogMsg("mDNSMacOSXGetEtcHostsFD etc hosts does not exist, watching etc"); return -1; }
    }

    // create a dispatch source to watch for changes to hosts file
    hostssrc = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, fd,
                                      (DISPATCH_VNODE_DELETE | DISPATCH_VNODE_WRITE | DISPATCH_VNODE_RENAME |
                                       DISPATCH_VNODE_ATTRIB | DISPATCH_VNODE_EXTEND | DISPATCH_VNODE_LINK | DISPATCH_VNODE_REVOKE), etcq);
    if (hostssrc == NULL)
    {
        close(fd);
        return -1;
    }
    dispatch_source_set_event_handler(hostssrc,
                                      ^{
                                          u_int32_t flags = dispatch_source_get_data(hostssrc);
                                          LogInfo("mDNSMacOSXGetEtcHostsFD: /etc/hosts changed 0x%x", flags);
                                          if ((flags & (DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME)) != 0)
                                          {
                                              dispatch_source_cancel(hostssrc);
                                              dispatch_release(hostssrc);
                                              hostssrc = NULL;
                                              // Bug in LibDispatch: wait a second before scheduling the block. If we schedule
                                              // the block immediately, we try to open the file and the file may not exist and may
                                              // fail to get a notification in the future. When the file does not exist and
                                              // we start to monitor the directory, on "dispatch_resume" of that source, there
                                              // is no guarantee that the file creation will be notified always because when
                                              // the dispatch_resume returns, the kevent manager may not have registered the
                                              // kevent yet but the file may have been created
                                              usleep(1000000);
                                              dispatch_async(etcq, ^{mDNSMacOSXUpdateEtcHosts(m);});
                                              return;
                                          }
                                          if ((flags & DISPATCH_VNODE_WRITE) != 0)
                                          {
                                              mDNSMacOSXUpdateEtcHosts(m);
                                          }
                                      });
    dispatch_source_set_cancel_handler(hostssrc, ^{LogInfo("mDNSMacOSXGetEtcHostsFD: Closing etchosts fd %d", fd); close(fd);});
    dispatch_resume(hostssrc);

    // Cleanup /etc source, no need to watch it if we already have /etc/hosts
    if (etcsrc)
    {
        dispatch_source_cancel(etcsrc);
        dispatch_release(etcsrc);
        etcsrc = NULL;
    }

    LogInfo("mDNSMacOSXGetEtcHostsFD: /etc/hosts being monitored, and not etc");
    return hostssrc ? (int)dispatch_source_get_handle(hostssrc) : -1;
#else
    (void)m;
    return fd;
#endif
}

// When /etc/hosts is modified, flush all the cache records as there may be local
// authoritative answers now
mDNSlocal void FlushAllCacheRecords(mDNS *const m)
{
    CacheRecord *cr;
    mDNSu32 slot;
    CacheGroup *cg;

    FORALL_CACHERECORDS(slot, cg, cr)
    {
        // Skip multicast.
        if (cr->resrec.InterfaceID) continue;

        // If a resource record can answer A or AAAA, they need to be flushed so that we will
        // never used to deliver an ADD or RMV
        if (RRTypeAnswersQuestionType(&cr->resrec, kDNSType_A) ||
            RRTypeAnswersQuestionType(&cr->resrec, kDNSType_AAAA))
        {
            LogInfo("FlushAllCacheRecords: Purging Resourcerecord %s", CRDisplayString(m, cr));
            mDNS_PurgeCacheResourceRecord(m, cr);
        }
    }
}

// Add new entries to the core. If justCheck is set, this function does not add, just returns true
mDNSlocal mDNSBool EtcHostsAddNewEntries(mDNS *const m, AuthHash *newhosts, mDNSBool justCheck)
{
    AuthGroup *ag;
    mDNSu32 slot;
    AuthRecord *rr, *primary, *rrnext;
    for (slot = 0; slot < AUTH_HASH_SLOTS; slot++)
        for (ag = newhosts->rrauth_hash[slot]; ag; ag = ag->next)
        {
            primary = NULL;
            for (rr = ag->members; rr; rr = rrnext)
            {
                rrnext = rr->next;
                AuthGroup *ag1;
                AuthRecord *rr1;
                mDNSBool found = mDNSfalse;
                ag1 = AuthGroupForRecord(&m->rrauth, slot, &rr->resrec);
                if (ag1 && ag1->members)
                {
                    if (!primary) primary = ag1->members;
                    rr1 = ag1->members;
                    while (rr1)
                    {
                        // We are not using InterfaceID in checking for duplicates. This means,
                        // if there are two addresses for a given name e.g., fe80::1%en0 and
                        // fe80::1%en1, we only add the first one. It is not clear whether
                        // this is a common case. To fix this, we also need to modify
                        // mDNS_Register_internal in how it handles duplicates. If it becomes a
                        // common case, we will fix it then.
                        if (IdenticalResourceRecord(&rr1->resrec, &rr->resrec))
                        {
                            LogInfo("EtcHostsAddNewEntries: Skipping, not adding %s", ARDisplayString(m, rr1));
                            found = mDNStrue;
                            break;
                        }
                        rr1 = rr1->next;
                    }
                }
                if (!found)
                {
                    if (justCheck)
                    {
                        LogInfo("EtcHostsAddNewEntries: Entry %s not registered with core yet", ARDisplayString(m, rr));
                        return mDNStrue;
                    }
                    RemoveAuthRecord(m, newhosts, rr);
                    // if there is no primary, point to self
                    rr->RRSet = (primary ? primary : rr);
                    rr->next = NULL;
                    LogInfo("EtcHostsAddNewEntries: Adding %s", ARDisplayString(m, rr));
                    if (mDNS_Register_internal(m, rr) != mStatus_NoError)
                        LogMsg("EtcHostsAddNewEntries: mDNS_Register failed for %s", ARDisplayString(m, rr));
                }
            }
        }
    return mDNSfalse;
}

// Delete entries from the core that are no longer needed. If justCheck is set, this function
// does not delete, just returns true
mDNSlocal mDNSBool EtcHostsDeleteOldEntries(mDNS *const m, AuthHash *newhosts, mDNSBool justCheck)
{
    AuthGroup *ag;
    mDNSu32 slot;
    AuthRecord *rr, *rrnext;
    for (slot = 0; slot < AUTH_HASH_SLOTS; slot++)
        for (ag = m->rrauth.rrauth_hash[slot]; ag; ag = ag->next)
            for (rr = ag->members; rr; rr = rrnext)
            {
                mDNSBool found = mDNSfalse;
                AuthGroup *ag1;
                AuthRecord *rr1;
                rrnext = rr->next;
                if (rr->RecordCallback != FreeEtcHosts) continue;
                ag1 = AuthGroupForRecord(newhosts, slot, &rr->resrec);
                if (ag1)
                {
                    rr1 = ag1->members;
                    while (rr1)
                    {
                        if (IdenticalResourceRecord(&rr1->resrec, &rr->resrec))
                        {
                            LogInfo("EtcHostsDeleteOldEntries: Old record %s found in new, skipping", ARDisplayString(m, rr));
                            found = mDNStrue;
                            break;
                        }
                        rr1 = rr1->next;
                    }
                }
                // there is no corresponding record in newhosts for the same name. This means
                // we should delete this from the core.
                if (!found)
                {
                    if (justCheck)
                    {
                        LogInfo("EtcHostsDeleteOldEntries: Record %s not found in new, deleting", ARDisplayString(m, rr));
                        return mDNStrue;
                    }
                    // if primary is going away, make sure that the rest of the records
                    // point to the new primary
                    if (rr == ag->members)
                    {
                        AuthRecord *new_primary = rr->next;
                        AuthRecord *r = new_primary;
                        while (r)
                        {
                            if (r->RRSet == rr)
                            {
                                LogInfo("EtcHostsDeleteOldEntries: Updating Resource Record %s to primary", ARDisplayString(m, r));
                                r->RRSet = new_primary;
                            }
                            else LogMsg("EtcHostsDeleteOldEntries: ERROR!! Resource Record %s not pointing to primary %##s", ARDisplayString(m, r), r->resrec.name);
                            r = r->next;
                        }
                    }
                    LogInfo("EtcHostsDeleteOldEntries: Deleting %s", ARDisplayString(m, rr));
                    mDNS_Deregister_internal(m, rr, mDNS_Dereg_normal);
                }
            }
    return mDNSfalse;
}

mDNSlocal void UpdateEtcHosts(mDNS *const m, void *context)
{
    AuthHash *newhosts = (AuthHash *)context;

    mDNS_CheckLock(m);

    //Delete old entries from the core if they are not present in the newhosts
    EtcHostsDeleteOldEntries(m, newhosts, mDNSfalse);
    // Add the new entries to the core if not already present in the core
    EtcHostsAddNewEntries(m, newhosts, mDNSfalse);
}

mDNSlocal void FreeNewHosts(AuthHash *newhosts)
{
    mDNSu32 slot;
    AuthGroup *ag, *agnext;
    AuthRecord *rr, *rrnext;

    for (slot = 0; slot < AUTH_HASH_SLOTS; slot++)
        for (ag = newhosts->rrauth_hash[slot]; ag; ag = agnext)
        {
            agnext = ag->next;
            for (rr = ag->members; rr; rr = rrnext)
            {
                rrnext = rr->next;
                freeL("etchosts", rr);
            }
            freeL("AuthGroups", ag);
        }
}

mDNSlocal void mDNSMacOSXUpdateEtcHosts(mDNS *const m)
{
    AuthHash newhosts;

    // As we will be modifying the core, we can only have one thread running at
    // any point in time.
    KQueueLock(m);

    mDNSPlatformMemZero(&newhosts, sizeof(AuthHash));

    // Get the file desecriptor (will trigger us to start watching for changes)
    int fd = mDNSMacOSXGetEtcHostsFD(m);
    if (fd != -1)
    {
        LogInfo("mDNSMacOSXUpdateEtcHosts: Parsing /etc/hosts fd %d", fd);
        mDNSMacOSXParseEtcHosts(m, fd, &newhosts);
    }
    else LogInfo("mDNSMacOSXUpdateEtcHosts: /etc/hosts is not present");

    // Optimization: Detect whether /etc/hosts changed or not.
    //
    // 1. Check to see if there are any new entries. We do this by seeing whether any entries in
    //    newhosts is already registered with core.  If we find at least one entry that is not
    //    registered with core, then it means we have work to do.
    //
    // 2. Next, we check to see if any of the entries that are registered with core is not present
    //   in newhosts. If we find at least one entry that is not present, it means we have work to
    //   do.
    //
    // Note: We may not have to hold the lock right here as KQueueLock is held which prevents any
    // other thread from running. But mDNS_Lock is needed here as we will be traversing the core
    // data structure in EtcHostsDeleteOldEntries/NewEntries which might expect the lock to be held
    // in the future and this code does not have to change.
    mDNS_Lock(m);
    // Add the new entries to the core if not already present in the core
    if (!EtcHostsAddNewEntries(m, &newhosts, mDNStrue))
    {
        // No new entries to add, check to see if we need to delete any old entries from the
        // core if they are not present in the newhosts
        if (!EtcHostsDeleteOldEntries(m, &newhosts, mDNStrue))
        {
            LogInfo("mDNSMacOSXUpdateEtcHosts: No work");
            FreeNewHosts(&newhosts);
            mDNS_Unlock(m);
            KQueueUnlock(m, "/etc/hosts changed");
            return;
        }
    }

    // This will flush the cache, stop and start the query so that the queries
    // can look at the /etc/hosts again
    //
    // Notes:
    //
    // We can't delete and free the records here. We wait for the mDNSCoreRestartAddressQueries to
    // deliver RMV events. It has to be done in a deferred way because we can't deliver RMV
    // events for local records *before* the RMV events for cache records. mDNSCoreRestartAddressQueries
    // delivers these events in the right order and then calls us back to delete them.
    //
    // Similarly, we do a deferred Registration of the record because mDNSCoreRestartAddressQueries
    // is a common function that looks at all local auth records and delivers a RMV including
    // the records that we might add here. If we deliver a ADD here, it will get a RMV and then when
    // the query is restarted, it will get another ADD. To avoid this (ADD-RMV-ADD), we defer registering
    // the record until the RMVs are delivered in mDNSCoreRestartAddressQueries after which UpdateEtcHosts
    // is called back where we do the Registration of the record. This results in RMV followed by ADD which
    // looks normal.
    mDNSCoreRestartAddressQueries(m, mDNSfalse, FlushAllCacheRecords, UpdateEtcHosts, &newhosts);
    FreeNewHosts(&newhosts);
    mDNS_Unlock(m);
    KQueueUnlock(m, "/etc/hosts changed");
}

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Initialization & Teardown
#endif

CF_EXPORT CFDictionaryRef _CFCopySystemVersionDictionary(void);
CF_EXPORT const CFStringRef _kCFSystemVersionProductNameKey;
CF_EXPORT const CFStringRef _kCFSystemVersionProductVersionKey;
CF_EXPORT const CFStringRef _kCFSystemVersionBuildVersionKey;

// Major version 13 is 10.9.x 
mDNSexport void mDNSMacOSXSystemBuildNumber(char *HINFO_SWstring)
{
    int major = 0, minor = 0;
    char letter = 0, prodname[256]="<Unknown>", prodvers[256]="<Unknown>", buildver[256]="<Unknown>";
    CFDictionaryRef vers = _CFCopySystemVersionDictionary();
    if (vers)
    {
        CFStringRef cfprodname = CFDictionaryGetValue(vers, _kCFSystemVersionProductNameKey);
        CFStringRef cfprodvers = CFDictionaryGetValue(vers, _kCFSystemVersionProductVersionKey);
        CFStringRef cfbuildver = CFDictionaryGetValue(vers, _kCFSystemVersionBuildVersionKey);
        if (cfprodname) 
            CFStringGetCString(cfprodname, prodname, sizeof(prodname), kCFStringEncodingUTF8);
        if (cfprodvers) 
            CFStringGetCString(cfprodvers, prodvers, sizeof(prodvers), kCFStringEncodingUTF8);
        if (cfbuildver && CFStringGetCString(cfbuildver, buildver, sizeof(buildver), kCFStringEncodingUTF8))
            sscanf(buildver, "%d%c%d", &major, &letter, &minor);
        CFRelease(vers);
    }
    if (!major) 
    { 
        major = 13; 
        LogMsg("Note: No Major Build Version number found; assuming 13"); 
    }
    if (HINFO_SWstring) 
        mDNS_snprintf(HINFO_SWstring, 256, "%s %s (%s), %s", prodname, prodvers, buildver, STRINGIFY(mDNSResponderVersion));
    //LogMsg("%s %s (%s), %d %c %d", prodname, prodvers, buildver, major, letter, minor);

    // If product name is "Mac OS X" (or similar) we set OSXVers, else we set iOSVers;
    if ((prodname[0] & 0xDF) == 'M') 
        OSXVers = major;
    else 
        iOSVers = major;
}

// Test to see if we're the first client running on UDP port 5353, by trying to bind to 5353 without using SO_REUSEPORT.
// If we fail, someone else got here first. That's not a big problem; we can share the port for multicast responses --
// we just need to be aware that we shouldn't expect to successfully receive unicast UDP responses.
mDNSlocal mDNSBool mDNSPlatformInit_CanReceiveUnicast(void)
{
    int err = -1;
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 3)
        LogMsg("mDNSPlatformInit_CanReceiveUnicast: socket error %d errno %d (%s)", s, errno, strerror(errno));
    else
    {
        struct sockaddr_in s5353;
        s5353.sin_family      = AF_INET;
        s5353.sin_port        = MulticastDNSPort.NotAnInteger;
        s5353.sin_addr.s_addr = 0;
        err = bind(s, (struct sockaddr *)&s5353, sizeof(s5353));
        close(s);
    }

    if (err) LogMsg("No unicast UDP responses");
    else debugf("Unicast UDP responses okay");
    return(err == 0);
}

mDNSlocal void CreatePTRRecord(mDNS *const m, const domainname *domain)
{
    AuthRecord *rr;
    const domainname *pname = (domainname *)"\x9" "localhost";

    rr= mallocL("localhosts", sizeof(*rr));
    if (rr == NULL) return;
    mDNSPlatformMemZero(rr, sizeof(*rr));

    mDNS_SetupResourceRecord(rr, mDNSNULL, mDNSInterface_LocalOnly, kDNSType_PTR, kHostNameTTL, kDNSRecordTypeKnownUnique, AuthRecordLocalOnly, mDNSNULL, mDNSNULL);
    AssignDomainName(&rr->namestorage, domain);

    rr->resrec.rdlength = DomainNameLength(pname);
    rr->resrec.rdata->u.name.c[0] = 0;
    AssignDomainName(&rr->resrec.rdata->u.name, pname);

    rr->resrec.namehash = DomainNameHashValue(rr->resrec.name);
    SetNewRData(&rr->resrec, mDNSNULL, 0);  // Sets rr->rdatahash for us
    mDNS_Register(m, rr);
}

// Setup PTR records for 127.0.0.1 and ::1. This helps answering them locally rather than relying
// on the external DNS server to answer this. Sometimes, the DNS servers don't respond in a timely
// fashion and applications depending on this e.g., telnetd, times out after 30 seconds creating
// a bad user experience. For now, we specifically create only localhosts to handle radar://9354225
//
// Note: We could have set this up while parsing the entries in /etc/hosts. But this is kept separate
// intentionally to avoid adding to the complexity of code handling /etc/hosts.
mDNSlocal void SetupLocalHostRecords(mDNS *const m)
{
    char buffer[MAX_REVERSE_MAPPING_NAME];
    domainname name;
    int i;
    struct in6_addr addr;
    mDNSu8 *ptr = addr.__u6_addr.__u6_addr8;

    if (inet_pton(AF_INET, "127.0.0.1", &addr) == 1)
    {
        mDNS_snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d.in-addr.arpa.",
                      ptr[3], ptr[2], ptr[1], ptr[0]);
        MakeDomainNameFromDNSNameString(&name, buffer);
        CreatePTRRecord(m, &name);
    }
    else LogMsg("SetupLocalHostRecords: ERROR!! inet_pton AF_INET failed");

    if (inet_pton(AF_INET6, "::1", &addr) == 1)
    {
        for (i = 0; i < 16; i++)
        {
            static const char hexValues[] = "0123456789ABCDEF";
            buffer[i * 4    ] = hexValues[ptr[15 - i] & 0x0F];
            buffer[i * 4 + 1] = '.';
            buffer[i * 4 + 2] = hexValues[ptr[15 - i] >> 4];
            buffer[i * 4 + 3] = '.';
        }
        mDNS_snprintf(&buffer[64], sizeof(buffer)-64, "ip6.arpa.");
        MakeDomainNameFromDNSNameString(&name, buffer);
        CreatePTRRecord(m, &name);
    }
    else LogMsg("SetupLocalHostRecords: ERROR!! inet_pton AF_INET6 failed");
}

// Construction of Default Browse domain list (i.e. when clients pass NULL) is as follows:
// 1) query for b._dns-sd._udp.local on LocalOnly interface
//    (.local manually generated via explicit callback)
// 2) for each search domain (from prefs pane), query for b._dns-sd._udp.<searchdomain>.
// 3) for each result from (2), register LocalOnly PTR record b._dns-sd._udp.local. -> <result>
// 4) result above should generate a callback from question in (1).  result added to global list
// 5) global list delivered to client via GetSearchDomainList()
// 6) client calls to enumerate domains now go over LocalOnly interface
//    (!!!KRS may add outgoing interface in addition)

mDNSlocal mStatus mDNSPlatformInit_setup(mDNS *const m)
{
    mStatus err;

    char HINFO_SWstring[256] = "";
    mDNSMacOSXSystemBuildNumber(HINFO_SWstring);

    err = mDNSHelperInit();
    if (err)
        return err;
    
    // Store mDNSResponder Platform 
    if (OSXVers)
    {
        m->mDNS_plat = platform_OSX;
    }
    else if (iOSVers)
    {
        if (IsAppleTV())
            m->mDNS_plat = platform_Atv;
        else
            m->mDNS_plat = platform_iOS;
    }
    else
    {
        m->mDNS_plat = platform_NonApple; 
    }   
        
    // In 10.4, mDNSResponder is launched very early in the boot process, while other subsystems are still in the process of starting up.
    // If we can't read the user's preferences, then we sleep a bit and try again, for up to five seconds before we give up.
    int i;
    for (i=0; i<100; i++)
    {
        domainlabel testlabel;
        testlabel.c[0] = 0;
        GetUserSpecifiedLocalHostName(&testlabel);
        if (testlabel.c[0]) break;
        usleep(50000);
    }

    m->hostlabel.c[0]        = 0;

    int get_model[2] = { CTL_HW, HW_MODEL };
    size_t len_model = sizeof(HINFO_HWstring_buffer);

    // Normal Apple model names are of the form "iPhone2,1", and
    // internal code names are strings containing no commas, e.g. "N88AP".
    // We used to ignore internal code names, but Apple now uses these internal code names
    // even in released shipping products, so we no longer ignore strings containing no commas.
//	if (sysctl(get_model, 2, HINFO_HWstring_buffer, &len_model, NULL, 0) == 0 && strchr(HINFO_HWstring_buffer, ','))
    if (sysctl(get_model, 2, HINFO_HWstring_buffer, &len_model, NULL, 0) == 0)
        HINFO_HWstring = HINFO_HWstring_buffer;

    // For names of the form "iPhone2,1" we use "iPhone" as the prefix for automatic name generation.
    // For names of the form "N88AP" containg no comma, we use the entire string.
    HINFO_HWstring_prefixlen = strchr(HINFO_HWstring_buffer, ',') ? strcspn(HINFO_HWstring, "0123456789") : strlen(HINFO_HWstring);

    if (mDNSPlatformInit_CanReceiveUnicast()) 
        m->CanReceiveUnicastOn5353 = mDNStrue;

    mDNSu32 hlen = mDNSPlatformStrLen(HINFO_HWstring);
    mDNSu32 slen = mDNSPlatformStrLen(HINFO_SWstring);
    if (hlen + slen < 254)
    {
        m->HIHardware.c[0] = hlen;
        m->HISoftware.c[0] = slen;
        mDNSPlatformMemCopy(&m->HIHardware.c[1], HINFO_HWstring, hlen);
        mDNSPlatformMemCopy(&m->HISoftware.c[1], HINFO_SWstring, slen);
    }

    m->p->permanentsockets.port  = MulticastDNSPort;
    m->p->permanentsockets.m     = m;
    m->p->permanentsockets.sktv4 = -1;
    m->p->permanentsockets.kqsv4.KQcallback = myKQSocketCallBack;
    m->p->permanentsockets.kqsv4.KQcontext  = &m->p->permanentsockets;
    m->p->permanentsockets.kqsv4.KQtask     = "IPv4 UDP packet reception";
    m->p->permanentsockets.sktv6 = -1;
    m->p->permanentsockets.kqsv6.KQcallback = myKQSocketCallBack;
    m->p->permanentsockets.kqsv6.KQcontext  = &m->p->permanentsockets;
    m->p->permanentsockets.kqsv6.KQtask     = "IPv6 UDP packet reception";

    err = SetupSocket(&m->p->permanentsockets, MulticastDNSPort, AF_INET, mDNSNULL);
    if (err) LogMsg("mDNSPlatformInit_setup: SetupSocket(AF_INET) failed error %d errno %d (%s)", err, errno, strerror(errno));
    err = SetupSocket(&m->p->permanentsockets, MulticastDNSPort, AF_INET6, mDNSNULL);
    if (err) LogMsg("mDNSPlatformInit_setup: SetupSocket(AF_INET6) failed error %d errno %d (%s)", err, errno, strerror(errno));

    struct sockaddr_in s4;
    socklen_t n4 = sizeof(s4);
    if (getsockname(m->p->permanentsockets.sktv4, (struct sockaddr *)&s4, &n4) < 0) 
        LogMsg("getsockname v4 error %d (%s)", errno, strerror(errno));
    else 
        m->UnicastPort4.NotAnInteger = s4.sin_port;

    if (m->p->permanentsockets.sktv6 >= 0)
    {
        struct sockaddr_in6 s6;
        socklen_t n6 = sizeof(s6);
        if (getsockname(m->p->permanentsockets.sktv6, (struct sockaddr *)&s6, &n6) < 0) LogMsg("getsockname v6 error %d (%s)", errno, strerror(errno));
        else m->UnicastPort6.NotAnInteger = s6.sin6_port;
    }

    m->p->InterfaceList      = mDNSNULL;
    m->p->userhostlabel.c[0] = 0;
    m->p->usernicelabel.c[0] = 0;
    m->p->prevoldnicelabel.c[0] = 0;
    m->p->prevnewnicelabel.c[0] = 0;
    m->p->prevoldhostlabel.c[0] = 0;
    m->p->prevnewhostlabel.c[0] = 0;
    m->p->NotifyUser         = 0;
    m->p->KeyChainTimer      = 0;
    m->p->WakeAtUTC          = 0;
    m->p->RequestReSleep     = 0;
    // Assume that everything is good to begin with. If something is not working,
    // we will detect that when we start sending questions.
    m->p->v4answers          = 1;
    m->p->v6answers          = 1;
    m->p->DNSTrigger         = 0;
    m->p->LastConfigGeneration = 0;

#if APPLE_OSX_mDNSResponder
    uuid_generate(m->asl_uuid);
#endif

    m->AutoTunnelRelayAddr = zerov6Addr;

    NetworkChangedKey_IPv4         = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4);
    NetworkChangedKey_IPv6         = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv6);
    NetworkChangedKey_Hostnames    = SCDynamicStoreKeyCreateHostNames(NULL);
    NetworkChangedKey_Computername = SCDynamicStoreKeyCreateComputerName(NULL);
    NetworkChangedKey_DNS          = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetDNS);
    NetworkChangedKey_StateInterfacePrefix = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL, kSCDynamicStoreDomainState, CFSTR(""), NULL);
    if (!NetworkChangedKey_IPv4 || !NetworkChangedKey_IPv6 || !NetworkChangedKey_Hostnames || !NetworkChangedKey_Computername || !NetworkChangedKey_DNS || !NetworkChangedKey_StateInterfacePrefix)
    { LogMsg("SCDynamicStore string setup failed"); return(mStatus_NoMemoryErr); }

    err = WatchForNetworkChanges(m);
    if (err) { LogMsg("mDNSPlatformInit_setup: WatchForNetworkChanges failed %d", err); return(err); }

    err = WatchForSysEvents(m);
    if (err) { LogMsg("mDNSPlatformInit_setup: WatchForSysEvents failed %d", err); return(err); }

    mDNSs32 utc = mDNSPlatformUTC();
    m->SystemWakeOnLANEnabled = SystemWakeForNetworkAccess();
    myGetIfAddrs(1);
    UpdateInterfaceList(m, utc);
    SetupActiveInterfaces(m, utc);

    // Explicitly ensure that our Keychain operations utilize the system domain.
#ifndef NO_SECURITYFRAMEWORK
    SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
#endif

    mDNS_Lock(m);
    SetDomainSecrets(m);
    SetLocalDomains();
    mDNS_Unlock(m);

#ifndef NO_SECURITYFRAMEWORK
    err = SecKeychainAddCallback(KeychainChanged, kSecAddEventMask|kSecDeleteEventMask|kSecUpdateEventMask, m);
    if (err) { LogMsg("mDNSPlatformInit_setup: SecKeychainAddCallback failed %d", err); return(err); }
#endif

#if !defined(kIOPMAcknowledgmentOptionSystemCapabilityRequirements) || TARGET_OS_EMBEDDED
    LogMsg("Note: Compiled without SnowLeopard Fine-Grained Power Management support");
#else
    IOPMConnection c;
    IOReturn iopmerr = IOPMConnectionCreate(CFSTR("mDNSResponder"), kIOPMSystemPowerStateCapabilityCPU, &c);
    if (iopmerr) LogMsg("IOPMConnectionCreate failed %d", iopmerr);
    else
    {
        iopmerr = IOPMConnectionSetNotification(c, m, SnowLeopardPowerChanged);
        if (iopmerr) LogMsg("IOPMConnectionSetNotification failed %d", iopmerr);
        else
        {
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
            IOPMConnectionSetDispatchQueue(c, dispatch_get_main_queue());
            LogInfo("IOPMConnectionSetDispatchQueue is now running");
#else
            iopmerr = IOPMConnectionScheduleWithRunLoop(c, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
            if (iopmerr) LogMsg("IOPMConnectionScheduleWithRunLoop failed %d", iopmerr);
            LogInfo("IOPMConnectionScheduleWithRunLoop is now running");
#endif /* MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM */
        }
    }
    m->p->IOPMConnection = iopmerr ? mDNSNULL : c;
    if (iopmerr) // If IOPMConnectionCreate unavailable or failed, proceed with old-style power notification code below
#endif // kIOPMAcknowledgmentOptionSystemCapabilityRequirements
    {
        m->p->PowerConnection = IORegisterForSystemPower(m, &m->p->PowerPortRef, PowerChanged, &m->p->PowerNotifier);
        if (!m->p->PowerConnection) { LogMsg("mDNSPlatformInit_setup: IORegisterForSystemPower failed"); return(-1); }
        else
        {
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
            IONotificationPortSetDispatchQueue(m->p->PowerPortRef, dispatch_get_main_queue());
#else
            CFRunLoopAddSource(CFRunLoopGetMain(), IONotificationPortGetRunLoopSource(m->p->PowerPortRef), kCFRunLoopDefaultMode);
#endif /* MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM */
        }
    }

#if APPLE_OSX_mDNSResponder
    // Note: We use SPMetricPortability > 35 to indicate a laptop of some kind
    // SPMetricPortability <= 35 means nominally a non-portable machine (i.e. Mac mini or better)
    // Apple TVs, AirPort base stations, and Time Capsules do not actually weigh 3kg, but we assign them
    // higher 'nominal' masses to indicate they should be treated as being relatively less portable than a laptop
    if      (!strncasecmp(HINFO_HWstring, "Xserve",       6)) { SPMetricPortability = 25 /* 30kg */; SPMetricMarginalPower = 84 /* 250W */; SPMetricTotalPower = 85 /* 300W */; }
    else if (!strncasecmp(HINFO_HWstring, "RackMac",      7)) { SPMetricPortability = 25 /* 30kg */; SPMetricMarginalPower = 84 /* 250W */; SPMetricTotalPower = 85 /* 300W */; }
    else if (!strncasecmp(HINFO_HWstring, "MacPro",       6)) { SPMetricPortability = 27 /* 20kg */; SPMetricMarginalPower = 84 /* 250W */; SPMetricTotalPower = 85 /* 300W */; }
    else if (!strncasecmp(HINFO_HWstring, "PowerMac",     8)) { SPMetricPortability = 27 /* 20kg */; SPMetricMarginalPower = 82 /* 160W */; SPMetricTotalPower = 83 /* 200W */; }
    else if (!strncasecmp(HINFO_HWstring, "iMac",         4)) { SPMetricPortability = 30 /* 10kg */; SPMetricMarginalPower = 77 /*  50W */; SPMetricTotalPower = 78 /*  60W */; }
    else if (!strncasecmp(HINFO_HWstring, "Macmini",      7)) { SPMetricPortability = 33 /*  5kg */; SPMetricMarginalPower = 73 /*  20W */; SPMetricTotalPower = 74 /*  25W */; }
    else if (!strncasecmp(HINFO_HWstring, "TimeCapsule", 11)) { SPMetricPortability = 34 /*  4kg */; SPMetricMarginalPower = 10 /*  ~0W */; SPMetricTotalPower = 70 /*  13W */; }
    else if (!strncasecmp(HINFO_HWstring, "AirPort",      7)) { SPMetricPortability = 35 /*  3kg */; SPMetricMarginalPower = 10 /*  ~0W */; SPMetricTotalPower = 70 /*  12W */; }
    else if (  IsAppleTV()  )                                 { SPMetricPortability = 35 /*  3kg */; SPMetricMarginalPower = 60 /*   1W */; SPMetricTotalPower = 63 /*   2W */; }
    else if (!strncasecmp(HINFO_HWstring, "MacBook",      7)) { SPMetricPortability = 37 /*  2kg */; SPMetricMarginalPower = 71 /*  13W */; SPMetricTotalPower = 72 /*  15W */; }
    else if (!strncasecmp(HINFO_HWstring, "PowerBook",    9)) { SPMetricPortability = 37 /*  2kg */; SPMetricMarginalPower = 71 /*  13W */; SPMetricTotalPower = 72 /*  15W */; }
    LogSPS("HW_MODEL: %.*s (%s) Portability %d Marginal Power %d Total Power %d Features %d",
           HINFO_HWstring_prefixlen, HINFO_HWstring, HINFO_HWstring, SPMetricPortability, SPMetricMarginalPower, SPMetricTotalPower, SPMetricFeatures);
#endif // APPLE_OSX_mDNSResponder

    // Currently this is not defined. SSL code will eventually fix this. If it becomes
    // critical, we will define this to workaround the bug in SSL.
#ifdef __SSL_NEEDS_SERIALIZATION__
    SSLqueue = dispatch_queue_create("com.apple.mDNSResponder.SSLQueue", NULL);
#else
    SSLqueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
#endif
    if (SSLqueue == mDNSNULL) LogMsg("dispatch_queue_create: SSL queue NULL");

    mDNSMacOSXUpdateEtcHosts(m);
    SetupLocalHostRecords(m);

    return(mStatus_NoError);
}

mDNSexport mStatus mDNSPlatformInit(mDNS *const m)
{
#if MDNS_NO_DNSINFO
    LogMsg("Note: Compiled without Apple-specific Split-DNS support");
#endif

    // Adding interfaces will use this flag, so set it now.
    m->DivertMulticastAdvertisements = !m->AdvertiseLocalAddresses;

#if APPLE_OSX_mDNSResponder
    m->SPSBrowseCallback = UpdateSPSStatus;
#endif // APPLE_OSX_mDNSResponder

    mStatus result = mDNSPlatformInit_setup(m);

    // We don't do asynchronous initialization on OS X, so by the time we get here the setup will already
    // have succeeded or failed -- so if it succeeded, we should just call mDNSCoreInitComplete() immediately
    if (result == mStatus_NoError)
    {
        mDNSCoreInitComplete(m, mStatus_NoError);

#if !NO_D2D
        // We only initialize if mDNSCore successfully initialized.
        if (D2DInitialize)
        {
            D2DStatus ds = D2DInitialize(CFRunLoopGetMain(), xD2DServiceCallback, m) ;
            if (ds != kD2DSuccess)
                LogMsg("D2DInitialiize failed: %d", ds);
            else
                LogMsg("D2DInitialize succeeded");
        }
#endif // ! NO_D2D

    }
    result = DNSSECCryptoInit(m);
    return(result);
}

mDNSexport void mDNSPlatformClose(mDNS *const m)
{
    if (m->p->PowerConnection)
    {
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
        IONotificationPortSetDispatchQueue(m->p->PowerPortRef, NULL);
#else
        CFRunLoopRemoveSource(CFRunLoopGetMain(), IONotificationPortGetRunLoopSource(m->p->PowerPortRef), kCFRunLoopDefaultMode);
#endif
        // According to <http://developer.apple.com/qa/qa2004/qa1340.html>, a single call
        // to IORegisterForSystemPower creates *three* objects that need to be disposed individually:
        IODeregisterForSystemPower(&m->p->PowerNotifier);
        IOServiceClose            ( m->p->PowerConnection);
        IONotificationPortDestroy ( m->p->PowerPortRef);
        m->p->PowerConnection = 0;
    }

    if (m->p->Store)
    {
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
        if (!SCDynamicStoreSetDispatchQueue(m->p->Store, NULL))
            LogMsg("mDNSPlatformClose: SCDynamicStoreSetDispatchQueue failed");
#else
        CFRunLoopRemoveSource(CFRunLoopGetMain(), m->p->StoreRLS, kCFRunLoopDefaultMode);
        CFRunLoopSourceInvalidate(m->p->StoreRLS);
        CFRelease(m->p->StoreRLS);
        m->p->StoreRLS = NULL;
#endif
        CFRelease(m->p->Store);
        m->p->Store    = NULL;
    }

    if (m->p->PMRLS)
    {
        CFRunLoopRemoveSource(CFRunLoopGetMain(), m->p->PMRLS, kCFRunLoopDefaultMode);
        CFRunLoopSourceInvalidate(m->p->PMRLS);
        CFRelease(m->p->PMRLS);
        m->p->PMRLS = NULL;
    }

    if (m->p->SysEventNotifier >= 0) { close(m->p->SysEventNotifier); m->p->SysEventNotifier = -1; }

#if !NO_D2D
    if (D2DTerminate)
    {
        D2DStatus ds = D2DTerminate();
        if (ds != kD2DSuccess)
            LogMsg("D2DTerminate failed: %d", ds);
        else
            LogMsg("D2DTerminate succeeded");
    }
#endif // ! NO_D2D

    mDNSs32 utc = mDNSPlatformUTC();
    MarkAllInterfacesInactive(m, utc);
    ClearInactiveInterfaces(m, utc);
    CloseSocketSet(&m->p->permanentsockets);

#if APPLE_OSX_mDNSResponder
    // clean up tunnels
    while (m->TunnelClients)
    {
        ClientTunnel *cur = m->TunnelClients;
        LogInfo("mDNSPlatformClose: removing client tunnel %p %##s from list", cur, cur->dstname.c);
        if (cur->q.ThisQInterval >= 0) mDNS_StopQuery(m, &cur->q);
        AutoTunnelSetKeys(cur, mDNSfalse);
        m->TunnelClients = cur->next;
        freeL("ClientTunnel", cur);
    }

    if (AnonymousRacoonConfig)
    {
        AnonymousRacoonConfig = mDNSNULL;
        LogInfo("mDNSPlatformClose: Deconfiguring autotunnel need not be done in mDNSResponder");
    }
#endif // APPLE_OSX_mDNSResponder
}

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - General Platform Support Layer functions
#endif

mDNSexport mDNSu32 mDNSPlatformRandomNumber(void)
{
    return(arc4random());
}

mDNSexport mDNSs32 mDNSPlatformOneSecond = 1000;
mDNSexport mDNSu32 mDNSPlatformClockDivisor = 0;

mDNSexport mStatus mDNSPlatformTimeInit(void)
{
    // Notes: Typical values for mach_timebase_info:
    // tbi.numer = 1000 million
    // tbi.denom =   33 million
    // These are set such that (mach_absolute_time() * numer/denom) gives us nanoseconds;
    //          numer  / denom = nanoseconds per hardware clock tick (e.g. 30);
    //          denom  / numer = hardware clock ticks per nanosecond (e.g. 0.033)
    // (denom*1000000) / numer = hardware clock ticks per millisecond (e.g. 33333)
    // So: mach_absolute_time() / ((denom*1000000)/numer) = milliseconds
    //
    // Arithmetic notes:
    // tbi.denom is at least 1, and not more than 2^32-1.
    // Therefore (tbi.denom * 1000000) is at least one million, but cannot overflow a uint64_t.
    // tbi.denom is at least 1, and not more than 2^32-1.
    // Therefore clockdivisor should end up being a number roughly in the range 10^3 - 10^9.
    // If clockdivisor is less than 10^3 then that means that the native clock frequency is less than 1MHz,
    // which is unlikely on any current or future Macintosh.
    // If clockdivisor is greater than 10^9 then that means the native clock frequency is greater than 1000GHz.
    // When we ship Macs with clock frequencies above 1000GHz, we may have to update this code.
    struct mach_timebase_info tbi;
    kern_return_t result = mach_timebase_info(&tbi);
    if (result == KERN_SUCCESS) mDNSPlatformClockDivisor = ((uint64_t)tbi.denom * 1000000) / tbi.numer;
    return(result);
}

mDNSexport mDNSs32 mDNSPlatformRawTime(void)
{
    if (mDNSPlatformClockDivisor == 0) { LogMsg("mDNSPlatformRawTime called before mDNSPlatformTimeInit"); return(0); }

    static uint64_t last_mach_absolute_time = 0;
    //static uint64_t last_mach_absolute_time = 0x8000000000000000LL;	// Use this value for testing the alert display
    uint64_t this_mach_absolute_time = mach_absolute_time();
    if ((int64_t)this_mach_absolute_time - (int64_t)last_mach_absolute_time < 0)
    {
        LogMsg("mDNSPlatformRawTime: last_mach_absolute_time %08X%08X", last_mach_absolute_time);
        LogMsg("mDNSPlatformRawTime: this_mach_absolute_time %08X%08X", this_mach_absolute_time);
        // Update last_mach_absolute_time *before* calling NotifyOfElusiveBug()
        last_mach_absolute_time = this_mach_absolute_time;
        // Note: This bug happens all the time on 10.3
        NotifyOfElusiveBug("mach_absolute_time went backwards!",
                           "This error occurs from time to time, often on newly released hardware, "
                           "and usually the exact cause is different in each instance.\r\r"
                           "Please file a new Radar bug report with the title “mach_absolute_time went backwards” "
                           "and assign it to Radar Component “Kernel” Version “X”.");
    }
    last_mach_absolute_time = this_mach_absolute_time;

    return((mDNSs32)(this_mach_absolute_time / mDNSPlatformClockDivisor));
}

mDNSexport mDNSs32 mDNSPlatformUTC(void)
{
    return time(NULL);
}

// Locking is a no-op here, because we're single-threaded with a CFRunLoop, so we can never interrupt ourselves
mDNSexport void     mDNSPlatformLock   (const mDNS *const m) { (void)m; }
mDNSexport void     mDNSPlatformUnlock (const mDNS *const m) { (void)m; }
mDNSexport void     mDNSPlatformStrCopy(      void *dst, const void *src)              { strcpy((char *)dst, (const char *)src); }
mDNSexport mDNSu32  mDNSPlatformStrLCopy(     void *dst, const void *src, mDNSu32 dstlen) { return (strlcpy((char *)dst, (const char *)src, dstlen)); }
mDNSexport mDNSu32  mDNSPlatformStrLen (                 const void *src)              { return(strlen((const char*)src)); }
mDNSexport void     mDNSPlatformMemCopy(      void *dst, const void *src, mDNSu32 len) { memcpy(dst, src, len); }
mDNSexport mDNSBool mDNSPlatformMemSame(const void *dst, const void *src, mDNSu32 len) { return(memcmp(dst, src, len) == 0); }
mDNSexport int      mDNSPlatformMemCmp(const void *dst, const void *src, mDNSu32 len) { return(memcmp(dst, src, len)); }
mDNSexport void     mDNSPlatformMemZero(      void *dst,                  mDNSu32 len) { memset(dst, 0, len); }
mDNSexport void     mDNSPlatformQsort  (      void *base, int nel, int width, int (*compar)(const void *, const void *))
{
    return (qsort(base, nel, width, compar));
}
#if !(APPLE_OSX_mDNSResponder && MACOSX_MDNS_MALLOC_DEBUGGING)
mDNSexport void *   mDNSPlatformMemAllocate(mDNSu32 len) { return(mallocL("mDNSPlatformMemAllocate", len)); }
#endif
mDNSexport void     mDNSPlatformMemFree    (void *mem)   { freeL("mDNSPlatformMemFree", mem); }

mDNSexport void mDNSPlatformSetAllowSleep(mDNS *const m, mDNSBool allowSleep, const char *reason)
{
    if (allowSleep && m->p->IOPMAssertion)
    {
        LogInfo("%s Destroying NoIdleSleep power assertion", __FUNCTION__);
        IOPMAssertionRelease(m->p->IOPMAssertion);
        m->p->IOPMAssertion = 0;
    }
    else if (!allowSleep)
    {
#ifdef kIOPMAssertionTypeNoIdleSleep
        if (m->p->IOPMAssertion)
        {
            IOPMAssertionRelease(m->p->IOPMAssertion);
            m->p->IOPMAssertion = 0;
        }

        CFStringRef assertionName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s.%d %s"), getprogname(), getpid(), reason ? reason : "");
        IOPMAssertionCreateWithName(kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, assertionName ? assertionName : CFSTR("mDNSResponder"), &m->p->IOPMAssertion);
        if (assertionName) CFRelease(assertionName);
        LogInfo("%s Creating NoIdleSleep power assertion", __FUNCTION__);
#endif
    }
}

mDNSexport void mDNSPlatformPreventSleep(mDNS *const m, mDNSu32 timeout, const char *reason)
{
	if (m->p->IOPMAssertion)
	{
		LogSPS("Sleep Assertion is already being held. Will not attempt to get it again for %d seconds for %s", timeout, reason);
        return;
	}
#ifdef kIOPMAssertionTypeNoIdleSleep

#if TARGET_OS_EMBEDDED
    if (!IsAppleTV())
        return; // No need for maintenance wakes on non-AppleTV embedded devices.
#endif

	double timeoutVal = (double)timeout;
    CFStringRef str = CFStringCreateWithCString(NULL, reason, kCFStringEncodingUTF8);
	CFNumberRef Timeout_num = CFNumberCreate(NULL, kCFNumberDoubleType, &timeoutVal);
	CFMutableDictionaryRef assertionProperties = CFDictionaryCreateMutable(NULL, 0,
																		   &kCFTypeDictionaryKeyCallBacks,
																		   &kCFTypeDictionaryValueCallBacks);
    if (IsAppleTV())
        CFDictionarySetValue(assertionProperties, kIOPMAssertionTypeKey, kIOPMAssertPreventUserIdleSystemSleep);
    else
        CFDictionarySetValue(assertionProperties, kIOPMAssertionTypeKey, kIOPMAssertMaintenanceActivity);

    CFDictionarySetValue(assertionProperties, kIOPMAssertionTimeoutKey, Timeout_num);
    CFDictionarySetValue(assertionProperties, kIOPMAssertionNameKey,    str);

    IOPMAssertionCreateWithProperties(assertionProperties, (IOPMAssertionID *)&m->p->IOPMAssertion);
    CFRelease(str);
    CFRelease(Timeout_num);
    CFRelease(assertionProperties);
    LogSPS("Got an idle sleep assertion for %d seconds for %s", timeout, reason);
#endif
}

mDNSexport void mDNSPlatformSendWakeupPacket(mDNS *const m, mDNSInterfaceID InterfaceID, char *EthAddr, char *IPAddr, int iteration)
{
    mDNSu32 ifindex;

    // Sanity check
    ifindex = mDNSPlatformInterfaceIndexfromInterfaceID(m, InterfaceID, mDNStrue);
    if (ifindex <= 0)
    {
        LogMsg("mDNSPlatformSendWakeupPacket: ERROR!! Invalid InterfaceID %u", ifindex);
        return;
    }
    mDNSSendWakeupPacket(ifindex, EthAddr, IPAddr, iteration);
}

mDNSexport mDNSBool mDNSPlatformInterfaceIsD2D(mDNSInterfaceID InterfaceID)
{
    NetworkInterfaceInfoOSX *info;

    if (InterfaceID == mDNSInterface_P2P)
        return mDNStrue;

    // mDNSInterface_BLE not considered a D2D interface for the purpose of this
    // routine, since it's not implemented via a D2D plugin.
    if (InterfaceID == mDNSInterface_BLE)
        return mDNSfalse;

    if (   (InterfaceID == mDNSInterface_Any) 
        || (InterfaceID == mDNSInterfaceMark)
        || (InterfaceID == mDNSInterface_LocalOnly)
        || (InterfaceID == mDNSInterface_Unicast))
        return mDNSfalse;

    // Compare to cached AWDL interface ID.
    if (AWDLInterfaceID && (InterfaceID == AWDLInterfaceID))
        return mDNStrue;

    info = IfindexToInterfaceInfoOSX(&mDNSStorage, InterfaceID);
    if (info == NULL)
    {
        // this log message can print when operations are stopped on an interface that has gone away
        LogInfo("mDNSPlatformInterfaceIsD2D: Invalid interface index %d", InterfaceID);
        return mDNSfalse;
    }

    return (mDNSBool) info->D2DInterface;
}

// Filter records send over P2P (D2D) type interfaces
// Note that the terms P2P and D2D are used synonymously in the current code and comments.
mDNSexport mDNSBool mDNSPlatformValidRecordForInterface(const AuthRecord *rr, mDNSInterfaceID InterfaceID)
{
    // For an explicit match to a valid interface ID, return true. 
    if (rr->resrec.InterfaceID == InterfaceID)
        return mDNStrue;

    // Only filtering records for D2D type interfaces, return true for all other interface types.
    if (!mDNSPlatformInterfaceIsD2D(InterfaceID))
        return mDNStrue;
    
    // If it's an AWDL interface the record must be explicitly marked to include AWDL.
    if (InterfaceID == AWDLInterfaceID)
    {
        if (rr->ARType == AuthRecordAnyIncludeAWDL || rr->ARType == AuthRecordAnyIncludeAWDLandP2P)
            return mDNStrue;
        else
            return mDNSfalse;
    }
    
    // Send record if it is explicitly marked to include all other P2P type interfaces.
    if (rr->ARType == AuthRecordAnyIncludeP2P || rr->ARType == AuthRecordAnyIncludeAWDLandP2P)
        return mDNStrue;

    // Don't send the record over this interface.
    return mDNSfalse;
}

// Filter questions send over P2P (D2D) type interfaces.
mDNSexport mDNSBool mDNSPlatformValidQuestionForInterface(DNSQuestion *q, const NetworkInterfaceInfo *intf)
{
    // For an explicit match to a valid interface ID, return true. 
    if (q->InterfaceID == intf->InterfaceID)
        return mDNStrue;

    // Only filtering questions for D2D type interfaces
    if (!mDNSPlatformInterfaceIsD2D(intf->InterfaceID))
        return mDNStrue;

    // If it's an AWDL interface the question must be explicitly marked to include AWDL.
    if (intf->InterfaceID == AWDLInterfaceID)
    {
        if (q->flags & kDNSServiceFlagsIncludeAWDL)
            return mDNStrue;
        else
            return mDNSfalse;
    }
    
    // Sent question if it is explicitly marked to include all other P2P type interfaces.
    if (q->flags & kDNSServiceFlagsIncludeP2P)
        return mDNStrue;

    // Don't send the question over this interface.
    return mDNSfalse;
}

// Returns true unless record was received over the AWDL interface and
// the question was not specific to the AWDL interface or did not specify kDNSServiceInterfaceIndexAny
// with the kDNSServiceFlagsIncludeAWDL flag set.
mDNSexport mDNSBool   mDNSPlatformValidRecordForQuestion(const ResourceRecord *const rr, const DNSQuestion *const q)
{
    if (!rr->InterfaceID || (rr->InterfaceID == q->InterfaceID))
        return mDNStrue;

    if ((rr->InterfaceID == AWDLInterfaceID) && !(q->flags & kDNSServiceFlagsIncludeAWDL))
        return mDNSfalse;

    return mDNStrue;
}

// formating time to RFC 4034 format
mDNSexport void mDNSPlatformFormatTime(unsigned long te, mDNSu8 *buf, int bufsize)
{
    struct tm tmTime;
    time_t t = (time_t)te;
    // Time since epoch : strftime takes "tm". Convert seconds to "tm" using
    // gmtime_r first and then use strftime
    gmtime_r(&t, &tmTime);
    strftime((char *)buf, bufsize, "%Y%m%d%H%M%S", &tmTime);
}

mDNSexport mDNSs32 mDNSPlatformGetPID()
{
    return getpid();
}

// Schedule a function asynchronously on the main queue
mDNSexport void mDNSPlatformDispatchAsync(mDNS *const m, void *context, AsyncDispatchFunc func)
{
    // KQueueLock/Unlock is used for two purposes
    //
    // 1. We can't be running along with the KQueue thread and hence acquiring the lock
    //    serializes the access to the "core"
    //
    // 2. KQueueUnlock also sends a message wake up the KQueue thread which in turn wakes
    //    up and calls udsserver_idle which schedules the messages across the uds socket.
    //    If "func" delivers something to the uds socket from the dispatch thread, it will
    //    not be delivered immediately if not for the Unlock.
    dispatch_async(dispatch_get_main_queue(), ^{
        KQueueLock(m);
        func(m, context);
        KQueueUnlock(m, "mDNSPlatformDispatchAsync");
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
        // KQueueUnlock is a noop. Hence, we need to run kick off the idle loop
        // to handle any message that "func" might deliver.
        TriggerEventCompletion();
#endif
    });
}

// definitions for device-info record construction
#define DEVINFO_MODEL       "model="
#define DEVINFO_MODEL_LEN   sizeof_string(DEVINFO_MODEL)

#define OSX_VER         "osxvers="
#define OSX_VER_LEN     sizeof_string(OSX_VER) 
#define VER_NUM_LEN     2  // 2 digits of version number added to base string

#define MODEL_COLOR           "ecolor="
#define MODEL_COLOR_LEN       sizeof_string(MODEL_COLOR)
#define MODEL_RGB_VALUE_LEN   sizeof_string("255,255,255") // 'r,g,b'

// Bytes available in TXT record for model name after subtracting space for other
// fixed size strings and their length bytes.
#define MAX_MODEL_NAME_LEN   (256 - (DEVINFO_MODEL_LEN + 1) - (OSX_VER_LEN + VER_NUM_LEN + 1) - (MODEL_COLOR_LEN + MODEL_RGB_VALUE_LEN + 1))

mDNSlocal mDNSu8 getModelIconColors(char *color)
{
	mDNSPlatformMemZero(color, MODEL_RGB_VALUE_LEN + 1);

#if !TARGET_OS_EMBEDDED && defined(kIOPlatformDeviceEnclosureColorKey)
	mDNSu8   red      = 0;
	mDNSu8   green    = 0;
	mDNSu8   blue     = 0;

	IOReturn rGetDeviceColor = IOPlatformGetDeviceColor(kIOPlatformDeviceEnclosureColorKey,
														&red, &green, &blue);
	if (kIOReturnSuccess == rGetDeviceColor)
	{
		// IOKit was able to get enclosure color for the current device.
		return snprintf(color, MODEL_RGB_VALUE_LEN + 1, "%d,%d,%d", red, green, blue);
	}
#endif // !TARGET_OS_EMBEDDED && defined(kIOPlatformDeviceEnclosureColorKey)

	return 0;
}


// Initialize device-info TXT record contents and return total length of record data.
mDNSexport mDNSu32 initializeDeviceInfoTXT(mDNS *m, mDNSu8 *ptr)
{
    mDNSu8 *bufferStart = ptr;
    mDNSu8 len = m->HIHardware.c[0] < MAX_MODEL_NAME_LEN ? m->HIHardware.c[0] : MAX_MODEL_NAME_LEN;

    *ptr = DEVINFO_MODEL_LEN + len; // total length of DEVINFO_MODEL string plus the hardware name string
    ptr++;
    mDNSPlatformMemCopy(ptr, DEVINFO_MODEL, DEVINFO_MODEL_LEN);
    ptr += DEVINFO_MODEL_LEN;
    mDNSPlatformMemCopy(ptr, m->HIHardware.c + 1, len);
    ptr += len;

    // only include this string for OSX
    if (OSXVers)
    {
        char    ver_num[VER_NUM_LEN + 1]; // version digits + null written by snprintf
        *ptr = OSX_VER_LEN + VER_NUM_LEN; // length byte
        ptr++;
        mDNSPlatformMemCopy(ptr, OSX_VER, OSX_VER_LEN);
        ptr += OSX_VER_LEN;
        // convert version number to ASCII, add 1 for terminating null byte written by snprintf()
        // WARNING: This code assumes that OSXVers is always exactly two digits
        snprintf(ver_num, VER_NUM_LEN + 1, "%d", OSXVers);
        mDNSPlatformMemCopy(ptr, ver_num, VER_NUM_LEN);
        ptr += VER_NUM_LEN;

		char rgb[MODEL_RGB_VALUE_LEN + 1]; // RGB value + null written by snprintf
		len = getModelIconColors(rgb);
		if (len)
		{
			*ptr = MODEL_COLOR_LEN + len; // length byte
			ptr++;

			mDNSPlatformMemCopy(ptr, MODEL_COLOR, MODEL_COLOR_LEN);
			ptr += MODEL_COLOR_LEN;

			mDNSPlatformMemCopy(ptr, rgb, len);
			ptr += len;
		}
    }

    return (ptr - bufferStart);
}



