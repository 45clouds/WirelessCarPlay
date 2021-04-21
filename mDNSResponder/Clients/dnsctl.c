/* -*- Mode: C; tab-width: 4 -*- 
 *
 * Copyright (c) 2012-2015 Apple Inc. All rights reserved.
 *
 * dnsctl.c 
 * Command-line tool using libdns_services.dylib 
 *   
 * To build only this tool, copy and paste the following on the command line:
 * On Apple 64bit Platforms ONLY OSX/iOS:
 * clang -Wall dnsctl.c /usr/lib/libdns_services.dylib -o dnsctl
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <net/if.h> // if_nametoindex()

#include "dns_services.h"
#include <xpc/xpc.h>
#include "dns_xpc.h"

//*************************************************************************************************************
// Globals:
//*************************************************************************************************************

static const char kFilePathSep   =  '/';

static DNSXConnRef ClientRef     =  NULL;

static xpc_connection_t dnsctl_conn = NULL;

//*************************************************************************************************************
// Utility Funcs:
//*************************************************************************************************************

static void printtimestamp(void)
{
    struct tm tm;
    int ms;
    static char date[16];
    static char new_date[16];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    localtime_r((time_t*)&tv.tv_sec, &tm);
    ms = tv.tv_usec/1000;
    strftime(new_date, sizeof(new_date), "%a %d %b %Y", &tm);
    //display date only if it has changed
    if (strncmp(date, new_date, sizeof(new_date)))
    {
        printf("DATE: ---%s---\n", new_date);
        strlcpy(date, new_date, sizeof(date));
    }
    printf("%2d:%02d:%02d.%03d  ", tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
}

static void print_usage(const char *arg0)
{
    fprintf(stderr, "%s USAGE:                                                                  \n", arg0);
    fprintf(stderr, "%s -DP Enable DNS Proxy with Default Parameters                            \n", arg0);
    fprintf(stderr, "%s -DP [-o <output interface>] [-i <input interface(s)>] Enable DNS Proxy  \n", arg0);
    fprintf(stderr, "%s -L [1/2/3/4] Change mDNSResponder Logging Level                         \n", arg0);
    fprintf(stderr, "%s -I Print mDNSResponder STATE INFO                                       \n", arg0);
}


static bool DebugEnabled()
{
    return true; // keep this true to debug the XPC msgs
}

static void DebugLog(const char *prefix, xpc_object_t o)
{
    if (!DebugEnabled())
        return;
    
    char *desc = xpc_copy_description(o);
    printf("%s: %s \n", prefix, desc);
    free(desc);
}

//*************************************************************************************************************
// CallBack Funcs:
//*************************************************************************************************************


// DNSXEnableProxy Callback from the Daemon
static void dnsproxy_reply(DNSXConnRef connRef, DNSXErrorType errCode)
{
    (void) connRef;
    printtimestamp();
    switch (errCode)
    {
        case kDNSX_NoError          :  printf("  SUCCESS   \n");
            break;
        case kDNSX_DaemonNotRunning :  printf(" NO DAEMON  \n");
            DNSXRefDeAlloc(ClientRef);    break;
        case kDNSX_BadParam          :  printf(" BAD PARAMETER \n");
            DNSXRefDeAlloc(ClientRef);    break;
        case kDNSX_Busy             :  printf(" BUSY \n");
            DNSXRefDeAlloc(ClientRef);    break;
        case kDNSX_UnknownErr       :
        default                     :  printf(" UNKNOWN ERR \n");
            DNSXRefDeAlloc(ClientRef);    break;
    }
    fflush(NULL);
    
}

//*************************************************************************************************************
// XPC Funcs:
//*************************************************************************************************************

static void Init_Connection(const char *servname)
{
    dnsctl_conn = xpc_connection_create_mach_service(servname, dispatch_get_main_queue(), XPC_CONNECTION_MACH_SERVICE_PRIVILEGED);
    
    xpc_connection_set_event_handler(dnsctl_conn, ^(xpc_object_t event)
    {
         printf("InitConnection: [%s] \n", xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION));
    });
    
    xpc_connection_resume(dnsctl_conn);
}

static void SendDictToServer(xpc_object_t msg)
{
    
    DebugLog("SendDictToServer Sending msg to Daemon", msg);
    
    xpc_connection_send_message_with_reply(dnsctl_conn, msg, dispatch_get_main_queue(), ^(xpc_object_t recv_msg)
    {
        xpc_type_t type = xpc_get_type(recv_msg);
                                               
        if (type == XPC_TYPE_DICTIONARY)
        {
            DebugLog("SendDictToServer Received reply msg from Daemon", recv_msg);
            /*
            // If we ever want to do something based on the reply of the daemon
            switch (daemon_status)
            {
                default:
                    break;
            }
            */
        }
        else
        {
            printf("SendDictToServer Received unexpected reply from daemon [%s]",
                                xpc_dictionary_get_string(recv_msg, XPC_ERROR_KEY_DESCRIPTION));
            DebugLog("SendDictToServer Unexpected Reply contents", recv_msg);
        }
        exit(1);
    });
}

//*************************************************************************************************************

int main(int argc, char **argv)
{
    // Extract program name from argv[0], which by convention contains the path to this executable
    const char *a0 = strrchr(argv[0], kFilePathSep) + 1;
    if (a0 == (const char *)1)
        a0 = argv[0];
    
    // Must run as root
    if (0 != geteuid())
    {
        fprintf(stderr, "%s MUST run as root!!\n", a0);
        exit(-1);
    }
    if ((sizeof(argv) == 8))
        printf("dnsctl running in 64-bit mode\n");
    else if ((sizeof(argv) == 4))
        printf("dnsctl running in 32-bit mode\n");
    
    // expects atleast one argument
    if (argc < 2)
        goto Usage;
    
    printtimestamp();
    if (!strcasecmp(argv[1], "-DP"))
    {
        DNSXErrorType err;
        // Default i/p intf is lo0 and o/p intf is primary interface
        IfIndex Ipintfs[MaxInputIf] =  {1, 0, 0, 0, 0};
        IfIndex Opintf = kDNSIfindexAny;
        
        if (argc == 2)
        {
            dispatch_queue_t my_Q = dispatch_queue_create("com.apple.dnsctl.callback_queue", NULL);
            err = DNSXEnableProxy(&ClientRef, kDNSProxyEnable, Ipintfs, Opintf, my_Q, dnsproxy_reply);
            if (err)
                fprintf(stderr, "DNSXEnableProxy returned %d\n", err);
        }
        else if (argc > 2)
        {
            argc--;
            argv++;
            if (!strcmp(argv[1], "-o"))
            {
                Opintf = if_nametoindex(argv[2]);
                if (!Opintf)
                    Opintf = atoi(argv[2]);
                if (!Opintf)
                {
                    fprintf(stderr, "Could not parse o/p interface [%s]: Passing default primary \n", argv[2]);
                    Opintf = kDNSIfindexAny;
                }
                argc -= 2;
                argv += 2;
            }
            if (argc > 2 && !strcmp(argv[1], "-i"))
            {
                int i;
                argc--;
                argv++;
                for (i = 0; i < MaxInputIf && argc > 1; i++)
                {
                    Ipintfs[i] = if_nametoindex(argv[1]);
                    if (!Ipintfs[i])
                        Ipintfs[i] = atoi(argv[1]);
                    if (!Ipintfs[i])
                    {
                        fprintf(stderr, "Could not parse i/p interface [%s]: Passing default lo0 \n", argv[2]);
                        Ipintfs[i] = 1;
                    }
                    argc--;
                    argv++;
                }
            }
            printf("Enabling DNSProxy on mDNSResponder \n");
            dispatch_queue_t my_Q = dispatch_queue_create("com.apple.dnsctl.callback_queue", NULL);
            err = DNSXEnableProxy(&ClientRef, kDNSProxyEnable, Ipintfs, Opintf, my_Q, dnsproxy_reply);
            if (err)
                fprintf(stderr, "DNSXEnableProxy returned %d\n", err);
        }
    }
    else if (!strcasecmp(argv[1], "-l"))
    {
        printf("Changing loglevel of mDNSResponder \n");
        Init_Connection(kDNSCTLService);

        // Create Dictionary To Send
        xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);

        if (argc == 2)
        {
            xpc_dictionary_set_uint64(dict, kDNSLogLevel, log_level1);
            
            SendDictToServer(dict);
            xpc_release(dict);
            dict = NULL;
        }
        else if (argc > 2)
        {
            argc--;
            argv++;
            switch (atoi(argv[1]))
            {
                case log_level1:
                    xpc_dictionary_set_uint64(dict, kDNSLogLevel, log_level1);
                    break;
                    
                case log_level2:
                    xpc_dictionary_set_uint64(dict, kDNSLogLevel, log_level2);
                    break;
                
                case log_level3:
                    xpc_dictionary_set_uint64(dict, kDNSLogLevel, log_level3);
                    break;
                
                case log_level4:
                    xpc_dictionary_set_uint64(dict, kDNSLogLevel, log_level4);
                    break;
                    
                default:
                    xpc_dictionary_set_uint64(dict, kDNSLogLevel, log_level1);
                    break;
            }
            SendDictToServer(dict);
            xpc_release(dict);
            dict = NULL;
        }
    }
    else if(!strcasecmp(argv[1], "-i"))
    {
        printf("Get STATE INFO of mDNSResponder \n");
        Init_Connection(kDNSCTLService);

        // Create Dictionary To Send
        xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_uint64(dict, kDNSStateInfo, full_state);
        SendDictToServer(dict);
        xpc_release(dict);
        dict = NULL;
    }
    else if(!strcasecmp(argv[1], "-th"))
    {
        printf("Sending Test message to mDNSResponder to forward to mDNSResponderHelper\n");
        Init_Connection(kDNSCTLService);
        
        // Create Dictionary To Send
        xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_uint64(dict, kmDNSResponderTests, test_helper_ipc);
        SendDictToServer(dict);
        xpc_release(dict);
        dict = NULL;
    }
    else if(!strcasecmp(argv[1], "-tl"))
    {
        printf("Testing mDNSResponder Logging\n");
        Init_Connection(kDNSCTLService);
        
        // Create Dictionary To Send
        xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_uint64(dict, kmDNSResponderTests, test_mDNS_log);
        SendDictToServer(dict);
        xpc_release(dict);
        dict = NULL;
    }
    else
    {
        goto Usage;
    }
    
    dispatch_main();
    
Usage:
    print_usage(a0);
    return 0;
}

/*
 
#include <getopt.h>
 
static int operation;

static int getfirstoption(int argc, char **argv, const char *optstr, int *pOptInd)
{
    // Return the recognized option in optstr and the option index of the next arg.
    int o = getopt(argc, (char *const *)argv, optstr);
    *pOptInd = optind;
    return o;
}
 
int opindex;
operation = getfirstoption(argc, argv, "lLDdPp", &opindex);
if (operation == -1)
    goto Usage;
 
 
 
switch (operation)
{
    case 'L':
    case 'l':
    {
        printtimestamp();
        printf("Change Verbosity Level of mDNSResponder\n");
 
        Init_Connection(kDNSCTLService);
 
        // Create Dictionary To Send
        xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);
        if (dict == NULL)
            printf("could not create the Msg Dict To Send! \n");
        xpc_dictionary_set_uint64(dict, kDNSLogLevel, log_level2);
 
        SendDictToServer(dict);
 
        xpc_release(dict);
        dict = NULL;
        break;
    }
 // exit(1);
 
}

*/

