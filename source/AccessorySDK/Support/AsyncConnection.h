/*
	File:    	AsyncConnection.h
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
	
	Copyright (C) 2010-2015 Apple Inc. All Rights Reserved.
*/

#ifndef	__AsyncConnection_h__
#define	__AsyncConnection_h__

#include "CommonServices.h"
#include "LogUtils.h"

#include CF_HEADER
#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AsyncConnection *		AsyncConnectionRef;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	AsyncConnectionFlags
	@abstract	Flags to affect how the connections work.
*/
typedef uint32_t		AsyncConnectionFlags;
#define kAsyncConnectionFlags_None							0
#define kAsyncConnectionFlag_P2P							( 1 << 0 ) // Allows connections on P2P interfaces.
#define kAsyncConnectionFlag_SuppressUnusable				( 1 << 1 ) // Suppress trying to connect on seemingly unusable interfaces.
#define kAsyncConnectionFlag_Reachability					( 1 << 2 ) // Use the reachability APIs before trying to connect.
#define kAsyncConnectionFlag_BoundInterface					( 1 << 3 ) // Set bound interface before connect if interface index available.
#define kAsyncConnectionFlag_NonCellular					( 1 << 4 ) // Don't allow connections over cellular links.
#define kAsyncConnectionFlag_NonExpensive					( 1 << 5 ) // Don't allow connections over expensive links (cellular, hotspot, etc.).
#define kAsyncConnectionFlag_NonLinkLocal					( 1 << 7 ) // Skip link-local addresses.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@callback	AsyncConnectionHandlerFunc
	@abstract	Function to be called when an async connection completes (success or failure).
	
	@param		inSock		Connected socket on success or kInvalidSocketRef on failure. If valid, you must close it.
	@param		inError		kNoErr on success or an error code on failure.
	@param		inArg		Context pointer that was provided to AsyncConnection_Connect via "inHandlerArg".
	
	@discussion
	
	Note: this callback will always be called exactly once.
*/
typedef void ( *AsyncConnectionHandlerFunc )( SocketRef inSock, OSStatus inError, void *inArg );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@callback	AsyncConnectionProgressFunc
	@abstract	Function to be called for progress events during the connection process.
*/
#define kAsyncConnectionPhase_QueryingSRV		1 // Details: Destination string.
#define kAsyncConnectionPhase_ResolvingDNS		2 // Details: Destination string.
#define kAsyncConnectionPhase_Connecting		3 // Details: sockaddr_ip *connectedAddr.
#define kAsyncConnectionPhase_Connected			4 // Details: AsyncConnectedInfo *info.

typedef struct
{
	sockaddr_ip			addr;				// Connected address.
	CFTimeInterval		reachabilitySecs;	// Seconds to perform reachability check.
	CFTimeInterval		srvSecs;			// Seconds to perform Bonjour SRV query.
	CFTimeInterval		dnsResolveSecs;		// Seconds to perform DNS name lookup.
	CFTimeInterval		connectSecs;		// Seconds to perform TCP connect.
	CFTimeInterval		totalSecs;			// Total seconds from the start until the TCP connect completed.
	
}	AsyncConnectedInfo;

typedef void ( *AsyncConnectionProgressFunc )( int inPhase, const void *inDetails, void *inArg );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@callback	AsyncConnectionProgressFunc
	@abstract	Function to be called while waiting for connections to complete.
*/
typedef OSStatus ( *AsyncConnectionWaitFunc )( void *inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		AsyncConnectionParams
	@abstract	Params for connection.
*/
typedef struct
{
	const char *					destination;			// Destination Bonjour name, DNS, and/or IP address to connect to.
	int								defaultPort;			// Default port if a port number is provided in the destination address.
	AsyncConnectionFlags			flags;					// Flags to control connection.
	uint64_t						timeoutNanos;			// Overall timeout for connection. Use DISPATCH_TIME_FOREVER for no timeout.
	int								socketSendBufferSize;	// Send buffer for socket. Use kSocketBufferSize_DontSet for default.
	int								socketRecvBufferSize;	// Receive buffer for socket. Use kSocketBufferSize_DontSet for default.
	AsyncConnectionProgressFunc		progressFunc;			// Function to call for progress. May be NULL.
	void *							progressArg;			// Argument to pass to progress function. May be NULL.
	AsyncConnectionHandlerFunc		handlerFunc;			// Function to call when connection succeeds or fails.
	void *							handlerArg;				// Argument to pass to handler function. May be NULL.
	DISPATCH_UNSAFE_UNRETAINED
	dispatch_queue_t				handlerQueue;			// Dispatch queue to invoke callbacks on.
	LogCategory *					logCategory;			// Log category to use for logging. NULL uses a custom category.
	uint64_t						ipv6DelayNanos;			// Nanoseconds to delay IPv6 connection attempts. Use 0 for no delay.
	
}	AsyncConnectionParams;

#define AsyncConnectionParamsInit( PTR )	memset( (PTR), 0, sizeof( AsyncConnectionParams ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AsyncConnection_Connect
	@abstract	Starts an async connect.
	
	@param		outConnection	Receives a connection object that can be used to cancel the connect.
	@param		inDestination	Destination to connect to. The following formats are supported:
	
		Bonjour service name:	<friendly name>.<service type>._tcp.<domain>[%<interface index>][:<port number>]
								For example: "Brother\032HL-5370DW\032series._http._tcp.local.%4:80".
								
								This name usually comes from a Bonjour browse of the service type of the device.
								You can use the name, type, and domain with DNSServiceConstructFullName to form
								the name to pass here. That function will escape things as needed.
		
		DNS name:				<DNS name>[%<interface index>][:<port number>]
								For example: "Brother-HL-5370DW-series.local.:80".
		
		IPv4 address:			<IPv4 address>[:<port number>]
								For example: "10.0.20.10:80"
		
		IPv6 address:			[[]<IPv6 address>[%<scope>][]][:<port number>]
								For example: "[fe80::20d:93ff:feca:f7af%4]:5000"
		
		URL:					<scheme>://<name/address in Bonjour/DNS/IPv4/IPv6 format>.
								For example: "http://www.apple.com".
		
		Destination may specify multiple destinations by separating them with an ASCII record separator character (\x1E). 
		Each one will be tried in parallel until a connection succeeds or they all fail.
		For example: "10.0.20.123" "\x1E" "www.apple.com".
	
	@param		inDefaultPort				Default TCP port to use if no other port is known. < 0 means force it.
	@param		inTimeoutNanos				Max nanoseconds to wait for a connection. Use 0 for no timeout.
	@param		inSocketSendBufferSize		Socket Send buffer size. See SocketSetBufferSize for details.
	@param		inSocketRecvBufferSize		Socket Receive buffer size. See SocketSetBufferSize for details.
	@param		inHandlerFunc				Function to be called on success or failure.
	@param		inHandlerArg				Argument to be passed to the handler function.
	@param		inQueue						Dispatch queue to invoke the handler on.
*/
#define kAsyncConnectionNoTimeout		0

OSStatus
	AsyncConnection_Connect( 
		AsyncConnectionRef *			outConnection, 
		const char *					inDestination, 
		int								inDefaultPort, 
		AsyncConnectionFlags			inFlags, 
		uint64_t						inTimeoutNanos, 
		int								inSocketSendBufferSize, 
		int								inSocketRecvBufferSize, 
		AsyncConnectionProgressFunc		inProgressFunc, 
		void *							inProgressArg, 
		AsyncConnectionHandlerFunc		inHandlerFunc, 
		void *							inHandlerArg, 
		dispatch_queue_t				inQueue );

OSStatus	AsyncConnection_ConnectEx( AsyncConnectionRef *outConnection, const AsyncConnectionParams *inParams );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AsyncConnection_Release
	@abstract	Asynchronously cancel the connection, invoke the callback (if it hasn't already), and release its memory.
	@discussion
	
	The handler will always be invoked exactly once so you must always wait for the handler to be invoked before 
	invalidating any resources that might be used by the handler function.
*/
void	AsyncConnection_Release( AsyncConnectionRef inConnection );
#define	AsyncConnection_Forget( X )		ForgetCustom( (X), AsyncConnection_Release )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AsyncConnection_ConnectSync
	@abstract	Synchronous helper function for legacy code.
*/
OSStatus
	AsyncConnection_ConnectSync( 
		const char *				inDestination, 
		int							inDefaultPort, 
		AsyncConnectionFlags		inFlags, 
		uint64_t					inTimeoutNanos, 
		int							inSocketSendBufferSize, 
		int							inSocketRecvBufferSize, 
		AsyncConnectionProgressFunc	inProgressFunc, 
		void *						inProgressArg, 
		SocketRef *					outSock );

OSStatus
	AsyncConnection_ConnectSyncEx( 
		const char *				inDestination, 
		int							inDefaultPort, 
		AsyncConnectionFlags		inFlags, 
		uint64_t					inTimeoutNanos, 
		int							inSocketSendBufferSize, 
		int							inSocketRecvBufferSize, 
		AsyncConnectionProgressFunc	inProgressFunc, 
		void *						inProgressArg, 
		AsyncConnectionWaitFunc		inWaitCallBack, 
		void *						inWaitContext, 
		SocketRef *					outSock );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AsyncConnection_Test
	@abstract	Unit test.
*/
#if( !EXCLUDE_UNIT_TESTS )
	OSStatus	AsyncConnection_Test( void );
#endif

#ifdef __cplusplus
}
#endif

#endif // __AsyncConnection_h__
