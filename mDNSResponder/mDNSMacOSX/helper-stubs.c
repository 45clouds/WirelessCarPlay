/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2007-2015 Apple Inc. All rights reserved.
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

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/vm_map.h>
#include <servers/bootstrap.h>
#include <IOKit/IOReturn.h>
#include <CoreFoundation/CoreFoundation.h>
#include "mDNSDebug.h"
#include "helper.h"
#include <dispatch/dispatch.h>
#include <arpa/inet.h>
#include <xpc/private.h>
#include <Block.h>

//
// Implementation Notes about the HelperQueue:
//
// To prevent blocking the main queue, all communications with mDNSResponderHelper should happen on
// HelperQueue. There are a few calls which are still synchronous and needs to be handled separately
// case by case.
//
// When spawning off the work to the HelperQueue, any arguments that are pointers need to be copied
// explicitly as they may cease to exist after the call returns. From within the block that is scheduled,
// arrays defined on the stack can't be referenced and hence it is enclosed them in a struct. If the array is
// an argument to the function, the blocks can reference them as they are passed in as pointers. But care should
// be taken to copy them locally as they may cease to exist when the function returns.
//


//*************************************************************************************************************
// Globals
static dispatch_queue_t HelperQueue;
static xpc_connection_t helper_xpc_conn = NULL;

static int64_t maxwait_secs = 5LL;

#define mDNSHELPER_DEBUG LogOperation

//*************************************************************************************************************
// Utility Functions

static void LogDebug(const char *prefix, xpc_object_t o)
{
    char *desc = xpc_copy_description(o);
    mDNSHELPER_DEBUG("LogDebug %s: %s", prefix, desc);
    free(desc);
}

//*************************************************************************************************************
// XPC Funcs:
//*************************************************************************************************************


mDNSlocal void Init_Connection(const char *servname)
{
    helper_xpc_conn = xpc_connection_create_mach_service(servname, HelperQueue, XPC_CONNECTION_MACH_SERVICE_PRIVILEGED);
    
    xpc_connection_set_event_handler(helper_xpc_conn, ^(xpc_object_t event)
    {
        mDNSHELPER_DEBUG("Init_Connection xpc: [%s] \n", xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION));
    });
    
    xpc_connection_resume(helper_xpc_conn);
}

mDNSlocal int SendDict_ToServer(xpc_object_t msg)
{
    __block int errorcode = kHelperErr_NoResponse;
    
    LogDebug("SendDict_ToServer Sending msg to Daemon", msg);
    
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    dispatch_retain(sem); // for the block below
    
    xpc_connection_send_message_with_reply(helper_xpc_conn, msg, HelperQueue, ^(xpc_object_t recv_msg)
    {
        xpc_type_t type = xpc_get_type(recv_msg);
                                               
        if (type == XPC_TYPE_DICTIONARY)
        {
            LogDebug("SendDict_ToServer Received reply msg from Daemon", recv_msg);
            uint64_t reply_status = xpc_dictionary_get_uint64(recv_msg, kHelperReplyStatus);
            errorcode = xpc_dictionary_get_int64(recv_msg, kHelperErrCode);
            
            switch (reply_status)
            {
                case kHelperReply_ACK:
                    mDNSHELPER_DEBUG("NoError: successful reply");
                    break;
                default:
                    LogMsg("default: Unexpected reply from Helper");
                    break;
            }
        }
        else
        {
            LogMsg("SendDict_ToServer Received unexpected reply from daemon [%s]",
                    xpc_dictionary_get_string(recv_msg, XPC_ERROR_KEY_DESCRIPTION));
            LogDebug("SendDict_ToServer Unexpected Reply contents", recv_msg);
        }
        
        dispatch_semaphore_signal(sem);
        dispatch_release(sem);
        
    });
    
    if (dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, (maxwait_secs * NSEC_PER_SEC))) != 0)
        LogMsg("SendDict_ToServer: UNEXPECTED WAIT_TIME in dispatch_semaphore_wait");
    
    dispatch_release(sem);
    
    mDNSHELPER_DEBUG("SendDict_ToServer returning with errorcode[%d]", errorcode);
    
    return errorcode;
}

mDNSlocal xpc_object_t SendDict_GetReply(xpc_object_t msg)
{
    // Create empty dictionary
    __block xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
    if (!dict) return NULL;
    xpc_retain(dict);

    LogDebug("SendDict_GetReply Sending msg to Daemon", msg);
    
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    dispatch_retain(sem); // for the block below
    
    xpc_connection_send_message_with_reply(helper_xpc_conn, msg, HelperQueue, ^(xpc_object_t recv_msg)
    {
        xpc_type_t type = xpc_get_type(recv_msg);
                                               
        if (type == XPC_TYPE_DICTIONARY)
        {
            LogDebug("SendDict_GetReply Received reply msg from Daemon", recv_msg);
            uint64_t reply_status = xpc_dictionary_get_uint64(recv_msg, kHelperReplyStatus);
            
            switch (reply_status)
            {
                case kHelperReply_ACK:
                    mDNSHELPER_DEBUG("NoError: successful reply");
                    break;
                default:
                    LogMsg("default: Unexpected reply from Helper");
                    break;
            }
            // Copy result into dict reply
            xpc_dictionary_apply(recv_msg, ^bool(const char *key, xpc_object_t value)
            {
                xpc_dictionary_set_value(dict, key, value);
                return true;
            });
        }
        else
        {
            LogMsg("SendDict_GetReply Received unexpected reply from daemon [%s]",
                    xpc_dictionary_get_string(recv_msg, XPC_ERROR_KEY_DESCRIPTION));
            LogDebug("SendDict_GetReply Unexpected Reply contents", recv_msg);
        }
        
        dispatch_semaphore_signal(sem);
        dispatch_release(sem);
        xpc_release(dict);

    });
    
    if (dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, (maxwait_secs * NSEC_PER_SEC))) != 0)
    {
        LogMsg("SendDict_GetReply: UNEXPECTED WAIT_TIME in dispatch_semaphore_wait");
        xpc_release(dict);
        dispatch_release(sem);

        return NULL;
    }
    
    dispatch_release(sem);
    
    return dict;
}

//**************************************************************************************************************

mDNSexport mStatus mDNSHelperInit()
{
    HelperQueue = dispatch_queue_create("com.apple.mDNSResponder.HelperQueue", NULL);
    if (HelperQueue == NULL)
    {
        LogMsg("dispatch_queue_create: Helper queue NULL");
        return mStatus_NoMemoryErr;
    }
    return mStatus_NoError;
}

void mDNSPreferencesSetName(int key, domainlabel *old, domainlabel *new)
{
    struct
    {
        char oldname[MAX_DOMAIN_LABEL+1];
        char newname[MAX_DOMAIN_LABEL+1];
    } names;

    mDNSPlatformMemZero(names.oldname, MAX_DOMAIN_LABEL + 1);
    mDNSPlatformMemZero(names.newname, MAX_DOMAIN_LABEL + 1);

    ConvertDomainLabelToCString_unescaped(old, names.oldname);
    
    if (new)
        ConvertDomainLabelToCString_unescaped(new, names.newname);
    
    
    mDNSHELPER_DEBUG("mDNSPreferencesSetName: XPC IPC Test oldname %s newname %s", names.oldname, names.newname);
    Init_Connection(kHelperService);
     
    // Create Dictionary To Send
    xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(dict, kHelperMode, set_name);
    
    xpc_dictionary_set_uint64(dict, kPrefsNameKey, key);
    xpc_dictionary_set_string(dict, kPrefsOldName, names.oldname);
    xpc_dictionary_set_string(dict, kPrefsNewName, names.newname);
    
    SendDict_ToServer(dict);
    xpc_release(dict);
    dict = NULL;
    
}

void mDNSRequestBPF()
{
     mDNSHELPER_DEBUG("mDNSRequestBPF: Using XPC IPC");
     Init_Connection(kHelperService);
     
     // Create Dictionary To Send
     xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
     xpc_dictionary_set_uint64(dict, kHelperMode, bpf_request);
     SendDict_ToServer(dict);
     xpc_release(dict);
     dict = NULL;

}

int mDNSPowerRequest(int key, int interval)
{
    int err_code = kHelperErr_NotConnected;
    
    mDNSHELPER_DEBUG("mDNSPowerRequest: Using XPC IPC calling out to Helper key is [%d] interval is [%d]", key, interval);
    Init_Connection(kHelperService);
    
    // Create Dictionary To Send
    xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(dict, kHelperMode, power_req);
    xpc_dictionary_set_uint64(dict, "powerreq_key", key);
    xpc_dictionary_set_uint64(dict, "powerreq_interval", interval);
    
    err_code = SendDict_ToServer(dict);
    xpc_release(dict);
    dict = NULL;
    
    mDNSHELPER_DEBUG("mDNSPowerRequest: Using XPC IPC returning error_code %d", err_code);
    return err_code;
}

int mDNSSetLocalAddressCacheEntry(int ifindex, int family, const v6addr_t ip, const ethaddr_t eth)
{
    int err_code = kHelperErr_NotConnected;
    
    mDNSHELPER_DEBUG("mDNSSetLocalAddressCacheEntry: Using XPC IPC calling out to Helper: ifindex is [%d] family is [%d]", ifindex, family);
    
    Init_Connection(kHelperService);
    
    // Create Dictionary To Send
    xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(dict, kHelperMode, set_localaddr_cacheentry);
    
    xpc_dictionary_set_uint64(dict, "slace_ifindex", ifindex);
    xpc_dictionary_set_uint64(dict, "slace_family", family);
    
    xpc_dictionary_set_data(dict, "slace_ip", (uint8_t*)ip, sizeof(v6addr_t));
    xpc_dictionary_set_data(dict, "slace_eth", (uint8_t*)eth, sizeof(ethaddr_t));
    
    err_code = SendDict_ToServer(dict);
    xpc_release(dict);
    dict = NULL;
    
    mDNSHELPER_DEBUG("mDNSSetLocalAddressCacheEntry: Using XPC IPC returning error_code %d", err_code);
    return err_code;
}


void mDNSNotify(const char *title, const char *msg) // Both strings are UTF-8 text
{
    mDNSHELPER_DEBUG("mDNSNotify() calling out to Helper XPC IPC title[%s] msg[%s]", title, msg);

    Init_Connection(kHelperService);
    
    // Create Dictionary To Send
    xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(dict, kHelperMode, user_notify);
    
    xpc_dictionary_set_string(dict, "notify_title", title);
    xpc_dictionary_set_string(dict, "notify_msg", msg);
    
    SendDict_ToServer(dict);
    xpc_release(dict);
    dict = NULL;
    
}


int mDNSKeychainGetSecrets(CFArrayRef *result)
{
    
    CFPropertyListRef plist = NULL;
    CFDataRef bytes = NULL;
    unsigned int numsecrets = 0;
    unsigned int secretsCnt = 0;
    int error_code = kHelperErr_NotConnected;
    xpc_object_t reply_dict = NULL;
    const void *sec = NULL;
    size_t secrets_size = 0;
    
    mDNSHELPER_DEBUG("mDNSKeychainGetSecrets: Using XPC IPC calling out to Helper");
    
    Init_Connection(kHelperService);
    
    // Create Dictionary To Send
    xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(dict, kHelperMode, keychain_getsecrets);

    reply_dict = SendDict_GetReply(dict);
 
    if (reply_dict != NULL)
    {
        numsecrets = xpc_dictionary_get_uint64(reply_dict, "keychain_num_secrets");
        sec = xpc_dictionary_get_data(reply_dict, "keychain_secrets", &secrets_size);
        secretsCnt = xpc_dictionary_get_uint64(reply_dict, "keychain_secrets_count");
        error_code = xpc_dictionary_get_int64(reply_dict,   kHelperErrCode);
    }
 
    mDNSHELPER_DEBUG("mDNSKeychainGetSecrets: Using XPC IPC calling out to Helper: numsecrets is %d, secretsCnt is %d error_code is %d secret_size is %d",
           numsecrets, secretsCnt, error_code, secrets_size);
     
    if (NULL == (bytes = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (void*)sec, secretsCnt, kCFAllocatorNull)))
    {
        error_code = kHelperErr_ApiErr;
        LogMsg("mDNSKeychainGetSecrets: CFDataCreateWithBytesNoCopy failed");
        goto fin;
    }
    
    if (NULL == (plist = CFPropertyListCreateWithData(kCFAllocatorDefault, bytes, kCFPropertyListImmutable, NULL, NULL)))
    {
        error_code = kHelperErr_ApiErr;
        LogMsg("mDNSKeychainGetSecrets: CFPropertyListCreateFromXMLData failed");
        goto fin;
    }
    
    if (CFArrayGetTypeID() != CFGetTypeID(plist))
    {
        error_code = kHelperErr_ApiErr;
        LogMsg("mDNSKeychainGetSecrets: Unexpected result type");
        CFRelease(plist);
        plist = NULL;
        goto fin;
    }
    
    *result = (CFArrayRef)plist;
    
    
fin:
    if (bytes)
        CFRelease(bytes);
    if (dict)
        xpc_release(dict);
    if (reply_dict)
        xpc_release(reply_dict);
    
    dict = NULL;
    reply_dict = NULL;
    
    return error_code;
}


int mDNSAutoTunnelSetKeys(int replacedelete, v6addr_t local_inner,
                          v6addr_t local_outer, short local_port, v6addr_t remote_inner,
                          v6addr_t remote_outer, short remote_port, const char* const prefix, const domainname *const fqdn)
{
    
    int err_code = kHelperErr_NotConnected;
    
    mDNSHELPER_DEBUG("mDNSAutoTunnelSetKeys: Using XPC IPC calling out to Helper. Parameters are repdel[%d], lport[%d], rport[%d], prefix[%s], fqdn[%##s]",
                      replacedelete, local_port, remote_port, prefix, fqdn->c);
    
    
    char buf1[INET6_ADDRSTRLEN];
    char buf2[INET6_ADDRSTRLEN];
    char buf3[INET6_ADDRSTRLEN];
    char buf4[INET6_ADDRSTRLEN];
    
    buf1[0] = 0;
    buf2[0] = 0;
    buf3[0] = 0;
    buf4[0] = 0;
    
    inet_ntop(AF_INET6, local_inner, buf1, sizeof(buf1));
    inet_ntop(AF_INET6, local_outer, buf2, sizeof(buf2));
    inet_ntop(AF_INET6, remote_inner, buf3, sizeof(buf3));
    inet_ntop(AF_INET6, remote_outer, buf4, sizeof(buf4));
    
    char fqdnStr[MAX_ESCAPED_DOMAIN_NAME + 10] = { 0 }; // Assume the prefix is no larger than 10 chars
    if (fqdn)
    {
        mDNSPlatformStrCopy(fqdnStr, prefix);
        ConvertDomainNameToCString(fqdn, fqdnStr + mDNSPlatformStrLen(prefix));
    }
    
    mDNSHELPER_DEBUG("mDNSAutoTunnelSetKeys: Using XPC IPC calling out to Helper: Parameters are local_inner is %s, local_outeris %s, remote_inner is %s, remote_outer is %s",
                      buf1, buf2, buf3, buf4);
    
    Init_Connection(kHelperService);
    
    // Create Dictionary To Send
    xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(dict, kHelperMode, autotunnel_setkeys);
    
    xpc_dictionary_set_data(dict, "autotunnelsetkeys_localinner",  (uint8_t*)local_inner,  sizeof(v6addr_t));
    xpc_dictionary_set_data(dict, "autotunnelsetkeys_localouter",  (uint8_t*)local_outer,  sizeof(v6addr_t));
    xpc_dictionary_set_data(dict, "autotunnelsetkeys_remoteinner", (uint8_t*)remote_inner, sizeof(v6addr_t));
    xpc_dictionary_set_data(dict, "autotunnelsetkeys_remoteouter", (uint8_t*)remote_outer, sizeof(v6addr_t));
    
    xpc_dictionary_set_uint64(dict, "autotunnelsetkeys_lport",  local_port);
    xpc_dictionary_set_uint64(dict, "autotunnelsetkeys_rport",  remote_port);
    xpc_dictionary_set_uint64(dict, "autotunnelsetkeys_repdel", replacedelete);

    // xpc_dictionary_set_string(dict, "autotunnelsetkeys_prefix",  prefix);
    xpc_dictionary_set_string(dict, "autotunnelsetkeys_fqdnStr", fqdnStr);
    
    err_code = SendDict_ToServer(dict);
    
    xpc_release(dict);
    dict = NULL;

    mDNSHELPER_DEBUG("mDNSAutoTunnelSetKeys: Using XPC IPC returning error_code %d", err_code);
    
    mDNSHELPER_DEBUG("mDNSAutoTunnelSetKeys: this should NOT be done in mDNSResponder/Helper. For future we shall be using <rdar://problem/13792729>");
    return err_code;
}

void mDNSSendWakeupPacket(unsigned int ifid, char *eth_addr, char *ip_addr, int iteration)
{
    // (void) ip_addr; // unused
    // (void) iteration; // unused

    mDNSHELPER_DEBUG("mDNSSendWakeupPacket: Entered ethernet address[%s],ip_address[%s], interface_id[%d], iteration[%d]",
           eth_addr, ip_addr, ifid, iteration);
    
    Init_Connection(kHelperService);
    
    // Create Dictionary To Send
    xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(dict, kHelperMode, send_wakepkt);
    
    xpc_dictionary_set_uint64(dict, "interface_index", ifid);
    xpc_dictionary_set_string(dict, "ethernet_address", eth_addr);
    xpc_dictionary_set_string(dict, "ip_address", ip_addr);
    xpc_dictionary_set_uint64(dict, "swp_iteration", iteration);
    
    SendDict_ToServer(dict);
    xpc_release(dict);
    dict = NULL;

}

void mDNSPacketFilterControl(uint32_t command, char * ifname, uint32_t count, pfArray_t portArray, pfArray_t protocolArray)
{
    struct
    {
        pfArray_t portArray;
        pfArray_t protocolArray;
    } pfa;
    
    mDNSPlatformMemCopy(pfa.portArray, portArray, sizeof(pfArray_t));
    mDNSPlatformMemCopy(pfa.protocolArray, protocolArray, sizeof(pfArray_t));

    mDNSHELPER_DEBUG("mDNSPacketFilterControl: XPC IPC, ifname %s", ifname);
    Init_Connection(kHelperService);
    
    // Create Dictionary To Send
    xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(dict, kHelperMode, p2p_packetfilter);
    
    xpc_dictionary_set_uint64(dict, "pf_opcode", command);
    if (ifname)
        xpc_dictionary_set_string(dict, "pf_ifname", ifname);
    xpc_dictionary_set_uint64(dict, "pf_count", count);

    xpc_dictionary_set_uint64(dict, "pf_port0", pfa.portArray[0]);
    xpc_dictionary_set_uint64(dict, "pf_port1", pfa.portArray[1]);
    xpc_dictionary_set_uint64(dict, "pf_port2", pfa.portArray[2]);
    xpc_dictionary_set_uint64(dict, "pf_port3", pfa.portArray[3]);
    
    xpc_dictionary_set_uint64(dict, "pf_protocol0", pfa.protocolArray[0]);
    xpc_dictionary_set_uint64(dict, "pf_protocol1", pfa.protocolArray[1]);
    xpc_dictionary_set_uint64(dict, "pf_protocol2", pfa.protocolArray[2]);
    xpc_dictionary_set_uint64(dict, "pf_protocol3", pfa.protocolArray[3]);
    SendDict_ToServer(dict);
    xpc_release(dict);
    dict = NULL;
    
    mDNSHELPER_DEBUG("mDNSPacketFilterControl: portArray0[%d] portArray1[%d] portArray2[%d] portArray3[%d] protocolArray0[%d] protocolArray1[%d] protocolArray2[%d] protocolArray3[%d]",
            pfa.portArray[0], pfa.portArray[1], pfa.portArray[2], pfa.portArray[3], pfa.protocolArray[0], pfa.protocolArray[1], pfa.protocolArray[2], pfa.protocolArray[3]);
    
}

void mDNSSendKeepalive(const v6addr_t sadd, const v6addr_t dadd, uint16_t lport, uint16_t rport, uint32_t seq, uint32_t ack, uint16_t win)
{

    mDNSHELPER_DEBUG("mDNSSendKeepalive: Using XPC IPC calling out to Helper: lport is[%d] rport is[%d] seq is[%d] ack is[%d] win is[%d]",
           lport, rport, seq, ack, win);
    
    char buf1[INET6_ADDRSTRLEN];
    char buf2[INET6_ADDRSTRLEN];
    
    buf1[0] = 0;
    buf2[0] = 0;
    
    inet_ntop(AF_INET6, sadd, buf1, sizeof(buf1));
    inet_ntop(AF_INET6, dadd, buf2, sizeof(buf2));
    mDNSHELPER_DEBUG("mDNSSendKeepalive: Using XPC IPC calling out to Helper: sadd is %s, dadd is %s", buf1, buf2);
    
    Init_Connection(kHelperService);
    
    // Create Dictionary To Send
    xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(dict, kHelperMode, send_keepalive);
    
    xpc_dictionary_set_data(dict, "send_keepalive_sadd", (uint8_t*)sadd, sizeof(v6addr_t));
    xpc_dictionary_set_data(dict, "send_keepalive_dadd", (uint8_t*)dadd, sizeof(v6addr_t));
    
    xpc_dictionary_set_uint64(dict, "send_keepalive_lport", lport);
    xpc_dictionary_set_uint64(dict, "send_keepalive_rport", rport);
    xpc_dictionary_set_uint64(dict, "send_keepalive_seq", seq);
    xpc_dictionary_set_uint64(dict, "send_keepalive_ack", ack);
    xpc_dictionary_set_uint64(dict, "send_keepalive_win", win);
    
    SendDict_ToServer(dict);
    xpc_release(dict);
    dict = NULL;
    
}

int mDNSRetrieveTCPInfo(int family, v6addr_t laddr, uint16_t lport, v6addr_t raddr, uint16_t rport, uint32_t *seq, uint32_t *ack, uint16_t *win, int32_t *intfid)
{
    int error_code = kHelperErr_NotConnected;
    xpc_object_t reply_dict = NULL;
    
    mDNSHELPER_DEBUG("mDNSRetrieveTCPInfo: Using XPC IPC calling out to Helper: lport is[%d] rport is[%d] family is[%d]",
           lport, rport, family);
    
    char buf1[INET6_ADDRSTRLEN];
    char buf2[INET6_ADDRSTRLEN];
    buf1[0] = 0;
    buf2[0] = 0;
    
    inet_ntop(AF_INET6, laddr, buf1, sizeof(buf1));
    inet_ntop(AF_INET6, raddr, buf2, sizeof(buf2));
    mDNSHELPER_DEBUG("mDNSRetrieveTCPInfo:: Using XPC IPC calling out to Helper: laddr is %s, raddr is %s", buf1, buf2);
    
    Init_Connection(kHelperService);
    
    // Create Dictionary To Send
    xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(dict, kHelperMode, retreive_tcpinfo);
    
    xpc_dictionary_set_data(dict, "retreive_tcpinfo_laddr", (uint8_t*)laddr, sizeof(v6addr_t));
    xpc_dictionary_set_data(dict, "retreive_tcpinfo_raddr", (uint8_t*)raddr, sizeof(v6addr_t));
    
    xpc_dictionary_set_uint64(dict, "retreive_tcpinfo_family", family);
    xpc_dictionary_set_uint64(dict, "retreive_tcpinfo_lport", lport);
    xpc_dictionary_set_uint64(dict, "retreive_tcpinfo_rport", rport);
    
    reply_dict = SendDict_GetReply(dict);
    
    if (reply_dict != NULL)
    {
        *seq = xpc_dictionary_get_uint64(reply_dict, "retreive_tcpinfo_seq");
        *ack = xpc_dictionary_get_uint64(reply_dict, "retreive_tcpinfo_ack");
        *win = xpc_dictionary_get_uint64(reply_dict, "retreive_tcpinfo_win");
        *intfid = (int32_t)xpc_dictionary_get_uint64(reply_dict, "retreive_tcpinfo_ifid");
        error_code = xpc_dictionary_get_int64(reply_dict, kHelperErrCode);
    }
    
    mDNSHELPER_DEBUG("mDNSRetrieveTCPInfo: Using XPC IPC calling out to Helper: seq is %d, ack is %d, win is %d, intfid is %d, error is %d",
           *seq, *ack, *win, *intfid, error_code);
    
    if (dict)
        xpc_release(dict);
    if (reply_dict)
        xpc_release(reply_dict);
    dict = NULL;
    reply_dict = NULL;

    return error_code;
}
