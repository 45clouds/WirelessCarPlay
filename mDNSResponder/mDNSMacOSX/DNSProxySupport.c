/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2011-2013 Apple Inc. All rights reserved.
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
#include "mDNSMacOSX.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/event.h>
#include <netinet/tcp.h>

#define ValidSocket(s) ((s) >= 0)

// Global to store the 4 DNS Proxy Listeners (UDPv4/6, TCPv4/6)
static int dp_listener[4];

#define NUM_PROXY_TCP_CONNS 100

typedef struct
{
    TCPSocket   sock;
    DNSMessage  *reply;
    mDNSu16     replyLen;
    mDNSu32     nread;
} ProxyTCPInfo_t;

// returns -1 for failures including the other end closing the socket
// returns 0 if successful in reading data, but still not read the data fully
// returns 1 if successful in reading all the data
mDNSlocal int ProxyTCPRead(ProxyTCPInfo_t *tcpInfo)
{
    long n;
    mDNSBool closed; 

    if (tcpInfo->nread < 2)         // First read the two-byte length preceeding the DNS message
    {
        mDNSu8 *lenptr = (mDNSu8 *)&tcpInfo->replyLen;
        n = mDNSPlatformReadTCP(&tcpInfo->sock, lenptr + tcpInfo->nread, 2 - tcpInfo->nread, &closed);
        if (n < 0 || closed)
        {
            LogMsg("ProxyTCPRead: attempt to read message length failed");
            return -1;
        }

        tcpInfo->nread += n;
        if (tcpInfo->nread < 2)
        {
            LogMsg("ProxyTCPRead: nread %d, n %d", tcpInfo->nread, n);
            return 0;
        }

        tcpInfo->replyLen = (mDNSu16)((mDNSu16)lenptr[0] << 8 | lenptr[1]);
        if (tcpInfo->replyLen < sizeof(DNSMessageHeader))
        {
            LogMsg("ProxyTCPRead: Message length too short (%d bytes)", tcpInfo->replyLen);
            return -1;
        }

        tcpInfo->reply = mallocL("ProxyTCPInfo", tcpInfo->replyLen);
        if (!tcpInfo->reply)
        {
            LogMsg("ProxyTCPRead: Memory failure");
            return -1;
        }
    }

    n = mDNSPlatformReadTCP(&tcpInfo->sock, ((char *)tcpInfo->reply) + (tcpInfo->nread - 2), tcpInfo->replyLen - (tcpInfo->nread - 2), &closed);

    if (n < 0 || closed)
    {
        LogMsg("ProxyTCPRead: read failure n %d, closed %d", n, closed);
        return -1;
    }
    tcpInfo->nread += n;
    if ((tcpInfo->nread - 2) != tcpInfo->replyLen)
        return 0;
    else 
        return 1;
}

mDNSlocal void ProxyTCPSocketCallBack(int s1, short filter, void *context)
{
    int ret;
    struct sockaddr_storage from;
    struct sockaddr_storage to;
    mDNSAddr senderAddr, destAddr;
    mDNSIPPort senderPort;
    ProxyTCPInfo_t *ti = (ProxyTCPInfo_t *)context;
    TCPSocket *sock = &ti->sock;
    KQSocketSet *kq = &sock->ss;
    struct tcp_info tcp_if;
    socklen_t size = sizeof(tcp_if);
    int32_t intf_id = 0;

    (void) filter;

    ret = ProxyTCPRead(ti);
    if (ret == -1)
    {
        mDNSPlatformDisposeProxyContext(ti);
        return; 
    }
    else if (!ret)
    {
        debugf("ProxyTCPReceive: Not yet read completely Actual length %d, Read length %d", ti->replyLen, ti->nread);
        return;
    }
    // We read all the data and hence not interested in read events anymore
    KQueueSet(s1, EV_DELETE, EVFILT_READ, sock->kqEntry);

    mDNSPlatformMemZero(&to, sizeof(to));
    mDNSPlatformMemZero(&from, sizeof(from));
    socklen_t len = sizeof(to);
    ret = getsockname(s1, (struct sockaddr*) &to, &len);
    if (ret < 0)
    {
        LogMsg("ProxyTCPReceive: getsockname(fd=%d) errno %d", s1, errno);
        mDNSPlatformDisposeProxyContext(ti);
        return;
    }
    ret = getpeername(s1, (struct sockaddr*) &from, &len);
    if (ret < 0)
    {
        LogMsg("ProxyTCPReceive: getpeername(fd=%d) errno %d", s1, errno);
        mDNSPlatformDisposeProxyContext(ti);
        return;
    }
    if (getsockopt(s1, IPPROTO_TCP, TCP_INFO, &tcp_if, &size) != 0)
    {
        LogMsg("ProxyTCPReceive: getsockopt for TCP_INFO failed (fd=%d) errno %d", s1, errno);
        return;
    }
    intf_id = tcp_if.tcpi_last_outif;

    if (from.ss_family == AF_INET)
    {
        struct sockaddr_in *s = (struct sockaddr_in*)&from;

        senderAddr.type = mDNSAddrType_IPv4;
        senderAddr.ip.v4.NotAnInteger = s->sin_addr.s_addr;
        senderPort.NotAnInteger = s->sin_port;

        s = (struct sockaddr_in *)&to;
        destAddr.type = mDNSAddrType_IPv4;
        destAddr.ip.v4.NotAnInteger = s->sin_addr.s_addr;

        LogInfo("ProxyTCPReceive received IPv4 packet(len %d) from %#-15a to %#-15a on skt %d %s ifindex %d",
                ti->replyLen, &senderAddr, &destAddr, s1, NULL, intf_id);
    }
    else if (from.ss_family == AF_INET6)
    {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)&from;
        senderAddr.type = mDNSAddrType_IPv6;
        senderAddr.ip.v6 = *(mDNSv6Addr*)&sin6->sin6_addr;
        senderPort.NotAnInteger = sin6->sin6_port;

        sin6 = (struct sockaddr_in6 *)&to;
        destAddr.type = mDNSAddrType_IPv6;
        destAddr.ip.v6 = *(mDNSv6Addr*)&sin6->sin6_addr;

        LogInfo("ProxyTCPReceive received IPv6 packet(len %d) from %#-15a to %#-15a on skt %d %s ifindex %d",
                ti->replyLen, &senderAddr, &destAddr, s1, NULL, intf_id);
    }
    else
    {
        LogMsg("ProxyTCPReceive from is unknown address family %d", from.ss_family);
        mDNSPlatformDisposeProxyContext(ti);
        return;
    }

    // We pass sock for the TCPSocket and the "ti" for context as that's what we want to free at the end.
    // In the UDP case, there is just a single socket and nothing to free. Hence, the context (last argument)
    // would be NULL.
    kq->m->p->TCPProxyCallback(kq->m, sock, ti->reply, (mDNSu8 *)ti->reply + ti->replyLen, &senderAddr, senderPort, &destAddr,
        UnicastDNSPort, (mDNSInterfaceID)(uintptr_t)intf_id, ti);
}

mDNSlocal void ProxyTCPAccept(int s1, short filter, void *context)
{
    int newfd;
    struct sockaddr_storage ss;
    socklen_t sslen = sizeof(ss);
    const int on = 1;
    KQSocketSet *listenSet = (KQSocketSet *)context;

    (void) filter;

    while ((newfd = accept(s1, (struct sockaddr *)&ss, &sslen)) != -1)
    {
        int err;
        int *s;
        KQueueEntry *k;
        KQSocketSet *kq;

        // Even though we just need a single KQueueEntry, for simplicity we re-use
        // the KQSocketSet
        ProxyTCPInfo_t *ti = mallocL("ProxyTCPContext", sizeof(ProxyTCPInfo_t));
        if (!ti)
        {
            LogMsg("ProxyTCPAccept: cannot allocate TCPSocket");
            close(newfd);
            return;
        }
        mDNSPlatformMemZero(ti, sizeof(ProxyTCPInfo_t));
        
        TCPSocket *sock = &ti->sock;

        kq = &sock->ss;
        kq->sktv4 = -1;
        kq->sktv6 = -1;
        kq->m = listenSet->m;

        fcntl(newfd, F_SETFL, fcntl(newfd, F_GETFL, 0) | O_NONBLOCK); // set non-blocking
        if (ss.ss_family == AF_INET)
        {
            s =  &kq->sktv4;
            k =  &kq->kqsv4;
            // Receive interface identifiers
            err = setsockopt(newfd, IPPROTO_IP, IP_RECVIF, &on, sizeof(on));
            if (err)
            {
                LogMsg("ProxyTCPAccept: IP_RECVIF %d errno %d (%s)", newfd, errno, strerror(errno));
                mDNSPlatformDisposeProxyContext(ti);
                close(newfd);
                return;
            }
        }
        else
        {
            s =  &kq->sktv6;
            k =  &kq->kqsv6;
            // We want to receive destination addresses and receive interface identifiers
            err = setsockopt(newfd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
            if (err)
            {
                LogMsg("ProxyTCPAccept: IP_RECVPKTINFO %d errno %d (%s)", newfd, errno, strerror(errno));
                mDNSPlatformDisposeProxyContext(ti);
                close(newfd);
                return;
            }
        }
        *s = newfd;
        // mDNSPlatformReadTCP/WriteTCP (unlike the UDP counterpart) does not provide the destination address
        // from which we can infer the destination address family. Hence we need to remember that here.
        // Instead of remembering the address family, we remember the right fd.
        sock->fd = newfd;
        sock->kqEntry = k;
        k->KQcallback = ProxyTCPSocketCallBack;
        k->KQcontext  = ti;
        k->KQtask     = "TCP Proxy packet reception";
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
        k->readSource = mDNSNULL;
        k->writeSource = mDNSNULL;
        k->fdClosed = mDNSfalse;
#endif
        KQueueSet(*s, EV_ADD, EVFILT_READ, k);
    }
}

mDNSlocal mStatus SetupUDPProxySocket(mDNS *const m, int skt, KQSocketSet *cp, u_short sa_family, mDNSBool useBackgroundTrafficClass)
{
    int         *s        = (sa_family == AF_INET) ? &cp->sktv4 : &cp->sktv6;
    KQueueEntry *k        = (sa_family == AF_INET) ? &cp->kqsv4 : &cp->kqsv6;
    const int on = 1;
    mStatus err = mStatus_NoError;

    cp->m = m;
    cp->closeFlag = mDNSNULL;

    // set default traffic class
    // setTrafficClass(skt, mDNSfalse);
    (void) useBackgroundTrafficClass;

    if (sa_family == AF_INET)
    {
        err = setsockopt(skt, IPPROTO_IP, IP_RECVDSTADDR, &on, sizeof(on));
        if (err < 0)
        {
            LogMsg("SetupUDPProxySocket: IP_RECVDSTADDR %d errno %d (%s)", skt, errno, strerror(errno));
            return err;
        }

        // We want to receive interface identifiers
        err = setsockopt(skt, IPPROTO_IP, IP_RECVIF, &on, sizeof(on));
        if (err < 0)
        {
            LogMsg("SetupUDPProxySocket: IP_RECVIF %d errno %d (%s)", skt, errno, strerror(errno));
            return err;
        }
    }
    else if (sa_family == AF_INET6)
    {
        // We want to receive destination addresses and receive interface identifiers
        err = setsockopt(skt, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
        if (err < 0)
        {
            LogMsg("SetupUDPProxySocket: IPV6_RECVPKTINFO %d errno %d (%s)", skt, errno, strerror(errno));
            return err;
        }

        // We want to receive packet hop count value so we can check it
        err = setsockopt(skt, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on, sizeof(on));
        if (err < 0)
        {
            LogMsg("SetupUDPProxySocket: IPV6_RECVHOPLIMIT %d errno %d (%s)", skt, errno, strerror(errno));
            return err;
        }
    }
    else
    {
        LogMsg("SetupUDPProxySocket: wrong family %d", sa_family);
        return -1;
    }

    if (fcntl(skt, F_SETFL, fcntl(skt, F_GETFL, 0) | O_NONBLOCK) < 0)
    {
        LogMsg("SetupUDPProxySocket: fnctl failed %d", errno);
        return -1;
    }

    *s = skt;
    //k->KQcallback = ProxyUDPSocketCallBack;
    k->KQcallback  = myKQSocketCallBack;
    k->KQcontext   = cp;
    k->KQtask      = "UDP Proxy packet reception";
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
    k->readSource  = mDNSNULL;
    k->writeSource = mDNSNULL;
    k->fdClosed    = mDNSfalse;
#endif

    KQueueSet(*s, EV_ADD, EVFILT_READ, k);

    return(err);
}

mDNSlocal mStatus SetupTCPProxySocket(mDNS *const m, int skt, KQSocketSet *cp, u_short sa_family, mDNSBool useBackgroundTrafficClass)
{
    int         *s        = (sa_family == AF_INET) ? &cp->sktv4 : &cp->sktv6;
    KQueueEntry *k        = (sa_family == AF_INET) ? &cp->kqsv4 : &cp->kqsv6;
    mStatus err;

    cp->m = m;
    // XXX may not be used by the TCP codepath 
    cp->closeFlag = mDNSNULL;

    // for TCP sockets, the traffic class is set once and not changed
    // setTrafficClass(skt, useBackgroundTrafficClass);
    (void) useBackgroundTrafficClass;

    // All the socket setup has already been done 
    err = listen(skt, NUM_PROXY_TCP_CONNS);
    if (err)
    {
        LogMsg("SetupTCPProxySocket: listen %d errno %d (%s)", skt, errno, strerror(errno));
        return err;
    }
    fcntl(skt, F_SETFL, fcntl(skt, F_GETFL, 0) | O_NONBLOCK); // set non-blocking
    
    *s = skt;
    k->KQcallback  = ProxyTCPAccept;
    k->KQcontext   = cp;
    k->KQtask      = "TCP Accept";
#ifdef MDNSRESPONDER_USES_LIB_DISPATCH_AS_PRIMARY_EVENT_LOOP_MECHANISM
    k->readSource  = mDNSNULL;
    k->writeSource = mDNSNULL;
    k->fdClosed    = mDNSfalse;
#endif
    KQueueSet(*s, EV_ADD, EVFILT_READ, k);
    return mStatus_NoError;
}

mDNSlocal void BindDPSocket(int fd, int sa_family)
{
    int err;
    const int on = 1;

    if (sa_family == AF_INET)
    {
        struct sockaddr_in addr;

        err = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
        if (err < 0) 
            LogMsg("BindDPSocket: setsockopt SO_REUSEPORT failed for IPv4 %d errno %d (%s)", fd, errno, strerror(errno));

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(53);

        err = bind(fd, (struct sockaddr*) &addr, sizeof(addr));
        if (err)
        {
            LogMsg("BindDPSocket: bind %d errno %d (%s)", fd, errno, strerror(errno));
            return;
        }
    }
    else
    {
        struct sockaddr_in6 addr6;

        // We want to receive only IPv6 packets. Without this option we get IPv4 packets too,
        // with mapped addresses of the form 0:0:0:0:0:FFFF:xxxx:xxxx, where xxxx:xxxx is the IPv4 address
        err = setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
        if (err < 0)
        {
            LogMsg("DPFBindSocket: setsockopt IPV6_V6ONLY %d errno %d (%s)", fd, errno, strerror(errno));
            return;
        }
        err = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
        if (err < 0)
            LogMsg("BindDPSocket: setsockopt SO_REUSEPORT failed for V6 %d errno %d (%s)", fd, errno, strerror(errno));

        memset(&addr6, 0, sizeof(addr6));
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(53);

        err = bind(fd, (struct sockaddr*) &addr6, sizeof(addr6));
        if (err)
        {
            LogMsg("BindDPSocket: bind6 %d errno %d (%s)", fd, errno, strerror(errno));
            return;
        }
    }
}

// Setup DNS Proxy Skts in main kevent loop and set the skt options
mDNSlocal void SetupDNSProxySkts(mDNS *const m, int fd[4])
{
    int i;
    mStatus err;
    KQSocketSet *udpSS;
    KQSocketSet *tcpSS;

    udpSS       = &m->p->UDPProxy.ss;
    tcpSS       = &m->p->TCPProxy.ss;
    udpSS->port = UnicastDNSPort;
    tcpSS->port = UnicastDNSPort;

    LogMsg("SetupDNSProxySkts: %d, %d, %d, %d", fd[0], fd[1], fd[2], fd[3]);

    // myKQSocketCallBack checks for proxy and calls the m->p->ProxyCallback instead of mDNSCoreReceive
    udpSS->proxy = mDNStrue;
    err = SetupUDPProxySocket(m, fd[0], udpSS, AF_INET, mDNSfalse);
    if (err)
        LogMsg("SetupDNSProxySkts: ERROR!! UDPv4 Socket");

    err = SetupUDPProxySocket(m, fd[1], udpSS, AF_INET6, mDNSfalse);
    if (err)
        LogMsg("SetupDNSProxySkts: ERROR!! UDPv6 Socket");

    err = SetupTCPProxySocket(m, fd[2], tcpSS, AF_INET, mDNSfalse);
    if (err)
        LogMsg("SetupDNSProxySkts: ERROR!! TCPv4 Socket");

    err = SetupTCPProxySocket(m, fd[3], tcpSS, AF_INET6, mDNSfalse);
    if (err)
        LogMsg("SetupDNSProxySkts: ERROR!! TCPv6 Socket");

    for (i = 0; i < 4; i++)
        dp_listener[i] = fd[i];   
} 

// Create and bind the DNS Proxy Skts for use
mDNSexport void mDNSPlatformInitDNSProxySkts(mDNS *const m, ProxyCallback UDPCallback, ProxyCallback TCPCallback)
{
    int dpskt[4];
    
    dpskt[0] = socket(AF_INET,  SOCK_DGRAM,  IPPROTO_UDP);
    dpskt[1] = socket(AF_INET6, SOCK_DGRAM,  IPPROTO_UDP);
    dpskt[2] = socket(AF_INET,  SOCK_STREAM, IPPROTO_TCP);
    dpskt[3] = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

    // Close all DNS Proxy skts in case any of them are invalid
    if (!ValidSocket(dpskt[0]) || !ValidSocket(dpskt[1]) ||
        !ValidSocket(dpskt[2]) || !ValidSocket(dpskt[3]))
    {   
        if (ValidSocket(dpskt[0]))
            close(dpskt[0]);
        if (ValidSocket(dpskt[1]))
            close(dpskt[1]);
        if (ValidSocket(dpskt[2]))
            close(dpskt[2]);
        if (ValidSocket(dpskt[3]))
            close(dpskt[3]);
    }

    BindDPSocket(dpskt[0], AF_INET);
    BindDPSocket(dpskt[1], AF_INET6);
    BindDPSocket(dpskt[2], AF_INET);
    BindDPSocket(dpskt[3], AF_INET6);

    LogInfo("mDNSPlatformInitDNSProxySkts: Opened Listener Sockets for DNS Proxy : %d, %d, %d, %d", 
             dpskt[0], dpskt[1], dpskt[2], dpskt[3]);

    m->p->UDPProxyCallback = UDPCallback;
    m->p->TCPProxyCallback = TCPCallback;

    SetupDNSProxySkts(m, dpskt);
}

mDNSexport void mDNSPlatformCloseDNSProxySkts(mDNS *const m)
{
    (void) m;
    int i;
    for (i = 0; i < 4; i++)
        close(dp_listener[i]);
    LogInfo("mDNSPlatformCloseDNSProxySkts: Closing DNS Proxy Listener Sockets");  
}

mDNSexport void mDNSPlatformDisposeProxyContext(void *context)
{
    ProxyTCPInfo_t *ti;
    TCPSocket *sock;
    KQSocketSet *kq;

    if (!context)
        return;

    ti = (ProxyTCPInfo_t *)context;
    sock = &ti->sock;

    kq = &sock->ss;
    if (kq->sktv4 != -1)
    {
        shutdown(kq->sktv4, 2);
        mDNSPlatformCloseFD(&kq->kqsv4, kq->sktv4);
    }
    if (kq->sktv6 != -1)
    {
        shutdown(kq->sktv6, 2);
        mDNSPlatformCloseFD(&kq->kqsv6, kq->sktv6);
    }
    if (kq->closeFlag)
        *kq->closeFlag = 1;

    if (ti->reply)
        freeL("ProxyTCPInfoLen", ti->reply);
    freeL("ProxyTCPContext", ti);
}

