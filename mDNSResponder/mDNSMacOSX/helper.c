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

#include <sys/cdefs.h>
#include <arpa/inet.h>
#include <bsm/libbsm.h>
#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/ipsec.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <Security/Security.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPreferencesSetSpecific.h>
#include <TargetConditionals.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <net/bpf.h>
#include <sys/sysctl.h>

#include "mDNSEmbeddedAPI.h"
#include "dns_sd.h"
#include "dnssd_ipc.h"
#include "libpfkey.h"
#include "helper.h"
#include "helper-server.h"
#include "P2PPacketFilter.h"

#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

#ifndef RTF_IFSCOPE
#define RTF_IFSCOPE 0x1000000
#endif

#if TARGET_OS_EMBEDDED
#ifndef MDNS_NO_IPSEC
#define MDNS_NO_IPSEC 1
#endif

#define NO_CFUSERNOTIFICATION 1
#define NO_SECURITYFRAMEWORK 1
#endif

// Embed the client stub code here, so we can access private functions like ConnectToServer, create_hdr, deliver_request
#include "../mDNSShared/dnssd_ipc.c"
#include "../mDNSShared/dnssd_clientstub.c"

typedef struct sadb_x_policy *ipsec_policy_t;

uid_t mDNSResponderUID;
gid_t mDNSResponderGID;

void helper_exit()
{
    os_log_info(log_handle,"mDNSResponderHelper exiting");
    exit(0);
}

mDNSexport void RequestBPF()
{
    DNSServiceRef ref;
    
    DNSServiceErrorType err = ConnectToServer(&ref, 0, send_bpf, NULL, NULL, NULL);
    if (err)
    {
        os_log(log_handle, "RequestBPF: ConnectToServer %d", err);
        return;
    }
    
    char *ptr;
    size_t len = sizeof(DNSServiceFlags);
    ipc_msg_hdr *hdr = create_hdr(send_bpf, &len, &ptr, 0, ref);
    if (!hdr)
    {
        os_log(log_handle, "RequestBPF: No mem to allocate");
        DNSServiceRefDeallocate(ref);
        return;
    }
    
    put_flags(0, &ptr);
    deliver_request(hdr, ref);      // Will free hdr for us
    DNSServiceRefDeallocate(ref);
    update_idle_timer();

    os_log_info(log_handle,"RequestBPF: Successful");
}


void PowerRequest(int key, int interval, int *err)
{
    *err = kHelperErr_DefaultErr;
    
    os_log_info(log_handle,"PowerRequest: key %d interval %d, err %d", key, interval, *err);
    
    CFArrayRef events = IOPMCopyScheduledPowerEvents();
    if (events)
    {
        int i;
        CFIndex count = CFArrayGetCount(events);
        for (i=0; i<count; i++)
        {
            CFDictionaryRef dict = CFArrayGetValueAtIndex(events, i);
            CFStringRef id = CFDictionaryGetValue(dict, CFSTR(kIOPMPowerEventAppNameKey));
            if (CFEqual(id, CFSTR("mDNSResponderHelper")))
            {
                CFDateRef EventTime = CFDictionaryGetValue(dict, CFSTR(kIOPMPowerEventTimeKey));
                CFStringRef EventType = CFDictionaryGetValue(dict, CFSTR(kIOPMPowerEventTypeKey));
                IOReturn result = IOPMCancelScheduledPowerEvent(EventTime, id, EventType);
                //os_log(log_handle, "Deleting old event %s");
                if (result)
                    os_log(log_handle, "IOPMCancelScheduledPowerEvent %d failed %d", i, result);
            }
        }
        CFRelease(events);
    }
    
    if (key < 0) // mDNSPowerRequest(-1,-1) means "clear any stale schedules" (see above)
    {
        *err = kHelperErr_NoErr;
    }
    else if (key == 0)      // mDNSPowerRequest(0, 0) means "sleep now"
    {
        IOReturn r = IOPMSleepSystem(IOPMFindPowerManagement(MACH_PORT_NULL));
        if (r)
        {
            usleep(100000);
            os_log_info(log_handle, "IOPMSleepSystem %d", r);
        }
        *err = r;
    }
    else if (key > 0)
    {
        CFDateRef wakeTime = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent() + interval);
        if (wakeTime)
        {
            CFMutableDictionaryRef scheduleDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            
            CFDictionaryAddValue(scheduleDict, CFSTR(kIOPMPowerEventTimeKey), wakeTime);
            CFDictionaryAddValue(scheduleDict, CFSTR(kIOPMPowerEventAppNameKey), CFSTR("mDNSResponderHelper"));
            CFDictionaryAddValue(scheduleDict, CFSTR(kIOPMPowerEventTypeKey), key ? CFSTR(kIOPMAutoWake) : CFSTR(kIOPMAutoSleep));
            
            IOReturn r = IOPMRequestSysWake(scheduleDict);
            if (r)
            {
                usleep(100000);
                os_log_info(log_handle, "IOPMRequestSysWake(%d) %d %x", interval, r, r);
            }
            *err = r;
            CFRelease(wakeTime);
            CFRelease(scheduleDict);
        }
    }
    
    update_idle_timer();
}

void SetLocalAddressCacheEntry(int ifindex, int family, const v6addr_t ip, const ethaddr_t eth, int *err)
{
    
#define IPv6FMTSTRING "%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X"
#define IPv6FMTARGS  ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7], ip[8], ip[9], ip[10], ip[11], ip[12], ip[13], ip[14], ip[15]
    
    if (family == 4)
    {
        os_log_info(log_handle,"SetLocalAddressCacheEntry %d IPv%d %d.%d.%d.%d %02X:%02X:%02X:%02X:%02X:%02X",
                       ifindex, family, ip[0], ip[1], ip[2], ip[3], eth[0], eth[1], eth[2], eth[3], eth[4], eth[5]);
    }
    else
    {
        os_log_info(log_handle,"SetLocalAddressCacheEntry %d IPv%d " IPv6FMTSTRING " %02X:%02X:%02X:%02X:%02X:%02X",
                       ifindex, family, IPv6FMTARGS, eth[0], eth[1], eth[2], eth[3], eth[4], eth[5]);
    }
    
    *err = kHelperErr_DefaultErr;
    
    static int s = -1, seq = 0;
    if (s < 0)
    {
        s = socket(PF_ROUTE, SOCK_RAW, 0);
        if (s < 0)
            os_log(log_handle, "SetLocalAddressCacheEntry: socket(PF_ROUTE, SOCK_RAW, 0) failed %d (%s)", errno, strerror(errno));
    }

    if (s >= 0)
    {
        struct timeval tv;
        gettimeofday(&tv, 0);
        if (family == 4)
        {
            struct { struct rt_msghdr hdr; struct sockaddr_inarp dst; struct sockaddr_dl sdl; } rtmsg;
            memset(&rtmsg, 0, sizeof(rtmsg));
            
            rtmsg.hdr.rtm_msglen         = sizeof(rtmsg);
            rtmsg.hdr.rtm_version        = RTM_VERSION;
            rtmsg.hdr.rtm_type           = RTM_ADD;
            rtmsg.hdr.rtm_index          = ifindex;
            rtmsg.hdr.rtm_flags          = RTF_HOST | RTF_STATIC | RTF_IFSCOPE;
            rtmsg.hdr.rtm_addrs          = RTA_DST | RTA_GATEWAY;
            rtmsg.hdr.rtm_pid            = 0;
            rtmsg.hdr.rtm_seq            = seq++;
            rtmsg.hdr.rtm_errno          = 0;
            rtmsg.hdr.rtm_use            = 0;
            rtmsg.hdr.rtm_inits          = RTV_EXPIRE;
            rtmsg.hdr.rtm_rmx.rmx_expire = tv.tv_sec + 30;
            
            rtmsg.dst.sin_len            = sizeof(rtmsg.dst);
            rtmsg.dst.sin_family         = AF_INET;
            rtmsg.dst.sin_port           = 0;
            rtmsg.dst.sin_addr.s_addr    = *(in_addr_t*)ip;
            rtmsg.dst.sin_srcaddr.s_addr = 0;
            rtmsg.dst.sin_tos            = 0;
            rtmsg.dst.sin_other          = 0;
            
            rtmsg.sdl.sdl_len            = sizeof(rtmsg.sdl);
            rtmsg.sdl.sdl_family         = AF_LINK;
            rtmsg.sdl.sdl_index          = ifindex;
            rtmsg.sdl.sdl_type           = IFT_ETHER;
            rtmsg.sdl.sdl_nlen           = 0;
            rtmsg.sdl.sdl_alen           = ETHER_ADDR_LEN;
            rtmsg.sdl.sdl_slen           = 0;
            
            // Target MAC address goes in rtmsg.sdl.sdl_data[0..5]; (See LLADDR() in /usr/include/net/if_dl.h)
            memcpy(rtmsg.sdl.sdl_data, eth, sizeof(ethaddr_t));
            
            int len = write(s, (char *)&rtmsg, sizeof(rtmsg));
            if (len < 0)
                os_log(log_handle, "SetLocalAddressCacheEntry: write(%zu) interface %d address %d.%d.%d.%d seq %d result %d errno %d (%s)",
                        sizeof(rtmsg), ifindex, ip[0], ip[1], ip[2], ip[3], rtmsg.hdr.rtm_seq, len, errno, strerror(errno));
            len = read(s, (char *)&rtmsg, sizeof(rtmsg));
            if (len < 0 || rtmsg.hdr.rtm_errno)
                os_log(log_handle, "SetLocalAddressCacheEntry: read (%zu) interface %d address %d.%d.%d.%d seq %d result %d errno %d (%s) %d",
                        sizeof(rtmsg), ifindex, ip[0], ip[1], ip[2], ip[3], rtmsg.hdr.rtm_seq, len, errno, strerror(errno), rtmsg.hdr.rtm_errno);
            
            *err = kHelperErr_NoErr;
        }
        else
        {
            struct { struct rt_msghdr hdr; struct sockaddr_in6 dst; struct sockaddr_dl sdl; } rtmsg;
            memset(&rtmsg, 0, sizeof(rtmsg));
            
            rtmsg.hdr.rtm_msglen         = sizeof(rtmsg);
            rtmsg.hdr.rtm_version        = RTM_VERSION;
            rtmsg.hdr.rtm_type           = RTM_ADD;
            rtmsg.hdr.rtm_index          = ifindex;
            rtmsg.hdr.rtm_flags          = RTF_HOST | RTF_STATIC | RTF_IFSCOPE;
            rtmsg.hdr.rtm_addrs          = RTA_DST | RTA_GATEWAY;
            rtmsg.hdr.rtm_pid            = 0;
            rtmsg.hdr.rtm_seq            = seq++;
            rtmsg.hdr.rtm_errno          = 0;
            rtmsg.hdr.rtm_use            = 0;
            rtmsg.hdr.rtm_inits          = RTV_EXPIRE;
            rtmsg.hdr.rtm_rmx.rmx_expire = tv.tv_sec + 30;
            
            rtmsg.dst.sin6_len           = sizeof(rtmsg.dst);
            rtmsg.dst.sin6_family        = AF_INET6;
            rtmsg.dst.sin6_port          = 0;
            rtmsg.dst.sin6_flowinfo      = 0;
            rtmsg.dst.sin6_addr          = *(struct in6_addr*)ip;
            rtmsg.dst.sin6_scope_id      = ifindex;
            
            rtmsg.sdl.sdl_len            = sizeof(rtmsg.sdl);
            rtmsg.sdl.sdl_family         = AF_LINK;
            rtmsg.sdl.sdl_index          = ifindex;
            rtmsg.sdl.sdl_type           = IFT_ETHER;
            rtmsg.sdl.sdl_nlen           = 0;
            rtmsg.sdl.sdl_alen           = ETHER_ADDR_LEN;
            rtmsg.sdl.sdl_slen           = 0;
            
            // Target MAC address goes in rtmsg.sdl.sdl_data[0..5]; (See LLADDR() in /usr/include/net/if_dl.h)
            memcpy(rtmsg.sdl.sdl_data, eth, sizeof(ethaddr_t));
            
            int len = write(s, (char *)&rtmsg, sizeof(rtmsg));
            if (len < 0)
                os_log(log_handle, "SetLocalAddressCacheEntry: write(%zu) interface %d address " IPv6FMTSTRING " seq %d result %d errno %d (%s)",
                        sizeof(rtmsg), ifindex, IPv6FMTARGS, rtmsg.hdr.rtm_seq, len, errno, strerror(errno));
            len = read(s, (char *)&rtmsg, sizeof(rtmsg));
            if (len < 0 || rtmsg.hdr.rtm_errno)
                os_log(log_handle, "SetLocalAddressCacheEntry: read (%zu) interface %d address " IPv6FMTSTRING " seq %d result %d errno %d (%s) %d",
                        sizeof(rtmsg), ifindex, IPv6FMTARGS, rtmsg.hdr.rtm_seq, len, errno, strerror(errno), rtmsg.hdr.rtm_errno);
            
            *err = kHelperErr_NoErr;
        }
    }
    
    update_idle_timer();
}


void UserNotify(const char *title, const char *msg)
{
    
#ifndef NO_CFUSERNOTIFICATION
    static const char footer[] = "(Note: This message only appears on machines with 17.x.x.x IP addresses"
    " or on debugging builds with ForceAlerts set — i.e. only at Apple — not on customer machines.)";
    CFStringRef alertHeader  = CFStringCreateWithCString(NULL, title,  kCFStringEncodingUTF8);
    CFStringRef alertBody    = CFStringCreateWithCString(NULL, msg,    kCFStringEncodingUTF8);
    CFStringRef alertFooter  = CFStringCreateWithCString(NULL, footer, kCFStringEncodingUTF8);
    CFStringRef alertMessage = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@\r\r%@"), alertBody, alertFooter);
    CFRelease(alertBody);
    CFRelease(alertFooter);
    int err = CFUserNotificationDisplayNotice(0.0, kCFUserNotificationStopAlertLevel, NULL, NULL, NULL, alertHeader, alertMessage, NULL);
    if (err)
        os_log(log_handle, "CFUserNotificationDisplayNotice returned %d", err);
    CFRelease(alertHeader);
    CFRelease(alertMessage);
#else
    (void)title;
    (void)msg;
#endif /* NO_CFUSERNOTIFICATION */
    
    update_idle_timer();
}


char usercompname[MAX_DOMAIN_LABEL+1] = {0}; // the last computer name the user saw
char userhostname[MAX_DOMAIN_LABEL+1] = {0}; // the last local host name the user saw
char lastcompname[MAX_DOMAIN_LABEL+1] = {0}; // the last computer name saved to preferences
char lasthostname[MAX_DOMAIN_LABEL+1] = {0}; // the last local host name saved to preferences

#ifndef NO_CFUSERNOTIFICATION
static CFStringRef CFS_OQ = NULL;
static CFStringRef CFS_CQ = NULL;
static CFStringRef CFS_Format = NULL;
static CFStringRef CFS_ComputerName = NULL;
static CFStringRef CFS_ComputerNameMsg = NULL;
static CFStringRef CFS_LocalHostName = NULL;
static CFStringRef CFS_LocalHostNameMsg = NULL;
static CFStringRef CFS_Problem = NULL;

static CFUserNotificationRef gNotification    = NULL;
static CFRunLoopSourceRef gNotificationRLS = NULL;

static void NotificationCallBackDismissed(CFUserNotificationRef userNotification, CFOptionFlags responseFlags)
{
    os_log_debug(log_handle,"entry");
    (void)responseFlags;    // Unused
    if (userNotification != gNotification) os_log(log_handle, "NotificationCallBackDismissed: Wrong CFUserNotificationRef");
    if (gNotificationRLS)
    {
        // Caution: don't use CFRunLoopGetCurrent() here, because the currently executing thread may not be our "CFRunLoopRun" thread.
        // We need to explicitly specify the desired CFRunLoop from which we want to remove this event source.
        CFRunLoopRemoveSource(gRunLoop, gNotificationRLS, kCFRunLoopDefaultMode);
        CFRelease(gNotificationRLS);
        gNotificationRLS = NULL;
        CFRelease(gNotification);
        gNotification = NULL;
    }
    // By dismissing the alert, the user has conceptually acknowleged the rename.
    // (e.g. the machine's name is now officially "computer-2.local", not "computer.local".)
    // If we get *another* conflict, the new alert should refer to the 'old' name
    // as now being "computer-2.local", not "computer.local"
    usercompname[0] = 0;
    userhostname[0] = 0;
    lastcompname[0] = 0;
    lasthostname[0] = 0;
    update_idle_timer();
    unpause_idle_timer();
}

static void ShowNameConflictNotification(CFMutableArrayRef header, CFStringRef subtext)
{
    CFMutableDictionaryRef dictionary = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!dictionary) return;

    os_log_debug(log_handle,"entry");

    CFDictionarySetValue(dictionary, kCFUserNotificationAlertHeaderKey, header);
    CFDictionarySetValue(dictionary, kCFUserNotificationAlertMessageKey, subtext);

    CFURLRef urlRef = CFURLCreateWithFileSystemPath(NULL, CFSTR("/System/Library/CoreServices/mDNSResponder.bundle"), kCFURLPOSIXPathStyle, true);
    if (urlRef) { CFDictionarySetValue(dictionary, kCFUserNotificationLocalizationURLKey, urlRef); CFRelease(urlRef); }

    if (gNotification)  // If notification already on-screen, update it in place
        CFUserNotificationUpdate(gNotification, 0, kCFUserNotificationCautionAlertLevel, dictionary);
    else                // else, we need to create it
    {
        SInt32 error;
        gNotification = CFUserNotificationCreate(NULL, 0, kCFUserNotificationCautionAlertLevel, &error, dictionary);
        if (!gNotification || error) { os_log(log_handle, "ShowNameConflictNotification: CFUserNotificationRef: Error %d", error); return; }
        gNotificationRLS = CFUserNotificationCreateRunLoopSource(NULL, gNotification, NotificationCallBackDismissed, 0);
        if (!gNotificationRLS) { os_log(log_handle, "ShowNameConflictNotification: RLS"); CFRelease(gNotification); gNotification = NULL; return; }
        // Caution: don't use CFRunLoopGetCurrent() here, because the currently executing thread may not be our "CFRunLoopRun" thread.
        // We need to explicitly specify the desired CFRunLoop to which we want to add this event source.
        CFRunLoopAddSource(gRunLoop, gNotificationRLS, kCFRunLoopDefaultMode);
        os_log_debug(log_handle,"gRunLoop=%p gNotification=%p gNotificationRLS=%p", gRunLoop, gNotification, gNotificationRLS);
        pause_idle_timer();
    }

    CFRelease(dictionary);
}

static CFMutableArrayRef CreateAlertHeader(const char* oldname, const char* newname, const CFStringRef msg, const char* suffix)
{
    CFMutableArrayRef alertHeader = NULL;

    const CFStringRef cfoldname = CFStringCreateWithCString(NULL, oldname,  kCFStringEncodingUTF8);
    // NULL newname means we've given up trying to construct a name that doesn't conflict
    const CFStringRef cfnewname = newname ? CFStringCreateWithCString(NULL, newname,  kCFStringEncodingUTF8) : NULL;
    // We tag a zero-width non-breaking space at the end of the literal text to guarantee that, no matter what
    // arbitrary computer name the user may choose, this exact text (with zero-width non-breaking space added)
    // can never be one that occurs in the Localizable.strings translation file.
    if (!cfoldname)
    {
        os_log(log_handle, "Could not construct CFStrings for old=%s", newname);
    }
    else if (newname && !cfnewname)
    {
        os_log(log_handle, "Could not construct CFStrings for new=%s", newname);
    }
    else
    {
        const CFStringRef s1 = CFStringCreateWithFormat(NULL, NULL, CFS_Format, cfoldname, suffix);
        const CFStringRef s2 = cfnewname ? CFStringCreateWithFormat(NULL, NULL, CFS_Format, cfnewname, suffix) : NULL;

        alertHeader = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

        if (!s1)
        {
            os_log(log_handle, "Could not construct secondary CFString for old=%s", oldname);
        }
        else if (cfnewname && !s2)
        {
            os_log(log_handle, "Could not construct secondary CFString for new=%s", newname);
        }
        else if (!alertHeader)
        {
            os_log(log_handle, "Could not construct CFArray for notification");
        }
        else
        {
            // Make sure someone is logged in.  We don't want this popping up over the login window
            uid_t uid;
            gid_t gid;
            CFStringRef userName = SCDynamicStoreCopyConsoleUser(NULL, &uid, &gid);
            if (userName)
            {
                if (!CFEqual(userName, CFSTR("_mbsetupuser")))
                {
                    CFArrayAppendValue(alertHeader, msg); // Opening phrase of message, provided by caller
                    CFArrayAppendValue(alertHeader, CFS_OQ); CFArrayAppendValue(alertHeader, s1); CFArrayAppendValue(alertHeader, CFS_CQ);
                    CFArrayAppendValue(alertHeader, CFSTR(" is already in use on this network. "));
                    if (s2)
                    {
                        CFArrayAppendValue(alertHeader, CFSTR("The name has been changed to "));
                        CFArrayAppendValue(alertHeader, CFS_OQ); CFArrayAppendValue(alertHeader, s2); CFArrayAppendValue(alertHeader, CFS_CQ);
                        CFArrayAppendValue(alertHeader, CFSTR("."));
                    }
                    else
                    {
                        CFArrayAppendValue(alertHeader, CFSTR("All attempts to find an available name by adding a number to the name were also unsuccessful."));
                    }
                }
                CFRelease(userName);
            }
        }
        if (s1) CFRelease(s1);
        if (s2) CFRelease(s2);
    }
    if (cfoldname) CFRelease(cfoldname);
    if (cfnewname) CFRelease(cfnewname);

    return alertHeader;
}
#endif /* ndef NO_CFUSERNOTIFICATION */

static void update_notification(void)
{
#ifndef NO_CFUSERNOTIFICATION
    os_log_debug(log_handle,"entry ucn=%s, uhn=%s, lcn=%s, lhn=%s", usercompname, userhostname, lastcompname, lasthostname);
    if (!CFS_OQ)
    {
        // Note: The "\xEF\xBB\xBF" byte sequence (U+FEFF) in the CFS_Format string is the UTF-8 encoding of the zero-width non-breaking space character.
        // By appending this invisible character on the end of literal names, we ensure the these strings cannot inadvertently match any string
        // in the localization file -- since we know for sure that none of our strings in the localization file contain the ZWNBS character.
        CFS_Format           = CFStringCreateWithCString(NULL, "%@%s\xEF\xBB\xBF", kCFStringEncodingUTF8);

        // The strings CFS_OQ, CFS_CQ and the others below are the localization keys for the “Localizable.strings” files,
        // and MUST NOT BE CHANGED, or localization substitution will be broken.
        // To change the text displayed to the user, edit the values in the appropriate “Localizable.strings” file, not the keys here.
        // This includes making changes for adding appropriate directionality overrides like LRM, LRE, RLE, PDF, etc. These need to go in the values
        // in the appropriate “Localizable.strings” entries, not in the keys here (which then won’t match *any* entry in the localization files).
        // These localization keys here were broken in <rdar://problem/8629082> and then subsequently repaired in
        // <rdar://problem/21071535> [mDNSResponder]: TA: Gala15A185: Incorrect punctuation marks when Change the host name to an exist one
        CFS_OQ               = CFStringCreateWithCString(NULL, "“",  kCFStringEncodingUTF8);	// DO NOT CHANGE THIS STRING
        CFS_CQ               = CFStringCreateWithCString(NULL, "”",  kCFStringEncodingUTF8);	// DO NOT CHANGE THIS STRING
        CFS_ComputerName     = CFStringCreateWithCString(NULL, "The name of your computer ",  kCFStringEncodingUTF8);
        CFS_ComputerNameMsg  = CFStringCreateWithCString(NULL, "To change the name of your computer, "
                                                         "open System Preferences and click Sharing, then type the name in the Computer Name field.",  kCFStringEncodingUTF8);
        CFS_LocalHostName    = CFStringCreateWithCString(NULL, "This computer’s local hostname ",  kCFStringEncodingUTF8);
        CFS_LocalHostNameMsg = CFStringCreateWithCString(NULL, "To change the local hostname, "
                                                         "open System Preferences and click Sharing, then click “Edit” and type the name in the Local Hostname field.",  kCFStringEncodingUTF8);
        CFS_Problem          = CFStringCreateWithCString(NULL, "This may indicate a problem with the local network. "
                                                         "Please inform your network administrator.",  kCFStringEncodingUTF8);
    }

    if (!usercompname[0] && !userhostname[0])
    {
        if (gNotificationRLS)
        {
            os_log_debug(log_handle,"canceling notification %p", gNotification);
            CFUserNotificationCancel(gNotification);
            unpause_idle_timer();
        }
    }
    else
    {
        CFMutableArrayRef header = NULL;
        CFStringRef* subtext = NULL;
        if (userhostname[0] && !lasthostname[0]) // we've given up trying to construct a name that doesn't conflict
        {
            header = CreateAlertHeader(userhostname, NULL, CFS_LocalHostName, ".local");
            subtext = &CFS_Problem;
        }
        else if (usercompname[0])
        {
            header = CreateAlertHeader(usercompname, lastcompname, CFS_ComputerName, "");
            subtext = &CFS_ComputerNameMsg;
        }
        else
        {
            header = CreateAlertHeader(userhostname, lasthostname, CFS_LocalHostName, ".local");
            subtext = &CFS_LocalHostNameMsg;
        }
        ShowNameConflictNotification(header, *subtext);
        CFRelease(header);
    }
#endif
}

void PreferencesSetName(int key, const char* old, const char* new)
{
    SCPreferencesRef session = NULL;
    Boolean ok = FALSE;
    Boolean locked = FALSE;
    CFStringRef cfstr = NULL;
    char* user = NULL;
    char* last = NULL;
    Boolean needUpdate = FALSE;
    
    os_log_info(log_handle,"PreferencesSetName: entry %s old=%s new=%s",
                   key==kmDNSComputerName ? "ComputerName" : (key==kmDNSLocalHostName ? "LocalHostName" : "UNKNOWN"), old, new);
    
    switch ((enum mDNSPreferencesSetNameKey)key)
    {
        case kmDNSComputerName:
            user = usercompname;
            last = lastcompname;
            break;
        case kmDNSLocalHostName:
            user = userhostname;
            last = lasthostname;
            break;
        default:
            os_log(log_handle, "PreferencesSetName: unrecognized key: %d", key);
            goto fin;
    }
    
    if (!last)
    {
        os_log(log_handle, "PreferencesSetName: no last ptr");
        goto fin;
    }
    
    if (!user)
    {
        os_log(log_handle, "PreferencesSetName:: no user ptr");
        goto fin;
    }
    
    if (0 == strncmp(old, new, MAX_DOMAIN_LABEL+1))
    {
        // old and new are same means the config changed i.e, the user has set something in the preferences pane.
        // This means the conflict has been resolved. We need to dismiss the dialogue.
        if (last[0] && 0 != strncmp(last, new, MAX_DOMAIN_LABEL+1))
        {
            last[0] = 0;
            user[0] = 0;
            needUpdate = TRUE;
        }
        goto fin;
    }
    else
    {
        // old and new are not same, this means there is a conflict. For the first conflict, we show
        // the old value and the new value. For all subsequent conflicts, while the dialogue is still
        // up, we do a real time update of the "new" value in the dialogue. That's why we update just
        // "last" here and not "user".
        if (strncmp(last, new, MAX_DOMAIN_LABEL+1))
        {
            strncpy(last, new, MAX_DOMAIN_LABEL);
            needUpdate = TRUE;
        }
    }
    
    // If we are not showing the dialogue, we need to remember the first "old" value so that
    // we maintain the same through the lifetime of the dialogue. Subsequent conflicts don't
    // update the "old" value.
    if (!user[0])
    {
        strncpy(user, old, MAX_DOMAIN_LABEL);
        needUpdate = TRUE;
    }
    
    if (!new[0]) // we've given up trying to construct a name that doesn't conflict
        goto fin;
    
    cfstr = CFStringCreateWithCString(NULL, new, kCFStringEncodingUTF8);
    
    session = SCPreferencesCreate(NULL, CFSTR(kHelperService), NULL);
    
    if (cfstr == NULL || session == NULL)
    {
        os_log(log_handle, "PreferencesSetName: SCPreferencesCreate failed");
        goto fin;
    }
    if (!SCPreferencesLock(session, 0))
    {
        os_log(log_handle,"PreferencesSetName: lock failed");
        goto fin;
    }
    locked = TRUE;
    
    switch ((enum mDNSPreferencesSetNameKey)key)
    {
        case kmDNSComputerName:
        {
            // We want to write the new Computer Name to System Preferences, without disturbing the user-selected
            // system-wide default character set used for things like AppleTalk NBP and NETBIOS service advertising.
            // Note that this encoding is not used for the computer name, but since both are set by the same call,
            // we need to take care to set the name without changing the character set.
            CFStringEncoding encoding = kCFStringEncodingUTF8;
            CFStringRef unused = SCDynamicStoreCopyComputerName(NULL, &encoding);
            if (unused)
            {
                CFRelease(unused);
                unused = NULL;
            }
            else
            {
                encoding = kCFStringEncodingUTF8;
            }
            
            ok = SCPreferencesSetComputerName(session, cfstr, encoding);
        }
            break;
            
        case kmDNSLocalHostName:
            ok = SCPreferencesSetLocalHostName(session, cfstr);
            break;
            
        default:
            break;
    }
    
    if (!ok || !SCPreferencesCommitChanges(session) ||
        !SCPreferencesApplyChanges(session))
    {
        os_log(log_handle, "PreferencesSetName: SCPreferences update failed");
        goto fin;
    }
    os_log_info(log_handle,"PreferencesSetName: succeeded");
    
fin:
    if (NULL != cfstr)
        CFRelease(cfstr);
    if (NULL != session)
    {
        if (locked)
            SCPreferencesUnlock(session);
        CFRelease(session);
    }
    update_idle_timer();
    if (needUpdate)
        update_notification();
    
}


enum DNSKeyFormat
{
    formatNotDNSKey, formatDdnsTypeItem, formatDnsPrefixedServiceItem, formatBtmmPrefixedServiceItem
};

// On Mac OS X on Intel, the four-character string seems to be stored backwards, at least sometimes.
// I suspect some overenthusiastic inexperienced engineer said, "On Intel everything's backwards,
// therefore I need to add some byte swapping in this API to make this four-character string backwards too."
// To cope with this we allow *both* "ddns" and "sndd" as valid item types.


#ifndef NO_SECURITYFRAMEWORK
static const char btmmprefix[] = "btmmdns:";
static const char dnsprefix[] = "dns:";
static const char ddns[] = "ddns";
static const char ddnsrev[] = "sndd";

static enum DNSKeyFormat getDNSKeyFormat(SecKeychainItemRef item, SecKeychainAttributeList **attributesp)
{
    static UInt32 tags[4] =
    {
        kSecTypeItemAttr, kSecServiceItemAttr, kSecAccountItemAttr, kSecLabelItemAttr
    };
    static SecKeychainAttributeInfo attributeInfo =
    {
        sizeof(tags)/sizeof(tags[0]), tags, NULL
    };
    SecKeychainAttributeList *attributes = NULL;
    enum DNSKeyFormat format;
    Boolean malformed = FALSE;
    OSStatus status = noErr;
    int i = 0;
    
    *attributesp = NULL;
    if (noErr != (status = SecKeychainItemCopyAttributesAndData(item, &attributeInfo, NULL, &attributes, NULL, NULL)))
    {
        os_log_info(log_handle,"getDNSKeyFormat: SecKeychainItemCopyAttributesAndData %d - skipping", status);
        goto skip;
    }
    if (attributeInfo.count != attributes->count)
        malformed = TRUE;
    for (i = 0; !malformed && i < (int)attributeInfo.count; ++i)
        if (attributeInfo.tag[i] != attributes->attr[i].tag)
            malformed = TRUE;
    if (malformed)
    {
        os_log(log_handle, "getDNSKeyFormat: malformed result from SecKeychainItemCopyAttributesAndData - skipping");
        goto skip;
    }
    
    os_log_info(log_handle,"getDNSKeyFormat: entry (\"%.*s\", \"%.*s\", \"%.*s\")",
                   (int)attributes->attr[0].length, attributes->attr[0].data,
                   (int)attributes->attr[1].length, attributes->attr[1].data,
                   (int)attributes->attr[2].length, attributes->attr[2].data);

    if (attributes->attr[1].length >= MAX_ESCAPED_DOMAIN_NAME +
        sizeof(dnsprefix)-1)
    {
        os_log(log_handle, "getDNSKeyFormat: kSecServiceItemAttr too long (%u) - skipping",
              (unsigned int)attributes->attr[1].length);
        goto skip;
    }
    if (attributes->attr[2].length >= MAX_ESCAPED_DOMAIN_NAME)
    {
        os_log(log_handle, "getDNSKeyFormat: kSecAccountItemAttr too long (%u) - skipping",
              (unsigned int)attributes->attr[2].length);
        goto skip;
    }
    if (attributes->attr[1].length >= sizeof(dnsprefix)-1 && 0 == strncasecmp(attributes->attr[1].data, dnsprefix, sizeof(dnsprefix)-1))
        format = formatDnsPrefixedServiceItem;
    else if (attributes->attr[1].length >= sizeof(btmmprefix)-1 && 0 == strncasecmp(attributes->attr[1].data, btmmprefix, sizeof(btmmprefix)-1))
        format = formatBtmmPrefixedServiceItem;
    else if (attributes->attr[0].length == sizeof(ddns)-1 && 0 == strncasecmp(attributes->attr[0].data, ddns, sizeof(ddns)-1))
        format = formatDdnsTypeItem;
    else if (attributes->attr[0].length == sizeof(ddnsrev)-1 && 0 == strncasecmp(attributes->attr[0].data, ddnsrev, sizeof(ddnsrev)-1))
        format = formatDdnsTypeItem;
    else
    {
        os_log_info(log_handle,"getDNSKeyFormat: uninterested in this entry");
        goto skip;
    }
    
    *attributesp = attributes;
    os_log_info(log_handle,"getDNSKeyFormat: accepting this entry");
    return format;
    
skip:
    SecKeychainItemFreeAttributesAndData(attributes, NULL);
    return formatNotDNSKey;
}

// Insert the attributes as defined by mDNSKeyChainAttributes
static CFPropertyListRef copyKeychainItemInfo(SecKeychainItemRef item, SecKeychainAttributeList *attributes, enum DNSKeyFormat format)
{
    CFMutableArrayRef entry = NULL;
    CFDataRef data = NULL;
    OSStatus status = noErr;
    UInt32 keylen = 0;
    void *keyp = 0;
    
    if (NULL == (entry = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks)))
    {
        os_log(log_handle, "copyKeychainItemInfo: CFArrayCreateMutable failed");
        goto error;
    }
    
    // Insert the Account attribute (kmDNSKcWhere)
    switch ((enum DNSKeyFormat)format)
    {
        case formatDdnsTypeItem:
            data = CFDataCreate(kCFAllocatorDefault, attributes->attr[1].data, attributes->attr[1].length);
            break;
        case formatDnsPrefixedServiceItem:
        case formatBtmmPrefixedServiceItem:
            data = CFDataCreate(kCFAllocatorDefault, attributes->attr[1].data, attributes->attr[1].length);
            break;
        default:
            os_log(log_handle, "copyKeychainItemInfo: unknown DNSKeyFormat value");
            break;
    }
    if (NULL == data)
    {
        os_log(log_handle, "copyKeychainItemInfo: CFDataCreate for attr[1] failed");
        goto error;
    }
    CFArrayAppendValue(entry, data);
    CFRelease(data);
    
    // Insert the Where attribute (kmDNSKcAccount)
    if (NULL == (data = CFDataCreate(kCFAllocatorDefault, attributes->attr[2].data, attributes->attr[2].length)))
    {
        os_log(log_handle, "copyKeychainItemInfo: CFDataCreate for attr[2] failed");
        goto error;
    }
    
    CFArrayAppendValue(entry, data);
    CFRelease(data);
    
    // Insert the Key attribute (kmDNSKcKey)
    if (noErr != (status = SecKeychainItemCopyAttributesAndData(item, NULL, NULL, NULL, &keylen, &keyp)))
    {
        os_log(log_handle, "copyKeychainItemInfo: could not retrieve key for \"%.*s\": %d",
              (int)attributes->attr[1].length, attributes->attr[1].data, status);
        goto error;
    }
    
    data = CFDataCreate(kCFAllocatorDefault, keyp, keylen);
    SecKeychainItemFreeAttributesAndData(NULL, keyp);
    if (NULL == data)
    {
        os_log(log_handle, "copyKeychainItemInfo: CFDataCreate for keyp failed");
        goto error;
    }
    CFArrayAppendValue(entry, data);
    CFRelease(data);
    
    // Insert the Name attribute (kmDNSKcName)
    if (NULL == (data = CFDataCreate(kCFAllocatorDefault, attributes->attr[3].data, attributes->attr[3].length)))
    {
        os_log(log_handle, "copyKeychainItemInfo: CFDataCreate for attr[3] failed");
        goto error;
    }
    
    CFArrayAppendValue(entry, data);
    CFRelease(data);
    return entry;
    
error:
    if (NULL != entry)
        CFRelease(entry);
    return NULL;
}
#endif

void KeychainGetSecrets(__unused unsigned int *numsecrets,__unused unsigned long *secrets, __unused unsigned int *secretsCnt, __unused int *err)
{
#ifndef NO_SECURITYFRAMEWORK
    CFWriteStreamRef stream = NULL;
    CFDataRef result = NULL;
    CFPropertyListRef entry = NULL;
    CFMutableArrayRef keys = NULL;
    SecKeychainRef skc = NULL;
    SecKeychainItemRef item = NULL;
    SecKeychainSearchRef search = NULL;
    SecKeychainAttributeList *attributes = NULL;
    enum DNSKeyFormat format;
    OSStatus status = 0;
   
    os_log_info(log_handle,"KeychainGetSecrets: entry");
    *err = kHelperErr_NoErr;
    *numsecrets = 0;
    *secrets = (vm_offset_t)NULL;

    if (NULL == (keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks)))
    {
        os_log(log_handle, "KeychainGetSecrets: CFArrayCreateMutable failed");
        *err = kHelperErr_ApiErr;
        goto fin;
    }
    if (noErr != (status = SecKeychainCopyDefault(&skc)))
    {
        *err = kHelperErr_ApiErr;
        goto fin;
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (noErr != (status = SecKeychainSearchCreateFromAttributes(skc, kSecGenericPasswordItemClass, NULL, &search)))
    {
        *err = kHelperErr_ApiErr;
        goto fin;
    }
    for (status = SecKeychainSearchCopyNext(search, &item); noErr == status; status = SecKeychainSearchCopyNext(search, &item))
    {
        if (formatNotDNSKey != (format = getDNSKeyFormat(item, &attributes)) &&
            NULL != (entry = copyKeychainItemInfo(item, attributes, format)))
        {
            CFArrayAppendValue(keys, entry);
            CFRelease(entry);
        }
        SecKeychainItemFreeAttributesAndData(attributes, NULL);
        CFRelease(item);
    }
#pragma clang diagnostic pop
    if (errSecItemNotFound != status)
         os_log(log_handle, "KeychainGetSecrets: SecKeychainSearchCopyNext failed: %d", status);
    
    if (NULL == (stream = CFWriteStreamCreateWithAllocatedBuffers(kCFAllocatorDefault, kCFAllocatorDefault)))
    {
        *err = kHelperErr_ApiErr;
        os_log(log_handle, "KeychainGetSecrets:CFWriteStreamCreateWithAllocatedBuffers failed");
        goto fin;
    }
    
    CFWriteStreamOpen(stream);
    if (0 == CFPropertyListWrite(keys, stream, kCFPropertyListBinaryFormat_v1_0, 0, NULL))
    {
        *err = kHelperErr_ApiErr;
        os_log(log_handle, "KeychainGetSecrets:CFPropertyListWriteToStream failed");
        goto fin;
    }
    result = CFWriteStreamCopyProperty(stream, kCFStreamPropertyDataWritten);
    
    if (KERN_SUCCESS != vm_allocate(mach_task_self(), secrets, CFDataGetLength(result), VM_FLAGS_ANYWHERE))
    {
        *err = kHelperErr_ApiErr;
        os_log(log_handle, "KeychainGetSecrets: vm_allocate failed");
        goto fin;
    }
    
    CFDataGetBytes(result, CFRangeMake(0, CFDataGetLength(result)), (void *)*secrets);
    *secretsCnt = CFDataGetLength(result);
    *numsecrets = CFArrayGetCount(keys);
    
    os_log_info(log_handle,"KeychainGetSecrets: succeeded");
    
fin:
    os_log_info(log_handle,"KeychainGetSecrets: returning numsecrets[%u] secrets[%lu] secrets addr[%p] secretscount[%u]",
                   *numsecrets, *secrets, secrets, *secretsCnt);
    
    if (NULL != stream)
    {
        CFWriteStreamClose(stream);
        CFRelease(stream);
    }
    if (NULL != result)
        CFRelease(result);
    if (NULL != keys)
        CFRelease(keys);
    if (NULL != search)
        CFRelease(search);
    if (NULL != skc)
        CFRelease(skc);
    update_idle_timer();
    
    *err = KERN_SUCCESS;
    
#else
    
    *err = KERN_FAILURE;
    
#endif
    
}


CF_EXPORT CFDictionaryRef _CFCopySystemVersionDictionary(void);
CF_EXPORT const CFStringRef _kCFSystemVersionBuildVersionKey;


void SendWakeupPacket(unsigned int ifid, const char *eth_addr, const char *ip_addr, int iteration)
{
    int bpf_fd, i, j;
    struct ifreq ifr;
    char ifname[IFNAMSIZ];
    char packet[512];
    char *ptr = packet;
    char bpf_device[12];
    struct ether_addr *ea;
    // (void) ip_addr; // unused
    // (void) iteration; // unused
    
    os_log_info(log_handle,"SendWakeupPacket() ether_addr[%s] ip_addr[%s] if_id[%d] iteration[%d]",
                   eth_addr, ip_addr, ifid, iteration);
    
    if (if_indextoname(ifid, ifname) == NULL)
    {
        os_log(log_handle, "SendWakeupPacket: invalid interface index %u", ifid);
        return;
    }
    
    ea = ether_aton(eth_addr);
    if (ea == NULL)
    {
        os_log(log_handle, "SendWakeupPacket: invalid ethernet address %s", eth_addr);
        return;
    }
    
    for (i = 0; i < 100; i++)
    {
        snprintf(bpf_device, sizeof(bpf_device), "/dev/bpf%d", i);
        bpf_fd = open(bpf_device, O_RDWR, 0);
        
        if (bpf_fd == -1)
            continue;
        else
            break;
    }
    
    if (bpf_fd == -1)
    {
        os_log(log_handle, "SendWakeupPacket: cannot find a bpf device");
        return;
    }
    
    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    
    if (ioctl(bpf_fd, BIOCSETIF, (char *)&ifr) < 0)
    {
        os_log(log_handle, "SendWakeupPacket: BIOCSETIF failed %s", strerror(errno));
        return;
    }
    
    // 0x00 Destination address
    for (i=0; i<6; i++)
        *ptr++ = ea->octet[i];
    
    // 0x06 Source address (Note: Since we don't currently set the BIOCSHDRCMPLT option,
    // BPF will fill in the real interface address for us)
    for (i=0; i<6; i++)
        *ptr++ = 0;
    
    // 0x0C Ethertype (0x0842)
    *ptr++ = 0x08;
    *ptr++ = 0x42;
    
    // 0x0E Wakeup sync sequence
    for (i=0; i<6; i++)
        *ptr++ = 0xFF;
    
    // 0x14 Wakeup data
    for (j=0; j<16; j++)
        for (i=0; i<6; i++)
            *ptr++ = ea->octet[i];
    
    // 0x74 Password
    for (i=0; i<6; i++)
        *ptr++ = 0;
    
    if (write(bpf_fd, packet, ptr - packet) < 0)
    {
        os_log(log_handle, "SendWakeupPacket: write failed %s", strerror(errno));
        return;
    }
    os_log(log_handle, "SendWakeupPacket: sent unicast eth_addr %s, ip_addr %s", eth_addr, ip_addr);
    
    // Send a broadcast one to handle ethernet switches that don't flood forward packets with
    // unknown mac addresses.
    for (i=0; i<6; i++)
        packet[i] = 0xFF;
    
    if (write(bpf_fd, packet, ptr - packet) < 0)
    {
        os_log(log_handle, "SendWakeupPacket: write failed %s", strerror(errno));
        return;
    }
    os_log(log_handle, "SendWakeupPacket: sent broadcast eth_addr %s, ip_addr %s", eth_addr, ip_addr);
    
    close(bpf_fd);

}


// Open the specified port for protocol in the P2P firewall.
void PacketFilterControl(uint32_t command, const char * ifname, uint32_t count, pfArray_t portArray, pfArray_t protocolArray)
{
    int error;
    
    os_log_info(log_handle,"PacketFilterControl: command %d ifname %s, count %d",
                   command, ifname, count);
    os_log_info(log_handle,"PacketFilterControl: portArray0[%d] portArray1[%d] portArray2[%d] portArray3[%d] protocolArray0[%d] protocolArray1[%d] protocolArray2[%d] protocolArray3[%d]", portArray[0], portArray[1], portArray[2], portArray[3], protocolArray[0], protocolArray[1], protocolArray[2], protocolArray[3]);
    
    switch (command)
    {
        case PF_SET_RULES:
            error = P2PPacketFilterAddBonjourRuleSet(ifname, count, portArray, protocolArray);
            if (error)
                os_log(log_handle, "P2PPacketFilterAddBonjourRuleSet failed %s", strerror(error));
            break;
            
        case PF_CLEAR_RULES:
            error = P2PPacketFilterClearBonjourRules();
            if (error)
                os_log(log_handle, "P2PPacketFilterClearBonjourRules failed %s", strerror(error));
            break;
            
        default:
            os_log(log_handle, "PacketFilterControl: invalid command %d", command);
            break;
    }

}

static unsigned long in_cksum(unsigned short *ptr, int nbytes)
{
    unsigned long sum;
    u_short oddbyte;
    
    /*
     * Our algorithm is simple, using a 32-bit accumulator (sum),
     * we add sequential 16-bit words to it, and at the end, fold back
     * all the carry bits from the top 16 bits into the lower 16 bits.
     */
    sum = 0;
    while (nbytes > 1)
    {
        sum += *ptr++;
        nbytes -= 2;
    }
    
    /* mop up an odd byte, if necessary */
    if (nbytes == 1)
    {
        /* make sure top half is zero */
        oddbyte = 0;
        
        /* one byte only */
        *((u_char *)&oddbyte) = *(u_char *)ptr;
        sum += oddbyte;
    }
    /* Add back carry outs from top 16 bits to low 16 bits. */
    sum = (sum >> 16) + (sum & 0xffff);
    
    /* add carry */
    sum += (sum >> 16);
    
    return sum;
}

static unsigned short InetChecksum(unsigned short *ptr, int nbytes)
{
    unsigned long sum;
    
    sum = in_cksum(ptr, nbytes);
    return (unsigned short)~sum;
}

static void TCPCheckSum(int af, struct tcphdr *t, int tcplen, const v6addr_t sadd6, const v6addr_t dadd6)
{
    unsigned long sum = 0;
    unsigned short *ptr;
    
    /* TCP header checksum */
    sum = in_cksum((unsigned short *)t, tcplen);
    
    if (af == AF_INET)
    {
        /* Pseudo header */
        ptr = (unsigned short *)sadd6;
        sum += *ptr++;
        sum += *ptr++;
        ptr = (unsigned short *)dadd6;
        sum += *ptr++;
        sum += *ptr++;
    }
    else if (af == AF_INET6)
    {
        /* Pseudo header */
        ptr = (unsigned short *)sadd6;
        sum += *ptr++;
        sum += *ptr++;
        sum += *ptr++;
        sum += *ptr++;
        sum += *ptr++;
        sum += *ptr++;
        sum += *ptr++;
        sum += *ptr++;
        ptr = (unsigned short *)dadd6;
        sum += *ptr++;
        sum += *ptr++;
        sum += *ptr++;
        sum += *ptr++;
        sum += *ptr++;
        sum += *ptr++;
        sum += *ptr++;
        sum += *ptr++;
    }
    
    sum += htons(tcplen);
    sum += htons(IPPROTO_TCP);
    
    while (sum >> 16)
        sum = (sum >> 16) + (sum & 0xFFFF);
    
    t->th_sum = ~sum;
    
}

void SendKeepalive(const v6addr_t sadd6, const v6addr_t dadd6, uint16_t lport, uint16_t rport, uint32_t seq, uint32_t ack, uint16_t win)
{
    
#define IPv6FMTSTRING "%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X"
#define IPv6FMTSARGS  sadd6[0], sadd6[1], sadd6[2], sadd6[3], sadd6[4], sadd6[5], sadd6[6], sadd6[7], sadd6[8], sadd6[9], sadd6[10], sadd6[11], sadd6[12], sadd6[13], sadd6[14], sadd6[15]
#define IPv6FMTDARGS  dadd6[0], dadd6[1], dadd6[2], dadd6[3], dadd6[4], dadd6[5], dadd6[6], dadd6[7], dadd6[8], dadd6[9], dadd6[10], dadd6[11], dadd6[12], dadd6[13], dadd6[14], dadd6[15]

    os_log_info(log_handle, "SendKeepalive:  "IPv6FMTSTRING" :space: "IPv6FMTSTRING"",
                IPv6FMTSARGS, IPv6FMTDARGS);
    
    struct packet4
    {
        struct ip ip;
        struct tcphdr tcp;
    } packet4;
    struct packet6
    {
        struct tcphdr tcp;
    } packet6;
    int sock, on;
    struct tcphdr *t;
    int af;
    struct sockaddr_storage ss_to;
    struct sockaddr_in *sin_to = (struct sockaddr_in *)&ss_to;
    struct sockaddr_in6 *sin6_to = (struct sockaddr_in6 *)&ss_to;
    void *packet;
    ssize_t packetlen;
    char ctlbuf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
    struct msghdr msghdr;
    struct iovec iov;
    ssize_t len;
    
    os_log_info(log_handle,"SendKeepalive invoked: lport is[%d] rport is[%d] seq is[%d] ack is[%d] win is[%d]",
                   lport, rport, seq, ack, win);
    
    char buf1[INET6_ADDRSTRLEN];
    char buf2[INET6_ADDRSTRLEN];
    
    buf1[0] = 0;
    buf2[0] = 0;
    
    inet_ntop(AF_INET6, sadd6, buf1, sizeof(buf1));
    inet_ntop(AF_INET6, dadd6, buf2, sizeof(buf2));
    
    os_log_info(log_handle,"SendKeepalive invoked: sadd6 is %s, dadd6 is %s", buf1, buf2);
    
    // all the incoming arguments are in network order
    if ((*(unsigned *)(sadd6 +4) == 0) && (*(unsigned *)(sadd6 + 8) == 0) && (*(unsigned *)(sadd6 + 12) == 0))
    {
        af = AF_INET;
        memset(&packet4, 0, sizeof (packet4));
        
        /* Fill in all the IP header information - should be in host order*/
        packet4.ip.ip_v = 4;            /* 4-bit Version */
        packet4.ip.ip_hl = 5;       /* 4-bit Header Length */
        packet4.ip.ip_tos = 0;      /* 8-bit Type of service */
        packet4.ip.ip_len = 40;     /* 16-bit Total length */
        packet4.ip.ip_id = 9864;        /* 16-bit ID field */
        packet4.ip.ip_off = 0;      /* 13-bit Fragment offset */
        packet4.ip.ip_ttl = 63;     /* 8-bit Time To Live */
        packet4.ip.ip_p = IPPROTO_TCP;  /* 8-bit Protocol */
        packet4.ip.ip_sum = 0;      /* 16-bit Header checksum (below) */
        memcpy(&packet4.ip.ip_src.s_addr, sadd6, 4);
        memcpy(&packet4.ip.ip_dst.s_addr, dadd6, 4);
        
        /* IP header checksum */
        packet4.ip.ip_sum = InetChecksum((unsigned short *)&packet4.ip, 20);
        t = &packet4.tcp;
        packet = &packet4;
        packetlen = 40; // sum of IPv4 header len(20) and TCP header len(20)
    }
    else
    {
        af = AF_INET6;
        memset(&packet6, 0, sizeof (packet6));
        t = &packet6.tcp;
        packet = &packet6;
        // We don't send IPv6 header, hence just the TCP header len (20)
        packetlen = 20;
    }
    
    /* Fill in all the TCP header information */
    t->th_sport = lport;        /* 16-bit Source port number */
    t->th_dport = rport;        /* 16-bit Destination port */
    t->th_seq = seq;            /* 32-bit Sequence Number */
    t->th_ack = ack;            /* 32-bit Acknowledgement Number */
    t->th_off = 5;              /* Data offset */
    t->th_flags = TH_ACK;
    t->th_win = win;
    t->th_sum = 0;              /* 16-bit checksum (below) */
    t->th_urp = 0;              /* 16-bit urgent offset */
    
    TCPCheckSum(af, t, 20, sadd6, dadd6);
    
    /* Open up a RAW socket */
    if ((sock = socket(af, SOCK_RAW, IPPROTO_TCP)) < 0)
    {
        os_log(log_handle, "SendKeepalive: socket %s", strerror(errno));
        return;
    }
    
    if (af == AF_INET)
    {
        on = 1;
        if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof (on)))
        {
            close(sock);
            os_log(log_handle, "SendKeepalive: setsockopt %s", strerror(errno));
            return;
        }
        
        memset(sin_to, 0, sizeof(struct sockaddr_in));
        sin_to->sin_len = sizeof(struct sockaddr_in);
        sin_to->sin_family = AF_INET;
        memcpy(&sin_to->sin_addr, sadd6, sizeof(struct in_addr));
        sin_to->sin_port = rport;
        
        msghdr.msg_control = NULL;
        msghdr.msg_controllen = 0;
        
    }
    else
    {
        struct cmsghdr *ctl;
        
        memset(sin6_to, 0, sizeof(struct sockaddr_in6));
        sin6_to->sin6_len = sizeof(struct sockaddr_in6);
        sin6_to->sin6_family = AF_INET6;
        memcpy(&sin6_to->sin6_addr, dadd6, sizeof(struct in6_addr));
        
        sin6_to->sin6_port = rport;
        sin6_to->sin6_flowinfo = 0;
        
        
        msghdr.msg_control = ctlbuf;
        msghdr.msg_controllen = sizeof(ctlbuf);
        ctl = CMSG_FIRSTHDR(&msghdr);
        ctl->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
        ctl->cmsg_level = IPPROTO_IPV6;
        ctl->cmsg_type = IPV6_PKTINFO;
        struct in6_pktinfo *pktinfo = (struct in6_pktinfo *) CMSG_DATA(ctl);
        memcpy(&pktinfo->ipi6_addr, sadd6, sizeof(struct in6_addr));
        pktinfo->ipi6_ifindex = 0;
    }
    
    msghdr.msg_name = (struct sockaddr *)&ss_to;
    msghdr.msg_namelen = ss_to.ss_len;
    iov.iov_base = packet;
    iov.iov_len = packetlen;
    msghdr.msg_iov = &iov;
    msghdr.msg_iovlen = 1;
    msghdr.msg_flags = 0;
    
again:
    len = sendmsg(sock, &msghdr, 0);
    if (len == -1)
    {
        if (errno == EINTR)
            goto again;
    }
    
    if (len != packetlen)
    {
        os_log(log_handle, "SendKeepalive: sendmsg failed %s", strerror(errno));
    }
    else
    {
        char source[INET6_ADDRSTRLEN], dest[INET6_ADDRSTRLEN];
        
        inet_ntop(af, (void *)sadd6, source, sizeof(source));
        inet_ntop(af, (void *)dadd6, dest, sizeof(dest));
        
        os_log(log_handle, "SendKeepalive: Success Source %s:%d, Dest %s:%d, %u, %u, %u",
                source, ntohs(lport), dest, ntohs(rport), ntohl(seq), ntohl(ack), ntohs(win));
        
    }
    close(sock);

}


void RetrieveTCPInfo(int family, const v6addr_t laddr, uint16_t lport, const v6addr_t raddr, uint16_t  rport, uint32_t *seq, uint32_t *ack, uint16_t *win, int32_t *intfid, int *err)
{
    
    struct tcp_info   ti;
    struct info_tuple itpl;
    int               mib[4];
    unsigned int      miblen;
    size_t            len;
    size_t            sz;
    
    memset(&itpl, 0, sizeof(struct info_tuple));
    memset(&ti,   0, sizeof(struct tcp_info));
    
    char buf1[INET6_ADDRSTRLEN];
    char buf2[INET6_ADDRSTRLEN];
    
    buf1[0] = 0;
    buf2[0] = 0;
    
    inet_ntop(AF_INET6, laddr, buf1, sizeof(buf1));
    inet_ntop(AF_INET6, raddr, buf2, sizeof(buf2));

    os_log_info(log_handle, "RetrieveTCPInfo invoked: laddr is %s, raddr is %s", buf1, buf2);
    
    os_log_info(log_handle,"RetrieveTCPInfo invoked: lport is[%d] rport is[%d] family is [%d]",
                   lport, rport, family);

    if (family == AF_INET)
    {
        memcpy(&itpl.itpl_local_sin.sin_addr,  laddr, sizeof(struct in_addr));
        memcpy(&itpl.itpl_remote_sin.sin_addr, raddr, sizeof(struct in_addr));
        itpl.itpl_local_sin.sin_port    = lport;
        itpl.itpl_remote_sin.sin_port   = rport;
        itpl.itpl_local_sin.sin_family  = AF_INET;
        itpl.itpl_remote_sin.sin_family = AF_INET;
    }
    else
    {
        memcpy(&itpl.itpl_local_sin6.sin6_addr,  laddr, sizeof(struct in6_addr));
        memcpy(&itpl.itpl_remote_sin6.sin6_addr, raddr, sizeof(struct in6_addr));
        itpl.itpl_local_sin6.sin6_port    = lport;
        itpl.itpl_remote_sin6.sin6_port   = rport;
        itpl.itpl_local_sin6.sin6_family  = AF_INET6;
        itpl.itpl_remote_sin6.sin6_family = AF_INET6;
    }
    itpl.itpl_proto = IPPROTO_TCP;
    sz = sizeof(mib)/sizeof(mib[0]);
    if (sysctlnametomib("net.inet.tcp.info", mib, &sz) == -1)
    {
        os_log(log_handle, "RetrieveTCPInfo: sysctlnametomib failed %d, %s", errno, strerror(errno));
        *err = errno;
    }
    miblen = (unsigned int)sz;
    len    = sizeof(struct tcp_info);
    if (sysctl(mib, miblen, &ti, &len, &itpl, sizeof(struct info_tuple)) == -1)
    {
        os_log(log_handle, "RetrieveTCPInfo: sysctl failed %d, %s", errno, strerror(errno));
        *err = errno;
    }
    
    *seq    = ti.tcpi_snd_nxt - 1;
    *ack    = ti.tcpi_rcv_nxt;
    *win    = ti.tcpi_rcv_space >> ti.tcpi_rcv_wscale;
    *intfid = ti.tcpi_last_outif;
    *err    = KERN_SUCCESS;
    
}

#ifndef MDNS_NO_IPSEC

static const char configHeader[] = "# BackToMyMac\n";
static const char g_racoon_config_dir[] = "/var/run/racoon/";
static const char g_racoon_config_dir_old[] = "/etc/racoon/remote/";

static int MacOSXSystemBuildNumber(char* letter_out, int* minor_out)
{
    int major = 0, minor = 0;
    char letter = 0, buildver[256]="<Unknown>";
    CFDictionaryRef vers = _CFCopySystemVersionDictionary();
    if (vers)
    {
        CFStringRef cfbuildver = CFDictionaryGetValue(vers, _kCFSystemVersionBuildVersionKey);
        if (cfbuildver) CFStringGetCString(cfbuildver, buildver, sizeof(buildver), kCFStringEncodingUTF8);
        sscanf(buildver, "%d%c%d", &major, &letter, &minor);
        CFRelease(vers);
    }
    else
        os_log_info(log_handle, "_CFCopySystemVersionDictionary failed");
    
    if (!major) { major=16; letter = 'A'; minor = 300; os_log_info(log_handle, "Note: No Major Build Version number found; assuming 16A300"); }
    if (letter_out) *letter_out = letter;
    if (minor_out) *minor_out = minor;
    return(major);
}

static int UseOldRacoon()
{
    static int g_oldRacoon = -1;
    
    if (g_oldRacoon == -1)
    {
        char letter = 0;
        int minor = 0;
        g_oldRacoon = (MacOSXSystemBuildNumber(&letter, &minor) < 10);
        os_log_debug(log_handle, "%s", g_oldRacoon ? "old" : "new");
    }
    
    return g_oldRacoon;
}

static int RacoonSignal()
{
    return UseOldRacoon() ? SIGHUP : SIGUSR1;
}

static int notifyRacoon(void)
{
    os_log_debug(log_handle,"entry");
    static const char racoon_pid_path[] = "/var/run/racoon.pid";
    char buf[] = "18446744073709551615"; /* largest 64-bit integer */
    char *p = NULL;
    ssize_t n = 0;
    unsigned long m = 0;
    int fd = open(racoon_pid_path, O_RDONLY);
    
    if (0 > fd)
    {
        os_log_debug(log_handle,"open \"%s\" failed, and that's OK: %s", racoon_pid_path,
              strerror(errno));
        return kHelperErr_RacoonNotificationFailed;
    }
    n = read(fd, buf, sizeof(buf)-1);
    close(fd);
    if (1 > n)
    {
        os_log_debug(log_handle,"read of \"%s\" failed: %s", racoon_pid_path,
              n == 0 ? "empty file" : strerror(errno));
        return kHelperErr_RacoonNotificationFailed;
    }
    buf[n] = '\0';
    m = strtoul(buf, &p, 10);
    if (*p != '\0' && !isspace(*p))
    {
        os_log_debug(log_handle,"invalid PID \"%s\" (around '%c')", buf, *p);
        return kHelperErr_RacoonNotificationFailed;
    }
    if (2 > m)
    {
        os_log_debug(log_handle,"refusing to kill PID %lu", m);
        return kHelperErr_RacoonNotificationFailed;
    }
    if (0 != kill(m, RacoonSignal()))
    {
        os_log_debug(log_handle,"Could not signal racoon (%lu): %s", m, strerror(errno));
        return kHelperErr_RacoonNotificationFailed;
    }
    
    os_log_debug(log_handle, "Sent racoon (%lu) signal %d", m, RacoonSignal());
    return 0;
}

static const char* GetRacoonConfigDir()
{
    return UseOldRacoon() ? g_racoon_config_dir_old : g_racoon_config_dir;
}
/*
static const char* GetOldRacoonConfigDir()
{
    return UseOldRacoon() ? NULL : g_racoon_config_dir_old;
}
*/

static void closefds(int from)
{
    int fd = 0;
    struct dirent entry, *entryp = NULL;
    DIR *dirp = opendir("/dev/fd");
    
    if (dirp == NULL)
    {
        /* fall back to the erroneous getdtablesize method */
        for (fd = from; fd < getdtablesize(); ++fd)
            close(fd);
        return;
    }
    while (0 == readdir_r(dirp, &entry, &entryp) && NULL != entryp)
    {
        fd = atoi(entryp->d_name);
        if (fd >= from && fd != dirfd(dirp))
            close(fd);
    }
    closedir(dirp);
}


static int startRacoonOld(void)
{
    os_log_debug(log_handle,"entry");
    char * const racoon_args[] = { "/usr/sbin/racoon", "-e", NULL   };
    ssize_t n = 0;
    pid_t pid = 0;
    int status = 0;
    
    if (0 == (pid = fork()))
    {
        closefds(0);
        execve(racoon_args[0], racoon_args, NULL);
        os_log_info(log_handle, "execve of \"%s\" failed: %s",
                racoon_args[0], strerror(errno));
        exit(2);
    }
    os_log_info(log_handle,"racoon (pid=%lu) started",
            (unsigned long)pid);
    n = waitpid(pid, &status, 0);
    if (-1 == n)
    {
        os_log(log_handle, "Unexpected waitpid failure: %s",
                strerror(errno));
        return kHelperErr_RacoonStartFailed;
    }
    else if (pid != n)
    {
        os_log(log_handle, "Unexpected waitpid return value %d", (int)n);
        return kHelperErr_RacoonStartFailed;
    }
    else if (WIFSIGNALED(status))
    {
        os_log(log_handle,
                "racoon (pid=%lu) terminated due to signal %d",
                (unsigned long)pid, WTERMSIG(status));
        return kHelperErr_RacoonStartFailed;
    }
    else if (WIFSTOPPED(status))
    {
        os_log(log_handle,
                "racoon (pid=%lu) has stopped due to signal %d",
                (unsigned long)pid, WSTOPSIG(status));
        return kHelperErr_RacoonStartFailed;
    }
    else if (0 != WEXITSTATUS(status))
    {
        os_log(log_handle,
                "racoon (pid=%lu) exited with status %d",
                (unsigned long)pid, WEXITSTATUS(status));
        return kHelperErr_RacoonStartFailed;
    }
    os_log_debug(log_handle, "racoon (pid=%lu) daemonized normally", (unsigned long)pid);
    return 0;
}

// constant and structure for the racoon control socket
#define VPNCTL_CMD_PING 0x0004
typedef struct vpnctl_hdr_struct
{
    u_int16_t msg_type;
    u_int16_t flags;
    u_int32_t cookie;
    u_int32_t reserved;
    u_int16_t result;
    u_int16_t len;
} vpnctl_hdr;

static int startRacoon(void)
{
    os_log_debug(log_handle,"entry");
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (0 > fd)
    {
        os_log(log_handle,"Could not create endpoint for racoon control socket: %d %s",
                errno, strerror(errno));
        return kHelperErr_RacoonStartFailed;
    }
    
    struct sockaddr_un saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sun_family = AF_UNIX;
    saddr.sun_len = sizeof(saddr);
    static const char racoon_control_sock_path[] = "/var/run/vpncontrol.sock";
    strcpy(saddr.sun_path, racoon_control_sock_path);
    int result = connect(fd, (struct sockaddr*) &saddr, saddr.sun_len);
    if (0 > result)
    {
        os_log(log_handle, "Could not connect racoon control socket %s: %d %s",
                racoon_control_sock_path, errno, strerror(errno));
        return kHelperErr_RacoonStartFailed;
    }
    
    u_int32_t btmm_cookie = 0x4d4d5442;
    vpnctl_hdr h = { htons(VPNCTL_CMD_PING), 0, btmm_cookie, 0, 0, 0 };
    size_t bytes = 0;
    ssize_t ret = 0;
    
    while (bytes < sizeof(vpnctl_hdr))
    {
        ret = write(fd, ((unsigned char*)&h)+bytes, sizeof(vpnctl_hdr) - bytes);
        if (ret == -1)
        {
            os_log(log_handle, "Could not write to racoon control socket: %d %s", errno, strerror(errno));
            return kHelperErr_RacoonStartFailed;
        }
        bytes += ret;
    }
    
    int nfds = fd + 1;
    fd_set fds;
    int counter = 0;
    struct timeval tv;
    bytes = 0;
    h.cookie = 0;
    
    for (counter = 0; counter < 100; counter++)
    {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv = (struct timeval){ 0, 10000 }; // 10 milliseconds * 100 iterations = 1 second max wait time
        
        result = select(nfds, &fds, (fd_set*)NULL, (fd_set*)NULL, &tv);
        if (result > 0)
        {
            if (FD_ISSET(fd, &fds))
            {
                ret = read(fd, ((unsigned char*)&h)+bytes, sizeof(vpnctl_hdr) - bytes);
                
                if (ret == -1)
                {
                    os_log(log_handle,"Could not read from racoon control socket: %d %s", errno, strerror(errno));
                    break;
                }
                bytes += ret;
                if (bytes >= sizeof(vpnctl_hdr)) break;
            }
            else
            {
                os_log_debug(log_handle, "select returned but fd_isset not on expected fd");
            }
        }
        else if (result < 0)
        {
            os_log_debug(log_handle, "select returned %d errno %d %s", result, errno, strerror(errno));
            if (errno != EINTR) break;
        }
    }
    
    close(fd);
    
    if (bytes < sizeof(vpnctl_hdr) || h.cookie != btmm_cookie)
        return kHelperErr_RacoonStartFailed;
    
    os_log_debug(log_handle, "racoon started");
    return 0;
}

static int kickRacoon(void)
{
    if ( 0 == notifyRacoon() )
        return 0;
    return UseOldRacoon() ? startRacoonOld() : startRacoon();
}

typedef enum _mDNSTunnelPolicyWhich
{
    kmDNSTunnelPolicySetup,
    kmDNSTunnelPolicyTeardown,
    kmDNSTunnelPolicyGenerate
} mDNSTunnelPolicyWhich;

// For kmDNSTunnelPolicySetup, you can setup IPv6-in-IPv6 tunnel or IPv6-in-IPv4 tunnel
// kmDNSNoTunnel is used for other Policy types
typedef enum _mDNSTunnelType
{
    kmDNSNoTunnel,
    kmDNSIPv6IPv4Tunnel,
    kmDNSIPv6IPv6Tunnel
} mDNSTunnelType;

static const uint8_t kWholeV6Mask = 128;

static unsigned int routeSeq = 1;

static int setupTunnelRoute(const v6addr_t local, const v6addr_t remote)
{
    struct
    {
        struct rt_msghdr hdr;
        struct sockaddr_in6 dst;
        struct sockaddr_in6 gtwy;
    } msg;
    int err = 0;
    int s = -1;
    
    if (0 > (s = socket(PF_ROUTE, SOCK_RAW, AF_INET)))
    {
        os_log(log_handle,"socket(PF_ROUTE, ...) failed: %s", strerror(errno));
        
        err = kHelperErr_RoutingSocketCreationFailed;
        goto fin;
    }
    
    memset(&msg, 0, sizeof(msg));
    msg.hdr.rtm_msglen = sizeof(msg);
    msg.hdr.rtm_type = RTM_ADD;
    /* The following flags are set by `route add -inet6 -host ...` */
    msg.hdr.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_HOST | RTF_STATIC;
    msg.hdr.rtm_version = RTM_VERSION;
    msg.hdr.rtm_seq = routeSeq++;
    msg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY;
    msg.hdr.rtm_inits = RTV_MTU;
    msg.hdr.rtm_rmx.rmx_mtu = 1280;
    
    msg.dst.sin6_len = sizeof(msg.dst);
    msg.dst.sin6_family = AF_INET6;
    memcpy(&msg.dst.sin6_addr, remote, sizeof(msg.dst.sin6_addr));
    
    msg.gtwy.sin6_len = sizeof(msg.gtwy);
    msg.gtwy.sin6_family = AF_INET6;
    memcpy(&msg.gtwy.sin6_addr, local, sizeof(msg.gtwy.sin6_addr));
    
    /* send message, ignore error when route already exists */
    if (0 > write(s, &msg, msg.hdr.rtm_msglen))
    {
        int errno_ = errno;
        
        os_log_info(log_handle,"write to routing socket failed: %s", strerror(errno_));
        if (EEXIST != errno_)
        {
            err = kHelperErr_RouteAdditionFailed;
            goto fin;
        }
    }
    
fin:
    if (0 <= s)
        close(s);
    return err;
}

static int teardownTunnelRoute(const v6addr_t remote)
{
    struct
    {
        struct rt_msghdr hdr;
        struct sockaddr_in6 dst;
    } msg;
    int err = 0;
    int s = -1;
    
    if (0 > (s = socket(PF_ROUTE, SOCK_RAW, AF_INET)))
    {
        os_log(log_handle, "socket(PF_ROUTE, ...) failed: %s", strerror(errno));
        err = kHelperErr_RoutingSocketCreationFailed;
        goto fin;
    }
    memset(&msg, 0, sizeof(msg));
    
    msg.hdr.rtm_msglen = sizeof(msg);
    msg.hdr.rtm_type = RTM_DELETE;
    msg.hdr.rtm_version = RTM_VERSION;
    msg.hdr.rtm_seq = routeSeq++;
    msg.hdr.rtm_addrs = RTA_DST;
    
    msg.dst.sin6_len = sizeof(msg.dst);
    msg.dst.sin6_family = AF_INET6;
    memcpy(&msg.dst.sin6_addr, remote, sizeof(msg.dst.sin6_addr));
    if (0 > write(s, &msg, msg.hdr.rtm_msglen))
    {
        int errno_ = errno;
        
        os_log_debug(log_handle,"write to routing socket failed: %s", strerror(errno_));
        
        if (ESRCH != errno_)
        {
            err = kHelperErr_RouteDeletionFailed;
            goto fin;
        }
    }
    
fin:
    if (0 <= s)
        close(s);
    return err;
}

static int v4addr_to_string(v4addr_t addr, char *buf, size_t buflen)
{
    if (NULL == inet_ntop(AF_INET, addr, buf, buflen))
    {
        os_log(log_handle, "v4addr_to_string() inet_ntop failed: %s", strerror(errno));
        return kHelperErr_InvalidNetworkAddress;
    }
    else
    {
        return 0;
    }
}

static int v6addr_to_string(const v6addr_t addr, char *buf, size_t buflen)
{
    if (NULL == inet_ntop(AF_INET6, addr, buf, buflen))
    {
        os_log(log_handle, "v6addr_to_string inet_ntop failed: %s", strerror(errno));
        return kHelperErr_InvalidNetworkAddress;
    }
    else
    {
        return 0;
    }
}

static int ensureExistenceOfRacoonConfigDir(const char* const racoon_config_dir)
{
    struct stat s;
    int ret = stat(racoon_config_dir, &s);
    if (ret != 0)
    {
        if (errno != ENOENT)
        {
            os_log(log_handle, "stat of \"%s\" failed (%d): %s",
                    racoon_config_dir, ret, strerror(errno));
            return -1;
        }
        else
        {
            ret = mkdir(racoon_config_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
            if (ret != 0)
            {
                os_log(log_handle, "mkdir \"%s\" failed: %s",
                        racoon_config_dir, strerror(errno));
                return -1;
            }
            else
            {
                os_log_info(log_handle, "created directory \"%s\"", racoon_config_dir);
            }
        }
    }
    else if (!(s.st_mode & S_IFDIR))
    {
        os_log(log_handle, "\"%s\" is not a directory!",
                racoon_config_dir);
        return -1;
    }
    
    return 0;
}



/* Caller owns object returned in `policy' */
static int generateTunnelPolicy(mDNSTunnelPolicyWhich which, mDNSTunnelType type, int in,
                     v4addr_t src, uint16_t src_port,
                     v4addr_t dst, uint16_t dst_port,
                     const v6addr_t src6, const v6addr_t dst6,
                     ipsec_policy_t *policy, size_t *len)
{
    char srcs[INET_ADDRSTRLEN], dsts[INET_ADDRSTRLEN];
    char srcs6[INET6_ADDRSTRLEN], dsts6[INET6_ADDRSTRLEN];
    char buf[512];
    char *inOut = in ? "in" : "out";
    ssize_t n = 0;
    int err = 0;
    
    *policy = NULL;
    *len = 0;
    
    switch (which)
    {
        case kmDNSTunnelPolicySetup:
            if (type == kmDNSIPv6IPv4Tunnel)
            {
                if (0 != (err = v4addr_to_string(src, srcs, sizeof(srcs))))
                    goto fin;
                if (0 != (err = v4addr_to_string(dst, dsts, sizeof(dsts))))
                    goto fin;
                n = snprintf(buf, sizeof(buf),
                             "%s ipsec esp/tunnel/%s[%u]-%s[%u]/require",
                             inOut, srcs, src_port, dsts, dst_port);
            }
            else if (type == kmDNSIPv6IPv6Tunnel)
            {
                if (0 != (err = v6addr_to_string(src6, srcs6, sizeof(srcs6))))
                    goto fin;
                if (0 != (err = v6addr_to_string(dst6, dsts6, sizeof(dsts6))))
                    goto fin;
                n = snprintf(buf, sizeof(buf),
                             "%s ipsec esp/tunnel/%s-%s/require",
                             inOut, srcs6, dsts6);
            }
            break;
        case kmDNSTunnelPolicyTeardown:
            n = strlcpy(buf, inOut, sizeof(buf));
            break;
        case kmDNSTunnelPolicyGenerate:
            n = snprintf(buf, sizeof(buf), "%s generate", inOut);
            break;
        default:
            err = kHelperErr_IPsecPolicyCreationFailed;
            goto fin;
    }
    
    if (n >= (int)sizeof(buf))
    {
        err = kHelperErr_ResultTooLarge;
        goto fin;
    }
    
    os_log_info(log_handle, "policy=\"%s\"", buf);
    
    if (NULL == (*policy = (ipsec_policy_t)ipsec_set_policy(buf, n)))
    {
        os_log_info(log_handle, "Could not create IPsec policy from \"%s\"", buf);
        err = kHelperErr_IPsecPolicyCreationFailed;
        goto fin;
    }
    *len = ((ipsec_policy_t)(*policy))->sadb_x_policy_len * 8;
    
fin:
    return err;
}

static int sendPolicy(int s, int setup,
           struct sockaddr *src, uint8_t src_bits,
           struct sockaddr *dst, uint8_t dst_bits,
           ipsec_policy_t policy, size_t len)
{
    static unsigned int policySeq = 0;
    int err = 0;
    
    os_log_debug(log_handle, "entry, setup=%d", setup);
    
    if (setup)
        err = pfkey_send_spdadd(s, src, src_bits, dst, dst_bits, -1,
                                (char *)policy, len, policySeq++);
    else
        err = pfkey_send_spddelete(s, src, src_bits, dst, dst_bits, -1,
                                   (char *)policy, len, policySeq++);
    
    if (0 > err)
    {
        os_log(log_handle, "Could not set IPsec policy: %s", ipsec_strerror());
        err = kHelperErr_IPsecPolicySetFailed;
        goto fin;
    }
    else
    {
        err = 0;
    }
    
    os_log_debug(log_handle, "succeeded");
    
fin:
    return err;
}

static int removeSA(int s, struct sockaddr *src, struct sockaddr *dst)
{
    int err = 0;
    
    os_log_debug(log_handle, "entry");
    
    err = pfkey_send_delete_all(s, SADB_SATYPE_ESP, IPSEC_MODE_ANY, src, dst);
    if (0 > err)
    {
        os_log(log_handle, "Could not remove IPsec SA: %s", ipsec_strerror());
        err = kHelperErr_IPsecRemoveSAFailed;
        goto fin;
    }
    err = pfkey_send_delete_all(s, SADB_SATYPE_ESP, IPSEC_MODE_ANY, dst, src);
    if (0 > err)
    {
        os_log(log_handle, "Could not remove IPsec SA: %s", ipsec_strerror());
        err = kHelperErr_IPsecRemoveSAFailed;
        goto fin;
    }
    else
        err = 0;
    
    os_log_debug(log_handle, "succeeded");
    
fin:
    return err;
}

static int doTunnelPolicy(mDNSTunnelPolicyWhich which, mDNSTunnelType type,
               const v6addr_t loc_inner, uint8_t loc_bits,
               v4addr_t loc_outer, uint16_t loc_port,
               const v6addr_t rmt_inner, uint8_t rmt_bits,
               v4addr_t rmt_outer, uint16_t rmt_port,
               const v6addr_t loc_outer6, const v6addr_t rmt_outer6)
{
    struct sockaddr_in6 sin6_loc;
    struct sockaddr_in6 sin6_rmt;
    ipsec_policy_t policy = NULL;
    size_t len = 0;
    int s = -1;
    int err = 0;
    
    os_log_debug(log_handle,"entry");
    if (0 > (s = pfkey_open()))
    {
        os_log(log_handle, "Could not create IPsec policy socket: %s", ipsec_strerror());
        err = kHelperErr_IPsecPolicySocketCreationFailed;
        goto fin;
    }
    
    memset(&sin6_loc, 0, sizeof(sin6_loc));
    sin6_loc.sin6_len = sizeof(sin6_loc);
    sin6_loc.sin6_family = AF_INET6;
    sin6_loc.sin6_port = htons(0);
    memcpy(&sin6_loc.sin6_addr, loc_inner, sizeof(sin6_loc.sin6_addr));
    
    memset(&sin6_rmt, 0, sizeof(sin6_rmt));
    sin6_rmt.sin6_len = sizeof(sin6_rmt);
    sin6_rmt.sin6_family = AF_INET6;
    sin6_rmt.sin6_port = htons(0);
    memcpy(&sin6_rmt.sin6_addr, rmt_inner, sizeof(sin6_rmt.sin6_addr));
    
    int setup = which != kmDNSTunnelPolicyTeardown;
    
    if (0 != (err = generateTunnelPolicy(which, type, 1,
                                         rmt_outer, rmt_port,
                                         loc_outer, loc_port,
                                         rmt_outer6, loc_outer6,
                                         &policy, &len)))
        goto fin;
    
    if (0 != (err = sendPolicy(s, setup,
                               (struct sockaddr *)&sin6_rmt, rmt_bits,
                               (struct sockaddr *)&sin6_loc, loc_bits,
                               policy, len)))
        goto fin;
    
    if (NULL != policy)
    {
        free(policy);
        policy = NULL;
    }
    
    if (0 != (err = generateTunnelPolicy(which, type, 0,
                                         loc_outer, loc_port,
                                         rmt_outer, rmt_port,
                                         loc_outer6, rmt_outer6,
                                         &policy, &len)))
        goto fin;
    if (0 != (err = sendPolicy(s, setup,
                               (struct sockaddr *)&sin6_loc, loc_bits,
                               (struct sockaddr *)&sin6_rmt, rmt_bits,
                               policy, len)))
        goto fin;
    
    if (which == kmDNSTunnelPolicyTeardown)
    {
        if (rmt_port)       // Outer tunnel is IPv4
        {
            if (loc_outer && rmt_outer)
            {
                struct sockaddr_in sin_loc;
                struct sockaddr_in sin_rmt;
                memset(&sin_loc, 0, sizeof(sin_loc));
                sin_loc.sin_len = sizeof(sin_loc);
                sin_loc.sin_family = AF_INET;
                memcpy(&sin_loc.sin_addr, loc_outer, sizeof(sin_loc.sin_addr));
                
                memset(&sin_rmt, 0, sizeof(sin_rmt));
                sin_rmt.sin_len = sizeof(sin_rmt);
                sin_rmt.sin_family = AF_INET;
                memcpy(&sin_rmt.sin_addr, rmt_outer, sizeof(sin_rmt.sin_addr));
                if (0 != (err = removeSA(s, (struct sockaddr *)&sin_loc, (struct sockaddr *)&sin_rmt)))
                    goto fin;
            }
        }
        else
        {
            if (loc_outer6 && rmt_outer6)
            {
                struct sockaddr_in6 sin6_lo;
                struct sockaddr_in6 sin6_rm;
                
                memset(&sin6_lo, 0, sizeof(sin6_lo));
                sin6_lo.sin6_len = sizeof(sin6_lo);
                sin6_lo.sin6_family = AF_INET6;
                memcpy(&sin6_lo.sin6_addr, loc_outer6, sizeof(sin6_lo.sin6_addr));
                
                memset(&sin6_rm, 0, sizeof(sin6_rm));
                sin6_rm.sin6_len = sizeof(sin6_rm);
                sin6_rm.sin6_family = AF_INET6;
                memcpy(&sin6_rm.sin6_addr, rmt_outer6, sizeof(sin6_rm.sin6_addr));
                if (0 != (err = removeSA(s, (struct sockaddr *)&sin6_lo, (struct sockaddr *)&sin6_rm)))
                    goto fin;
            }
        }
    }
    
    os_log_debug(log_handle,"succeeded");
    
fin:
    if (s >= 0)
        pfkey_close(s);
    if (NULL != policy)
        free(policy);
    return err;
}

#endif /* ndef MDNS_NO_IPSEC */

int HelperAutoTunnelSetKeys(int replacedelete, const v6addr_t loc_inner, const v6addr_t loc_outer6, uint16_t loc_port, const v6addr_t rmt_inner,
                            const v6addr_t rmt_outer6, uint16_t rmt_port, const char *id, int *err)
{
#ifndef MDNS_NO_IPSEC
    static const char config[] =
    "%s"
    "remote %s [%u] {\n"
    "  disconnect_on_idle idle_timeout 600 idle_direction idle_outbound;\n"
    "  exchange_mode aggressive;\n"
    "  doi ipsec_doi;\n"
    "  situation identity_only;\n"
    "  verify_identifier off;\n"
    "  generate_policy on;\n"
    "  my_identifier user_fqdn \"%s\";\n"
    "  shared_secret keychain \"%s\";\n"
    "  nonce_size 16;\n"
    "  lifetime time 15 min;\n"
    "  initial_contact on;\n"
    "  support_proxy on;\n"
    "  nat_traversal force;\n"
    "  proposal_check claim;\n"
    "  proposal {\n"
    "    encryption_algorithm aes;\n"
    "    hash_algorithm sha256;\n"
    "    authentication_method pre_shared_key;\n"
    "    dh_group 2;\n"
    "    lifetime time 15 min;\n"
    "  }\n"
    "  proposal {\n"
    "    encryption_algorithm aes;\n"
    "    hash_algorithm sha1;\n"
    "    authentication_method pre_shared_key;\n"
    "    dh_group 2;\n"
    "    lifetime time 15 min;\n"
    "  }\n"
    "}\n\n"
    "sainfo address %s any address %s any {\n"
    "  pfs_group 2;\n"
    "  lifetime time 10 min;\n"
    "  encryption_algorithm aes;\n"
    "  authentication_algorithm hmac_sha256,hmac_sha1;\n"
    "  compression_algorithm deflate;\n"
    "}\n\n"
    "sainfo address %s any address %s any {\n"
    "  pfs_group 2;\n"
    "  lifetime time 10 min;\n"
    "  encryption_algorithm aes;\n"
    "  authentication_algorithm hmac_sha256,hmac_sha1;\n"
    "  compression_algorithm deflate;\n"
    "}\n";
    char path[PATH_MAX] = "";
    char li[INET6_ADDRSTRLEN], lo[INET_ADDRSTRLEN], lo6[INET6_ADDRSTRLEN],
    ri[INET6_ADDRSTRLEN], ro[INET_ADDRSTRLEN], ro6[INET6_ADDRSTRLEN];
    FILE *fp = NULL;
    int fd = -1;
    char tmp_path[PATH_MAX] = "";
    v4addr_t loc_outer, rmt_outer;
    
    os_log_debug(log_handle,"HelperAutoTunnelSetKeys: entry");
    *err = kHelperErr_NoErr;

    char buf1[INET6_ADDRSTRLEN];
    char buf2[INET6_ADDRSTRLEN];
    char buf3[INET6_ADDRSTRLEN];
    char buf4[INET6_ADDRSTRLEN];
    
    buf1[0] = 0;
    buf2[0] = 0;
    buf3[0] = 0;
    buf4[0] = 0;
    
    inet_ntop(AF_INET6, loc_inner,  buf1, sizeof(buf1));
    inet_ntop(AF_INET6, loc_outer6, buf2, sizeof(buf2));
    inet_ntop(AF_INET6, rmt_inner,  buf3, sizeof(buf3));
    inet_ntop(AF_INET6, rmt_outer6, buf4, sizeof(buf4));
    
    os_log_info(log_handle, "HelperAutoTunnelSetKeys: Parameters are local_inner is %s, local_outer is %s, remote_inner is %s, remote_outer is %s id is %s",
                     buf1, buf2, buf3, buf4, id);
    
    switch ((enum mDNSAutoTunnelSetKeysReplaceDelete)replacedelete)
    {
        case kmDNSAutoTunnelSetKeysReplace:
        case kmDNSAutoTunnelSetKeysDelete:
            break;
        default:
            *err = kHelperErr_InvalidTunnelSetKeysOperation;
            goto fin;
    }
    
    if (0 != (*err = v6addr_to_string(loc_inner, li, sizeof(li))))
        goto fin;
    if (0 != (*err = v6addr_to_string(rmt_inner, ri, sizeof(ri))))
        goto fin;
    
    os_log_debug(log_handle, "loc_inner=%s rmt_inner=%s", li, ri);
    
    if (!rmt_port)
    {
        loc_outer[0] = loc_outer[1] = loc_outer[2] = loc_outer[3] = 0;
        rmt_outer[0] = rmt_outer[1] = rmt_outer[2] = rmt_outer[3] = 0;
        
        if (0 != (*err = v6addr_to_string(loc_outer6, lo6, sizeof(lo6))))
            goto fin;
        if (0 != (*err = v6addr_to_string(rmt_outer6, ro6, sizeof(ro6))))
            goto fin;
        
        os_log_debug(log_handle, "IPv6 outer tunnel: loc_outer6=%s rmt_outer6=%s", lo6, ro6);
        
        if ((int)sizeof(path) <= snprintf(path, sizeof(path), "%s%s.conf", GetRacoonConfigDir(), ro6))
        {
            *err = kHelperErr_ResultTooLarge;
            goto fin;
        }
    }
    else
    {
        loc_outer[0] = loc_outer6[0];
        loc_outer[1] = loc_outer6[1];
        loc_outer[2] = loc_outer6[2];
        loc_outer[3] = loc_outer6[3];
        
        rmt_outer[0] = rmt_outer6[0];
        rmt_outer[1] = rmt_outer6[1];
        rmt_outer[2] = rmt_outer6[2];
        rmt_outer[3] = rmt_outer6[3];
        
        if (0 != (*err = v4addr_to_string(loc_outer, lo, sizeof(lo))))
            goto fin;
        if (0 != (*err = v4addr_to_string(rmt_outer, ro, sizeof(ro))))
            goto fin;
        
        os_log_debug(log_handle, "IPv4 outer tunnel: loc_outer=%s loc_port=%u rmt_outer=%s rmt_port=%u",
                    lo, loc_port, ro, rmt_port);
        
        if ((int)sizeof(path) <= snprintf(path, sizeof(path), "%s%s.%u.conf", GetRacoonConfigDir(), ro, rmt_port))
        {
            *err = kHelperErr_ResultTooLarge;
            goto fin;
        }
    }
    
    if (kmDNSAutoTunnelSetKeysReplace == replacedelete)
    {
        if (0 > ensureExistenceOfRacoonConfigDir(GetRacoonConfigDir()))
        {
            *err = kHelperErr_RacoonConfigCreationFailed;
            goto fin;
        }
        if ((int)sizeof(tmp_path) <=
            snprintf(tmp_path, sizeof(tmp_path), "%s.XXXXXX", path))
        {
            *err = kHelperErr_ResultTooLarge;
            goto fin;
        }
        if (0 > (fd = mkstemp(tmp_path)))
        {
            os_log(log_handle, "mkstemp \"%s\" failed: %s", tmp_path, strerror(errno));
            *err = kHelperErr_RacoonConfigCreationFailed;
            goto fin;
        }
        if (NULL == (fp = fdopen(fd, "w")))
        {
            os_log(log_handle, "fdopen: %s", strerror(errno));
            *err = kHelperErr_RacoonConfigCreationFailed;
            goto fin;
        }
        
        fd = -1;
        fprintf(fp, config, configHeader, (!rmt_port ? ro6 : ro), rmt_port, id, id, ri, li, li, ri);
        fclose(fp);
        fp = NULL;
        
        if (0 > rename(tmp_path, path))
        {
            os_log(log_handle, "rename \"%s\" \"%s\" failed: %s", tmp_path, path, strerror(errno));
            *err = kHelperErr_RacoonConfigCreationFailed;
            goto fin;
        }
    }
    else
    {
        if (0 != unlink(path))
            os_log_debug(log_handle, "unlink \"%s\" failed: %s", path, strerror(errno));
    }
    
    if (0 != (*err = doTunnelPolicy(kmDNSTunnelPolicyTeardown, kmDNSNoTunnel,
                                    loc_inner, kWholeV6Mask, loc_outer, loc_port,
                                    rmt_inner, kWholeV6Mask, rmt_outer, rmt_port, loc_outer6, rmt_outer6)))
        goto fin;
    if (kmDNSAutoTunnelSetKeysReplace == replacedelete &&
        0 != (*err = doTunnelPolicy(kmDNSTunnelPolicySetup, (!rmt_port ? kmDNSIPv6IPv6Tunnel : kmDNSIPv6IPv4Tunnel),
                                    loc_inner, kWholeV6Mask, loc_outer, loc_port,
                                    rmt_inner, kWholeV6Mask, rmt_outer, rmt_port, loc_outer6, rmt_outer6)))
        goto fin;
    
    if (0 != (*err = teardownTunnelRoute(rmt_inner)))
        goto fin;
    
    if (kmDNSAutoTunnelSetKeysReplace == replacedelete &&
        0 != (*err = setupTunnelRoute(loc_inner, rmt_inner)))
        goto fin;
    
    if (kmDNSAutoTunnelSetKeysReplace == replacedelete &&
        0 != (*err = kickRacoon()))
        goto fin;
    
    os_log_debug(log_handle, "succeeded");
    
fin:
    if (NULL != fp)
        fclose(fp);
    if (0 <= fd)
        close(fd);
    unlink(tmp_path);
#else
    (void)replacedelete; (void)loc_inner; (void)loc_outer6; (void)loc_port; (void)rmt_inner;
    (void)rmt_outer6; (void)rmt_port; (void)id;
    
    *err = kHelperErr_IPsecDisabled;
#endif /* MDNS_NO_IPSEC */
    update_idle_timer();
    return KERN_SUCCESS;
}




