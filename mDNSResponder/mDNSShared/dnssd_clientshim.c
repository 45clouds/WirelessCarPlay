/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

 * This file defines a simple shim layer between a client calling the "/usr/include/dns_sd.h" APIs
 * and an implementation of mDNSCore ("mDNSEmbeddedAPI.h" APIs) in the same address space.
 * When the client calls a dns_sd.h function, the shim calls the corresponding mDNSEmbeddedAPI.h
 * function, and when mDNSCore calls the shim's callback, we call through to the client's callback.
 * The shim is responsible for two main things:
 * - converting string parameters between C string format and native DNS format,
 * - and for allocating and freeing memory.
 */

#include "dns_sd.h"             // Defines the interface to the client layer above
#include "mDNSEmbeddedAPI.h"        // The interface we're building on top of
extern mDNS mDNSStorage;        // We need to pass the address of this storage to the lower-layer functions

#if MDNS_BUILDINGSHAREDLIBRARY || MDNS_BUILDINGSTUBLIBRARY
#pragma export on
#endif

//*************************************************************************************************************
// General Utility Functions

// All mDNS_DirectOP structures start with the pointer to the type-specific disposal function.
// Optional type-specific data follows these three fields
// When the client starts an operation, we return the address of the corresponding mDNS_DirectOP
// as the DNSServiceRef for the operation
// We stash the value in core context fields so we can get it back to recover our state in our callbacks,
// and pass it though to the client for it to recover its state

typedef struct mDNS_DirectOP_struct mDNS_DirectOP;
typedef void mDNS_DirectOP_Dispose (mDNS_DirectOP *op);
struct mDNS_DirectOP_struct
{
    mDNS_DirectOP_Dispose  *disposefn;
};

typedef struct
{
    mDNS_DirectOP_Dispose  *disposefn;
    DNSServiceRegisterReply callback;
    void                   *context;
    mDNSBool autoname;                      // Set if this name is tied to the Computer Name
    mDNSBool autorename;                    // Set if we just got a name conflict and now need to automatically pick a new name
    domainlabel name;
    domainname host;
    ServiceRecordSet s;
} mDNS_DirectOP_Register;

typedef struct
{
    mDNS_DirectOP_Dispose  *disposefn;
    DNSServiceBrowseReply callback;
    void                   *context;
    DNSQuestion q;
} mDNS_DirectOP_Browse;

typedef struct
{
    mDNS_DirectOP_Dispose  *disposefn;
    DNSServiceResolveReply callback;
    void                   *context;
    const ResourceRecord   *SRV;
    const ResourceRecord   *TXT;
    DNSQuestion qSRV;
    DNSQuestion qTXT;
} mDNS_DirectOP_Resolve;

typedef struct
{
    mDNS_DirectOP_Dispose      *disposefn;
    DNSServiceQueryRecordReply callback;
    void                       *context;
    DNSQuestion q;
} mDNS_DirectOP_QueryRecord;

dnssd_sock_t DNSServiceRefSockFD(DNSServiceRef sdRef)
{
    (void)sdRef;    // Unused
    return(0);
}

DNSServiceErrorType DNSServiceProcessResult(DNSServiceRef sdRef)
{
    (void)sdRef;    // Unused
    return(kDNSServiceErr_NoError);
}

void DNSServiceRefDeallocate(DNSServiceRef sdRef)
{
    mDNS_DirectOP *op = (mDNS_DirectOP *)sdRef;
    //LogMsg("DNSServiceRefDeallocate");
    op->disposefn(op);
}

//*************************************************************************************************************
// Domain Enumeration

// Not yet implemented, so don't include in stub library
// We DO include it in the actual Extension, so that if a later client compiled to use this
// is run against this Extension, it will get a reasonable error code instead of just
// failing to launch (Strong Link) or calling an unresolved symbol and crashing (Weak Link)
#if !MDNS_BUILDINGSTUBLIBRARY
DNSServiceErrorType DNSServiceEnumerateDomains
(
    DNSServiceRef                       *sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    DNSServiceDomainEnumReply callback,
    void                                *context  /* may be NULL */
)
{
    (void)sdRef;            // Unused
    (void)flags;            // Unused
    (void)interfaceIndex;   // Unused
    (void)callback;         // Unused
    (void)context;          // Unused
    return(kDNSServiceErr_Unsupported);
}
#endif

//*************************************************************************************************************
// Register Service

mDNSlocal void FreeDNSServiceRegistration(mDNS_DirectOP_Register *x)
{
    while (x->s.Extras)
    {
        ExtraResourceRecord *extras = x->s.Extras;
        x->s.Extras = x->s.Extras->next;
        if (extras->r.resrec.rdata != &extras->r.rdatastorage)
            mDNSPlatformMemFree(extras->r.resrec.rdata);
        mDNSPlatformMemFree(extras);
    }

    if (x->s.RR_TXT.resrec.rdata != &x->s.RR_TXT.rdatastorage)
        mDNSPlatformMemFree(x->s.RR_TXT.resrec.rdata);

    if (x->s.SubTypes) mDNSPlatformMemFree(x->s.SubTypes);

    mDNSPlatformMemFree(x);
}

static void DNSServiceRegisterDispose(mDNS_DirectOP *op)
{
    mDNS_DirectOP_Register *x = (mDNS_DirectOP_Register*)op;
    x->autorename = mDNSfalse;
    // If mDNS_DeregisterService() returns mStatus_NoError, that means that the service was found in the list,
    // is sending its goodbye packet, and we'll get an mStatus_MemFree message when we can free the memory.
    // If mDNS_DeregisterService() returns an error, it means that the service had already been removed from
    // the list, so we should go ahead and free the memory right now
    if (mDNS_DeregisterService(&mDNSStorage, &x->s) != mStatus_NoError)
        FreeDNSServiceRegistration(x);
}

mDNSlocal void RegCallback(mDNS *const m, ServiceRecordSet *const sr, mStatus result)
{
    mDNS_DirectOP_Register *x = (mDNS_DirectOP_Register*)sr->ServiceContext;

    domainlabel name;
    domainname type, dom;
    char namestr[MAX_DOMAIN_LABEL+1];       // Unescaped name: up to 63 bytes plus C-string terminating NULL.
    char typestr[MAX_ESCAPED_DOMAIN_NAME];
    char domstr [MAX_ESCAPED_DOMAIN_NAME];
    if (!DeconstructServiceName(sr->RR_SRV.resrec.name, &name, &type, &dom)) return;
    if (!ConvertDomainLabelToCString_unescaped(&name, namestr)) return;
    if (!ConvertDomainNameToCString(&type, typestr)) return;
    if (!ConvertDomainNameToCString(&dom, domstr)) return;

    if (result == mStatus_NoError)
    {
        if (x->callback)
            x->callback((DNSServiceRef)x, 0, result, namestr, typestr, domstr, x->context);
    }
    else if (result == mStatus_NameConflict)
    {
        if (x->autoname) mDNS_RenameAndReregisterService(m, sr, mDNSNULL);
        else if (x->callback)
            x->callback((DNSServiceRef)x, 0, result, namestr, typestr, domstr, x->context);
    }
    else if (result == mStatus_MemFree)
    {
        if (x->autorename)
        {
            x->autorename = mDNSfalse;
            x->name = mDNSStorage.nicelabel;
            mDNS_RenameAndReregisterService(m, &x->s, &x->name);
        }
        else
            FreeDNSServiceRegistration(x);
    }
}

DNSServiceErrorType DNSServiceRegister
(
    DNSServiceRef                       *sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    const char                          *name,         /* may be NULL */
    const char                          *regtype,
    const char                          *domain,       /* may be NULL */
    const char                          *host,         /* may be NULL */
    uint16_t notAnIntPort,
    uint16_t txtLen,
    const void                          *txtRecord,    /* may be NULL */
    DNSServiceRegisterReply callback,                  /* may be NULL */
    void                                *context       /* may be NULL */
)
{
    mStatus err = mStatus_NoError;
    const char *errormsg = "Unknown";
    domainlabel n;
    domainname t, d, h, srv;
    mDNSIPPort port;
    unsigned int size = sizeof(RDataBody);
    AuthRecord *SubTypes = mDNSNULL;
    mDNSu32 NumSubTypes = 0;
    mDNS_DirectOP_Register *x;
    (void)flags;            // Unused
    (void)interfaceIndex;   // Unused

    // Check parameters
    if (!name) name = "";
    if (!name[0]) n = mDNSStorage.nicelabel;
    else if (!MakeDomainLabelFromLiteralString(&n, name))                              { errormsg = "Bad Instance Name"; goto badparam; }
    if (!regtype || !*regtype || !MakeDomainNameFromDNSNameString(&t, regtype))        { errormsg = "Bad Service Type";  goto badparam; }
    if (!MakeDomainNameFromDNSNameString(&d, (domain && *domain) ? domain : "local.")) { errormsg = "Bad Domain";        goto badparam; }
    if (!MakeDomainNameFromDNSNameString(&h, (host   && *host  ) ? host   : ""))       { errormsg = "Bad Target Host";   goto badparam; }
    if (!ConstructServiceName(&srv, &n, &t, &d))                                       { errormsg = "Bad Name";          goto badparam; }
    port.NotAnInteger = notAnIntPort;

    // Allocate memory, and handle failure
    if (size < txtLen)
        size = txtLen;
    x = (mDNS_DirectOP_Register *)mDNSPlatformMemAllocate(sizeof(*x) - sizeof(RDataBody) + size);
    if (!x) { err = mStatus_NoMemoryErr; errormsg = "No memory"; goto fail; }

    // Set up object
    x->disposefn = DNSServiceRegisterDispose;
    x->callback  = callback;
    x->context   = context;
    x->autoname = (!name[0]);
    x->autorename = mDNSfalse;
    x->name = n;
    x->host = h;

    // Do the operation
    err = mDNS_RegisterService(&mDNSStorage, &x->s,
                               &x->name, &t, &d, // Name, type, domain
                               &x->host, port, // Host and port
                               txtRecord, txtLen, // TXT data, length
                               SubTypes, NumSubTypes, // Subtypes
                               mDNSInterface_Any, // Interface ID
                               RegCallback, x, 0); // Callback, context, flags
    if (err) { mDNSPlatformMemFree(x); errormsg = "mDNS_RegisterService"; goto fail; }

    // Succeeded: Wrap up and return
    *sdRef = (DNSServiceRef)x;
    return(mStatus_NoError);

badparam:
    err = mStatus_BadParamErr;
fail:
    LogMsg("DNSServiceBrowse(\"%s\", \"%s\") failed: %s (%ld)", regtype, domain, errormsg, err);
    return(err);
}

//*************************************************************************************************************
// Add / Update / Remove records from existing Registration

// Not yet implemented, so don't include in stub library
// We DO include it in the actual Extension, so that if a later client compiled to use this
// is run against this Extension, it will get a reasonable error code instead of just
// failing to launch (Strong Link) or calling an unresolved symbol and crashing (Weak Link)
#if !MDNS_BUILDINGSTUBLIBRARY
DNSServiceErrorType DNSServiceAddRecord
(
    DNSServiceRef sdRef,
    DNSRecordRef                        *RecordRef,
    DNSServiceFlags flags,
    uint16_t rrtype,
    uint16_t rdlen,
    const void                          *rdata,
    uint32_t ttl
)
{
    (void)sdRef;        // Unused
    (void)RecordRef;    // Unused
    (void)flags;        // Unused
    (void)rrtype;       // Unused
    (void)rdlen;        // Unused
    (void)rdata;        // Unused
    (void)ttl;          // Unused
    return(kDNSServiceErr_Unsupported);
}

DNSServiceErrorType DNSServiceUpdateRecord
(
    DNSServiceRef sdRef,
    DNSRecordRef RecordRef,                            /* may be NULL */
    DNSServiceFlags flags,
    uint16_t rdlen,
    const void                          *rdata,
    uint32_t ttl
)
{
    (void)sdRef;        // Unused
    (void)RecordRef;    // Unused
    (void)flags;        // Unused
    (void)rdlen;        // Unused
    (void)rdata;        // Unused
    (void)ttl;          // Unused
    return(kDNSServiceErr_Unsupported);
}

DNSServiceErrorType DNSServiceRemoveRecord
(
    DNSServiceRef sdRef,
    DNSRecordRef RecordRef,
    DNSServiceFlags flags
)
{
    (void)sdRef;        // Unused
    (void)RecordRef;    // Unused
    (void)flags;        // Unused
    return(kDNSServiceErr_Unsupported);
}
#endif

//*************************************************************************************************************
// Browse for services

static void DNSServiceBrowseDispose(mDNS_DirectOP *op)
{
    mDNS_DirectOP_Browse *x = (mDNS_DirectOP_Browse*)op;
    //LogMsg("DNSServiceBrowseDispose");
    mDNS_StopBrowse(&mDNSStorage, &x->q);
    mDNSPlatformMemFree(x);
}

mDNSlocal void FoundInstance(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
{
    DNSServiceFlags flags = AddRecord ? kDNSServiceFlagsAdd : (DNSServiceFlags)0;
    domainlabel name;
    domainname type, domain;
    char cname[MAX_DOMAIN_LABEL+1];         // Unescaped name: up to 63 bytes plus C-string terminating NULL.
    char ctype[MAX_ESCAPED_DOMAIN_NAME];
    char cdom [MAX_ESCAPED_DOMAIN_NAME];
    mDNS_DirectOP_Browse *x = (mDNS_DirectOP_Browse*)question->QuestionContext;
    (void)m;        // Unused

    if (answer->rrtype != kDNSType_PTR)
    { LogMsg("FoundInstance: Should not be called with rrtype %d (not a PTR record)", answer->rrtype); return; }

    if (!DeconstructServiceName(&answer->rdata->u.name, &name, &type, &domain))
    {
        LogMsg("FoundInstance: %##s PTR %##s received from network is not valid DNS-SD service pointer",
               answer->name->c, answer->rdata->u.name.c);
        return;
    }

    ConvertDomainLabelToCString_unescaped(&name, cname);
    ConvertDomainNameToCString(&type, ctype);
    ConvertDomainNameToCString(&domain, cdom);
    if (x->callback)
        x->callback((DNSServiceRef)x, flags, 0, 0, cname, ctype, cdom, x->context);
}

DNSServiceErrorType DNSServiceBrowse
(
    DNSServiceRef                       *sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    const char                          *regtype,
    const char                          *domain,    /* may be NULL */
    DNSServiceBrowseReply callback,
    void                                *context    /* may be NULL */
)
{
    mStatus err = mStatus_NoError;
    const char *errormsg = "Unknown";
    domainname t, d;
    mDNS_DirectOP_Browse *x;
    (void)flags;            // Unused
    (void)interfaceIndex;   // Unused

    // Check parameters
    if (!regtype[0] || !MakeDomainNameFromDNSNameString(&t, regtype))      { errormsg = "Illegal regtype"; goto badparam; }
    if (!MakeDomainNameFromDNSNameString(&d, *domain ? domain : "local.")) { errormsg = "Illegal domain";  goto badparam; }

    // Allocate memory, and handle failure
    x = (mDNS_DirectOP_Browse *)mDNSPlatformMemAllocate(sizeof(*x));
    if (!x) { err = mStatus_NoMemoryErr; errormsg = "No memory"; goto fail; }

    // Set up object
    x->disposefn = DNSServiceBrowseDispose;
    x->callback  = callback;
    x->context   = context;
    x->q.QuestionContext = x;

    // Do the operation
    err = mDNS_StartBrowse(&mDNSStorage, &x->q, &t, &d, mDNSNULL, mDNSInterface_Any, flags, (flags & kDNSServiceFlagsForceMulticast) != 0, (flags & kDNSServiceFlagsBackgroundTrafficClass) != 0, FoundInstance, x);
    if (err) { mDNSPlatformMemFree(x); errormsg = "mDNS_StartBrowse"; goto fail; }

    // Succeeded: Wrap up and return
    *sdRef = (DNSServiceRef)x;
    return(mStatus_NoError);

badparam:
    err = mStatus_BadParamErr;
fail:
    LogMsg("DNSServiceBrowse(\"%s\", \"%s\") failed: %s (%ld)", regtype, domain, errormsg, err);
    return(err);
}

//*************************************************************************************************************
// Resolve Service Info

static void DNSServiceResolveDispose(mDNS_DirectOP *op)
{
    mDNS_DirectOP_Resolve *x = (mDNS_DirectOP_Resolve*)op;
    if (x->qSRV.ThisQInterval >= 0) mDNS_StopQuery(&mDNSStorage, &x->qSRV);
    if (x->qTXT.ThisQInterval >= 0) mDNS_StopQuery(&mDNSStorage, &x->qTXT);
    mDNSPlatformMemFree(x);
}

mDNSlocal void FoundServiceInfo(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
{
    mDNS_DirectOP_Resolve *x = (mDNS_DirectOP_Resolve*)question->QuestionContext;
    (void)m;    // Unused
    if (!AddRecord)
    {
        if (answer->rrtype == kDNSType_SRV && x->SRV == answer) x->SRV = mDNSNULL;
        if (answer->rrtype == kDNSType_TXT && x->TXT == answer) x->TXT = mDNSNULL;
    }
    else
    {
        if (answer->rrtype == kDNSType_SRV) x->SRV = answer;
        if (answer->rrtype == kDNSType_TXT) x->TXT = answer;
        if (x->SRV && x->TXT && x->callback)
        {
            char fullname[MAX_ESCAPED_DOMAIN_NAME], targethost[MAX_ESCAPED_DOMAIN_NAME];
            ConvertDomainNameToCString(answer->name, fullname);
            ConvertDomainNameToCString(&x->SRV->rdata->u.srv.target, targethost);
            x->callback((DNSServiceRef)x, 0, 0, kDNSServiceErr_NoError, fullname, targethost,
                        x->SRV->rdata->u.srv.port.NotAnInteger, x->TXT->rdlength, (unsigned char*)x->TXT->rdata->u.txt.c, x->context);
        }
    }
}

DNSServiceErrorType DNSServiceResolve
(
    DNSServiceRef                       *sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    const char                          *name,
    const char                          *regtype,
    const char                          *domain,
    DNSServiceResolveReply callback,
    void                                *context  /* may be NULL */
)
{
    mStatus err = mStatus_NoError;
    const char *errormsg = "Unknown";
    domainlabel n;
    domainname t, d, srv;
    mDNS_DirectOP_Resolve *x;

    (void)flags;            // Unused
    (void)interfaceIndex;   // Unused

    // Check parameters
    if (!name[0]    || !MakeDomainLabelFromLiteralString(&n, name  )) { errormsg = "Bad Instance Name"; goto badparam; }
    if (!regtype[0] || !MakeDomainNameFromDNSNameString(&t, regtype)) { errormsg = "Bad Service Type";  goto badparam; }
    if (!domain[0]  || !MakeDomainNameFromDNSNameString(&d, domain )) { errormsg = "Bad Domain";        goto badparam; }
    if (!ConstructServiceName(&srv, &n, &t, &d))                      { errormsg = "Bad Name";          goto badparam; }

    // Allocate memory, and handle failure
    x = (mDNS_DirectOP_Resolve *)mDNSPlatformMemAllocate(sizeof(*x));
    if (!x) { err = mStatus_NoMemoryErr; errormsg = "No memory"; goto fail; }

    // Set up object
    x->disposefn = DNSServiceResolveDispose;
    x->callback  = callback;
    x->context   = context;
    x->SRV       = mDNSNULL;
    x->TXT       = mDNSNULL;

    x->qSRV.ThisQInterval       = -1;       // So that DNSServiceResolveDispose() knows whether to cancel this question
    x->qSRV.InterfaceID         = mDNSInterface_Any;
    x->qSRV.flags               = 0;
    x->qSRV.Target              = zeroAddr;
    AssignDomainName(&x->qSRV.qname, &srv);
    x->qSRV.qtype               = kDNSType_SRV;
    x->qSRV.qclass              = kDNSClass_IN;
    x->qSRV.LongLived           = mDNSfalse;
    x->qSRV.ExpectUnique        = mDNStrue;
    x->qSRV.ForceMCast          = mDNSfalse;
    x->qSRV.ReturnIntermed      = mDNSfalse;
    x->qSRV.SuppressUnusable    = mDNSfalse;
    x->qSRV.SearchListIndex     = 0;
    x->qSRV.AppendSearchDomains = 0;
    x->qSRV.RetryWithSearchDomains = mDNSfalse;
    x->qSRV.TimeoutQuestion     = 0;
    x->qSRV.WakeOnResolve       = 0;
    x->qSRV.UseBackgroundTrafficClass = (flags & kDNSServiceFlagsBackgroundTrafficClass) != 0;
    x->qSRV.ValidationRequired  = 0;
    x->qSRV.ValidatingResponse  = 0;
    x->qSRV.ProxyQuestion       = 0;
    x->qSRV.qnameOrig           = mDNSNULL;
    x->qSRV.AnonInfo            = mDNSNULL;
    x->qSRV.pid                 = mDNSPlatformGetPID();
    x->qSRV.QuestionCallback    = FoundServiceInfo;
    x->qSRV.QuestionContext     = x;

    x->qTXT.ThisQInterval       = -1;       // So that DNSServiceResolveDispose() knows whether to cancel this question
    x->qTXT.InterfaceID         = mDNSInterface_Any;
    x->qTXT.flags               = 0;
    x->qTXT.Target              = zeroAddr;
    AssignDomainName(&x->qTXT.qname, &srv);
    x->qTXT.qtype               = kDNSType_TXT;
    x->qTXT.qclass              = kDNSClass_IN;
    x->qTXT.LongLived           = mDNSfalse;
    x->qTXT.ExpectUnique        = mDNStrue;
    x->qTXT.ForceMCast          = mDNSfalse;
    x->qTXT.ReturnIntermed      = mDNSfalse;
    x->qTXT.SuppressUnusable    = mDNSfalse;
    x->qTXT.SearchListIndex     = 0;
    x->qTXT.AppendSearchDomains = 0;
    x->qTXT.RetryWithSearchDomains = mDNSfalse;
    x->qTXT.TimeoutQuestion     = 0;
    x->qTXT.WakeOnResolve       = 0;
    x->qTXT.UseBackgroundTrafficClass = (flags & kDNSServiceFlagsBackgroundTrafficClass) != 0;
    x->qTXT.ValidationRequired  = 0;
    x->qTXT.ValidatingResponse  = 0;
    x->qTXT.ProxyQuestion       = 0;
    x->qTXT.qnameOrig           = mDNSNULL;
    x->qTXT.AnonInfo            = mDNSNULL;
    x->qTXT.pid                 = mDNSPlatformGetPID();
    x->qTXT.QuestionCallback    = FoundServiceInfo;
    x->qTXT.QuestionContext     = x;

    err = mDNS_StartQuery(&mDNSStorage, &x->qSRV);
    if (err) { DNSServiceResolveDispose((mDNS_DirectOP*)x); errormsg = "mDNS_StartQuery qSRV"; goto fail; }
    err = mDNS_StartQuery(&mDNSStorage, &x->qTXT);
    if (err) { DNSServiceResolveDispose((mDNS_DirectOP*)x); errormsg = "mDNS_StartQuery qTXT"; goto fail; }

    // Succeeded: Wrap up and return
    *sdRef = (DNSServiceRef)x;
    return(mStatus_NoError);

badparam:
    err = mStatus_BadParamErr;
fail:
    LogMsg("DNSServiceResolve(\"%s\", \"%s\", \"%s\") failed: %s (%ld)", name, regtype, domain, errormsg, err);
    return(err);
}

//*************************************************************************************************************
// Connection-oriented calls

// Not yet implemented, so don't include in stub library
// We DO include it in the actual Extension, so that if a later client compiled to use this
// is run against this Extension, it will get a reasonable error code instead of just
// failing to launch (Strong Link) or calling an unresolved symbol and crashing (Weak Link)
#if !MDNS_BUILDINGSTUBLIBRARY
DNSServiceErrorType DNSServiceCreateConnection(DNSServiceRef *sdRef)
{
    (void)sdRef;    // Unused
    return(kDNSServiceErr_Unsupported);
}

DNSServiceErrorType DNSServiceRegisterRecord
(
    DNSServiceRef sdRef,
    DNSRecordRef                        *RecordRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    const char                          *fullname,
    uint16_t rrtype,
    uint16_t rrclass,
    uint16_t rdlen,
    const void                          *rdata,
    uint32_t ttl,
    DNSServiceRegisterRecordReply callback,
    void                                *context    /* may be NULL */
)
{
    (void)sdRef;            // Unused
    (void)RecordRef;        // Unused
    (void)flags;            // Unused
    (void)interfaceIndex;   // Unused
    (void)fullname;         // Unused
    (void)rrtype;           // Unused
    (void)rrclass;          // Unused
    (void)rdlen;            // Unused
    (void)rdata;            // Unused
    (void)ttl;              // Unused
    (void)callback;         // Unused
    (void)context;          // Unused
    return(kDNSServiceErr_Unsupported);
}
#endif

//*************************************************************************************************************
// DNSServiceQueryRecord

static void DNSServiceQueryRecordDispose(mDNS_DirectOP *op)
{
    mDNS_DirectOP_QueryRecord *x = (mDNS_DirectOP_QueryRecord*)op;
    if (x->q.ThisQInterval >= 0) mDNS_StopQuery(&mDNSStorage, &x->q);
    mDNSPlatformMemFree(x);
}

mDNSlocal void DNSServiceQueryRecordResponse(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, QC_result AddRecord)
{
    mDNS_DirectOP_QueryRecord *x = (mDNS_DirectOP_QueryRecord*)question->QuestionContext;
    char fullname[MAX_ESCAPED_DOMAIN_NAME];
    (void)m;    // Unused
    ConvertDomainNameToCString(answer->name, fullname);
    x->callback((DNSServiceRef)x, AddRecord ? kDNSServiceFlagsAdd : (DNSServiceFlags)0, 0, kDNSServiceErr_NoError,
                fullname, answer->rrtype, answer->rrclass, answer->rdlength, answer->rdata->u.data, answer->rroriginalttl, x->context);
}

DNSServiceErrorType DNSServiceQueryRecord
(
    DNSServiceRef                       *sdRef,
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    const char                          *fullname,
    uint16_t rrtype,
    uint16_t rrclass,
    DNSServiceQueryRecordReply callback,
    void                                *context  /* may be NULL */
)
{
    mStatus err = mStatus_NoError;
    const char *errormsg = "Unknown";
    mDNS_DirectOP_QueryRecord *x;

    (void)flags;            // Unused
    (void)interfaceIndex;   // Unused

    // Allocate memory, and handle failure
    x = (mDNS_DirectOP_QueryRecord *)mDNSPlatformMemAllocate(sizeof(*x));
    if (!x) { err = mStatus_NoMemoryErr; errormsg = "No memory"; goto fail; }

    // Set up object
    x->disposefn = DNSServiceQueryRecordDispose;
    x->callback  = callback;
    x->context   = context;

    x->q.ThisQInterval       = -1;      // So that DNSServiceResolveDispose() knows whether to cancel this question
    x->q.InterfaceID         = mDNSInterface_Any;
    x->q.flags               = flags;
    x->q.Target              = zeroAddr;
    MakeDomainNameFromDNSNameString(&x->q.qname, fullname);
    x->q.qtype               = rrtype;
    x->q.qclass              = rrclass;
    x->q.LongLived           = (flags & kDNSServiceFlagsLongLivedQuery) != 0;
    x->q.ExpectUnique        = mDNSfalse;
    x->q.ForceMCast          = (flags & kDNSServiceFlagsForceMulticast) != 0;
    x->q.ReturnIntermed      = (flags & kDNSServiceFlagsReturnIntermediates) != 0;
    x->q.SuppressUnsable     = (flags & kDNSServiceFlagsSuppressUnusable) != 0;
    x->q.SearchListIndex     = 0;
    x->q.AppendSearchDomains = 0;
    x->q.RetryWithSearchDomains = mDNSfalse;
    x->q.TimeoutQuestion     = 0;
    x->q.WakeOnResolve       = 0;
    x->q.UseBackgroundTrafficClass = (flags & kDNSServiceFlagsBackgroundTrafficClass) != 0;
    x->q.ValidationRequired  = 0;
    x->q.ValidatingResponse  = 0;
    x->q.ProxyQuestion       = 0;
    x->q.qnameOrig           = mDNSNULL;
    x->q.AnonInfo            = mDNSNULL;
    x->q.pid                 = mDNSPlatformGetPID();
    x->q.QuestionCallback    = DNSServiceQueryRecordResponse;
    x->q.QuestionContext     = x;

    err = mDNS_StartQuery(&mDNSStorage, &x->q);
    if (err) { DNSServiceResolveDispose((mDNS_DirectOP*)x); errormsg = "mDNS_StartQuery"; goto fail; }

    // Succeeded: Wrap up and return
    *sdRef = (DNSServiceRef)x;
    return(mStatus_NoError);

fail:
    LogMsg("DNSServiceQueryRecord(\"%s\", %d, %d) failed: %s (%ld)", fullname, rrtype, rrclass, errormsg, err);
    return(err);
}

//*************************************************************************************************************
// DNSServiceGetAddrInfo

static void DNSServiceGetAddrInfoDispose(mDNS_DirectOP *op)
{
    mDNS_DirectOP_GetAddrInfo *x = (mDNS_DirectOP_GetAddrInfo*)op;
    if (x->aQuery) DNSServiceRefDeallocate(x->aQuery);
    mDNSPlatformMemFree(x);
}

static void DNSSD_API DNSServiceGetAddrInfoResponse(
    DNSServiceRef inRef,
    DNSServiceFlags inFlags,
    uint32_t inInterfaceIndex,
    DNSServiceErrorType inErrorCode,
    const char *        inFullName,
    uint16_t inRRType,
    uint16_t inRRClass,
    uint16_t inRDLen,
    const void *        inRData,
    uint32_t inTTL,
    void *              inContext )
{
    mDNS_DirectOP_GetAddrInfo *     x = (mDNS_DirectOP_GetAddrInfo*)inContext;
    struct sockaddr_in sa4;

    mDNSPlatformMemZero(&sa4, sizeof(sa4));
    if (inErrorCode == kDNSServiceErr_NoError && inRRType == kDNSServiceType_A)
    {
        sa4.sin_family = AF_INET;
        mDNSPlatformMemCopy(&sa4.sin_addr.s_addr, inRData, 4);
    }

    x->callback((DNSServiceRef)x, inFlags, inInterfaceIndex, inErrorCode, inFullName,
                (const struct sockaddr *) &sa4, inTTL, x->context);
}

DNSServiceErrorType DNSSD_API DNSServiceGetAddrInfo(
    DNSServiceRef *             outRef,
    DNSServiceFlags inFlags,
    uint32_t inInterfaceIndex,
    DNSServiceProtocol inProtocol,
    const char *                inHostName,
    DNSServiceGetAddrInfoReply inCallback,
    void *                      inContext )
{
    const char *                    errormsg = "Unknown";
    DNSServiceErrorType err;
    mDNS_DirectOP_GetAddrInfo *     x;

    // Allocate memory, and handle failure
    x = (mDNS_DirectOP_GetAddrInfo *)mDNSPlatformMemAllocate(sizeof(*x));
    if (!x) { err = mStatus_NoMemoryErr; errormsg = "No memory"; goto fail; }

    // Set up object
    x->disposefn = DNSServiceGetAddrInfoDispose;
    x->callback  = inCallback;
    x->context   = inContext;
    x->aQuery    = mDNSNULL;

    // Start the query.
    // (It would probably be more efficient to code this using mDNS_StartQuery directly,
    // instead of wrapping DNSServiceQueryRecord, which then unnecessarily allocates
    // more memory and then just calls through to mDNS_StartQuery. -- SC June 2010)
    err = DNSServiceQueryRecord(&x->aQuery, inFlags, inInterfaceIndex, inHostName, kDNSServiceType_A,
                                kDNSServiceClass_IN, DNSServiceGetAddrInfoResponse, x);
    if (err) { DNSServiceGetAddrInfoDispose((mDNS_DirectOP*)x); errormsg = "DNSServiceQueryRecord"; goto fail; }

    *outRef = (DNSServiceRef)x;
    return(mStatus_NoError);

fail:
    LogMsg("DNSServiceGetAddrInfo(\"%s\", %d) failed: %s (%ld)", inHostName, inProtocol, errormsg, err);
    return(err);
}

//*************************************************************************************************************
// DNSServiceReconfirmRecord

// Not yet implemented, so don't include in stub library
// We DO include it in the actual Extension, so that if a later client compiled to use this
// is run against this Extension, it will get a reasonable error code instead of just
// failing to launch (Strong Link) or calling an unresolved symbol and crashing (Weak Link)
#if !MDNS_BUILDINGSTUBLIBRARY
DNSServiceErrorType DNSSD_API DNSServiceReconfirmRecord
(
    DNSServiceFlags flags,
    uint32_t interfaceIndex,
    const char                         *fullname,
    uint16_t rrtype,
    uint16_t rrclass,
    uint16_t rdlen,
    const void                         *rdata
)
{
    (void)flags;            // Unused
    (void)interfaceIndex;   // Unused
    (void)fullname;         // Unused
    (void)rrtype;           // Unused
    (void)rrclass;          // Unused
    (void)rdlen;            // Unused
    (void)rdata;            // Unused
    return(kDNSServiceErr_Unsupported);
}


#endif  // !MDNS_BUILDINGSTUBLIBRARY
