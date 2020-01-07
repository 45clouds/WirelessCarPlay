/*
	File:    	NetUtils.h
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	410.8
	
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
	
	Copyright (C) 2006-2015 Apple Inc. All Rights Reserved.
*/

#ifndef	__NetUtils_h__
#define	__NetUtils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#if( TARGET_OS_WINDOWS )
	#include "NetUtilsWindows.h"
#endif

// Configuration

#if( !defined( NETUTILS_USE_DNS_SD_GETADDRINFO ) )
	#if( TARGET_OS_NETBSD )
		#define NETUTILS_USE_DNS_SD_GETADDRINFO		1
	#else
		#define NETUTILS_USE_DNS_SD_GETADDRINFO		0
	#endif
#endif

#if( !defined( NETUTILS_HAVE_GETADDRINFO ) )
	#if( NETUTILS_USE_DNS_SD_GETADDRINFO || TARGET_OS_POSIX || TARGET_OS_WINDOWS )
		#define NETUTILS_HAVE_GETADDRINFO		1
	#else
		#define NETUTILS_HAVE_GETADDRINFO		0
	#endif
#endif

// Includes

#if( TARGET_OS_BSD )
	#include <sys/event.h>
#endif


#if( TARGET_OS_POSIX )
	#include <netdb.h>
#endif

#if( TARGET_OS_POSIX )
	#include <sys/types.h>
	
	#include <arpa/inet.h>
	#include <fcntl.h>
	#include <ifaddrs.h>
	#include <sys/socket.h>
	#include <sys/uio.h>
#endif


#if( TARGET_OS_BSD )
	#define APPLE_HAVE_ROUTING_SUPPORT		1
#else
	#define APPLE_HAVE_ROUTING_SUPPORT		0
#endif

#if( APPLE_HAVE_ROUTING_SUPPORT )
	#include <net/route.h>
#endif

#if( TARGET_OS_LINUX )
	#include <sys/ioctl.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if( APPLE_HAVE_ROUTING_SUPPORT )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CreateUsableInterfaceList
	@abstract	Creates a linked list of ifaddr structures for all the usable IPv4 and IPv6 interfaces.
	@discussion	You must use ReleaseUsableInterfaceList rather than freeifaddrs to free the returned interface list.
*/
OSStatus	CreateUsableInterfaceList( const char *inInterfaceName, int inFamily, struct ifaddrs **outList );
void		ReleaseUsableInterfaceList( struct ifaddrs *inList );
OSStatus	ValidateInterfaceUsability( SocketRef inInfoSock, const struct ifaddrs *inIA );
#define		WaitForUsableInterfaces()		CreateUsableInterfaceList( NULL, AF_UNSPEC, NULL )
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	InterfaceRemoveAllAddrs
	@abstract	Removes IPv4 and IPv6 addresses for the specified interface.
*/
#if( TARGET_OS_POSIX )
	void	InterfaceRemoveAllAddrs( const char *ifname );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	DrainUDPSocket
	@abstract	Drains any packets from a UDP socket.
*/
OSStatus	DrainUDPSocket( SocketRef inSock, int inTimeoutSecs, int *outDrainedPackets );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HostIsPotentiallyReachable
	@abstract	Quick check if a host is potentially reachable with or without dialing a modem, etc.
*/
Boolean	HostIsPotentiallyReachable( const char *inHost, Boolean inAllowModemConnect );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetLoopbackInterfaceInfo
	@abstract	Gets the name or index of the loopback interface (e.g. "lo0").
	
	@param		inNameBuf	If non-NULL, receives interface name. Must be at least IF_NAMESIZE + 1 bytes.
	@param		inMaxLen	Max number of bytes to store in inNameBuf.
	@param		outIndex	If non-NULL, receives interface index.
*/
OSStatus	GetLoopbackInterfaceInfo( char *inNameBuf, size_t inMaxLen, uint32_t *outIndex );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetPrimaryMACAddress
	@abstract	Gets the 6-byte MAC address of the primary network interface.
	@result		Scalar version of the 6-byte MAC address.
*/
uint64_t	GetPrimaryMACAddress( uint8_t outMAC[ 6 ], OSStatus *outErr );

// Function to implement for platforms that don't have built-in support.
extern OSStatus	GetPrimaryMACAddressPlatform( uint8_t outMAC[ 6 ] );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetUDPOutgoingSockAddr
	@abstract	Determines the outgoing (source) sockaddr we'd use to send a UDP packet to the specified remote sockaddr.
	
	@param		inRemoteAddr		sockaddr of remote host.
	@param		outLocalAddr		Receives sockaddr we'd use as a source address when sending to "inRemoteAddr".
									This should be a sockaddr_storage so it's big enough to hold any sockaddr.
*/
OSStatus	GetUDPOutgoingSockAddr( const void *inRemoteAddr, void *outLocalAddr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	LocalHostIsOnAppleInternalNetwork
	@abstract	Check if the current computer can access the Apple internal network with minimal network activity.
*/
Boolean	LocalHostIsOnAppleInternalNetwork( void );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NetworkStackSupportsIPv4MappedIPv6Addresses
	@abstract	Return true if the network stack supports IPv4-mapped IPv6 addresses (i.e. IPv6 socket can handle IPv4 and IPv6).
*/
Boolean	NetworkStackSupportsIPv4MappedIPv6Addresses( void );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	OpenSelfConnectedLoopbackSocket
	@abstract	Opens a UDP socket bound to the loopback interface and connected to itself. Useful for thread communication.
*/
OSStatus	OpenSelfConnectedLoopbackSocket( SocketRef *outSock );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SendSelfConnectedLoopbackMessage
	@abstract	Sends a message to a socket set up with OpenSelfConnectedLoopbackSocket().
*/
OSStatus	SendSelfConnectedLoopbackMessage( SocketRef inSock, const void *inMsg, size_t inLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SendUDPQuitPacket
	@abstract	Sends an empty UDP packet to the IPv4 or IPv6 loopback address.
	@discussion	Used by internal code to send a quit packet to gracefully exit a thread.
*/
OSStatus	SendUDPQuitPacket( int inPort, SocketRef inSockV4, SocketRef inSockV6 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TCPServerSocketPairOpen / UDPServerSocketPairOpen / ServerSocketPairOpen
	@abstract	Opens a pair of TCP or UDP server sockets, one for IPv4 and one for IPv6 bound to the same port number.
	@discussion
	
	Not all IP stacks support IPv4-mapped IPv6 addresses and even when they do support them, you can run into
	situations where IPv6 is disabled on one interface, but not another, causing problems when relying solely
	on IPv6 sockets to support both IPv4 and IPv6. Using a separate socket for each type of interface allows 
	code to work even if IPv4 or IPv6 is disabled entirely or disabled on certain interfaces.
	
	Note: even if kNoErr is returned, either outSockV4 or outSockV6 may be kInvalidSocketRef (but not both)
	if that socket family is not supported (e.g. IPv6 not supported by the OS). Callers must check before using.
*/
#define kSocketPort_Auto		0 // Let the network stack assign a dynamic port number.

#define TCPServerSocketPairOpen( IN_PORT, OUT_PORT, RCV_BUF_SIZE, OUT_SOCKV4, OUT_SOCKV6 )	\
	ServerSocketPairOpen( SOCK_STREAM, IPPROTO_TCP, (IN_PORT), (OUT_PORT), (RCV_BUF_SIZE), (OUT_SOCKV4), (OUT_SOCKV6) )

#define UDPServerSocketPairOpen( IN_PORT, OUT_PORT, OUT_SOCKV4, OUT_SOCKV6 )	\
	ServerSocketPairOpen( SOCK_DGRAM, IPPROTO_UDP, (IN_PORT), (OUT_PORT), kSocketBufferSize_DontSet,	\
		(OUT_SOCKV4), (OUT_SOCKV6) )

OSStatus
	ServerSocketPairOpen( 
		int			inType, 
		int			inProtocol, 
		int			inPort, 
		int *		outPort, 
		int			inRcvBufSize, 
		SocketRef *	outSockV4, 
		SocketRef *	outSockV6 );

OSStatus
	ServerSocketOpen( 
		int			inFamily, 
		int			inType, 
		int			inProtocol, 
		int			inPort, 
		int *		outPort, 
		int			inRcvBufSize, 
		SocketRef *	outSock );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	UDPClientSocketOpen
	@abstract	Opens a UDP socket for use by a client.
	
	@param		inFamily		IPv4 or IPv6 family of socket to create. May be AF_UNSPEC to use the same family as inPeerAddr.
	@param		inPeerAddr		Address of peer to connect socket to. May be NULL to not connect it to a peer.
	@param		inPeerPort		Port number to override inPeerAddr. Use 0 to not override the port number.
	@param		inListenPort	Port number to bind the socket to for listening.
								< -1: Try negated port, but if in use, let the network stack choose a port.
								  -1: Don't bind the socket for listening.
								   0: Let the network stack choose a port.
								 > 0: Try to bind to the specified port and fail if that port is in use.
	@param		outListenPort	Receives port number that was bound. May be NULL.
	@param		outSock			Receives created socket. If function returns kNoErr, caller must close this socket.
*/
OSStatus
	UDPClientSocketOpen( 
		int				inFamily, 
		const void *	inPeerAddr, 
		int				inPeerPort, 
		int				inListenPort, 
		int *			outListenPort, 
		SocketRef *		outSock );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	UpdateIOVec
	@abstract	Updates an iovec array to account for the specified amount of data.
	@result		kNoErr if the iovec array has been completed used. EWOULDBLOCK if there's still more to process.
	@discussion
	
	This is intended to be called after one of the iovec functions, such as writev to set it up for the next
	call (if needed). Note: ioArray and ioCount are not updated if kNoErr is returned. Example usage:
	
	iovec_t			iov[ 3 ];
	iovec_t *		iop;
	int				ion;
	ssize_t			n;
	
	... fill in "iov"
	iop = iov;
	ion = 3;
	
	for( ;; )
	{
		n = writev( fd, iop, ion );
		if( n > 0 )
		{
			err = UpdateIOVec( &iop, &ion, (size_t) n );
			if( err == kNoErr ) break;
		}
		else
		{
			... handle error
		}
	}
*/
OSStatus	UpdateIOVec( iovec_t **ioArray, int *ioCount, size_t inAmount );

#if( TARGET_OS_WINDOWS )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	writev
	@abstract	Windows version of the Unix writev function.
*/
ssize_t	writev( SocketRef inSock, const iovec_t *inArray, int inCount );
#endif

#if( NETUTILS_USE_DNS_SD_GETADDRINFO )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	getaddrinfo_dnssd
	@abstract	getaddrinfo that uses DNS-SD so it works with Bonjour names on NetBSD.
*/
int
	getaddrinfo_dnssd( 
		const char *			inNode, 
		const char *			inService, 
		const struct addrinfo *	inHints, 
		struct addrinfo **		outResults );
void	freeaddrinfo_dnssd( struct addrinfo *inAddrs );
#endif

#if 0
#pragma mark == NetSocket ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		NetSocket
	@abstract	Socket API that is fully cancelable.
*/
typedef struct NetSocket *		NetSocketRef;

typedef void ( *NetSocket_SetOptionsCallBackPtr )( NetSocketRef inSock, SocketRef inNativeSock, void *inContext );

typedef OSStatus
	( *NetSocket_ReadFunc )( 
		NetSocketRef	inSock, 
		size_t			inMinSize, 
		size_t			inMaxSize, 
		void *			inBuffer, 
		size_t *		outSize, 
		int				inFlags, 
		int32_t			inTimeoutSecs );

typedef OSStatus	( *NetSocket_WriteFunc )( NetSocketRef inSock, const void *inBuffer, size_t inSize, int32_t inTimeoutSecs );
typedef OSStatus	( *NetSocket_WriteVFunc )( NetSocketRef inSock, iovec_t *inArray, int inCount, int32_t inTimeoutSecs );
#if( TARGET_HAVE_FDREF )
	typedef OSStatus
		( *NetSocket_WriteFileFunc )( 
			NetSocketRef	inSock, 
			iovec_t *		inHeaderArray, 
			int				inHeaderCount, 
			iovec_t *		inTrailerArray, 
			int				inTrailerCount, 
			FDRef			inFileFD, 
			int64_t			inFileOffset, 
			int64_t			inFileAmount, 
			int32_t			inTimeoutSecs );
#endif

typedef void		( *NetSocket_FreeFunc )( NetSocketRef inSock );

// NetSocket

#define kNetSocketMagic			0x6E736F63 // nsoc
#define kNetSocketMagicBad		0x4E534F43 // NSOC

struct NetSocket
{
	uint32_t					magic;			// Must be kNetSocketMagic 'NSoc' if valid.
	SocketRef					nativeSock;		// Actual socket used for networking.
	Boolean						canceled;		// true if any cancel occurred. Everything fails after a cancel.
#if( TARGET_OS_POSIX )
	int							sendCancel;		// Write side of cancel pipe. Write to this to cancel socket operations.
	int							recvCancel;		// Read side of cancel pipe. select/read this to detect cancels.
#elif( TARGET_OS_WINDOWS )
	HANDLE						sockEvent;		// Event for async notification of events on the socket.
	HANDLE						cancelEvent;	// Event handle to signal and wait for cancels.
#else
	SocketRef					cancelSock;		// Read/write UDP socket for canceling.
#endif
	
	NetSocket_ReadFunc			readFunc;		// Function for reading data. Can be overridden by subclasses.
	NetSocket_WriteFunc			writeFunc;		// Function for writing data. Can be overridden by subclasses.
	NetSocket_WriteVFunc		writeVFunc;		// Function for writing via iovec's. Can be overridden by subclasses.
#if( TARGET_HAVE_FDREF )
	NetSocket_WriteFileFunc		writeFileFunc;	// Function for writing a file. Can be overridden by subclasses.
#endif
	NetSocket_FreeFunc			freeFunc;		// Function for subclasses to clean up. May be NULL.
	
	char *						leftoverPtr;	// Optional ptr for leftover body data when reading headers, etc.
	char *						leftoverEnd;	// Optional end of leftover body data when reading headers, etc.
	
	uint8_t *					readBuffer;		// Buffer for large reads.
	size_t						readBufLen;		// Number of bytes 'readBuffer' can hold.
	
	void *						transportCtx;	// Context for a custom transport. Not touched by NetSocket library.
	int32_t						timeoutSecs;	// Timeout specified by the most recent high-level NetSocket call.
	
	LogCategory *				ucat;			// Optional category to log to.
};

OSStatus	NetSocket_Create( NetSocketRef *outSock );
OSStatus	NetSocket_CreateWithNative( NetSocketRef *outSock, SocketRef inSock );
OSStatus	NetSocket_Delete( NetSocketRef inSock );

#define NetSocket_Forget( X ) \
	do \
	{ \
		if( *(X) ) \
		{ \
			OSStatus		NetSocket_ForgetErr; \
			\
			DEBUG_USE_ONLY( NetSocket_ForgetErr ); \
			\
			NetSocket_ForgetErr = NetSocket_Delete( *(X) );	 \
			check_noerr( NetSocket_ForgetErr ); \
			*(X) = NULL; \
		} \
		\
	}	while( 0 )

OSStatus	NetSocket_Cancel( NetSocketRef inSock );
OSStatus	NetSocket_Reset( NetSocketRef inSock );
SocketRef	NetSocket_GetNative( NetSocketRef inSock );

// Connections

typedef uint32_t	NetSocketConnectFlags;

#define kNetSocketConnect_NoFlags		0
#define kNetSocketConnect_ForcePort		( 1 << 0 ) //! Force the passed in port to be used instead a port in the string.

OSStatus	NetSocket_TCPConnect( NetSocketRef inSock, const char *inHostList, int inDefaultPort, int32_t inTimeoutSecs );
OSStatus
	NetSocket_TCPConnectEx( 
		NetSocketRef					inSock, 
		NetSocketConnectFlags			inFlags, 
		const char *					inHostList, 
		int								inDefaultPort, 
		int32_t							inTimeoutSecs, 
		NetSocket_SetOptionsCallBackPtr	inCallBack, 
		void *							inContext );
OSStatus	NetSocket_Disconnect( NetSocketRef inSock, int32_t inTimeoutSecs );

// Reading

#define NetSocket_Read( NETSOCK, MIN_SIZE, MAX_SIZE, BUFFER, OUT_SIZE, TIMEOUT_SECS )	\
	(NETSOCK)->readFunc( (NETSOCK), (MIN_SIZE), (MAX_SIZE), (BUFFER), (OUT_SIZE), 0, (TIMEOUT_SECS) )

#define NetSocket_Peek( NETSOCK, MIN_SIZE, MAX_SIZE, BUFFER, OUT_SIZE, TIMEOUT_SECS )	\
	(NETSOCK)->readFunc( (NETSOCK), (MIN_SIZE), (MAX_SIZE), (BUFFER), (OUT_SIZE), MSG_PEEK, (TIMEOUT_SECS) )

OSStatus
	NetSocket_ReadInternal( 
		NetSocketRef	inSock, 
		size_t			inMinSize, 
		size_t			inMaxSize, 
		void *			inBuffer, 
		size_t *		outSize, 
		int				inFlags,
		int32_t			inTimeoutSecs );

#if( TARGET_HAVE_FDREF )
	OSStatus
		NetSocket_ReadFile( 
			NetSocketRef	inSock, 
			int64_t			inAmount, 
			FDRef			inFileFD, 
			int64_t			inFileOffset, 
			int32_t			inTimeoutSecs );
#endif

// Writing

#define NetSocket_Write( NETSOCK, BUFFER, SIZE, TIMEOUT_SECS )	\
	(NETSOCK)->writeFunc( (NETSOCK), (BUFFER), (SIZE), (TIMEOUT_SECS) )

#define NetSocket_WriteV( NETSOCK, IOVEC_ARRAY, IOVEC_COUNT, TIMEOUT_SECS )	\
	(NETSOCK)->writeVFunc( (NETSOCK), (IOVEC_ARRAY), (IOVEC_COUNT), (TIMEOUT_SECS) )

#define NetSocket_WriteFile( NETSOCK, 					\
	HEADER_IOVEC_ARRAY,  HEADER_IOVEC_COUNT, 			\
	TRAILER_IOVEC_ARRAY, TRAILER_IOVEC_COUNT,			\
	FILE_FD, FILE_OFFSET, FILE_AMOUNT, 					\
	TIMEOUT_SECS )										\
														\
	(NETSOCK)->writeFileFunc( (NETSOCK), 				\
		(HEADER_IOVEC_ARRAY),  (HEADER_IOVEC_COUNT), 	\
		(TRAILER_IOVEC_ARRAY), (TRAILER_IOVEC_COUNT),	\
		(FILE_FD), (FILE_OFFSET), (FILE_AMOUNT), 		\
		TIMEOUT_SECS )

OSStatus	NetSocket_WriteInternal( NetSocketRef inSock, const void *inBuffer, size_t inSize, int32_t inTimeoutSecs );
#if( TARGET_OS_POSIX || TARGET_OS_WINDOWS )
	OSStatus	NetSocket_WriteVInternal( NetSocketRef inSock, iovec_t *inArray, int inCount, int32_t inTimeoutSecs );
#endif
OSStatus	NetSocket_WriteVSlow( NetSocketRef inSock, iovec_t *inArray, int inCount, int32_t inTimeoutSecs );
#if( TARGET_HAVE_FDREF )
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
			int32_t			inTimeoutSecs );
	
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
			int32_t			inTimeoutSecs );
#endif

// Waiting

typedef enum
{
	kNetSocketWaitType_Read		= 0, 
	kNetSocketWaitType_Write	= 1, 
	kNetSocketWaitType_Connect	= 2
	
}	NetSocketWaitType;

OSStatus
	NetSocket_Wait( 
		NetSocketRef		inSock, 
		SocketRef			inNativeSock, 
		NetSocketWaitType	inWaitType, 
		int32_t				inTimeoutSecs );

#if( !EXCLUDE_UNIT_TESTS )
	OSStatus	NetSocket_Test( void );
#endif

#if 0
#pragma mark -
#pragma mark == NetTransportDelegate ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		NetTransportDelegate
	@abstract	Non-blocking transport abstraction for reading and writing data.
*/
typedef OSStatus	( *NetTransportInitialize_f )( SocketRef inSock, void *inContext );
typedef void		( *NetTransportFinalize_f )( void *inContext );
typedef OSStatus	( *NetTransportRead_f )( void *inBuffer, size_t inMaxLen, size_t *outLen, void *inContext );
typedef OSStatus	( *NetTransportWriteV_f )( iovec_t **ioArray, int *ioCount, void *inContext );

typedef struct
{
	void *							context;
	NetTransportInitialize_f		initialize_f;
	NetTransportFinalize_f			finalize_f;
	NetTransportRead_f				read_f;
	NetTransportWriteV_f			writev_f;
	
}	NetTransportDelegate;

#define NetTransportDelegateInit( PTR )	memset( (PTR), 0, sizeof( NetTransportDelegate ) )

#if 0
#pragma mark -
#pragma mark == SocketUtils ==
#endif

//===========================================================================================================================
//	SocketUtils
//===========================================================================================================================

#define	kHostStringMaxSize		272	//! 255 character host name + ':' + 5 digit port number + Null + slop.

// Dynamic IP Port Numbers. Defined by IANA in <http://www.iana.org/assignments/port-numbers> as of 2005-05-09.

#define	kDynamicIPPortMin		49152
#define	kDynamicIPPortMax		65535

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TCPConnect
	@abstract	Connects to a host.
	
	@param		inHostList			Host to connect to. May be a comma-separate list of hosts (e.g. "10.0.1.1,192.168.0.1").
	@param		inDefaultService	Default service (i.e. port number) to use if not specified as part of the host.
	@param		inSeconds			Timeout in seconds.
	@param		outSock				Receives connected socket on success.
*/
OSStatus	TCPConnect( const char *inHostList, const char *inDefaultService, int inSeconds, SocketRef *outSock );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketAccept
	@abstract	Accepts a connection on a socket with a timeout.
	
	@param		inSock		Socket to accept the connection on.
	@param		inSeconds	Number of seconds to wait for the connection. Use -1 to wait forever.
	@param		outSock		Receives the socket for the new connection, if successful.
	@param		outAddr		Receives sockaddr of connected peer. May be NULL.
*/
OSStatus	SocketAccept( SocketRef inSock, int inSeconds, SocketRef *outSock, sockaddr_ip *outAddr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketConnect
	@abstract	Connects a socket with a timeout.
	
	@param		inSock			Socket to connect.
	@param		inSockAddr		sockaddr of the destination to connect to.
	@param		inSeconds		Number of seconds to wait for a successful connection,
*/
OSStatus	SocketConnect( SocketRef inSock, const void *inSockAddr, int inSeconds );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketCloseGracefully
	@abstract	Waits for the peer to close its end of the connection then closes the socket.
	
	@param		inSock			Socket to close.
	@param		inTimeoutSecs	Max seconds to wait for the peer to close its connection before closing the socket.
*/
OSStatus	SocketCloseGracefully( SocketRef inSock, int inTimeoutSecs );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketRecvFrom
	@abstract	Receives a UDP packet.
	@discussion
	
	To get receive ticks, you must call SocketSetPacketTimestamps when the socket is created.
	To get the receiving interface, you must set the IPPROTO_IP / IP_RECVIF socket option to 1.
*/
OSStatus
	SocketRecvFrom( 
		SocketRef			inSock, 
		void *				inBuf, 
		size_t				inMaxLen, 
		size_t *			outLen, 
		void *				outFrom,		// May be NULL.
		size_t				inFromMaxLen,	// May be 0.
		size_t *			outFromLen,		// May be NULL.
		uint64_t *			outTicks,		// May be NULL.
		uint32_t *			outIfIndex, 	// May be NULL.
		char *				outIfName );	// May be NULL.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketReadAll / SocketWriteAll
	@abstract	Blocks until all the data is read or written (or the connection ends).
*/
OSStatus	SocketReadAll( SocketRef inSock, void *inData, size_t inSize );
OSStatus	SocketWriteAll( SocketRef inSock, const void *inData, size_t inSize, int32_t inTimeoutSecs );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketReadData
	@abstract	Reads data into the specified buffer in a non-blocking manner.
	@discussion
	
	This may need to be called multiple times to read all the data. Callers should initialize *ioOffset to 0 and 
	pass in the total number of bytes to read then call this function each time the socket is readable until 
	it returns one of the following results:
	
	kNoErr:
		All the data was read successfully.
	
	EWOULDBLOCK:
		There was not enough data immediately available in the socket buffer to completely read the data.
		The caller should call this function again when more data is available (e.g. select says it's readable).
	
	Any other error:
		The data could not be read because of an error. The socket should be closed.
*/
OSStatus	SocketReadData( SocketRef inSock, void *inBuffer, size_t inSize, size_t *ioOffset );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketWriteData
	@abstract	Writes data gathered from the specified iovec array in a non-blocking manner.
	@discussion
	
	This may need to be called multiple times to write all the data. Callers should initialize ioArray to point to the 
	beginning of an iovec array that has been filled in with the data to write. ioCount should be initialized to the 
	initial number of items in the array pointed to by ioArray. This function will update ioArray and ioCount as needed 
	for subsequent calls. This function should be called each time the socket is writable until it returns one of the 
	following results:
	
	kNoErr:
		All the data was written successfully.
	
	EWOULDBLOCK:
		All the data could not be sent immediately. All the parameters have been updated for the next call. 
		Call this function again the when the socket becomes writable.
	
	Any other error:
		The data could not be read because of an error. The socket should be closed.
*/
OSStatus	SocketWriteData( SocketRef inSock, iovec_t **ioArray, int *ioCount );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketSendFile
	@abstract	Zero-copy way to send file data across the network with optional header and/or trailer data.
	@discussion
	
	Here's the steps needed before the first call to this function:
	
	1.	If you need to send header data, initialize an iovec array with the ptr/len of the header data.
		Assign an iovec * with the address of the iovec structure and pass its address in "ioHeaderArray".
		Assign an int the number of iovec structures in the iovec array and pass its address in "ioHeaderCount".
		These fields will be updated if needed for any subsequent calls to this function.
		
		If you don't need to send header data, pass NULL for ioHeaderArray and NULL for ioHeaderCount.
	
	2.	If you need to send trailer data, initialize an iovec array with the ptr/len of the trailer data.
		Assign an iovec * with the address of the iovec structure and pass its address in "ioTrailerArray".
		Assign an int the number of iovec structures in the iovec array and pass its address in "ioTrailerCount".
		These fields will be updated if needed for any subsequent calls to this function.
		
		If you don't need to send trailer data, pass NULL for ioTrailerArray and NULL for ioTrailerCount.
	
	3.	Assign an int64_t with the offset in the file to send.
		Assign an int64_t with the number of bytes file data to send.
	
	Then call this function and it will return one of the following error codes:
	
	kNoErr:
		All the data was read successfully.
	
	EWOULDBLOCK:
		All the data could not be sent immediately. Al the parameters have been updated for the next call. 
		Call this function again the when the socket becomes writable.
	
	Any other error:
		The data could not be read because of an error. The socket should be closed.
	
	Example:
	
	iovec_t			headerv[ 1 ];
	iovec_t *		headerp;
	int				headern;
	iovec_t			trailerv[ 1 ];
	iovec_t *		trailerp;
	int				trailern;
	int64_t			fileOffset;
	int64_t			fileAmount;
	
	headerv[ 0 ].iov_base = "abcd";
	headerv[ 0 ].iov_len  = 4;
	headerp = headerv;
	headern = 1;
	
	trailerv[ 0 ].iov_base = "xyz";
	trailerv[ 0 ].iov_len  = 3;
	trailerp = trailerv;
	trailern = 1;
	
	fileOffset = 20; // Start send from 20 bytes into the file.
	fileAmount = 50; // Send 50 bytes from offset 20 (bytes 20-69 inclusive).
	
	for( ;; )
	{
		err = SocketSendFile( sock, &headerp, &headern, &trailerp, &trailern, fileFD, &fileOffset, &fileAmount );
		if( err == kNoErr )			break;
		if( err == EWOULDBLOCK )	err = kNoErr;
		if( err == EPIPE )			goto exit;
		require_noerr( err, exit );
		
		... select, kevent, WaitForMultipleObjects or whatever to wait until socket becomes writable again.
	}
*/
#if( TARGET_OS_MACOSX )
	#define TARGET_HAS_SENDFILE		1
	
	OSStatus
		SocketSendFile( 
			SocketRef	inSock, 
			iovec_t **	ioHeaderArray,	// May be NULL.
			int *		ioHeaderCount, 	// May be NULL.
			iovec_t **	ioTrailerArray,	// May be NULL.
			int *		ioTrailerCount,	// May be NULL.
			FDRef		inFileFD, 
			int64_t *	ioFileOffset, 	// Initialize to offset then maintain across calls.
			int64_t *	ioFileAmount );	// Initialize to amount then maintain across calls.
#else
	#define TARGET_HAS_SENDFILE		0
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketTransportRead / SocketTransportWriteV
	@abstract	Transport read/write functions that work with a socket as its context.
*/
OSStatus	SocketTransportRead( void *inBuffer, size_t inMaxLen, size_t *outLen, void *inContext );
OSStatus	SocketTransportWriteV( iovec_t **ioArray, int *ioCount, void *inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketSetBoundInterface
	@abstract	Sets the interface to use for outgoing connections.
*/
OSStatus	SocketSetBoundInterface( SocketRef inSock, int inFamily, uint32_t inIfIndex );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketSetBufferSize
	@abstract	Set the socket's buffer size (SO_RCVBUF or SO_SNDBUF).
	
	@param		inSock		Socket to set the buffer size of.
	@param		inWhich		Which buffer to change. Must be either SO_RCVBUF (receive) or SO_SNDBUF (send).
	@param		inSize		Size  >  0, is an absolute size to set the socket buffer to.
							Size ==  0, means maximize the socket buffer size (big as the kernel will allow).
							Size == -1, means don't set the socket buffer size (useful for callers with a size input).
							Size  < -1, means maximize the socket buffer size, up to the negated size.
*/
#define kSocketBufferSize_DontSet				-1
#define kSocketBufferSize_Max					0
#define SocketBufferSize_UpToLimit( LIMIT )		-(LIMIT)

int			SocketGetBufferSize( SocketRef inSock, int inWhich, OSStatus *outErr );
OSStatus	SocketSetBufferSize( SocketRef inSock, int inWhich, int inSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketIsDefunct
	@abstract	Returns true if the socket is defunct.
	@discussion	On iOS, if you lock the device/etc, a socket can become defunct and you have to re-create it.
*/
Boolean	SocketIsDefunct( SocketRef inSock );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketGetFamily
	@abstract	Gets the address family of a socket.
*/
int	SocketGetFamily( SocketRef inSock, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketGetInterfaceInfo
	@abstract	Gets the interface name and/or interface index for a connected socket.
	
	@param		inSock				Connected socket. May be kInvalidSocketRef if "inIfName" specifies the interface to use.
	@param		inIfName			Interface to look up. Only used if inSock is kInvalidSocketRef.
	@param		outIfName			Optionally receives interface name. Must be at least IF_NAMESIZE + 1 bytes.
	@param		outIfIndex			Optionally receives interface index.
	@param		outMACAddress		Optionally receives interface MAC address.
	@param		outMedia			Optionally receives media options (see if_media.h for constants).
	@param		outFlags			Optionally receives interface flags (see net/if.h for constants).
	@param		outExtendedFlags	Optionally receives extended interface flags (see net/if.h for constants).
	@param		outOtherFlags		Optionally receives other flags defined by this function. See kNetInterfaceFlag_*.
	@param		outTransportType	Optionally receives transport type of interface.
*/
#define kNetInterfaceFlag_Inactive		( 1 << 0 ) // Interface is not attached to a working network.

typedef uint32_t		NetTransportType;
#define kNetTransportType_Undefined		0
#define kNetTransportType_Ethernet		( 1 << 0 )
#define kNetTransportType_WiFi			( 1 << 1 ) // Infrastructure WiFi
#define kNetTransportType_USB			( 1 << 3 )
#define kNetTransportType_DirectLink	( 1 << 4 ) // Point-to-point wired link (not WiFi Direct).
#define kNetTransportType_BTLE			( 1 << 5 ) // Bluetooth Low Energy
#define kNetTransportType_WiFiDirect	( 1 << 6 ) // Point-to-point WiFi Direct link.

#define kNetTransportType_AnyInfra		( kNetTransportType_Ethernet | kNetTransportType_WiFi )

#define NetTransportTypeIsP2P( X )		( (X) == kNetTransportType_WiFiDirect )

#define NetTransportTypeIsWiFi( X ) ( \
	( (X) == kNetTransportType_WiFi ) || \
	( (X) == kNetTransportType_WiFiDirect ) )

#define NetTransportTypeIsWired( X ) ( \
	( (X) == kNetTransportType_Ethernet ) || \
	( (X) == kNetTransportType_USB ) || \
	( (X) == kNetTransportType_DirectLink ) )

#define NetTransportTypeIsWireless( X ) ( \
	( (X) == kNetTransportType_WiFi ) || \
	( (X) == kNetTransportType_WiFiDirect ) || \
	( (X) == kNetTransportType_BTLE ) )

#define NetTransportTypeToString( X ) ( \
	( (X) == kNetTransportType_Ethernet )	? "Enet"	: \
	( (X) == kNetTransportType_WiFi )		? "WiFi"	: \
	( (X) == kNetTransportType_WiFiDirect )	? "WFD"		: \
	( (X) == kNetTransportType_USB )		? "USB"		: \
	( (X) == kNetTransportType_DirectLink )	? "Direct"	: \
	( (X) == kNetTransportType_BTLE )		? "BTLE"	: \
											  "?" )

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
		NetTransportType *	outTransportType );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketSetKeepAlive
	@abstract	Enables/disables TCP keep alive and configures the time between probes and the max probes before giving up.
	
	@param		inSock					Socket to set keep-alive options for.
	@param		inIdleSecs				Number of idle seconds before a keep-alive probe is sent.
	@param		inMaxUnansweredProbes	Max number of unanswered probes before a connection is terminated.
*/
OSStatus	SocketSetKeepAlive( SocketRef inSock, int inIdleSecs, int inMaxUnansweredProbes );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketSetNonBlocking
	@abstract	Sets the non-blocking state of a socket.
	
	@param		inSock			Socket to set the non-blocking state of.
	@param		inNonBlocking	Non-zero to make non-blocking, 0 to make blocking.
*/
OSStatus	SocketSetNonBlocking( SocketRef inSock, int inNonBlocking );

#define SocketMakeNonBlocking( SOCK )	SocketSetNonBlocking( ( SOCK ), 1 )
#define SocketMakeBlocking( SOCK )		SocketSetNonBlocking( ( SOCK ), 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketSetNotSentLowWatermark
	@abstract	Configures a socket to become writable when the TCP connection's never-sent data is fewer than X bytes.
	
	@param		inSock		Socket to set the non-blocking state of.
	@param		inSize		Number of bytes to set the low watermark. 0 disables.
	
	@discussion
	
	This socket option allows an application to keep TCP's pipeline full without filling the kernel with a large backlog
	of stale data. The ability to generate just-in-time data is helpful for real-time clicks, such as audio/video streaming.
	
	On Darwin, this uses the TCP_NOTSENT_LOWAT socket option. See <radar:9362898> for more detail.
*/
OSStatus	SocketSetNotSentLowWatermark( SocketRef inSock, size_t inSize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketSetP2P
	@abstract	Allows or prevents the use of P2P interfaces.
	@discussion	Warning: Allowing P2P interfaces opens the application up to a lot of security issues because it allows
				communication between devices that may not be on the same network (e.g. WiFi Direct).
*/
OSStatus	SocketSetP2P( SocketRef inSock, int inAllow );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketGetPacketReceiveInterface
	@abstract	Enables/disables/returns the interface index (and optionally the name) where the packet was received.
	@discussion	inNameBuf must be at least IF_NAMESIZE bytes.
*/
#if( TARGET_OS_POSIX )
uint32_t	SocketGetPacketReceiveInterface( struct msghdr *inPacket, char *inNameBuf );
OSStatus	SocketSetPacketReceiveInterface( SocketRef inSock, int inEnable );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketSetPacketTimestamps
	@abstract	Enables or disables receiving timestamp for when a packet was received by the kernel.
	@discussion	Prefers SO_TIMESTAMP_MONOTONIC if available, but will use SO_TIMESTAMP if available otherwise.
*/
OSStatus	SocketSetPacketTimestamps( SocketRef inSock, int inEnabled );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketGetPacketUpTicks
	@abstract	Gets the UpTicks when a packet was received.
	
	@param		inPacket	msghdr for the packet as returned by recvmsg().
	
	@discussion
	
	The socket must have been set up to record packet timestamps by using an API like SocketSetPacketTimestamps().
	The code receiving the packet must have also used recvmsg to receive to get the msghdr for it.
*/
#if( TARGET_OS_POSIX )
	uint64_t	SocketGetPacketUpTicks( struct msghdr *inPacket );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketSetQoS
	@abstract	Sets the IP_TOS/IPV6_TCLASS/SO_TRAFFIC_CLASS settings for a socket.
	@disucssion	See RFC 4594 for details on DiffServ values.
*/
#define kSocketQoS_Default					 0 // WMM=best effort,	DSCP=0b000000 (CS0), SO_TRAFFIC_CLASS=SO_TC_BE.
#define kSocketQoS_Background				 1 // WMM=background,	DSCP=0b001000 (CS1), SO_TRAFFIC_CLASS=SO_TC_BK.
#define kSocketQoS_Video					 2 // WMM=video,		DSCP=0b100000 (CS4), SO_TRAFFIC_CLASS=SO_TC_VI.
#define kSocketQoS_Voice					 3 // WMM=voice,		DSCP=0b110000 (CS6), SO_TRAFFIC_CLASS=SO_TC_VO.
#define kSocketQoS_AirPlayAudio				10 // WMM=video,		DSCP=0b100000 (CS4), SO_TRAFFIC_CLASS=SO_TC_AV.
#define kSocketQoS_AirPlayScreenAudio		11 // WMM=voice,		DSCP=0b110000 (CS6), SO_TRAFFIC_CLASS=SO_TC_VO.
#define kSocketQoS_AirPlayScreenVideo		12 // WMM=video,		DSCP=0b100000 (CS4), SO_TRAFFIC_CLASS=SO_TC_VI.
#define kSocketQoS_NTP						20 // WMM=voice,		DSCP=0b110000 (CS6), SO_TRAFFIC_CLASS=SO_TC_CTL.

OSStatus	SocketSetQoS( SocketRef inSock, int inQoS );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketJoinMulticast / SocketLeaveMulticast
	@abstract	Joins or leaves a multicast group, optionally on a particular interface.
	@discussion
	
	If a a non-NULL interface name is specified, it is used.
	Otherwise, if a non-zero interface index is specified, it is used.
	Otherwise, the system is allowed to choose the interface.
*/
OSStatus	SocketJoinMulticast( SocketRef inSock, const void *inAddr, const char *inIfName, uint32_t inIfIndex );
OSStatus	SocketLeaveMulticast( SocketRef inSock, const void *inAddr, const char *inIfName, uint32_t inIfIndex );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketSetMulticastInterface
	@abstract	Sets the interface to send multicast packets.
	@discussion
	
	If a a non-NULL interface name is specified, it is used.
	Otherwise, if a non-zero interface index is specified, it is used.
	Otherwise, the system is allowed to choose the interface.
*/
OSStatus	SocketSetMulticastInterface( SocketRef inSock, const char *inIfName, uint32_t inIfIndex );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketSetMulticastLoop
	@abstract	Enables or disables multicast loopback.
	@discussion	When enabled, multicast traffics sent on this socket is looped back and received by this socket.
*/
OSStatus	SocketSetMulticastLoop( SocketRef inSock, Boolean inEnableLoopback );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketSetMulticastTTL
	@abstract	Sets the TTL used when sending multicast packets.
*/
OSStatus	SocketSetMulticastTTL( SocketRef inSock, int inTTL );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SockAddrToString
				StringToSockAddr

	@abstract	Converts an AF_INET or AF_INET6 sockaddr to a C string and vice-versa.
	
	@param		inSA		Ptr to a sockaddr-compatible structure (e.g. sockaddr_in *, sockaddr_storage *, etc.).
	@param		outStr		Ptr to string buffer to receive the result. Must be at least 128 bytes to hold the longest string.
	
	@discussion
	
	Examples:
	
		IPv6 with scope ID and port 80: 		"[fe80::5445:5245:444f%5]:80"	(Windows...integer scope)
		IPv6 with scope ID and port 80: 		"[fe80::5445:5245:444f%en0]:80" (Mac...textual scope)
		IPv6 with scope ID and zero port:		"fe80::5445:5245:444f%5"		(Windows...integer scope)
		IPv6 with scope ID and zero port:		"fe80::5445:5245:444f%en0"		(Mac...textual scope)
		IPv6 with no scope ID and zero port:	"fe80::5445:5245:444f"
		
		IPv4 with port 80:						"127.0.0.1:80"
		IPv4 with zero port:					"127.0.0.1"
*/
#define	kSockAddrStringMaxSize			128	//! Maximum size of a SockAddr string (includes the null terminator).

typedef uint32_t	SockAddrStringFlags;

#define	kSockAddrStringFlagsNone				0
#define	kSockAddrStringFlagsNoPort				( 1 << 0 )	//! Do not append the ":port" port number to the string.
#define	kSockAddrStringFlagsNoScope				( 1 << 1 )	//! Do not append the "%<scope>" scope ID to an IPv6 string.
#define	kSockAddrStringFlagsForceIPv6Brackets	( 1 << 2 )	//! Force brackets for IPv6 addresses (e.g. "[fe80::5445:5245:444f]").
#define	kSockAddrStringFlagsEscapeScopeID		( 1 << 3 )	//! Percent-escape scope IDs (e.g. %25en0 instead of %en0).

OSStatus	SockAddrToString( const void *inSA, SockAddrStringFlags inFlags, char *outStr );
OSStatus	StringToSockAddr( const char *inStr, void *outSA, size_t inSASize, size_t *outSASize );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SockAddrGetFamily
	@abstract	Gets the family of the sockaddr.
*/
#define SockAddrGetFamily( SA )		( ( (const struct sockaddr *)(SA) )->sa_family )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SockAddrGetSize
	@abstract	Gets the size of the sockaddr.
*/
socklen_t	SockAddrGetSize( const void *inSA );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SockAddrGetPort
				SockAddrSetPort
	@abstract	Accesses the port number in a sockaddr in host byte order. Note: sockaddr must have family field set.
*/
int		SockAddrGetPort( const void *inSA );
void	SockAddrSetPort( void *inSA, int inPort );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SockAddrCompareAddr
	@abstract	Compares the address portion of a sockaddr and returns memcmp-style response:
				<  0 a  < b
				== 0 a == b
				>  0 a  > b
*/
int	SockAddrCompareAddr( const void *inA1, const void *inA2 );
int	SockAddrCompareAddrEx( const void *inA1, const void *inA2, Boolean inUseIPv6IfIndex );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SockAddrCopy
	@abstract	Copies a sockaddr.
*/
void	SockAddrCopy( const void *inSrc, void *inDst );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SockAddrConvertToIPv6
	@abstract	Converts a sockaddr to IPv6 sockaddr_in6 (if AF_INET, convert to IPv4-mapped IPv6, if AF_INET6, just copy).
	
	@param		inSrc	Address to convert.
	@param		outDst	Receives the converted address. May point to the same memory as "inSrc".
	
	@discussion	This function can be useful convert an address from an IPv4 socket so it's usable by an IPv6 socket.
*/
OSStatus	SockAddrConvertToIPv6( const void *inSrc, void *outDst );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SockAddrGetIPv4
	@abstract	Gets the 32-bit, network byte order IPv4 address out of a sockaddr structure, if it contains an IPv4 
				address (IPv4 or IPv4-mapped/compat IPv6).
*/
OSStatus	SockAddrGetIPv4( const void *inSrc, uint32_t *outIPv4 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SockAddrSimplify
	@abstract	Converts a sockaddr to its simplest form; e.g. if it's IPv6 address wrapping an IPv4 address, convert to IPv4.
	
	@param		inSrc	Address to simplify.
	@param		outDst	Receives the simplified address. May point to the same memory as "inSrc".
	
	@discussion
	
	This function is normally used when IPv4 compatibility is needed when otherwise using IPv6 addresses. For example, a
	dual-stack IPv4/IPv6 server will often use a single IPv6 socket and the IP stack will return an IPv4-mapped IPv6 
	address when communicating with an IPv4 client. This works in many cases, but if a multicast socket needs to be used
	with the same IPv4 client, the address needs to be converted to its IPv4 equivalent.
*/
OSStatus	SockAddrSimplify( const void *inSrc, void *outDst );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SockAddrToDeviceID
	@abstract	Generates a 64-bit device ID from a sockaddr.
	@discussion
	
	A device ID is a 64-bit number used to uniquely identify a device. Ideally, every device would simply provide its MAC
	address and we'd use that. However, older devices didn't do this so to support mapping older devices to reasonable
	unique device IDs, IP addresses are the only thing we have. So device IDs reserves a portion of the number space:
	
	0x00xxxxxxxxxxxxxx MAC  address type device ID (preferred).
	0x04xxxxxxxxxxxxxx IPv4 address type device ID.
	0x06xxxxxxxxxxxxxx IPv6 address type device ID.
*/
uint64_t	SockAddrToDeviceID( const void *inSockAddr );

#if 0
#pragma mark -
#pragma mark == Addressing ==
#endif

//===========================================================================================================================
//	Addressing
//===========================================================================================================================

// Converts a prefix bit count to a mask (e.g. 24 would be 255.255.255.0 or 0xFFFFFF00). Must be <= 32.
// Note: the mask returned is in host byte order.

#define IPv4BitsToMask( BITS )		( (uint32_t)( ( (BITS) != 0 ) ? ( UINT32_C( 0xFFFFFFFF ) << ( 32 - (BITS) ) ) : 0 ) )

// Calculates the default router address for the specified address and subnet (e.g. 10.0.20.6 and 0xFFFFFF00 -> 10.0.20.1).
// Note: ADDR and SUBNET must be in network byte order. Result is in network byte order.

#define IPv4DefaultRouterForAddressAndSubnet( ADDR, SUBNET )	( htonl( ( ntohl( (ADDR) ) & ntohl( (SUBNET) ) ) | 1 ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsIPv4MartianAddress/IsIPv6MartianAddress
	@abstract	Returns true if "inAddr" is a martian address. Returns false otherwise.
	@discussion	For details, see <http://6session.wordpress.com/2009/04/08/ipv6-martian-and-bogon-filters/>.
*/
#define kMartianFlags_None					0
#define kMartianFlags_AllowUnspecified		( 1 << 0 ) //! Don't consider ::/128 martian.
#define kMartianFlags_AllowLinkLocal		( 1 << 1 ) //! Don't consider fe80::/10 martian.
#define kMartianFlags_AllowUniqueLocal		( 1 << 2 ) //! Don't consider fc00::/7 martian.

Boolean	IsIPv4MartianAddress( uint32_t inAddr );						// Note: network byte order IPv4 address (same as sin_addr.s_addr).
Boolean	IsIPv6MartianAddress( const void *inAddr );						// Note: 16-byte IPv6 address (not a sockaddr_in6).
Boolean	IsIPv6MartianAddressEx( const void *inAddr, uint32_t inFlags );	// Note: 16-byte IPv6 address (not a sockaddr_in6).

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsGlobalIPv4Address
	@abstract	Returns non-zero if the network byte order IPv4 address is not :
					IPv4 Link Local
					RFC 1918 private address
					Loopback
					Multicast
					the zero address
	
	@param		inIPv4	32-bit IPv4 address in network byte order (i.e. same format as sin_addr.s_addr).
	
	@result		Non-zero = Global Address.
				0		 = Not a global address.
*/
int	IsGlobalIPv4Address( uint32_t inIPv4 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsPrivateIPv4Address
	@abstract	Returns non-zero if the network byte order IPv4 address is an RFC 1918 private address or 0 otherwise.
	
	@param		inIPv4	32-bit IPv4 address in network byte order (i.e. same format as sin_addr.s_addr).
	
	@result		Non-zero = Private Address.
				0		 = Not a private address.
*/
int	IsPrivateIPv4Address( uint32_t inIPv4 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsRoutableIPv4Address
	@abstract	Returns non-zero if the network byte order IPv4 address is not :
					IPv4 Link Local
					the zero address
	
	@param		inIPv4	32-bit IPv4 address in network byte order (i.e. same format as sin_addr.s_addr).
	
	@result		Non-zero = Routable Address.
				0		 = Not a Routable address.
*/
int	IsRoutableIPv4Address( uint32_t inIPv4 );

#if 0
#pragma mark == Misc ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CompareMACAddresses
	@abstract	Compare two Ethernet MAC addresses, return :
					 0 - addresses are the same
					>0 - address 1 is greater than address 2 
					<0 - address 1 is less than address 2 
	
	@param		inMACAddress1		6-byte Ethernet address of interface
	@param		inMACAddress2		6-byte Ethernet address of interface
*/
int	CompareMACAddresses( const void *inMACAddress1, const void *inMACAddress2 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetInterfaceExtendedFlags
	@abstract	Returns extended flags for an interface (e.g. IFEF_* constants from the private version of if.h).
*/
uint64_t	GetInterfaceExtendedFlags( const char *inIfName );
Boolean		IsP2PInterface( const char *inIfName );
Boolean		IsWiFiNetworkInterface( const char *inIfName );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetIPAddress
	@abstract	Returns IPv4 address and/or IPv4 netmask of interface
	
	@param		inInterfaceName		interface name (i.e., "eth0")
	@param		outIPAddress		32-bit IPv4 address in network byte order (i.e. same format as sin_addr.s_addr) - may be NULL.
	@param		outNetmask			32-bit IPv4 netmask in network byte order (i.e. same format as sin_addr.s_addr) - may be NULL.
*/
OSStatus	GetIPAddress( const char *inInterfaceName, uint32_t *outIPAddress, uint32_t *outNetmask );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetPrimaryIPAddress
	@abstract	Returns the primary IPv4 and/or IPv6 address.
*/
OSStatus	GetPrimaryIPAddress( sockaddr_ip *outIPv4, sockaddr_ip *outIPv6 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetLocalHostName
	@abstract	Returns the local hostname, such as "mycomputer.local", suitable for resolving via DNS or Bonjour.
*/
OSStatus	GetLocalHostName( char *inBuf, size_t inMaxLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetInterfaceMACAddress
	@abstract	Returns Ethernet MAC address of interface
	
	@param		inInterfaceName		interface name (i.e., "eth0")
	@param		outMACAddress		6-byte Ethernet address of interface
*/
OSStatus	GetInterfaceMACAddress( const char *inInterfaceName, uint8_t *outMACAddress );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SetInterfaceMACAddress
	@abstract	sets the Ethernet MAC address of interface
	
	@param		inInterfaceName		interface name (i.e., "eth0")
	@param		inMACAddress		6-byte Ethernet address of interface
*/
OSStatus	SetInterfaceMACAddress( const char *inInterfaceName, const uint8_t *inMACAddress );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GetPeerMACAddress
	@abstract	Search the ARP/ND table for a peer by IPv4/IPv6 address and if found, return its MAC address.
	
	@param		inPeerAddr		sockaddr_in/sockaddr_in6 of the peer to search for.
	@param		outMAC			Receives 6-byte MAC address of peer on success.
*/
OSStatus	GetPeerMACAddress( const void *inPeerAddr, uint8_t outMAC[ 6 ] );

#if( !EXCLUDE_UNIT_TESTS )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SocketUtilsTest
	@abstract	Unit test.
*/
OSStatus	SocketUtilsTest( void );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NetUtilsTest
	@abstract	Unit test.
*/
#if( !EXCLUDE_UNIT_TESTS )
	OSStatus	NetUtilsTest( void );
#endif

#ifdef __cplusplus
}
#endif

#endif // __NetUtils_h__
