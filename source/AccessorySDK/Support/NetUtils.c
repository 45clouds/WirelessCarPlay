/*
	File:    	NetUtils.c
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	410.12
	
	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
	Apple software is governed by and subject to the terms and conditions of your MFi License,
	including, but not limited to, the restrictions specified in the provision entitled ”Public 
	Software”, and is further subject to your agreement to the following additional terms, and your 
	agreement that the use, installation, modification or redistribution of this Apple software
	constitutes acceptance of these additional terms. If you do not agree with these additional terms,
	please do not use, install, modify or redistribute this Apple software.
	
	Subject to all of these terms and in consideration of your agreement to abide by them, Apple grants
	you, for as long as you are a current and in good-standing MFi Licensee, a personal, non-exclusive 
	license, under Apple's copyrights in this original Apple software (the "Apple Software"), to use, 
	reproduce, and modify the Apple Software in source form, and to use, reproduce, modify, and 
	redistribute the Apple Software, with or without modifications, in binary form. While you may not 
	redistribute the Apple Software in source form, should you redistribute the Apple Software in binary
	form, you must retain this notice and the following text and disclaimers in all such redistributions
	of the Apple Software. Neither the name, trademarks, service marks, or logos of Apple Inc. may be
	used to endorse or promote products derived from the Apple Software without specific prior written
	permission from Apple. Except as expressly stated in this notice, no other rights or licenses, 
	express or implied, are granted by Apple herein, including but not limited to any patent rights that
	may be infringed by your derivative works or by other works in which the Apple Software may be 
	incorporated.  
	
	Unless you explicitly state otherwise, if you provide any ideas, suggestions, recommendations, bug 
	fixes or enhancements to Apple in connection with this software (“Feedback”), you hereby grant to
	Apple a non-exclusive, fully paid-up, perpetual, irrevocable, worldwide license to make, use, 
	reproduce, incorporate, modify, display, perform, sell, make or have made derivative works of,
	distribute (directly or indirectly) and sublicense, such Feedback in connection with Apple products 
	and services. Providing this Feedback is voluntary, but if you do provide Feedback to Apple, you 
	acknowledge and agree that Apple may exercise the license granted above without the payment of 
	royalties or further consideration to Participant.
	
	The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES NO WARRANTIES, EXPRESS OR 
	IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY 
	AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR
	IN COMBINATION WITH YOUR PRODUCTS.
	
	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES 
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION 
	AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
	(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE 
	POSSIBILITY OF SUCH DAMAGE.
	
	Copyright (C) 2006-2016 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "NetUtils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CommonServices.h"
#include "DebugServices.h"

#if( TARGET_OS_DARWIN )
	#if( !COMMON_SERVICES_NO_CORE_SERVICES )
		#include <IOKit/IOBSD.h>
		#include <IOKit/IOKitLib.h>
		#include <IOKit/network/IOEthernetController.h>
		#include <IOKit/network/IOEthernetInterface.h>
		#include <IOKit/network/IONetworkInterface.h>
	#endif
	#if( !COMMON_SERVICES_NO_CORE_SERVICES && !COMMON_SERVICES_NO_SYSTEM_CONFIGURATION )
		#include <SystemConfiguration/SystemConfiguration.h>
	#endif
#endif


#if( TARGET_OS_POSIX )
	#include <sys/types.h>
	
	#include <arpa/inet.h>
	#include <fcntl.h>
	#include <ifaddrs.h>
	#include <net/if.h>
	#if( TARGET_OS_BSD || TARGET_OS_QNX )
		#include <net/if_dl.h>
		#include <net/if_media.h>
	#endif
	#if( TARGET_OS_FREEBSD )
		#include <net/if_var.h>
	#endif
	#if( TARGET_OS_LINUX )
		#include <linux/if_packet.h>
	#endif
	#if( TARGET_OS_NETBSD )
		#include <net/if_ether.h>
		#include <net/if_types.h>
	#endif
		#include <net/route.h>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <signal.h>
	#if( TARGET_OS_BSD )
		#include <sys/event.h>
	#endif
	#include <sys/ioctl.h>
	#include <sys/socket.h>
	#include <sys/sysctl.h>
	#include <sys/time.h>
	#include <sys/uio.h>
	#include <unistd.h>
#endif

#if( TARGET_OS_WINDOWS )
	#include <Iphlpapi.h>
	#include <wininet.h>
	#pragma comment( lib, "Iphlpapi.lib" )
	#pragma comment( lib, "wininet.lib" )
	#pragma comment( lib, "WS2_32.lib" )
#endif

#if( NETUTILS_USE_DNS_SD_GETADDRINFO )
	#include <dns_sd.h>
#endif
#include "CFUtils.h"
#include "MiscUtils.h"
#include "StringUtils.h"
#include "TickUtils.h"

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	DrainUDPSocket
//===========================================================================================================================

OSStatus	DrainUDPSocket( SocketRef inSock, int inTimeoutMs, int *outDrainedPackets )
{
	OSStatus			err;
	uint64_t			deadlineTicks;
	int					timeoutSecs;
	int					timeoutUSecs;
	fd_set				readSet;
	struct timeval		timeout;
	ssize_t				n;
	char				buf[ 32 ];
	int					drained;
	sockaddr_ip			sip;
	socklen_t			len;
	
	deadlineTicks = UpTicks() + MillisecondsToUpTicks( (uint64_t) inTimeoutMs );
	timeoutSecs   =   inTimeoutMs / kMillisecondsPerSecond;
	timeoutUSecs  = ( inTimeoutMs % kMillisecondsPerSecond ) * kMicrosecondsPerMillisecond;
	
	drained = 0;
	FD_ZERO( &readSet );
	do
	{
		FD_SET( inSock, &readSet );
		timeout.tv_sec  = timeoutSecs;
		timeout.tv_usec = timeoutUSecs;
		n = select( (int)( inSock + 1 ), &readSet, NULL, NULL, &timeout );
		if( n == 0 ) break;
		err = map_global_value_errno( n == 1, n );
		require_noerr( err, exit );
		
		len = (socklen_t) sizeof( sip );
		n = recvfrom( inSock, buf, (int) sizeof( buf ), 0, &sip.sa, &len );
		err = map_socket_value_errno( inSock, n >= 0, n );
		require_noerr( err, exit );
		++drained;
		
	}	while( UpTicks() < deadlineTicks );
	
	err = kNoErr;
	
exit:
	if( outDrainedPackets ) *outDrainedPackets = drained;
	return( err );
}

#if( TARGET_OS_POSIX )
//===========================================================================================================================
//	GetLoopbackInterfaceInfo
//===========================================================================================================================

OSStatus	GetLoopbackInterfaceInfo( char *inNameBuf, size_t inMaxLen, uint32_t *outIndex )
{
	OSStatus				err;
	struct ifaddrs *		ifaList;
	struct ifaddrs *		ifa;
	
	err = getifaddrs( &ifaList );
	require_noerr( err, exit );
	
	for( ifa = ifaList; ifa; ifa = ifa->ifa_next )
	{
		if( ifa->ifa_name && ( ifa->ifa_flags & IFF_LOOPBACK ) )
		{
			if( inNameBuf ) strlcpy( inNameBuf, ifa->ifa_name, inMaxLen );
			if( outIndex )  *outIndex = if_nametoindex( ifa->ifa_name );
			break;
		}
	}
	freeifaddrs( ifaList );
	require_action( ifa, exit, err = kNotFoundErr );
	
exit:
	return( err );
}
#endif

//===========================================================================================================================
//	_GetPrimaryMACAddress (Platform-specific versions)
//===========================================================================================================================

#if  ( TARGET_OS_DARWIN || TARGET_OS_FREEBSD || TARGET_OS_QNX )
#define HasBuiltInGetPrimaryMACAddress		1
static OSStatus	_GetPrimaryMACAddress( uint8_t outMAC[ 6 ] )
{
	OSStatus					err;
	struct ifaddrs *			iaList;
	const struct ifaddrs *		ia;
	
	iaList = NULL;
	err = getifaddrs( &iaList );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
	
	for( ia = iaList; ia; ia = ia->ifa_next )
	{
		const struct sockaddr_dl *		sdl;
		
		if( !( ia->ifa_flags & IFF_UP ) )			continue; // Skip inactive.
		if( !ia->ifa_addr )							continue; // Skip no addr.
		if( ia->ifa_addr->sa_family != AF_LINK )	continue; // Skip non-AF_LINK.
		sdl = (const struct sockaddr_dl *) ia->ifa_addr;
		if( sdl->sdl_alen != 6 )					continue; // Skip wrong length.
		
		memcpy( outMAC, LLADDR( sdl ), 6 );
		break;
	}
	require_action( ia, exit, err = kNotFoundErr );
	
exit:
	if( iaList ) freeifaddrs( iaList );
	return( err );
}
#elif( TARGET_OS_MAC )
#define HasBuiltInGetPrimaryMACAddress		1
static OSStatus	_GetPrimaryMACAddress( uint8_t outMAC[ 6 ] )
{
	OSStatus					err;
	CFMutableDictionaryRef		matchingDict;
	CFMutableDictionaryRef		propertyMatchDict;
	io_iterator_t				serviceIter;
	io_object_t					service;
	Boolean						found;
	
	// Build a matching dictionary to find the primary Ethernet-style service.
	
	matchingDict = IOServiceMatching( kIOEthernetInterfaceClass );
	require_action( matchingDict, exit, err = kIOReturnNoMemory );
	
	propertyMatchDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( propertyMatchDict, exit, err = kIOReturnNoMemory );
	
	CFDictionarySetValue( propertyMatchDict, CFSTR( kIOPrimaryInterface ), kCFBooleanTrue );
	CFDictionarySetValue( matchingDict, CFSTR( kIOPropertyMatchKey ), propertyMatchDict );
	CFRelease( propertyMatchDict );
	
	err = IOServiceGetMatchingServices( kIOMasterPortDefault, matchingDict, &serviceIter ); // Note: consumes matchingDict.
	matchingDict = NULL;
	require_noerr_quiet( err, exit );
	require_action_quiet( serviceIter, exit, err = kIOReturnNotFound );
	
	// Search the returned services for a parent controller with a valid MAC address (should be the first and only one).
	
	found = false;
	while( ( service = IOIteratorNext( serviceIter ) ) != 0 )
	{
		io_object_t		controllerService;
		
		err = IORegistryEntryGetParentEntry( service, kIOServicePlane, &controllerService );
		check_noerr( err );
		if( !err )
		{
			CFDataRef		macData;
			
			macData = (CFDataRef) IORegistryEntryCreateCFProperty( controllerService, CFSTR( kIOMACAddress ), NULL, 0 );
			check( macData );
			if( macData )
			{
				check( CFGetTypeID( macData ) == CFDataGetTypeID() );
				if( CFGetTypeID( macData ) == CFDataGetTypeID() )
				{
					check_compile_time_code( kIOEthernetAddressSize == 6 );
					CFDataGetBytes( macData, CFRangeMake( 0, kIOEthernetAddressSize ), outMAC );
					found = true;
				}
				CFRelease( macData );
			}
		}
		
		IOObjectRelease( service );
		if( found ) break;
	}
	IOObjectRelease( serviceIter );
	
	err = found ? kIOReturnSuccess : kIOReturnNotFound;
	
exit:
	if( matchingDict ) CFRelease( matchingDict );
	return( err );
}
#elif( TARGET_OS_LINUX )
#define HasBuiltInGetPrimaryMACAddress		1
static OSStatus	_GetPrimaryMACAddress( uint8_t outMAC[ 6 ] )
{
	OSStatus					err;
	struct ifaddrs *			iaList;
	const struct ifaddrs *		ia;
	
	iaList = NULL;
	err = getifaddrs( &iaList );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
	
	for( ia = iaList; ia; ia = ia->ifa_next )
	{
		const struct sockaddr_ll *		sll;
		
		if( !( ia->ifa_flags & IFF_UP ) )			continue; // Skip inactive.
		if( ia->ifa_flags & IFF_LOOPBACK )			continue; // Skip loopback.
		if( !ia->ifa_addr )							continue; // Skip no addr.
		if( ia->ifa_addr->sa_family != AF_PACKET )	continue; // Skip non-AF_PACKET.
		sll = (const struct sockaddr_ll *) ia->ifa_addr;
		if( sll->sll_halen != 6 )					continue; // Skip wrong length.
		
		memcpy( outMAC, sll->sll_addr, 6 );
		break;
	}
	require_action( ia, exit, err = kNotFoundErr );
	
exit:
	if( iaList ) freeifaddrs( iaList );
	return( err );
}
#elif( TARGET_OS_WINDOWS )
#define HasBuiltInGetPrimaryMACAddress		1
static OSStatus	_GetPrimaryMACAddress( uint8_t outMAC[ 6 ] )
{
	OSStatus				err;
	IP_ADAPTER_INFO *		infoBuffer;
	ULONG					infoBufferSize;
	IP_ADAPTER_INFO *		info;
	int						i;
	
	err = ERROR_BUFFER_OVERFLOW;
	infoBuffer = NULL;
	infoBufferSize = sizeof( *infoBuffer ) * 8;
	for( i = 0; i < 100; ++i )
	{
		infoBuffer = (IP_ADAPTER_INFO *) calloc( 1, infoBufferSize );
		require_action( infoBuffer, exit, err = ERROR_NOT_ENOUGH_MEMORY );
		
		err = GetAdaptersInfo( infoBuffer, &infoBufferSize );
		if( err == ERROR_SUCCESS ) break;
		
		free( infoBuffer );
	}
	require_noerr( err, exit );
	
	err = ERROR_NO_DATA;
	for( info = infoBuffer; info; info = info->Next )
	{
		if( info->AddressLength == 6 )
		{
			memcpy( outMAC, info->Address, 6 );
			err = ERROR_SUCCESS;
			break;
		}
	}
	free( infoBuffer );
	
exit:
	return( err );
}
#endif

//===========================================================================================================================
//	GetPrimaryMACAddress
//===========================================================================================================================

uint64_t	GetPrimaryMACAddress( uint8_t outMAC[ 6 ], OSStatus *outErr )
{
	OSStatus		err;
	uint8_t			mac[ 6 ];
	uint64_t		u64;
	
	if( !outMAC ) outMAC = mac;
	memset( outMAC, 0, 6 );
#if( HasBuiltInGetPrimaryMACAddress )
	err = _GetPrimaryMACAddress( outMAC );
#else
	err = GetPrimaryMACAddressPlatform( outMAC );
#endif
	u64 = ReadBig48( outMAC );
	if( outErr ) *outErr = err;
	return( u64 );
}

//===========================================================================================================================
//	OpenSelfConnectedLoopbackSocket
//===========================================================================================================================

OSStatus	OpenSelfConnectedLoopbackSocket( SocketRef *outSock )
{
	OSStatus		err;
	SocketRef		sock;
	sockaddr_ip		sip;
	socklen_t		len;
	
	sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	err = map_socket_creation_errno( sock );
	require_noerr( err, exit );
	
	memset( &sip.v4, 0, sizeof( sip.v4 ) );
	sip.v4.sin_family		= AF_INET;
	sip.v4.sin_port			= 0;
	sip.v4.sin_addr.s_addr	= htonl( INADDR_LOOPBACK );
	err = bind( sock, &sip.sa, sizeof( sip.v4 ) );
	err = map_socket_noerr_errno( sock, err );
	require_noerr( err, exit );
	
	len = (socklen_t) sizeof( sip );
	err = getsockname( sock, &sip.sa, &len );
	err = map_socket_noerr_errno( sock, err );
	require_noerr( err, exit );
	
	err = connect( sock, &sip.sa, len );
	err = map_socket_noerr_errno( sock, err );
	require_noerr( err, exit );
	
	*outSock = sock;
	sock = kInvalidSocketRef;
	
exit:
	ForgetSocket( &sock );
	return( err );
}

//===========================================================================================================================
//	SendSelfConnectedLoopbackMessage
//===========================================================================================================================

OSStatus	SendSelfConnectedLoopbackMessage( SocketRef inSock, const void *inMsg, size_t inLen )
{
	OSStatus		err;
	sockaddr_ip		sip;
	socklen_t		len;
	ssize_t			n;
	
	len = (socklen_t) sizeof( sip );
	err = getsockname( inSock, &sip.sa, &len );
	err = map_socket_noerr_errno( inSock, err );
	require_noerr( err, exit );
	
	n = sendto( inSock, (char *) inMsg, inLen, 0, &sip.sa, (socklen_t) sizeof( sip.v4 ) );
	err = map_socket_value_errno( inSock, n == ( (ssize_t) inLen ), n );
	if( err == EISCONN )
	{
		n = send( inSock, (char *) inMsg, inLen, 0 );
		err = map_socket_value_errno( inSock, n == ( (ssize_t) inLen ), n );
	}
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	ServerSocketPairOpen
//===========================================================================================================================

OSStatus
	ServerSocketPairOpen( 
		int			inType, 
		int			inProtocol, 
		int			inPort, 
		int *		outPort, 
		int			inRcvBufSize, 
		SocketRef *	outSockV4, 
		SocketRef *	outSockV6 )
{
	OSStatus		err;
	OSStatus		err2;
	SocketRef		sockV4;
	SocketRef		sockV6;
	
	sockV4 = kInvalidSocketRef;
	sockV6 = kInvalidSocketRef;
	
	err  = ServerSocketOpen( AF_INET,  inType, inProtocol, inPort, &inPort, inRcvBufSize, &sockV4 );
#if( defined( AF_INET6 ) )
	err2 = ServerSocketOpen( AF_INET6, inType, inProtocol, inPort, &inPort, inRcvBufSize, &sockV6 );
#else
	err2 = kUnsupportedErr;
#endif
	require( !err || !err2, exit );
	
	if( outPort ) *outPort = inPort;
	*outSockV4 = sockV4;
	*outSockV6 = sockV6;
	sockV4 = kInvalidSocketRef;
	sockV6 = kInvalidSocketRef;
	err = kNoErr;
	
exit:
	ForgetSocket( &sockV4 );
	ForgetSocket( &sockV6 );
	return( err );
}

//===========================================================================================================================
//	ServerSocketOpen
//===========================================================================================================================

OSStatus
	ServerSocketOpen( 
		int			inFamily, 
		int			inType, 
		int			inProtocol, 
		int			inPort, 
		int *		outPort, 
		int			inRcvBufSize, 
		SocketRef *	outSock )
{
	OSStatus		err;
	int				port;
	SocketRef		sock;
	int				name;
	int				option;
	sockaddr_ip		sip;
	socklen_t		len;
	
	port = ( inPort < 0 ) ? -inPort : inPort; // Negated port number means "try this port, but allow dynamic".
	
	sock = socket( inFamily, inType, inProtocol );
	err = map_socket_creation_errno( sock );
	require_noerr_quiet( err, exit );
	
#if( defined( SO_NOSIGPIPE ) )
	setsockopt( sock, SOL_SOCKET, SO_NOSIGPIPE, &(int){ 1 }, (socklen_t) sizeof( int ) );
#endif
	
	err = SocketMakeNonBlocking( sock );
	require_noerr( err, exit );
	
	// Set receive buffer size. This has to be done on the listening socket *before* listen is called because
	// accept does not return until after the window scale option is exchanged during the 3-way handshake. 
	// Since accept returns a new socket, the only way to use a larger window scale option is to set the buffer
	// size on the listening socket since SO_RCVBUF is inherited by the accepted socket. See UNPv1e3 Section 7.5.
	
	err = SocketSetBufferSize( sock, SO_RCVBUF, inRcvBufSize );
	check_noerr( err );
	
	// Allow port or address reuse because we may bind separate IPv4 and IPv6 sockets to the same port.
	
	option = 1;
	name = ( inType == SOCK_DGRAM ) ? SO_REUSEPORT : SO_REUSEADDR;
	err = setsockopt( sock, SOL_SOCKET, name, (char *) &option, (socklen_t) sizeof( option ) );
	err = map_socket_noerr_errno( sock, err );
	require_noerr( err, exit );
	
	if( inFamily == AF_INET )
	{
		// Bind to the port. If it fails, retry with a dynamic port.
		
		memset( &sip.v4, 0, sizeof( sip.v4 ) );
		SIN_LEN_SET( &sip.v4 );
		sip.v4.sin_family		= AF_INET;
		sip.v4.sin_port			= htons( (uint16_t) port );
		sip.v4.sin_addr.s_addr	= htonl( INADDR_ANY );
		err = bind( sock, &sip.sa, (socklen_t) sizeof( sip.v4 ) );
		err = map_socket_noerr_errno( sock, err );
		if( err && ( inPort < 0 ) )
		{
			sip.v4.sin_port = 0;
			err = bind( sock, &sip.sa, (socklen_t) sizeof( sip.v4 ) );
			err = map_socket_noerr_errno( sock, err );
		}
		require_noerr( err, exit );
	}
#if( defined( AF_INET6 ) )
	else if( inFamily == AF_INET6 )
	{
		// Restrict this socket to IPv6 only because we're going to use a separate socket for IPv4.
		
		option = 1;
		err = setsockopt( sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *) &option, (socklen_t) sizeof( option ) );
		err = map_socket_noerr_errno( sock, err );
		require_noerr( err, exit );
		
		// Bind to the port. If it fails, retry with a dynamic port.
		
		memset( &sip.v6, 0, sizeof( sip.v6 ) );
		SIN6_LEN_SET( &sip.v6 );
		sip.v6.sin6_family	= AF_INET6;
		sip.v6.sin6_port	= htons( (uint16_t) port );
		sip.v6.sin6_addr	= in6addr_any;	
		err = bind( sock, &sip.sa, (socklen_t) sizeof( sip.v6 ) );
		err = map_socket_noerr_errno( sock, err );
		if( err && ( inPort < 0 ) )
		{
			sip.v6.sin6_port = 0;
			err = bind( sock, &sip.sa, (socklen_t) sizeof( sip.v6 ) );
			err = map_socket_noerr_errno( sock, err );
		}
		require_noerr( err, exit );
	}
#endif
	else
	{
		dlogassert( "Unsupported family: %d", inFamily );
		err = kUnsupportedErr;
		goto exit;
	}
	
	if( inType == SOCK_STREAM )
	{
		err = listen( sock, SOMAXCONN );
		err = map_socket_noerr_errno( sock, err );
		if( err )
		{
			err = listen( sock, 5 );
			err = map_socket_noerr_errno( sock, err );
			require_noerr( err, exit );
		}
	}
	
	if( outPort )
	{
		len = (socklen_t) sizeof( sip );
		err = getsockname( sock, &sip.sa, &len );
		err = map_socket_noerr_errno( sock, err );
		require_noerr( err, exit );
		
		*outPort = SockAddrGetPort( &sip );
	}
	*outSock = sock;
	sock = kInvalidSocketRef;
	
exit:
	ForgetSocket( &sock );
	return( err );
}

//===========================================================================================================================
//	UpdateIOVec
//===========================================================================================================================

OSStatus	UpdateIOVec( iovec_t **ioArray, int *ioCount, size_t inAmount )
{
	iovec_t *		ptr;
	iovec_t *		end;
	
	ptr = *ioArray;
	end = ptr + *ioCount;
	while( ptr < end )
	{
		size_t		len;
		
		len = ptr->iov_len;
		if( inAmount >= len )
		{
			ptr->iov_base = ( (char *)( ptr->iov_base ) ) + len;
			ptr->iov_len  = 0;
			++ptr;
			inAmount -= len;
		}
		else
		{
			ptr->iov_base = ( (char *)( ptr->iov_base ) ) + inAmount;
			ptr->iov_len  = (unsigned int)( len - inAmount );
			
			*ioArray = ptr;
			*ioCount = (int)( end - ptr );
			return( EWOULDBLOCK );
		}
	}
	return( kNoErr );
}

//===========================================================================================================================
//	writev
//===========================================================================================================================

#if( TARGET_OS_WINDOWS )
ssize_t	writev( SocketRef inSock, const iovec_t *inArray, int inCount )
{
	int			err;
	DWORD		n;
	
	err = WSASend( inSock, (iovec_t *) inArray, inCount, &n, 0, NULL, NULL );
	return( err ? err : n );
}
#endif

#if 0
#pragma mark -
#endif

#if( NETUTILS_USE_DNS_SD_GETADDRINFO )
//===========================================================================================================================
//	getaddrinfo_dnssd
//===========================================================================================================================

typedef struct
{
	const struct addrinfo *		hints;
	int							port;
	struct addrinfo *			results;
	Boolean						moreComing;
	Boolean						gotV4;
	Boolean						gotV6;
	
}	getaddrinfo_context;

static void DNSSD_API
	getaddrinfo_dnssd_callback(
		DNSServiceRef				inServiceRef,
		DNSServiceFlags				inFlags,
		uint32_t					inIFI,
		DNSServiceErrorType			inErrorCode,
		const char *				inHostName,
		const struct sockaddr *		inAddr,
		uint32_t					inTTL,
		void *						inContext );

int
	getaddrinfo_dnssd( 
		const char *			inNode, 
		const char *			inService, 
		const struct addrinfo *	inHints, 
		struct addrinfo **		outResults )
{
	OSStatus				err;
	DNSServiceProtocol		protocols;
	DNSServiceRef			queryRef;
	SocketRef				querySock;
	getaddrinfo_context		context;
	fd_set					readSet;
	int						n;
	struct timeval			timeout;
	
	queryRef = NULL;
	
	context.hints		= inHints;
	context.results		= NULL;
	context.gotV4		= false;
#if( defined( AF_INET6 ) )
	context.gotV6		= false;
#else
	context.gotV6		= true; // Skip IPv6 checks if we don't support IPv6.
#endif
	context.moreComing	= false;
	context.port		= 0;
	if( inService ) sscanf( inService, "%d", &context.port );
	
	protocols = 0; // Default to letting mDNSResponder intelligently decide the protocol.
	if( inHints )
	{
		if(      inHints->ai_family == AF_INET  ) protocols = kDNSServiceProtocol_IPv4;
		#if( defined( AF_INET6 ) )
		else if( inHints->ai_family == AF_INET6 ) protocols = kDNSServiceProtocol_IPv6;
		#endif
	}
	
	// Start an async address query.
	
	err = DNSServiceGetAddrInfo( &queryRef, kDNSServiceFlagsReturnIntermediates, kDNSServiceInterfaceIndexAny, 
		protocols, inNode, getaddrinfo_dnssd_callback, &context );
	require_noerr( err, exit );
	
	// Wait for the Bonjour response.
	
	querySock = DNSServiceRefSockFD( queryRef );
	if( IsValidSocket( querySock ) )
	{
		FD_ZERO( &readSet );
		while( context.moreComing || !context.gotV4 || !context.gotV6 )
		{
			FD_SET( querySock, &readSet );
			timeout.tv_sec  = 5;
			timeout.tv_usec = 0;
			n = select( querySock + 1, &readSet, NULL, NULL, &timeout );
			if( n > 0 )
			{
				err = DNSServiceProcessResult( queryRef );
				require_noerr( err, exit );
			}
			else if( n == 0 ) // Timeout
			{
				break;
			}
			else
			{
				err = global_value_errno( n );
				if( err == EINTR ) continue;
				dlogassert( "select failed: %#m", err );
				break;
			}
		}
	}
	else
	{
		uint64_t		timeoutTicks;
		
		// No socket so assume we're using the DNS-SD shim that runs async and poll for completion.
		
		timeoutTicks = UpTicks() + ( 5 * UpTicksPerSecond() );
		while( context.moreComing || !context.gotV4 || !context.gotV6 )
		{
			if( UpTicks() >= timeoutTicks ) break;
			usleep( 50000 );
		}
	}
	require_action_quiet( context.results, exit, err = kNotFoundErr );
	
	*outResults = context.results;
	context.results = NULL;
	
exit:
	if( queryRef )			DNSServiceRefDeallocate( queryRef );
	if( context.results )	freeaddrinfo_dnssd( context.results );
	return( (int) err );
}

//===========================================================================================================================
//	getaddrinfo_dnssd_callback
//===========================================================================================================================

static void DNSSD_API
	getaddrinfo_dnssd_callback(
		DNSServiceRef				inServiceRef,
		DNSServiceFlags				inFlags,
		uint32_t					inIFI,
		DNSServiceErrorType			inErrorCode,
		const char *				inHostName,
		const struct sockaddr *		inAddr,
		uint32_t					inTTL,
		void *						inContext )
{
	getaddrinfo_context * const		context = (getaddrinfo_context *) inContext;
	const sockaddr_ip * const		sip		= (const sockaddr_ip *) inAddr;
	const sockaddr_ip *				tempSIP;
	struct addrinfo **				next;
	struct addrinfo *				curr;
	struct addrinfo *				newAddr;
	
	(void) inServiceRef;	// Unused
	(void) inHostName;		// Unused
	(void) inTTL;			// Unused
	
	newAddr = NULL;
	
	if( inErrorCode == -65791 ) goto exit; // Ignore mStatus_ConfigChanged
	require( inAddr, exit );
	#if( defined( AF_INET6 ) )
	require( ( inAddr->sa_family == AF_INET ) || ( inAddr->sa_family == AF_INET6 ), exit );
	#else
	require( inAddr->sa_family == AF_INET, exit );
	#endif
	
	if( inErrorCode == kDNSServiceErr_NoError )
	{
		require_quiet( inFlags & kDNSServiceFlagsAdd, exit );
		
		// Allocate a new address and fill it in.
		
		newAddr = (struct addrinfo *) calloc( 1, sizeof( *newAddr ) );
		require( newAddr, exit );
		
		newAddr->ai_family = sip->sa.sa_family;
		if( context->hints )
		{
			newAddr->ai_socktype = context->hints->ai_socktype;
			newAddr->ai_protocol = context->hints->ai_protocol;
		}
		if(      sip->sa.sa_family == AF_INET  ) newAddr->ai_addrlen = (socklen_t) sizeof( struct sockaddr_in );
		#if( defined( AF_INET6 ) )
		else if( sip->sa.sa_family == AF_INET6 ) newAddr->ai_addrlen = (socklen_t) sizeof( struct sockaddr_in6 );
		#endif
		else
		{
			dlogassert( "unknown sa_family: %d", sip->sa.sa_family );
			newAddr->ai_addrlen = (socklen_t) sizeof( struct sockaddr );
		}
		
		newAddr->ai_addr = (struct sockaddr *) malloc( sizeof( *sip ) );
		require( newAddr->ai_addr, exit );
		memcpy( newAddr->ai_addr, sip, sizeof( *sip ) );
		SockAddrSetPort( newAddr->ai_addr, context->port );
		#if( defined( AF_INET6 ) )
		{
			sockaddr_ip *		newSIP;
			
			newSIP = (sockaddr_ip *) newAddr->ai_addr;
			if( ( newSIP->sa.sa_family == AF_INET6 ) && IN6_IS_ADDR_LINKLOCAL( &newSIP->v6.sin6_addr ) )
			{
				newSIP->v6.sin6_scope_id = inIFI;
			}
		}
		#endif
		
		// Add to the list. Prioritize as IPv6 link local, IPv4 routable, and then others.
		
		if( ( sip->sa.sa_family == AF_INET ) && !SockAddrIsLinkLocal( sip ) )
		{
			for( next = &context->results; ( curr = *next ) != NULL; next = &curr->ai_next )
			{
				tempSIP = (const sockaddr_ip *) curr->ai_addr;
				#if( defined( AF_INET6 ) )
					if( !( ( tempSIP->sa.sa_family == AF_INET6 ) && IN6_IS_ADDR_LINKLOCAL( &tempSIP->v6.sin6_addr ) ) &&
						!( ( tempSIP->sa.sa_family == AF_INET  ) && !SockAddrIsLinkLocal( tempSIP ) ) )
				#else
					if( !( ( tempSIP->sa.sa_family == AF_INET  ) && !SockAddrIsLinkLocal( tempSIP ) ) )
				#endif
				{
					break;
				}
			}
			newAddr->ai_next = curr;
			*next = newAddr;
		}
		#if( defined( AF_INET6 ) )
		else if( ( sip->sa.sa_family == AF_INET6 ) && IN6_IS_ADDR_LINKLOCAL( &sip->v6.sin6_addr ) )
		{
			newAddr->ai_next = context->results;
			context->results = newAddr;
		}
		#endif
		else
		{
			for( next = &context->results; ( curr = *next ) != NULL; next = &curr->ai_next ) {}
			*next = newAddr;
		}
		newAddr = NULL;
	}
	else if( inErrorCode != kDNSServiceErr_NoSuchRecord )
	{
		dlogassert( "'%s' failed: %#m", inHostName, (OSStatus) inErrorCode );
		goto exit;
	}
	
	if(      sip->sa.sa_family == AF_INET  ) context->gotV4 = true;
#if( defined( AF_INET6 ) )
	else if( sip->sa.sa_family == AF_INET6 ) context->gotV6 = true;
#endif
	context->moreComing = ( inFlags & kDNSServiceFlagsMoreComing ) ? true : false;
	
exit:
	if( newAddr ) freeaddrinfo_dnssd( newAddr );
}

//===========================================================================================================================
//	freeaddrinfo_dnssd
//===========================================================================================================================

void	freeaddrinfo_dnssd( struct addrinfo *inAddrs )
{
	struct addrinfo *		ai;
	
	while( ( ai = inAddrs ) != NULL )
	{
		inAddrs = ai->ai_next;
		if( ai->ai_addr ) free( ai->ai_addr );
		free( ai );
	}
}
#endif // NETUTILS_USE_DNS_SD_GETADDRINFO

#if 0
#pragma mark -
#pragma mark == NetSocket ==
#endif

//===========================================================================================================================
//	NetSocket
//===========================================================================================================================

static OSStatus
	_NetSocket_Connect( 
		NetSocketRef	inSock, 
		SocketRef		inNativeSock, 
		const void *	inSockAddr, 
		int32_t			inTimeoutSecs );

ulog_define( NetSocket, kLogLevelInfo, kLogFlags_PrintTime, "", NULL );

//===========================================================================================================================
//	NetSocket_Create
//===========================================================================================================================

OSStatus	NetSocket_Create( NetSocketRef *outSock )
{
	OSStatus			err;
	NetSocketRef		obj;
	
	// Allocate the object and initialize it enough that we can safely clean up on partial failures.
	
	obj = (NetSocketRef) calloc( 1, sizeof( *obj ) );
	require_action( obj, exit, err = kNoMemoryErr );
	
	obj->magic		= kNetSocketMagic;
	obj->nativeSock	= kInvalidSocketRef;
	
#if( TARGET_OS_POSIX )
	obj->sendCancel = kInvalidFD;
	obj->recvCancel = kInvalidFD;
#elif( !TARGET_OS_WINDOWS )
	obj->cancelSock = kInvalidSocketRef;
#endif
	
	// Set up cancel support.

#if( TARGET_OS_POSIX )
{
	int		pipeFDs[ 2 ];
	
	err = pipe( pipeFDs );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
	
	obj->sendCancel = pipeFDs[ 1 ];
	obj->recvCancel = pipeFDs[ 0 ];
	
	SocketMakeNonBlocking( obj->sendCancel );
	SocketMakeNonBlocking( obj->recvCancel );
}
#elif( TARGET_OS_WINDOWS )
	obj->sockEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	err = map_global_value_errno( obj->sockEvent, obj->sockEvent );
	require_noerr( err, exit );
	
	obj->cancelEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	err = map_global_value_errno( obj->cancelEvent, obj->cancelEvent );
	require_noerr( err, exit );
#else
{
	sockaddr_ip		sip;
	socklen_t		len;
	
	obj->cancelSock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	err = map_socket_creation_errno( obj->cancelSock );
	require_noerr( err, exit );
	
	SocketMakeNonBlocking( obj->cancelSock );
	
	memset( &sip.v4, 0, sizeof( sip.v4 ) );
	sip.v4.sin_family		= AF_INET;
	sip.v4.sin_port			= 0;
	sip.v4.sin_addr.s_addr	= htonl( INADDR_LOOPBACK );
	err = bind( obj->cancelSock, &sip.sa, sizeof( sip.v4 ) );
	err = map_socket_noerr_errno( obj->cancelSock, err );
	require_noerr( err, exit );
	
	len = (socklen_t) sizeof( sip );
	err = getsockname( obj->cancelSock, &sip.sa, &len );
	err = map_socket_noerr_errno( obj->cancelSock, err );
	require_noerr( err, exit );
	
	err = connect( obj->cancelSock, &sip.sa, len );
	err = map_socket_noerr_errno( obj->cancelSock, err );
	require_noerr( err, exit );
}
#endif
	
	obj->readFunc		= NetSocket_ReadInternal;
	obj->writeFunc		= NetSocket_WriteInternal;
#if( TARGET_OS_POSIX || TARGET_OS_WINDOWS )
	obj->writeVFunc		= NetSocket_WriteVInternal;
#else
	obj->writeVFunc		= NetSocket_WriteVSlow;
#endif
	
	*outSock = obj;
	obj = NULL;
	
exit:
	if( obj ) NetSocket_Delete( obj );
	return( err );
}

//===========================================================================================================================
//	NetSocket_CreateWithNative
//===========================================================================================================================

OSStatus	NetSocket_CreateWithNative( NetSocketRef *outSock, SocketRef inSock )
{
	OSStatus			err;
	NetSocketRef		sock;
	
	require_action( IsValidSocket( inSock ), exit, err = kParamErr );
	
	err = NetSocket_Create( &sock );
	require_noerr( err, exit );
	
	sock->nativeSock = inSock;
	*outSock = sock;
	
exit:
	return( err );
}

//===========================================================================================================================
//	NetSocket_Delete
//===========================================================================================================================

OSStatus	NetSocket_Delete( NetSocketRef inSock )
{
	OSStatus		err;
	
	require_action( inSock && ( inSock->magic == kNetSocketMagic ), exit, err = kBadReferenceErr );
	
	if( inSock->freeFunc ) inSock->freeFunc( inSock );	
	
	ForgetSocket( &inSock->nativeSock );
#if( TARGET_OS_POSIX )
	ForgetFD( &inSock->sendCancel );
	ForgetFD( &inSock->recvCancel );
#elif( TARGET_OS_WINDOWS )
	ForgetWinHandle( &inSock->sockEvent );
	ForgetWinHandle( &inSock->cancelEvent );
#else
	ForgetSocket( &inSock->cancelSock );
#endif
	ForgetMem( &inSock->readBuffer );
	inSock->magic = kNetSocketMagicBad;
	free( inSock );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	NetSocket_GetNative
//===========================================================================================================================

SocketRef	NetSocket_GetNative( NetSocketRef inSock )
{
	SocketRef		nativeSock;
	
	nativeSock = kInvalidSocketRef;
	require( inSock && ( inSock->magic == kNetSocketMagic ), exit );
	check_string( IsValidSocket( inSock->nativeSock ), "illegal when socket is not set up" );
	check_string( !inSock->canceled, "illegal when canceled" );
	
	nativeSock = inSock->nativeSock;
	
exit:
	return( nativeSock );
}

//===========================================================================================================================
//	NetSocket_Cancel
//===========================================================================================================================

OSStatus	NetSocket_Cancel( NetSocketRef inSock )
{
	OSStatus		err;
	
	require_action( inSock && ( inSock->magic == kNetSocketMagic ), exit, err = kBadReferenceErr );
	require_action_quiet( !inSock->canceled, exit, err = kNoErr );
	
	inSock->canceled = true;
	
#if( TARGET_OS_POSIX )
{
	ssize_t		n;
	
	n = write( inSock->sendCancel, "Q", 1 );
	err = map_global_value_errno( n == 1, n );
	require_noerr( err, exit );
}	
#elif( TARGET_OS_WINDOWS )
{
	BOOL		good;
	
	good = SetEvent( inSock->cancelEvent );
	err = map_global_value_errno( good, good );
	require_noerr( err, exit );
}
#else
{
	ssize_t		n;
	
	n = send( inSock->cancelSock, "Q", 1, 0 );
	err = map_socket_value_errno( inSock->cancelSock, n == 1, n );
	require_noerr( err, exit );
}
#endif
	
exit:
	return( err );
}

//===========================================================================================================================
//	NetSocket_Reset
//===========================================================================================================================

OSStatus	NetSocket_Reset( NetSocketRef inSock )
{
	OSStatus		err;
	
	require_action( inSock && ( inSock->magic == kNetSocketMagic ), exit, err = kBadReferenceErr );
	
#if( TARGET_OS_POSIX )
{
	ssize_t		i, n;
	char		buf[ 16 ];
	
	for( i = 0; i < 100; ++i )
	{
		n = read( inSock->recvCancel, buf, sizeof( buf ) );
		if( n <= 0 ) break;
	}
	check( i < 100 );
}	
#elif( TARGET_OS_WINDOWS )
{
	BOOL		good;
	
	good = ResetEvent( inSock->cancelEvent );
	err = map_global_value_errno( good, good );
	check_noerr( err );
}
#else
{
	ssize_t		i, n;
	char		buf[ 16 ];
	
	for( i = 0; i < 100; ++i )
	{
		n = recv( inSock->cancelSock, buf, sizeof( buf ), 0 );
		if( n <= 0 ) break;
	}
	check( i < 100 );
}
#endif
	
	inSock->canceled = false;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	NetSocket_TCPConnect
//===========================================================================================================================

OSStatus	NetSocket_TCPConnect( NetSocketRef inSock, const char *inHostList, int inDefaultPort, int32_t inTimeoutSecs )
{
	return( NetSocket_TCPConnectEx( inSock, kNetSocketConnect_NoFlags, inHostList, inDefaultPort, inTimeoutSecs, NULL, NULL ) );
}

//===========================================================================================================================
//	NetSocket_TCPConnectEx
//===========================================================================================================================

OSStatus
	NetSocket_TCPConnectEx( 
		NetSocketRef					inSock, 
		NetSocketConnectFlags			inFlags, 
		const char *					inHostList, 
		int								inDefaultPort, 
		int32_t							inTimeoutSecs, 
		NetSocket_SetOptionsCallBackPtr	inCallBack, 
		void *							inContext )
{
	OSStatus			err;
	SocketRef			nativeSock;
	const char *		ptr;
	const char *		end;
	
	require_action( inSock && ( inSock->magic == kNetSocketMagic ), exit, err = kBadReferenceErr );
	require_action_quiet( !inSock->canceled, exit, err = kCanceledErr );
	
	// Parse each comma-separated host from the list until we successfully connect, cancel, or hit the end of the list.
	
	nativeSock = kInvalidSocketRef;
	for( ptr = inHostList; *ptr != '\0'; ptr = end )
	{
		size_t			len;
		char			host[ kHostStringMaxSize ];
		sockaddr_ip		sip;
		size_t			sipLen;
		
		// Parse a host.
		
		for( end = ptr; ( *end != '\0' ) && ( *end != ',' ); ++end ) {}
		len = (size_t)( end - ptr );
		require_action_quiet( len < sizeof( host ), exit, err = kSizeErr );
		
		memcpy( host, ptr, len );
		host[ len ] = '\0';
		if( *end != '\0' ) ++end;
		
		// First try the host as a numeric address string.
		
		err = StringToSockAddr( host, &sip, sizeof( sip ), &sipLen );
		if( err == kNoErr )
		{
			nativeSock = socket( sip.sa.sa_family, SOCK_STREAM, IPPROTO_TCP );
			if( !IsValidSocket( nativeSock ) ) continue;
			
			if( inCallBack ) inCallBack( inSock, nativeSock, inContext );
			
			if( ( inFlags & kNetSocketConnect_ForcePort ) || ( SockAddrGetPort( &sip ) == 0 ) )
			{
				SockAddrSetPort( &sip, inDefaultPort );
			}
			
			err = _NetSocket_Connect( inSock, nativeSock, &sip, inTimeoutSecs );
			if( err == kNoErr ) break;
			
			close_compat( nativeSock );
			nativeSock = kInvalidSocketRef;
			
			if( err == kCanceledErr ) goto exit;
			continue;
		}
		
		// The host doesn't appear to be numeric so try it as a DNS name. Parse any colon-separated port first.
		
		#if( NETUTILS_HAVE_GETADDRINFO )
		{
			int			port;
			char *		portPtr;
		
			port = 0;
			for( portPtr = host; ( *portPtr != '\0' ) && ( *portPtr != ':' ); ++portPtr ) {}
			if( *portPtr == ':' )
			{
				*portPtr++ = '\0';
				port = (int) strtoul( portPtr, NULL, 10 );
			}
			if( ( inFlags & kNetSocketConnect_ForcePort ) || ( port == 0 ) )
			{
				port = inDefaultPort;
			}
			if( port > 0 )
			{
				char					portStr[ 32 ];
				struct addrinfo			hints;
				struct addrinfo *		aiList;
				struct addrinfo *		ai;
				
				// Try connecting to each resolved address until we successfully connect or hit the end of the list.
				
				snprintf( portStr, sizeof( portStr ), "%u", port );
				memset( &hints, 0, sizeof( hints ) );
				hints.ai_family	  = AF_UNSPEC;
				hints.ai_socktype = SOCK_STREAM;
				#if( NETUTILS_USE_DNS_SD_GETADDRINFO )
					err = getaddrinfo_dnssd( host, portStr, &hints, &aiList );
				#else
					err = getaddrinfo( host, portStr, &hints, &aiList );
				#endif
				if( err || !aiList ) continue;
				
				for( ai = aiList; ai; ai = ai->ai_next )
				{
					nativeSock = socket( ai->ai_family, ai->ai_socktype, ai->ai_protocol );
					if( !IsValidSocket( nativeSock ) ) continue;
					
					if( inCallBack ) inCallBack( inSock, nativeSock, inContext );
					
					err = _NetSocket_Connect( inSock, nativeSock, ai->ai_addr, inTimeoutSecs );
					if( err == kNoErr ) break;
					
					close_compat( nativeSock );
					nativeSock = kInvalidSocketRef;
					
					if( err == kCanceledErr ) break;
				}
				#if( NETUTILS_USE_DNS_SD_GETADDRINFO )
					freeaddrinfo_dnssd( aiList );
				#else
					freeaddrinfo( aiList );
				#endif
				if( err == kCanceledErr ) goto exit;
				if( IsValidSocket( nativeSock ) ) break;
			}
		}
		#endif
	}
	require_action_quiet( IsValidSocket( nativeSock ), exit, err = kConnectionErr );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_NetSocket_Connect
//===========================================================================================================================

static OSStatus
	_NetSocket_Connect( 
		NetSocketRef	inSock, 
		SocketRef		inNativeSock, 
		const void *	inSockAddr, 
		int32_t			inTimeoutSecs )
{
	OSStatus		err;
	int				tempInt;
	socklen_t		tempLen;
	
	require_action( inSock && ( inSock->magic == kNetSocketMagic ), exit, err = kBadReferenceErr );
	require_action_quiet( !inSock->canceled, exit, err = kCanceledErr );
	require_action( !IsValidSocket( inSock->nativeSock ), exit, err = kAlreadyInUseErr );
	
	err = SocketMakeNonBlocking( inNativeSock );
	require_noerr( err, exit );
	
	// Start the connection to the remote host. This may return 0 if it can connect immediately. Other errors
	// are ignored at this point because it varies between platforms. Real errors will be handled further down.
	
	err = connect( inNativeSock, (struct sockaddr *) inSockAddr, SockAddrGetSize( inSockAddr ) );
	if( err )
	{
		err = NetSocket_Wait( inSock, inNativeSock, kNetSocketWaitType_Connect, inTimeoutSecs );
		if( err ) goto exit;
		
		// Check if the connection was successful by checking the current pending socket error. Some sockets 
		// implementations return an error from getsockopt itself to signal an error so handle that case as well.
		// Note: Older versions of Windows CE don't support SO_ERROR, but this assumes you're using a newer version.
		
		tempInt = 0;
		tempLen = (socklen_t) sizeof( tempInt );
		err = getsockopt( inNativeSock, SOL_SOCKET, SO_ERROR, (char *) &tempInt, &tempLen );
		if( err == 0 ) err = tempInt;
		if( err ) goto exit;
	}
	
	// Succcessful connect so the NetSocket now owns the native socket.
	
#if( defined( SO_NOSIGPIPE ) )
	setsockopt( inNativeSock, SOL_SOCKET, SO_NOSIGPIPE, &(int){ 1 }, (socklen_t) sizeof( int ) );
#endif
	
	// Disable nagle so data we send is not delayed. Code should coalesce writes to minimize small writes instead.
			
	tempInt = 1;
	setsockopt( inNativeSock, IPPROTO_TCP, TCP_NODELAY, (char *) &tempInt, (socklen_t) sizeof( tempInt ) );
	
	inSock->nativeSock = inNativeSock;
	
exit:
	return( err );
}

//===========================================================================================================================
//	NetSocket_Disconnect
//===========================================================================================================================

OSStatus	NetSocket_Disconnect( NetSocketRef inSock, int32_t inTimeoutSecs )
{
	OSStatus			err;
	uint64_t			deadline;
	
	require_action( inSock && ( inSock->magic == kNetSocketMagic ), exit2, err = kBadReferenceErr );
	require_action( inTimeoutSecs >= 0, exit, err = kParamErr ); // Don't allow waiting forever.
	require_action_quiet( !inSock->canceled, exit, err = kCanceledErr );
	require_action( IsValidSocket( inSock->nativeSock ), exit, err = kNotPreparedErr );
	
	// Shutdown the write side of the connection so the peer receives an EOF and knows we're closing.
	
	err = shutdown( inSock->nativeSock, SHUT_WR_COMPAT );
	err = map_socket_noerr_errno( inSock->nativeSock, err );
	require_noerr_quiet( err, exit );
	
	// Read and discard data from the peer until we get an EOF or time out. This uses an absolute
	// deadline timeout to protect against a malicious or misbehaving peer sending data forever.
	
	deadline = UpMicroseconds() + ( ( (uint64_t) inTimeoutSecs ) * 1000000 );
	for( ;; )
	{
		char		buf[ 32 ];
		ssize_t		n;
		
		n = recv( inSock->nativeSock, buf, (int) sizeof( buf ), 0 );
		if( n > 0 )
		{
			dlog( kLogLevelNotice, "### %zd bytes received on graceful shutdown: %#H\n", 
				n, buf, (size_t) n, (size_t) n );
		}
		else if( n == 0 )
		{
			err = kNoErr;
			break;
		}
		else
		{
			err = socket_value_errno( inSock->nativeSock, n );
			if( err == EWOULDBLOCK )
			{
				err = NetSocket_Wait( inSock, inSock->nativeSock, kNetSocketWaitType_Read, inTimeoutSecs );
				if( err ) goto exit;
			}
		#if( TARGET_OS_POSIX )
			else if( err == EINTR ) continue;
		#endif
			else { dlogassert( "recv error: %#m", err ); goto exit; }
		}
		
		if( UpMicroseconds() > deadline )
		{
			err = kTimeoutErr;
			goto exit;
		}
	}
	
exit:
	ForgetSocket( &inSock->nativeSock );
	
exit2:
	return( err );
}

//===========================================================================================================================
//	NetSocket_ReadInternal
//===========================================================================================================================

OSStatus
	NetSocket_ReadInternal( 
		NetSocketRef	inSock, 
		size_t			inMinSize, 
		size_t			inMaxSize, 
		void *			inBuffer, 
		size_t *		outSize, 
		int				inFlags,
		int32_t			inTimeoutSecs )
{
	OSStatus		err;
	size_t			readSize;
	char *			bufferPtr;
	ssize_t			remaining;
	ssize_t			n;
	
	readSize = 0;
	
	require_action( inSock && ( inSock->magic == kNetSocketMagic ), exit, err = kBadReferenceErr );
	require_action_quiet( !inSock->canceled, exit, err = kCanceledErr );
	require_action( IsValidSocket( inSock->nativeSock ), exit, err = kNotPreparedErr );
	
	// If we've already buffered data from a previous read then use that first.
	
	bufferPtr = (char *) inBuffer;
	remaining = (ssize_t) inMaxSize;
	n = (ssize_t)( inSock->leftoverEnd - inSock->leftoverPtr );
	if( n > 0 )
	{
		if( n > remaining ) n = remaining;
		memcpy( bufferPtr, inSock->leftoverPtr, n );
		inSock->leftoverPtr	+= n;
		readSize			+= ( (size_t) n );
		bufferPtr			+= n;
		remaining			-= n;
	}
	
	while( remaining > 0 )
	{
		n = recv( inSock->nativeSock, bufferPtr, (size_t) remaining, inFlags );
		if( n > 0 )
		{
			bufferPtr += n;
			remaining -= n;
			readSize  += ( (size_t) n );
		}
		else if( n == 0 )
		{
			// Only return an EOF error if we haven't read at least the min size. This way the caller
			// doesn't need to care if the peer disconnected after they sent the min amount of data.
			// They'll get a kNoErr with the data then they can get the EOF on a subsequent call.
			
			err = ( readSize < inMinSize ) ? kConnectionErr : kNoErr;
			goto exit;
		}
		else
		{
			err = socket_value_errno( inSock->nativeSock, n );
			if( err == EWOULDBLOCK )
			{
				// If we already got the min amount of data then treat it as success. For sentinel-based
				// protocols like HTTP, we don't know much data the peer is going to send so the caller 
				// would use a min size of 1 and a max size that's the size of the buffer. That way, the
				// caller can get the response data immediately and parse it to determine if more data 
				// should be arriving or if it got everything it was expecting.
				
				if( readSize >= inMinSize )
				{
					err = kNoErr;
					goto exit;
				}
				err = NetSocket_Wait( inSock, inSock->nativeSock, kNetSocketWaitType_Read, inTimeoutSecs );
				if( err ) goto exit;
			}
		#if( TARGET_OS_POSIX )
			else if( err == EINTR )
			{
				continue;
			}
		#endif
			else
			{
				if( ( err != ECONNRESET ) && ( err != ETIMEDOUT ) )
				{
					dlogassert( "recv() error: %#m", err );
				}
				goto exit;
			}
		}
	}
	err = kNoErr;
	
exit:
	if( outSize ) *outSize = readSize;
	return( err );
}

//===========================================================================================================================
//	NetSocket_WriteInternal
//===========================================================================================================================

OSStatus	NetSocket_WriteInternal( NetSocketRef inSock, const void *inBuffer, size_t inSize, int32_t inTimeoutSecs )
{
	OSStatus			err;
	const char *		bufferPtr;
	size_t				remaining;
	
	require_action( inSock && ( inSock->magic == kNetSocketMagic ), exit, err = kBadReferenceErr );
	require_action_quiet( !inSock->canceled, exit, err = kCanceledErr );
	require_action( IsValidSocket( inSock->nativeSock ), exit, err = kNotPreparedErr );
	
	bufferPtr = (const char *) inBuffer;
	remaining = inSize;
	while( remaining > 0 )
	{
		ssize_t		n;
		
		#if( TARGET_OS_POSIX )
			n = write( inSock->nativeSock, bufferPtr, remaining );
		#else
			n = send( inSock->nativeSock, (char *) bufferPtr, (int) remaining, 0 );
		#endif
		if( n > 0 )
		{
			bufferPtr += n;
			remaining -= ( (size_t) n );
		}
		else
		{
			err = socket_value_errno( inSock->nativeSock, n );
			if( err == EWOULDBLOCK )
			{
				err = NetSocket_Wait( inSock, inSock->nativeSock, kNetSocketWaitType_Write, inTimeoutSecs );
				if( err ) goto exit;
			}
		#if( TARGET_OS_POSIX )
			else if( err == EINTR ) continue;
			else if( err == EPIPE ) goto exit;
		#endif
			else if( err == ECONNRESET ) goto exit;
			else { dlogassert( "send/write() error: %#m", err ); goto exit; }
		}
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	NetSocket_WriteVInternal
//===========================================================================================================================

#if( TARGET_OS_POSIX || TARGET_OS_WINDOWS )
OSStatus	NetSocket_WriteVInternal( NetSocketRef inSock, iovec_t *inArray, int inCount, int32_t inTimeoutSecs )
{
	OSStatus			err;
	ssize_t				n;
	
	require_action( inSock && ( inSock->magic == kNetSocketMagic ), exit, err = kBadReferenceErr );
	require_action_quiet( !inSock->canceled, exit, err = kCanceledErr );
	require_action( IsValidSocket( inSock->nativeSock ), exit, err = kNotPreparedErr );
	if( inCount == 0 ) { err = kNoErr; goto exit; }
	
	for( ;; )
	{
		n = writev( inSock->nativeSock, inArray, inCount );
		if( n > 0 )
		{
			while( ( inCount > 0 ) && ( n >= (ssize_t)( inArray->iov_len ) ) )
			{
				n -= inArray->iov_len;
				++inArray;
				--inCount;
			}
			if( inCount == 0 ) break;
			
			inArray->iov_base  = ( (char *)( inArray->iov_base ) ) + n;
			inArray->iov_len  -= ( (size_t) n );
		}
		else if( n == 0 )
		{
			dlogassert( "Bad writev() param" );
			err = kParamErr;
			goto exit;
		}
		else
		{
			err = socket_value_errno( inSock->nativeSock, n );
			if( err == EWOULDBLOCK )
			{
				err = NetSocket_Wait( inSock, inSock->nativeSock, kNetSocketWaitType_Write, inTimeoutSecs );
				if( err ) goto exit;
			}
		#if( TARGET_OS_POSIX )
			else if( err == EINTR ) continue;
			else if( err == EPIPE ) goto exit;
		#endif
			else if( err == ECONNRESET ) goto exit;
			else if( err == EHOSTDOWN )  goto exit;
			else { dlogassert( "writev() error: %zd, %#m", n, err ); goto exit; }
		}
	}
	err = kNoErr;
	
exit:
	return( err );
}
#endif

//===========================================================================================================================
//	NetSocket_WriteVSlow
//===========================================================================================================================

OSStatus	NetSocket_WriteVSlow( NetSocketRef inSock, iovec_t *inArray, int inCount, int32_t inTimeoutSecs )
{
	OSStatus		err;
	iovec_t *		src;
	iovec_t *		end;
	
	require_action( inSock && ( inSock->magic == kNetSocketMagic ), exit, err = kBadReferenceErr );
	
	src = inArray;
	end = src + inCount;
	for( ; src < end; ++src )
	{
		err = NetSocket_Write( inSock, src->iov_base, src->iov_len, inTimeoutSecs );
		if( err ) goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	NetSocket_Wait
//===========================================================================================================================

#if( TARGET_OS_WINDOWS )
OSStatus
	NetSocket_Wait( 
		NetSocketRef		inSock, 
		SocketRef			inNativeSock, 
		NetSocketWaitType	inWaitType, 
		int32_t				inTimeoutSecs )
{
	OSStatus		err;
	long			eventMask;
	DWORD			timeoutMs;
	DWORD			result;
	HANDLE			waitArray[ 2 ];
	
	require_action( inSock && ( inSock->magic == kNetSocketMagic ), exit, err = kBadReferenceErr );
	require_action_quiet( !inSock->canceled, exit, err = kCanceledErr );
	require_action( IsValidSocket( inNativeSock ), exit, err = kNotPreparedErr );
	
	if(      inWaitType == kNetSocketWaitType_Read )	eventMask = FD_CLOSE | FD_READ;
	else if( inWaitType == kNetSocketWaitType_Write )	eventMask = FD_CLOSE | FD_WRITE;
	else if( inWaitType == kNetSocketWaitType_Connect )	eventMask = FD_CLOSE | FD_CONNECT;
	else
	{
		dlogassert( "bad wait type: %d", inWaitType );
		err = kParamErr;
		goto exit;
	}
	
	err = WSAEventSelect( inNativeSock, inSock->sockEvent, eventMask );
	err = map_noerr_errno( err );
	require_noerr( err, exit );
	
	waitArray[ 0 ] = inSock->sockEvent;
	waitArray[ 1 ] = inSock->cancelEvent;
	timeoutMs = ( inTimeoutSecs < 0 ) ? INFINITE : (DWORD)( inTimeoutSecs * 1000 );
	
	result = WaitForMultipleObjects( 2, waitArray, FALSE, timeoutMs );
	if(      result ==   WAIT_OBJECT_0 )		err = kNoErr;
	else if( result == ( WAIT_OBJECT_0 + 1 ) )	err = kCanceledErr;
	else if( result ==   WAIT_TIMEOUT )			err = kTimeoutErr;
	else
	{
		dlogassert( "wait failed: 0x%X/%#m", result, GetLastError() );
		err = kUnknownErr;
	}
	
exit:
	return( err );
}
#endif // TARGET_OS_WINDOWS

//===========================================================================================================================
//	NetSocket_Wait
//===========================================================================================================================

#if( !TARGET_OS_WINDOWS )
OSStatus
	NetSocket_Wait( 
		NetSocketRef		inSock, 
		SocketRef			inNativeSock, 
		NetSocketWaitType	inWaitType, 
		int32_t				inTimeoutSecs )
{
	OSStatus			err;
	fd_set				readSet;
	fd_set 				writeSet;
	fd_set * 			writeSetPtr;
	struct timeval		timeout;
	struct timeval *	timeoutPtr;
	int					maxFD;
	int					n;
	
	require_action( inSock && ( inSock->magic == kNetSocketMagic ), exit, err = kBadReferenceErr );
	require_action_quiet( !inSock->canceled, exit, err = kCanceledErr );
	require_action( IsValidSocket( inNativeSock ), exit, err = kNotPreparedErr );
	
	FD_ZERO( &readSet );
#if( TARGET_OS_POSIX )
	FD_SET( inSock->recvCancel, &readSet );
#else
	FD_SET( inSock->cancelSock, &readSet );
#endif
	
	// Set up the read/write sets for select.
	
	if( inWaitType == kNetSocketWaitType_Read )
	{
		FD_SET( inNativeSock, &readSet );
		writeSetPtr = NULL;
	}
	else if( inWaitType == kNetSocketWaitType_Write )
	{
		FD_ZERO( &writeSet );
		FD_SET( inNativeSock, &writeSet );
		writeSetPtr = &writeSet;
	}
	else if( inWaitType == kNetSocketWaitType_Connect )
	{
		// Sockets become writable on success and readable and writable on error.
		
		FD_SET( inNativeSock, &readSet );
		FD_ZERO( &writeSet );
		FD_SET( inNativeSock, &writeSet );
		writeSetPtr = &writeSet;
	}
	else
	{
		dlogassert( "bad wait type: %d", inWaitType );
		err = kParamErr;
		goto exit;
	}
	
	// Set up the timeout.
	
	if( inTimeoutSecs < 0 )
	{
		timeoutPtr = NULL;
	}
	else
	{
		timeout.tv_sec  = inTimeoutSecs;
		timeout.tv_usec = 0;
		timeoutPtr = &timeout;
	}
	
	// Wait for an event.
	
	maxFD = -1;
	if( inNativeSock		> maxFD ) maxFD = inNativeSock;
#if( TARGET_OS_POSIX )
	if( inSock->recvCancel	> maxFD ) maxFD = inSock->recvCancel;
#else
	if( inSock->cancelSock	> maxFD ) maxFD = inSock->cancelSock;
#endif
	do
	{
		n = select( maxFD + 1, &readSet, writeSetPtr, NULL, timeoutPtr );
		err = map_global_value_errno( n >= 0, n );
		
	}	while( err == EINTR );
	
#if( TARGET_OS_POSIX )
	if(      n  > 0 ) err = FD_ISSET( inSock->recvCancel, &readSet ) ? kCanceledErr : kNoErr;
#else
	if(      n  > 0 ) err = FD_ISSET( inSock->cancelSock, &readSet ) ? kCanceledErr : kNoErr;
#endif
	else if( n == 0 ) err = kTimeoutErr;
	else { dlogassert( "select() error: %#m", err ); goto exit; }
	
exit:
	return( err );
}
#endif // !TARGET_OS_WINDOWS

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	NetSocket_Test
//===========================================================================================================================

OSStatus	NetSocket_Test( void )
{
	OSStatus			err;
	NetSocketRef		sock;
	const char *		str;
	char				buf[ 1024 ];
	size_t				size;
	
	err = NetSocket_Create( &sock );
	require_noerr( err, exit );
	
	err = NetSocket_TCPConnect( sock, "bj.apple.com:80", 0, 5 );
	require_noerr( err, exit );
	
	str =
		"GET / HTTP/1.1\r\n"
		"Host: bj.apple.com\r\n"
		"Connection: close\r\n"
		"\r\n";
	size = strlen( str );
	err = NetSocket_Write( sock, str, size, 5 );
	require_noerr( err, exit );
	
	for( ;; )
	{
		err = NetSocket_Read( sock, 1, sizeof( buf ),  buf, &size, 5 );
		if( err == kConnectionErr ) break;
		require_noerr( err, exit );
		
		dlog( kLogLevelMax, "%.*s", (int) size, buf );
	}
	
	err = NetSocket_Disconnect( sock, 5 );
	require_noerr( err, exit );
	
	err = NetSocket_Delete( sock );
	require_noerr( err, exit );
	
exit:
	return( err );
}

#endif // !EXCLUDE_UNIT_TESTS

#if 0
#pragma mark -
#pragma mark == SocketUtils ==
#endif

//===========================================================================================================================
//	TCPConnect
//===========================================================================================================================

OSStatus	TCPConnect( const char *inHostList, const char *inDefaultService, int inSeconds, SocketRef *outSock )
{
	OSStatus			err;
	SocketRef			sock;
	int					defaultPort;
	const char *		ptr;
	const char *		end;
	
	// Save off the default port for any entries in the host list without an embedded port number.
	
	if( inDefaultService )	defaultPort = (int) strtoul( inDefaultService, NULL, 10 );
	else					defaultPort = 0;
	
	// Parse each comma-separated host from the list until we successfully connect or hit the end of the list.
	
	sock = kInvalidSocketRef;
	for( ptr = inHostList; *ptr != '\0'; ptr = end )
	{
		size_t			len;
		char			host[ kHostStringMaxSize ];
		sockaddr_ip		sip;
		size_t			sipLen;
		
		for( end = ptr; ( *end != '\0' ) && ( *end != ',' ); ++end ) {}
		len = (size_t)( end - ptr );
		require_action_quiet( len < sizeof( host ), exit, err = kSizeErr );
		memcpy( host, ptr, len );
		host[ len ] = '\0';
		if( *end != '\0' ) ++end;
		
		// First try the host as a numeric address string.
		
		err = StringToSockAddr( host, &sip, sizeof( sip ), &sipLen );
		if( err == kNoErr )
		{
			sock = socket( sip.sa.sa_family, SOCK_STREAM, IPPROTO_TCP );
			if( !IsValidSocket( sock ) ) continue;
			
			if( SockAddrGetPort( &sip ) == 0 ) SockAddrSetPort( &sip, defaultPort );
			err = SocketConnect( sock, &sip, inSeconds );
			if( err == kNoErr ) break;
			
			close_compat( sock );
			sock = kInvalidSocketRef;
			continue;
		}
		
		// The host doesn't appear to be numeric so try it as a DNS name. Parse any colon-separated port first.
		
		#if( NETUTILS_HAVE_GETADDRINFO )
		{
			int			port;
			char *		portPtr;
			
			port = 0;
			for( portPtr = host; ( *portPtr != '\0' ) && ( *portPtr != ':' ); ++portPtr ) {}
			if( *portPtr == ':' )
			{
				*portPtr++ = '\0';
				port = (int) strtoul( portPtr, NULL, 10 );
			}
			if( port == 0 ) port = defaultPort;
			if( port  > 0 )
			{
				char					portStr[ 32 ];
				struct addrinfo			hints;
				struct addrinfo *		aiList;
				struct addrinfo *		ai;
				
				// Try connecting to each resolved address until we successfully connect or hit the end of the list.
				
				snprintf( portStr, sizeof( portStr ), "%u", port );
				memset( &hints, 0, sizeof( hints ) );
				hints.ai_family	  = AF_UNSPEC;
				hints.ai_socktype = SOCK_STREAM;
				#if( NETUTILS_USE_DNS_SD_GETADDRINFO )
					err = getaddrinfo_dnssd( host, portStr, &hints, &aiList );
				#else
					err = getaddrinfo( host, portStr, &hints, &aiList );
				#endif
				if( err || !aiList ) continue;
				
				for( ai = aiList; ai; ai = ai->ai_next )
				{
					sock = socket( ai->ai_family, ai->ai_socktype, ai->ai_protocol );
					if( !IsValidSocket( sock ) ) continue;
					
					err = SocketConnect( sock, ai->ai_addr, inSeconds );
					if( err == kNoErr ) break;
					
					close_compat( sock );
					sock = kInvalidSocketRef;
				}
				#if( NETUTILS_USE_DNS_SD_GETADDRINFO )
					freeaddrinfo_dnssd( aiList );
				#else
					freeaddrinfo( aiList );
				#endif
				if( IsValidSocket( sock ) ) break;
			}
		}
		#endif
	}
	require_action_quiet( IsValidSocket( sock ), exit, err = kConnectionErr );
	*outSock = sock;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	SocketAccept
//===========================================================================================================================

OSStatus	SocketAccept( SocketRef inSock, int inSeconds, SocketRef *outSock, sockaddr_ip *outAddr )
{
	OSStatus				err;
	uint64_t				deadlineTicks, nowTicks;
	fd_set					readSet;
	struct timeval			timeout;
	struct timeval *		timeoutPtr;
	socklen_t				len;
	int						n;
	SocketRef				newSock;
		
	// Make the socket non-blocking to avoid a race between select() and accept(). If the client aborts the connection
	// after select() returns readable, but before we call accept(), some network stacks will block instead of returning
	// an error about the aborted connection. To avoid this, make sure the socket is non-blocking so we get EWOULDBLOCK.
	// See Stevens UNPv1r3 Section 16.6 for more info.
	
	SocketMakeNonBlocking( inSock );
	
	deadlineTicks = ( inSeconds >= 0 ) ? ( UpTicks() + SecondsToUpTicks( (uint64_t) inSeconds ) ) : kUpTicksForever;
	len = 0;
	FD_ZERO( &readSet );
	for( ;; )
	{
		// Wait until a connection is accepted or a timeout occurs.
		
		FD_SET( inSock, &readSet );
		if( inSeconds >= 0 )
		{
			timeout.tv_sec  = inSeconds;
			timeout.tv_usec = 0;
			timeoutPtr = &timeout;
		}
		else
		{
			timeoutPtr = NULL;
		}
		n = select( (int)( inSock + 1 ), &readSet, NULL, NULL, timeoutPtr );
		err = select_errno( n );
		require_noerr_quiet( err, exit );
		
		// Accept the connection. May fail if client aborts between select() and accept().
		
		if( outAddr ) len = (socklen_t) sizeof( *outAddr );
		newSock = accept( inSock, outAddr ? &outAddr->sa : NULL, outAddr ? &len : NULL );
		err = map_socket_value_errno( inSock, IsValidSocket( newSock ), newSock );
		if( !err )
		{
			int		option;
			
			#if( defined( SO_NOSIGPIPE ) )
				setsockopt( newSock, SOL_SOCKET, SO_NOSIGPIPE, &(int){ 1 }, (socklen_t) sizeof( int ) );
			#endif
			
			SocketMakeNonBlocking( newSock );
			
			// Disable nagle so data we send is not delayed. Code should coalesce writes to minimize small writes instead.
			
			option = 1;
			setsockopt( newSock, IPPROTO_TCP, TCP_NODELAY, (char *) &option, (socklen_t) sizeof( option ) );
			break;
		}
		
		// Update the timeout so we don't livelock on repeatedly aborting clients.
		
		if( inSeconds >= 0 )
		{
			nowTicks = UpTicks();
			if( deadlineTicks > nowTicks )	inSeconds = (int)( ( deadlineTicks - nowTicks ) / UpTicksPerSecond() );
			else							inSeconds = 0;
		}
	}
	*outSock = newSock;
	
exit:
	return( err );
}

//===========================================================================================================================
//	SocketConnect
//===========================================================================================================================

OSStatus	SocketConnect( SocketRef inSock, const void *inSockAddr, int inSeconds )
{
	OSStatus			err;
	fd_set				readSet;
	fd_set				writeSet;
	struct timeval		timeout;
	int					n;
	int					tempInt;
	socklen_t			tempLen;
	
	// Make the socket non-blocking so connect returns immediately and we can control the timeout.
	
	err = SocketMakeNonBlocking( inSock );
	require_noerr( err, exit );
	
	// Start the connection process to the remote host. This may return 0 if it can connect immediately. Other errors
	// are ignored at this point because it varies between platforms. Real errors will be handled further down.
	
	err = connect( inSock, (struct sockaddr *) inSockAddr, SockAddrGetSize( inSockAddr ) );
	if( err == 0 ) goto exit;
	
	// Wait for the connection to complete or a timeout.
	
	FD_ZERO( &readSet );
	FD_ZERO( &writeSet );
	FD_SET( inSock, &readSet );
	FD_SET( inSock, &writeSet );
	timeout.tv_sec 	= inSeconds;
	timeout.tv_usec = 0;	
	n = select( (int)( inSock + 1 ), &readSet, &writeSet, NULL, &timeout );
	err = select_errno( n );
	require_noerr_quiet( err, exit );
	
	// Check if connection was successful by checking the current pending socket error. Some sockets implementations 
	// return an error from the getsockopt function itself to signal an error so handle that case as well.
	
#if( TARGET_OS_WINDOWS_CE )
	// Windows CE does not appear to support SO_ERROR (it always returns -1) so assume a connection if it's writable.
	require_action_quiet( FD_ISSET( inSock, &writeSet ), exit, err = kConnectionErr );
#else
	require_action( FD_ISSET( inSock, &readSet ) || FD_ISSET( inSock, &writeSet ), exit, err = kUnknownErr );
	
	tempInt = 0;
	tempLen = (socklen_t) sizeof( tempInt );
	err = getsockopt( inSock, SOL_SOCKET, SO_ERROR, (char *) &tempInt, &tempLen );
	err = map_socket_noerr_errno( inSock, err );
	if( err == 0 ) err = tempInt;
	require_noerr_quiet( err, exit );
#endif
	
#if( defined( SO_NOSIGPIPE ) )
	setsockopt( inSock, SOL_SOCKET, SO_NOSIGPIPE, &(int){ 1 }, (socklen_t) sizeof( int ) );
#endif
	
	// Disable nagle so responses we send are not delayed. Code should coalesce writes to minimize small writes instead.
	
	tempInt = 1;
	setsockopt( inSock, IPPROTO_TCP, TCP_NODELAY, (char *) &tempInt, (socklen_t) sizeof( tempInt ) );
	
exit:
	return( err );
}

//===========================================================================================================================
//	SocketCloseGracefully
//===========================================================================================================================

OSStatus	SocketCloseGracefully( SocketRef inSock, int inTimeoutSecs )
{
	OSStatus			err;
	uint64_t			deadline;
	uint64_t			now;
	uint64_t			delta;
	struct timeval		timeout;
	fd_set				readSet;
	ssize_t				n;
	char				buf[ 32 ];
	
	check_string( inTimeoutSecs >= 0, "infinite timeout not allowed" );
	
	deadline = UpMicroseconds() + ( ( (uint64_t) inTimeoutSecs ) * kMicrosecondsPerSecond );
	FD_ZERO( &readSet );
	
	// Shutdown the write side of the connection so the peer receives an EOF and knows we're closing.
	// Note: this ignores the result of shutdown because the peer may closed it and we'd get an error.
	
	shutdown( inSock, SHUT_WR_COMPAT );
	
	// Read and discard data from the peer until we get an EOF, an error, or we time out.
	
	for( ;; )
	{
		now = UpMicroseconds();
		if( now >= deadline )
		{
			timeout.tv_sec  = 0;
			timeout.tv_usec = 0;
		}
		else
		{
			delta = deadline - now;
			timeout.tv_sec  = (int)( delta / kMicrosecondsPerSecond );
			timeout.tv_usec = (int)( delta % kMicrosecondsPerSecond );
		}
		
		FD_SET( inSock, &readSet );
		n = select( (int)( inSock + 1 ), &readSet, NULL, NULL, &timeout );
		if( n == 0 ) { dlogassert( "timeout waiting for graceful close" ); break; }
		if( n  < 0 ) { err = global_value_errno( n ); check_noerr( err );  break; }
		
		n = recv( inSock, buf, sizeof( buf ), 0 );
		if( n > 0 ) continue;
		break;
	}
	
	// Finally do the real close.
	
	err = close_compat( inSock );
	err = map_socket_noerr_errno( inSock, err );
	check_noerr( err );
	
	return( err );
}

//===========================================================================================================================
//	SocketRecvFrom
//===========================================================================================================================

#if( TARGET_OS_POSIX )
OSStatus
	SocketRecvFrom( 
		SocketRef			inSock, 
		void *				inBuf, 
		size_t				inMaxLen, 
		size_t *			outLen, 
		void *				outFrom, 
		size_t				inFromMaxLen, 
		size_t *			outFromLen, 
		uint64_t *			outTicks, 
		uint32_t *			outIfIndex, 
		char *				outIfName )
{
	OSStatus				err;
	struct iovec			iov;
	struct msghdr			msg;
	uint8_t					controlData[ 256 ];
	ssize_t					n;
	
	iov.iov_base		= inBuf;
	iov.iov_len			= inMaxLen;
	msg.msg_name		= outFrom;
	msg.msg_namelen		= (socklen_t) inFromMaxLen;
	msg.msg_iov			= &iov;
	msg.msg_iovlen		= 1;
	msg.msg_control		= controlData;
	msg.msg_controllen	= (socklen_t) sizeof( controlData );
	msg.msg_flags		= 0;
	
	for( ;; )
	{
		n = recvmsg( inSock, &msg, 0 );
		err = map_socket_value_errno( inSock, n >= 0, n );
		if( err == EINTR ) continue;
		require_noerr_quiet( err, exit );
		break;
	}
	
	if( outLen )			*outLen     = (size_t) n;
	if( outFromLen )		*outFromLen = msg.msg_namelen;
	if( outTicks )			*outTicks	= SocketGetPacketUpTicks( &msg );
	if( outIfIndex )		*outIfIndex	= SocketGetPacketReceiveInterface( &msg, outIfName );
	else if( outIfName )	SocketGetPacketReceiveInterface( &msg, outIfName );
	
exit:
	return( err );
}
#else
OSStatus
	SocketRecvFrom( 
		SocketRef			inSock, 
		void *				inBuf, 
		size_t				inMaxLen, 
		size_t *			outLen, 
		void *				outFrom, 
		size_t				inFromMaxLen, 
		size_t *			outFromLen, 
		uint64_t *			outTicks, 
		uint32_t *			outIfIndex, 
		char *				outIfName )
{
	OSStatus				err;
	socklen_t				len;
	ssize_t					n;
	
	len = (socklen_t) inFromMaxLen;
	n = recvfrom( inSock, (char *) inBuf, inMaxLen, 0, (struct sockaddr *) outFrom, outFrom ? &len : NULL );
	err = map_socket_value_errno( inSock, n >= 0, n );
	require_noerr_quiet( err, exit );
	
	if( outLen )			*outLen     = (size_t) n;
	if( outFromLen )		*outFromLen = len;
	if( outTicks )			*outTicks	= UpTicks();
	if( outIfIndex )		*outIfIndex	= 0;
	else if( outIfName )	*outIfName = '\0';
	
exit:
	return( err );
}
#endif

//===========================================================================================================================
//	SocketReadData
//===========================================================================================================================

OSStatus	SocketReadData( SocketRef inSock, void *inBuffer, size_t inSize, size_t *ioOffset )
{
	OSStatus		err;
	size_t			offset;
	size_t			remaining;
	uint8_t *		dst;
	ssize_t			n;
	
	offset = *ioOffset;
	require_action_quiet( offset != inSize, exit, err = kNoErr );
	require_action( offset < inSize, exit, err = kNoSpaceErr );
	
	remaining = inSize - offset;
	dst = ( (uint8_t *) inBuffer ) + offset;
	do
	{
		n = read_compat( inSock, (char *) dst, remaining );
		err = map_socket_value_errno( inSock, n >= 0, n );
		
	}	while( err == EINTR );
	
	if( n > 0 )
	{
		offset += ( (size_t) n );
		*ioOffset = offset;
		err = ( offset == inSize ) ? kNoErr : EWOULDBLOCK;
	}
	else if( n == 0 )
	{
		err = kConnectionErr;
	}
	else if( err != EWOULDBLOCK )
	{
		dlogassert( "recv failed: %#m", err );
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	SocketWriteData
//===========================================================================================================================

OSStatus	SocketWriteData( SocketRef inSock, iovec_t **ioArray, int *ioCount )
{
#if( TARGET_OS_POSIX || TARGET_OS_WINDOWS )
	OSStatus		err;
	ssize_t			n;
	
	if( *ioCount == 0 ) return( kNoErr );
	do
	{
		n = writev( inSock, *ioArray, *ioCount );
		err = map_socket_value_errno( inSock, n >= 0, n );
		
	}	while( err == EINTR );
	
	if( n > 0 )
	{
		err = UpdateIOVec( ioArray, ioCount, (size_t) n );
	}
	else if( ( err != EWOULDBLOCK ) && ( err != EPIPE ) )
	{
		dlogassert( "writev failed: %#m\n", err );
	}
	return( err );
#else
	OSStatus		err;
	ssize_t			n;
	
	err = kNoErr;
	while( *ioCount > 0 )
	{
		do
		{
			n = send( inSock, ( *ioArray )->iov_base, ( *ioArray )->iov_len, 0 );
			err = map_socket_value_errno( inSock, n >= 0, n );
			
		}	while( err == EINTR );
		if( err )
		{
			if( ( err != EWOULDBLOCK ) && ( err != EPIPE ) )
				dlogassert( "writev failed: %#m\n", err );
			break;
		}
		
		err = UpdateIOVec( ioArray, ioCount, (size_t) n );
		if( err == kNoErr ) break;
	}
	return( err );
#endif
}

//===========================================================================================================================
//	SocketTransportRead
//===========================================================================================================================

OSStatus	SocketTransportRead( void *inBuffer, size_t inMaxLen, size_t *outLen, void *inContext )
{
	SocketRef const		sock = (SocketRef)(intptr_t) inContext;
	OSStatus			err;
	ssize_t				n;
	
	do
	{
		n = read_compat( sock, (char *) inBuffer, inMaxLen );
		err = map_socket_value_errno( sock, n >= 0, n );
		
	}	while( err == EINTR );
	if(      n  > 0 ) *outLen = (size_t) n;
	else if( n == 0 ) err = kConnectionErr;
	return( err );
}

//===========================================================================================================================
//	SocketTransportWriteV
//===========================================================================================================================

OSStatus	SocketTransportWriteV( iovec_t **ioArray, int *ioCount, void *inContext )
{
	return( SocketWriteData( (SocketRef)(intptr_t) inContext, ioArray, ioCount ) );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	SocketSetBoundInterface
//===========================================================================================================================

OSStatus	SocketSetBoundInterface( SocketRef inSock, int inFamily, uint32_t inIfIndex )
{
	OSStatus		err;
	
	if( 0 ) {}
#if( defined( AF_INET6 ) && defined( IPV6_BOUND_IF ) )
	else if( inFamily == AF_INET6 ) err = setsockopt( inSock, IPPROTO_IPV6, IPV6_BOUND_IF, &inIfIndex, (socklen_t) sizeof( inIfIndex ) );
#endif
#if( defined( IP_BOUND_IF ) )
	else if( inFamily == AF_INET )  err = setsockopt( inSock, IPPROTO_IP,   IP_BOUND_IF,   &inIfIndex, (socklen_t) sizeof( inIfIndex ) );
#endif
	else
	{
		(void) inSock;
		(void) inFamily;
		(void) inIfIndex;
		
		dlogassert( "Set bound interface unsupported for family %d", inFamily );
		err = kUnsupportedErr;
		goto exit;
	};
	err = map_socket_noerr_errno( inSock, err );
	check_noerr( err );
	
exit:
	return( err );
}

//===========================================================================================================================
//	SocketGetBufferSize
//===========================================================================================================================

int	SocketGetBufferSize( SocketRef inSock, int inWhich, OSStatus *outErr )
{
	OSStatus		err;
	int				bufferSize;
	socklen_t		len;
	
	bufferSize = 0;
	len = (socklen_t) sizeof( bufferSize );
	err = getsockopt( inSock, SOL_SOCKET, inWhich, (char *) &bufferSize, &len );
	err = map_socket_noerr_errno( inSock, err );
	require_noerr( err, exit );
	
exit:
	if( outErr ) *outErr = err;
	return( bufferSize );
}

//===========================================================================================================================
//	SocketSetBufferSize
//===========================================================================================================================

OSStatus	SocketSetBufferSize( SocketRef inSock, int inWhich, int inSize )
{
	OSStatus		err;
	int				value;
	
	require_action( IsValidSocket( inSock ), exit, err = kParamErr );
	require_action( ( inWhich == SO_RCVBUF ) || ( inWhich == SO_SNDBUF ), exit, err = kParamErr );
	
	// Size  >  0, is an absolute size to set the socket buffer to.
	// Size ==  0, means maximize the socket buffer size (big as the kernel will allow).
	// Size == -1, means don't set the socket buffer size (useful for callers with a size input).
	// Size  < -1, means maximize the socket buffer size, up to the negated size.
	
	if( inSize > 0 )
	{
		err = setsockopt( inSock, SOL_SOCKET, inWhich, (char *) &inSize, (socklen_t) sizeof( inSize ) );
		err = map_socket_noerr_errno( inSock, err );
		require_noerr( err, exit );
		goto exit;
	}
	else if( inSize == -1 )
	{
		err = kNoErr;
		goto exit;
	}
	inSize = -inSize;
	
	// Get the max size from the system. If _PC_SOCK_MAXBUF ever becomes supported, we could start using that.
	
#if( TARGET_OS_DARWIN || TARGET_OS_FREEBSD )
{
	size_t		size;
	
	size = sizeof( value );
	err = sysctlbyname( "kern.ipc.maxsockbuf", &value, &size, NULL, 0 );
	err = map_global_noerr_errno( err );
	check_noerr( err );
	if( err ) value = 256 * 1024;	// Default to 256 KB.
}
#elif( TARGET_OS_NETBSD )
{
	size_t		size;
	
	size = sizeof( value );
	err = sysctlbyname( "kern.sbmax", &value, &size, NULL, 0 );
	err = map_global_noerr_errno( err );
	check_noerr( err );
	if( err ) value = 256 * 1024;	// Default to 256 KB.
}
#elif( TARGET_OS_LINUX )
{
	const char *		path;
	FILE *				file;
	int					n;
	
	value = 0;
	path = ( inWhich == SO_RCVBUF ) ? "/proc/sys/net/core/rmem_max" : "/proc/sys/net/core/wmem_max";
	file = fopen( path, "r" );
	check( file );
	if( file )
	{
		n = fscanf( file, "%d", &value );
		if( n != 1 ) value = 0;
		fclose( file );
	}
	if( value <= 0 ) value = 256 * 1024;	// Default to 256 KB.
}
#elif( TARGET_OS_QNX )
{
	int			mib[] = { CTL_KERN, KERN_SBMAX };
	size_t		size;
	
	size = sizeof( value );
	err = sysctl( mib, (u_int) countof( mib ), &value, &size, NULL, 0 );
	err = map_global_noerr_errno( err );
	check_noerr( err );
	if( err || ( value <= 4096 ) ) value = 128 * 1024; // Default to 128 KB.
}
#elif( TARGET_OS_WINDOWS )
	value = 256 * 1024; 			// Default to 256 KB.
#else
	#warning "don't know how to get the max socket buffer size on this platform."
	value = 256 * 1024;				// Default to 256 KB.
#endif
	
	// Reduce the max by about 15% because most systems report a size that doesn't account for overhead.
	// Cap it if there's a caller limit. Some systems have a huge maxsockbuf (8 MB) so this avoids waste.
	// Also round up to a 4 KB boundary so we're not reserving weird sizes.
	
	value = ( ( ( value * 85 ) / 100 ) + 4095 ) & ~4095;
	if( ( inSize > 0 ) && ( value > inSize ) )
	{
		value = inSize;
	}
	
	// Try to set the size and keep skrinking it until we find the true max (within 1 KB). Give up below 32 KB.
	
	for( ;; )
	{
		int		lastValue;
		
		DEBUG_USE_ONLY( lastValue );
		
		err = setsockopt( inSock, SOL_SOCKET, inWhich, (char *) &value, (socklen_t) sizeof( value ) );
		err = map_socket_noerr_errno( inSock, err );
		if( !err ) break;
		
		lastValue = value;
		value -= 1024;
		if( value >= ( 32 * 1024 ) )
		{
			dlog( kLogLevelNotice, "### couldn't set SO_SNDBUF to %d...trying %d (%#m)\n", lastValue, value, err );
		}
		else
		{
			dlogassert( "### couldn't set SO_SNDBUF to %d...giving up at %d (%#m)\n", lastValue, value, err );
			break;
		}
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	SocketIsDefunct
//===========================================================================================================================

Boolean	SocketIsDefunct( SocketRef inSock )
{
#if( defined( SO_ISDEFUNCT ) )
	OSStatus		err;
	int				defunct;
	socklen_t		len;
	
	defunct = 0;
	len = (socklen_t) sizeof( defunct );
	err = getsockopt( inSock, SOL_SOCKET, SO_ISDEFUNCT, &defunct, &len );
	err = map_socket_noerr_errno( inSock, err );
	check_noerr( err );
	return( defunct ? true : false );
#else
	(void) inSock;
	return( false );
#endif
}

//===========================================================================================================================
//	SocketGetFamily
//===========================================================================================================================

int	SocketGetFamily( SocketRef inSock, OSStatus *outErr )
{
	OSStatus		err;
	sockaddr_ip		sip;
	socklen_t		len;
	
	len = (socklen_t) sizeof( sip );
	err = getsockname( inSock, &sip.sa, &len );
	err = map_socket_noerr_errno( inSock, err );
	require_noerr( err, exit );
	
exit:
	if( outErr ) *outErr = err;
	return( err ? AF_UNSPEC : sip.sa.sa_family );
}

//===========================================================================================================================
//	SocketGetInterfaceInfo
//===========================================================================================================================

#if( !defined( SIOCGIFEFLAGS ) && TARGET_OS_DARWIN )
	#define	SIOCGIFEFLAGS		_IOWR('i', 142, struct ifreq) // get extended ifnet flags
#endif

#if( defined( ifr_eflags ) )
	#define ifr_get_eflags( IFR )		(IFR)->ifr_eflags
#elif( TARGET_OS_DARWIN )
	#define ifr_get_eflags( IFR )		( *( (const uint64_t *) &(IFR)->ifr_ifru ) )
#else
	#define ifr_get_eflags( IFR )		0
#endif

#if( TARGET_OS_POSIX || TARGET_OS_WINDOWS )
OSStatus
	SocketGetInterfaceInfo( 
		SocketRef			inSock, 
		const char *		inIfName, 
		char *				outIfName, 
		uint32_t *			outIfIndex, 
		uint8_t				outMACAddress[ 6 ], 
		uint32_t *			outMedia, 
		uint32_t *			outFlags, 
		uint64_t *			outExtendedFlags, 
		uint64_t *			outOtherFlags, 
		NetTransportType *	outTransportType )
{
	OSStatus				err;
	SocketRef				tempSock = kInvalidSocketRef;
	sockaddr_ip				sip;
	socklen_t				len;
	struct ifaddrs *		ifaList 	= NULL;
	struct ifaddrs *		ifa			= NULL;
#if( TARGET_OS_POSIX )
	struct ifreq			ifr;
#endif
	uint32_t				ifmedia		= 0;
	uint64_t				eflags		= 0;
	uint64_t				otherFlags	= 0;
	
	if( !IsValidSocket( inSock ) )
	{
		tempSock = socket( AF_INET, SOCK_DGRAM, 0 );
		err = map_socket_creation_errno( tempSock );
		require_noerr( err, exit );
		inSock = tempSock;
	}
	if( !inIfName )
	{
		len = (socklen_t) sizeof( sip );
		err = getsockname( inSock, &sip.sa, &len );
		err = map_socket_noerr_errno( inSock, err );
		require_noerr( err, exit );
		
		err = SockAddrSimplify( &sip, &sip );
		require_noerr( err, exit );
		
		err = getifaddrs( &ifaList );
		require_noerr( err, exit );
		require_action( ifaList, exit, err = kNotFoundErr );
		
		for( ifa = ifaList; ifa; ifa = ifa->ifa_next )
		{
			if( !ifa->ifa_addr )											continue; // Skip if address not valid.
			if( SockAddrCompareAddrEx( ifa->ifa_addr, &sip, true ) != 0 )	continue; // Skip if address doesn't match.
			
			inIfName = ifa->ifa_name;
			if( outIfName )  strlcpy( outIfName, inIfName, IF_NAMESIZE + 1 );
			break;
		}
		require_action( inIfName, exit, err = kNotFoundErr );
		
		if( outMACAddress )
		{
			memset( outMACAddress, 0, 6 );
			#if( TARGET_OS_BSD || TARGET_OS_QNX )
				for( ifa = ifaList; ifa; ifa = ifa->ifa_next )
				{
					if( ( ifa->ifa_addr->sa_family == AF_LINK ) && ( strcmp( ifa->ifa_name, inIfName ) == 0 ) )
					{
						const struct sockaddr_dl * const		sdl = (const struct sockaddr_dl *) ifa->ifa_addr;
						
						if( sdl->sdl_alen == 6 )
						{
							memcpy( outMACAddress, &sdl->sdl_data[ sdl->sdl_nlen ], 6 );
							break;
						}
					}
				}
			#elif( TARGET_OS_LINUX )
				memset( &ifr, 0, sizeof( ifr ) );
				strlcpy( ifr.ifr_name, inIfName, sizeof( ifr.ifr_name ) );
				err = ioctl( inSock, SIOCGIFHWADDR, &ifr );
				err = map_socket_noerr_errno( inSock, err );
				if( !err ) memcpy( outMACAddress, &ifr.ifr_hwaddr.sa_data, 6 );
			#else
				GetInterfaceMACAddress( inIfName, outMACAddress );
			#endif
		}
	}
	else if( outMACAddress )
	{
		memset( outMACAddress, 0, 6 );
		GetInterfaceMACAddress( inIfName, outMACAddress );
	}
	if( outIfIndex )
	{
		*outIfIndex = if_nametoindex( inIfName );
	}
	if( outMedia || outOtherFlags || outTransportType )
	{
		#if( defined( SIOCGIFMEDIA ) )
			struct ifmediareq		ifmr;
			
			memset( &ifmr, 0, sizeof( ifmr ) );
			strlcpy( ifmr.ifm_name, inIfName, sizeof( ifmr.ifm_name ) );
			err = ioctl( inSock, SIOCGIFMEDIA, &ifmr );
			err = map_socket_noerr_errno( inSock, err );
			ifmedia = !err ? ( (uint32_t) ifmr.ifm_current ) : 0;
			if( outMedia ) *outMedia = ifmedia;
			
			if( ( ifmr.ifm_status & IFM_AVALID ) && !( ifmr.ifm_status & IFM_ACTIVE ) )
			{
				otherFlags |= kNetInterfaceFlag_Inactive;
			}
		#else
			(void) ifmedia;
			
			if( outMedia ) *outMedia = 0;
		#endif
	}
	if( outFlags )
	{
		if( ifa )
		{
			*outFlags = ifa->ifa_flags;
		}
		else
		{
			#if( TARGET_OS_POSIX )
				memset( &ifr, 0, sizeof( ifr ) );
				strlcpy( ifr.ifr_name, inIfName, sizeof( ifr.ifr_name ) );
				err = ioctl( inSock, SIOCGIFFLAGS, &ifr );
				err = map_socket_noerr_errno( inSock, err );
				*outFlags = !err ? ( (uint32_t) ifr.ifr_flags ) : 0;
			#else
				*outFlags = 0;
			#endif
		}
	}
	if( outExtendedFlags || outTransportType )
	{
		#if( defined( SIOCGIFEFLAGS ) )
			memset( &ifr, 0, sizeof( ifr ) );
			strlcpy( ifr.ifr_name, inIfName, sizeof( ifr.ifr_name ) );
			err = ioctl( inSock, SIOCGIFEFLAGS, &ifr );
			err = map_socket_noerr_errno( inSock, err );
			eflags = !err ? ifr_get_eflags( &ifr ) : 0;
		#endif
		if( outExtendedFlags ) *outExtendedFlags = eflags;
	}
	if( outOtherFlags ) *outOtherFlags = otherFlags;
	if( outTransportType )
	{
		if( 0 ) {}
		#if( defined( IFM_IEEE80211 ) )
		else if( IFM_TYPE( ifmedia ) == IFM_IEEE80211 )	*outTransportType = kNetTransportType_WiFi;
		#endif
		#if( defined( IFEF_DIRECTLINK ) )
		else if( eflags & IFEF_DIRECTLINK )				*outTransportType = kNetTransportType_DirectLink;
		#endif
		else											*outTransportType = kNetTransportType_Ethernet;
	}
	err = kNoErr;
	
exit:
	if( ifaList ) freeifaddrs( ifaList );
	ForgetSocket( &tempSock );
	return( err );
}
#endif

//===========================================================================================================================
//	SocketSetKeepAlive
//===========================================================================================================================

OSStatus	SocketSetKeepAlive( SocketRef inSock, int inIdleSecs, int inMaxUnansweredProbes )
{
	OSStatus		err;
	int				option;
	
	option = ( ( inIdleSecs > 0 ) && ( inMaxUnansweredProbes > 0 ) ) ? 1 : 0;
	err = setsockopt( inSock, SOL_SOCKET, SO_KEEPALIVE, (char *) &option, (socklen_t) sizeof( option ) );
	err = map_socket_noerr_errno( inSock, err );
	require_noerr( err, exit );
	if( !option ) goto exit;
	
	// Set the idle seconds before keep-alive probes start being sent.
	
#if( defined( TCP_KEEPALIVE ) )
	option = inIdleSecs;
	if( option <= 0 ) option = 1;
	err = setsockopt( inSock, IPPROTO_TCP, TCP_KEEPALIVE, (char *) &option, (socklen_t) sizeof( option ) );
	err = map_socket_noerr_errno( inSock, err );
	check_noerr( err );
#elif( defined( TCP_KEEPIDLE ) )
	err = setsockopt( inSock, IPPROTO_TCP, TCP_KEEPIDLE, &option, (socklen_t) sizeof( option ) );
	err = map_socket_noerr_errno( inSock, err );
	check_noerr( err );
#endif
	
	// Set the idle seconds between keep-alive probes.
	
#if( defined( TCP_KEEPINTVL ) )
	option = inIdleSecs;
	if( option <= 0 ) option = 1;
	err = setsockopt( inSock, IPPROTO_TCP, TCP_KEEPINTVL, &option, (socklen_t) sizeof( option ) );
	err = map_socket_noerr_errno( inSock, err );
	check_noerr( err );
#endif
	
	// Set the max number of unanswered keep-alive probes before giving up.
	
#if( defined( TCP_KEEPCNT ) )
	option = inMaxUnansweredProbes;
	if( option <= 0 ) option = 1;
	err = setsockopt( inSock, IPPROTO_TCP, TCP_KEEPCNT, &option, (socklen_t) sizeof( option ) );
	err = map_socket_noerr_errno( inSock, err );
	check_noerr( err );
#endif
	
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	SocketSetNonBlocking
//===========================================================================================================================

OSStatus	SocketSetNonBlocking( SocketRef inSock, int inNonBlocking )
{
#if( TARGET_OS_VXWORKS )
	OSStatus		err;
	
	err = ioctl( inSock, FIONBIO, (int) &inNonBlocking );
	err = map_socket_noerr_errno( inSock, err );
	check_noerr( err );
	return( err );
#elif( TARGET_OS_WINDOWS )
	OSStatus		err;
	u_long			param;
	
	param = ( inNonBlocking != 0 );
	err = ioctlsocket( inSock, FIONBIO, &param );
	err = map_socket_noerr_errno( inSock, err );
	check_noerr( err );
	return( err );
#else
	OSStatus		err;
	int				flags;
	
	flags = fcntl( inSock, F_GETFL, 0 );
	if( inNonBlocking )	flags |= O_NONBLOCK;
	else				flags &= ~O_NONBLOCK;
	err = fcntl( inSock, F_SETFL, flags );
	err = map_socket_value_errno( inSock, err != -1, err );
	check_noerr( err );
	return( err );
#endif
}

//===========================================================================================================================
//	SocketSetNotSentLowWatermark
//===========================================================================================================================

OSStatus	SocketSetNotSentLowWatermark( SocketRef inSock, size_t inSize )
{
#if( defined( TCP_NOTSENT_LOWAT ) )
	OSStatus		err;
	
	require_action( inSize <= INT_MAX, exit, err = kRangeErr );
	
	err = setsockopt( inSock, IPPROTO_TCP, TCP_NOTSENT_LOWAT, &(int){ (int) inSize }, (socklen_t) sizeof( int ) );
	err = map_socket_noerr_errno( inSock, err );
	if( !err )	dlog( kLogLevelVerbose, "Socket %d TCP_NOTSENT_LOWAT: %zu\n", inSock, inSize );
	else		dlog( kLogLevelNotice,  "### TCP_NOTSENT_LOWAT failed: %#m\n", err );
	
exit:
	return( err );
#else
	(void) inSock;
	(void) inSize;
	
	dlog( kLogLevelNotice,  "### TCP_NOTSENT_LOWAT not supported on this platform\n" );
	return( kUnsupportedErr );
#endif
}

//===========================================================================================================================
//	SocketSetP2P
//===========================================================================================================================

#if( TARGET_OS_DARWIN && !defined( SO_RECV_ANYIF ) )
	#define SO_RECV_ANYIF		0x1104 // Enables IFEF_LOCALNET_PRIVATE (P2P) interfaces to be used for connect(), bind(), etc.
#endif

OSStatus	SocketSetP2P( SocketRef inSock, int inAllow )
{
#if( defined( SO_RECV_ANYIF ) )
	OSStatus		err;
	
	err = setsockopt( inSock, SOL_SOCKET, SO_RECV_ANYIF, &inAllow, (socklen_t) sizeof( inAllow ) );
	err = map_socket_noerr_errno( inSock, err );
	if( !err )	dlog( kLogLevelVerbose, "Socket %d P2P: %s\n", inSock, inAllow ? "allow" : "prevent" );
	else		dlog( kLogLevelNotice,  "### SO_RECV_ANYIF failed: %#m\n", err );
	return( err );
#else
	(void) inSock;
	(void) inAllow;
	
	return( kUnsupportedErr );
#endif
}

//===========================================================================================================================
//	SocketGetPacketReceiveInterface
//===========================================================================================================================

#if( TARGET_OS_POSIX )
uint32_t	SocketGetPacketReceiveInterface( struct msghdr *inPacket, char *inNameBuf )
{
	struct cmsghdr *		cmPtr;
	
	for( cmPtr = CMSG_FIRSTHDR( inPacket ); cmPtr; cmPtr = CMSG_NXTHDR( inPacket, cmPtr ) )
	{
		#if( defined( IP_RECVIF ) )
		if( ( cmPtr->cmsg_level == IPPROTO_IP ) && ( cmPtr->cmsg_type == IP_RECVIF ) )
		{
			const struct sockaddr_dl * const		sdl = (const struct sockaddr_dl * ) CMSG_DATA( cmPtr );
			
			if( inNameBuf && ( sdl->sdl_nlen < IF_NAMESIZE ) )
			{
				memcpy( inNameBuf, sdl->sdl_data, sdl->sdl_nlen );
				inNameBuf[ sdl->sdl_nlen ] = '\0';
			}
			return( sdl->sdl_index );
		}
		#endif
		
		#if( defined( IP_PKTINFO ) && !defined( IP_RECVIF ) )
		if( ( cmPtr->cmsg_level == IPPROTO_IP ) && ( cmPtr->cmsg_type == IP_PKTINFO ) )
		{
			const struct in_pktinfo * const		info = (const struct in_pktinfo * ) CMSG_DATA( cmPtr );
			
			if( inNameBuf )
			{
				*inNameBuf = '\0';
				if_indextoname( info->ipi_ifindex, inNameBuf );
			}
			return( info->ipi_ifindex );
		}
		#endif
		
		#if( defined( IPV6_PKTINFO ) && !defined( IP_RECVIF ) )
		if( ( cmPtr->cmsg_level == IPPROTO_IPV6 ) && ( cmPtr->cmsg_type == IPV6_PKTINFO ) )
		{
			const struct in6_pktinfo * const		info = (const struct in6_pktinfo *) CMSG_DATA( cmPtr );
			
			if( inNameBuf )
			{
				*inNameBuf = '\0';
				if_indextoname( info->ipi6_ifindex, inNameBuf );
			}
			return( info->ipi6_ifindex );
		}
		#endif
	}
	dlogassert( "Receive interface not found. Did you enable it with setsockopt?" );
	return( 0 );
}
#endif

//===========================================================================================================================
//	SocketSetPacketReceiveInterface
//===========================================================================================================================

OSStatus	SocketSetPacketReceiveInterface( SocketRef inSock, int inEnable )
{
	OSStatus		err;
	
#if( defined( IP_RECVIF ) )
	err = setsockopt( inSock, IPPROTO_IP, IP_RECVIF, (char *) &inEnable, (socklen_t) sizeof( inEnable ) );
	err = map_socket_noerr_errno( inSock, err );
	require_noerr( err, exit );
#else
	int family = SocketGetFamily( inSock, NULL );
	if( family == AF_INET )
	{
		#if( defined( IP_PKTINFO ) )
			err = setsockopt( inSock, IPPROTO_IP, IP_PKTINFO, &inEnable, (socklen_t) sizeof( inEnable ) );
			err = map_socket_noerr_errno( inSock, err );
			require_noerr( err, exit );
		#else
			dlogassert( "IPv4 Packet receive interface not supported on this platform" );
			err = kUnsupportedErr;
			goto exit;
		#endif
	}
#if( defined( AF_INET6 ) )
	else if( family == AF_INET6 )
	{
		#if( defined( IPV6_PKTINFO ) )
			err = setsockopt( inSock, IPPROTO_IPV6, IPV6_PKTINFO, &inEnable, (socklen_t) sizeof( inEnable ) );
			err = map_socket_noerr_errno( inSock, err );
			require_noerr( err, exit );
		#else
			dlogassert( "IPv6 Packet receive interface not supported on this platform" );
			err = kUnsupportedErr;
			goto exit;
		#endif
	}
#endif
	else
	{
		dlogassert( "Receive interface not supported for this socket family: %d", family );
		err = kTypeErr;
		goto exit;
	}
#endif
	
exit:
	return( err );
}

//===========================================================================================================================
//	SocketSetPacketTimestamps
//===========================================================================================================================

OSStatus	SocketSetPacketTimestamps( SocketRef inSock, int inEnabled )
{
#if( defined( SO_TIMESTAMP_MONOTONIC ) )
	OSStatus		err;
	
	err = setsockopt( inSock, SOL_SOCKET, SO_TIMESTAMP_MONOTONIC, &inEnabled, (socklen_t) sizeof( inEnabled ) );
	err = map_socket_noerr_errno( inSock, err );
	check_noerr( err );
	return( err );
#elif( defined( SO_TIMESTAMP ) )
	OSStatus		err;
	
	err = setsockopt( inSock, SOL_SOCKET, SO_TIMESTAMP, &inEnabled, (socklen_t) sizeof( inEnabled ) );
	err = map_socket_noerr_errno( inSock, err );
	check_noerr( err );
	return( err );
#else
	(void) inSock;
	(void) inEnabled;
	
	dlog( kLogLevelWarning, "### SO_TIMESTAMP/SO_TIMESTAMP_MONOTONIC not supported...using less accurate timestamps\n" );
	return( kNoErr );
#endif
}

//===========================================================================================================================
//	SocketGetPacketTicks
//===========================================================================================================================

#if( TARGET_OS_POSIX )
uint64_t	SocketGetPacketUpTicks( struct msghdr *inPacket )
{
#if( defined( SCM_TIMESTAMP_MONOTONIC ) || defined( SCM_TIMESTAMP ) )
	struct cmsghdr *		cmPtr;
	
	for( cmPtr = CMSG_FIRSTHDR( inPacket ); cmPtr; cmPtr = CMSG_NXTHDR( inPacket, cmPtr ) )
	{
		#if( defined( SCM_TIMESTAMP_MONOTONIC ) )
			if( ( cmPtr->cmsg_level == SOL_SOCKET ) && ( cmPtr->cmsg_type == SCM_TIMESTAMP_MONOTONIC ) )
			{
				uint64_t		ticks;
				
				memcpy( &ticks, CMSG_DATA( cmPtr ), sizeof( ticks ) );
				return( ticks );
			}
		#endif
		
		#if( defined( SCM_TIMESTAMP ) )
			if( ( cmPtr->cmsg_level == SOL_SOCKET ) && ( cmPtr->cmsg_type == SCM_TIMESTAMP ) )
			{
				struct timeval		tv;
				uint64_t			trueTicks, fakeTicks;
				int64_t				deltaTicks;
				uint64_t			ticks;
				
				// Adjust for the gettimeofday/microtime vs UpTicks delta every time (changes over time).
				
				gettimeofday( &tv, NULL );
				trueTicks  = UpTicks();
				fakeTicks  = SecondsToUpTicks( (uint64_t) tv.tv_sec ) + MicrosecondsToUpTicks( (uint32_t) tv.tv_usec );
				deltaTicks = (int64_t)( fakeTicks - trueTicks );
				
				memcpy( &tv, CMSG_DATA( cmPtr ), sizeof( tv ) );
				ticks = SecondsToUpTicks( (uint64_t) tv.tv_sec ) + MicrosecondsToUpTicks( (uint32_t) tv.tv_usec );
				ticks -= deltaTicks;
				return( ticks );
			}
		#endif
	}
	
	dlogassert( "SO_TIMESTAMP not found. Did you enable it with setsockopt?" );
	return( UpTicks() );
#else
	(void) inPacket;
	
	return( UpTicks() ); // No kernel support for packet timestamps so the current time is the best we can do.
#endif
}
#endif

//===========================================================================================================================
//	SocketSetQoS
//===========================================================================================================================

#if( TARGET_OS_IPHONE )
	#if( !defined( SO_TRAFFIC_CLASS ) )
		#error "iOS build, but no SO_TRAFFIC_CLASS? Probably include order issue with System/sys/socket.h"
	#endif
#endif
#if( TARGET_OS_MACOSX && !COMMON_SERVICES_NO_CORE_SERVICES )
	#if( !defined( SO_TRAFFIC_CLASS ) )
		#error "Mac build, but no SO_TRAFFIC_CLASS? Probably include order issue with System/sys/socket.h"
	#endif
#endif

OSStatus	SocketSetQoS( SocketRef inSock, int inQoS )
{
	OSStatus		err;
	int				family;
	int				diffServ;
	
	// Set IP_TOS/IPV6_TCLASS. This sets what goes into the TOS/TCLASS value in the IP header.
	
	if(      inQoS == kSocketQoS_Default )				diffServ = 0x00; // WMM=best effort, DSCP=0b000000 (CS0).
	else if( inQoS == kSocketQoS_Background )			diffServ = 0x20; // WMM=background,  DSCP=0b001000 (CS1).
	else if( inQoS == kSocketQoS_Video )				diffServ = 0x80; // WMM=video,       DSCP=0b100000 (CS4).
	else if( inQoS == kSocketQoS_Voice )				diffServ = 0xC0; // WMM=voice,       DSCP=0b110000 (CS6).
	else if( inQoS == kSocketQoS_AirPlayAudio )			diffServ = 0x80; // WMM=video,       DSCP=0b100000 (CS4).
	else if( inQoS == kSocketQoS_AirPlayScreenAudio )	diffServ = 0xC0; // WMM=voice,       DSCP=0b110000 (CS6).
	else if( inQoS == kSocketQoS_AirPlayScreenVideo )	diffServ = 0x80; // WMM=video,       DSCP=0b100000 (CS4).
	else if( inQoS == kSocketQoS_NTP )					diffServ = 0xC0; // WMM=voice,       DSCP=0b110000 (CS6).
	else { dlogassert( "Bad QoS value: %d", inQoS ); err = kParamErr; goto exit; }
	
	family = SocketGetFamily( inSock, NULL );
	if( family == AF_INET )
	{
		#if( defined( IP_TOS ) )
			err = setsockopt( inSock, IPPROTO_IP, IP_TOS, (char *) &diffServ, (socklen_t) sizeof( diffServ ) );
			err = map_socket_noerr_errno( inSock, err );
			if( err ) dlog( kLogLevelNotice, "### Set IP_TOS=0x%x failed: %#m\n", diffServ, err );
		#else
			dlog( kLogLevelVerbose, "### no IP_TOS support\n" );
		#endif
	}
#if( defined( AF_INET6 ) )
	else if( family == AF_INET6 )
	{
		#if( defined( IPV6_TCLASS ) )
			err = setsockopt( inSock, IPPROTO_IPV6, IPV6_TCLASS, (char *) &diffServ, (socklen_t) sizeof( diffServ ) );
			err = map_socket_noerr_errno( inSock, err );
			if( err ) dlog( kLogLevelNotice, "### Set IPV6_TCLASS=0x%x failed: %#m\n", diffServ, err );
		#else
			dlog( kLogLevelVerbose, "### no IPV6_TCLASS support\n" );
		#endif
	}
#endif
	else
	{
		dlogassert( "bad socket family %d", family );
	}
	
	// Set SO_TRAFFIC_CLASS. This is used by the driver for mbuf prioritization.
	
#if( defined( SO_TRAFFIC_CLASS ) && SO_TRAFFIC_CLASS )
{
	int		trafficClass;
	
	if(      inQoS == kSocketQoS_Default )				trafficClass = SO_TC_BE;
	else if( inQoS == kSocketQoS_Background )			trafficClass = SO_TC_BK;
	else if( inQoS == kSocketQoS_Video )				trafficClass = SO_TC_VI;
	else if( inQoS == kSocketQoS_Voice )				trafficClass = SO_TC_VO;
	else if( inQoS == kSocketQoS_AirPlayAudio )			trafficClass = SO_TC_AV;
	else if( inQoS == kSocketQoS_AirPlayScreenAudio )	trafficClass = SO_TC_VO;
	else if( inQoS == kSocketQoS_AirPlayScreenVideo )	trafficClass = SO_TC_VI;
	else if( inQoS == kSocketQoS_NTP )					trafficClass = SO_TC_CTL;
	else { dlogassert( "Bad QoS value: %d", inQoS ); err = kParamErr; goto exit; }
	
	err = setsockopt( inSock, SOL_SOCKET, SO_TRAFFIC_CLASS, &trafficClass, (socklen_t) sizeof( trafficClass ) );
	err = map_socket_noerr_errno( inSock, err );
	if( err ) dlog( kLogLevelNotice, "### Set SO_TRAFFIC_CLASS=%d failed: %#m\n", trafficClass, err );
}
#endif
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	SocketJoinMulticast / SocketLeaveMulticast
//===========================================================================================================================

static OSStatus
	_SocketJoinOrLeaveMulticast( 
		SocketRef		inSock, 
		const void *	inAddr, 
		const char *	inIfName, 
		uint32_t		inIfIndex, 
		Boolean			inJoin );

OSStatus	SocketJoinMulticast( SocketRef inSock, const void *inAddr, const char *inIfName, uint32_t inIfIndex )
{
	return( _SocketJoinOrLeaveMulticast( inSock, inAddr, inIfName, inIfIndex, true ) );
}

OSStatus	SocketLeaveMulticast( SocketRef inSock, const void *inAddr, const char *inIfName, uint32_t inIfIndex )
{
	return( _SocketJoinOrLeaveMulticast( inSock, inAddr, inIfName, inIfIndex, false ) );
}

static OSStatus
	_SocketJoinOrLeaveMulticast( 
		SocketRef		inSock, 
		const void *	inAddr, 
		const char *	inIfName, 
		uint32_t		inIfIndex, 
		Boolean			inJoin )
{
	const sockaddr_ip * const		sip = (const sockaddr_ip *) inAddr;
	OSStatus						err;
	
	if( sip->sa.sa_family == AF_INET )
	{
		struct ip_mreq		mreq;
		
		mreq.imr_multiaddr = sip->v4.sin_addr;
		#if( TARGET_OS_POSIX )
		if( inIfName )
		{
			struct ifreq		ifr;

			memset( &ifr, 0, sizeof( ifr ) );
			strlcpy( ifr.ifr_name, inIfName, sizeof( ifr.ifr_name ) );
			
			err = ioctl( inSock, SIOCGIFADDR, &ifr );
			err = map_socket_noerr_errno( inSock, err );
			require_noerr( err, exit );
			
			mreq.imr_interface = ( (const struct sockaddr_in *) &ifr.ifr_addr )->sin_addr;
		}
		else if( inIfIndex != 0 )
		{
			struct ifreq		ifr;
			
			memset( &ifr, 0, sizeof( ifr ) );
			inIfName = if_indextoname( inIfIndex, ifr.ifr_name );
			require_action( inIfName, exit, err = kNameErr );
			
			err = ioctl( inSock, SIOCGIFADDR, &ifr );
			err = map_socket_noerr_errno( inSock, err );
			require_noerr( err, exit );
			
			mreq.imr_interface = ( (const struct sockaddr_in *) &ifr.ifr_addr )->sin_addr;
		}
		else
		#endif
		{
			mreq.imr_interface.s_addr = htonl( INADDR_ANY );
		}
		
		// Remove before adding in case there's stale state.
		
		if( inJoin )
		{
			setsockopt( inSock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *) &mreq, (socklen_t) sizeof( mreq ) );
			err = setsockopt( inSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq, (socklen_t) sizeof( mreq ) );
			err = map_socket_noerr_errno( inSock, err );
			require_noerr( err, exit );
		}
		else
		{
			err = setsockopt( inSock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *) &mreq, (socklen_t) sizeof( mreq ) );
			err = map_socket_noerr_errno( inSock, err );
			require_noerr( err, exit );
		}
	}
#if( defined( AF_INET6 ) )
	else if( sip->sa.sa_family == AF_INET6 )
	{
		struct ipv6_mreq		mreq;
		
		mreq.ipv6mr_multiaddr = sip->v6.sin6_addr;
		if( inIfName && ( inIfIndex == 0 ) )
		{
			mreq.ipv6mr_interface = if_nametoindex( inIfName );
			require_action( mreq.ipv6mr_interface != 0, exit, err = kNameErr );
		}
		else
		{
			mreq.ipv6mr_interface = inIfIndex;
		}
		
		// To workaround <rdar://problem/5600899>, remove before adding in case there's stale state.
		
		if( inJoin )
		{
			setsockopt( inSock, IPPROTO_IPV6, IPV6_LEAVE_GROUP, (char *) &mreq, (socklen_t) sizeof( mreq ) );
			err = setsockopt( inSock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *) &mreq, (socklen_t) sizeof( mreq ) );
			err = map_socket_noerr_errno( inSock, err );
			require_noerr( err, exit );
		}
		else
		{
			err = setsockopt( inSock, IPPROTO_IPV6, IPV6_LEAVE_GROUP, (char *) &mreq, (socklen_t) sizeof( mreq ) );
			err = map_socket_noerr_errno( inSock, err );
			require_noerr( err, exit );
		}
	}
#endif
	else
	{
		dlogassert( "Unsupported family: %d", sip->sa.sa_family );
		err = kUnsupportedErr;
		goto exit;
	}
	
exit:
	return( err );
}

#if( TARGET_OS_POSIX )
//===========================================================================================================================
//	SocketSetMulticastInterface
//===========================================================================================================================

OSStatus	SocketSetMulticastInterface( SocketRef inSock, const char *inIfName, uint32_t inIfIndex )
{
	OSStatus		err;
	int				family;
	
	family = SocketGetFamily( inSock, NULL );
	if( family == AF_INET )
	{
		struct ifreq				ifr;
		struct in_addr				tempAddr;
		const struct in_addr *		addr;
		
		// If IP_MULTICAST_IFINDEX is defined, try using it. It may still fail if run on older systems.
		
		#if( defined( IP_MULTICAST_IFINDEX ) )
			if( inIfName && ( inIfIndex == 0 ) )
			{
				inIfIndex = if_nametoindex( inIfName );
				require_action( inIfIndex != 0, exit, err = kNameErr );
			}
			
			err = setsockopt( inSock, IPPROTO_IP, IP_MULTICAST_IFINDEX, &inIfIndex, (socklen_t) sizeof( inIfIndex ) );
			err = map_socket_noerr_errno( inSock, err );
			if( !err ) goto exit;
		#endif
		
		if( inIfName )
		{
			memset( &ifr, 0, sizeof( ifr ) );
			strlcpy( ifr.ifr_name, inIfName, sizeof( ifr.ifr_name ) );
			
			err = ioctl( inSock, SIOCGIFADDR, &ifr );
			err = map_socket_noerr_errno( inSock, err );
			require_noerr( err, exit );
			
			addr = &( (const struct sockaddr_in *) &ifr.ifr_addr )->sin_addr;
		}
		else if( inIfIndex != 0 )
		{
			memset( &ifr, 0, sizeof( ifr ) );
			inIfName = if_indextoname( inIfIndex, ifr.ifr_name );
			require_action( inIfName, exit, err = kNameErr );
			
			err = ioctl( inSock, SIOCGIFADDR, &ifr );
			err = map_socket_noerr_errno( inSock, err );
			require_noerr( err, exit );
			
			addr = &( (const struct sockaddr_in *) &ifr.ifr_addr )->sin_addr;
		}
		else
		{
			tempAddr.s_addr = htonl( INADDR_ANY );
			addr = &tempAddr;
		}
		
		err = setsockopt( inSock, IPPROTO_IP, IP_MULTICAST_IF, (char *) addr, (socklen_t) sizeof( *addr ) );
		err = map_socket_noerr_errno( inSock, err );
		require_noerr( err, exit );
	}
#if( defined( AF_INET6 ) )
	else if( family == AF_INET6 )
	{		
		if( inIfName && ( inIfIndex == 0 ) )
		{
			inIfIndex = if_nametoindex( inIfName );
			require_action( inIfIndex != 0, exit, err = kNameErr );
		}
		
		err = setsockopt( inSock, IPPROTO_IPV6, IPV6_MULTICAST_IF, (char *) &inIfIndex, (socklen_t) sizeof( inIfIndex ) );
		err = map_socket_noerr_errno( inSock, err );
		require_noerr( err, exit );
	}
#endif
	else
	{
		dlogassert( "Unsupported family: %d", family );
		err = kUnsupportedErr;
		goto exit;
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	SocketSetMulticastLoop
//===========================================================================================================================

OSStatus	SocketSetMulticastLoop( SocketRef inSock, Boolean inEnableLoopback )
{
	OSStatus		err;
	int				family;
	
	family = SocketGetFamily( inSock, NULL );
	if( family == AF_INET )
	{
		uint8_t		option;
		
		option = inEnableLoopback;
		err = setsockopt( inSock, IPPROTO_IP, IP_MULTICAST_LOOP, (char *) &option, (socklen_t) sizeof( option ) );
		err = map_socket_noerr_errno( inSock, err );
		require_noerr( err, exit );
	}
#if( defined( AF_INET6 ) )
	else if( family == AF_INET6 )
	{
		int		option;
		
		option = inEnableLoopback;
		err = setsockopt( inSock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (char *) &option, (socklen_t) sizeof( option ) );
		err = map_socket_noerr_errno( inSock, err );
		require_noerr( err, exit );
	}
#endif
	else
	{
		dlogassert( "Unsupported family: %d", family );
		err = kUnsupportedErr;
		goto exit;
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	SocketSetMulticastTTL
//===========================================================================================================================

OSStatus	SocketSetMulticastTTL( SocketRef inSock, int inTTL )
{
	OSStatus		err;
	int				family;
	
	family = SocketGetFamily( inSock, NULL );
	if( family == AF_INET )
	{
		err = setsockopt( inSock, IPPROTO_IP, IP_MULTICAST_TTL, (char *) &inTTL, (socklen_t) sizeof( inTTL ) );
		err = map_socket_noerr_errno( inSock, err );
		require_noerr( err, exit );
	}
#if( defined( AF_INET6 ) )
	else if( family == AF_INET6 )
	{
		err = setsockopt( inSock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *) &inTTL, (socklen_t) sizeof( inTTL ) );
		err = map_socket_noerr_errno( inSock, err );
		require_noerr( err, exit );
	}
#endif
	else
	{
		dlogassert( "Unsupported family: %d", family );
		err = kUnsupportedErr;
		goto exit;
	}
	
exit:
	return( err );
}
#endif // TARGET_OS_POSIX

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	SockAddrToString
//===========================================================================================================================

OSStatus	SockAddrToString( const void *inSA, SockAddrStringFlags inFlags, char *outStr )
{
	OSStatus					err;
	const struct sockaddr *		sa;
	int							port;
	
	sa = (const struct sockaddr *) inSA;
	if( sa->sa_family == AF_INET  )
	{
		const struct sockaddr_in *		sa4;
		
		sa4 = (const struct sockaddr_in *) inSA;
		port = ( inFlags & kSockAddrStringFlagsNoPort ) ? 0 : ntohs( sa4->sin_port );
		IPv4AddressToCString( ntohl( sa4->sin_addr.s_addr ), port, outStr );
	}
#if( defined( AF_INET6 ) )
	else if( sa->sa_family == AF_INET6  )
	{
		const struct sockaddr_in6 *		sa6;
		uint32_t						scopeID;
		uint32_t						flags;
		
		sa6 = (const struct sockaddr_in6 *) inSA;
		port = ( inFlags & kSockAddrStringFlagsNoPort ) ? -1 : (int) ntohs( sa6->sin6_port );
		if( ( port <= 0 ) && ( inFlags & kSockAddrStringFlagsForceIPv6Brackets ) )
		{
			port = kIPv6AddressToCStringForceIPv6Brackets;
		}
		
		scopeID = ( inFlags & kSockAddrStringFlagsNoScope ) ? 0 : sa6->sin6_scope_id;
		flags   = ( inFlags & kSockAddrStringFlagsEscapeScopeID ) ? kIPv6AddressToCStringEscapeScopeID : 0;
		IPv6AddressToCString( sa6->sin6_addr.s6_addr, scopeID, port, -1, outStr, flags );
	}
#endif
	else
	{
		dlogassert( "unsupported family: %d", sa->sa_family );
		err = kUnsupportedErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	StringToSockAddr
//===========================================================================================================================

OSStatus	StringToSockAddr( const char *inStr, void *outSA, size_t inSASize, size_t *outSASize )
{
	OSStatus		err;
	uint32_t		ipv4;
	int				port;
	
	// Try to parse as IPv4 first (most common) and if that fails, try IPv6.
	
	port = 0;
	err = StringToIPv4Address( inStr, kStringToIPAddressFlagsNone, &ipv4, &port, NULL, NULL, NULL );
	if( err == kNoErr )
	{
		struct sockaddr_in *		sa4;
		
		require_action_quiet( inSASize >= sizeof( *sa4 ), exit, err = kSizeErr );
		
		sa4 = (struct sockaddr_in *) outSA;
		memset( sa4, 0, sizeof( *sa4 ) );
		SIN_LEN_SET( sa4 );
		sa4->sin_family 		= AF_INET;
		sa4->sin_port			= htons( (uint16_t) port );
		sa4->sin_addr.s_addr	= htonl( ipv4 );
		
		if( outSASize ) *outSASize = sizeof( *sa4 );
	}
#if( defined( AF_INET6 ) )
	else
	{
		uint8_t						ipv6[ 16 ];
		uint32_t					scope;
		struct sockaddr_in6 *		sa6;
		
		require_action_quiet( inSASize >= sizeof( *sa6 ), exit, err = kSizeErr );
		
		scope = 0;
		err = StringToIPv6Address( inStr, kStringToIPAddressFlagsNone, ipv6, &scope, &port, NULL, NULL );
		require_noerr_quiet( err, exit );
		
		sa6 = (struct sockaddr_in6 *) outSA;
		memset( sa6, 0, sizeof( *sa6 ) );
		SIN6_LEN_SET( sa6 );
		sa6->sin6_family 	= AF_INET6;
		sa6->sin6_port		= htons( (uint16_t) port );
		memcpy( sa6->sin6_addr.s6_addr, ipv6, 16 );
		sa6->sin6_scope_id	= scope;
		
		if( outSASize ) *outSASize = sizeof( *sa6 );
	}
#endif
	
exit:
	return( err );
}

//===========================================================================================================================
//	SockAddrGetSize
//===========================================================================================================================

socklen_t	SockAddrGetSize( const void *inSA )
{
	int		family;
	
	family = ( (const struct sockaddr *) inSA )->sa_family;
	if(      family == AF_INET )  return( (socklen_t) sizeof( struct sockaddr_in ) );
#if( defined( AF_INET6 ) )
	else if( family == AF_INET6 ) return( (socklen_t) sizeof( struct sockaddr_in6 ) );
#endif
	dlogassert( "unknown family: %d", family );
	return( 0 );	
}

//===========================================================================================================================
//	SockAddrGetPort
//===========================================================================================================================

int	SockAddrGetPort( const void *inSA )
{
	int							port;
	const struct sockaddr *		sa;
	
	sa = (const struct sockaddr *) inSA;
	if(      sa->sa_family == AF_INET  ) port = ntohs( ( (const struct sockaddr_in  *) sa )->sin_port );
#if( defined( AF_INET6 ) )
	else if( sa->sa_family == AF_INET6 ) port = ntohs( ( (const struct sockaddr_in6 *) sa )->sin6_port );
#endif
	else { dlogassert( "unknown family: %d", sa->sa_family ); port = -1; }
	return( port );
}

//===========================================================================================================================
//	SockAddrSetPort
//===========================================================================================================================

void	SockAddrSetPort( void *inSA, int inPort )
{
	const struct sockaddr *		sa;
	
	check( inPort >= 0 );
	
	sa = (const struct sockaddr *) inSA;
	if(      sa->sa_family == AF_INET  ) ( (struct sockaddr_in  *) sa )->sin_port  = htons( (u_short) inPort );
#if( defined( AF_INET6 ) )
	else if( sa->sa_family == AF_INET6 ) ( (struct sockaddr_in6 *) sa )->sin6_port = htons( (u_short) inPort );
#endif
	else dlogassert( "unknown family: %d", sa->sa_family );
}

//===========================================================================================================================
//	SockAddrCompareAddr
//===========================================================================================================================

int	SockAddrCompareAddr( const void *inA1, const void *inA2 )
{
	return( SockAddrCompareAddrEx( inA1, inA2, false ) );
}

//===========================================================================================================================
//	SockAddrCompareAddrEx
//===========================================================================================================================

int	SockAddrCompareAddrEx( const void *inA1, const void *inA2, Boolean inUseIPv6IfIndex )
{
	int							cmp;
	const struct sockaddr *		a1;
	const struct sockaddr *		a2;
	
#if( !defined( AF_INET6 ) )
	(void) inUseIPv6IfIndex;
#endif
	
	a1 = (const struct sockaddr *) inA1;
	a2 = (const struct sockaddr *) inA2;
	cmp = ( (int) a1->sa_family ) - ( (int) a2->sa_family );
	if( cmp == 0 )
	{
		switch( a1->sa_family )
		{
			case AF_INET:
				cmp = memcmp( &( (const struct sockaddr_in *) a1 )->sin_addr, 
							  &( (const struct sockaddr_in *) a2 )->sin_addr, 
							  sizeof( struct in_addr ) );
				break;
			
			#if( defined( AF_INET6 ) )
			case AF_INET6:
			{
				const struct sockaddr_in6 *		sa6_1;
				const struct sockaddr_in6 *		sa6_2;
				struct sockaddr_in6				sa6_1copy;
				struct sockaddr_in6				sa6_2copy;
				int								ifCmp;
				
				// Some stacks store the IPv6 scope ID in the second word of the address so check for that and fix it.
				
				sa6_1 = (const struct sockaddr_in6 *) a1;
				sa6_2 = (const struct sockaddr_in6 *) a2;
				ifCmp = 0;
				if( IN6_IS_ADDR_LINKLOCAL( &sa6_1->sin6_addr ) && IN6_IS_ADDR_LINKLOCAL( &sa6_2->sin6_addr ) )
				{
					SockAddrSimplify( sa6_1, &sa6_1copy );
					SockAddrSimplify( sa6_2, &sa6_2copy );
					sa6_1 = &sa6_1copy;
					sa6_2 = &sa6_2copy;
					
					if( inUseIPv6IfIndex )
					{
						ifCmp = (int)( sa6_1->sin6_scope_id - sa6_2->sin6_scope_id );
					}
				}
				
				cmp = memcmp( &sa6_1->sin6_addr, &sa6_2->sin6_addr, sizeof( struct in6_addr ) );
				if( cmp == 0 ) cmp = ifCmp;
				break;
			}
			#endif
			
			case AF_UNSPEC:
				cmp = 0; // Treat two AF_UNSPEC addresses as equal.
				break;
				
			default:
				dlogassert( "unknown family: %d", a1->sa_family );
				cmp = -1;
				break;
		}
	}
	return( cmp );
}

//===========================================================================================================================
//	SockAddrCopy
//===========================================================================================================================

void	SockAddrCopy( const void *inSrc, void *inDst )
{
	const struct sockaddr *		src;
	
	src  = (const struct sockaddr *) inSrc;
	if(      src->sa_family == AF_INET )  memmove( inDst, src, sizeof( struct sockaddr_in ) );
#if( defined( AF_INET6 ) )
	else if( src->sa_family == AF_INET6 ) memmove( inDst, src, sizeof( struct sockaddr_in6 ) );
#endif
	else dlogassert( "unknown family: %d", src->sa_family );
}

#if( TARGET_OS_QNX )
	// Workaround buggy QNX headers by redefining macros.
	
	#undef  IN6_IS_ADDR_V4MAPPED
	#define IN6_IS_ADDR_V4MAPPED( a ) ( \
		( (a)->__u6_addr.__u6_addr32[ 0 ] == 0 ) &&	\
		( (a)->__u6_addr.__u6_addr32[ 1 ] == 0 ) &&	\
		( (a)->__u6_addr.__u6_addr32[ 2 ] == ntohl( (uint32_t) 0x0000ffff ) ) )
	
	#undef  IN6_IS_ADDR_V4COMPAT
	#define IN6_IS_ADDR_V4COMPAT(a) ( \
		( (a)->__u6_addr.__u6_addr32[ 0 ] == 0 ) && \
		( (a)->__u6_addr.__u6_addr32[ 1 ] == 0 ) && \
		( (a)->__u6_addr.__u6_addr32[ 2 ] == 0 ) && \
		( (a)->__u6_addr.__u6_addr32[ 3 ] != 0 ) && \
		( (a)->__u6_addr.__u6_addr32[ 3 ] != ntohl( (uint32_t) 1 ) ) )
#endif

//===========================================================================================================================
//	SockAddrSimplify
//
//	Converts a sockaddr to its simplest form; e.g. if it's IPv6 address wrapping an IPv4 address, convert to IPv4.
//	Note: "inSrc" may point to the same memory as "outDst", such as SockAddrSimplify( &addr, &addr ).
//===========================================================================================================================

OSStatus	SockAddrSimplify( const void *inSrc, void *outDst )
{
	OSStatus		err;
	int				family;
	
	family = ( (const struct sockaddr *) inSrc )->sa_family;
	if( family == AF_INET )
	{
		if( inSrc != outDst )
		{
			memmove( outDst, inSrc, sizeof( struct sockaddr_in ) );
		}
	}
#if( defined( AF_INET6 ) )
	else if( family == AF_INET6 )
	{
		const struct sockaddr_in6 *		sa6;
		
		// If the IPv6 address is wrapping an IPv4 address then convert to an IPv4 address for true IPv4 compatibility.
		
		sa6 = (const struct sockaddr_in6 *) inSrc;
		if( IN6_IS_ADDR_V4MAPPED( &sa6->sin6_addr ) || IN6_IS_ADDR_V4COMPAT( &sa6->sin6_addr ) )
		{
			struct sockaddr_in *		sa4;
			uint8_t						a4[ 4 ];
			uint16_t					port;
			
			memcpy( a4, &sa6->sin6_addr.s6_addr[ 12 ], 4 ); // IPv4 address is in the low 32 bits of the IPv6 address.
			port = sa6->sin6_port;
			
			sa4 = (struct sockaddr_in *) outDst;
			memset( sa4, 0, sizeof( *sa4 ) );
			SIN_LEN_SET( sa4 );
			sa4->sin_family	= AF_INET;
			sa4->sin_port	= port;
			memcpy( &sa4->sin_addr.s_addr, a4, 4 );
		}
		else if( IN6_IS_ADDR_LINKLOCAL( &sa6->sin6_addr ) )
		{
			struct sockaddr_in6		sa6Copy;
			
			// Some stacks store the IPv6 scope ID in the second word of the address so check for that and fix it.
			
			sa6Copy = *sa6;
			if( sa6Copy.sin6_scope_id == 0 )
			{
				sa6Copy.sin6_scope_id = ReadBig16( &sa6Copy.sin6_addr.s6_addr[ 2 ] );
			}
			sa6Copy.sin6_addr.s6_addr[ 2 ] = 0;
			sa6Copy.sin6_addr.s6_addr[ 3 ] = 0;
			memcpy( outDst, &sa6Copy, sizeof( sa6Copy ) );
		}
		else
		{
			if( inSrc != outDst )
			{
				memmove( outDst, inSrc, sizeof( struct sockaddr_in6 ) );
			}
		}
	}
#endif
	else
	{
		dlogassert( "unknown family: %d", family );
		err = kUnsupportedErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	SockAddrToDeviceID
//===========================================================================================================================

uint64_t	SockAddrToDeviceID( const void *inSockAddr )
{
	uint64_t		deviceID;
	sockaddr_ip		sip;
	
	SockAddrSimplify( inSockAddr, &sip );
	if( sip.sa.sa_family == AF_INET )
	{
		deviceID = UINT64_C( 0x0400000000000000 ); // 4 = IPv4 address type.
		deviceID |= ntohl( sip.v4.sin_addr.s_addr );
	}
#if( defined( AF_INET6 ) )
	else if( sip.sa.sa_family == AF_INET6 )
	{
		deviceID = UINT64_C( 0x0600000000000000 ); // 6 = IPv6 address type.
		deviceID |= ( (uint64_t)( sip.v6.sin6_addr.s6_addr[  9 ] ) ) << 48;
		deviceID |= ( (uint64_t)( sip.v6.sin6_addr.s6_addr[ 10 ] ) ) << 40;
		deviceID |= ( (uint64_t)( sip.v6.sin6_addr.s6_addr[ 11 ] ) ) << 32;
		deviceID |= ( (uint64_t)( sip.v6.sin6_addr.s6_addr[ 12 ] ) ) << 24;
		deviceID |= ( (uint64_t)( sip.v6.sin6_addr.s6_addr[ 13 ] ) ) << 16;
		deviceID |= ( (uint64_t)( sip.v6.sin6_addr.s6_addr[ 14 ] ) ) <<  8;
		deviceID |=               sip.v6.sin6_addr.s6_addr[ 15 ];
	}
#endif
	else
	{
		dlogassert( "Bad sockaddr family: %d", sip.sa.sa_family );
		deviceID = 0;
	}
	return( deviceID );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	GetInterfaceMACAddress
//===========================================================================================================================

#if( TARGET_OS_BSD || TARGET_OS_QNX || TARGET_OS_WINDOWS )
OSStatus	GetInterfaceMACAddress( const char *inInterfaceName, uint8_t *outMACAddress )
{
	OSStatus				err;
	struct ifaddrs *		iflist = NULL;
	struct ifaddrs *		ifa;
	
	err = getifaddrs( &iflist );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
	
	for( ifa = iflist; ifa; ifa = ifa->ifa_next )
	{
		if( ( ifa->ifa_addr->sa_family == AF_LINK ) && ( strcmp( ifa->ifa_name, inInterfaceName ) == 0 ) )
		{
			const struct sockaddr_dl * const		sdl = (const struct sockaddr_dl *) ifa->ifa_addr;
			
			if( sdl->sdl_alen == 6 )
			{
				memcpy( outMACAddress, &sdl->sdl_data[ sdl->sdl_nlen ], 6 );
				goto exit;
			}
		}
	}
	err = kNotFoundErr;
	
exit:
	if( iflist ) freeifaddrs( iflist );
	return( err );
}
#endif

#if( TARGET_OS_LINUX )
OSStatus	GetInterfaceMACAddress( const char *inInterfaceName, uint8_t *outMACAddress )
{
	OSStatus			err;
	SocketRef			sock;
	struct ifreq		ifr;
	
	sock = socket( AF_INET, SOCK_DGRAM, 0 );
	err = map_socket_creation_errno( sock );
	require_noerr( err, exit );
	
	memset( &ifr, 0, sizeof( ifr ) );
	strlcpy( ifr.ifr_name, inInterfaceName, sizeof( ifr.ifr_name ) );
	err = ioctl( sock, SIOCGIFHWADDR, &ifr );
	err = map_socket_noerr_errno( sock, err );
	require_noerr_quiet( err, exit );
	
	memcpy( outMACAddress, &ifr.ifr_hwaddr.sa_data, 6 );
	
exit:
	ForgetSocket( &sock );
	return( err );
}
#endif

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	SocketUtilsTest
//===========================================================================================================================

OSStatus	SocketUtilsTest( void )
{
	OSStatus		err;
	sockaddr_ip		sip;
	size_t			sipLen;
	char			str[ 256 ];
	char			str2[ 256 ];
	SocketRef		sock;
	
	//
	// SockAddrToString and StringToSockAddr.
	//
	
	// IPv6 with scope ID and port 80.
	
#if( TARGET_OS_WINDOWS )
	strlcpy( str, "[fe80::5445:5245:444f%5]:80", sizeof( str ) );
#else
	strlcpy( str, "[fe80::5445:5245:444f%en0]:80", sizeof( str ) );
#endif
	err = StringToSockAddr( str, &sip, sizeof( sip ), &sipLen );
	require_noerr( err, exit );
	err = SockAddrToString( &sip, kSockAddrStringFlagsNone, str2 );
	require_noerr( err, exit );
	require_action( strcmp( str, str2 ) == 0, exit, err = kResponseErr );
	require_action( SockAddrGetPort( &sip ) == 80, exit, err = kResponseErr );
	
	err = SockAddrToString( &sip, kSockAddrStringFlagsNoPort, str2 );
	require_noerr( err, exit );
#if( TARGET_OS_WINDOWS )
	require_action( strcmp( str2, "fe80::5445:5245:444f%5" ) == 0, exit, err = kResponseErr );
#else
	require_action( strcmp( str2, "fe80::5445:5245:444f%en0" ) == 0, exit, err = kResponseErr );
#endif
	
	err = SockAddrToString( &sip, kSockAddrStringFlagsNoPort | kSockAddrStringFlagsEscapeScopeID, str2 );
	require_noerr( err, exit );
#if( TARGET_OS_WINDOWS )
	require_action( strcmp( str2, "fe80::5445:5245:444f%255" ) == 0, exit, err = kResponseErr );
#else
	require_action( strcmp( str2, "fe80::5445:5245:444f%25en0" ) == 0, exit, err = kResponseErr );
#endif
	
	// IPv6 with scope ID and zero port
	
#if( TARGET_OS_WINDOWS )
	strlcpy( str, "fe80::5445:5245:444f%5", sizeof( str ) );
#else
	strlcpy( str, "fe80::5445:5245:444f%en0", sizeof( str ) );
#endif
	err = StringToSockAddr( str, &sip, sizeof( sip ), &sipLen );
	require_noerr( err, exit );
	err = SockAddrToString( &sip, kSockAddrStringFlagsNone, str2 );
	require_noerr( err, exit );
	require_action( strcmp( str, str2 ) == 0, exit, err = kResponseErr );
	
	err = SockAddrToString( &sip, kSockAddrStringFlagsNoPort, str2 );
	require_noerr( err, exit );
	require_action( strcmp( str, str2 ) == 0, exit, err = kResponseErr );
	
	// IPv6 with no scope ID and zero port
	
	strlcpy( str, "fe80::5445:5245:444f", sizeof( str ) );
	err = StringToSockAddr( str, &sip, sizeof( sip ), &sipLen );
	require_noerr( err, exit );
	err = SockAddrToString( &sip, kSockAddrStringFlagsNone, str2 );
	require_noerr( err, exit );
	require_action( strcmp( str, str2 ) == 0, exit, err = kResponseErr );
	
	err = SockAddrToString( &sip, kSockAddrStringFlagsNoPort, str2 );
	require_noerr( err, exit );
	require_action( strcmp( str, str2 ) == 0, exit, err = kResponseErr );
	
	// IPv6 forcing brackets.
	
	strlcpy( str, "fe80::5445:5245:444f", sizeof( str ) );
	err = StringToSockAddr( str, &sip, sizeof( sip ), &sipLen );
	require_noerr( err, exit );
	err = SockAddrToString( &sip, kSockAddrStringFlagsForceIPv6Brackets, str );
	require_noerr( err, exit );
	require_action( strcmp( str, "[fe80::5445:5245:444f]" ) == 0, exit, err = kResponseErr );
	
	// IPv4 with port 80
	
	strlcpy( str, "127.0.0.1:80", sizeof( str ) );
	err = StringToSockAddr( str, &sip, sizeof( sip ), &sipLen );
	require_noerr( err, exit );
	err = SockAddrToString( &sip, kSockAddrStringFlagsNone, str2 );
	require_noerr( err, exit );
	require_action( strcmp( str, str2 ) == 0, exit, err = kResponseErr );
	
	err = SockAddrToString( &sip, kSockAddrStringFlagsNoPort, str2 );
	require_noerr( err, exit );
	require_action( strcmp( str2, "127.0.0.1" ) == 0, exit, err = kResponseErr );
	
	// IPv4 with zero port
	
	strlcpy( str, "127.0.0.1", sizeof( str ) );
	err = StringToSockAddr( str, &sip, sizeof( sip ), &sipLen );
	require_noerr( err, exit );
	err = SockAddrToString( &sip, kSockAddrStringFlagsNone, str2 );
	require_noerr( err, exit );
	require_action( strcmp( str, str2 ) == 0, exit, err = kResponseErr );
	
	err = SockAddrToString( &sip, kSockAddrStringFlagsNoPort, str2 );
	require_noerr( err, exit );
	require_action( strcmp( str, str2 ) == 0, exit, err = kResponseErr );
	
	// Non-numeric
	
	err = StringToSockAddr( "www.apple.com", &sip, sizeof( sip ), &sipLen );
	require_action( err != kNoErr, exit, err = kResponseErr );
	
	err = StringToSockAddr( "www.apple.com:80", &sip, sizeof( sip ), &sipLen );
	require_action( err != kNoErr, exit, err = kResponseErr );
	
	// Simplify
	
	err = StringToSockAddr( "[::ffff:12.106.32.254]:1234", &sip, sizeof( sip ), &sipLen );
	require_noerr( err, exit );
	err = SockAddrSimplify( &sip, &sip );
	require_noerr( err, exit );
	err = SockAddrToString( &sip, kSockAddrStringFlagsNone, str );
	require_noerr( err, exit );
	require_action( strcmp( str, "12.106.32.254:1234" ) == 0, exit, err = kResponseErr );
	
	err = StringToSockAddr( "[::12.106.32.254]:1234", &sip, sizeof( sip ), &sipLen );
	require_noerr( err, exit );
	err = SockAddrSimplify( &sip, &sip );
	require_noerr( err, exit );
	err = SockAddrToString( &sip, kSockAddrStringFlagsNone, str );
	require_noerr( err, exit );
	require_action( strcmp( str, "12.106.32.254:1234" ) == 0, exit, err = kResponseErr );
	
	err = StringToSockAddr( "[fe80::5445:5245:444f]:1234", &sip, sizeof( sip ), &sipLen );
	require_noerr( err, exit );
	err = SockAddrSimplify( &sip, &sip );
	require_noerr( err, exit );
	err = SockAddrToString( &sip, kSockAddrStringFlagsNone, str );
	require_noerr( err, exit );
	require_action( strcmp( str, "[fe80::5445:5245:444f]:1234" ) == 0, exit, err = kResponseErr );
	
	err = StringToSockAddr( "12.106.32.254:1234", &sip, sizeof( sip ), &sipLen );
	require_noerr( err, exit );
	err = SockAddrSimplify( &sip, &sip );
	require_noerr( err, exit );
	err = SockAddrToString( &sip, kSockAddrStringFlagsNone, str );
	require_noerr( err, exit );
	require_action( strcmp( str, "12.106.32.254:1234" ) == 0, exit, err = kResponseErr );
	
	//
	// TCPConnect
	//

#if( defined( HARD_CODED_TEST ) && HARD_CODED_TEST )
	err = TCPConnect( "[fe80::20a:95ff:fec3:6140%en1]:8080,17.205.21.142:8080", "80", 5, &sock );
	require_noerr( err, exit );
	close_compat( sock );
#endif
	
	err = TCPConnect( "www.apple.com", "80", 5, &sock );
	require_noerr( err, exit );
	close_compat( sock );
		
	// Other
	
	err = TCPConnect( "www.apple.com", "80", 5, &sock );
	require_noerr( err, exit );
	
	err = SocketCloseGracefully( sock, 3 );
	require_noerr( err, exit );
exit:
	return( err );
}

//===========================================================================================================================
//	NetUtilsTest
//===========================================================================================================================

OSStatus	NetUtilsTest( void )
{
	OSStatus			err;
	uint8_t				buf[ 16 ], buf2[ 16 ];
	const char *		str;
	iovec_t				iov[ 3 ];
	iovec_t *			iop;
	int					ion;
	sockaddr_ip			sip;
	
#if( TARGET_RT_BIG_ENDIAN )	
	memcpy( buf, "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F", 16 );
	memset( buf2, 0, 16 );
	HostToBigMem( buf, 16, buf2 );
	require_action( memcmp( buf2, "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F", 16 ) == 0, 
		exit, err = kResponseErr );
	HostToLittleMem( buf, 16, buf2 );
	require_action( memcmp( buf2, "\x0F\x0E\x0D\x0C\x0B\x0A\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00", 16 ) == 0, 
		exit, err = kResponseErr );
#else
	memcpy( buf, "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F", 16 );
	memset( buf2, 0, 16 );
	HostToBigMem( buf, 16, buf2 );
	require_action( memcmp( buf2, "\x0F\x0E\x0D\x0C\x0B\x0A\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00", 16 ) == 0, 
		exit, err = kResponseErr );
	HostToLittleMem( buf, 16, buf2 );
	require_action( memcmp( buf, "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F", 16 ) == 0, 
		exit, err = kResponseErr );
#endif
	
#if( defined( AF_INET6 ) )
	err = StringToSockAddr( "fe80::217:f2ff:fec8:d6e7", &sip, sizeof( sip ), NULL );
	require_noerr( err, exit );
	require_action( SockAddrIsLinkLocal( &sip ), exit, err = -1 );
	require_action( SockAddrIsIPv6LinkLocal( &sip ), exit, err = -1 );
	require_action( !SockAddrIsIPv4LinkLocal( &sip ), exit, err = -1 );
	
	err = StringToSockAddr( "fd11:cb96:a4d7:4109:217:f2ff:fec8:d6e7", &sip, sizeof( sip ), NULL );
	require_noerr( err, exit );
	require_action( !SockAddrIsLinkLocal( &sip ), exit, err = -1 );
	require_action( !SockAddrIsIPv6LinkLocal( &sip ), exit, err = -1 );
	require_action( !SockAddrIsIPv4LinkLocal( &sip ), exit, err = -1 );
#endif
	
	err = StringToSockAddr( "169.254.20.15", &sip, sizeof( sip ), NULL );
	require_noerr( err, exit );
	require_action( SockAddrIsLinkLocal( &sip ), exit, err = -1 );
#if( defined( AF_INET6 ) )
	require_action( !SockAddrIsIPv6LinkLocal( &sip ), exit, err = -1 );
#endif
	require_action( SockAddrIsIPv4LinkLocal( &sip ), exit, err = -1 );
	
	err = StringToSockAddr( "10.0.20.15", &sip, sizeof( sip ), NULL );
	require_noerr( err, exit );
	require_action( !SockAddrIsLinkLocal( &sip ), exit, err = -1 );
#if( defined( AF_INET6 ) )
	require_action( !SockAddrIsIPv6LinkLocal( &sip ), exit, err = -1 );
#endif
	require_action( !SockAddrIsIPv4LinkLocal( &sip ), exit, err = -1 );
	
	// UpdateIOVec
	
	str = "mynameis";
	iov[ 0 ].iov_base = (char *) &str[ 0 ];
	iov[ 0 ].iov_len  = 2;
	iov[ 1 ].iov_base = (char *) &str[ 2 ];
	iov[ 1 ].iov_len  = 4;
	iov[ 2 ].iov_base = (char *) &str[ 6 ];
	iov[ 2 ].iov_len  = 2;
	iop = iov;
	ion = 3;
	
	err = UpdateIOVec( &iop, &ion, 2 );
	require_action( err == EWOULDBLOCK, exit, err = kResponseErr );
	require_action( iop == &iov[ 1 ], exit, err = kResponseErr );
	require_action( ion == 2, exit, err = kResponseErr );
	require_action( iop->iov_len == 4, exit, err = kResponseErr );
	require_action( iop->iov_base == (char *) &str[ 2 ], exit, err = kResponseErr );
	
	err = UpdateIOVec( &iop, &ion, 1 );
	require_action( err == EWOULDBLOCK, exit, err = kResponseErr );
	require_action( iop == &iov[ 1 ], exit, err = kResponseErr );
	require_action( ion == 2, exit, err = kResponseErr );
	require_action( iop->iov_len == 3, exit, err = kResponseErr );
	require_action( iop->iov_base == (char *) &str[ 3 ], exit, err = kResponseErr );
	
	err = UpdateIOVec( &iop, &ion, 5 );
	require_action( err == kNoErr, exit, err = kResponseErr );
	
	// TCPListener
	
#if( defined( TCP_LISTENER_TEST ) && TCP_LISTENER_TEST )
	err = TCPListenerTest();
	require_noerr( err, exit );
#endif
	
	err = NetSocket_Test();
	require_noerr( err, exit );
	
	err = SocketUtilsTest();
	require_noerr( err, exit );
	
exit:
	printf( "NetUtilsTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
