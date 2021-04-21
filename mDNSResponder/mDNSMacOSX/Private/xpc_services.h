/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2012 Apple Inc. All rights reserved.
 *
 *
 *
 *    File:       xpc_services.h
 *
 *    Contains:   Interfaces necessary to talk to xpc_services.c
 *
 */

#ifndef XPC_SERVICES_H
#define XPC_SERVICES_H

#include "mDNSEmbeddedAPI.h"
#include <xpc/xpc.h>

extern void xpc_server_init(void);
extern void xpcserver_info(mDNS *const m);

extern mDNSBool IsEntitled(xpc_connection_t conn, const char *password);
extern void init_dnsctl_service(void);

extern void INFOCallback(void);

#endif // XPC_SERVICES_H
