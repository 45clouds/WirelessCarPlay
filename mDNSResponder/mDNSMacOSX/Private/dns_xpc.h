/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2012 Apple Inc. All rights reserved.
 *
 * Defines the common interface between mDNSResponder and the Private ClientLibrary(libdnsprivate.dylib)
 * Uses XPC as the IPC Mechanism
 *
 */

#ifndef DNS_XPC_H
#define DNS_XPC_H

#define kDNSProxyService "com.apple.mDNSResponder.dnsproxy"
#define kDNSCTLService   "com.apple.mDNSResponder.dnsctl"

#define kDNSProxyParameters     "DNSProxyParameters"

#define kDNSInIfindex0          "InputArrayInterfaceIndex[0]"
#define kDNSInIfindex1          "InputArrayInterfaceIndex[1]"
#define kDNSInIfindex2          "InputArrayInterfaceIndex[2]"
#define kDNSInIfindex3          "InputArrayInterfaceIndex[3]"
#define kDNSInIfindex4          "InputArrayInterfaceIndex[4]"

#define kDNSOutIfindex          "OutputInterfaceIndex"

#define kDNSDaemonReply         "DaemonReplyStatusToClient"

typedef enum
{
    kDNSMsg_NoError       =  0,
    kDNSMsg_Busy
} DaemonReplyStatusCodes;

#define kDNSLogLevel            "DNSLoggingVerbosity"

typedef enum
{
    log_level1 = 1, // logging off
    log_level2,     // logging USR1
    log_level3,     // logging USR2
    log_level4,     // logging USR1/2
} DNSLogLevels;

#define kDNSStateInfo            "DNSStateInfoLevels"

typedef enum
{
    full_state = 1,   // full state info of mDNSResponder (INFO)
} DNSStateInfo;

#define kmDNSResponderTests           "mDNSResponderTests"

typedef enum
{
    test_helper_ipc = 1,   // invokes mDNSResponder to send a test msg to mDNSResponderHelper
    test_mDNS_log,         // invokes mDNSResponder to log using different internal macros
} mDNSTestModes;



#endif // DNS_XPC_H
