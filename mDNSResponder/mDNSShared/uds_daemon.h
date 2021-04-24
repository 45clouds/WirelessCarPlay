/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2002-2013 Apple Inc. All rights reserved.
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
#include "dnssd_ipc.h"

/* Client interface: */

#define SRS_PORT(S) mDNSVal16((S)->RR_SRV.resrec.rdata->u.srv.port)

#define LogTimer(MSG,T) LogMsgNoIdent( MSG " %08X %11d  %08X %11d", (T), (T), (T)-now, (T)-now)

extern int udsserver_init(dnssd_sock_t skts[], mDNSu32 count);
extern mDNSs32 udsserver_idle(mDNSs32 nextevent);
extern void udsserver_info(mDNS *const m);  // print out info about current state
extern void udsserver_handle_configchange(mDNS *const m);
extern int udsserver_exit(void);    // should be called prior to app exit
extern void LogMcastStateInfo(mDNS *const m, mDNSBool mflag, mDNSBool start, mDNSBool mstatelog);
#define LogMcastQ       (mDNS_McastLoggingEnabled == 0) ? ((void)0) : LogMcastQuestion
#define LogMcastS       (mDNS_McastLoggingEnabled == 0) ? ((void)0) : LogMcastService
#define LogMcast        (mDNS_McastLoggingEnabled == 0) ? ((void)0) : LogMsg
#define LogMcastNoIdent (mDNS_McastLoggingEnabled == 0) ? ((void)0) : LogMsgNoIdent

/* Routines that uds_daemon expects to link against: */

typedef void (*udsEventCallback)(int fd, short filter, void *context);
extern mStatus udsSupportAddFDToEventLoop(dnssd_sock_t fd, udsEventCallback callback, void *context, void **platform_data);
extern int     udsSupportReadFD(dnssd_sock_t fd, char* buf, int len, int flags, void *platform_data);
extern mStatus udsSupportRemoveFDFromEventLoop(dnssd_sock_t fd, void *platform_data); // Note: This also CLOSES the file descriptor as well

extern void RecordUpdatedNiceLabel(mDNS *const m, mDNSs32 delay);

// Globals and functions defined in uds_daemon.c and also shared with the old "daemon.c" on OS X

extern mDNS mDNSStorage;
extern DNameListElem *AutoRegistrationDomains;
extern DNameListElem *AutoBrowseDomains;

extern mDNSs32 ChopSubTypes(char *regtype, char **AnonData);
extern AuthRecord *AllocateSubTypes(mDNSs32 NumSubTypes, char *p, char **AnonData);
extern int CountExistingRegistrations(domainname *srv, mDNSIPPort port);
extern mDNSBool callExternalHelpers(mDNSInterfaceID InterfaceID, const domainname *const domain, DNSServiceFlags flags);
extern void FreeExtraRR(mDNS *const m, AuthRecord *const rr, mStatus result);
extern int CountPeerRegistrations(mDNS *const m, ServiceRecordSet *const srs);

#if APPLE_OSX_mDNSResponder

// D2D interface support
extern void external_start_browsing_for_service(mDNSInterfaceID InterfaceID, const domainname *const type, DNS_TypeValues qtype, DNSServiceFlags flags, DNSQuestion * q);
extern void external_stop_browsing_for_service(mDNSInterfaceID InterfaceID, const domainname *const type, DNS_TypeValues qtype, DNSServiceFlags flags);
extern void external_start_advertising_service(const ResourceRecord *const resourceRecord, DNSServiceFlags flags);
extern void external_stop_advertising_service(const ResourceRecord *const resourceRecord, DNSServiceFlags flags);
extern void external_start_resolving_service(mDNSInterfaceID InterfaceID, const domainname *const fqdn, DNSServiceFlags flags);
extern void external_stop_resolving_service(mDNSInterfaceID InterfaceID, const domainname *const fqdn, DNSServiceFlags flags);
extern void external_connection_release(const domainname *instance);

extern void internal_start_browsing_for_service(mDNSInterfaceID InterfaceID, const domainname *const type, DNS_TypeValues qtype, DNSServiceFlags flags);
extern void internal_stop_browsing_for_service(mDNSInterfaceID InterfaceID, const domainname *const type, DNS_TypeValues qtype, DNSServiceFlags flags);
extern void internal_start_advertising_service(const ResourceRecord *const resourceRecord, DNSServiceFlags flags);
extern void internal_stop_advertising_service(const ResourceRecord *const resourceRecord, DNSServiceFlags flags);

#else   // APPLE_OSX_mDNSResponder

#define external_start_browsing_for_service(A,B,C,D,E) (void)(A)
#define external_stop_browsing_for_service(A,B,C,D)  (void)(A)
#define external_start_advertising_service(A,B)      (void)(A)
#define external_stop_advertising_service(A,B)       do { (void)(A); (void)(B); } while (0)
#define external_start_resolving_service(A,B,C)      (void)(A)
#define external_stop_resolving_service(A,B,C)       (void)(A)
#define external_connection_release(A)               (void)(A)

#endif // APPLE_OSX_mDNSResponder

extern const char mDNSResponderVersionString_SCCS[];
#define mDNSResponderVersionString (mDNSResponderVersionString_SCCS+5)

#if DEBUG
extern void SetDebugBoundPath(void);
extern int IsDebugSocketInUse(void);
#endif
