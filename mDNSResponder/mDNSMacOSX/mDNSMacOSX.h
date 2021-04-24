/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2002-2015 Apple Inc. All rights reserved.
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

#ifndef __mDNSMacOSX_h
#define __mDNSMacOSX_h

#ifdef  __cplusplus
extern "C" {
#endif

#include <SystemConfiguration/SystemConfiguration.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "mDNSEmbeddedAPI.h"  // for domain name structure

#include <net/if.h>
#include <os/log.h>

//#define MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
#include <dispatch/dispatch.h>
#include <dispatch/private.h>
#endif

#if TARGET_OS_EMBEDDED
#define NO_SECURITYFRAMEWORK 1
#define NO_CFUSERNOTIFICATION 1
#include <MobileGestalt.h> // for IsAppleTV()
#endif

#ifndef NO_SECURITYFRAMEWORK
#include <Security/SecureTransport.h>
#include <Security/Security.h>
#endif /* NO_SECURITYFRAMEWORK */

enum mDNSDynamicStoreSetConfigKey
{
    kmDNSMulticastConfig = 1,
    kmDNSDynamicConfig,
    kmDNSPrivateConfig,
    kmDNSBackToMyMacConfig,
    kmDNSSleepProxyServersState,
    kmDNSDebugState, 
};

typedef struct NetworkInterfaceInfoOSX_struct NetworkInterfaceInfoOSX;

typedef void (*KQueueEventCallback)(int fd, short filter, void *context);
typedef struct
{
    KQueueEventCallback KQcallback;
    void                *KQcontext;
    const char          *KQtask;        // For debugging messages
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
    dispatch_source_t readSource;
    dispatch_source_t writeSource;
    mDNSBool fdClosed;
#endif
} KQueueEntry;

typedef struct
{
    mDNSIPPort port; // MUST BE FIRST FIELD -- UDPSocket_struct begins with a KQSocketSet,
    // and mDNSCore requires every UDPSocket_struct to begin with a mDNSIPPort port
    mDNS                    *m;
    int sktv4;
    KQueueEntry kqsv4;
    int sktv6;
    KQueueEntry kqsv6;
    int                     *closeFlag;
    mDNSBool proxy;
} KQSocketSet;

struct UDPSocket_struct
{
    KQSocketSet ss;     // First field of KQSocketSet has to be mDNSIPPort -- mDNSCore requires every UDPSocket_struct to begin with mDNSIPPort port
};

// TCP socket support

typedef enum
{
    handshake_required,
    handshake_in_progress,
    handshake_completed,
    handshake_to_be_closed
} handshakeStatus;

struct TCPSocket_struct
{
    TCPSocketFlags flags;       // MUST BE FIRST FIELD -- mDNSCore expects every TCPSocket_struct to begin with TCPSocketFlags flags
    TCPConnectionCallback callback;
    int fd;
    KQueueEntry *kqEntry;
    KQSocketSet ss;
#ifndef NO_SECURITYFRAMEWORK
    SSLContextRef tlsContext;
    pthread_t handshake_thread;
#endif /* NO_SECURITYFRAMEWORK */
    domainname hostname;
    void *context;
    mDNSBool setup;
    mDNSBool connected;
    handshakeStatus handshake;
    mDNS *m; // So we can call KQueueLock from the SSLHandshake thread
    mStatus err;
};

struct NetworkInterfaceInfoOSX_struct
{
    NetworkInterfaceInfo ifinfo;                // MUST be the first element in this structure
    NetworkInterfaceInfoOSX *next;
    mDNS                    *m;
    mDNSu8 Exists;                              // 1 = currently exists in getifaddrs list; 0 = doesn't
                                                // 2 = exists, but McastTxRx state changed
    mDNSu8 Flashing;                            // Set if interface appeared for less than 60 seconds and then vanished
    mDNSu8 Occulting;                           // Set if interface vanished for less than 60 seconds and then came back
    mDNSu8 D2DInterface;                        // IFEF_LOCALNET_PRIVATE flag indicates we should call
                                                // D2D plugin for operations over this interface
    mDNSs32 AppearanceTime;                     // Time this interface appeared most recently in getifaddrs list
                                                // i.e. the first time an interface is seen, AppearanceTime is set.
                                                // If an interface goes away temporarily and then comes back then
                                                // AppearanceTime is updated to the time of the most recent appearance.
    mDNSs32 LastSeen;                           // If Exists==0, last time this interface appeared in getifaddrs list
    unsigned int ifa_flags;
    struct in_addr ifa_v4addr;
    mDNSu32 scope_id;                           // interface index / IPv6 scope ID
    mDNSEthAddr BSSID;                          // BSSID of 802.11 base station, if applicable
    u_short sa_family;
    int BPF_fd;                                 // -1 uninitialized; -2 requested BPF; -3 failed
    int BPF_mcfd;                               // Socket for our IPv6 ND group membership
    u_int BPF_len;
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
    dispatch_source_t BPF_source;
#else
    CFSocketRef BPF_cfs;
    CFRunLoopSourceRef BPF_rls;
#endif
    NetworkInterfaceInfoOSX *Registered;        // non-NULL means registered with mDNS Core
};

struct mDNS_PlatformSupport_struct
{
    NetworkInterfaceInfoOSX *InterfaceList;
    KQSocketSet permanentsockets;
    int num_mcasts;                             // Number of multicasts received during this CPU scheduling period (used for CPU limiting)
    domainlabel userhostlabel;                  // The hostlabel as it was set in System Preferences the last time we looked
    domainlabel usernicelabel;                  // The nicelabel as it was set in System Preferences the last time we looked
    // Following four variables are used for optimization where the helper is not
    // invoked when not needed. It records the state of what we told helper the
    // last time we invoked mDNSPreferencesSetName
    domainlabel prevoldhostlabel;               // Previous m->p->userhostlabel
    domainlabel prevnewhostlabel;               // Previous m->hostlabel
    domainlabel prevoldnicelabel;               // Previous m->p->usernicelabel
    domainlabel prevnewnicelabel;               // Previous m->nicelabel
    mDNSs32 NotifyUser;
    mDNSs32 HostNameConflict;                   // Time we experienced conflict on our link-local host name
    mDNSs32 KeyChainTimer;

    SCDynamicStoreRef Store;
    CFRunLoopSourceRef StoreRLS;
    CFRunLoopSourceRef PMRLS;
    int SysEventNotifier;
    KQueueEntry SysEventKQueue;
    IONotificationPortRef PowerPortRef;
    io_connect_t PowerConnection;
    io_object_t PowerNotifier;
#ifdef kIOPMAcknowledgmentOptionSystemCapabilityRequirements
    IOPMConnection IOPMConnection;
#endif
    IOPMAssertionID IOPMAssertion;
    long SleepCookie;                           // Cookie we need to pass to IOAllowPowerChange()
    long WakeAtUTC;
    mDNSs32 RequestReSleep;
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
    dispatch_source_t timer;
    dispatch_source_t custom;
#else
    pthread_mutex_t BigMutex;
#endif
    mDNSs32 BigMutexStartTime;
    int WakeKQueueLoopFD;
    mDNSu8 v4answers;                  // non-zero if we are receiving answers
    mDNSu8 v6answers;                  // for A/AAAA from external DNS servers
    mDNSs32 DNSTrigger;                // Time the DNSTrigger was given
    uint64_t LastConfigGeneration;     // DNS configuration generation number
    UDPSocket UDPProxy;
    TCPSocket TCPProxy;
    ProxyCallback *UDPProxyCallback;
    ProxyCallback *TCPProxyCallback;
};

extern int OfferSleepProxyService;
extern int DisableSleepProxyClient;
extern int UseInternalSleepProxy;
extern int OSXVers, iOSVers;

extern int KQueueFD;

extern void NotifyOfElusiveBug(const char *title, const char *msg); // Both strings are UTF-8 text
extern void SetDomainSecrets(mDNS *m);
extern void mDNSMacOSXNetworkChanged(mDNS *const m);
extern void mDNSMacOSXSystemBuildNumber(char *HINFO_SWstring);
extern NetworkInterfaceInfoOSX *IfindexToInterfaceInfoOSX(const mDNS *const m, mDNSInterfaceID ifindex);
extern void mDNSUpdatePacketFilter(const ResourceRecord *const excludeRecord);
extern void myKQSocketCallBack(int s1, short filter, void *context);
extern void mDNSDynamicStoreSetConfig(int key, const char *subkey, CFPropertyListRef value);
extern void UpdateDebugState(void);

#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
extern int KQueueSet(int fd, u_short flags, short filter, KQueueEntry *const entryRef);
mDNSexport void TriggerEventCompletion(void);
#else
extern int KQueueSet(int fd, u_short flags, short filter, const KQueueEntry *const entryRef);
#endif

// When events are processed on the non-kqueue thread (i.e. CFRunLoop notifications like Sleep/Wake,
// Interface changes, Keychain changes, etc.) they must use KQueueLock/KQueueUnlock to lock out the kqueue thread
extern void KQueueLock(mDNS *const m);
extern void KQueueUnlock(mDNS *const m, const char* task);
extern void mDNSPlatformCloseFD(KQueueEntry *kq, int fd);
extern ssize_t myrecvfrom(const int s, void *const buffer, const size_t max,
                             struct sockaddr *const from, size_t *const fromlen, mDNSAddr *dstaddr, char *ifname, mDNSu8 *ttl);

extern mDNSBool DictionaryIsEnabled(CFDictionaryRef dict);

extern void CUPInit(mDNS *const m);
extern const char *DNSScopeToString(mDNSu32 scope);

// If any event takes more than WatchDogReportingThreshold milliseconds to be processed, we log a warning message
// General event categories are:
//  o Mach client request initiated / terminated
//  o UDS client request
//  o Handling UDP packets received from the network
//  o Environmental change events:
//    - network interface changes
//    - sleep/wake
//    - keychain changes
//  o Name conflict dialog dismissal
//  o Reception of Unix signal (e.g. SIGINFO)
//  o Idle task processing
// If we find that we're getting warnings for any of these categories, and it's not evident
// what's causing the problem, we may need to subdivide some categories into finer-grained
// sub-categories (e.g. "Idle task processing" covers a pretty broad range of sub-tasks).

extern int WatchDogReportingThreshold;

struct CompileTimeAssertionChecks_mDNSMacOSX
{
    // Check our structures are reasonable sizes. Including overly-large buffers, or embedding
    // other overly-large structures instead of having a pointer to them, can inadvertently
    // cause structure sizes (and therefore memory usage) to balloon unreasonably.

    // Checks commented out when sizeof(DNSQuestion) change cascaded into having to change yet another
    // set of hardcoded size values because these structures contain one or more DNSQuestion
    // instances.
//    char sizecheck_NetworkInterfaceInfoOSX[(sizeof(NetworkInterfaceInfoOSX) <=  7084) ? 1 : -1];
    char sizecheck_mDNS_PlatformSupport   [(sizeof(mDNS_PlatformSupport)    <=  1378) ? 1 : -1];
};

#ifdef  __cplusplus
}
#endif

#endif
