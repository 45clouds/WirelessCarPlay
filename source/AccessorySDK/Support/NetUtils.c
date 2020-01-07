/*
	File:    	NetUtils.c
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	410.8
	
	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
	Apple software is governed by and subject to the terms and conditions of your MFi License,
	including, but not limited to, the restrictions specified in the provision entitled ‚ÄùPublic 
	Software‚Ä? and is further subject to your agreement to the following additional terms, and your 
	agreement that the use, installation, modification or redistribution of this Apple software
	constitutes acceptance of these additional terms. If you do not agree with these additional terms,
	please do not use, install, modify or redistribute this Apple software.
	
	Subject to all of these terms and in¬†consideration of your agreement to abide by them, Apple grants
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
	fixes or enhancements to Apple in connection with this software (‚ÄúFeedback‚Ä?, you hereby grant to
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
	
	Copyright (C) 2006-2015 Apple Inc. All Rights Reserved.
*/

#include "NetUtils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include <glib.h>
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
	#if( TARGET_OS_DARWIN && APPLE_HAVE_ROUTING_SUPPORT )
		#include <net/ethernet.h>
		#include <net/if_types.h>
		#include <net/route.h>
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
	#if( APPLE_HAVE_ROUTING_SUPPORT )
		#include <netinet6/in6_var.h>
	#endif
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
#include "SystemUtils.h"
#include "TickUtils.h"

#if( APPLE_HAVE_ROUTING_SUPPORT )

//===========================================================================================================================
//	CreateUsableInterfaceList
//===========================================================================================================================

#define	IsUsableInterfaceAddress( FAMILY, IA )	\
	( ( ( FAMILY ) != AF_INET ) || ( ( (const struct sockaddr_in *)( ( IA )->ifa_addr ) )->sin_addr.s_addr != 0 ) )

OSStatus	CreateUsableInterfaceList( const char *inInterfaceName, int inFamily, struct ifaddrs **outList )
{
	OSStatus							err;
	SocketRef							infoSock;
	struct ifaddrs *					iaList;
	const struct ifaddrs *				ia;
	int									family;
	struct ifaddrs *					dstList;
	struct ifaddrs **					dstNextPtr;
	struct ifaddrs *					dstIA;
	size_t								size;
	struct sockaddr_in6 *				sa6;
	
	iaList		= NULL;
	dstList		= NULL;
	dstNextPtr	= &dstList;
	
	// Set up an IPv6 socket so we can check the state of interfaces using SIOCGIFAFLAG_IN6.
	
	infoSock = socket( AF_INET6, SOCK_DGRAM, 0 );
	err = map_socket_creation_errno( infoSock );
	check_noerr( err );
	
	// Set up sockets for each approved interface.
	
	err = getifaddrs( &iaList );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
	
	for( ia = iaList; ia; ia = ia->ifa_next )
	{
		if( !( ia->ifa_flags & IFF_UP ) || !ia->ifa_addr || !ia->ifa_name ) 		continue; // Must be up and valid.
		family = ia->ifa_addr->sa_family;
		if( ( family != AF_INET ) && ( family != AF_INET6 ) )						continue; // We only care about IPv4 and IPv6.
		if( ( inFamily != AF_UNSPEC ) && ( family != inFamily ) )					continue; // Skip families we're not interested in.
		if( !IsUsableInterfaceAddress( family, ia ) )								continue; // Skip things like 0.0.0.0 addresses.
		if( inInterfaceName && ( strcmp( ia->ifa_name, inInterfaceName ) != 0 ) )	continue; // If there's a name, it must match.
		if( ValidateInterfaceUsability( infoSock, ia ) != kNoErr )					continue; // Interface must be usable.
		if( !outList ) continue; // If the caller just wants to wait for usability then just move to the next interface.
		
		// The interface appears usable so add it to the list. Link it in first to avoid leaks if copying its info fails.
		
		dstIA = (struct ifaddrs *) calloc( 1, sizeof( *dstIA ) );
		require_action( dstIA, exit, err = kNoMemoryErr );
		
		*dstNextPtr = dstIA;
		dstNextPtr	= &dstIA->ifa_next;
		
		dstIA->ifa_name = strdup( ia->ifa_name );
		require_action( dstIA->ifa_name, exit, err = kNoMemoryErr );
		
		dstIA->ifa_flags = ia->ifa_flags;
		
		if( family == AF_INET )	size = sizeof( struct sockaddr_in );
		else					size = sizeof( struct sockaddr_in6 );
		dstIA->ifa_addr = (struct sockaddr	*) malloc( size );
		require_action( dstIA->ifa_addr, exit, err = kNoMemoryErr );
		memcpy( dstIA->ifa_addr, ia->ifa_addr, size );
		
		// Inside the BSD kernel they use a hack where they stuff the sin6->sin6_scope_id value into the second word 
		// of the IPv6 link-local address, so they can just pass around IPv6 address structures instead of full 
		// sockaddr_in6 structures. Those hacked IPv6 addresses aren't supposed to escape the kernel in that form, but 
		// they do. To work around this we always whack the second word of any IPv6 link-local address back to zero.
		// Additionally, getifaddrs may not set sin6_scope_id so we have copy it from the embedded scope ID.
		// See <rdar://problem/3926654&4504347> to track this issue on Mac OS X.
		
		if( family == AF_INET6 )
		{
			sa6 = (struct sockaddr_in6 *) dstIA->ifa_addr;
			if( IN6_IS_ADDR_LINKLOCAL( &sa6->sin6_addr ) )
			{
				if( sa6->sin6_scope_id == 0 )
				{
					sa6->sin6_scope_id = ReadBig16( &sa6->sin6_addr.s6_addr[ 2 ] );
				}
				sa6->sin6_addr.s6_addr[ 2 ] = 0;
				sa6->sin6_addr.s6_addr[ 3 ] = 0;
			}
		}
	}
	
	if( outList )
	{
		*outList = dstList;
		dstList = NULL;
	}
	err = kNoErr;
	
exit:
	if( dstList )					ReleaseUsableInterfaceList( dstList );
	if( iaList )					freeifaddrs( iaList );
	if( IsValidSocket( infoSock ) )	close_compat( infoSock );
	return( err );
}

//===========================================================================================================================
//	ReleaseUsableInterfaceList
//===========================================================================================================================

void	ReleaseUsableInterfaceList( struct ifaddrs *inList )
{
	struct ifaddrs *		ia;
	struct ifaddrs *		iaNext;
	
	for( ia = inList; ia; ia = iaNext )
	{
		iaNext = ia->ifa_next;
		ForgetMem( &ia->ifa_name );
		ForgetMem( &ia->ifa_addr );
		ForgetMem( &ia->ifa_netmask );
		ForgetMem( &ia->ifa_dstaddr );
		ForgetMem( &ia->ifa_data );
		free( ia );
	}
}

//===========================================================================================================================
//	ValidateInterfaceUsability
//===========================================================================================================================

#if( defined( IN6_IFF_TEMPORARY ) )
	#define	kIPv6BadFlags		( IN6_IFF_DUPLICATED | IN6_IFF_DETACHED | IN6_IFF_DEPRECATED | IN6_IFF_TEMPORARY )
#else
	#define	kIPv6BadFlags		( IN6_IFF_DUPLICATED | IN6_IFF_DETACHED | IN6_IFF_DEPRECATED )
#endif

#define	kIPv6TentativeTimeoutUSec		5000000 // 5 seconds
#define	kIPv6TentativeDelay				 100000 // 100 ms
#define	kIPv6TentativeTries				( kIPv6TentativeTimeoutUSec / kIPv6TentativeDelay )

OSStatus	ValidateInterfaceUsability( SocketRef inInfoSock, const struct ifaddrs *inIA )
{
	OSStatus				err;
	struct in6_ifreq		ifr6;
	int						tentativeTries;
	int						flags;
	
	// If this is an IPv6 interface, get the extra IPv6 flags to see if it is really ready for use. It may be doing
	// Duplicate-Address-Detection (DAD) where it reports itself as IFF_UP, but it can't really be used. During this
	// time, the IN6_IFF_TENTATIVE flag wil be set, but unfortunately, there is currently no notification when it is
	// no longer doing DAD. So to hack around this, this code will poll until it either becomes ready (no bad flags 
	// set), it becomes unusable (e.g. it's a duplicate), or it doesn't become either of those in a timely manner.
		
	err = kNoErr;
	if( ( inIA->ifa_addr->sa_family == AF_INET6 ) && IsValidSocket( inInfoSock ) )
	{
		tentativeTries = 0;
		for( ;; )
		{
			memset( &ifr6, 0, sizeof( ifr6 ) );
			strlcpy( ifr6.ifr_name, inIA->ifa_name, sizeof( ifr6.ifr_name ) );
			ifr6.ifr_addr = *( (const struct sockaddr_in6 *) inIA->ifa_addr );
			
			err = ioctl( inInfoSock, SIOCGIFAFLAG_IN6, &ifr6 );
			err = map_socket_value_errno( inInfoSock, err != -1, err );
			require_noerr( err, exit );
			
			flags = ifr6.ifr_ifru.ifru_flags6;
			if( flags & kIPv6BadFlags )
			{
				dlog( kLogLevelNotice, "%s: %8s SIOCGIFAFLAG_IN6 (0x%X) not usable...skipping\n", __ROUTINE__, 
					inIA->ifa_name, flags );
				err = kStateErr;
				goto exit;
			}
			if( flags & IN6_IFF_TENTATIVE )
			{
				if( ++tentativeTries >= kIPv6TentativeTries )
				{
					dlog( kLogLevelWarning, "%s: %8s tentative (0x%X) for too long...skipping\n", __ROUTINE__, 
						inIA->ifa_name, flags );
					err = kTimeoutErr;
					goto exit;
				}
				dlog( kLogLevelInfo, "%s: %8s tentative (0x%X) %d\n", __ROUTINE__, inIA->ifa_name, flags, tentativeTries );
				usleep( kIPv6TentativeDelay );
				continue;
			}
			break;
		}
	}
	
exit:
	return( err );
}
#endif // APPLE_HAVE_ROUTING_SUPPORT

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

#if( TARGET_OS_WINDOWS )
	static OSStatus	LookupHostIPv4Addr( const char *inHost, int inTimeoutMs, uint32_t *outAddr );
#endif

//===========================================================================================================================
//	HostIsPotentiallyReachable
//===========================================================================================================================

Boolean	HostIsPotentiallyReachable( const char *inHost, Boolean inAllowModemConnect )
{
#if( TARGET_OS_MAC && !COMMON_SERVICES_NO_CORE_SERVICES && !COMMON_SERVICES_NO_SYSTEM_CONFIGURATION )

	Boolean							reachable;
	SCNetworkReachabilityRef		reachRef;
	SCNetworkConnectionFlags		flags;
	
	reachable = false;
	
	reachRef = SCNetworkReachabilityCreateWithName( NULL, inHost );
	check( reachRef );
	if( reachRef )
	{
		if( SCNetworkReachabilityGetFlags( reachRef, &flags ) )
		{
			if( flags & kSCNetworkFlagsReachable )
			{
				if( !( flags & kSCNetworkFlagsConnectionRequired ) || inAllowModemConnect )
				{
					reachable = true;
				}
			}
		}
		CFRelease( reachRef );
	}
	return( reachable );
	
#elif( TARGET_OS_WINDOWS )

	DWORD		flags;
	
	(void) inHost; // Unused
	
	// Windows doesn't provide an equivalent API to the Mac's SCNetworkCheckReachabilityByName. IsDestinationReachable 
	// pings the host and thus generates network traffic, which is undesirable. The best we can do is to check if an 
	// active Internet connection is available. If an active Internet connection is not available, but a modem connection 
	// is allowed, then we check if the system is configured for a modem connection.
	
	flags = 0;
	if( InternetGetConnectedState( &flags, 0 ) )
	{
		return( true );
	}
	if( ( flags & INTERNET_CONNECTION_MODEM ) && ( flags & INTERNET_CONNECTION_CONFIGURED ) && inAllowModemConnect )
	{
		return( true );
	}
	return( false );

#else

	(void) inHost;				// Unused
	(void) inAllowModemConnect; // Unused
	
	// No portable API for this so assume it's reachable and let the failure logic handle it.
	
	return( true );

#endif
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

#if  ( ( TARGET_OS_DARWIN && COMMON_SERVICES_NO_CORE_SERVICES ) || TARGET_OS_FREEBSD || TARGET_OS_QNX )
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
//	GetUDPOutgoingSockAddr
//
//	Determines the local (source) sockaddr we'd use to send a UDP packet to the specified remote sockaddr.
//===========================================================================================================================

OSStatus	GetUDPOutgoingSockAddr( const void *inRemoteAddr, void *outLocalAddr )
{
	OSStatus					err;
	SocketRef					sock;
	const struct sockaddr *		sa;
	socklen_t					saLen;
	
	sock = kInvalidSocketRef;
	
	sa = (const struct sockaddr *) inRemoteAddr;
	if(      sa->sa_family == AF_INET  ) saLen = (socklen_t) sizeof( struct sockaddr_in );
#if( defined( AF_INET6 ) )
	else if( sa->sa_family == AF_INET6 ) saLen = (socklen_t) sizeof( struct sockaddr_in6 );
#endif
	else
	{
		dlogassert( "unsupport family: %d", sa->sa_family );
		err = kUnsupportedErr;
		goto exit;
	}
	
	// Connect the UDP socket to the destination address to bind the socket to the address of the interface.
	// This should be the same address used as our source address, but there is a race condition here because
	// the IP routing tables could change before we send the packet.
	
	sock = socket( sa->sa_family, SOCK_DGRAM, IPPROTO_UDP );
	err = map_socket_creation_errno( sock );
	require_noerr( err, exit );
	
	err = connect( sock, sa, saLen );
	err = map_socket_noerr_errno( sock, err );
	require_noerr( err, exit );
	
	err = getsockname( sock, (struct sockaddr *) outLocalAddr, &saLen );
	err = map_socket_noerr_errno( sock, err );
	require_noerr( err, exit );
	
exit:
	ForgetSocket( &sock );
	return( err );
}


#if( TARGET_OS_WINDOWS )
//===========================================================================================================================
//	LookupHostIPv4Addr
//===========================================================================================================================

#define kLookupHostIPv4Addr_WindowClass			TEXT( "LookupHostIPv4Addr" )
#define kLookupHostIPv4Addr_WindowMessage		( WM_USER + 0x100 )

static OSStatus	LookupHostIPv4Addr( const char *inHost, int inTimeoutMs, uint32_t *outAddr )
{
	OSStatus		err;
	HINSTANCE		instance;
	WNDCLASSEX		wcex;
	BOOL			registeredClass;
	HWND			wind;
	HANDLE			lookup;
	char			buf[ MAXGETHOSTSTRUCT ];
	UINT_PTR		timerID;
	MSG				msg;
	BOOL			good;
	
	registeredClass = FALSE;
	wind			= NULL;
	lookup			= NULL;
	timerID			= 0;
	
	// Create a temporary, invisible window since the Windows async DNS APIs require a window.
	
	instance = GetModuleHandle( NULL );
	err = map_global_value_errno( instance, instance );
	require_noerr( err, exit );
	
	wcex.cbSize			= sizeof( wcex );
	wcex.style			= 0;
	wcex.lpfnWndProc	= (WNDPROC) DefWindowProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= instance;
	wcex.hIcon			= NULL;
	wcex.hCursor		= NULL;
	wcex.hbrBackground	= NULL;
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= kLookupHostIPv4Addr_WindowClass;
	wcex.hIconSm		= NULL;
	RegisterClassEx( &wcex );
	registeredClass = TRUE;
	
	wind = CreateWindow( wcex.lpszClassName, wcex.lpszClassName, 0, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, 
		instance, NULL );
	err = map_global_value_errno( wind, wind );
	require_noerr( err, exit );
	
	// Start the async DNS lookup and wait for a response or a timeout.
	
	lookup = WSAAsyncGetHostByName( wind, kLookupHostIPv4Addr_WindowMessage, inHost, buf, (int) sizeof( buf ) );
	err = map_global_value_errno( lookup, lookup );
	require_noerr( err, exit );
	
	timerID = SetTimer( wind, 1, (UINT) inTimeoutMs, NULL );
	err = map_global_value_errno( timerID != 0, timerID );
	require_noerr( err, exit );
	
	for( ;; )
	{
		good = GetMessage( &msg, wind, 0, 0 );
		err = map_global_value_errno( good, good );
		require_noerr( err, exit );
		
		if( msg.message == kLookupHostIPv4Addr_WindowMessage )
		{
			err = WSAGETASYNCERROR( msg.lParam );
			if( err == 0 )
			{
				const struct hostent *		hostInfo;
				
				hostInfo = (const struct hostent *) buf;
				require_action( hostInfo->h_addr_list, exit, err = kUnknownErr );
				
				memcpy( outAddr, hostInfo->h_addr_list[ 0 ], 4 );
			}
			break;
		}
		else if( msg.message == WM_TIMER )
		{
			err = kTimeoutErr;
			break;
		}
		else
		{
			dlog( kLogLevelInfo, "%s: unhandled message: %X, %X, %X\n", __ROUTINE__, msg.message, msg.wParam, msg.lParam );
		}
	}
	
exit:
	if( timerID )			KillTimer( wind, timerID );
	if( lookup )			WSACancelAsyncRequest( lookup );
	if( wind )				DestroyWindow( wind );
	if( registeredClass )	UnregisterClass( kLookupHostIPv4Addr_WindowClass, instance );
	return( err );
}
#endif // TARGET_OS_WINDOWS

//===========================================================================================================================
//	NetworkStackSupportsIPv4MappedIPv6Addresses
//===========================================================================================================================

Boolean	NetworkStackSupportsIPv4MappedIPv6Addresses( void )
{
#if( TARGET_OS_WINDOWS )
	// Vista and later support IPv4-mapped IPv6 addresses. Previous versions of Windows don't.
	
	return( RunningWindowsVistaOrLater() );
#else
	return( true );
#endif
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
//	SendUDPQuitPacket
//===========================================================================================================================

OSStatus	SendUDPQuitPacket( int inPort, SocketRef inSockV4, SocketRef inSockV6 )
{
	OSStatus		err;
	ssize_t			n;
	sockaddr_ip		sip;
	
	err = kNotHandledErr;
	if( IsValidSocket( inSockV4 ) )
	{
		memset( &sip.v4, 0, sizeof( sip.v4 ) );
		SIN_LEN_SET( &sip.v4 );
		sip.v4.sin_family		= AF_INET;
		sip.v4.sin_port			= htons( (uint16_t) inPort );
		sip.v4.sin_addr.s_addr	= htonl( INADDR_LOOPBACK );
		
		n = sendto( inSockV4, "", 0, 0, &sip.sa, (socklen_t) sizeof( sip.v4 ) );
		err = map_socket_value_errno( inSockV4, n == 0, n );
		check_noerr( err );
	}
#if( defined( AF_INET6 ) )
	if( err && IsValidSocket( inSockV6 ) )
	{
		memset( &sip.v6, 0, sizeof( sip.v6 ) );
		SIN6_LEN_SET( &sip.v6 );
		sip.v6.sin6_family	= AF_INET6;
		sip.v6.sin6_port	= htons( (uint16_t) inPort );
		sip.v6.sin6_addr	= in6addr_loopback;
		
		n = sendto( inSockV6, "", 0, 0, &sip.sa, (socklen_t) sizeof( sip.v6 ) );
		err = map_socket_value_errno( inSockV6, n == 0, n );
		check_noerr( err );
	}
#endif
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
//	UDPClientSocketOpen
//===========================================================================================================================

OSStatus
	UDPClientSocketOpen( 
		int				inFamily, 
		const void *	inPeerAddr, 
		int				inPeerPort, 
		int				inListenPort, 
		int *			outListenPort, 
		SocketRef *		outSock )
{
	OSStatus		err;
	SocketRef		sock = kInvalidSocketRef;
	sockaddr_ip		peerAddr;
	sockaddr_ip		sip;
	socklen_t		len;
	
	if( inPeerAddr )
	{
		err = SockAddrSimplify( inPeerAddr, &peerAddr );
		require_noerr( err, exit );
		inPeerAddr = &peerAddr;
	}
	if( inFamily == AF_UNSPEC )
	{
		require_action( inPeerAddr, exit, err = kParamErr );
		inFamily = ( (const struct sockaddr *) inPeerAddr )->sa_family;
	}
	sock = socket( inFamily, SOCK_DGRAM, IPPROTO_UDP );
	err = map_socket_creation_errno( sock );
	require_noerr( err, exit );
	
#if( defined( SO_NOSIGPIPE ) )
	setsockopt( sock, SOL_SOCKET, SO_NOSIGPIPE, &(int){ 1 }, (socklen_t) sizeof( int ) );
#endif
	
	SocketMakeNonBlocking( sock );
	
	// If a listening port was specified, bind to it.
	// A negated port number, other than -1, means "try this port, but allow dynamic". -1 means "don't listen".
	
	if( inListenPort != -1 )
	{
		inListenPort = ( inListenPort < 0 ) ? -inListenPort : inListenPort;
		if( inFamily == AF_INET )
		{
			memset( &sip.v4, 0, sizeof( sip.v4 ) );
			SIN_LEN_SET( &sip.v4 );
			sip.v4.sin_family		= AF_INET;
			sip.v4.sin_port			= htons( (uint16_t) inListenPort );
			sip.v4.sin_addr.s_addr	= htonl( INADDR_ANY );
			len = sizeof( sip.v4 );
		}
		#if( defined( AF_INET6 ) )
		else if( inFamily == AF_INET6 )
		{
			memset( &sip.v6, 0, sizeof( sip.v6 ) );
			SIN6_LEN_SET( &sip.v6 );
			sip.v6.sin6_family		= AF_INET6;
			sip.v6.sin6_port		= htons( (uint16_t) inListenPort );
			sip.v6.sin6_flowinfo	= 0;
			sip.v6.sin6_addr		= in6addr_any;
			sip.v6.sin6_scope_id	= 0;
			len = sizeof( sip.v6 );
		}
		#endif
		else
		{
			dlogassert( "Bad family: %d", inFamily );
			err = kUnsupportedErr;
			goto exit;
		}
		err = bind( sock, &sip.sa, len );
		err = map_socket_noerr_errno( sock, err );
		if( err && ( inListenPort != 0 ) )
		{
			SockAddrSetPort( &sip, 0 );
			err = bind( sock, &sip.sa, len );
			err = map_socket_noerr_errno( sock, err );
		}
		require_noerr( err, exit );
	}
	
	// If a peer address is specified, connect to it we receive ICMP errors, save temporary connects in the kernel, etc.
	// If we're listening on a multicast address then don't connect since that would prevent any packets from being received.
	
	if( inPeerAddr && ( ( inListenPort == -1 ) || !SockAddrIsMulticast( inPeerAddr ) ) )
	{
		if( inPeerPort > 0 ) SockAddrSetPort( &peerAddr, inPeerPort );
		err = connect( sock, &peerAddr.sa, SockAddrGetSize( &peerAddr ) );
		err = map_socket_noerr_errno( sock, err );
		require_noerr( err, exit );
	}
	
	if( outListenPort )
	{
		len = (socklen_t) sizeof( sip );
		err = getsockname( sock, &sip.sa, &len );
		err = map_socket_noerr_errno( sock, err );
		require_noerr( err, exit );
		
		*outListenPort = SockAddrGetPort( &sip );
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
#if( TARGET_HAS_SENDFILE )
	obj->writeFileFunc	= NetSocket_WriteFileInternal;
#elif( TARGET_OS_POSIX )
	obj->writeFileFunc	= NetSocket_WriteFileSlow;
#else
	// $$$ TO DO: Implement non-sendfile-based version.
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
//	NetSocket_ReadFile
//===========================================================================================================================

#define kNetSocket_MaxReadBufferLen		( 4 * kBytesPerMegaByte )

#if( TARGET_OS_POSIX )
OSStatus
	NetSocket_ReadFile( 
		NetSocketRef	inSock, 
		int64_t			inAmount, 
		FDRef			inFileFD, 
		int64_t			inFileOffset, 
		int32_t			inTimeoutSecs )
{
	OSStatus			err;
	off_t				off;
	size_t				len;
	ssize_t				n;
	const uint8_t *		ptr;
	
	len = (size_t) Min( inAmount, kNetSocket_MaxReadBufferLen );
	if( len > inSock->readBufLen )
	{
		ForgetMem( &inSock->readBuffer );
		inSock->readBuffer = (uint8_t *) malloc( len );
		require_action( inSock->readBuffer, exit, err = kNoMemoryErr );
		
		inSock->readBufLen = len;
	}
	
	off = lseek( inFileFD, inFileOffset, SEEK_SET );
	err = map_global_value_errno( off != (off_t) -1, off );
	require_noerr( err, exit );
	
	while( inAmount > 0 )
	{
		len = (size_t) Min( inAmount, (int64_t) inSock->readBufLen );
		err = NetSocket_Read( inSock, len, len, inSock->readBuffer, &len, inTimeoutSecs );
		require_noerr_quiet( err, exit );
		inAmount -= len;
		
		ptr = inSock->readBuffer;
		while( len > 0 )
		{
			n = WriteFD( inFileFD, ptr, len );
			err = map_global_value_errno( n > 0, n );
			require_noerr( err, exit );
			
			ptr += n;
			len -= ( (size_t) n );
		}
	}
	
exit:
	if( inSock->readBufLen > kBytesPerMegaByte )
	{
		ForgetMem( &inSock->readBuffer );
		inSock->readBufLen = 0;
	}
	return( err );
}
#endif // TARGET_OS_POSIX

//===========================================================================================================================
//	NetSocket_WriteFileInternal
//===========================================================================================================================

#if( TARGET_HAS_SENDFILE )
OSStatus
	NetSocket_WriteFileInternal( 
		NetSocketRef	inSock, 
		iovec_t *		inHeaderArray, 
		int				inHeaderCount, 
		iovec_t *		inTrailerArray, 
		int				inTrailerCount, 
		FDRef			inFileFD, 
		int64_t			inFileOffset, 
		int64_t			inFileAmount, 
		int32_t			inTimeoutSecs )
{
	OSStatus		err;
	
	require_action( inSock && ( inSock->magic == kNetSocketMagic ), exit, err = kBadReferenceErr );
	require_action_quiet( !inSock->canceled, exit, err = kCanceledErr );
	require_action( IsValidSocket( inSock->nativeSock ), exit, err = kNotPreparedErr );
	
	for( ;; )
	{
		err = SocketSendFile( inSock->nativeSock, &inHeaderArray, &inHeaderCount, &inTrailerArray, &inTrailerCount, 
			inFileFD, &inFileOffset, &inFileAmount );
		if( err == kNoErr )			break;
		if( err == EWOULDBLOCK )	err = kNoErr;
		if( err == EPIPE )			goto exit;
		if( err == ENOTCONN )		{ err = EPIPE; goto exit; } // Workaround sendfile() bug where it returns ENOTCONN.
		require_noerr( err, exit );
		
		err = NetSocket_Wait( inSock, inSock->nativeSock, kNetSocketWaitType_Write, inTimeoutSecs );
		if( err ) goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}
#endif

//===========================================================================================================================
//	NetSocket_WriteFileSlow
//===========================================================================================================================

#define NETSOCKET_WRITE_FILE_LOGGING		1

#if( TARGET_OS_POSIX )
OSStatus
	NetSocket_WriteFileSlow( 
		NetSocketRef	inSock, 
		iovec_t *		inHeaderArray, 
		int				inHeaderCount, 
		iovec_t *		inTrailerArray, 
		int				inTrailerCount, 
		FDRef			inFileFD, 
		int64_t			inFileOffset, 
		int64_t			inFileAmount, 
		int32_t			inTimeoutSecs )
{
#if( NETSOCKET_WRITE_FILE_LOGGING )
	LogCategory *	ucat = inSock->ucat ? inSock->ucat : &log_category_from_name( NetSocket );
	uint64_t		startTicks, progressDelayTicks, nextProgressTicks, nowTicks;
	int64_t			totalBytes, currentBytes;
	double			ticksPerSecF, deltaTicksF;
	double			currentBytesF, totalBytesF, mbPerSec;
#endif
	OSStatus		err;
	Boolean			sentHeader;
	uint8_t *		buf;
	size_t			bufLen;
	size_t			len;
	ssize_t			n;
	iovec_t			iov[ 8 ];
	int				ion;
	
#if( NETSOCKET_WRITE_FILE_LOGGING )
	startTicks			= UpTicks();
	progressDelayTicks	= SecondsToUpTicks( 1 );
	nextProgressTicks	= 0;
	totalBytes			= inFileAmount;
	currentBytes		= 0;
	ticksPerSecF		= (double) UpTicksPerSecond();
#endif
	
	buf = NULL;
	require_action( inSock && ( inSock->magic == kNetSocketMagic ), exit, err = kBadReferenceErr );
	require_action_quiet( !inSock->canceled, exit, err = kCanceledErr );
	require_action( IsValidSocket( inSock->nativeSock ), exit, err = kNotPreparedErr );
	
	err = ( lseek( inFileFD, inFileOffset, SEEK_SET ) != -1 ) ? kNoErr : errno_safe();
	require_noerr( err, exit );
	
	sentHeader = false;
	if( inFileAmount > 0 )
	{
		bufLen = 1 * kBytesPerMegaByte;
		buf = (uint8_t *) malloc( bufLen );
		require_action( buf, exit, err = kNoMemoryErr );
		
		while( inFileAmount > 0 )
		{
			len = (size_t) Min( inFileAmount, (off_t) bufLen );
			n = read( inFileFD, buf, len );
			if( n > 0 )
			{
				if( !sentHeader )
				{
					for( ion = 0; ion < inHeaderCount; ++ion )
					{
						iov[ ion ] = inHeaderArray[ ion ];
					}
					iov[ ion ].iov_base = buf;
					iov[ ion ].iov_len  = (size_t) n;
					++ion;
					
					err = NetSocket_WriteV( inSock, iov, ion, inTimeoutSecs );
					if( err ) goto exit;
					sentHeader = true;
				}
				else
				{
					err = NetSocket_Write( inSock, buf, (size_t) n, inTimeoutSecs );
					if( err ) goto exit;
				}
				inFileAmount -= n;
				
				#if( NETSOCKET_WRITE_FILE_LOGGING )
					currentBytes += n;
					nowTicks = UpTicks();
					if( nowTicks >= nextProgressTicks )
					{
						currentBytesF	= (double) currentBytes;
						totalBytesF		= (double) totalBytes;
						deltaTicksF		= (double)( nowTicks - startTicks );
						mbPerSec		= ( ( currentBytesF / deltaTicksF ) * ticksPerSecF ) / kBytesPerMegaByte;
						ulog( ucat, kLogLevelInfo, "Wrote %10lld of %10lld, %6.2f%%, %6.2f Mbit/sec\n", 
							currentBytes, totalBytes, 100.0 * ( currentBytesF / totalBytesF ), mbPerSec * 8 );
						nextProgressTicks = nowTicks + progressDelayTicks;
					}
				#endif
			}
			else if( n == 0 )
			{
				err = kUnderrunErr;
				goto exit;
			}
			else
			{
				err = errno_safe();
				dlogassert( "read failed: %#m", err );
				goto exit;
			}
		}
	}
	if( !sentHeader )
	{
		err = NetSocket_WriteV( inSock, inHeaderArray, inHeaderCount, inTimeoutSecs );
		require_noerr( err, exit );
	}
	
	err = NetSocket_WriteV( inSock, inTrailerArray, inTrailerCount, inTimeoutSecs );
	require_noerr( err, exit );
	
exit:
	if( buf ) free( buf );
	
#if( NETSOCKET_WRITE_FILE_LOGGING )
	mbPerSec = ( ( ( (double) currentBytes ) / ( (double)( UpTicks() - startTicks ) ) ) * ticksPerSecF ) / kBytesPerMegaByte;
	ulog( ucat, kLogLevelInfo, "Wrote %lld of %lld, %6.2f Mbit/sec: %#m\n", currentBytes, totalBytes, mbPerSec * 8, err );
#endif
	
	return( err );
}
#endif // TARGET_OS_POSIX

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
//	SocketReadAll
//===========================================================================================================================

OSStatus	SocketReadAll( SocketRef inSock, void *inData, size_t inSize )
{
	OSStatus		err;
	uint8_t *		dst;
	uint8_t *		lim;
	ssize_t			n;
	
	dst = (uint8_t *) inData;
	lim = dst + inSize;
	while( dst < lim )
	{
		n = recv( inSock, (char *) dst, (size_t)( lim - dst ), 0 );
		if( n > 0 )
		{
			dst += n;
		}
		else if( n == 0 )
		{
			err = kConnectionErr;
			goto exit;
		}
		else
		{
			err = socket_value_errno( inSock, n );
			if( err == EWOULDBLOCK )
			{
				fd_set		readSet;
				
				FD_ZERO( &readSet );
				FD_SET( inSock, &readSet );
				n = select( (int)( inSock + 1 ), &readSet, NULL, NULL, NULL );
				if( n == 0 ) { err = kTimeoutErr; goto exit; }
				err = map_global_value_errno( n > 0, n );
				require_noerr( err, exit );
			}
			#if( TARGET_OS_POSIX )
			else if( err == EINTR )
			{
				continue;
			}
			#endif
			else
			{
				goto exit;
			}
		}
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	SocketWriteAll
//===========================================================================================================================

OSStatus	SocketWriteAll( SocketRef inSock, const void *inData, size_t inSize, int32_t inTimeoutSecs )
{
	OSStatus			err;
	const uint8_t *		src;
	const uint8_t *		end;
	fd_set				writeSet;
	struct timeval		timeout;
	ssize_t				n;
	
	FD_ZERO( &writeSet );
	src = (const uint8_t *) inData;
	end = src + inSize;
	while( src < end )
	{
		FD_SET( inSock, &writeSet );
		timeout.tv_sec 	= inTimeoutSecs;
		timeout.tv_usec = 0;
		n = select( (int)( inSock + 1 ), NULL, &writeSet, NULL, &timeout );
		if( n == 0 ) { err = kTimeoutErr; goto exit; }
		err = map_socket_value_errno( inSock, n > 0, n );
		require_noerr( err, exit );
		
		n = send( inSock, (char *) src, (size_t)( end - src ), 0 );
		err = map_socket_value_errno( inSock, n >= 0, n );
		if( err == EINTR ) continue;
		require_noerr( err, exit );
		
		src += n;
	}
	err = kNoErr;
	
exit:
	return( err );
}

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
//	SocketSendFile
//===========================================================================================================================

#if( TARGET_HAS_SENDFILE )
extern int sendfile(int fd, int s, off_t offset, off_t *len, struct sf_hdtr *hdtr, int flags) __attribute__((weak_import));

static void	SocketSendFile_UpdateIOVec( iovec_t **ioArray, int *ioCount, off_t *ioAmount );

OSStatus
	SocketSendFile( 
		SocketRef	inSock, 
		iovec_t **	ioHeaderArray, 
		int *		ioHeaderCount, 
		iovec_t **	ioTrailerArray, 
		int *		ioTrailerCount, 
		FDRef		inFileFD, 
		int64_t *	ioFileOffset, 
		int64_t *	ioFileAmount )
{
	OSStatus			err;
	struct sf_hdtr		hdtr;
	off_t				fileOffset;
	off_t				fileAmount;
	off_t				len;
	int					i;
	
	if( ioHeaderArray )						hdtr.headers = *ioHeaderArray;
	else									hdtr.headers = NULL;
	if( hdtr.headers && ioHeaderCount )		hdtr.hdr_cnt = *ioHeaderCount;
	else									hdtr.hdr_cnt = 0;
	
	if( ioTrailerArray )					hdtr.trailers = *ioTrailerArray;
	else									hdtr.trailers = NULL;
	if( hdtr.trailers && ioTrailerCount )	hdtr.trl_cnt = *ioTrailerCount;
	else									hdtr.trl_cnt = 0;
	
	fileOffset = *ioFileOffset;
	fileAmount = *ioFileAmount;
	
	for( ;; )
	{
		len = fileAmount;
		if( hdtr.headers )
		{
			for( i = 0; i < hdtr.hdr_cnt; ++i )
			{
				len += hdtr.headers[ i ].iov_len;
			}
		}
		err = sendfile( inFileFD, inSock, fileOffset, &len, &hdtr, 0 );
		check( ( hdtr.hdr_cnt == 0 ) || hdtr.headers );
		
		if( hdtr.headers ) SocketSendFile_UpdateIOVec( &hdtr.headers, &hdtr.hdr_cnt, &len );
		if( len >= fileAmount )
		{
			len			-= fileAmount;
			fileOffset	+= fileAmount;
			fileAmount	 = 0;
		}
		else
		{
			fileOffset	+= len;
			fileAmount	-= len;
			len			 = 0;
		}
		if( hdtr.trailers ) SocketSendFile_UpdateIOVec( &hdtr.trailers, &hdtr.trl_cnt, &len );
		
		if( err == 0 ) break;
		err = errno_compat();
		if( err == EINTR )	continue;
		if( err == 0 )		err = kUnknownErr;
		break;
	}
	
	if( ioHeaderArray )		*ioHeaderArray	= hdtr.headers;
	if( ioHeaderCount )		*ioHeaderCount	= hdtr.hdr_cnt;
	if( ioTrailerArray )	*ioTrailerArray	= hdtr.trailers;
	if( ioTrailerCount )	*ioTrailerCount	= hdtr.trl_cnt;
	*ioFileOffset = fileOffset;
	*ioFileAmount = fileAmount;
	return( err );
}

//===========================================================================================================================
//	SocketSendFile_UpdateIOVec
//===========================================================================================================================

static void	SocketSendFile_UpdateIOVec( iovec_t **ioArray, int *ioCount, off_t *ioAmount )
{
	iovec_t *		ptr;
	iovec_t *		end;
	off_t			amount;
	
	ptr		= *ioArray;
	end		= ptr + *ioCount;
	amount	= *ioAmount;
	while( ptr < end )
	{
		off_t		len;
		
		len = (off_t) ptr->iov_len;
		if( amount >= len )
		{
			amount -= len;
			++ptr;
		}
		else
		{
			ptr->iov_base = ( (char *)( ptr->iov_base ) ) + amount;
			ptr->iov_len  = (unsigned int)( len - amount );
			amount = 0;
			break;
		}
	}
	*ioArray  = ( ptr < end ) ? ptr : NULL; // sendfile fails with EINVAL if non-NULL with a count of 0 so NULL it.
	*ioCount  = (int)( end - ptr );
	*ioAmount = amount;
}
#endif // TARGET_HAS_SENDFILE

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
		/* lsk */	
		if(( strcmp( inIfName, "tether" ) == 0 )  
		   || ( strcmp( inIfName, "uap0" ) == 0 ))  *outTransportType = kNetTransportType_WiFi;
		
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

//===========================================================================================================================
//	SockAddrConvertToIPv6
//
//	Converts a sockaddr to IPv6 sockaddr_in6 (if AF_INET, convert to IPv4-mapped IPv6, if AF_INET6, just copy).
//	Note: "inSrc" may point to the same memory as "outDst", such as SockAddrConvertToIPv6( &addr, &addr ).
//===========================================================================================================================

#if( defined( AF_INET6 ) )
OSStatus	SockAddrConvertToIPv6( const void *inSrc, void *outDst )
{
	OSStatus		err;
	int				family;
	
	family = ( (const struct sockaddr *) inSrc )->sa_family;
	if( family == AF_INET )
	{
		const struct sockaddr_in *		sa4;
		struct sockaddr_in6 *			sa6;
		uint8_t							a4[ 4 ];
		uint16_t						port;
		
		// Locally save off needed fields from the IPv4 address so we can re-build as IPv6 in-place.
		
		sa4 = (const struct sockaddr_in *) inSrc;
		memcpy( a4, &sa4->sin_addr.s_addr, 4 );
		port = sa4->sin_port;
		
		// Re-build the IPv4 address as an IPv4-mapped IPv6 address.
		
		sa6 = (struct sockaddr_in6 *) outDst;
		memset( sa6, 0, sizeof( *sa6 ) );
		SIN6_LEN_SET( sa6 );
		sa6->sin6_family	= AF_INET6;
		sa6->sin6_port		= port;
		sa6->sin6_flowinfo	= 0;
		sa6->sin6_addr.s6_addr[ 10 ] = 0xFF;			// ::FFFF:<32-bit IPv4 address>
		sa6->sin6_addr.s6_addr[ 11 ] = 0xFF;
		memcpy( &sa6->sin6_addr.s6_addr[ 12 ], a4, 4 ); // IPv4 address is in the low 32 bits of the IPv6 address.
		sa6->sin6_scope_id	= 0;
	}
	else if( family == AF_INET6 )
	{
		if( inSrc != outDst )
		{
			memmove( outDst, inSrc, sizeof( struct sockaddr_in6 ) );
		}
	}
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
#endif

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
//	SockAddrGetIPv4
//===========================================================================================================================

OSStatus	SockAddrGetIPv4( const void *inSrc, uint32_t *outIPv4 )
{
	OSStatus		err;
	int				family;
	uint32_t		ipv4;
	
	family = ( (const struct sockaddr *) inSrc )->sa_family;
	if( family == AF_INET )
	{
		ipv4 = ( (const struct sockaddr_in *) inSrc )->sin_addr.s_addr;
	}
#if( defined( AF_INET6 ) )
	else if( family == AF_INET6 )
	{
		const struct sockaddr_in6 *			sa6;
		
		sa6 = (const struct sockaddr_in6 *) inSrc;
		if( IN6_IS_ADDR_V4MAPPED( &sa6->sin6_addr ) || IN6_IS_ADDR_V4COMPAT( &sa6->sin6_addr ) )
		{
			memcpy( &ipv4, &sa6->sin6_addr.s6_addr[ 12 ], 4 ); // IPv4 address is in the low 32 bits of the IPv6 address.
		}
		else
		{
			err = kTypeErr;
			goto exit;
		}
	}
#endif
	else
	{
		dlogassert( "unknown family: %d", family );
		err = kUnsupportedErr;
		goto exit;
	}
	
	if( outIPv4 ) *outIPv4 = ipv4;
	err = kNoErr;
	
exit:
	return( err );
}

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

static Boolean	_EqualBits( const void *inA, const void *inB, uint8_t inBitCount );

//===========================================================================================================================
//	IsIPv4MartianAddress
//===========================================================================================================================

typedef struct
{
	uint8_t		addr[ 4 ];
	uint8_t		prefix;
	
}	MartianRuleIPv4;

Boolean	IsIPv4MartianAddress( uint32_t inAddr )
{
	static const MartianRuleIPv4	kMartianRules[] =
	{
		{ { 0x00, 0x00, 0x00, 0x00 },  8 }, // 0.0.0.0/8			Self identification
		{ { 0x00, 0x00, 0x00, 0x00 }, 32 }, // 0.0.0.0/32			Broadcast
		{ { 0x0a, 0x00, 0x00, 0x00 },  8 }, // 10.0.0.0/8			Private Networks (RFC 1918)
		{ { 0x7f, 0x00, 0x00, 0x00 },  8 }, // 127.0.0.0/8			Loopback
		{ { 0x80, 0x00, 0x00, 0x00 }, 16 }, // 128.0.0.0/16			IANA Reserved (RFC 3330)
		{ { 0xac, 0x10, 0x00, 0x00 }, 12 }, // 172.16.0.0/12		Private Networks (RFC 1918)
		{ { 0xa9, 0xfe, 0x00, 0x00 }, 16 }, // 169.254.0.0/16		Link Local
		{ { 0xbf, 0xff, 0x00, 0x00 }, 16 }, // 191.255.0.0/16		IANA Reserved (RFC 3330)
		{ { 0xc0, 0x00, 0x00, 0x00 }, 24 }, // 192.0.0.0/24			IANA Reserved (RFC 3330)
		{ { 0xc0, 0x00, 0x02, 0x00 }, 24 }, // 192.0.2.0/24			Test-Net (RFC 3330)
		{ { 0xc0, 0xa8, 0x00, 0x00 }, 16 }, // 192.168.0.0/16		Private Networks (RFC 1918)
		{ { 0xc6, 0x12, 0x00, 0x00 }, 15 }, // 198.18.0.0/15		Network Interconnect Device Benchmark Testing
		{ { 0xdf, 0xff, 0xff, 0x00 }, 24 }, // 223.255.255.0/24		Special Use Networks (RFC 3330)
		{ { 0xe0, 0x00, 0x00, 0x00 },  4 }, // 224.0.0.0/4			Multicast
		{ { 0xf0, 0x00, 0x00, 0x00 },  4 }  // 240.0.0.0/4			Class E Reserved
	};
	
	const MartianRuleIPv4 *		src;
	const MartianRuleIPv4 *		end;
	
	src = kMartianRules;
	end = src + countof( kMartianRules );
	for( ; src < end; ++src )
	{
		if( _EqualBits( &inAddr, src->addr, src->prefix ) )
		{
			return( true );
		}
	}
	return( false );
}

//===========================================================================================================================
//	IsIPv6MartianAddress
//===========================================================================================================================

typedef struct
{
	const char *		addr;
	uint8_t				prefix;
	
}	MartianRuleIPv6;

Boolean	IsIPv6MartianAddress( const void *inAddr )
{
	return( IsIPv6MartianAddressEx( inAddr, kMartianFlags_None ) );
}

Boolean	IsIPv6MartianAddressEx( const void *inAddr, uint32_t inFlags )
{
	static const MartianRuleIPv6		kMartianRules[] =
	{
		{ "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",	128 }, //  0 ::/128				Unspecified address
		{ "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01",	128 }, //  1 ::1/128			Local host loopback address
		{ "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",					 96 }, //  2 ::/96				IPv4-compatible IPv6 address - deprecated by RFC4291
		{ "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff",					 96 }, //  3 ::ffff:0.0.0.0/96	IPv4-mapped addresses
		{ "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xe0",				100 }, //  4 ::224.0.0.0/100	Compatible address (IPv4 format)
		{ "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x7f",				104 }, //  5 ::127.0.0.0/104	Compatible address (IPv4 format)
		{ "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",				104 }, //  6 ::0.0.0.0/104		Compatible address (IPv4 format)
		{ "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff",				104 }, //  7 ::255.0.0.0/104	Compatible address (IPv4 format)
		{ "\x00",																  8 }, //  8 0000::/8			Pool used for unspecified, loopback and embedded IPv4 addresses
		{ "\x02",																  7 }, //  9 0200::/7			OSI NSAP-mapped prefix set (RFC4548) - deprecated by RFC4048
		{ "\x3f\xfe",															 16 }, // 10 3ffe::/16			Former 6bone, now decommissioned
		{ "\x20\x01\x0d\xb8",													 32 }, // 11 2001:db8::/32		Reserved by IANA for special purposes and documentation
		{ "\x20\x02\xe0",														 20 }, // 12 2002:e000::/20		Invalid 6to4 packets (IPv4 multicast)
		{ "\x20\x02\x7f",														 24 }, // 13 2002:7f00::/24		Invalid 6to4 packets (IPv4 loopback)
		{ "\x20\x02\x00",														 24 }, // 14 2002:0000::/24		Invalid 6to4 packets (IPv4 default)
		{ "\x20\x02\xff",														 24 }, // 15 2002:ff00::/24		Invalid 6to4 packets
		{ "\x20\x02\x0a",														 24 }, // 16 2002:0a00::/24		Invalid 6to4 packets (IPv4 private 10.0.0.0/8 network)
		{ "\x20\x02\xac\x10",													 28 }, // 17 2002:ac10::/28		Invalid 6to4 packets (IPv4 private 172.16.0.0/12 network)
		{ "\x20\x02\xc0\xa8",													 32 }, // 18 2002:c0a8::/32		Invalid 6to4 packets (IPv4 private 192.168.0.0/16 network)
		{ "\xfc",																  7 }, // 19 fc00::/7			Unicast Unique Local Addresses (ULA) - RFC 4193
		{ "\xfe\x80",															 10 }, // 20 fe80::/10			Link-local Unicast
		{ "\xfe\xc0",															 10 }, // 21 fec0::/10			Site-local Unicast - deprecated by RFC 3879 (replaced by ULA)
		{ "\xff",																  8 }, // 22 ff00::/8			Multicast
	};
	
	size_t						i;
	const MartianRuleIPv6 *		rule;
	
	for( i = 0; i < countof( kMartianRules ); ++i )
	{
		rule = &kMartianRules[ i ];
		if( _EqualBits( inAddr, rule->addr, rule->prefix ) )
		{
			if(      ( inFlags & kMartianFlags_AllowUnspecified ) && ( i ==  0 ) ) return( false );
			else if( ( inFlags & kMartianFlags_AllowLinkLocal )   && ( i == 20 ) ) return( false );
			else if( ( inFlags & kMartianFlags_AllowUniqueLocal ) && ( i == 19 ) ) return( false );
			return( true );
		}
	}
	return( false );
}

//===========================================================================================================================
//	_EqualBits
//===========================================================================================================================

static Boolean	_EqualBits( const void *inA, const void *inB, uint8_t inBitCount )
{
	const uint8_t *		a;
	const uint8_t *		b;
	
	a = (const uint8_t *) inA;
	b = (const uint8_t *) inB;
	for( ; inBitCount >= 8; inBitCount -= 8 )
	{
		if( *a++ != *b++ )
			return( false );
	}
	if( inBitCount > 0 )
	{
		if( ( *a ^ *b ) & ~( 0x0FF >> inBitCount ) )
			return( false );
	}
	return( true );
}

//===========================================================================================================================
//	IsGlobalIPv4Address
//
//	Note: The input IPv4 address must be in network byte order (i.e. same format as sin_addr.s_addr).
//===========================================================================================================================

int	IsGlobalIPv4Address( uint32_t inIPv4 )
{
	if( IsPrivateIPv4Address( inIPv4 ) )		return( 0 ); // RFC 1918 private address.
	inIPv4 = ntohl( inIPv4 );
	if( ( inIPv4 >> 16 ) == 0xA9FE )			return( 0 ); // 169.254.x.x/16	Link-Local
	if( ( inIPv4 >> 16 ) == 0x0000 )			return( 0 ); // 0.0.x.x/16		Zero
	if( ( inIPv4 & 0xFF000000 ) == 0x7F000000 )	return( 0 ); // 127.x.x.x/24	Loopback
	if( ( inIPv4 & 0xFF000000 ) >= 0xE0000000 )	return( 0 ); // 224.x.x.x/24	Mulitcast
	return( 1 );
}

//===========================================================================================================================
//	IsPrivateIPv4Address
//
//	Note: The input IPv4 address must be in network byte order (i.e. same format as sin_addr.s_addr).
//===========================================================================================================================

int	IsPrivateIPv4Address( uint32_t inIPv4 )
{
	// Note: these address ranges come from RFC 1918.
	
	inIPv4 = ntohl( inIPv4 );
	if( ( inIPv4 >= 0x0A000000 ) && ( inIPv4 <= 0x0AFFFFFF ) ) return( 1 );	// 10.0.0.0    - 10.255.255.255  (10/8 prefix)
	if( ( inIPv4 >= 0xAC100000 ) && ( inIPv4 <= 0xAC1FFFFF ) ) return( 1 );	// 172.16.0.0  - 172.31.255.255  (172.16/12 prefix)
	if( ( inIPv4 >= 0xC0A80000 ) && ( inIPv4 <= 0xC0A8FFFF ) ) return( 1 );	// 192.168.0.0 - 192.168.255.255 (192.168/16 prefix)
	return( 0 );
}

//===========================================================================================================================
//	IsRoutableIPv4Address
//
//	Note: The input IPv4 address must be in network byte order (i.e. same format as sin_addr.s_addr).
//===========================================================================================================================

int	IsRoutableIPv4Address( uint32_t inIPv4 )
{
	inIPv4 = ntohl( inIPv4 );
	if( ( inIPv4 >> 16 ) == 0xA9FE ) return( 0 ); // 169.254.x.x/16	Link-Local
	if( ( inIPv4 >> 16 ) == 0x0000 ) return( 0 ); // 0.0.x.x/16		Zero
	return( 1 );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	CompareMACAddresses
//===========================================================================================================================

int	CompareMACAddresses( const void *inMACAddress1, const void *inMACAddress2 )
{
	return( memcmp( inMACAddress1, inMACAddress2, 6 ) );
}

//===========================================================================================================================
//	GetInterfaceExtendedFlags
//===========================================================================================================================

#if( defined( SIOCGIFEFLAGS ) )
uint64_t	GetInterfaceExtendedFlags( const char *inIfName )
{
	OSStatus			err;
	SocketRef			sock;
	struct ifreq		ifr;
	
	sock = socket( AF_INET, SOCK_DGRAM, 0 );
	err = map_socket_creation_errno( sock );
	require_noerr( err, exit );
	
	memset( &ifr, 0, sizeof( ifr ) );
	strlcpy( ifr.ifr_name, inIfName, sizeof( ifr.ifr_name ) );
	err = ioctl( sock, SIOCGIFEFLAGS, (caddr_t) &ifr );
	err = map_socket_noerr_errno( sock, err );
	ForgetSocket( &sock );
	
exit:
	return( !err ? ifr_get_eflags( &ifr ) : 0 );
}
#endif

//===========================================================================================================================
//	IsP2PInterface
//===========================================================================================================================

#if( defined( SIOCGIFEFLAGS ) )
Boolean	IsP2PInterface( const char *inIfName )
{
	return( ( GetInterfaceExtendedFlags( inIfName ) & IFEF_LOCALNET_PRIVATE ) ? true : false );
}
#else
#if( TARGET_OS_DARWIN )
	#error "SIOCGIFEFLAGS not defined on Darwin?"
#endif
Boolean	IsP2PInterface( const char *inIfName )
{
	(void) inIfName;
	
	return( false );
}
#endif

//===========================================================================================================================
//	IsWiFiNetworkInterface
//===========================================================================================================================

#if  ( TARGET_OS_POSIX && defined( IFM_IEEE80211 ) )
Boolean	IsWiFiNetworkInterface( const char *inIfName )
{
	uint32_t		ifmedia = 0;
	
	SocketGetInterfaceInfo( kInvalidSocketRef, inIfName, NULL, NULL, NULL, &ifmedia, NULL, NULL, NULL, NULL );
	return( IFM_TYPE( ifmedia ) == IFM_IEEE80211 );
}
#else
Boolean	IsWiFiNetworkInterface( const char *inIfName )
{
	(void) inIfName;
	
	return( false );
}
#endif

//===========================================================================================================================
//	GetIPAddress
//===========================================================================================================================

#if( TARGET_OS_POSIX )
OSStatus	GetIPAddress( const char *inInterfaceName, uint32_t *outIPAddress, uint32_t *outNetmask )
{
	OSStatus err = kParamErr;
	require( inInterfaceName, exit );

	struct ifaddrs *ifap;
	if ( getifaddrs( &ifap ) == 0 )
	{
		struct ifaddrs *ifp = ifap;

		while ( ifp != NULL )
		{
			if ( strcmp( inInterfaceName, ifp->ifa_name ) == 0 )
			{
				if ( ifp->ifa_addr->sa_family == AF_INET )
				{
					struct sockaddr_in* sa4 = (struct sockaddr_in*)ifp->ifa_addr;
					struct sockaddr_in* mask = (struct sockaddr_in*)ifp->ifa_netmask;
					
					dlog(kLogLevelVerbose, "GetIPAddress: found address %.4a on interface %s\n",
						&(sa4->sin_addr.s_addr), inInterfaceName );
					
					if ( outIPAddress )
					{
						*outIPAddress = sa4->sin_addr.s_addr;
					}
					
					if ( outNetmask )
					{
						*outNetmask = mask->sin_addr.s_addr;
					}

					// we found one -- cool
					err = kNoErr;
					if ( !SockAddrIsLinkLocal( sa4 ) && ( sa4->sin_addr.s_addr != 0 ) )
					{
						// stop looking if we found a non-link-local one, but make sure it's not 0
						break;
					}
				}
			}
			
			ifp = ifp->ifa_next;
		}
		
		freeifaddrs( ifap );
	}
	
exit:

	return( err );
}
#endif // TARGET_OS_POSIX

//===========================================================================================================================
//	GetPrimaryIPAddress
//===========================================================================================================================

#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
OSStatus	GetPrimaryIPAddress( sockaddr_ip *outIPv4, sockaddr_ip *outIPv6 )
{
	OSStatus				err;
	SCDynamicStoreRef		store;
	CFStringRef				entity  = NULL;
	CFStringRef				primary	= NULL;
	CFDictionaryRef			dict;
	CFStringRef				str;
	CFArrayRef				array;
	char					cstr[ 128 ];
	
	store = SCDynamicStoreCreate( NULL, CFSTR( "NetUtils:GetPrimaryIPAddress" ), NULL, NULL );
	err = map_scerror( store );
	require_noerr( err, exit );
	
	// Get the primary network interface. Try IPv4 first.
	
	entity = SCDynamicStoreKeyCreateNetworkGlobalEntity( NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4 );
	if( entity )
	{
		dict = (CFDictionaryRef) SCDynamicStoreCopyValue( store, entity );
		if( dict )
		{
			if( CFIsType( dict, CFDictionary ) )
			{
				str = CFDictionaryGetCFString( dict, kSCDynamicStorePropNetPrimaryInterface, NULL );
				if( str )
				{
					primary = str;
					CFRetain( primary );
				}
			}
			CFRelease( dict );
		}
		CFRelease( entity );
	}
	
	// If no IPv4, try IPv6.
	
	if( !primary )
	{
		entity = SCDynamicStoreKeyCreateNetworkGlobalEntity( NULL, kSCDynamicStoreDomainState, kSCEntNetIPv6 );
		if( entity )
		{
			dict = (CFDictionaryRef) SCDynamicStoreCopyValue( store, entity );
			if( dict )
			{
				if( CFIsType( dict, CFDictionary ) )
				{
					str = CFDictionaryGetCFString( dict, kSCDynamicStorePropNetPrimaryInterface, NULL );
					if( str )
					{
						primary = str;
						CFRetain( primary );
					}
				}
				CFRelease( dict );
			}
			CFRelease( entity );
		}
	}
	if( !primary )
	{
		primary = CFSTR( "en0" );
		CFRetain( primary );
	}
	
	// IPv4 address
	
	if( outIPv4 )
	{
		outIPv4->sa.sa_family = AF_UNSPEC;
		
		entity = SCDynamicStoreKeyCreateNetworkInterfaceEntity( NULL, kSCDynamicStoreDomainState, primary, kSCEntNetIPv4 );
		if( entity )
		{
			dict = (CFDictionaryRef) SCDynamicStoreCopyValue( store, entity );
			if( dict )
			{
				if( CFIsType( dict, CFDictionary ) )
				{
					array = CFDictionaryGetCFArray( dict, kSCPropNetIPv4Addresses, NULL );
					if( array && ( CFArrayGetCount( array ) > 0 ) )
					{
						str = CFArrayGetCFStringAtIndex( array, 0, NULL );
						if( str )
						{
							*cstr = '\0';
							CFStringGetCString( str, cstr, (CFIndex) sizeof( cstr ), kCFStringEncodingUTF8 );
							StringToSockAddr( cstr, outIPv4, sizeof( *outIPv4 ), NULL );
							if( outIPv4->sa.sa_family != AF_INET ) outIPv4->sa.sa_family = AF_UNSPEC;
						}
					}
				}
				CFRelease( dict );
			}
			CFRelease( entity );
		}
	}
	
	// IPv6 address
	
	if( outIPv6 )
	{
		outIPv6->sa.sa_family = AF_UNSPEC;
		
		entity = SCDynamicStoreKeyCreateNetworkInterfaceEntity( NULL, kSCDynamicStoreDomainState, primary, kSCEntNetIPv6 );
		if( entity )
		{
			dict = (CFDictionaryRef) SCDynamicStoreCopyValue( store, entity );
			if( dict )
			{
				if( CFIsType( dict, CFDictionary ) )
				{
					array = CFDictionaryGetCFArray( dict, kSCPropNetIPv6Addresses, NULL );
					if( array && ( CFArrayGetCount( array ) > 0 ) )
					{
						str = CFArrayGetCFStringAtIndex( array, 0, NULL );
						if( str )
						{
							*cstr = '\0';
							CFStringGetCString( str, cstr, (CFIndex) sizeof( cstr ), kCFStringEncodingUTF8 );
							StringToSockAddr( cstr, outIPv6, sizeof( *outIPv6 ), NULL );
							if( outIPv6->sa.sa_family != AF_INET6 ) outIPv6->sa.sa_family = AF_UNSPEC;
							if( SockAddrIsIPv6LinkLocal( outIPv6 ) )
							{
								*cstr = '\0';
								CFStringGetCString( primary, cstr, (CFIndex) sizeof( cstr ), kCFStringEncodingUTF8 );
								outIPv6->v6.sin6_scope_id = if_nametoindex( cstr );
							}
						}
					}
				}
				CFRelease( dict );
			}
			CFRelease( entity );
		}
	}
	
exit:
	CFReleaseNullSafe( primary );
	CFReleaseNullSafe( store );
	return( err );
}
#endif // TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES

//===========================================================================================================================
//	GetPrimaryIPAddress
//===========================================================================================================================

#if( !( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES ) )
OSStatus	GetPrimaryIPAddress( sockaddr_ip *outIPv4, sockaddr_ip *outIPv6 )
{
	OSStatus				err;
	struct ifaddrs *		iflist = NULL;
	struct ifaddrs *		ifa;
	Boolean					needIPv4, needIPv6;
	
	if( outIPv4 ) outIPv4->sa.sa_family = AF_UNSPEC;
	if( outIPv6 ) outIPv6->sa.sa_family = AF_UNSPEC;
	
	err = getifaddrs( &iflist );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
	
	needIPv4 = outIPv4 ? true : false;
	needIPv6 = outIPv6 ? true : false;
	for( ifa = iflist; ifa && ( needIPv4 || needIPv6 ); ifa = ifa->ifa_next )
	{
		if( !( ifa->ifa_flags & IFF_UP ) )		continue; // Skip inactive.
		if( ifa->ifa_flags & IFF_LOOPBACK )		continue; // Skip loopback.
		if( ifa->ifa_flags & IFF_POINTOPOINT )	continue; // Skip PPP/tunnels.
		if( !ifa->ifa_addr )					continue; // Skip no addr.
		
		if( needIPv4 && ( ifa->ifa_addr->sa_family == AF_INET ) )
		{
			SockAddrCopy( ifa->ifa_addr, outIPv4 );
			needIPv4 = false;
		}
		if( needIPv6 && ( ifa->ifa_addr->sa_family == AF_INET6 ) )
		{
			SockAddrCopy( ifa->ifa_addr, outIPv6 );
			needIPv6 = false;
		}
	}
	
exit:
	if( iflist ) freeifaddrs( iflist );
	return( err );
}
#endif

//===========================================================================================================================
//	GetLocalHostName
//===========================================================================================================================

#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
OSStatus	GetLocalHostName( char *inBuf, size_t inMaxLen )
{
	OSStatus		err;
	CFStringRef		tempCFStr;
	Boolean			good;
	size_t			len;
	
	tempCFStr = SCDynamicStoreCopyLocalHostName( NULL );
	require_action( tempCFStr, exit, err = kUnknownErr );
	
	good = CFStringGetCString( tempCFStr, inBuf, (CFIndex) inMaxLen, kCFStringEncodingUTF8 );
	CFRelease( tempCFStr );
	require_action( good, exit, err = kUnknownErr );
	
	len = strlcat( inBuf, ".local", inMaxLen );
	require_action( len < inMaxLen, exit, err = kSizeErr );
	err = kNoErr;
	
exit:
	return( err );
}
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

//===========================================================================================================================
//	SetInterfaceMACAddress
//===========================================================================================================================

#if( APPLE_HAVE_ROUTING_SUPPORT )

#if( TARGET_OS_NETBSD )
OSStatus	SetInterfaceMACAddress( const char *inInterfaceName, const uint8_t *inMACAddress )
{
	OSStatus					err;
	int							sock;
	char						ifrBuf[ sizeof( struct ifreq ) + sizeof( struct sockaddr_dl ) ]; // 12 bytes bigger than necessary...
	struct ifreq *				ifr;
	struct sockaddr_dl *		sdl;
	
	sock = socket( PF_INET, SOCK_DGRAM, 0 );
	err = map_socket_creation_errno( sock );
	require_noerr( err, exit );
	
	memset( ifrBuf, 0, sizeof( ifrBuf ) );
	ifr = (struct ifreq *) ifrBuf;
	strncpy( ifr->ifr_name, inInterfaceName, sizeof( ifr->ifr_name ) );
	
	sdl = (struct sockaddr_dl *) &ifr->ifr_addr;
	sdl->sdl_len	= sizeof( *sdl );
	sdl->sdl_family	= AF_LINK;
	sdl->sdl_type	= IFT_ETHER;
	sdl->sdl_nlen	= 0;
	sdl->sdl_alen	= ETHER_ADDR_LEN;
	memcpy( LLADDR( sdl ), inMACAddress, ETHER_ADDR_LEN );
	
	err = ioctl( sock, SIOCSIFADDR, ifr );
	err = map_socket_creation_errno( sock );
	close( sock );
	dlog( kLogLevelInfo, "Set MAC Address for %s to %.6a returned %d\n", inInterfaceName, inMACAddress, err );
	require_noerr( err, exit );
	
exit:
	return( err );
}
#endif

#if( TARGET_OS_DARWIN )
OSStatus	SetInterfaceMACAddress( const char *inInterfaceName, const uint8_t *inMACAddress )
{
	OSStatus			err;
	int					sock;
	char				ifrBuf[ sizeof( struct ifreq ) + sizeof( struct sockaddr_dl ) ]; // 12 bytes bigger than necessary...
	struct ifreq *		ifr;
	struct ifreq		ifflags;
	short				flags;
	
	sock = socket( PF_INET, SOCK_DGRAM, 0 );
	err = map_socket_creation_errno( sock );
	require_noerr( err, exit );
	
	memset( ifrBuf, 0, sizeof( ifrBuf ) );
	ifr = (struct ifreq *) ifrBuf;
	strncpy( ifr->ifr_name, inInterfaceName, sizeof( ifr->ifr_name ) );
	
	/* Note that this only works for some like bridges & bonds */
	ifr->ifr_addr.sa_len = ETHER_ADDR_LEN;
	memcpy( ifr->ifr_addr.sa_data, inMACAddress, ETHER_ADDR_LEN );

	/* xnu doesn't allow resetting the MAC if the i/face isn't UP
	 * Seems to me like the time to accept a change is when it's DOWN, but hey...
	 */
	memset( &ifflags, 0, sizeof( ifflags ) );
	strncpy( ifflags.ifr_name, inInterfaceName, sizeof( ifflags.ifr_name ) );
	err = ioctl( sock, SIOCGIFFLAGS, &ifflags );
	if (err) {
		dlog( kLogLevelError, "Set MAC Address failed to get flags for %s (%d)\n",
			inInterfaceName, errno);
		ifflags.ifr_flags = IFF_UP;
	}
	flags = ifflags.ifr_flags;
	if ((flags & IFF_UP) == 0) {
		ifflags.ifr_flags |= IFF_UP;
		if (ioctl( sock, SIOCSIFFLAGS, &ifflags )) {
			dlog( kLogLevelError,
				"Set MAC Address failed to set (1) flags for %s (%d)\n",
				inInterfaceName, errno);
			flags |= IFF_UP;	/* avoid trying to reset if we fail to set */
		}
	}

	err = ioctl( sock, SIOCSIFLLADDR, ifr );

	if ((flags & IFF_UP) == 0) {
		ifflags.ifr_flags = flags;
		if (ioctl( sock, SIOCSIFFLAGS, &ifflags ) < 0) {
			dlog( kLogLevelError,
				"Set MAC Address failed to set (2) flags for %s (%d)\n",
				inInterfaceName, errno);
		}
	}
	
	dlog( kLogLevelInfo, "Set MAC Address for %s to %.6a returned %d\n", inInterfaceName, inMACAddress, err);

	close( sock );
	require_action( err >= 0, exit, err = kUnknownErr );

	err = kNoErr;
	
exit:
	return err;
}
#endif
#endif // APPLE_HAVE_ROUTING_SUPPORT

#if( TARGET_OS_BSD )
//===========================================================================================================================
//	GetPeerMACAddress
//===========================================================================================================================

#define ROUNDUP( x, len )	( ( (x) & ( (len) - 1 ) ) ? ( 1 + ( (x) | ( (len) - 1 ) ) ) : (x) )
#define NEXT_SA( x )		( ( (caddr_t)(x) ) + ( (x)->sa_len ? ROUNDUP( (x)->sa_len, sizeof( uint32_t ) ) : sizeof( uint32_t ) ) )

OSStatus	GetPeerMACAddress( const void *inPeerAddr, uint8_t outMAC[ 6 ] )
{
	const sockaddr_ip * const		peerSA = (const sockaddr_ip *) inPeerAddr;
	OSStatus						err;
	uint8_t *						buf = NULL;
	int								i;
	int								mib[ 6 ];
	size_t							neededSize;
	const uint8_t *					src;
	const uint8_t *					end;
	const struct rt_msghdr *		rtm;
	
	// Get the entire ARP/ND table. Retry on failures because the table may grow between calls.
	
	err = kUnknownErr; // Workarond clang bug where it thinks it's uninitialized: <radar:16204689>.
	neededSize = 0;
	for( i = 0; i < 100; ++i )
	{
		mib[ 0 ] = CTL_NET;
		mib[ 1 ] = PF_ROUTE;
		mib[ 2 ] = 0;
		mib[ 3 ] = peerSA->sa.sa_family;
		mib[ 4 ] = NET_RT_FLAGS;
		mib[ 5 ] = RTF_LLINFO;
		
		err = sysctl( mib, 6, NULL, &neededSize, NULL, 0 );
		err = map_noerr_errno( err );
		require_noerr( err, exit );
		require_action_quiet( neededSize > 0, exit, err = kNotFoundErr );
		
		buf = (uint8_t *) malloc( neededSize );
		require_action( buf, exit, err = kNoMemoryErr );
		
		err = sysctl( mib, 6, buf, &neededSize, NULL, 0 );
		err = map_noerr_errno( err );
		if( !err ) break;
		
		free( buf );
		buf = NULL;
	}
	require_noerr( err, exit );
	
	// Search for the specified address.
	
	src = buf;
	end = src + neededSize;
	for( ; src < end; src += rtm->rtm_msglen )
	{
		struct sockaddr *			sa;
		struct sockaddr_dl *		sdl;
		
		rtm = (struct rt_msghdr *) src;
		sa  = (struct sockaddr *)( rtm + 1 );
		sdl = (struct sockaddr_dl *) NEXT_SA( sa );
		
		if( sdl->sdl_family != AF_LINK )				continue; // Skip if it's not a MAC address.
		if( sdl->sdl_alen != 6 )						continue; // Skip if it's not 6-byte MAC address.
		if( sdl->sdl_index == 0 )						continue; // Skip if there's no interface.
		if( SockAddrCompareAddr( sa, peerSA ) != 0 )	continue; // Skip if the IP address doesn't match.
		
		memcpy( outMAC, LLADDR( sdl ), sdl->sdl_alen );
		err = kNoErr;
		goto exit;
	}
	err = kNotFoundErr;
	
exit:
	FreeNullSafe( buf );
	return( err );
}
#endif // TARGET_OS_BSD

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
	uint32_t		u32;
	int				i;
	
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
	
	// ConvertToIPv6
	
#if( defined( AF_INET6 ) )
	err = StringToSockAddr( "12.106.32.254:1234", &sip, sizeof( sip ), &sipLen );
	require_noerr( err, exit );
	err = SockAddrConvertToIPv6( &sip, &sip );
	require_noerr( err, exit );
	err = SockAddrToString( &sip, kSockAddrStringFlagsNone, str );
	require_noerr( err, exit );
	require_action( strcmp( str, "[::ffff:12.106.32.254]:1234" ) == 0, exit, err = kResponseErr );
	
	err = StringToSockAddr( "[fe80::5445:5245:444f]:5678", &sip, sizeof( sip ), &sipLen );
	require_noerr( err, exit );
	err = SockAddrConvertToIPv6( &sip, &sip );
	require_noerr( err, exit );
	err = SockAddrToString( &sip, kSockAddrStringFlagsNone, str );
	require_noerr( err, exit );
	require_action( strcmp( str, "[fe80::5445:5245:444f]:5678" ) == 0, exit, err = kResponseErr );
#endif
	
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
	
	// Addressing
	
	u32 = UINT32_C( 0xFFFFFFFF );
	for( i = 32; i >= 0; --i )
	{
		require_action( IPv4BitsToMask( i ) == u32, exit, err = kResponseErr );
		u32 <<= 1;
	}
	
	require_action( IsIPv4MartianAddress( htonl( 0x00000000 ) ), exit, err = -1 );
	require_action( IsIPv4MartianAddress( htonl( 0x00000001 ) ), exit, err = -1 );
	require_action( IsIPv4MartianAddress( htonl( 0x0a000001 ) ), exit, err = -1 );
	require_action( IsIPv4MartianAddress( htonl( 0x7f000001 ) ), exit, err = -1 );
	require_action( IsIPv4MartianAddress( htonl( 0x80000001 ) ), exit, err = -1 );
	require_action( IsIPv4MartianAddress( htonl( 0xac100001 ) ), exit, err = -1 );
	require_action( IsIPv4MartianAddress( htonl( 0xa9fe0001 ) ), exit, err = -1 );
	require_action( IsIPv4MartianAddress( htonl( 0xbfff0001 ) ), exit, err = -1 );
	require_action( IsIPv4MartianAddress( htonl( 0xc0000001 ) ), exit, err = -1 );
	require_action( IsIPv4MartianAddress( htonl( 0xc0a80001 ) ), exit, err = -1 );
	require_action( IsIPv4MartianAddress( htonl( 0xc6120001 ) ), exit, err = -1 );
	require_action( IsIPv4MartianAddress( htonl( 0xdfffff01 ) ), exit, err = -1 );
	require_action( IsIPv4MartianAddress( htonl( 0xe0010001 ) ), exit, err = -1 );
	require_action( IsIPv4MartianAddress( htonl( 0xe00000fb ) ), exit, err = -1 );
	require_action( IsIPv4MartianAddress( htonl( 0xf1000001 ) ), exit, err = -1 );
	require_action( IsIPv4MartianAddress( htonl( 0xffffffff ) ), exit, err = -1 );
	require_action( !IsIPv4MartianAddress( htonl( 0x11CE0102 ) ), exit, err = -1 ); // 17.206.1.2
	require_action( !IsIPv4MartianAddress( htonl( 0x08080808 ) ), exit, err = -1 ); // 8.8.8.8
	
	require_action(  IsIPv6MartianAddress( "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" ), exit, err = -1 );
	require_action(  !IsIPv6MartianAddressEx( "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 
		kMartianFlags_AllowUnspecified ), exit, err = -1 );
	require_action(  IsIPv6MartianAddress( "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\xff\xff" ), exit, err = -1 );
	require_action(  IsIPv6MartianAddress( "\x20\x02\xe0\x00\x00\x00\x00\x00\x00\x00\x12\x00\x00\x00\x00\x34" ), exit, err = -1 );
	require_action(  IsIPv6MartianAddress( "\xfe\x80\x00\x11\x22\x33\x44\x55\x00\x00\x00\x00\x00\x00\xaa\xbb" ), exit, err = -1 );
	require_action(  !IsIPv6MartianAddressEx( "\xfe\x80\x00\x11\x22\x33\x44\x55\x00\x00\x00\x00\x00\x00\xaa\xbb", 
		kMartianFlags_AllowLinkLocal ), exit, err = -1 );
	require_action(  IsIPv6MartianAddress( "\xff\x80\x00\x11\x22\x33\x44\x55\x00\x00\x00\x00\x00\x00\xaa\xbb" ), exit, err = -1 );
	require_action( !IsIPv6MartianAddress( "\x2c\x00\x00\x00\x85\xa3\x00\x00\x00\x00\x8a\x2e\x03\x70\x73\x34" ), exit, err = -1 );
	
	require_action(  IsGlobalIPv4Address( htonl( 0x11CE0102 ) ), exit, err = kResponseErr ); // 17.206.1.2
	require_action( !IsGlobalIPv4Address( htonl( 0xA9FE0102 ) ), exit, err = kResponseErr ); // Link-Local
	require_action( !IsGlobalIPv4Address( htonl( 0xC0A80C23 ) ), exit, err = kResponseErr ); // Private
	require_action( !IsGlobalIPv4Address( htonl( 0x7F000001 ) ), exit, err = kResponseErr ); // Loopback
	require_action( !IsGlobalIPv4Address( htonl( 0xE00000FB ) ), exit, err = kResponseErr ); // Multicast
	require_action( !IsGlobalIPv4Address( htonl( 0x00001000 ) ), exit, err = kResponseErr ); // Zero
	
	require_action( IsPrivateIPv4Address( htonl( 0xC0A80C23 ) ), exit, err = kResponseErr );
	require_action( !IsPrivateIPv4Address( htonl( 0x11CD167B ) ), exit, err = kResponseErr );
	
	require_action(  IsRoutableIPv4Address( htonl( 0x11CE0102 ) ), exit, err = kResponseErr );
	require_action( !IsRoutableIPv4Address( htonl( 0xA9FE0102 ) ), exit, err = kResponseErr );
	require_action( !IsRoutableIPv4Address( htonl( 0x00001000 ) ), exit, err = kResponseErr );
	
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
	
	require_action( CompareMACAddresses( "\x00\x11\x22\x33\x44\x55", "\x00\x11\x22\x33\x44\x56" )  < 0, exit, err = kResponseErr );
	require_action( CompareMACAddresses( "\x00\x11\x22\x33\x44\x56", "\x00\x11\x22\x33\x44\x55" )  > 0, exit, err = kResponseErr );
	require_action( CompareMACAddresses( "\x00\x11\x22\x33\x44\x55", "\x00\x11\x22\x33\x44\x55" ) == 0, exit, err = kResponseErr );
	
	require_action( CompareMACAddresses( "\x00\x11\x22\x33\x44\x55", "\x01\x11\x22\x33\x44\x55" )  < 0, exit, err = kResponseErr );
	require_action( CompareMACAddresses( "\x01\x11\x22\x33\x44\x55", "\x00\x11\x22\x33\x44\x55" )  > 0, exit, err = kResponseErr );
	require_action( CompareMACAddresses( "\x01\x11\x22\x33\x44\x55", "\x01\x11\x22\x33\x44\x55" ) == 0, exit, err = kResponseErr );
	
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
	
#if( APPLE_HAVE_ROUTING_SUPPORT )
{
	struct ifaddrs *		iaList;
	
	err = CreateUsableInterfaceList( NULL, AF_UNSPEC, &iaList );
	require_noerr( err, exit );
	
	ReleaseUsableInterfaceList( iaList );
	
	err = CreateUsableInterfaceList( "en0", AF_UNSPEC, &iaList );
	require_noerr( err, exit );
	
	ReleaseUsableInterfaceList( iaList );	
}
#endif
	
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
