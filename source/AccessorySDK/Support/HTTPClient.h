/*
	File:    	HTTPClient.h
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
	
	Copyright (C) 2011-2015 Apple Inc. All Rights Reserved.
*/

#ifndef	__HTTPClient_h__
#define	__HTTPClient_h__

#include "CFUtils.h"
#include "CommonServices.h"
#include "HTTPMessage.h"
#include "NetUtils.h"

#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientCreate
	@abstract	Creates a new HTTP client.
*/
typedef struct HTTPClientPrivate *		HTTPClientRef;

CFTypeID	HTTPClientGetTypeID( void );
OSStatus	HTTPClientCreate( HTTPClientRef *outClient );
OSStatus	HTTPClientCreateWithSocket( HTTPClientRef *outClient, SocketRef inSock );
#define 	HTTPClientForget( X ) do { if( *(X) ) { HTTPClientInvalidate( *(X) ); CFRelease( *(X) ); *(X) = NULL; } } while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientInvalidate
	@abstract	Cancels all outstanding operations.
*/
void	HTTPClientInvalidate( HTTPClientRef inClient );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientGetPeerAddress
	@abstract	Gets the address of the connected peer.
	@discussion	Only valid after a connection has been established.
*/
OSStatus	HTTPClientGetPeerAddress( HTTPClientRef inClient, void *inSockAddr, size_t inMaxLen, size_t *outLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientSetDebugDelegate
	@abstract	Sets a delegate for debug hooks.
*/
typedef void
	( *HTTPClientDebugDelegateSendMessage_f )( 
		const void *	inHeadersPtr, 
		size_t			inHeadersLen, 
		const void *	inBodyPtr, 
		size_t			inBodyLen, 
		void *			inContext );
typedef void
	( *HTTPClientDebugDelegateReceiveMessage_f )( 
		const void *	inHeadersPtr, 
		size_t			inHeadersLen, 
		const void *	inBodyPtr, 
		size_t			inBodyLen, 
		void *			inContext );

typedef struct
{
	void *										context;
	HTTPClientDebugDelegateSendMessage_f		sendMessage_f;
	HTTPClientDebugDelegateReceiveMessage_f		receiveMessage_f;
	
}	HTTPClientDebugDelegate;

#define HTTPClientDebugDelegateInit( PTR )	memset( (PTR), 0, sizeof( HTTPClientDebugDelegate ) );

void	HTTPClientSetDebugDelegate( HTTPClientRef inClient, const HTTPClientDebugDelegate *inDelegate );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientSetDelegate
	@abstract	Sets a delegate to handle events, etc.
*/
typedef void ( *HTTPClientHandleBinary_f )( uint8_t inChannelID, const uint8_t *inPtr, size_t inLen, void *inContext );
typedef void ( *HTTPClientHandleEvent_f )( HTTPMessageRef inEvent, void *inContext );
typedef void ( *HTTPClientInvalidated_f )( OSStatus inReason, void *inContext );

typedef struct
{
	void *						context;
	HTTPClientInvalidated_f		invalidated_f;
	HTTPClientHandleBinary_f	handleBinary_f;
	HTTPClientHandleEvent_f		handleEvent_f;
	
}	HTTPClientDelegate;

#define HTTPClientDelegateInit( PTR )	memset( (PTR), 0, sizeof( HTTPClientDelegate ) );

void *	HTTPClientGetDelegateContext( HTTPClientRef me );
void	HTTPClientSetDelegate( HTTPClientRef inClient, const HTTPClientDelegate *inDelegate );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientSetDestination
	@abstract	Sets the destination hostname, IP address, URL, etc. of the HTTP server to talk to.
	@discussion	Note: this cannot be changed once set.
*/
OSStatus	HTTPClientSetDestination( HTTPClientRef inClient, const char *inDestination, int inDefaultPort );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientSetDispatchQueue
	@abstract	Sets the GCD queue to perform all operations on.
	@discussion	Note: this cannot be changed once operations have started.
*/
void	HTTPClientSetDispatchQueue( HTTPClientRef inClient, dispatch_queue_t inQueue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientSetFlags
	@abstract	Enables or disables P2P connections.
*/
typedef uint32_t		HTTPClientFlags;
#define kHTTPClientFlag_None						0
#define kHTTPClientFlag_P2P							( 1 << 0 ) // Enable P2P connections.
#define kHTTPClientFlag_SuppressUnusable			( 1 << 1 ) // Suppress trying to connect on seemingly unusable interfaces.
#define kHTTPClientFlag_Reachability				( 1 << 2 ) // Use the reachability APIs before trying to connect.
#define kHTTPClientFlag_BoundInterface				( 1 << 3 ) // Set bound interface before connect if interface index available.
#define kHTTPClientFlag_Events						( 1 << 4 ) // Enable support for unsolicited events from the server.
#define kHTTPClientFlag_NonCellular					( 1 << 5 ) // Don't allow connections over cellular links.
#define kHTTPClientFlag_NonExpensive				( 1 << 6 ) // Don't allow connections over expensive links (cellular, hotspot, etc.).
#define kHTTPClientFlag_NonLinkLocal				( 1 << 8 ) // Skip link-local addresses.

void	HTTPClientSetFlags( HTTPClientRef inClient, HTTPClientFlags inFlags, HTTPClientFlags inMask );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientSetKeepAlive
	@abstract	Enables/disables TCP keep alive and configures the time between probes and the max probes before giving up.
	
	@param		inClient				Client to set keep-alive options for.
	@param		inIdleSecs				Number of idle seconds before a keep-alive probe is sent.
	@param		inMaxUnansweredProbes	Max number of unanswered probes before a connection is terminated.
*/
void	HTTPClientSetKeepAlive( HTTPClientRef inClient, int inIdleSecs, int inMaxUnansweredProbes );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientSetLogging
	@abstract	Sets the log category to use for HTTP connection and other logging.
*/
void	HTTPClientSetConnectionLogging( HTTPClientRef me, LogCategory *inLogCategory );
void	HTTPClientSetLogging( HTTPClientRef inClient, LogCategory *inLogCategory );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		HTTPClientProperties
	@abstract	Properties to get/set for the HTTPClient.
*/

// [Number:HTTPAuthorizationScheme] Auth schemes to allow. Defaults to none.
#define kHTTPClientProperty_AllowedAuthSchemes		CFSTR( "allowedAuthSchemes" )

// [String] Password to use for HTTP auth.
#define kHTTPClientProperty_Password				CFSTR( "password" )

// [Boolean] Use RFC 2617-style digest auth (i.e. lowercase hex). Defaults to true.
#define kHTTPClientProperty_RFC2617DigestAuth		CFSTR( "rfc2617DigestAuth" )

// [String] Username to use for HTTP auth.
#define kHTTPClientProperty_Username				CFSTR( "username" )

CFTypeRef	_HTTPClientCopyProperty( CFTypeRef inObject, CFStringRef inProperty, OSStatus *outErr );
OSStatus	_HTTPClientSetProperty( CFTypeRef inObject, CFStringRef inProperty, CFTypeRef inValue );

CFObjectDefineStandardAccessors( HTTPClientRef, HTTPClientProperty, _HTTPClientCopyProperty, _HTTPClientSetProperty )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientSetTimeout
	@abstract	Sets the seconds without any data before a response or event times out.
	@discussion	For responses, if a message has a timeout, it override this value.
*/
void	HTTPClientSetTimeout( HTTPClientRef inClient, int inSecs );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientSetTransportDelegate
	@abstract	Sets a delegate for transport-specific reading and writing data.
*/
void	HTTPClientSetTransportDelegate( HTTPClientRef inClient, const NetTransportDelegate *inDelegate );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientDetach
	@abstract	Detaches a client from its socket to hand it off to other code (e.g. reverse connections).
	@discussion
	
	This finishes handling any messages that may already be in progress and tears down the client, but leaves the
	socket open. The handler is called after the client has fully quiesced and the socket can be safely used.
*/
typedef void ( *HTTPClientDetachHandler_f )( SocketRef inSock, void *inContext1, void *inContext2, void *inContext3 );
OSStatus
	HTTPClientDetach( 
		HTTPClientRef				inClient, 
		HTTPClientDetachHandler_f	inHandler, 
		void *						inContext1, 
		void *						inContext2, 
		void *						inContext3 );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientSendBinaryBytes
	@abstract	Sends a one-way message containing raw binary data and calls back when it has been written.
*/
OSStatus
	HTTPClientSendBinaryBytes( 
		HTTPClientRef					inClient, 
		HTTPMessageFlags				inFlags, 
		uint8_t							inChannelID, 
		const void *					inPtr, 
		size_t							inLen,  
		HTTPMessageBinaryCompletion_f	inCompletion, 
		void *							inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientSendMessage
	@abstract	Sends an HTTP message.
*/
OSStatus	HTTPClientSendMessage( HTTPClientRef inClient, HTTPMessageRef inMsg );
OSStatus	HTTPClientSendMessageSync( HTTPClientRef inClient, HTTPMessageRef inMsg );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	HTTPClientTest
	@abstract	Unit test.
*/
OSStatus	HTTPClientTest( void );

#ifdef __cplusplus
}
#endif

#endif // __HTTPClient_h__
