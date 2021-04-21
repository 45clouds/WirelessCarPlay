/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2016 Apple Inc. All rights reserved.
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

#import "Metrics.h"

#if (TARGET_OS_EMBEDDED)
#import <CoreUtils/SoftLinking.h>
#import <WirelessDiagnostics/AWDDNSDomainStats.h>
#import <WirelessDiagnostics/AWDMDNSResponderDNSStatistics.h>
#import <WirelessDiagnostics/AWDMDNSResponderResolveStats.h>
#import <WirelessDiagnostics/AWDMDNSResponderResolveStatsDNSServer.h>
#import <WirelessDiagnostics/AWDMDNSResponderResolveStatsDomain.h>
#import <WirelessDiagnostics/AWDMDNSResponderResolveStatsHostname.h>
#import <WirelessDiagnostics/AWDMDNSResponderResolveStatsResult.h>
#import <WirelessDiagnostics/AWDMetricIds_MDNSResponder.h>
#import <WirelessDiagnostics/WirelessDiagnostics.h>
#import <WirelessDiagnostics/AWDMDNSResponderServicesStats.h>

#import "DNSCommon.h"
#import "mDNSMacOSX.h"
#import "DebugServices.h"

//===========================================================================================================================
//  External Frameworks
//===========================================================================================================================

SOFT_LINK_FRAMEWORK(PrivateFrameworks, WirelessDiagnostics)

SOFT_LINK_CLASS(WirelessDiagnostics, AWDDNSDomainStats)
SOFT_LINK_CLASS(WirelessDiagnostics, AWDMDNSResponderDNSStatistics)
SOFT_LINK_CLASS(WirelessDiagnostics, AWDMDNSResponderResolveStats)
SOFT_LINK_CLASS(WirelessDiagnostics, AWDMDNSResponderResolveStatsDNSServer)
SOFT_LINK_CLASS(WirelessDiagnostics, AWDMDNSResponderResolveStatsDomain)
SOFT_LINK_CLASS(WirelessDiagnostics, AWDMDNSResponderResolveStatsHostname)
SOFT_LINK_CLASS(WirelessDiagnostics, AWDMDNSResponderResolveStatsResult)
SOFT_LINK_CLASS(WirelessDiagnostics, AWDServerConnection)
SOFT_LINK_CLASS(WirelessDiagnostics, AWDMetricManager)

#define AWDDNSDomainStatsSoft                           getAWDDNSDomainStatsClass()
#define AWDMDNSResponderDNSStatisticsSoft               getAWDMDNSResponderDNSStatisticsClass()
#define AWDMDNSResponderResolveStatsSoft                getAWDMDNSResponderResolveStatsClass()
#define AWDMDNSResponderResolveStatsResultSoft          getAWDMDNSResponderResolveStatsResultClass()
#define AWDMDNSResponderResolveStatsDNSServerSoft       getAWDMDNSResponderResolveStatsDNSServerClass()
#define AWDMDNSResponderResolveStatsDomainSoft          getAWDMDNSResponderResolveStatsDomainClass()
#define AWDMDNSResponderResolveStatsHostnameSoft        getAWDMDNSResponderResolveStatsHostnameClass()
#define AWDServerConnectionSoft                         getAWDServerConnectionClass()
#define AWDMetricManagerSoft                            getAWDMetricManagerClass()

//===========================================================================================================================
//  Macros
//===========================================================================================================================

#define countof(X)                      (sizeof(X) / sizeof(X[0]))
#define countof_field(TYPE, FIELD)      countof(((TYPE *)0)->FIELD)
#define increment_saturate(VAR, MAX)    do {if ((VAR) < (MAX)) {++(VAR);}} while (0)
#define ForgetMem(X)                    do {if(*(X)) {free(*(X)); *(X) = NULL;}} while(0)

//===========================================================================================================================
//  Constants
//===========================================================================================================================

#define kQueryStatsMaxQuerySendCount    10
#define kQueryStatsSendCountBinCount    (kQueryStatsMaxQuerySendCount + 1)
#define kQueryStatsLatencyBinCount      55
#define kResolveStatsMaxObjCount        2000

//===========================================================================================================================
//  Data structures
//===========================================================================================================================

typedef struct
{
    const char *            cstr;       // Name of domain as a c-string.
    const domainname *      name;       // Name of domain as length-prefixed labels.
    int                     labelCount; // Number of labels in domain name. Used for domain name comparisons.

}   Domain;

// Important: Do not add to this list without getting privacy approval beforehand. See <rdar://problem/24155761&26397203>.
// If you get approval and do add a domain to this list, make sure it passes ValidateDNSStatsDomains() below.

static const Domain     kQueryStatsDomains[] =
{
    { ".",              (domainname *)"",                            0 },
    { "apple.com.",     (domainname *)"\x5" "apple"     "\x3" "com", 2 },
    { "icloud.com.",    (domainname *)"\x6" "icloud"    "\x3" "com", 2 },
    { "mzstatic.com.",  (domainname *)"\x8" "mzstatic"  "\x3" "com", 2 },
    { "me.com.",        (domainname *)"\x2" "me"        "\x3" "com", 2 },
    { "google.com.",    (domainname *)"\x6" "google"    "\x3" "com", 2 },
    { "youtube.com.",   (domainname *)"\x7" "youtube"   "\x3" "com", 2 },
    { "facebook.com.",  (domainname *)"\x8" "facebook"  "\x3" "com", 2 },
    { "baidu.com.",     (domainname *)"\x5" "baidu"     "\x3" "com", 2 },
    { "yahoo.com.",     (domainname *)"\x5" "yahoo"     "\x3" "com", 2 },
    { "qq.com.",        (domainname *)"\x2" "qq"        "\x3" "com", 2 },
};

check_compile_time(countof(kQueryStatsDomains) == 11);

// DNSHist contains the per domain per network type histogram data that goes in a DNSDomainStats protobuf message. See
// <rdar://problem/23980546> MDNSResponder.proto update.
//
// answeredQuerySendCountBins
//
// An array of 11 histogram bins. The value at index i, for 0 <= i <= 9, is the number of times that an answered DNS query
// was sent i times. The value at index 10 is the number of times that an answered query was sent 10+ times.
//
// unansweredQuerySendCountBins
//
// An array of 11 histogram bins. The value at index i, for 0 <= i <= 9, is the number of times that an unanswered DNS query
// was sent i times. The value at index 10 is the number of times that an unanswered query was sent 10+ times.
//
// responseLatencyBins
//
// An array of 55 histogram bins. Each array value is the number of DNS queries that were answered in a paricular time
// interval. The 55 consecutive non-overlapping time intervals have the following non-inclusive upper bounds (all values are
// in milliseconds): 1, 2, 3, 4, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 180, 190,
// 200, 250, 300, 350, 400, 450, 500, 550, 600, 650, 700, 750, 800, 850, 900, 950, 1000, 1500, 2000, 2500, 3000, 3500, 4000,
// 4500, 5000, 6000, 7000, 8000, 9000, 10000, ∞.

typedef struct
{
    uint16_t    unansweredQuerySendCountBins[kQueryStatsSendCountBinCount];
    uint16_t    unansweredQueryDurationBins[kQueryStatsLatencyBinCount];
    uint16_t    answeredQuerySendCountBins[kQueryStatsSendCountBinCount];
    uint16_t    responseLatencyBins[kQueryStatsLatencyBinCount];
    uint16_t    negAnsweredQuerySendCountBins[kQueryStatsSendCountBinCount];
    uint16_t    negResponseLatencyBins[kQueryStatsLatencyBinCount];

}   DNSHist;

check_compile_time(sizeof(DNSHist) <= 512);
check_compile_time(countof_field(DNSHist, unansweredQuerySendCountBins)  == (kQueryStatsMaxQuerySendCount + 1));
check_compile_time(countof_field(DNSHist, answeredQuerySendCountBins)    == (kQueryStatsMaxQuerySendCount + 1));
check_compile_time(countof_field(DNSHist, negAnsweredQuerySendCountBins) == (kQueryStatsMaxQuerySendCount + 1));

// Important: Do not modify kResponseLatencyMsLimits because the code used to generate AWD reports expects the response
// latency histogram bins to observe these time interval upper bounds.

static const mDNSu32        kResponseLatencyMsLimits[] =
{
        1,     2,     3,     4,     5,
       10,    20,    30,    40,    50,    60,    70,    80,    90,
      100,   110,   120,   130,   140,   150,   160,   170,   180,   190,
      200,   250,   300,   350,   400,   450,   500,   550,   600,   650,   700,   750,   800,   850,   900,   950,
     1000,  1500,  2000,  2500,  3000,  3500,  4000,  4500,
     5000,  6000,  7000,  8000,  9000,
    10000
};

check_compile_time(countof(kResponseLatencyMsLimits) == 54);
check_compile_time(countof_field(DNSHist, unansweredQueryDurationBins) == (countof(kResponseLatencyMsLimits) + 1));
check_compile_time(countof_field(DNSHist, responseLatencyBins)         == (countof(kResponseLatencyMsLimits) + 1));
check_compile_time(countof_field(DNSHist, negResponseLatencyBins)      == (countof(kResponseLatencyMsLimits) + 1));

typedef struct
{
    DNSHist *       histAny;    // Histogram data for queries of any resource record type.
    DNSHist *       histA;      // Histogram data for queries for A resource records.
    DNSHist *       histAAAA;   // Histogram data for queries for AAAA resource records.

}   DNSHistSet;

typedef struct DNSDomainStats       DNSDomainStats;
struct DNSDomainStats
{
    DNSDomainStats *        next;           // Pointer to next domain stats in list.
    const Domain *          domain;         // Domain for which these stats are collected.
    DNSHistSet *            nonCellular;    // Query stats for queries sent over non-cellular interfaces.
    DNSHistSet *            cellular;       // Query stats for queries sent over cellular interfaces.
};

check_compile_time(sizeof(struct DNSDomainStats) <= 32);

static const Domain     kResolveStatsDomains[] =
{
    { "apple.com.",     (domainname *)"\x5" "apple"    "\x3" "com", 2 },
    { "icloud.com.",    (domainname *)"\x6" "icloud"   "\x3" "com", 2 },
    { "mzstatic.com.",  (domainname *)"\x8" "mzstatic" "\x3" "com", 2 },
    { "me.com.",        (domainname *)"\x2" "me"       "\x3" "com", 2 },
};

check_compile_time(countof(kResolveStatsDomains) == 4);

typedef struct ResolveStatsDomain           ResolveStatsDomain;
typedef struct ResolveStatsHostname         ResolveStatsHostname;
typedef struct ResolveStatsDNSServer        ResolveStatsDNSServer;
typedef struct ResolveStatsIPv4AddrSet      ResolveStatsIPv4AddrSet;
typedef struct ResolveStatsIPv6Addr         ResolveStatsIPv6Addr;
typedef struct ResolveStatsNegAAAASet       ResolveStatsNegAAAASet;

struct ResolveStatsDomain
{
    ResolveStatsDomain *        next;           // Next domain object in list.
    ResolveStatsHostname *      hostnameList;   // List of hostname objects in this domain.
    const Domain *              domainInfo;     // Pointer to domain info.
};

struct ResolveStatsHostname
{
    ResolveStatsHostname *          next;       // Next hostname object in list.
    ResolveStatsIPv4AddrSet *       addrV4List; // List of IPv4 addresses to which this hostname resolved.
    ResolveStatsIPv6Addr *          addrV6List; // List of IPv6 addresses to which this hostname resolved.
    ResolveStatsNegAAAASet *        negV6List;  // List of negative AAAA response objects.
    uint8_t                         name[1];    // Variable length storage for hostname as length-prefixed labels.
};

check_compile_time(sizeof(ResolveStatsHostname) <= 64);

struct ResolveStatsDNSServer
{
    ResolveStatsDNSServer *     next;           // Next DNS server object in list.
    uint8_t                     id;             // 8-bit ID assigned to this DNS server used by IP address objects.
    mDNSBool                    isForCell;      // True if this DNS server belongs to a cellular interface.
    mDNSBool                    isAddrV6;       // True if this DNS server has an IPv6 address instead of IPv4.
    uint8_t                     addrBytes[1];   // Variable length storage for DNS server's IP address.
};

check_compile_time(sizeof(ResolveStatsDNSServer) <= 32);

typedef struct
{
    uint16_t        count;          // Number of times this IPv4 address was provided as a resolution result.
    uint8_t         serverID;       // 8-bit ID of the DNS server from which this IPv4 address came.
    uint8_t         isNegative;
    uint8_t         addrBytes[4];   // IPv4 address bytes.

}   IPv4AddrCounter;

check_compile_time(sizeof(IPv4AddrCounter) <= 8);

struct ResolveStatsIPv4AddrSet
{
    ResolveStatsIPv4AddrSet *       next;           // Next set of IPv4 address counters in list.
    IPv4AddrCounter                 counters[3];    // Array of IPv4 address counters.
};

check_compile_time(sizeof(ResolveStatsIPv4AddrSet) <= 32);

struct ResolveStatsIPv6Addr
{
    ResolveStatsIPv6Addr *      next;           // Next IPv6 address object in list.
    uint16_t                    count;          // Number of times this IPv6 address was provided as a resolution result.
    uint8_t                     serverID;       // 8-bit ID of the DNS server from which this IPv6 address came.
    uint8_t                     addrBytes[16];  // IPv6 address bytes.
};

check_compile_time(sizeof(ResolveStatsIPv6Addr) <= 32);

typedef struct
{
    uint16_t        count;      // Number of times that a negative response was returned by a DNS server.
    uint8_t         serverID;   // 8-bit ID of the DNS server that sent the negative responses.

}   NegAAAACounter;

check_compile_time(sizeof(NegAAAACounter) <= 4);

struct ResolveStatsNegAAAASet
{
    ResolveStatsNegAAAASet *        next;           // Next set of negative AAAA response counters in list.
    NegAAAACounter                  counters[6];    // Array of negative AAAA response counters.
};

check_compile_time(sizeof(ResolveStatsNegAAAASet) <= 32);

typedef enum
{
    kResponseType_IPv4Addr  = 1,
    kResponseType_IPv6Addr  = 2,
    kResponseType_NegA      = 3,
    kResponseType_NegAAAA   = 4

}   ResponseType;

typedef struct
{
    ResponseType        type;
    const uint8_t *     data;

}   Response;

//===========================================================================================================================
//  Globals
//===========================================================================================================================

extern mDNS     mDNSStorage;

static DNSDomainStats *             gDomainStatsList            = NULL;
static ResolveStatsDomain *         gResolveStatsList           = NULL;
static ResolveStatsDNSServer *      gResolveStatsServerList     = NULL;
static unsigned int                 gResolveStatsNextServerID   = 0;
static int                          gResolveStatsObjCount       = 0;
static AWDServerConnection *        gAWDServerConnection        = nil;

//===========================================================================================================================
//  Local Prototypes
//===========================================================================================================================

mDNSlocal mStatus   DNSDomainStatsCreate(const Domain *inDomain, DNSDomainStats **outStats);
mDNSlocal void      DNSDomainStatsFree(DNSDomainStats *inStats);
mDNSlocal void      DNSDomainStatsFreeList(DNSDomainStats *inList);
mDNSlocal mStatus   DNSDomainStatsUpdate(DNSDomainStats *inStats, uint16_t inType, const ResourceRecord *inRR, mDNSu32 inQuerySendCount, mDNSu32 inLatencyMs, mDNSBool inForCell);

mDNSlocal mStatus   ResolveStatsDomainCreate(const Domain *inDomain, ResolveStatsDomain **outDomain);
mDNSlocal void      ResolveStatsDomainFree(ResolveStatsDomain *inDomain);
mDNSlocal mStatus   ResolveStatsDomainUpdate(ResolveStatsDomain *inDomain, const domainname *inHostname, const Response *inResp, const mDNSAddr *inDNSAddr, mDNSBool inForCell);
mDNSlocal mStatus   ResolveStatsDomainCreateAWDVersion(const ResolveStatsDomain *inDomain, AWDMDNSResponderResolveStatsDomain **outDomain);

mDNSlocal mStatus   ResolveStatsHostnameCreate(const domainname *inName, ResolveStatsHostname **outHostname);
mDNSlocal void      ResolveStatsHostnameFree(ResolveStatsHostname *inHostname);
mDNSlocal mStatus   ResolveStatsHostnameUpdate(ResolveStatsHostname *inHostname, const Response *inResp, uint8_t inServerID);
mDNSlocal mStatus   ResolveStatsHostnameCreateAWDVersion(const ResolveStatsHostname *inHostname, AWDMDNSResponderResolveStatsHostname **outHostname);

mDNSlocal mStatus   ResolveStatsDNSServerCreate(const mDNSAddr *inAddr, mDNSBool inForCell, ResolveStatsDNSServer **outServer);
mDNSlocal void      ResolveStatsDNSServerFree(ResolveStatsDNSServer *inServer);
mDNSlocal mStatus   ResolveStatsDNSServerCreateAWDVersion(const ResolveStatsDNSServer *inServer, AWDMDNSResponderResolveStatsDNSServer **outServer);

mDNSlocal mStatus   ResolveStatsIPv4AddrSetCreate(ResolveStatsIPv4AddrSet **outSet);
mDNSlocal void      ResolveStatsIPv4AddrSetFree(ResolveStatsIPv4AddrSet *inSet);

mDNSlocal mStatus   ResolveStatsIPv6AddressCreate(uint8_t inServerID, const uint8_t inAddrBytes[16], ResolveStatsIPv6Addr **outAddr);
mDNSlocal void      ResolveStatsIPv6AddressFree(ResolveStatsIPv6Addr *inAddr);

mDNSlocal mStatus   ResolveStatsNegAAAASetCreate(ResolveStatsNegAAAASet **outSet);
mDNSlocal void      ResolveStatsNegAAAASetFree(ResolveStatsNegAAAASet *inSet);
mDNSlocal mStatus   ResolveStatsGetServerID(const mDNSAddr *inServerAddr, mDNSBool inForCell, uint8_t *outServerID);

mDNSlocal mStatus   CreateDomainStatsList(DNSDomainStats **outList);
mDNSlocal mStatus   CreateResolveStatsList(ResolveStatsDomain **outList);
mDNSlocal void      FreeResolveStatsList(ResolveStatsDomain *inList);
mDNSlocal void      FreeResolveStatsServerList(ResolveStatsDNSServer *inList);
mDNSlocal mStatus   SubmitAWDMetric(UInt32 inMetricID);
mDNSlocal mStatus   SubmitAWDMetricQueryStats(void);
mDNSlocal mStatus   SubmitAWDMetricResolveStats(void);
mDNSlocal mStatus   CreateAWDDNSDomainStats(DNSHist *inHist, const char *inDomain, mDNSBool inForCell, AWDDNSDomainStats_RecordType inType, AWDDNSDomainStats **outStats);
mDNSlocal mStatus   AddAWDDNSDomainStats(AWDMDNSResponderDNSStatistics *inMetric, DNSHistSet *inSet, const char *inDomain, mDNSBool inForCell);
mDNSlocal void      LogDNSHistSet(const DNSHistSet *inSet, const char *inDomain, mDNSBool inForCell);
mDNSlocal void      LogDNSHist(const DNSHist *inHist, const char *inDomain, mDNSBool inForCell, const char *inType);
mDNSlocal void      LogDNSHistSendCounts(const uint16_t inSendCountBins[kQueryStatsSendCountBinCount]);
mDNSlocal void      LogDNSHistLatencies(const uint16_t inLatencyBins[kQueryStatsLatencyBinCount]);
#if (METRICS_VALIDATE_DNS_STATS_DOMAINS)
mDNSlocal void      ValidateDNSStatsDomains(void);
#endif

//===========================================================================================================================
//  MetricsInit
//===========================================================================================================================

mStatus MetricsInit(void)
{
    mStatus     err;

#if (METRICS_VALIDATE_DNS_STATS_DOMAINS)
    ValidateDNSStatsDomains();
#endif

    err = CreateDomainStatsList(&gDomainStatsList);
    require_noerr_quiet(err, exit);

    err = CreateResolveStatsList(&gResolveStatsList);
    require_noerr_quiet(err, exit);

    @autoreleasepool
    {
        gAWDServerConnection = [[AWDServerConnectionSoft alloc]
            initWithComponentId:     AWDComponentId_MDNSResponder
            andBlockOnConfiguration: NO];

        if (gAWDServerConnection)
        {
            [gAWDServerConnection
                registerQueriableMetricCallback: ^(UInt32 inMetricID)
                {
                    SubmitAWDMetric(inMetricID);
                }
                forIdentifier: (UInt32)AWDMetricId_MDNSResponder_DNSStatistics];

            [gAWDServerConnection
                registerQueriableMetricCallback: ^(UInt32 inMetricID)
                {
                    SubmitAWDMetric(inMetricID);
                }
                forIdentifier: (UInt32)AWDMetricId_MDNSResponder_ResolveStats];
            
            [gAWDServerConnection
                registerQueriableMetricCallback: ^(UInt32 inMetricID)
                {
                    SubmitAWDMetric(inMetricID);
                }
                forIdentifier: (UInt32)AWDMetricId_MDNSResponder_ServicesStats];
        }
        else
        {
            LogMsg("MetricsInit: failed to create AWD server connection.");
        }
    }
exit:
    return (err);
}

//===========================================================================================================================
//  MetricsUpdateUDNSQueryStats
//===========================================================================================================================

mDNSexport void MetricsUpdateUDNSQueryStats(const domainname *inQueryName, mDNSu16 inType, const ResourceRecord *inRR, mDNSu32 inSendCount, mDNSu32 inLatencyMs, mDNSBool inForCell)
{
    DNSDomainStats *        stats;
    int                     queryLabelCount;
    const domainname *      queryParentDomain;
    mDNSBool                isQueryInDomain;
    int                     skipCount;
    int                     skipCountLast = -1;

    queryLabelCount = CountLabels(inQueryName);

    for (stats = gDomainStatsList; stats; stats = stats->next)
    {
        isQueryInDomain = mDNSfalse;
        if (strcmp(stats->domain->cstr, ".") == 0)
        {
            // All queries are in the root domain.
            isQueryInDomain = mDNStrue;
        }
        else
        {
            skipCount = queryLabelCount - stats->domain->labelCount;
            if (skipCount >= 0)
            {
                if (skipCount != skipCountLast)
                {
                    queryParentDomain = SkipLeadingLabels(inQueryName, skipCount);
                    skipCountLast = skipCount;
                }
                isQueryInDomain = SameDomainName(queryParentDomain, stats->domain->name);
            }
        }

        if (isQueryInDomain)
        {
            DNSDomainStatsUpdate(stats, inType, inRR, inSendCount, inLatencyMs, inForCell);
        }
    }

}

//===========================================================================================================================
//  MetricsUpdateUDNSResolveStats
//===========================================================================================================================

mDNSexport void MetricsUpdateUDNSResolveStats(const domainname *inQueryName, const ResourceRecord *inRR, mDNSBool inForCell)
{
    ResolveStatsDomain *        domain;
    domainname                  hostname;
    size_t                      hostnameLen;
    mDNSBool                    isQueryInDomain;
    int                         skipCount;
    int                         skipCountLast = -1;
    int                         queryLabelCount;
    const domainname *          queryParentDomain;
    Response                    response;

    require_quiet((inRR->rrtype == kDNSType_A) || (inRR->rrtype == kDNSType_AAAA), exit);
    require_quiet(inRR->rDNSServer, exit);

    queryLabelCount = CountLabels(inQueryName);

    for (domain = gResolveStatsList; domain; domain = domain->next)
    {
        isQueryInDomain = mDNSfalse;
        skipCount = queryLabelCount - domain->domainInfo->labelCount;
        if (skipCount >= 0)
        {
            if (skipCount != skipCountLast)
            {
                queryParentDomain = SkipLeadingLabels(inQueryName, skipCount);
                skipCountLast = skipCount;
            }
            isQueryInDomain = SameDomainName(queryParentDomain, domain->domainInfo->name);
        }
        if (!isQueryInDomain) continue;

        hostnameLen = (size_t)(queryParentDomain->c - inQueryName->c);
        if (hostnameLen >= sizeof(hostname.c)) continue;

        memcpy(hostname.c, inQueryName->c, hostnameLen);
        hostname.c[hostnameLen] = 0;

        if (inRR->RecordType == kDNSRecordTypePacketNegative)
        {
            response.type = (inRR->rrtype == kDNSType_A) ? kResponseType_NegA : kResponseType_NegAAAA;
            response.data = NULL;
        }
        else
        {
            response.type = (inRR->rrtype == kDNSType_A) ? kResponseType_IPv4Addr : kResponseType_IPv6Addr;
            response.data = (inRR->rrtype == kDNSType_A) ? inRR->rdata->u.ipv4.b : inRR->rdata->u.ipv6.b;
        }
        ResolveStatsDomainUpdate(domain, &hostname, &response, &inRR->rDNSServer->addr, inForCell);
    }

exit:
    return;
}

//===========================================================================================================================
//  LogMetrics
//===========================================================================================================================

mDNSexport void LogMetrics(void)
{
    DNSDomainStats *                    stats;
    const ResolveStatsDomain *          domain;
    const ResolveStatsHostname *        hostname;
    const ResolveStatsDNSServer *       server;
    const ResolveStatsIPv4AddrSet *     addrV4;
    const ResolveStatsIPv6Addr *        addrV6;
    const ResolveStatsNegAAAASet *      negV6;
    int                                 hostnameCount;
    int                                 i;
    unsigned int                        serverID;
    int                                 serverObjCount   = 0;
    int                                 hostnameObjCount = 0;
    int                                 addrObjCount     = 0;

    LogMsgNoIdent("---- DNS query stats by domain -----");

    for (stats = gDomainStatsList; stats; stats = stats->next)
    {
        if (!stats->nonCellular && !stats->cellular)
        {
            LogMsgNoIdent("No data for %s", stats->domain->cstr);
            continue;
        }
        if (stats->nonCellular) LogDNSHistSet(stats->nonCellular, stats->domain->cstr, mDNSfalse);
        if (stats->cellular)    LogDNSHistSet(stats->cellular,    stats->domain->cstr, mDNStrue);
    }

    LogMsgNoIdent("---- DNS resolve stats by domain -----");

    LogMsgNoIdent("Servers:");
    for (server = gResolveStatsServerList; server; server = server->next)
    {
        serverObjCount++;
        LogMsgNoIdent(server->isAddrV6 ? "%2u: %s %.16a" : "%2u: %s %.4a",
            server->id, server->isForCell ? " C" : "NC", server->addrBytes);
    }

    for (domain = gResolveStatsList; domain; domain = domain->next)
    {
        hostnameCount = 0;
        for (hostname = domain->hostnameList; hostname; hostname = hostname->next) { hostnameCount++; }
        hostnameObjCount += hostnameCount;

        LogMsgNoIdent("%##s (%d hostname%s)", domain->domainInfo->name, hostnameCount, (hostnameCount == 1) ? "" : "s");

        for (hostname = domain->hostnameList; hostname; hostname = hostname->next)
        {
            LogMsgNoIdent("    %##s", hostname->name);
            for (serverID = 0; serverID < gResolveStatsNextServerID; ++serverID)
            {
                for (addrV4 = hostname->addrV4List; addrV4; addrV4 = addrV4->next)
                {
                    if (serverID == 0) addrObjCount++;
                    for (i = 0; i < (int)countof(addrV4->counters); ++i)
                    {
                        const IPv4AddrCounter *      counter;

                        counter = &addrV4->counters[i];
                        if (counter->count == 0) break;
                        if (counter->serverID == serverID)
                        {
                            if (counter->isNegative)
                            {
                                LogMsgNoIdent("%10u: %3u negative A", counter->serverID, counter->count);
                            }
                            else
                            {
                                LogMsgNoIdent("%10u: %3u %.4a", counter->serverID, counter->count, counter->addrBytes);
                            }
                        }
                    }
                }
                for (addrV6 = hostname->addrV6List; addrV6; addrV6 = addrV6->next)
                {
                    if (serverID == 0) addrObjCount++;
                    if (addrV6->serverID == serverID)
                    {
                        LogMsgNoIdent("%10u: %3u %.16a", addrV6->serverID, addrV6->count, addrV6->addrBytes);
                    }
                }
                for (negV6 = hostname->negV6List; negV6; negV6 = negV6->next)
                {
                    if (serverID == 0) addrObjCount++;
                    for (i = 0; i < (int)countof(negV6->counters); ++i)
                    {
                        const NegAAAACounter *      counter;

                        counter = &negV6->counters[i];
                        if (counter->count == 0) break;
                        if (counter->serverID == serverID)
                        {
                            LogMsgNoIdent("%10u: %3u negative AAAA", counter->serverID, counter->count);
                        }
                    }
                }
            }
        }
    }
    LogMsgNoIdent("Total object count: %3d (server %d hostname %d address %d)",
        serverObjCount + hostnameObjCount + addrObjCount, serverObjCount, hostnameObjCount, addrObjCount);
    
    LogMsgNoIdent("---- Num of Services Registered -----");
    LogMsgNoIdent("Current_number_of_services_registered :[%d], Max_number_of_services_registered :[%d]",
                  curr_num_regservices, max_num_regservices);
}

//===========================================================================================================================
//  DNSDomainStatsCreate
//===========================================================================================================================

mDNSlocal mStatus DNSDomainStatsCreate(const Domain *inDomain, DNSDomainStats **outStats)
{
    mStatus                 err;
    DNSDomainStats *        obj;

    obj = (DNSDomainStats *)calloc(1, sizeof(*obj));
    require_action_quiet(obj, exit, err = mStatus_NoMemoryErr);

    obj->domain = inDomain;

    *outStats = obj;
    err = mStatus_NoError;

exit:
    return (err);
}

//===========================================================================================================================
//  DNSDomainStatsFree
//===========================================================================================================================

mDNSlocal void DNSDomainStatsFree(DNSDomainStats *inStats)
{
    if (inStats->nonCellular)
    {
        ForgetMem(&inStats->nonCellular->histAny);
        ForgetMem(&inStats->nonCellular->histA);
        ForgetMem(&inStats->nonCellular->histAAAA);
        free(inStats->nonCellular);
        inStats->nonCellular = NULL;
    }
    if (inStats->cellular)
    {
        ForgetMem(&inStats->cellular->histAny);
        ForgetMem(&inStats->cellular->histA);
        ForgetMem(&inStats->cellular->histAAAA);
        free(inStats->cellular);
        inStats->cellular = NULL;
    }
    free(inStats);
}

//===========================================================================================================================
//  DNSDomainStatsFreeList
//===========================================================================================================================

mDNSlocal void DNSDomainStatsFreeList(DNSDomainStats *inList)
{
    DNSDomainStats *        stats;

    while ((stats = inList) != NULL)
    {
        inList = stats->next;
        DNSDomainStatsFree(stats);
    }
}

//===========================================================================================================================
//  DNSDomainStatsUpdate
//===========================================================================================================================

mDNSlocal mStatus DNSDomainStatsUpdate(DNSDomainStats *inStats, uint16_t inType, const ResourceRecord *inRR, mDNSu32 inQuerySendCount, mDNSu32 inLatencyMs, mDNSBool inForCell)
{
    mStatus             err;
    DNSHistSet **       p;
    DNSHistSet *        set;
    DNSHist *           histAny;
    DNSHist *           hist;
    int                 i;

    require_action_quiet(inRR || (inQuerySendCount > 0), exit, err = mStatus_NoError);

    p = inForCell ? &inStats->cellular : &inStats->nonCellular;
    if ((set = *p) == NULL)
    {
        set = (DNSHistSet *)calloc(1, sizeof(*set));
        require_action_quiet(set, exit, err = mStatus_NoMemoryErr);
        *p = set;
    }
    if ((histAny = set->histAny) == NULL)
    {
        histAny = (DNSHist *)calloc(1, sizeof(*histAny));
        require_action_quiet(histAny, exit, err = mStatus_NoMemoryErr);
        set->histAny = histAny;
    }
    if (inType == kDNSType_A)
    {
        if ((hist = set->histA) == NULL)
        {
            hist = (DNSHist *)calloc(1, sizeof(*hist));
            require_action_quiet(hist, exit, err = mStatus_NoMemoryErr);
            set->histA = hist;
        }
    }
    else if (inType == kDNSType_AAAA)
    {
        if ((hist = set->histAAAA) == NULL)
        {
            hist = (DNSHist *)calloc(1, sizeof(*hist));
            require_action_quiet(hist, exit, err = mStatus_NoMemoryErr);
            set->histAAAA = hist;
        }
    }
    else
    {
        hist = NULL;
    }

    if (inRR)
    {
        uint16_t *          sendCountBins;
        uint16_t *          latencyBins;
        const mDNSBool      isNegative = (inRR->RecordType == kDNSRecordTypePacketNegative);

        i = Min(inQuerySendCount, kQueryStatsMaxQuerySendCount);

        sendCountBins = isNegative ? histAny->negAnsweredQuerySendCountBins : histAny->answeredQuerySendCountBins;
        increment_saturate(sendCountBins[i], UINT16_MAX);
        if (hist)
        {
            sendCountBins = isNegative ? hist->negAnsweredQuerySendCountBins : hist->answeredQuerySendCountBins;
            increment_saturate(sendCountBins[i], UINT16_MAX);
        }

        if (inQuerySendCount > 0)
        {
            for (i = 0; (i < (int)countof(kResponseLatencyMsLimits)) && (inLatencyMs >= kResponseLatencyMsLimits[i]); ++i) {}
            latencyBins = isNegative ? histAny->negResponseLatencyBins : histAny->responseLatencyBins;
            increment_saturate(latencyBins[i], UINT16_MAX);
            if (hist)
            {
                latencyBins = isNegative ? hist->negResponseLatencyBins : hist->responseLatencyBins;
                increment_saturate(latencyBins[i], UINT16_MAX);
            }
        }
    }
    else
    {
        i = Min(inQuerySendCount, kQueryStatsMaxQuerySendCount);
        increment_saturate(histAny->unansweredQuerySendCountBins[i], UINT16_MAX);
        if (hist) increment_saturate(hist->unansweredQuerySendCountBins[i], UINT16_MAX);

        for (i = 0; (i < (int)countof(kResponseLatencyMsLimits)) && (inLatencyMs >= kResponseLatencyMsLimits[i]); ++i) {}
        increment_saturate(histAny->unansweredQueryDurationBins[i], UINT16_MAX);
        if (hist) increment_saturate(hist->unansweredQueryDurationBins[i], UINT16_MAX);
    }
    err = mStatus_NoError;

exit:
    return (err);
}

//===========================================================================================================================
//  ResolveStatsDomainCreate
//===========================================================================================================================

mDNSlocal mStatus ResolveStatsDomainCreate(const Domain *inDomain, ResolveStatsDomain **outDomain)
{
    mStatus                     err;
    ResolveStatsDomain *        obj;

    obj = (ResolveStatsDomain *)calloc(1, sizeof(*obj));
    require_action_quiet(obj, exit, err = mStatus_NoMemoryErr);

    obj->domainInfo = inDomain;

    *outDomain = obj;
    err = mStatus_NoError;

exit:
    return (err);
}

//===========================================================================================================================
//  ResolveStatsDomainFree
//===========================================================================================================================

mDNSlocal void ResolveStatsDomainFree(ResolveStatsDomain *inDomain)
{
    ResolveStatsHostname *      hostname;

    while ((hostname = inDomain->hostnameList) != NULL)
    {
        inDomain->hostnameList = hostname->next;
        ResolveStatsHostnameFree(hostname);
    }
    free(inDomain);
}

//===========================================================================================================================
//  ResolveStatsDomainUpdate
//===========================================================================================================================

mDNSlocal mStatus ResolveStatsDomainUpdate(ResolveStatsDomain *inDomain, const domainname *inHostname, const Response *inResp, const mDNSAddr *inDNSAddr, mDNSBool inForCell)
{
    mStatus                     err;
    ResolveStatsHostname **     p;
    ResolveStatsHostname *      hostname;
    uint8_t                     serverID;

    for (p = &inDomain->hostnameList; (hostname = *p) != NULL; p = &hostname->next)
    {
        if (SameDomainName((domainname *)hostname->name, inHostname)) break;
    }

    if (!hostname)
    {
        require_action_quiet(gResolveStatsObjCount < kResolveStatsMaxObjCount, exit, err = mStatus_Refused);
        err = ResolveStatsHostnameCreate(inHostname, &hostname);
        require_noerr_quiet(err, exit);
        gResolveStatsObjCount++;
        *p = hostname;
    }

    err = ResolveStatsGetServerID(inDNSAddr, inForCell, &serverID);
    require_noerr_quiet(err, exit);

    err = ResolveStatsHostnameUpdate(hostname, inResp, serverID);
    require_noerr_quiet(err, exit);

exit:
    return (err);
}

//===========================================================================================================================
//  ResolveStatsHostnameCreate
//===========================================================================================================================

mDNSlocal mStatus ResolveStatsHostnameCreate(const domainname *inName, ResolveStatsHostname **outHostname)
{
    mStatus                     err;
    ResolveStatsHostname *      obj;
    size_t                      nameLen;

    nameLen = DomainNameLength(inName);
    require_action_quiet(nameLen > 0, exit, err = mStatus_Invalid);

    obj = (ResolveStatsHostname *)calloc(1, sizeof(*obj) - 1 + nameLen);
    require_action_quiet(obj, exit, err = mStatus_NoMemoryErr);

    memcpy(obj->name, inName, nameLen);

    *outHostname = obj;
    err = mStatus_NoError;

exit:
    return (err);
}

//===========================================================================================================================
//  ResolveStatsDomainCreateAWDVersion
//===========================================================================================================================

mDNSlocal mStatus ResolveStatsDomainCreateAWDVersion(const ResolveStatsDomain *inDomain, AWDMDNSResponderResolveStatsDomain **outDomain)
{
    mStatus                                     err;
    AWDMDNSResponderResolveStatsDomain *        domain;
    ResolveStatsHostname *                      hostname;
    AWDMDNSResponderResolveStatsHostname *      awdHostname;
    NSString *                                  name;

    domain = [[AWDMDNSResponderResolveStatsDomainSoft alloc] init];
    require_action_quiet(domain, exit, err = mStatus_UnknownErr);

    name = [[NSString alloc] initWithUTF8String:inDomain->domainInfo->cstr];
    require_action_quiet(name, exit, err = mStatus_UnknownErr);

    domain.name = name;
    [name release];
    name = nil;

    for (hostname = inDomain->hostnameList; hostname; hostname = hostname->next)
    {
        err = ResolveStatsHostnameCreateAWDVersion(hostname, &awdHostname);
        require_noerr_quiet(err, exit);

        [domain addHostname:awdHostname];
        [awdHostname release];
        awdHostname = nil;
    }

    *outDomain = domain;
    domain = nil;
    err = mStatus_NoError;

exit:
    [domain release];
    return (err);
}

//===========================================================================================================================
//  ResolveStatsHostnameFree
//===========================================================================================================================

mDNSlocal void ResolveStatsHostnameFree(ResolveStatsHostname *inHostname)
{
    ResolveStatsIPv4AddrSet *       addrV4;
    ResolveStatsIPv6Addr *          addrV6;
    ResolveStatsNegAAAASet *        negV6;

    while ((addrV4 = inHostname->addrV4List) != NULL)
    {
        inHostname->addrV4List = addrV4->next;
        ResolveStatsIPv4AddrSetFree(addrV4);
    }
    while ((addrV6 = inHostname->addrV6List) != NULL)
    {
        inHostname->addrV6List = addrV6->next;
        ResolveStatsIPv6AddressFree(addrV6);
    }
    while ((negV6 = inHostname->negV6List) != NULL)
    {
        inHostname->negV6List = negV6->next;
        ResolveStatsNegAAAASetFree(negV6);
    }
    free(inHostname);
}

//===========================================================================================================================
//  ResolveStatsHostnameUpdate
//===========================================================================================================================

mDNSlocal mStatus ResolveStatsHostnameUpdate(ResolveStatsHostname *inHostname, const Response *inResp, uint8_t inServerID)
{
    mStatus     err;

    if ((inResp->type == kResponseType_IPv4Addr) || (inResp->type == kResponseType_NegA))
    {
        ResolveStatsIPv4AddrSet **      p;
        ResolveStatsIPv4AddrSet *       addrV4;
        int                             i;
        IPv4AddrCounter *               counter;

        for (p = &inHostname->addrV4List; (addrV4 = *p) != NULL; p = &addrV4->next)
        {
            for (i = 0; i < (int)countof(addrV4->counters); ++i)
            {
                counter = &addrV4->counters[i];
                if (counter->count == 0) break;
                if (counter->serverID != inServerID) continue;
                if (inResp->type == kResponseType_NegA)
                {
                    if (counter->isNegative) break;
                }
                else
                {
                    if (memcmp(counter->addrBytes, inResp->data, 4) == 0) break;
                }
            }
            if (i < (int)countof(addrV4->counters)) break;
        }
        if (!addrV4)
        {
            require_action_quiet(gResolveStatsObjCount < kResolveStatsMaxObjCount, exit, err = mStatus_Refused);
            err = ResolveStatsIPv4AddrSetCreate(&addrV4);
            require_noerr_quiet(err, exit);
            gResolveStatsObjCount++;

            *p = addrV4;
            counter = &addrV4->counters[0];
        }
        if (counter->count == 0)
        {
            counter->serverID = inServerID;
            if (inResp->type == kResponseType_NegA)
            {
                counter->isNegative = 1;
            }
            else
            {
                counter->isNegative = 0;
                memcpy(counter->addrBytes, inResp->data, 4);
            }
        }
        increment_saturate(counter->count, UINT16_MAX);
        err = mStatus_NoError;
    }
    else if (inResp->type == kResponseType_IPv6Addr)
    {
        ResolveStatsIPv6Addr **     p;
        ResolveStatsIPv6Addr *      addrV6;

        for (p = &inHostname->addrV6List; (addrV6 = *p) != NULL; p = &addrV6->next)
        {
            if ((addrV6->serverID == inServerID) && (memcmp(addrV6->addrBytes, inResp->data, 16) == 0)) break;
        }
        if (!addrV6)
        {
            require_action_quiet(gResolveStatsObjCount < kResolveStatsMaxObjCount, exit, err = mStatus_Refused);
            err = ResolveStatsIPv6AddressCreate(inServerID, inResp->data, &addrV6);
            require_noerr_quiet(err, exit);
            gResolveStatsObjCount++;

            *p = addrV6;
        }
        increment_saturate(addrV6->count, UINT16_MAX);
        err = mStatus_NoError;
    }
    else if (inResp->type == kResponseType_NegAAAA)
    {
        ResolveStatsNegAAAASet **       p;
        ResolveStatsNegAAAASet *        negV6;
        int                             i;
        NegAAAACounter *                counter;

        for (p = &inHostname->negV6List; (negV6 = *p) != NULL; p = &negV6->next)
        {
            for (i = 0; i < (int)countof(negV6->counters); ++i)
            {
                counter = &negV6->counters[i];
                if ((counter->count == 0) || (counter->serverID == inServerID)) break;
            }
            if (i < (int)countof(negV6->counters)) break;
        }
        if (!negV6)
        {
            require_action_quiet(gResolveStatsObjCount < kResolveStatsMaxObjCount, exit, err = mStatus_Refused);
            err = ResolveStatsNegAAAASetCreate(&negV6);
            require_noerr_quiet(err, exit);
            gResolveStatsObjCount++;

            *p = negV6;
            counter = &negV6->counters[0];
        }
        if (counter->count == 0) counter->serverID = inServerID;
        increment_saturate(counter->count, UINT16_MAX);
        err = mStatus_NoError;
    }
    else
    {
        err = mStatus_Invalid;
    }

exit:
    return (err);
}

//===========================================================================================================================
//  ResolveStatsHostnameCreateAWDVersion
//===========================================================================================================================

mDNSlocal mStatus ResolveStatsHostnameCreateAWDVersion(const ResolveStatsHostname *inHostname, AWDMDNSResponderResolveStatsHostname **outHostname)
{
    mStatus                                     err;
    AWDMDNSResponderResolveStatsHostname *      hostname;
    NSString *                                  name;
    char                                        nameBuf[MAX_ESCAPED_DOMAIN_NAME];
    const char *                                ptr;
    ResolveStatsIPv4AddrSet *                   addrV4;
    ResolveStatsIPv6Addr *                      addrV6;
    ResolveStatsNegAAAASet *                    negV6;
    AWDMDNSResponderResolveStatsResult *        result = nil;
    int                                         i;

    hostname = [[AWDMDNSResponderResolveStatsHostnameSoft alloc] init];
    require_action_quiet(hostname, exit, err = mStatus_UnknownErr);

    ptr = ConvertDomainNameToCString((domainname *)inHostname->name, nameBuf);
    require_action_quiet(ptr, exit, err = mStatus_UnknownErr);

    name = [[NSString alloc] initWithUTF8String:nameBuf];
    require_action_quiet(name, exit, err = mStatus_UnknownErr);

    hostname.name = name;
    [name release];
    name = nil;

    for (addrV4 = inHostname->addrV4List; addrV4; addrV4 = addrV4->next)
    {
        for (i = 0; i < (int)countof(addrV4->counters); ++i)
        {
            const IPv4AddrCounter *     counter;
            NSData *                    addrBytes;

            counter = &addrV4->counters[i];
            if (counter->count == 0) break;

            result = [[AWDMDNSResponderResolveStatsResultSoft alloc] init];
            require_action_quiet(result, exit, err = mStatus_UnknownErr);

            if (counter->isNegative)
            {
                result.type = AWDMDNSResponderResolveStatsResult_ResultType_NegA;
            }
            else
            {
                addrBytes = [[NSData alloc] initWithBytes:counter->addrBytes length:4];
                require_action_quiet(addrBytes, exit, err = mStatus_UnknownErr);

                result.type = AWDMDNSResponderResolveStatsResult_ResultType_IPv4Addr;
                result.data = addrBytes;
                [addrBytes release];
            }
            result.count    = counter->count;
            result.serverID = counter->serverID;

            [hostname addResult:result];
            [result release];
            result = nil;
        }
    }

    for (addrV6 = inHostname->addrV6List; addrV6; addrV6 = addrV6->next)
    {
        NSData *        addrBytes;

        result = [[AWDMDNSResponderResolveStatsResultSoft alloc] init];
        require_action_quiet(result, exit, err = mStatus_UnknownErr);

        addrBytes = [[NSData alloc] initWithBytes:addrV6->addrBytes length:16];
        require_action_quiet(addrBytes, exit, err = mStatus_UnknownErr);

        result.type     = AWDMDNSResponderResolveStatsResult_ResultType_IPv6Addr;
        result.count    = addrV6->count;
        result.serverID = addrV6->serverID;
        result.data     = addrBytes;

        [addrBytes release];

        [hostname addResult:result];
        [result release];
        result = nil;
    }

    for (negV6 = inHostname->negV6List; negV6; negV6 = negV6->next)
    {
        for (i = 0; i < (int)countof(negV6->counters); ++i)
        {
            const NegAAAACounter *      counter;

            counter = &negV6->counters[i];
            if (counter->count == 0) break;

            result = [[AWDMDNSResponderResolveStatsResultSoft alloc] init];
            require_action_quiet(result, exit, err = mStatus_UnknownErr);

            result.type     = AWDMDNSResponderResolveStatsResult_ResultType_NegAAAA;
            result.count    = counter->count;
            result.serverID = counter->serverID;

            [hostname addResult:result];
            [result release];
            result = nil;
        }
    }

    *outHostname = hostname;
    hostname = nil;
    err = mStatus_NoError;

exit:
    [result release];
    [hostname release];
    return (err);
}

//===========================================================================================================================
//  ResolveStatsDNSServerCreate
//===========================================================================================================================

mDNSlocal mStatus ResolveStatsDNSServerCreate(const mDNSAddr *inAddr, mDNSBool inForCell, ResolveStatsDNSServer **outServer)
{
    mStatus                     err;
    ResolveStatsDNSServer *     obj;
    size_t                      addrLen;

    require_action_quiet((inAddr->type == mDNSAddrType_IPv4) || (inAddr->type == mDNSAddrType_IPv6), exit, err = mStatus_Invalid);

    addrLen = (inAddr->type == mDNSAddrType_IPv4) ? 4 : 16;
    obj = (ResolveStatsDNSServer *)calloc(1, sizeof(*obj) - 1 + addrLen);
    require_action_quiet(obj, exit, err = mStatus_NoMemoryErr);

    obj->isForCell = inForCell;
    if (inAddr->type == mDNSAddrType_IPv4)
    {
        obj->isAddrV6 = mDNSfalse;
        memcpy(obj->addrBytes, inAddr->ip.v4.b, addrLen);
    }
    else
    {
        obj->isAddrV6 = mDNStrue;
        memcpy(obj->addrBytes, inAddr->ip.v6.b, addrLen);
    }

    *outServer = obj;
    err = mStatus_NoError;

exit:
    return (err);
}

//===========================================================================================================================
//  ResolveStatsDNSServerFree
//===========================================================================================================================

mDNSlocal void ResolveStatsDNSServerFree(ResolveStatsDNSServer *inServer)
{
    free(inServer);
}

//===========================================================================================================================
//  ResolveStatsDNSServerCreateAWDVersion
//===========================================================================================================================

mDNSlocal mStatus ResolveStatsDNSServerCreateAWDVersion(const ResolveStatsDNSServer *inServer, AWDMDNSResponderResolveStatsDNSServer **outServer)
{
    mStatus                                     err;
    AWDMDNSResponderResolveStatsDNSServer *     server;
    NSData *                                    addrBytes = nil;

    server = [[AWDMDNSResponderResolveStatsDNSServerSoft alloc] init];
    require_action_quiet(server, exit, err = mStatus_UnknownErr);

    addrBytes = [[NSData alloc] initWithBytes:inServer->addrBytes length:(inServer->isAddrV6 ? 16 : 4)];
    require_action_quiet(addrBytes, exit, err = mStatus_UnknownErr);

    server.serverID = inServer->id;
    server.address  = addrBytes;
    if (inServer->isForCell)
    {
        server.networkType = AWDMDNSResponderResolveStatsDNSServer_NetworkType_Cellular;
    }
    else
    {
        server.networkType = AWDMDNSResponderResolveStatsDNSServer_NetworkType_NonCellular;
    }

    *outServer = server;
    server = nil;
    err = mStatus_NoError;

exit:
    [addrBytes release];
    [server release];
    return (err);
}

//===========================================================================================================================
//  ResolveStatsIPv4AddrSetCreate
//===========================================================================================================================

mDNSlocal mStatus ResolveStatsIPv4AddrSetCreate(ResolveStatsIPv4AddrSet **outSet)
{
    mStatus                         err;
    ResolveStatsIPv4AddrSet *       obj;

    obj = (ResolveStatsIPv4AddrSet *)calloc(1, sizeof(*obj));
    require_action_quiet(obj, exit, err = mStatus_NoMemoryErr);

    *outSet = obj;
    err = mStatus_NoError;

exit:
    return (err);
}

//===========================================================================================================================
//  ResolveStatsIPv4AddrSetFree
//===========================================================================================================================

mDNSlocal void ResolveStatsIPv4AddrSetFree(ResolveStatsIPv4AddrSet *inSet)
{
    free(inSet);
}

//===========================================================================================================================
//  ResolveStatsIPv6AddressCreate
//===========================================================================================================================

mDNSlocal mStatus ResolveStatsIPv6AddressCreate(uint8_t inServerID, const uint8_t inAddrBytes[16], ResolveStatsIPv6Addr **outAddr)
{
    mStatus                     err;
    ResolveStatsIPv6Addr *      obj;

    obj = (ResolveStatsIPv6Addr *)calloc(1, sizeof(*obj));
    require_action_quiet(obj, exit, err = mStatus_NoMemoryErr);

    obj->serverID = inServerID;
    memcpy(obj->addrBytes, inAddrBytes, 16);

    *outAddr = obj;
    err = mStatus_NoError;

exit:
    return (err);
}

//===========================================================================================================================
//  ResolveStatsIPv6AddressFree
//===========================================================================================================================

mDNSlocal void ResolveStatsIPv6AddressFree(ResolveStatsIPv6Addr *inAddr)
{
    free(inAddr);
}

//===========================================================================================================================
//  ResolveStatsNegAAAASetCreate
//===========================================================================================================================

mDNSlocal mStatus ResolveStatsNegAAAASetCreate(ResolveStatsNegAAAASet **outSet)
{
    mStatus                         err;
    ResolveStatsNegAAAASet *        obj;

    obj = (ResolveStatsNegAAAASet *)calloc(1, sizeof(*obj));
    require_action_quiet(obj, exit, err = mStatus_NoMemoryErr);

    *outSet = obj;
    err = mStatus_NoError;

exit:
    return (err);
}

//===========================================================================================================================
//  ResolveStatsNegAAAASetFree
//===========================================================================================================================

mDNSlocal void ResolveStatsNegAAAASetFree(ResolveStatsNegAAAASet *inSet)
{
    free(inSet);
}

//===========================================================================================================================
//  ResolveStatsGetServerID
//===========================================================================================================================

mDNSlocal mStatus ResolveStatsGetServerID(const mDNSAddr *inServerAddr, mDNSBool inForCell, uint8_t *outServerID)
{
    mStatus                         err;
    ResolveStatsDNSServer **        p;
    ResolveStatsDNSServer *         server;

    require_action_quiet((inServerAddr->type == mDNSAddrType_IPv4) || (inServerAddr->type == mDNSAddrType_IPv6), exit, err = mStatus_Invalid);

    for (p = &gResolveStatsServerList; (server = *p) != NULL; p = &server->next)
    {
        if ((inForCell && server->isForCell) || (!inForCell && !server->isForCell))
        {
            if (inServerAddr->type == mDNSAddrType_IPv4)
            {
                if (!server->isAddrV6 && (memcmp(server->addrBytes, inServerAddr->ip.v4.b, 4) == 0)) break;
            }
            else
            {
                if (server->isAddrV6 && (memcmp(server->addrBytes, inServerAddr->ip.v6.b, 16) == 0)) break;
            }
        }
    }

    if (!server)
    {
        require_action_quiet(gResolveStatsNextServerID <= UINT8_MAX, exit, err = mStatus_Refused);
        require_action_quiet(gResolveStatsObjCount < kResolveStatsMaxObjCount, exit, err = mStatus_Refused);
        err = ResolveStatsDNSServerCreate(inServerAddr, inForCell, &server);
        require_noerr_quiet(err, exit);
        gResolveStatsObjCount++;

        server->id   = gResolveStatsNextServerID++;
        server->next = gResolveStatsServerList;
        gResolveStatsServerList = server;
    }
    else if (gResolveStatsServerList != server)
    {
        *p = server->next;
        server->next = gResolveStatsServerList;
        gResolveStatsServerList = server;
    }

    *outServerID = server->id;
    err = mStatus_NoError;

exit:
    return (err);
}

//===========================================================================================================================
//  CreateDomainStatsList
//===========================================================================================================================

mDNSlocal mStatus CreateDomainStatsList(DNSDomainStats **outList)
{
    mStatus                 err;
    int                     i;
    DNSDomainStats *        stats;
    DNSDomainStats **       p;
    DNSDomainStats *        list = NULL;

    p = &list;
    for (i = 0; i < (int)countof(kQueryStatsDomains); ++i)
    {
        err = DNSDomainStatsCreate(&kQueryStatsDomains[i], &stats);
        require_noerr_quiet(err, exit);

        *p = stats;
        p = &stats->next;
    }

    *outList = list;
    list = NULL;

exit:
    DNSDomainStatsFreeList(list);
    return (err);
}

//===========================================================================================================================
//  CreateResolveStatsList
//===========================================================================================================================

mDNSlocal mStatus CreateResolveStatsList(ResolveStatsDomain **outList)
{
    mStatus                     err;
    int                         i;
    ResolveStatsDomain *        domain;
    ResolveStatsDomain **       p;
    ResolveStatsDomain *        list = NULL;

    p = &list;
    for (i = 0; i < (int)countof(kResolveStatsDomains); ++i)
    {
        err = ResolveStatsDomainCreate(&kResolveStatsDomains[i], &domain);
        require_noerr_quiet(err, exit);

        *p = domain;
        p = &domain->next;
    }

    *outList = list;
    list = NULL;

exit:
    FreeResolveStatsList(list);
    return (err);
}

//===========================================================================================================================
//  FreeResolveStatsList
//===========================================================================================================================

mDNSlocal void FreeResolveStatsList(ResolveStatsDomain *inList)
{
    ResolveStatsDomain *        domain;

    while ((domain = inList) != NULL)
    {
        inList = domain->next;
        ResolveStatsDomainFree(domain);
    }
}

//===========================================================================================================================
//  FreeResolveStatsServerList
//===========================================================================================================================

mDNSlocal void FreeResolveStatsServerList(ResolveStatsDNSServer *inList)
{
    ResolveStatsDNSServer *     server;

    while ((server = inList) != NULL)
    {
        inList = server->next;
        ResolveStatsDNSServerFree(server);
    }
}

//===========================================================================================================================
//  SubmitAWDMetric
//===========================================================================================================================

mDNSlocal mStatus SubmitAWDMetric(UInt32 inMetricID)
{
    mStatus     err = mStatus_NoError;

    switch (inMetricID)
    {
        case AWDMetricId_MDNSResponder_DNSStatistics:
            err = SubmitAWDMetricQueryStats();
            break;

        case AWDMetricId_MDNSResponder_ResolveStats:
            err = SubmitAWDMetricResolveStats();
            break;

        case AWDMetricId_MDNSResponder_ServicesStats:
            [AWDMetricManagerSoft postMetricWithId:AWDMetricId_MDNSResponder_ServicesStats integerValue:max_num_regservices];
            KQueueLock(&mDNSStorage);
            // reset the no of max services since we want to collect the max no of services registered per AWD submission period
            max_num_regservices = curr_num_regservices;
            KQueueUnlock(&mDNSStorage, "SubmitAWDSimpleMetricServiceStats");
            break;
            
        default:
            err = mStatus_UnsupportedErr;
            break;
    }

    if (err)
        LogMsg("SubmitAWDMetric for metric ID 0x%08X failed with error %d", inMetricID, err);
    return (err);
}

//===========================================================================================================================
//  SubmitAWDMetricQueryStats
//===========================================================================================================================

mDNSlocal mStatus SubmitAWDMetricQueryStats(void)
{
    mStatus                             err;
    BOOL                                success;
    DNSDomainStats *                    stats;
    DNSDomainStats *                    newDomainStatsList;
    DNSDomainStats *                    domainStatsList = NULL;
    AWDMetricContainer *                container       = nil;
    AWDMDNSResponderDNSStatistics *     metric          = nil;

    err = CreateDomainStatsList(&newDomainStatsList);
    require_noerr_quiet(err, exit);

    domainStatsList = gDomainStatsList;

    KQueueLock(&mDNSStorage);
    gDomainStatsList = newDomainStatsList;
    KQueueUnlock(&mDNSStorage, "SubmitAWDMetricQueryStats");

    container = [gAWDServerConnection newMetricContainerWithIdentifier:AWDMetricId_MDNSResponder_DNSStatistics];
    require_action_quiet(container, exit, err = mStatus_UnknownErr);

    metric = [[AWDMDNSResponderDNSStatisticsSoft alloc] init];
    require_action_quiet(metric, exit, err = mStatus_UnknownErr);

    while ((stats = domainStatsList) != NULL)
    {
        if (stats->nonCellular)
        {
            err = AddAWDDNSDomainStats(metric, stats->nonCellular, stats->domain->cstr, mDNSfalse);
            require_noerr_quiet(err, exit);
        }
        if (stats->cellular)
        {
            err = AddAWDDNSDomainStats(metric, stats->cellular, stats->domain->cstr, mDNStrue);
            require_noerr_quiet(err, exit);
        }
        domainStatsList = stats->next;
        DNSDomainStatsFree(stats);
    }

    container.metric = metric;
    success = [gAWDServerConnection submitMetric:container];
    LogMsg("SubmitAWDMetricQueryStats: metric submission %s.", success ? "succeeded" : "failed" );
    err = success ? mStatus_NoError : mStatus_UnknownErr;

exit:
    [metric release];
    [container release];
    DNSDomainStatsFreeList(domainStatsList);
    return (err);
}

//===========================================================================================================================
//  SubmitAWDMetricResolveStats
//===========================================================================================================================

mDNSlocal mStatus SubmitAWDMetricResolveStats(void)
{
    mStatus                             err;
    ResolveStatsDomain *                newResolveStatsList;
    ResolveStatsDomain *                domainList  = NULL;
    ResolveStatsDNSServer *             serverList  = NULL;
    AWDMetricContainer *                container   = nil;
    AWDMDNSResponderResolveStats *      metric      = nil;
    ResolveStatsDNSServer *             server;
    ResolveStatsDomain *                domain;
    BOOL                                success;

    err = CreateResolveStatsList(&newResolveStatsList);
    require_noerr_quiet(err, exit);

    domainList = gResolveStatsList;
    serverList = gResolveStatsServerList;

    KQueueLock(&mDNSStorage);
    gResolveStatsList           = newResolveStatsList;
    gResolveStatsServerList     = NULL;
    gResolveStatsNextServerID   = 0;
    gResolveStatsObjCount       = 0;
    KQueueUnlock(&mDNSStorage, "SubmitAWDMetricResolveStats");

    container = [gAWDServerConnection newMetricContainerWithIdentifier:AWDMetricId_MDNSResponder_ResolveStats];
    require_action_quiet(container, exit, err = mStatus_UnknownErr);

    metric = [[AWDMDNSResponderResolveStatsSoft alloc] init];
    require_action_quiet(metric, exit, err = mStatus_UnknownErr);

    while ((server = serverList) != NULL)
    {
        AWDMDNSResponderResolveStatsDNSServer *     awdServer;

        serverList = server->next;
        err = ResolveStatsDNSServerCreateAWDVersion(server, &awdServer);
        ResolveStatsDNSServerFree(server);
        require_noerr_quiet(err, exit);

        [metric addServer:awdServer];
        [awdServer release];
    }

    while ((domain = domainList) != NULL)
    {
        AWDMDNSResponderResolveStatsDomain *        awdDomain;

        domainList = domain->next;
        err = ResolveStatsDomainCreateAWDVersion(domain, &awdDomain);
        ResolveStatsDomainFree(domain);
        require_noerr_quiet(err, exit);

        [metric addDomain:awdDomain];
        [awdDomain release];
    }

    container.metric = metric;
    success = [gAWDServerConnection submitMetric:container];
    LogMsg("SubmitAWDMetricResolveStats: metric submission %s.", success ? "succeeded" : "failed" );
    err = success ? mStatus_NoError : mStatus_UnknownErr;

exit:
    [metric release];
    [container release];
    FreeResolveStatsList(domainList);
    FreeResolveStatsServerList(serverList);
    return (err);
}

//===========================================================================================================================
//  CreateAWDDNSDomainStats
//===========================================================================================================================

mDNSlocal mStatus CreateAWDDNSDomainStats(DNSHist *inHist, const char *inDomain, mDNSBool inForCell, AWDDNSDomainStats_RecordType inType, AWDDNSDomainStats **outStats)
{
    mStatus                 err;
    AWDDNSDomainStats *     awdStats    = nil;
    NSString *              domain      = nil;
    uint32_t                sendCountBins[kQueryStatsSendCountBinCount];
    uint32_t                latencyBins[kQueryStatsLatencyBinCount];
    int                     i;
    unsigned int            totalAnswered;
    unsigned int            totalNegAnswered;
    unsigned int            totalUnanswered;

    awdStats = [[AWDDNSDomainStatsSoft alloc] init];
    require_action_quiet(awdStats, exit, err = mStatus_UnknownErr);

    domain = [[NSString alloc] initWithUTF8String:inDomain];
    require_action_quiet(domain, exit, err = mStatus_UnknownErr);

    awdStats.domain      = domain;
    awdStats.networkType = inForCell ? AWDDNSDomainStats_NetworkType_Cellular : AWDDNSDomainStats_NetworkType_NonCellular;
    awdStats.recordType  = inType;

    totalAnswered = 0;
    for (i = 0; i < kQueryStatsSendCountBinCount; ++i)
    {
        sendCountBins[i] = inHist->answeredQuerySendCountBins[i];
        totalAnswered   += inHist->answeredQuerySendCountBins[i];
    }
    [awdStats setAnsweredQuerySendCounts:sendCountBins count:kQueryStatsSendCountBinCount];

    totalNegAnswered = 0;
    for (i = 0; i < kQueryStatsSendCountBinCount; ++i)
    {
        sendCountBins[i]  = inHist->negAnsweredQuerySendCountBins[i];
        totalNegAnswered += inHist->negAnsweredQuerySendCountBins[i];
    }
    [awdStats setNegAnsweredQuerySendCounts:sendCountBins count:kQueryStatsSendCountBinCount];

    totalUnanswered = 0;
    for (i = 0; i < kQueryStatsSendCountBinCount; ++i)
    {
        sendCountBins[i] = inHist->unansweredQuerySendCountBins[i];
        totalUnanswered += inHist->unansweredQuerySendCountBins[i];
    }
    [awdStats setUnansweredQuerySendCounts:sendCountBins count:kQueryStatsSendCountBinCount];

    if (totalAnswered > inHist->answeredQuerySendCountBins[0])
    {
        for (i = 0; i < kQueryStatsLatencyBinCount; ++i)
        {
            latencyBins[i] = inHist->responseLatencyBins[i];
        }
        [awdStats setResponseLatencyMs:latencyBins count:kQueryStatsLatencyBinCount];
    }

    if (totalNegAnswered > inHist->negAnsweredQuerySendCountBins[0])
    {
        for (i = 0; i < kQueryStatsLatencyBinCount; ++i)
        {
            latencyBins[i] = inHist->negResponseLatencyBins[i];
        }
        [awdStats setNegResponseLatencyMs:latencyBins count:kQueryStatsLatencyBinCount];
    }

    if (totalUnanswered > 0)
    {
        for (i = 0; i < kQueryStatsLatencyBinCount; ++i)
        {
            latencyBins[i] = inHist->unansweredQueryDurationBins[i];
        }
        [awdStats setUnansweredQueryDurationMs:latencyBins count:kQueryStatsLatencyBinCount];
    }

    *outStats = awdStats;
    awdStats = nil;
    err = mStatus_NoError;

exit:
    [domain release];
    [awdStats release];
    return (err);
}

//===========================================================================================================================
//  AddAWDDNSDomainStats
//===========================================================================================================================

mDNSlocal mStatus AddAWDDNSDomainStats(AWDMDNSResponderDNSStatistics *inMetric, DNSHistSet *inSet, const char *inDomain, mDNSBool inForCell)
{
    mStatus                 err;
    AWDDNSDomainStats *     awdStats;

    if (inSet->histAny)
    {
        err = CreateAWDDNSDomainStats(inSet->histAny, inDomain, inForCell, AWDDNSDomainStats_RecordType_Any, &awdStats);
        require_noerr_quiet(err, exit);

        [inMetric addStats:awdStats];
        [awdStats release];
    }
    if (inSet->histA)
    {
        err = CreateAWDDNSDomainStats(inSet->histA, inDomain, inForCell, AWDDNSDomainStats_RecordType_A, &awdStats);
        require_noerr_quiet(err, exit);

        [inMetric addStats:awdStats];
        [awdStats release];
    }
    if (inSet->histAAAA)
    {
        err = CreateAWDDNSDomainStats(inSet->histAAAA, inDomain, inForCell, AWDDNSDomainStats_RecordType_AAAA, &awdStats);
        require_noerr_quiet(err, exit);

        [inMetric addStats:awdStats];
        [awdStats release];
    }
    err = mStatus_NoError;

exit:
    return (err);
}

//===========================================================================================================================
//  LogDNSHistSet
//===========================================================================================================================

mDNSlocal void LogDNSHistSet(const DNSHistSet *inSet, const char *inDomain, mDNSBool inForCell)
{
    if (inSet->histAny)     LogDNSHist(inSet->histAny,  inDomain, inForCell, "Any");
    if (inSet->histA)       LogDNSHist(inSet->histA,    inDomain, inForCell, "A");
    if (inSet->histAAAA)    LogDNSHist(inSet->histAAAA, inDomain, inForCell, "AAAA");
}

//===========================================================================================================================
//  LogDNSHist
//===========================================================================================================================

#define Percent(N, D)       ((N) * 100) / (D), (((N) * 10000) / (D)) % 100
#define PercentFmt          "%3u.%02u"
#define LogStat(LABEL, COUNT, ACCUMULATOR, TOTAL) \
    LogMsgNoIdent("%s %5u " PercentFmt " " PercentFmt, (LABEL), (COUNT), Percent(COUNT, TOTAL), Percent(ACCUMULATOR, TOTAL))

mDNSlocal void LogDNSHist(const DNSHist *inHist, const char *inDomain, mDNSBool inForCell, const char *inType)
{
    unsigned int        totalAnswered;
    unsigned int        totalNegAnswered;
    unsigned int        totalUnanswered;
    int                 i;

    totalAnswered = 0;
    for (i = 0; i < kQueryStatsSendCountBinCount; ++i)
    {
        totalAnswered += inHist->answeredQuerySendCountBins[i];
    }

    totalNegAnswered = 0;
    for (i = 0; i < kQueryStatsSendCountBinCount; ++i)
    {
        totalNegAnswered += inHist->negAnsweredQuerySendCountBins[i];
    }

    totalUnanswered = 0;
    for (i = 0; i < kQueryStatsSendCountBinCount; ++i)
    {
        totalUnanswered += inHist->unansweredQuerySendCountBins[i];
    }

    LogMsgNoIdent("Domain: %s (%s, %s)", inDomain, inForCell ? "C" : "NC", inType);
    LogMsgNoIdent("Answered questions            %4u", totalAnswered);
    LogMsgNoIdent("Negatively answered questions %4u", totalNegAnswered);
    LogMsgNoIdent("Unanswered questions          %4u", totalUnanswered);
    LogMsgNoIdent("-- Query send counts ---------");
    LogDNSHistSendCounts(inHist->answeredQuerySendCountBins);
    LogMsgNoIdent("-- Query send counts (NAQs) --");
    LogDNSHistSendCounts(inHist->negAnsweredQuerySendCountBins);

    if (totalAnswered > inHist->answeredQuerySendCountBins[0])
    {
        LogMsgNoIdent("--- Response times -----------");
        LogDNSHistLatencies(inHist->responseLatencyBins);
    }

    if (totalNegAnswered > inHist->negAnsweredQuerySendCountBins[0])
    {
        LogMsgNoIdent("--- Response times (NAQs) ----");
        LogDNSHistLatencies(inHist->negResponseLatencyBins);
    }

    if (totalUnanswered > 0)
    {
        LogMsgNoIdent("--- Unanswered query times ---");
        LogDNSHistLatencies(inHist->unansweredQueryDurationBins);
    }
}

//===========================================================================================================================
//  LogDNSHistSendCounts
//===========================================================================================================================

mDNSlocal void LogDNSHistSendCounts(const uint16_t inSendCountBins[kQueryStatsSendCountBinCount])
{
    uint32_t        total;
    char            label[16];
    int             i;

    total = 0;
    for (i = 0; i < kQueryStatsSendCountBinCount; ++i)
    {
        total += inSendCountBins[i];
    }

    if (total > 0)
    {
        uint32_t        accumulator = 0;

        for (i = 0; i < kQueryStatsSendCountBinCount; ++i)
        {
            accumulator += inSendCountBins[i];
            if (i < (kQueryStatsSendCountBinCount - 1))
            {
                snprintf(label, sizeof(label), "%2d ", i);
            }
            else
            {
                snprintf(label, sizeof(label), "%2d+", i);
            }
            LogStat(label, inSendCountBins[i], accumulator, total);
            if (accumulator == total) break;
        }
    }
    else
    {
        LogMsgNoIdent("No data.");
    }
}

//===========================================================================================================================
//  LogDNSHistLatencies
//===========================================================================================================================

mDNSlocal void LogDNSHistLatencies(const uint16_t inLatencyBins[kQueryStatsLatencyBinCount])
{
    uint32_t        total;
    int             i;
    char            label[16];

    total = 0;
    for (i = 0; i < kQueryStatsLatencyBinCount; ++i)
    {
        total += inLatencyBins[i];
    }

    if (total > 0)
    {
        uint32_t        accumulator = 0;

        for (i = 0; i < kQueryStatsLatencyBinCount; ++i)
        {
            accumulator += inLatencyBins[i];
            if (i < (int)countof(kResponseLatencyMsLimits))
            {
                snprintf(label, sizeof(label), "< %5u ms", kResponseLatencyMsLimits[i]);
            }
            else
            {
                snprintf(label, sizeof(label), "<     ∞ ms");
            }
            LogStat(label, inLatencyBins[i], accumulator, total);
            if (accumulator == total) break;
        }
    }
    else
    {
        LogMsgNoIdent("No data.");
    }
}

//===========================================================================================================================
//  ValidateDNSStatsDomains
//===========================================================================================================================

#if (METRICS_VALIDATE_DNS_STATS_DOMAINS)
#warning "Do not include ValidateDNSStatsDomains() in customer release!"
mDNSlocal void ValidateDNSStatsDomains(void)
{
    int                 i;
    const Domain *      domain;
    mDNSu8 *            ptr;
    domainname          domainNameExpected;
    int                 labelCountExpected;
    mDNSBool            domainNamesEqual;
    mDNSBool            failed = mDNSfalse;

    for (i = 0; i < countof(kQueryStatsDomains); ++i)
    {
        domain = &kQueryStatsDomains[i];

        if (strcmp(domain->cstr, ".") == 0)
        {
            domainNameExpected.c[0] = 0;
        }
        else
        {
            ptr = MakeDomainNameFromDNSNameString(&domainNameExpected, domain->cstr);
            if (!ptr)
            {
                LogMsg("ValidateDNSStatsDomains: Failed to make domain name for \"%s\".", domain->cstr);
                failed = mDNStrue;
                goto exit;
            }
        }

        domainNamesEqual = SameDomainName(domain->name, &domainNameExpected);
        labelCountExpected = CountLabels(&domainNameExpected);
        if (domainNamesEqual && (domain->labelCount == labelCountExpected))
        {
            LogMsg("ValidateDNSStatsDomains: \"%s\" passed.", domain->cstr);
        }
        else
        {
            if (!domainNamesEqual)
            {
                LogMsg("ValidateDNSStatsDomains: \"%s\" failed: incorrect domain name.", domain->cstr);
            }
            if (domain->labelCount != labelCountExpected)
            {
                LogMsg("ValidateDNSStatsDomains: \"%s\" failed: incorrect label count. Actual %d, expected %d.",
                    domain->cstr, domain->labelCount, labelCountExpected);
            }
            failed = mDNStrue;
        }
    }

exit:
    if (failed) abort();
}
#endif
#endif // TARGET_OS_EMBEDDED
