/*
	File:    	HTTPClient.c
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

#include "HTTPClient.h"

#include <stdlib.h>
#include <string.h>

#include "AsyncConnection.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "HTTPMessage.h"
#include "HTTPUtils.h"
#include "NetUtils.h"
#include "PrintFUtils.h"
#include "StringUtils.h"
#include <glib.h>
#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

#if( !defined( HTTP_CLIENT_TEST_GCM ) )
	#define HTTP_CLIENT_TEST_GCM		0
#endif
#if( HTTP_CLIENT_TEST_GCM )
	#include "NetTransportGCM.h"
#endif

//===========================================================================================================================
//	Structures
//===========================================================================================================================

typedef enum
{
	kHTTPClientStateIdle				= 0, 
	kHTTPClientStateConnecting			= 1, 
	kHTTPClientStatePreparingRequest	= 2, 
	kHTTPClientStateWritingRequest		= 3, 
	kHTTPClientStateReadingResponse		= 4, 
	kHTTPClientStateWaitingForClose		= 5, 
	kHTTPClientStateFinishingMesssage	= 6, 
	kHTTPClientStatePreparingForEvent	= 7, 
	kHTTPClientStateReadingEvent		= 8, 
	kHTTPClientStateError				= 9
	
}	HTTPClientState;

struct HTTPClientPrivate
{
	CFRuntimeBase				base;					// CF type info. Must be first.
	dispatch_queue_t			queue;					// Queue to perform all operations on.
	
	// Configuration
	
	HTTPAuthorizationScheme		allowedAuthSchemes;		// Auth schemes to allow.
	HTTPClientDelegate			delegate;				// Delegate for handling events, etc.
	char *						destination;			// Destination hostname, IP address, URL, etc. of the server to talk to.
	int							defaultPort;			// Default port to connect to if not provided in the destination string.
	HTTPClientFlags				flags;					// Flags affecting operation.
	LogCategory *				ucat;					// Log category to use for logging.
	LogCategory *				connectionUcat;			// Log category to use for connection logging.
	int							keepAliveIdleSecs;		// Number of idle seconds before a keep-alive probe is sent.
	int							keepAliveMaxUnansweredProbes; // Max number of unanswered probes before a connection is terminated
	char *						password;				// Password for HTTP auth.
	Boolean						rfc2617DigestAuth;		// True to use RFC 2617-style digest auth (i.e. lowercase hex).
	int							timeoutSecs;			// Seconds without any data before timing out on a response or event.
	char *						username;				// Username for HTTP auth.
	
	// Networking
	
	HTTPClientState				state;					// State of the client: connecting, reading, writing, etc.
	AsyncConnectionRef			connector;				// Used for connecting. Only non-NULL while connecting.
	SocketRef					sock;					// Socket used for I/O to the server.
	int							sockRefCount;			// Number of references to the socket.
	dispatch_source_t			readSource;				// GCD source for readability notification.
	Boolean						readSuspended;			// True if GCD read source has been suspended.
	dispatch_source_t			writeSource;			// GCD source for writability notification.
	Boolean						writeSuspended;			// True if GCD write source has been suspended.
	dispatch_source_t			timerSource;			// GCD source for handling timeouts.
	char						extraDataBuf[ 8192 ];	// Extra data from a previous read.
	size_t						extraDataLen;			// Number of bytes in extraDataBuf;
	HTTPClientAuthorizationInfo	httpAuthInfo;			// Info for HTTP-style auth.
	HTTPMessageRef				eventMsg;				// Object for reading events.
	HTTPClientDebugDelegate		debugDelegate;			// Delegate for debug hooks.
	NetTransportDelegate		transportDelegate;		// Delegate for transport-specific message reading/writing.
	Boolean						hasTransportDelegate;	// True if a transport delegate has been set.
	HTTPClientDetachHandler_f	detachHandler_f;		// Function to call when a detach has completed.
	void *						context1;				// Generic context ptr.
	void *						context2;				// Generic context ptr.
	void *						context3;				// Generic context ptr.
	
	// Messages
	
	HTTPMessageRef				messageList;			// List of messages to send.
	HTTPMessageRef *			messageNext;			// Ptr to append next message to send.
};

check_compile_time( sizeof_field( struct HTTPClientPrivate, extraDataBuf ) == sizeof_field( HTTPHeader, buf ) );

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static void		_HTTPClientGetTypeID( void *inContext );
static void		_HTTPClientFinalize( CFTypeRef inCF );
static void		_HTTPClientInvalidate( void *inContext );
static void		_HTTPClientRunStateMachine( HTTPClientRef me );
static void		_HTTPClientSendBinaryCompletion( HTTPMessageRef inMsg );
static void		_HTTPClientSendMessage( void *inContext );
static void		_HTTPClientConnectHandler( SocketRef inSock, OSStatus inError, void *inArg );
static void		_HTTPClientReadHandler( void *inContext );
static void		_HTTPClientWriteHandler( void *inContext );
static void		_HTTPClientCancelHandler( void *inContext );
static void		_HTTPClientErrorHandler( HTTPClientRef me, OSStatus inError );
static void		_HTTPClientTimerFiredHandler( void *inContext );
static void		_HTTPClientTimerCanceledHandler( void *inContext );
static void		_HTTPClientCompleteMessage( HTTPClientRef me, HTTPMessageRef inMsg, OSStatus inStatus );
static OSStatus	_HTTPClientSaveRequest( HTTPMessageRef inMsg );
static OSStatus	_HTTPClientPrepareForAuthResend( HTTPClientRef me, HTTPMessageRef inMsg );
static OSStatus	_HTTPClientHandleIOError( HTTPClientRef me, OSStatus inError, Boolean inRead );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static dispatch_once_t			gHTTPClientInitOnce = 0;
static CFTypeID					gHTTPClientTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kHTTPClientClass = 
{
	0,						// version
	"HTTPClient",			// className
	NULL,					// init
	NULL,					// copy
	_HTTPClientFinalize,	// finalize
	NULL,					// equal -- NULL means pointer equality.
	NULL,					// hash  -- NULL means pointer hash.
	NULL,					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

ulog_define( HTTPClientCore, kLogLevelNotice, kLogFlags_Default, "HTTPClient", NULL );

//===========================================================================================================================
//	HTTPClientGetTypeID
//===========================================================================================================================

CFTypeID	HTTPClientGetTypeID( void )
{
	dispatch_once_f( &gHTTPClientInitOnce, NULL, _HTTPClientGetTypeID );
	return( gHTTPClientTypeID );
}

static void _HTTPClientGetTypeID( void *inContext )
{
	(void) inContext;
	
	gHTTPClientTypeID = _CFRuntimeRegisterClass( &kHTTPClientClass );
	check( gHTTPClientTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	HTTPClientCreate
//===========================================================================================================================

OSStatus	HTTPClientCreate( HTTPClientRef *outClient )
{
	OSStatus			err;
	HTTPClientRef		me;
	size_t				extraLen;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (HTTPClientRef) _CFRuntimeCreateInstance( NULL, HTTPClientGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->sock = kInvalidSocketRef;
	
	me->queue = dispatch_get_main_queue();
	arc_safe_dispatch_retain( me->queue );
	
	me->rfc2617DigestAuth			= true;
	me->ucat						= &log_category_from_name( HTTPClientCore );
	me->transportDelegate.read_f	= SocketTransportRead;
	me->transportDelegate.writev_f	= SocketTransportWriteV;
	me->messageNext = &me->messageList;
	HTTPClientAuthorization_Init( &me->httpAuthInfo );
	
	*outClient = me;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPClientCreateWithSocket
//===========================================================================================================================

OSStatus	HTTPClientCreateWithSocket( HTTPClientRef *outClient, SocketRef inSock )
{
	OSStatus			err;
	HTTPClientRef		client;
	
	err = HTTPClientCreate( &client );
	require_noerr( err, exit );
	
	client->sock = inSock;
	*outClient = client;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_HTTPClientFinalize
//===========================================================================================================================

static void	_HTTPClientFinalize( CFTypeRef inCF )
{
	HTTPClientRef const		me = (HTTPClientRef) inCF;
	
	dispatch_forget( &me->queue );
	ForgetMem( &me->destination );
	ForgetMem( &me->password );
	ForgetMem( &me->username );
	if( me->transportDelegate.finalize_f ) me->transportDelegate.finalize_f( me->transportDelegate.context );
	check( me->sockRefCount == 0 );
	ForgetSocket( &me->sock );
	HTTPClientAuthorization_Free( &me->httpAuthInfo );
}

//===========================================================================================================================
//	HTTPClientInvalidate
//===========================================================================================================================

void	HTTPClientInvalidate( HTTPClientRef me )
{
	CFRetain( me );
	dispatch_async_f( me->queue, me, _HTTPClientInvalidate );
}

static void	_HTTPClientInvalidate( void *inContext )
{
	HTTPClientRef const		me = (HTTPClientRef) inContext;
	
	_HTTPClientErrorHandler( me, kCanceledErr );
	CFRelease( me );
}

//===========================================================================================================================
//	HTTPClientGetPeerAddress
//===========================================================================================================================

OSStatus	HTTPClientGetPeerAddress( HTTPClientRef inClient, void *inSockAddr, size_t inMaxLen, size_t *outLen )
{
	OSStatus		err;
	socklen_t		len;
	
	len = (socklen_t) inMaxLen;
	err = getpeername( inClient->sock, (struct sockaddr *) inSockAddr, &len );
	err = map_socket_noerr_errno( inClient->sock, err );
	require_noerr( err, exit );
	
	if( outLen ) *outLen = len;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPClientSetTransportDelegate
//===========================================================================================================================

void	HTTPClientSetDebugDelegate( HTTPClientRef me, const HTTPClientDebugDelegate *inDelegate )
{
	me->debugDelegate = *inDelegate;
}

//===========================================================================================================================
//	HTTPClientGetDelegateContext
//===========================================================================================================================

void *	HTTPClientGetDelegateContext( HTTPClientRef me )
{
	return( me->delegate.context );
}

//===========================================================================================================================
//	HTTPClientSetDelegate
//===========================================================================================================================

void	HTTPClientSetDelegate( HTTPClientRef me, const HTTPClientDelegate *inDelegate )
{
	me->delegate = *inDelegate;
}

//===========================================================================================================================
//	HTTPClientSetDestination
//===========================================================================================================================

OSStatus	HTTPClientSetDestination( HTTPClientRef me, const char *inDestination, int inDefaultPort )
{
	OSStatus		err;
	char *			tempStr;
	
	tempStr = strdup( inDestination );
	require_action( tempStr, exit, err = kNoMemoryErr );
	
	if( me->destination ) free( me->destination );
	me->destination = tempStr;
	me->defaultPort = inDefaultPort;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPClientSetDispatchQueue
//===========================================================================================================================

void	HTTPClientSetDispatchQueue( HTTPClientRef me, dispatch_queue_t inQueue )
{
	ReplaceDispatchQueue( &me->queue, inQueue );
}

//===========================================================================================================================
//	HTTPClientSetFlags
//===========================================================================================================================

void	HTTPClientSetFlags( HTTPClientRef me, HTTPClientFlags inFlags, HTTPClientFlags inMask )
{
	me->flags = ( me->flags & ~inMask ) | ( inFlags & inMask );
}

//===========================================================================================================================
//	HTTPClientSetKeepAlive
//===========================================================================================================================

void	HTTPClientSetKeepAlive( HTTPClientRef me, int inIdleSecs, int inMaxUnansweredProbes )
{
	me->keepAliveIdleSecs				= inIdleSecs;
	me->keepAliveMaxUnansweredProbes	= inMaxUnansweredProbes;
}

//===========================================================================================================================
//	HTTPClientSetConnectionLogging
//===========================================================================================================================

void	HTTPClientSetConnectionLogging( HTTPClientRef me, LogCategory *inLogCategory )
{
	me->connectionUcat = inLogCategory;
}

//===========================================================================================================================
//	HTTPClientSetLogging
//===========================================================================================================================

void	HTTPClientSetLogging( HTTPClientRef me, LogCategory *inLogCategory )
{
	me->ucat = inLogCategory;
}

//===========================================================================================================================
//	_HTTPClientCopyProperty
//===========================================================================================================================

CFTypeRef	_HTTPClientCopyProperty( CFTypeRef inObject, CFStringRef inProperty, OSStatus *outErr )
{
	HTTPClientRef const		me		= (HTTPClientRef) inObject;
	CFTypeRef				value	= NULL;
	OSStatus				err;
	
	(void) me;
	(void) inProperty;
	
	if( 0 ) {}
	
	// Unsupported.
	
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	_HTTPClientSetProperty
//===========================================================================================================================

OSStatus	_HTTPClientSetProperty( CFTypeRef inObject, CFStringRef inProperty, CFTypeRef inValue )
{
	HTTPClientRef const		me = (HTTPClientRef) inObject;
	OSStatus				err;
	char *					cptr;
	
	if( 0 ) {}
	
	// AllowedAuthSchemes
	
	else if( CFEqual( inProperty, kHTTPClientProperty_AllowedAuthSchemes ) )
	{
		me->allowedAuthSchemes = (HTTPAuthorizationScheme) CFGetInt64( inValue, NULL );
	}
	
	// Password
	
	else if( CFEqual( inProperty, kHTTPClientProperty_Password ) )
	{
		require_action( !inValue || CFIsType( inValue, CFString ), exit, err = kTypeErr );
		
		cptr = NULL;
		if( inValue && ( CFStringGetLength( (CFStringRef) inValue ) > 0 ) )
		{
			err = CFStringCopyUTF8CString( (CFStringRef) inValue, &cptr );
			require_noerr( err, exit );
		}
		FreeNullSafe( me->password );
		me->password = cptr;
	}
	
	// RFC2617DigestAuth
	
	else if( CFEqual( inProperty, kHTTPClientProperty_RFC2617DigestAuth ) )
	{
		me->rfc2617DigestAuth = CFGetBoolean( inValue, NULL );
	}
	
	// Username
	
	else if( CFEqual( inProperty, kHTTPClientProperty_Username ) )
	{
		require_action( !inValue || CFIsType( inValue, CFString ), exit, err = kTypeErr );
		
		cptr = NULL;
		if( inValue && ( CFStringGetLength( (CFStringRef) inValue ) > 0 ) )
		{
			err = CFStringCopyUTF8CString( (CFStringRef) inValue, &cptr );
			require_noerr( err, exit );
		}
		FreeNullSafe( me->username );
		me->username = cptr;
	}
	
	// Other
	
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPClientSetTimeout
//===========================================================================================================================

void	HTTPClientSetTimeout( HTTPClientRef me, int inSecs )
{
	me->timeoutSecs = inSecs;
}

//===========================================================================================================================
//	HTTPClientSetTransportDelegate
//===========================================================================================================================

void	HTTPClientSetTransportDelegate( HTTPClientRef me, const NetTransportDelegate *inDelegate )
{
	if( me->transportDelegate.finalize_f ) me->transportDelegate.finalize_f( me->transportDelegate.context );
	me->transportDelegate = *inDelegate;
	me->hasTransportDelegate = true;
	
	if( IsValidSocket( me->sock ) && me->transportDelegate.initialize_f )
	{
		me->transportDelegate.initialize_f( me->sock, me->transportDelegate.context );
	}
}

//===========================================================================================================================
//	_HTTPClientRunStateMachine
//===========================================================================================================================

static void	_HTTPClientRunStateMachine( HTTPClientRef me )
{
	OSStatus			err;
	HTTPMessageRef		msg;
	
	for( ;; )
	{
		if( me->state == kHTTPClientStateIdle )
		{
			AsyncConnectionFlags		flags;
			uint64_t					nanos;
			AsyncConnectionParams		params;
			
			msg = me->messageList;
			if( !msg )
			{
				require_action_quiet( !me->detachHandler_f, exit, err = kCanceledErr );
				
				if( me->flags & kHTTPClientFlag_Events )
				{
					if( me->extraDataLen > 0 )
					{
						me->state = kHTTPClientStatePreparingForEvent;
						continue;
					}
					dispatch_resume_if_suspended( me->readSource, &me->readSuspended );
				}
				break;
			}
			
			if( IsValidSocket( me->sock ) )
			{
				if( me->readSource )
				{
					me->state = kHTTPClientStatePreparingRequest;
					continue;
				}
				CFRetain( me );
				_HTTPClientConnectHandler( me->sock, kNoErr, me );
				break;
			}
			
			check( !me->connector );
			flags = 0;
			if( me->flags & kHTTPClientFlag_P2P )					flags |= kAsyncConnectionFlag_P2P;
			if( me->flags & kHTTPClientFlag_SuppressUnusable )		flags |= kAsyncConnectionFlag_SuppressUnusable;
			if( me->flags & kHTTPClientFlag_Reachability )			flags |= kAsyncConnectionFlag_Reachability;
			if( me->flags & kHTTPClientFlag_BoundInterface )		flags |= kAsyncConnectionFlag_BoundInterface;
			if( me->flags & kHTTPClientFlag_NonCellular )			flags |= kAsyncConnectionFlag_NonCellular;
			if( me->flags & kHTTPClientFlag_NonExpensive )			flags |= kAsyncConnectionFlag_NonExpensive;
			if( me->flags & kHTTPClientFlag_NonLinkLocal )			flags |= kAsyncConnectionFlag_NonLinkLocal;
			
			nanos = msg->timeoutNanos;
			if( ( nanos == kHTTPNoTimeout ) && ( msg->connectTimeoutSecs > 0 ) )
			{
				nanos = ( (uint64_t) msg->connectTimeoutSecs ) * kNanosecondsPerSecond;
			}
			if( ( nanos == kHTTPNoTimeout ) && ( me->timeoutSecs > 0 ) )
			{
				nanos = ( (uint64_t) me->timeoutSecs ) * kNanosecondsPerSecond;
			}
			
			AsyncConnectionParamsInit( &params );
			params.destination			= me->destination;
			params.defaultPort			= me->defaultPort;
			params.flags				= flags;
			params.timeoutNanos			= nanos;
			params.socketSendBufferSize	= kSocketBufferSize_DontSet;
			params.socketRecvBufferSize	= kSocketBufferSize_DontSet;
			params.progressFunc			= NULL;
			params.progressArg			= NULL;
			params.handlerFunc			= _HTTPClientConnectHandler;
			params.handlerArg			= me;
			params.handlerQueue			= me->queue;
			params.logCategory			= me->connectionUcat;
			err = AsyncConnection_ConnectEx( &me->connector, &params );
			require_noerr( err, exit );
			
			CFRetain( me );
			me->state = kHTTPClientStateConnecting;
			break;
		}
		else if( me->state == kHTTPClientStateConnecting )
		{
			break;
		}
		else if( me->state == kHTTPClientStatePreparingRequest )
		{
			int		timeoutSecs;
			
			dispatch_source_forget( &me->timerSource );
			msg = me->messageList;
			require_action( msg, exit, err = kInternalErr );
			if( msg->ion == 0 )
			{
				msg->header.statusCode = kHTTPStatus_OK;
				_HTTPClientCompleteMessage( me, msg, kNoErr );
				me->state = kHTTPClientStateIdle;
				continue;
			}
			timeoutSecs = msg->dataTimeoutSecs;
			if( timeoutSecs <= 0 ) timeoutSecs = me->timeoutSecs;
			if( timeoutSecs > 0 )
			{
				me->timerSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, me->queue );
				require_action( me->timerSource, exit, err = kUnknownErr );
				CFRetain( me );
				dispatch_set_context( me->timerSource, me );
				dispatch_source_set_event_handler_f( me->timerSource, _HTTPClientTimerFiredHandler );
				dispatch_source_set_cancel_handler_f( me->timerSource, _HTTPClientTimerCanceledHandler );
				dispatch_source_set_timer( me->timerSource, dispatch_time_seconds( timeoutSecs ), 
					DISPATCH_TIME_FOREVER, 500 * kNanosecondsPerMillisecond );
				dispatch_resume( me->timerSource );
			}
			me->state = kHTTPClientStateWritingRequest;
		}
		else if( me->state == kHTTPClientStateWritingRequest )
		{
			msg = me->messageList;
			require_action( msg, exit, err = kInternalErr );
			err = HTTPMessageWriteMessage( msg, me->transportDelegate.writev_f, me->transportDelegate.context );
			err = _HTTPClientHandleIOError( me, err, false );
			if( err == EWOULDBLOCK ) break;
			require_noerr_quiet( err, exit );
			LogHTTP( me->ucat, me->ucat, msg->header.buf, msg->header.len, msg->bodyPtr, msg->bodyLen );
			if( me->debugDelegate.sendMessage_f )
			{
				me->debugDelegate.sendMessage_f( msg->header.buf, msg->header.len, msg->bodyPtr, msg->bodyLen, 
					me->debugDelegate.context );
			}
			me->state = HTTPMessageIsInterleavedBinary( msg ) ? 
				kHTTPClientStateFinishingMesssage : kHTTPClientStateReadingResponse;
			if( me->allowedAuthSchemes && me->username && me->password )
			{
				err = _HTTPClientSaveRequest( msg );
				require_noerr( err, exit );
			}
			HTTPMessageReset( msg );
		}
		else if( me->state == kHTTPClientStateReadingResponse )
		{
			msg = me->messageList;
			require_action( msg, exit, err = kInternalErr );
			msg->header.extraDataPtr = me->extraDataBuf;
			msg->header.extraDataLen = me->extraDataLen;
			err = HTTPMessageReadMessageEx( msg, me->transportDelegate.read_f, me->transportDelegate.context );
			err = _HTTPClientHandleIOError( me, err, true );
			if( err == EWOULDBLOCK ) break;
			require_noerr_quiet( err, exit );
			memmove( me->extraDataBuf, msg->header.extraDataPtr, msg->header.extraDataLen );
			me->extraDataLen = msg->header.extraDataLen;
			msg->header.extraDataLen = 0;
			
			LogHTTP( me->ucat, me->ucat, msg->header.buf, msg->header.len, msg->bodyPtr, msg->bodyLen );
			if( me->debugDelegate.receiveMessage_f )
			{
				me->debugDelegate.receiveMessage_f( msg->header.buf, msg->header.len, msg->bodyPtr, msg->bodyLen, 
					me->debugDelegate.context );
			}
			if( HTTPMessageIsInterleavedBinary( msg ) )
			{
				if( me->delegate.handleBinary_f )
				{
					me->delegate.handleBinary_f( msg->header.channelID, msg->bodyPtr, msg->bodyLen, me->delegate.context );
				}
				HTTPMessageReset( msg );
				continue;
			}
			else if( ( me->flags & kHTTPClientFlag_Events ) && 
				     ( strncmpx( msg->header.protocolPtr, msg->header.protocolLen, "EVENT/1.0" ) == 0 ) )
			{
				if( me->delegate.handleEvent_f ) me->delegate.handleEvent_f( msg, me->delegate.context );
				HTTPMessageReset( msg );
				continue;
			}
			if( ( msg->header.statusCode == kHTTPStatus_Unauthorized ) && 
				me->allowedAuthSchemes && me->username && me->password )
			{
				err = _HTTPClientPrepareForAuthResend( me, msg );
				if( !err )
				{
					me->state = kHTTPClientStatePreparingRequest;
					continue;
				}
			}
			me->state = kHTTPClientStateFinishingMesssage;
		}
		else if( me->state == kHTTPClientStateFinishingMesssage )
		{
			msg = me->messageList;
			require_action( msg, exit, err = kInternalErr );
			if( msg->closeAfterRequest )
			{
				// Note: the shutdown() is after the response is read because some servers, such as Akamai's, 
				// treat a TCP half-close as a client abort and will disconnect instead of sending a response.
				
				shutdown( me->sock, SHUT_WR_COMPAT );
				me->state = kHTTPClientStateWaitingForClose;
			}
			else
			{
				_HTTPClientCompleteMessage( me, msg, kNoErr );
				dispatch_source_forget( &me->timerSource );
				me->state = kHTTPClientStateIdle;
			}
		}
		else if( me->state == kHTTPClientStateWaitingForClose )
		{
			uint8_t		buf[ 16 ];
			size_t		len;
			
			len = 0;
			err = me->transportDelegate.read_f( buf, sizeof( buf ), &len, me->transportDelegate.context );
			err = _HTTPClientHandleIOError( me, err, true );
			if( err == EWOULDBLOCK ) break;
			if( len > 0 ) ulog( me->ucat, kLogLevelNotice, "### Read %zu bytes after connection close\n", len );
			if( err != kConnectionErr ) ulog( me->ucat, kLogLevelNotice, "### Error on wait for close: %#m\n", err );
			
			msg = me->messageList;
			require_action( msg, exit, err = kInternalErr );
			_HTTPClientCompleteMessage( me, msg, kNoErr );
			_HTTPClientErrorHandler( me, kEndingErr );
			me->state = kHTTPClientStateIdle;
		}
		else if( me->state == kHTTPClientStatePreparingForEvent )
		{
			dispatch_source_forget( &me->timerSource );
			if( me->timeoutSecs > 0 )
			{
				me->timerSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, me->queue );
				require_action( me->timerSource, exit, err = kUnknownErr );
				CFRetain( me );
				dispatch_set_context( me->timerSource, me );
				dispatch_source_set_event_handler_f( me->timerSource, _HTTPClientTimerFiredHandler );
				dispatch_source_set_cancel_handler_f( me->timerSource, _HTTPClientTimerCanceledHandler );
				dispatch_source_set_timer( me->timerSource, dispatch_time_seconds( me->timeoutSecs ), 
					DISPATCH_TIME_FOREVER, 500 * kNanosecondsPerMillisecond );
				dispatch_resume( me->timerSource );
			}
			me->state = kHTTPClientStateReadingEvent;
		}
		else if( me->state == kHTTPClientStateReadingEvent )
		{
			msg = me->eventMsg;
			msg->header.extraDataPtr = me->extraDataBuf;
			msg->header.extraDataLen = me->extraDataLen;
			err = HTTPMessageReadMessageEx( msg, me->transportDelegate.read_f, me->transportDelegate.context );
			err = _HTTPClientHandleIOError( me, err, true );
			if( err == EWOULDBLOCK )
			{
				// Workaround spurious notifications by checking if we read any data and if not, go back to idle.
				
				if( msg->header.len == 0 )
				{
					ulog( me->ucat, kLogLevelInfo, "### Ignoring spurious readability notification\n" );
					dispatch_source_forget( &me->timerSource );
					me->state = kHTTPClientStateIdle;
				}
				break;
			}
			require_noerr_quiet( err, exit );
			memmove( me->extraDataBuf, msg->header.extraDataPtr, msg->header.extraDataLen );
			me->extraDataLen = msg->header.extraDataLen;
			msg->header.extraDataLen = 0;
			
			LogHTTP( me->ucat, me->ucat, msg->header.buf, msg->header.len, msg->bodyPtr, msg->bodyLen );
			if( me->debugDelegate.receiveMessage_f )
			{
				me->debugDelegate.receiveMessage_f( msg->header.buf, msg->header.len, msg->bodyPtr, msg->bodyLen, 
					me->debugDelegate.context );
			}
			if( HTTPMessageIsInterleavedBinary( msg ) )
			{
				if( me->delegate.handleBinary_f )
				{
					me->delegate.handleBinary_f( msg->header.channelID, msg->bodyPtr, msg->bodyLen, me->delegate.context );
				}
			}
			else if( me->delegate.handleEvent_f )
			{
				me->delegate.handleEvent_f( msg, me->delegate.context );
			}
			HTTPMessageReset( msg );
			dispatch_source_forget( &me->timerSource );
			me->state = kHTTPClientStateIdle;
		}
		else if( me->state == kHTTPClientStateError )
		{
			ulog( me->ucat, kLogLevelWarning, "### Running HTTP client in error state for %s\n", me->destination );
			err = kStateErr;
			goto exit;
		}
		else
		{
			ulog( me->ucat, kLogLevelWarning, "### Bad HTTP client state %u for %s\n", me->state, me->destination );
			err = kInternalErr;
			goto exit;
		}
	}
	err = kNoErr;
	
exit:
	if( err ) _HTTPClientErrorHandler( me, err );
}

//===========================================================================================================================
//	_HTTPClientConnectHandler
//===========================================================================================================================

static void	_HTTPClientConnectHandler( SocketRef inSock, OSStatus inError, void *inArg )
{
	HTTPClientRef const		me = (HTTPClientRef) inArg;
	OSStatus				err;
	
	require_noerr_action_quiet( inError, exit, err = inError );
	require_action_quiet( me->messageList, exit, err = kAlreadyCanceledErr );
	
	if( me->transportDelegate.initialize_f )
	{
		err = me->transportDelegate.initialize_f( inSock, me->transportDelegate.context );
		require_noerr( err, exit );
	}
	else if( !me->hasTransportDelegate ) me->transportDelegate.context = (void *)(intptr_t) inSock;
	
	// Configure socket.
	
	if( ( me->keepAliveIdleSecs > 0 ) && ( me->keepAliveMaxUnansweredProbes > 0 ) )
	{
		SocketSetKeepAlive( inSock, me->keepAliveIdleSecs, me->keepAliveMaxUnansweredProbes );
	}
	
	// Set up a source to notify us when the socket is readable.
	
	me->readSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, (uintptr_t) inSock, 0, me->queue );
	require_action( me->readSource, exit, err = kUnknownErr );
	++me->sockRefCount;
	CFRetain( me );
	dispatch_set_context( me->readSource, me );
	dispatch_source_set_event_handler_f( me->readSource, _HTTPClientReadHandler );
	dispatch_source_set_cancel_handler_f( me->readSource, _HTTPClientCancelHandler );
	dispatch_resume( me->readSource );
	
	// Set up a source to notify us when the socket is writable.
	
	me->writeSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_WRITE, (uintptr_t) inSock, 0, me->queue );
	require_action( me->writeSource, exit, err = kUnknownErr );
	++me->sockRefCount;
	CFRetain( me );
	dispatch_set_context( me->writeSource, me );
	dispatch_source_set_event_handler_f( me->writeSource, _HTTPClientWriteHandler );
	dispatch_source_set_cancel_handler_f( me->writeSource, _HTTPClientCancelHandler );
	me->writeSuspended = true; // Don't resume until we get EWOULDBLOCK.
	
	// Set up for reading unsolicited events from the server.
	
	if( me->flags & kHTTPClientFlag_Events )
	{
		err = HTTPMessageCreate( &me->eventMsg );
		require_noerr( err, exit );
	}
	
	me->sock = inSock;
	me->state = kHTTPClientStatePreparingRequest;
	_HTTPClientRunStateMachine( me );
	err = kNoErr;
	
exit:
	if( err )
	{
		ulog( me->ucat, kLogLevelInfo, "### HTTP connect to %s failed: %#m\n", me->destination, err );
		if( me->sockRefCount == 0 ) ForgetSocket( &inSock );
		_HTTPClientErrorHandler( me, err );
	}
	CFRelease( me );
}

//===========================================================================================================================
//	_HTTPClientReadHandler
//===========================================================================================================================

static void	_HTTPClientReadHandler( void *inContext )
{
	HTTPClientRef const		me = (HTTPClientRef) inContext;
	HTTPMessageRef			msg;
	
	check( !me->readSuspended );
	dispatch_suspend( me->readSource ); // Disable readability notification until we get another EWOULDBLOCK.
	me->readSuspended = true;
	
	// Push out timeout when data comes in.
	
	msg = me->messageList;
	if( msg && ( msg->dataTimeoutSecs > 0 ) && me->timerSource )
	{
		dispatch_source_set_timer( me->timerSource, dispatch_time_seconds( msg->dataTimeoutSecs ), 
			DISPATCH_TIME_FOREVER, 500 * kNanosecondsPerMillisecond );
	}
	
	if( ( me->flags & kHTTPClientFlag_Events ) && ( me->state == kHTTPClientStateIdle ) && !msg )
	{
		me->state = kHTTPClientStatePreparingForEvent;
	}
	
	_HTTPClientRunStateMachine( me );
}

//===========================================================================================================================
//	_HTTPClientWriteHandler
//===========================================================================================================================

static void	_HTTPClientWriteHandler( void *inContext )
{
	HTTPClientRef const		me = (HTTPClientRef) inContext;
	HTTPMessageRef			msg;
	
	check( !me->writeSuspended );
	dispatch_suspend( me->writeSource ); // Disable writability notification until we get another EWOULDBLOCK.
	me->writeSuspended = true;
	
	// Push out timeout when data flows out.
	
	msg = me->messageList;
	if( msg && ( msg->dataTimeoutSecs > 0 ) && me->timerSource )
	{
		dispatch_source_set_timer( me->timerSource, dispatch_time_seconds( msg->dataTimeoutSecs ), 
			DISPATCH_TIME_FOREVER, 500 * kNanosecondsPerMillisecond );
	}
	
	_HTTPClientRunStateMachine( me );
}

//===========================================================================================================================
//	_HTTPClientCancelHandler
//===========================================================================================================================

static void	_HTTPClientCancelHandler( void *inContext )
{
	HTTPClientRef const		me = (HTTPClientRef) inContext;
	
	if( --me->sockRefCount == 0 )
	{
		if( me->detachHandler_f )
		{
			check( IsValidSocket( me->sock ) );
			me->detachHandler_f( me->sock, me->context1, me->context2, me->context3 );
			me->sock = kInvalidSocketRef;
		}
		ForgetSocket( &me->sock );
	}
	CFRelease( me );
}

//===========================================================================================================================
//	_HTTPClientErrorHandler
//===========================================================================================================================

static void	_HTTPClientErrorHandler( HTTPClientRef me, OSStatus inError )
{
	HTTPMessageRef				msg;
	HTTPClientInvalidated_f		invalidated_f;
	
	if( inError && ( inError != kCanceledErr ) && ( inError != kEndingErr ) )
	{
		ulog( me->ucat, kLogLevelInfo, "### HTTP client to %s error: %#m\n", me->destination, inError );
	}
	
	me->state = kHTTPClientStateError;
	if( me->connector )
	{
		AsyncConnection_Release( me->connector );
		me->connector = NULL;
	}
	dispatch_source_forget_ex( &me->readSource,  &me->readSuspended );
	dispatch_source_forget_ex( &me->writeSource, &me->writeSuspended );
	dispatch_source_forget( &me->timerSource );
	ForgetCF( &me->eventMsg );
	
	while( ( msg = me->messageList ) != NULL )
	{
		_HTTPClientCompleteMessage( me, msg, inError );
	}
	
	invalidated_f = me->delegate.invalidated_f;
	me->delegate.invalidated_f = NULL;
	if( invalidated_f ) invalidated_f( inError, me->delegate.context );
}

//===========================================================================================================================
//	_HTTPClientTimerFiredHandler
//===========================================================================================================================

static void	_HTTPClientTimerFiredHandler( void *inContext )
{
	HTTPClientRef const		me = (HTTPClientRef) inContext;
	
	ulog( me->ucat, kLogLevelNotice, "### Timeout on HTTP connection to %s\n", me->destination );
	_HTTPClientErrorHandler( me, kTimeoutErr );
}

//===========================================================================================================================
//	_HTTPClientTimerCanceledHandler
//===========================================================================================================================

static void	_HTTPClientTimerCanceledHandler( void *inContext )
{
	HTTPClientRef const		me = (HTTPClientRef) inContext;
	
	CFRelease( me );
}

//===========================================================================================================================
//	_HTTPClientCompleteMessage
//===========================================================================================================================

static void	_HTTPClientCompleteMessage( HTTPClientRef me, HTTPMessageRef inMsg, OSStatus inStatus )
{
	if( ( me->messageList = inMsg->next ) == NULL )
		  me->messageNext = &me->messageList;
	inMsg->status = inStatus;
	if( inMsg->completion ) inMsg->completion( inMsg );
	CFRelease( inMsg );
}

//===========================================================================================================================
//	HTTPClientDetach
//===========================================================================================================================

OSStatus
	HTTPClientDetach( 
		HTTPClientRef				me, 
		HTTPClientDetachHandler_f	inHandler, 
		void *						inContext1, 
		void *						inContext2, 
		void *						inContext3 )
{
	OSStatus		err;
	
	require_action( !me->detachHandler_f, exit, err = kAlreadyInUseErr );
	me->detachHandler_f	= inHandler;
	me->context1		= inContext1;
	me->context2		= inContext2;
	me->context3		= inContext3;
	ulog( me->ucat, kLogLevelTrace, "Detaching client for %s\n", me->destination );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPClientSendBinaryBytes
//===========================================================================================================================

OSStatus
	HTTPClientSendBinaryBytes( 
		HTTPClientRef					me, 
		HTTPMessageFlags				inFlags, 
		uint8_t							inChannelID, 
		const void *					inPtr, 
		size_t							inLen,  
		HTTPMessageBinaryCompletion_f	inCompletion, 
		void *							inContext )
{
	OSStatus			err;
	HTTPMessageRef		msg = NULL;
	uint8_t *			dst;
	
	require_action( inLen <= 0xFFFF, exit, err = kSizeErr );
	
	err = HTTPMessageCreate( &msg );
	require_noerr( err, exit );
	
	if( inFlags & kHTTPMessageFlag_NoCopy )
	{
		msg->bodyPtr = (uint8_t *) inPtr;
		msg->bodyLen = inLen;
	}
	else
	{
		err = HTTPMessageSetBodyLength( msg, inLen );
		require_noerr( err, exit );
		if( inLen > 0 ) memmove( msg->bodyPtr, inPtr, inLen ); // memmove in case inPtr is in the middle of the buffer.
	}
	
	dst = (uint8_t *) msg->header.buf;
	dst[ 0 ] = '$';
	dst[ 1 ] = inChannelID;
	dst[ 2 ] = (uint8_t)( inLen >> 8 );
	dst[ 3 ] = (uint8_t)( inLen & 0xFF );
	msg->header.len	= 4;
	
	if( inCompletion )
	{
		msg->binaryCompletion_f	= inCompletion;
		msg->userContext1		= inContext;
		msg->completion			= _HTTPClientSendBinaryCompletion;
	}
	
	err = HTTPClientSendMessage( me, msg );
	require_noerr( err, exit );
	
exit:
	CFReleaseNullSafe( msg );
	return( err );
}

//===========================================================================================================================
//	_HTTPClientSendBinaryCompletion
//===========================================================================================================================

static void	_HTTPClientSendBinaryCompletion( HTTPMessageRef inMsg )
{
	inMsg->binaryCompletion_f( inMsg->status, inMsg->userContext1 );
}

//===========================================================================================================================
//	HTTPClientSendMessage
//===========================================================================================================================

OSStatus	HTTPClientSendMessage( HTTPClientRef me, HTTPMessageRef inMsg )
{
	OSStatus		err;
	
	if( inMsg->header.len > 0 )
	{
		if( inMsg->closeAfterRequest )
		{
			HTTPHeader_SetField( &inMsg->header, kHTTPHeader_Connection, "close" );
		}
		
		err = HTTPHeader_Commit( &inMsg->header );
		require_noerr( err, exit );
		
		inMsg->iov[ 0 ].iov_base = inMsg->header.buf;
		inMsg->iov[ 0 ].iov_len  = inMsg->header.len;
		inMsg->ion = 1;
		if( inMsg->bodyLen > 0 )
		{
			inMsg->iov[ 1 ].iov_base = (char *) inMsg->bodyPtr;
			inMsg->iov[ 1 ].iov_len  = inMsg->bodyLen;
			inMsg->ion = 2;
		}
		inMsg->iop = inMsg->iov;
	}
	else
	{
		inMsg->ion = 0;
	}
	
	CFRetain( inMsg );
	CFRetain( me );
	inMsg->httpContext1 = me;
	dispatch_async_f( me->queue, inMsg, _HTTPClientSendMessage );
	err = kNoErr;
	
exit:
	return( err );
}

static void	_HTTPClientSendMessage( void *inContext )
{
	HTTPMessageRef const		msg = (HTTPMessageRef) inContext;
	HTTPClientRef const			me  = (HTTPClientRef) msg->httpContext1;
	
	msg->next = NULL;
	*me->messageNext = msg;
	 me->messageNext = &msg->next;
	_HTTPClientRunStateMachine( me );
	CFRelease( me );
}

//===========================================================================================================================
//	HTTPClientSendMessageSync
//===========================================================================================================================

static void	_HTTPClientSendMessageSyncCompletion( HTTPMessageRef inMsg );

OSStatus	HTTPClientSendMessageSync( HTTPClientRef me, HTTPMessageRef inMsg )
{
	OSStatus					err;
	dispatch_semaphore_t		sem;
	
	sem = dispatch_semaphore_create( 0 );
	require_action( sem, exit, err = kNoMemoryErr );
	
	inMsg->httpContext2 = sem;
	inMsg->completion   = _HTTPClientSendMessageSyncCompletion;
	
	err = HTTPClientSendMessage( me, inMsg );
	require_noerr( err, exit );
	
	dispatch_semaphore_wait( sem, DISPATCH_TIME_FOREVER );
	err = inMsg->status;
	if( !err && !IsHTTPStatusCode_Success( inMsg->header.statusCode ) )
	{
		err = HTTPStatusToOSStatus( inMsg->header.statusCode );
	}
	
exit:
	if( sem ) arc_safe_dispatch_release( sem );
	return( err );
}

static void	_HTTPClientSendMessageSyncCompletion( HTTPMessageRef inMsg )
{
	dispatch_semaphore_signal( (dispatch_semaphore_t) inMsg->httpContext2 );
}

//===========================================================================================================================
//	_HTTPClientSaveRequest
//===========================================================================================================================

static OSStatus	_HTTPClientSaveRequest( HTTPMessageRef inMsg )
{
	OSStatus		err;
	
	if( !inMsg->requestHeader )
	{
		inMsg->requestHeader = (HTTPHeader *) calloc( 1, sizeof( *inMsg->requestHeader ) );
		require_action( inMsg->requestHeader, exit, err = kNoMemoryErr );
	}
	memcpy( inMsg->requestHeader->buf, inMsg->header.buf, inMsg->header.len );
	inMsg->requestHeader->len = inMsg->header.len;
	
	ForgetPtrLen( &inMsg->requestBodyPtr, &inMsg->requestBodyLen );
	if( inMsg->bodyLen > 0 )
	{
		if( inMsg->bodyPtr == inMsg->bigBodyBuf )
		{
			inMsg->requestBodyPtr	= inMsg->bodyPtr;
			inMsg->bodyPtr			= NULL;
			inMsg->bigBodyBuf		= NULL;
		}
		else
		{
			inMsg->requestBodyPtr = (uint8_t *) malloc( inMsg->bodyLen );
			require_action( inMsg->requestBodyPtr, exit, err = kNoMemoryErr );
			memcpy( inMsg->requestBodyPtr, inMsg->bodyPtr, inMsg->bodyLen );
		}
		inMsg->requestBodyLen = inMsg->bodyLen;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_HTTPClientPrepareForAuthResend
//===========================================================================================================================

static OSStatus	_HTTPClientPrepareForAuthResend( HTTPClientRef me, HTTPMessageRef inMsg )
{
	OSStatus		err;
	
	require_action( inMsg->requestHeader, exit, err = kInternalErr );
	
	err = HTTPHeader_Parse( inMsg->requestHeader );
	require_noerr( err, exit );
	inMsg->requestHeader->firstErr = kAlreadyInUseErr;
	
	me->httpAuthInfo.allowedAuthSchemes	= me->allowedAuthSchemes;
	me->httpAuthInfo.requestHeader		= inMsg->requestHeader;
	me->httpAuthInfo.responseHeader		= &inMsg->header;
	me->httpAuthInfo.uppercaseHex		= !me->rfc2617DigestAuth;
	me->httpAuthInfo.username			= me->username;
	me->httpAuthInfo.password			= me->password;
	err = HTTPClientAuthorization_Apply( &me->httpAuthInfo );
	require_noerr_quiet( err, exit );
	
	err = HTTPHeader_Commit( inMsg->requestHeader );
	require_noerr( err, exit );
	memcpy( inMsg->header.buf, inMsg->requestHeader->buf, inMsg->requestHeader->len );
	inMsg->header.len = inMsg->requestHeader->len;
	
	FreeNullSafe( inMsg->bigBodyBuf );
	inMsg->bigBodyBuf		= inMsg->requestBodyPtr;
	inMsg->bodyPtr			= inMsg->requestBodyPtr;
	inMsg->bodyLen			= inMsg->requestBodyLen;
	inMsg->requestBodyPtr	= NULL;
	inMsg->requestBodyLen	= 0;
	
	inMsg->iov[ 0 ].iov_base = inMsg->header.buf;
	inMsg->iov[ 0 ].iov_len  = inMsg->header.len;
	inMsg->ion = 1;
	if( inMsg->bodyLen > 0 )
	{
		inMsg->iov[ 1 ].iov_base = (char *) inMsg->bodyPtr;
		inMsg->iov[ 1 ].iov_len  = inMsg->bodyLen;
		inMsg->ion = 2;
	}
	inMsg->iop = inMsg->iov;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_HTTPClientHandleIOError
//===========================================================================================================================

static OSStatus	_HTTPClientHandleIOError( HTTPClientRef me, OSStatus inError, Boolean inRead )
{
	switch( inError )
	{
		case kReadWouldBlockErr:
			dispatch_resume_if_suspended( me->readSource, &me->readSuspended );
			inError = EWOULDBLOCK;
			break;
		
		case kWriteWouldBlockErr:
			dispatch_resume_if_suspended( me->writeSource, &me->writeSuspended );
			inError = EWOULDBLOCK;
			break;
		
		case EWOULDBLOCK:
			if( inRead )	dispatch_resume_if_suspended( me->readSource, &me->readSuspended );
			else			dispatch_resume_if_suspended( me->writeSource, &me->writeSuspended );
			break;
		
		case kWouldBlockErr:
			dispatch_resume_if_suspended( me->readSource, &me->readSuspended );
			dispatch_resume_if_suspended( me->writeSource, &me->writeSuspended );
			inError = EWOULDBLOCK;
			break;
		
		default:
			break;
	}
	return( inError );
}

#if 0
#pragma mark -
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	HTTPClientTest
//===========================================================================================================================

#if( !defined( HTTP_CLIENT_TEST_AUTH ) )
	#define HTTP_CLIENT_TEST_AUTH		0
#endif
#if( HTTP_CLIENT_TEST_AUTH )
	static OSStatus	_HTTPClientTestAuth( void );
#endif

ulog_define( HTTPClientTest, kLogLevelWarning, kLogFlags_Default, "HTTPClient", NULL );

static void	_HTTPClientTestCompletion( HTTPMessageRef inMsg );

OSStatus	HTTPClientTest( void )
{
	OSStatus				err;
	HTTPClientRef			client	= NULL;
	HTTPMessageRef			msg		= NULL;
	HTTPMessageRef			msg2	= NULL;
	dispatch_queue_t		queue	= NULL;
	dispatch_semaphore_t	sem		= NULL;
	
#if( HTTP_CLIENT_TEST_AUTH )
	err = _HTTPClientTestAuth();
	require_noerr( err, exit );
#endif
	
	err = HTTPClientCreate( &client );
	require_noerr( err, exit );
	HTTPClientSetLogging( client, &log_category_from_name( HTTPClientTest ) );
	
#if( HTTP_CLIENT_TEST_GCM )
{
	NetTransportDelegate		delegate;
	
	err = NetTransportGCMConfigure( &delegate, NULL, 
		(const uint8_t *) "0123456789abcdef", (const uint8_t *) "0123456789abcdef", 
		(const uint8_t *) "0123456789ABCDEF", (const uint8_t *) "0123456789ABCDEF" );
	require_noerr( err, exit );
	HTTPClientSetTransportDelegate( client, &delegate );
}
#endif
	
	queue = dispatch_queue_create( "HTTPClientTest", NULL );
	require_action( queue, exit, err = -1 );
	HTTPClientSetDispatchQueue( client, queue );
	
#if( HTTP_CLIENT_TEST_GCM )
	err = HTTPClientSetDestination( client, "127.0.0.1", 8000 );
#else
	err = HTTPClientSetDestination( client, "www.apple.com", 80 );
#endif
	require_noerr( err, exit );
	
	err = HTTPMessageCreate( &msg );
	require_noerr( err, exit );
	
	err = HTTPMessageCreate( &msg2 );
	require_noerr( err, exit );
	
	// Connect-only test.
	
	err = HTTPClientSendMessageSync( client, msg );
	require_noerr( err, exit );
	
	// Simple get.
	
	HTTPHeader_InitRequest( &msg->header, "GET", "/", "HTTP/1.1" );
	HTTPHeader_SetField( &msg->header, kHTTPHeader_Host, "www.apple.com" );
	err = HTTPClientSendMessageSync( client, msg );
	if( IsHTTPOSStatus( err ) ) err = kNoErr;
	require_noerr( err, exit );
	FPrintF( stderr, "\twww.apple.com -> %#m\n", msg->header.statusCode );
	HTTPMessageReset( msg );
	
	// Simple get, close after request.
	
	HTTPHeader_InitRequest( &msg->header, "GET", "/", "HTTP/1.1" );
	HTTPHeader_SetField( &msg->header, kHTTPHeader_Host, "www.apple.com" );
#if( !HTTP_CLIENT_TEST_GCM )
	msg->closeAfterRequest = true;
#endif
	err = HTTPClientSendMessageSync( client, msg );
	if( IsHTTPOSStatus( err ) ) err = kNoErr;
	require_noerr( err, exit );
	FPrintF( stderr, "\twww.apple.com/ (close) -> %#m\n", msg->header.statusCode );
	HTTPMessageReset( msg );
	msg->closeAfterRequest = false;
	
	// Simple get with redirect response.
	
	HTTPHeader_InitRequest( &msg->header, "GET", "/developer", "HTTP/1.1" );
	HTTPHeader_SetField( &msg->header, kHTTPHeader_Host, "www.apple.com" );
	err = HTTPClientSendMessageSync( client, msg );
	if( IsHTTPOSStatus( err ) ) err = kNoErr;
	require_noerr( err, exit );
	FPrintF( stderr, "\twww.apple.com/developer -> %#m\n", msg->header.statusCode );
	HTTPMessageReset( msg );
	HTTPClientForget( &client );
	
	// Chunked get.
	
	err = HTTPClientCreate( &client );
	require_noerr( err, exit );
	HTTPClientSetDispatchQueue( client, queue );
	HTTPClientSetLogging( client, &log_category_from_name( HTTPClientTest ) );
	err = HTTPClientSetDestination( client, "http://httpbin.org", 80 );
	require_noerr( err, exit );
	HTTPHeader_InitRequest( &msg->header, "GET", "/drip?numbytes=400&duration=3&delay=1", "HTTP/1.1" );
	HTTPHeader_SetField( &msg->header, kHTTPHeader_Host, "httpbin.org" );
	err = HTTPClientSendMessageSync( client, msg );
	require_noerr( err, exit );
	require_action( msg->bodyLen == 400, exit, err = kSizeErr );
	FPrintF( stderr, "\thttp://httpbin.org/drip?numbytes=400&duration=3&delay=1 -> %#m\n", err );
	HTTPMessageReset( msg );
	HTTPClientForget( &client );
	
	// Timeout without any data.
	
	err = HTTPClientCreate( &client );
	require_noerr( err, exit );
	HTTPClientSetDispatchQueue( client, queue );
	HTTPClientSetLogging( client, &log_category_from_name( HTTPClientTest ) );
	err = HTTPClientSetDestination( client, "http://httpbin.org", 80 );
	require_noerr( err, exit );
	
	HTTPHeader_InitRequest( &msg->header, "GET", "/delay/10", "HTTP/1.1" );
	HTTPHeader_SetField( &msg->header, kHTTPHeader_Host, "httpbin.org" );
	msg->connectTimeoutSecs	= 3;
	msg->dataTimeoutSecs	= 5;
	
	err = HTTPClientSendMessageSync( client, msg );
	require_action( err == kTimeoutErr, exit, err = kResponseErr );
	FPrintF( stderr, "\thttp://httpbin.org/delay/8 -> %#m\n", err );
	
	HTTPClientForget( &client );
	
	// Timeout with delayed data within timeout period.
	
	err = HTTPClientCreate( &client );
	require_noerr( err, exit );
	HTTPClientSetDispatchQueue( client, queue );
	HTTPClientSetLogging( client, &log_category_from_name( HTTPClientTest ) );
	err = HTTPClientSetDestination( client, "http://httpbin.org", 80 );
	require_noerr( err, exit );
	
	HTTPHeader_InitRequest( &msg->header, "GET", "/delay/3", "HTTP/1.1" );
	HTTPHeader_SetField( &msg->header, kHTTPHeader_Host, "httpbin.org" );
	msg->connectTimeoutSecs	= 3;
	msg->dataTimeoutSecs	= 5;
	
	err = HTTPClientSendMessageSync( client, msg );
	require_noerr( err, exit );
	FPrintF( stderr, "\thttp://httpbin.org/delay/3 -> %#m\n", err );
	
	HTTPClientForget( &client );
	
	// Timeout with no data using only a client-wide timeout.
	
	err = HTTPClientCreate( &client );
	require_noerr( err, exit );
	HTTPClientSetDispatchQueue( client, queue );
	HTTPClientSetLogging( client, &log_category_from_name( HTTPClientTest ) );
	HTTPClientSetTimeout( client, 5 );
	err = HTTPClientSetDestination( client, "http://httpbin.org", 80 );
	require_noerr( err, exit );
	
	HTTPHeader_InitRequest( &msg->header, "GET", "/delay/10", "HTTP/1.1" );
	HTTPHeader_SetField( &msg->header, kHTTPHeader_Host, "httpbin.org" );
	msg->connectTimeoutSecs	= 0;
	msg->dataTimeoutSecs	= 0;
	err = HTTPClientSendMessageSync( client, msg );
	require_action( err == kTimeoutErr, exit, err = kResponseErr );
	FPrintF( stderr, "\thttp://httpbin.org/delay/8 -> %#m\n", err );
	
	HTTPClientForget( &client );
	
	// Back-to-back messages.
	
	sem = dispatch_semaphore_create( 0 );
	require_action( sem, exit, err = kNoMemoryErr );
	
	err = HTTPClientCreate( &client );
	require_noerr( err, exit );
	HTTPClientSetDispatchQueue( client, queue );
	HTTPClientSetLogging( client, &log_category_from_name( HTTPClientTest ) );
	err = HTTPClientSetDestination( client, "www.apple.com", 80 );
	require_noerr( err, exit );
	
	HTTPHeader_InitRequest( &msg->header, "GET", "/", "HTTP/1.1" );
	HTTPHeader_SetField( &msg->header, kHTTPHeader_Host, "www.apple.com" );
	msg->connectTimeoutSecs		= 3;
	msg->dataTimeoutSecs		= 5;
	msg->userContext1			= client;
	msg->userContext2			= sem;
	msg->userContext3			= (void *)(uintptr_t) 1;
	msg->completion   			= _HTTPClientTestCompletion;
	err = HTTPClientSendMessage( client, msg );
	require_noerr( err, exit );
	
	HTTPHeader_InitRequest( &msg2->header, "GET", "/", "HTTP/1.1" );
	HTTPHeader_SetField( &msg2->header, kHTTPHeader_Host, "www.apple.com" );
	msg2->connectTimeoutSecs	= 3;
	msg2->dataTimeoutSecs		= 5;
	msg2->userContext1			= client;
	msg2->userContext2			= sem;
	msg2->userContext3			= (void *)(uintptr_t) 2;
	msg2->completion   			= _HTTPClientTestCompletion;
	err = HTTPClientSendMessage( client, msg2 );
	require_noerr( err, exit );
	
	dispatch_semaphore_wait( sem, DISPATCH_TIME_FOREVER );
	FPrintF( stderr, "\thttp://www.apple.com (back-to-back)\n" );
	
	HTTPClientForget( &client );
	
exit:
	CFReleaseNullSafe( client );
	CFReleaseNullSafe( msg );
	CFReleaseNullSafe( msg2 );
	dispatch_forget( &queue );
	dispatch_forget( &sem );
	printf( "HTTPClientTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

static void	_HTTPClientTestCompletion( HTTPMessageRef inMsg )
{
	HTTPClientRef const		client = (HTTPClientRef) inMsg->userContext1;
	int						num    = (int)(uintptr_t) inMsg->userContext3;
	
	FPrintF( stderr, "\tCompleted %s '%.*s' (%d)\n", client->destination, (int) inMsg->header.urlLen, inMsg->header.urlPtr, num );
	if( num == 2 )
	{
		dispatch_semaphore_signal( (dispatch_semaphore_t) inMsg->userContext2 );
	}
}

#if( HTTP_CLIENT_TEST_AUTH )
//===========================================================================================================================
//	_HTTPClientTestAuth
//===========================================================================================================================

static OSStatus	_HTTPClientTestAuth( void )
{
	OSStatus				err;
	HTTPClientRef			client	= NULL;
	HTTPMessageRef			msg		= NULL;
	dispatch_queue_t		queue;
	
	err = HTTPClientCreate( &client );
	require_noerr( err, exit );
	HTTPClientSetLogging( client, &log_category_from_name( HTTPClientTest ) );
	
	queue = dispatch_queue_create( "HTTPClientTest", NULL );
	require_action( queue, exit, err = -1 );
	HTTPClientSetDispatchQueue( client, queue );
	dispatch_release( queue );
	
	err = HTTPMessageCreate( &msg );
	require_noerr( err, exit );
	
	err = HTTPClientSetDestination( client, "127.0.0.1", 8000 );
	require_noerr( err, exit );
	
	// Password test (no password, no body).
	
	HTTPMessageReset( msg );
	HTTPHeader_InitRequest( &msg->header, "GET", "/none", "HTTP/1.1" );
	err = HTTPClientSendMessageSync( client, msg );
	require_action( err == HTTPStatusToOSStatus( kHTTPStatus_Unauthorized ), exit, err = -1 );
	
	// Password test (no password, with body).
	
	HTTPMessageReset( msg );
	HTTPHeader_InitRequest( &msg->header, "GET", "/none", "HTTP/1.1" );
	HTTPMessageSetBody( msg, "text/plain", "abcd", 4 );
	err = HTTPClientSendMessageSync( client, msg );
	require_action( err == HTTPStatusToOSStatus( kHTTPStatus_Unauthorized ), exit, err = -1 );
	
	// Password test (wrong password, with body).
	
	HTTPMessageReset( msg );
	err = HTTPClientPropertySetUInt64( client, kHTTPClientProperty_AllowedAuthSchemes, kHTTPAuthorizationScheme_Digest );
	require_noerr( err, exit );
	err = HTTPClientPropertySetValue( client, kHTTPClientProperty_Password, CFSTR( "password2" ) );
	require_noerr( err, exit );
	err = HTTPClientPropertySetValue( client, kHTTPClientProperty_Username, CFSTR( "user" ) );
	require_noerr( err, exit );
	HTTPHeader_InitRequest( &msg->header, "GET", "/bad", "HTTP/1.1" );
	HTTPMessageSetBody( msg, "text/plain", "body123", 7 );
	err = HTTPClientSendMessageSync( client, msg );
	require_action( err == HTTPStatusToOSStatus( kHTTPStatus_Unauthorized ), exit, err = -1 );
	
	// Password test (wrong password, no body).
	
	HTTPMessageReset( msg );
	err = HTTPClientPropertySetUInt64( client, kHTTPClientProperty_AllowedAuthSchemes, kHTTPAuthorizationScheme_Digest );
	require_noerr( err, exit );
	err = HTTPClientPropertySetValue( client, kHTTPClientProperty_Password, CFSTR( "password2" ) );
	require_noerr( err, exit );
	err = HTTPClientPropertySetValue( client, kHTTPClientProperty_Username, CFSTR( "user" ) );
	require_noerr( err, exit );
	HTTPHeader_InitRequest( &msg->header, "GET", "/bad", "HTTP/1.1" );
	err = HTTPClientSendMessageSync( client, msg );
	require_action( err == HTTPStatusToOSStatus( kHTTPStatus_Unauthorized ), exit, err = -1 );
	
	// Password test (correct password, with body).
	
	HTTPMessageReset( msg );
	err = HTTPClientPropertySetUInt64( client, kHTTPClientProperty_AllowedAuthSchemes, kHTTPAuthorizationScheme_Digest );
	require_noerr( err, exit );
	err = HTTPClientPropertySetValue( client, kHTTPClientProperty_Password, CFSTR( "password" ) );
	require_noerr( err, exit );
	err = HTTPClientPropertySetValue( client, kHTTPClientProperty_Username, CFSTR( "user" ) );
	require_noerr( err, exit );
	HTTPHeader_InitRequest( &msg->header, "GET", "/good", "HTTP/1.1" );
	HTTPMessageSetBody( msg, "text/plain", "ABC", 3 );
	err = HTTPClientSendMessageSync( client, msg );
	require_noerr( err, exit );
	require_action( MemEqual( msg->bodyPtr, msg->bodyLen, "/good", 5 ), exit, err = -1 );
	
	// Password test (correct password 2, no body).
	
	HTTPMessageReset( msg );
	err = HTTPClientPropertySetUInt64( client, kHTTPClientProperty_AllowedAuthSchemes, kHTTPAuthorizationScheme_Digest );
	require_noerr( err, exit );
	err = HTTPClientPropertySetValue( client, kHTTPClientProperty_Password, CFSTR( "password" ) );
	require_noerr( err, exit );
	err = HTTPClientPropertySetValue( client, kHTTPClientProperty_Username, CFSTR( "user" ) );
	require_noerr( err, exit );
	HTTPHeader_InitRequest( &msg->header, "GET", "/good2", "HTTP/1.1" );
	err = HTTPClientSendMessageSync( client, msg );
	require_noerr( err, exit );
	require_action( MemEqual( msg->bodyPtr, msg->bodyLen, "/good2", 6 ), exit, err = -1 );
	HTTPMessageReset( msg );
	
exit:
	CFReleaseNullSafe( client );
	CFReleaseNullSafe( msg );
	return( err );
}
#endif // HTTP_CLIENT_TEST_AUTH
#endif // !EXCLUDE_UNIT_TESTS
