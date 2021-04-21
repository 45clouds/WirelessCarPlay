/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2015 Apple Inc. All rights reserved.
 *
 * dnsctl_server.c
 * mDNSResponder 
 *
 * XPC as an IPC mechanism to communicate with dnsctl. Open only to Apple OSX/iOS clients
 */

#include "xpc_services.h"
#include "dns_xpc.h"

#include "mDNSMacOSX.h"      // KQueueLock/KQueueUnlock
#include "helper.h"          // mDNSResponderHelper tests
#include <xpc/xpc.h>

// ***************************************************************************
// Globals
extern mDNS mDNSStorage;
static dispatch_queue_t dnsctlserver_queue = NULL;
// ***************************************************************************

mDNSlocal void handle_logging(mDNSu32 log_level)
{
    KQueueLock(&mDNSStorage);
    
    switch (log_level)
    {
        case log_level1:
            mDNS_LoggingEnabled = mDNS_PacketLoggingEnabled = 0;
            LogMsg("USR1 Logging:[%s] USR2 Logging:[%s]", mDNS_LoggingEnabled ? "Enabled" : "Disabled", mDNS_PacketLoggingEnabled ? "Enabled" : "Disabled");
            break;
            
        case log_level2:
            mDNS_LoggingEnabled = 1;
            LogMsg("USR1 Logging %s", mDNS_LoggingEnabled ? "Enabled" : "Disabled");
            break;
            
        case log_level3:
            mDNS_PacketLoggingEnabled = 1;
            LogMsg("USR2 Logging %s", mDNS_PacketLoggingEnabled ? "Enabled" : "Disabled");
            break;
            
        case log_level4:
            mDNS_LoggingEnabled = 1 ;
            mDNS_PacketLoggingEnabled = 1;
            LogMsg("USR1 Logging:%s USR2 Logging:%s", mDNS_LoggingEnabled ? "Enabled" : "Disabled", mDNS_PacketLoggingEnabled ? "Enabled" : "Disabled");
            break;
            
        default:
            mDNS_LoggingEnabled = 0 ;
            mDNS_PacketLoggingEnabled = 0;
            break;
    }
    UpdateDebugState();
    
    KQueueUnlock(&mDNSStorage, "LogLevel changed");
}

mDNSlocal void handle_stateinfo(mDNSu32 state_level)
{
    KQueueLock(&mDNSStorage);
    
    switch (state_level)
    {
        case full_state:
            INFOCallback();
            break;
            
        default:
            INFOCallback();
            break;
    }
    
    KQueueUnlock(&mDNSStorage, "StateInfo dumped");
}


mDNSlocal void handle_test_mode(mDNSu32 test_mode)
{
    KQueueLock(&mDNSStorage);
    
    switch (test_mode)
    {
        case test_helper_ipc:
            mDNSNotify("mDNSResponderHelperTest", "This is just a test message to mDNSResponderHelper. This is NOT an actual alert");
            break;
            
        case test_mDNS_log:
            LogInfo("LogInfo: Should be logged at INFO level");
            LogOperation("LogOperation: Should be logged at INFO level");
            LogMsg("LogMsg: Should be logged at DEFAULT level");
            LogSPS("LogSPS: Should be logged at INFO level");
            break;
            
        default:
            LogMsg("Unidentified Test mode: Please add this test");
            break;
    }
    
    KQueueUnlock(&mDNSStorage, "Test Msg to mDNSResponderHelper");
}

mDNSlocal void handle_terminate()
{
    
    LogInfo("handle_terminate: Client terminated connection.");
    
}

mDNSlocal void handle_requests(xpc_object_t req)
{
    mDNSu32 log_level, state_level, test_mode;
    xpc_connection_t remote_conn = xpc_dictionary_get_remote_connection(req);

    LogInfo("handle_requests: Handler for dnsctl requests");
 
    xpc_object_t response = xpc_dictionary_create_reply(req);
    
    // Return Success Status to the client (essentially ACKing the request)
    if (response)
    {
        xpc_dictionary_set_uint64(response, kDNSDaemonReply, kDNSMsg_NoError);
        xpc_connection_send_message(remote_conn, response);
        xpc_release(response);  
    }
    else
    { 
        LogMsg("handle_requests: Response Dictionary could not be created");
        return;
    }

    // switch here based on dictionary
    // to handle various different requests like logging, INFO snapshot, etc..
    if ((xpc_dictionary_get_uint64(req, kDNSLogLevel)))
    {
        log_level = (mDNSu32)(xpc_dictionary_get_uint64(req, kDNSLogLevel));
        handle_logging(log_level);
    }
    else if ((xpc_dictionary_get_uint64(req, kDNSStateInfo)))
    {
        state_level = (mDNSu32)(xpc_dictionary_get_uint64(req, kDNSStateInfo));
        handle_stateinfo(state_level);
    }
    else if ((xpc_dictionary_get_uint64(req, kmDNSResponderTests)))
    {
        test_mode = (mDNSu32)(xpc_dictionary_get_uint64(req, kmDNSResponderTests));
        handle_test_mode(test_mode);
    }
}

mDNSlocal void accept_client(xpc_connection_t conn)
{
    uid_t c_euid;
    int   c_pid;
    c_euid  = xpc_connection_get_euid(conn);
    c_pid   = xpc_connection_get_pid(conn);

    if (c_euid != 0 || !IsEntitled(conn, kDNSCTLService))
    {   
        LogMsg("accept_client: Client PID[%d] is missing Entitlement or is NOT running as root!", c_pid);
        xpc_connection_cancel(conn);
        return; 
    }
    
    xpc_retain(conn);
    xpc_connection_set_target_queue(conn, dnsctlserver_queue);
    xpc_connection_set_event_handler(conn, ^(xpc_object_t req_msg)
        {
            xpc_type_t type = xpc_get_type(req_msg);

            if (type == XPC_TYPE_DICTIONARY)
            {
                handle_requests(req_msg);
            }
            else // We hit this case ONLY if Client Terminated Connection OR Crashed
            {
                LogInfo("accept_client: Client %p teared down the connection", (void *) conn);
                handle_terminate();
                xpc_release(conn);
            }
        });
    
    xpc_connection_resume(conn);
                
}

mDNSexport void init_dnsctl_service(void)
{
    
    xpc_connection_t dnsctl_listener = xpc_connection_create_mach_service(kDNSCTLService, NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);
    if (!dnsctl_listener || xpc_get_type(dnsctl_listener) != XPC_TYPE_CONNECTION)
    {
        LogMsg("init_dnsctl_service: Error Creating XPC Listener for DNSCTL Server!");
        return;
    }
    
    dnsctlserver_queue = dispatch_queue_create("com.apple.mDNSResponder.dnsctlserver_queue", NULL);
    
    xpc_connection_set_event_handler(dnsctl_listener, ^(xpc_object_t eventmsg)
        {
            xpc_type_t type = xpc_get_type(eventmsg);

            if (type == XPC_TYPE_CONNECTION)
            {
                LogInfo("init_dnsctl_service: New DNSCTL Client %p", eventmsg);
                accept_client(eventmsg);
            }
            else if (type == XPC_TYPE_ERROR) // Ideally, we would never hit these cases
            {
                LogMsg("init_dnsctl_service: XPCError: %s", xpc_dictionary_get_string(eventmsg, XPC_ERROR_KEY_DESCRIPTION));
            }
            else
            {
                LogMsg("init_dnsctl_service: Unknown EventMsg type");
            }
        });
    
    xpc_connection_resume(dnsctl_listener);
}



