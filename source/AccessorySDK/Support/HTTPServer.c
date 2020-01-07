/*
	File:    	HTTPServer.c
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
	
	To Do:
	
	- Flag to close the connection after a send has completed (Connection: close in response?).
	- Timeout support for reading and writing over the network and for connections.
	- Allow delegate to have request body read in chunks into delegate-provided buffer(s) for DAAP PUTs.
	- Figure out a way to handle readability while in a writing state. For example, if we get a peer close while waiting
	  for the delegate to process a request, the socket should be closed. Another case is if we support pipelined 
	  requests since we'll then have constant readability hogging the CPU while we're trying to write.
	- Read headers then read body separately via user buffer/callback (RTSP binary data for AirTunes or DAAP PUT).
	- Send response headers then send response body in chunks via callback (DAAP item data or big content blocks).
*/

#include "HTTPServer.h"

#include "CFUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "HTTPMessage.h"
#include "HTTPUtils.h"
#include "NetUtils.h"
#include "RandomNumberUtils.h"
#include <glib.h>
#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

#if( TARGET_OS_POSIX )
	#include <netinet/tcp.h>
#endif

#if( !defined( HTTP_SERVER_TEST_GCM ) )
	#define HTTP_SERVER_TEST_GCM		0
#endif
#if( HTTP_SERVER_TEST_GCM )
	#include "NetTransportGCM.h"
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static void		_HTTPServerGetTypeID( void *inContext );
static void		_HTTPServerFinalize( CFTypeRef inCF );
static OSStatus	_HTTPServerStart( HTTPServerRef me );
static void		_HTTPServerStop( HTTPServerRef me );
static void		_HTTPServerAcceptConnection( void *inContext );
static void		_HTTPServerCloseConnection( HTTPConnectionRef inCnx, void *inContext );
static void		_HTTPServerListenerCanceled( void *inContext );

static void		_HTTPConnectionGetTypeID( void *inContext );
static void		_HTTPConnectionFinalize( CFTypeRef inCF );
static void		_HTTPConnectionReadHandler( void *inContext );
static void		_HTTPConnectionWriteHandler( void *inContext );
static void		_HTTPConnectionCancelHandler( void *inContext );
static void		_HTTPConnectionRunStateMachine( HTTPConnectionRef inCnx );
static OSStatus	_HTTPConnectionHandleIOError( HTTPConnectionRef inCnx, OSStatus inError, Boolean inRead );

static void		_HTTPConnectionSendBinaryCompletion( HTTPMessageRef inMsg );
static void		_HTTPConnectionSendEvent( void *inContext );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static dispatch_once_t			gHTTPServerInitOnce = 0;
static CFTypeID					gHTTPServerTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kHTTPServerClass = 
{
	0,							// version
	"HTTPServer",				// className
	NULL,						// init
	NULL,						// copy
	_HTTPServerFinalize,		// finalize
	NULL,						// equal -- NULL means pointer equality.
	NULL,						// hash  -- NULL means pointer hash.
	NULL,						// copyFormattingDesc
	NULL,						// copyDebugDesc
	NULL,						// reclaim
	NULL						// refcount
};

static dispatch_once_t			gHTTPConnectionInitOnce = 0;
static CFTypeID					gHTTPConnectionTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kHTTPConnectionClass = 
{
	0,							// version
	"HTTPConnection",			// className
	NULL,						// init
	NULL,						// copy
	_HTTPConnectionFinalize,	// finalize
	NULL,						// equal -- NULL means pointer equality.
	NULL,						// hash  -- NULL means pointer hash.
	NULL,						// copyFormattingDesc
	NULL,						// copyDebugDesc
	NULL,						// reclaim
	NULL						// refcount
};

ulog_define( HTTPServerCore, kLogLevelNotice, kLogFlags_Default, "HTTPServer", NULL );

//===========================================================================================================================
//	HTTPServerGetTypeID
//===========================================================================================================================

CFTypeID	HTTPServerGetTypeID( void )
{
	dispatch_once_f( &gHTTPServerInitOnce, NULL, _HTTPServerGetTypeID );
	return( gHTTPServerTypeID );
}

static void _HTTPServerGetTypeID( void *inContext )
{
	(void) inContext;
	
	gHTTPServerTypeID = _CFRuntimeRegisterClass( &kHTTPServerClass );
	check( gHTTPServerTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	HTTPServerCreate
//===========================================================================================================================

OSStatus	HTTPServerCreate( HTTPServerRef *outServer, const HTTPServerDelegate *inDelegate )
{
	OSStatus			err;
	HTTPServerRef		me;
	size_t				extraLen;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (HTTPServerRef) _CFRuntimeCreateInstance( NULL, HTTPServerGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	HTTPServerSetDispatchQueue( me, NULL ); // Default to the main queue.
	me->ucat		= &log_category_from_name( HTTPServerCore );
	me->delegate	= *inDelegate;
	
	if( me->delegate.initializeServer_f )
	{
		err = me->delegate.initializeServer_f( me, me->delegate.context );
		require_noerr( err, exit );
	}
	
	*outServer = me;
	me = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( me );
	return( err );
}

//===========================================================================================================================
//	_HTTPServerFinalize
//===========================================================================================================================

static void	_HTTPServerFinalize( CFTypeRef inCF )
{
	HTTPServerRef const		me = (HTTPServerRef) inCF;
	
	if( me->delegate.finalizeServer_f ) me->delegate.finalizeServer_f( me, me->delegate.context );
	dispatch_forget( &me->queue );
	ForgetMem( &me->password );
	ForgetMem( &me->realm );
}

//===========================================================================================================================
//	HTTPServerSetDispatchQueue
//===========================================================================================================================

void	HTTPServerSetDispatchQueue( HTTPServerRef me, dispatch_queue_t inQueue )
{
	ReplaceDispatchQueue( &me->queue, inQueue );
}

//===========================================================================================================================
//	HTTPServerSetLogging
//===========================================================================================================================

void	HTTPServerSetLogging( HTTPServerRef me, LogCategory *inLogCategory )
{
	me->ucat = inLogCategory;
}

//===========================================================================================================================
//	_HTTPServerControl
//===========================================================================================================================

OSStatus
	_HTTPServerControl( 
		CFTypeRef		inObject, 
		CFObjectFlags	inFlags, 
		CFStringRef		inCommand, 
		CFTypeRef		inQualifier, 
		CFTypeRef		inParams )
{
	HTTPServerRef const		me = (HTTPServerRef) inObject;
	OSStatus				err;
	
	if( 0 ) {}
	
	// StartServer
	
	else if( CFEqual( inCommand, kHTTPServerCommand_StartServer ) )
	{
		err = _HTTPServerStart( me );
		require_noerr( err, exit );
	}
	
	// StopServer
	
	else if( CFEqual( inCommand, kHTTPServerCommand_StopServer ) )
	{
		_HTTPServerStop( me );
	}
	
	// Other
	
	else if( me->delegate.control_f )
	{
		return( me->delegate.control_f( me, inFlags, inCommand, inQualifier, inParams, me->delegate.context ) );
	}
	else
	{
		dlogassert( "Unsupported command %@", inCommand );
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_HTTPServerCopyProperty
//===========================================================================================================================

CFTypeRef
	_HTTPServerCopyProperty( 
		CFTypeRef		inObject, 
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		OSStatus *		outErr )
{
	HTTPServerRef const		me		= (HTTPServerRef) inObject;
	CFTypeRef				value	= NULL;
	OSStatus				err;
	
	if( 0 ) {}
	
	// Other
	
	else if( me->delegate.copyProperty_f )
	{
		return( me->delegate.copyProperty_f( me, inFlags, inProperty, inQualifier, outErr, me->delegate.context ) );
	}
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
//	_HTTPServerSetProperty
//===========================================================================================================================

OSStatus
	_HTTPServerSetProperty( 
		CFTypeRef		inObject, 
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		CFTypeRef		inValue )
{
	HTTPServerRef const		me = (HTTPServerRef) inObject;
	OSStatus				err;
	char *					utf8;
	
	if( 0 ) {}
	
	// P2P
	
	else if( CFEqual( inProperty, kHTTPServerProperty_P2P ) )
	{
		me->allowP2P = CFGetBoolean( inValue, NULL );
		if( me->listenerV4 && IsValidSocket( me->listenerV4->sock ) ) SocketSetP2P( me->listenerV4->sock, me->allowP2P );
		if( me->listenerV6 && IsValidSocket( me->listenerV6->sock ) ) SocketSetP2P( me->listenerV6->sock, me->allowP2P );
	}
	
	// Password
	
	else if( CFEqual( inProperty, kHTTPServerProperty_Password ) )
	{
		require_action( !inValue || CFIsType( inValue, CFString ), exit, err = kTypeErr );
		
		utf8 = NULL;
		if( inValue && ( CFStringGetLength( (CFStringRef) inValue ) > 0 ) )
		{
			err = CFStringCopyUTF8CString( (CFStringRef) inValue, &utf8 );
			require_noerr( err, exit );
		}
		if( me->password ) free( me->password );
		me->password = utf8;
	}
	
	// Realm
	
	else if( CFEqual( inProperty, kHTTPServerProperty_Realm ) )
	{
		require_action( !inValue || CFIsType( inValue, CFString ), exit, err = kTypeErr );
		
		utf8 = NULL;
		if( inValue && ( CFStringGetLength( (CFStringRef) inValue ) > 0 ) )
		{
			err = CFStringCopyUTF8CString( (CFStringRef) inValue, &utf8 );
			require_noerr( err, exit );
		}
		if( me->realm ) free( me->realm );
		me->realm = utf8;
	}
	
	// Other
	
	else if( me->delegate.setProperty_f )
	{
		return( me->delegate.setProperty_f( me, inFlags, inProperty, inQualifier, inValue, me->delegate.context ) );
	}
	else
	{
		dlogassert( "Set of unsupported property %@", inProperty );
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_HTTPServerStart
//===========================================================================================================================

static OSStatus	_HTTPServerStart( HTTPServerRef me )
{
	OSStatus					err;
	SocketRef					sockV4 = kInvalidSocketRef;
	SocketRef					sockV6 = kInvalidSocketRef;
	HTTPListenerContext *		listener;
	dispatch_source_t			source;
	
	// Set up listener sockets.
	
	err = TCPServerSocketPairOpen( me->listenPort, &me->listeningPort, kSocketBufferSize_DontSet, &sockV4, &sockV6 );
	require_noerr( err, exit );
	if( IsValidSocket( sockV4 ) )
	{
		if( me->allowP2P ) SocketSetP2P( sockV4, true );
		
		listener = (HTTPListenerContext *) calloc( 1, sizeof( *listener ) );
		require_action( listener, exit, err = kNoMemoryErr );
		
		source = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, (uintptr_t) sockV4, 0, me->queue );
		if( !source ) free( listener );
		require_action( source, exit, err = kUnknownErr );
		
		CFRetain( me );
		listener->source	= source;
		listener->sock		= sockV4;
		listener->server	= me;
		me->listenerV4		= listener;
		dispatch_set_context( source, listener );
		dispatch_source_set_event_handler_f( source, _HTTPServerAcceptConnection );
		dispatch_source_set_cancel_handler_f( source, _HTTPServerListenerCanceled );
		dispatch_resume( source );
		sockV4 = kInvalidSocketRef;
	}
	if( IsValidSocket( sockV6 ) )
	{
		if( me->allowP2P ) SocketSetP2P( sockV6, true );
		
		listener = (HTTPListenerContext *) calloc( 1, sizeof( *listener ) );
		require_action( listener, exit, err = kNoMemoryErr );
		
		source = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, (uintptr_t) sockV6, 0, me->queue );
		if( !source ) free( listener );
		require_action( source, exit, err = kUnknownErr );
		
		CFRetain( me );
		listener->source	= source;
		listener->sock		= sockV6;
		listener->server	= me;
		me->listenerV6		= listener;
		dispatch_set_context( source, listener );
		dispatch_source_set_event_handler_f( source, _HTTPServerAcceptConnection );
		dispatch_source_set_cancel_handler_f( source, _HTTPServerListenerCanceled );
		dispatch_resume( source );
		sockV6 = kInvalidSocketRef;
	}
	
	// Start the delegate.
	
	if( me->delegate.control_f )
	{
		err = me->delegate.control_f( me, 0, kHTTPServerCommand_StartServer, NULL, NULL, me->delegate.context );
		require_noerr( err, exit );
	}
	
	CFRetain( me );
	me->started = true;
	http_server_ulog( me, kLogLevelInfo, "Listening on port %d\n", me->listeningPort );
	
exit:
	ForgetSocket( &sockV4 );
	ForgetSocket( &sockV6 );
	if( err ) _HTTPServerStop( me );
	return( err );
}

//===========================================================================================================================
//	_HTTPServerStop
//===========================================================================================================================

static void	_HTTPServerStop( HTTPServerRef me )
{
	if( me->listenerV4 )
	{
		dispatch_source_forget( &me->listenerV4->source );
		me->listenerV4 = NULL;
	}
	if( me->listenerV6 )
	{
		dispatch_source_forget( &me->listenerV6->source );
		me->listenerV6 = NULL;
	}
	while( me->connections )
	{
		_HTTPServerCloseConnection( me->connections, me->connections->delegate.context );
	}
	if( me->started )
	{
		if( me->delegate.control_f )
		{
			me->delegate.control_f( me, 0, kHTTPServerCommand_StopServer, NULL, NULL, me->delegate.context );
		}
		me->started = false;
		CFRelease( me );
	}
}

//===========================================================================================================================
//	_HTTPServerAcceptConnection
//===========================================================================================================================

static void	_HTTPServerAcceptConnection( void *inContext )
{
	HTTPListenerContext * const		listener	= (HTTPListenerContext *) inContext;
	HTTPServerRef const				server		= listener->server;
	OSStatus						err;
	SocketRef						newSock		= kInvalidSocketRef;
	HTTPConnectionDelegate			delegate;
	HTTPConnectionRef				cnx			= NULL;
	
	newSock = accept( listener->sock, NULL, NULL );
	err = map_socket_creation_errno( newSock );
	require_noerr( err, exit );
	
	HTTPConnectionDelegateInit( &delegate );
	delegate.context			= server->delegate.context;
	delegate.extraLen			= server->delegate.connectionExtraLen;
	delegate.httpProtocol		= server->delegate.httpProtocol;
	delegate.initialize_f		= server->delegate.initializeConnection_f;
	delegate.finalize_f			= server->delegate.finalizeConnection_f;
	delegate.requiresAuth_f		= server->delegate.requiresAuth_f;
	delegate.handleMessage_f	= server->delegate.handleMessage_f;
	delegate.initResponse_f		= server->delegate.initResponse_f;
	
	err = HTTPConnectionCreate( &cnx, &delegate, newSock, server );
	require_noerr( err, exit );
	newSock = kInvalidSocketRef;
	
	cnx->close_f = _HTTPServerCloseConnection;
	HTTPConnectionSetDispatchQueue( cnx, server->queue );
	
	cnx->next = server->connections;
	server->connections = cnx;
	
	err = HTTPConnectionStart( cnx );
	require_noerr( err, exit );
	
	http_server_ulog( server, kLogLevelTrace, "Accepted connection from %##a to %##a\n", &cnx->peerAddr, &cnx->selfAddr );
	cnx = NULL;
	
exit:
	ForgetSocket( &newSock );
	if( cnx ) _HTTPServerCloseConnection( cnx, cnx->delegate.context );
	if( err ) http_server_ulog( server, kLogLevelWarning, "### Accept connection failed: %#m\n", err );
}

//===========================================================================================================================
//	_HTTPServerCloseConnection
//===========================================================================================================================

static void	_HTTPServerCloseConnection( HTTPConnectionRef inCnx, void *inContext )
{
	HTTPServerRef const		server = inCnx->server;
	HTTPConnectionRef *		next;
	HTTPConnectionRef		curr;
	
	(void) inContext;
	
	for( next = &server->connections; ( curr = *next ) != NULL; next = &curr->next )
	{
		if( curr == inCnx )
		{
			*next = curr->next;
			break;
		}
	}
	HTTPConnectionStop( inCnx );
	if( ( inCnx->selfAddr.sa.sa_family != AF_UNSPEC ) && !inCnx->detachHandler_f )
	{
		http_server_ulog( server, kLogLevelTrace, "Closing  connection from %##a to %##a\n", 
			&inCnx->peerAddr, &inCnx->selfAddr );
	}
	CFRelease( inCnx );
}

//===========================================================================================================================
//	_HTTPServerListenerCanceled
//===========================================================================================================================

static void	_HTTPServerListenerCanceled( void *inContext )
{
	HTTPListenerContext * const		listener = (HTTPListenerContext *) inContext;
	
	ForgetSocket( &listener->sock );
	CFRelease( listener->server );
	free( listener );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	HTTPConnectionGetTypeID
//===========================================================================================================================

CFTypeID	HTTPConnectionGetTypeID( void )
{
	dispatch_once_f( &gHTTPConnectionInitOnce, NULL, _HTTPConnectionGetTypeID );
	return( gHTTPConnectionTypeID );
}

static void _HTTPConnectionGetTypeID( void *inContext )
{
	(void) inContext;
	
	gHTTPConnectionTypeID = _CFRuntimeRegisterClass( &kHTTPConnectionClass );
	check( gHTTPConnectionTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	HTTPConnectionCreate
//===========================================================================================================================

OSStatus
	HTTPConnectionCreate( 
		HTTPConnectionRef *				outCnx, 
		const HTTPConnectionDelegate *	inDelegate, 
		SocketRef						inSock, 
		HTTPServerRef					inServer )
{
	OSStatus				err;
	HTTPConnectionRef		cnx;
	size_t					extraLen;
	
	extraLen = ( sizeof( *cnx ) + inDelegate->extraLen ) - sizeof( cnx->base );
	cnx = (HTTPConnectionRef) _CFRuntimeCreateInstance( NULL, HTTPConnectionGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( cnx, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) cnx ) + sizeof( cnx->base ), 0, extraLen );
	
	HTTPConnectionSetDelegate( cnx, inDelegate );
	cnx->ucat = inServer ? inServer->ucat : &log_category_from_name( HTTPServerCore );
	cnx->sock = kInvalidSocketRef;
	cnx->transportDelegate.read_f	= SocketTransportRead;
	cnx->transportDelegate.writev_f	= SocketTransportWriteV;
	
	if( inServer )
	{
		CFRetain( inServer );
		cnx->server = inServer;
	}
	
	err = HTTPMessageCreate( &cnx->requestMsg );
	require_noerr( err, exit );
	
	err = HTTPMessageCreate( &cnx->responseMsg );
	require_noerr( err, exit );
	
	cnx->eventNext = &cnx->eventList;
	
	cnx->sock = inSock;
	if( inServer && inServer->delegate.connectionCreated_f )
	{
		void *		extraPtr;
		
		extraPtr = ( cnx->delegate.extraLen > 0 ) ? ( ( (uint8_t *) cnx ) + sizeof( struct HTTPConnectionPrivate ) ) : NULL;
		inServer->delegate.connectionCreated_f( inServer, cnx, extraPtr, inServer->delegate.context );
	}
	if( cnx->delegate.initialize_f )
	{
		err = cnx->delegate.initialize_f( cnx, cnx->delegate.context );
		if( err ) cnx->sock = kInvalidSocketRef;
		require_noerr( err, exit );
	}
	
	*outCnx = cnx;
	cnx = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( cnx );
	return( err );
}

//===========================================================================================================================
//	_HTTPConnectionFinalize
//===========================================================================================================================

static void	_HTTPConnectionFinalize( CFTypeRef inCF )
{
	HTTPConnectionRef const		cnx = (HTTPConnectionRef) inCF;
	HTTPMessageRef				msg;
	
	if( cnx->delegate.finalize_f ) cnx->delegate.finalize_f( cnx, cnx->delegate.context );
	ForgetCF( &cnx->server );
	dispatch_forget( &cnx->queue );
	check( cnx->readSource == NULL );
	check( cnx->writeSource == NULL );
	ForgetCF( &cnx->requestMsg );
	ForgetCF( &cnx->responseMsg );
	while( ( msg = cnx->eventList ) != NULL )
	{
		cnx->eventList = msg->next;
		CFRelease( msg );
	}
	if( cnx->transportDelegate.finalize_f ) cnx->transportDelegate.finalize_f( cnx->transportDelegate.context );
	if( cnx->detachHandler_f )
	{
		check( IsValidSocket( cnx->sock ) );
		cnx->detachHandler_f( cnx->sock, cnx->context1, cnx->context2, cnx->context3 );
		cnx->sock = kInvalidSocketRef;
	}
	ForgetSocket( &cnx->sock );
}

//===========================================================================================================================
//	HTTPConnectionSetDelegate
//===========================================================================================================================

void	HTTPConnectionSetDelegate( HTTPConnectionRef inCnx, const HTTPConnectionDelegate *inDelegate )
{
	inCnx->delegate = *inDelegate;
	if( !inCnx->delegate.httpProtocol ) inCnx->delegate.httpProtocol = "HTTP/1.1";
}

//===========================================================================================================================
//	HTTPConnectionSetDispatchQueue
//===========================================================================================================================

void	HTTPConnectionSetDispatchQueue( HTTPConnectionRef inCnx, dispatch_queue_t inQueue )
{
	ReplaceDispatchQueue( &inCnx->queue, inQueue );
}

//===========================================================================================================================
//	HTTPConnectionStart
//===========================================================================================================================

OSStatus	HTTPConnectionStart( HTTPConnectionRef inCnx )
{
	OSStatus		err;
	socklen_t		len;
	int				option;
	
	if( !inCnx->queue ) HTTPConnectionSetDispatchQueue( inCnx, dispatch_get_main_queue() );
	
	// Disable SIGPIPE on this socket so we get EPIPE errors instead of terminating the process.
	
#if( defined( SO_NOSIGPIPE ) )
	setsockopt( inCnx->sock, SOL_SOCKET, SO_NOSIGPIPE, &(int){ 1 }, (socklen_t) sizeof( int ) );
#endif
	
	err = SocketMakeNonBlocking( inCnx->sock );
	check_noerr( err );
	
	// Get addresses for both sides and interface info.
	
	len = (socklen_t) sizeof( inCnx->selfAddr );
	err = getsockname( inCnx->sock, &inCnx->selfAddr.sa, &len );
	err = map_socket_noerr_errno( inCnx->sock, err );
	check_noerr( err );
	
	len = (socklen_t) sizeof( inCnx->peerAddr );
	err = getpeername( inCnx->sock, &inCnx->peerAddr.sa, &len );
	err = map_socket_noerr_errno( inCnx->sock, err );
	check_noerr( err );
	
#if( TARGET_OS_POSIX )
	SocketGetInterfaceInfo( inCnx->sock, NULL, inCnx->ifName, &inCnx->ifIndex, inCnx->ifMACAddress, &inCnx->ifMedia, 
		&inCnx->ifFlags, &inCnx->ifExtendedFlags, NULL, &inCnx->transportType );
#endif
	if( !NetTransportTypeIsP2P( inCnx->transportType ) ) SocketSetP2P( inCnx->sock, false ); // Clear if P2P was inherited.
	
	// Disable nagle so responses we send are not delayed. We coalesce writes to minimize small writes anyway.
	
	option = 1;
	err = setsockopt( inCnx->sock, IPPROTO_TCP, TCP_NODELAY, (char *) &option, (socklen_t) sizeof( option ) );
	err = map_socket_noerr_errno( inCnx->sock, err );
	check_noerr( err );
	
	// Set up the transport.
	
	if( inCnx->transportDelegate.initialize_f )
	{
		err = inCnx->transportDelegate.initialize_f( inCnx->sock, inCnx->transportDelegate.context );
		require_noerr( err, exit );
	}
	else if( !inCnx->hasTransportDelegate ) inCnx->transportDelegate.context = (void *)(intptr_t) inCnx->sock;
	
	// Set up a source to notify us when the socket is readable.
	
	inCnx->readSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, (uintptr_t) inCnx->sock, 0, inCnx->queue );
	require_action( inCnx->readSource, exit, err = kUnknownErr );
	CFRetain( inCnx );
	dispatch_set_context( inCnx->readSource, inCnx );
	dispatch_source_set_event_handler_f( inCnx->readSource, _HTTPConnectionReadHandler );
	dispatch_source_set_cancel_handler_f( inCnx->readSource, _HTTPConnectionCancelHandler );
	dispatch_resume( inCnx->readSource );
	
	// Set up a source to notify us when the socket is writable.
	
	inCnx->writeSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_WRITE, (uintptr_t) inCnx->sock, 0, inCnx->queue );
	require_action( inCnx->writeSource, exit, err = kUnknownErr );
	CFRetain( inCnx );
	dispatch_set_context( inCnx->writeSource, inCnx );
	dispatch_source_set_event_handler_f( inCnx->writeSource, _HTTPConnectionWriteHandler );
	dispatch_source_set_cancel_handler_f( inCnx->writeSource, _HTTPConnectionCancelHandler );
	inCnx->writeSuspended = true; // Don't resume until we get EWOULDBLOCK.
	
exit:
	if( err ) HTTPConnectionStop( inCnx );
	return( err );
}

//===========================================================================================================================
//	HTTPConnectionStop
//===========================================================================================================================

void	HTTPConnectionStop( HTTPConnectionRef inCnx )
{
	Boolean		started;
	
	started = inCnx->readSource && inCnx->writeSource;
	if( IsValidSocket( inCnx->sock ) && !inCnx->detachHandler_f ) shutdown( inCnx->sock, SHUT_WR_COMPAT );
	dispatch_source_forget_ex( &inCnx->readSource,  &inCnx->readSuspended );
	dispatch_source_forget_ex( &inCnx->writeSource, &inCnx->writeSuspended );
	if( started && inCnx->delegate.close_f ) inCnx->delegate.close_f( inCnx, inCnx->delegate.context );
}

//===========================================================================================================================
//	HTTPConnectionDetach
//===========================================================================================================================

OSStatus
	HTTPConnectionDetach( 
		HTTPConnectionRef				inCnx, 
		HTTPConnectionDetachHandler_f	inHandler, 
		void *							inContext1, 
		void *							inContext2, 
		void *							inContext3 )
{
	OSStatus		err;
	
	require_action( !inCnx->detachHandler_f, exit, err = kAlreadyInUseErr );
	inCnx->detachHandler_f	= inHandler;
	inCnx->context1			= inContext1;
	inCnx->context2			= inContext2;
	inCnx->context3			= inContext3;
	http_server_ulog( inCnx->server, kLogLevelTrace, "Detaching connection %##a -> %##a\n", &inCnx->peerAddr, &inCnx->selfAddr );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPConnectionSetTransportDelegate
//===========================================================================================================================

void	HTTPConnectionSetTransportDelegate( HTTPConnectionRef inCnx, const NetTransportDelegate *inDelegate )
{
	if( inCnx->transportDelegate.finalize_f ) inCnx->transportDelegate.finalize_f( inCnx->transportDelegate.context );
	inCnx->transportDelegate = *inDelegate;
	inCnx->hasTransportDelegate = true;
	
	if( IsValidSocket( inCnx->sock ) && inCnx->transportDelegate.initialize_f )
	{
		inCnx->transportDelegate.initialize_f( inCnx->sock, inCnx->transportDelegate.context );
	}
}

//===========================================================================================================================
//	_HTTPConnectionReadHandler
//===========================================================================================================================

static void	_HTTPConnectionReadHandler( void *inContext )
{
	HTTPConnectionRef const		cnx = (HTTPConnectionRef) inContext;
	
	check( !cnx->readSuspended );
	dispatch_suspend( cnx->readSource ); // Disable readability notification until we get another EWOULDBLOCK.
	cnx->readSuspended = true;
	
	_HTTPConnectionRunStateMachine( cnx );
}

//===========================================================================================================================
//	_HTTPConnectionWriteHandler
//===========================================================================================================================

static void	_HTTPConnectionWriteHandler( void *inContext )
{
	HTTPConnectionRef const		cnx = (HTTPConnectionRef) inContext;
	
	check( !cnx->writeSuspended );
	dispatch_suspend( cnx->writeSource ); // Disable writability notification until we get another EWOULDBLOCK.
	cnx->writeSuspended = true;
	
	_HTTPConnectionRunStateMachine( cnx );
}

//===========================================================================================================================
//	_HTTPConnectionCancelHandler
//===========================================================================================================================

static void	_HTTPConnectionCancelHandler( void *inContext )
{
	HTTPConnectionRef const		cnx = (HTTPConnectionRef) inContext;
	
	CFRelease( cnx );
}

//===========================================================================================================================
//	_HTTPConnectionRunStateMachine
//===========================================================================================================================

static void	_HTTPConnectionRunStateMachine( HTTPConnectionRef inCnx )
{
	OSStatus			err;
	HTTPMessageRef		msg;
	
	for( ;; )
	{
		if( inCnx->state == kHTTPConnectionStateReadingRequest )
		{
			msg = inCnx->requestMsg;
			err = HTTPMessageReadMessageEx( msg, inCnx->transportDelegate.read_f, inCnx->transportDelegate.context );
			err = _HTTPConnectionHandleIOError( inCnx, err, true );
			if( err == EWOULDBLOCK ) break;
			require_noerr_quiet( err, exit );
			LogHTTP( inCnx->ucat, inCnx->ucat, msg->header.buf, msg->header.len, msg->bodyPtr, msg->bodyLen );
			
			if( HTTPMessageIsInterleavedBinary( msg ) )
			{
				err = inCnx->delegate.handleBinary_f( inCnx, msg->header.channelID, msg->bodyPtr, msg->bodyLen, 
					inCnx->delegate.context );
				require_noerr_quiet( err, exit );
				HTTPMessageReset( inCnx->requestMsg );
			}
			else
			{
				err = inCnx->delegate.handleMessage_f( inCnx, msg, inCnx->delegate.context );
				require_noerr_quiet( err, exit );
				if( inCnx->state == kHTTPConnectionStateReadingRequest )
				{
					inCnx->state = kHTTPConnectionStateProcessingRequest;
					break;
				}
			}
		}
		else if( inCnx->state == kHTTPConnectionStateProcessingRequest )
		{

			break;
		}
		else if( inCnx->state == kHTTPConnectionStateWritingResponse )
		{
			msg = inCnx->responseMsg;
			err = HTTPMessageWriteMessage( msg, inCnx->transportDelegate.writev_f, inCnx->transportDelegate.context );
			err = _HTTPConnectionHandleIOError( inCnx, err, false );
			if( err == EWOULDBLOCK ) break;
			require_noerr_quiet( err, exit );
			if( inCnx->responseMsg->completion )
			{
				inCnx->responseMsg->completion( inCnx->responseMsg );
				inCnx->responseMsg->completion = NULL;
			}
			require_action_quiet( inCnx->requestMsg->header.persistent && !inCnx->detachHandler_f, exit, err = kEndingErr );
			
			HTTPMessageReset( inCnx->requestMsg );
			HTTPMessageReset( inCnx->responseMsg );
			inCnx->state = kHTTPConnectionStatePreparingEvent;
		}
		else if( inCnx->state == kHTTPConnectionStatePreparingEvent )
		{

			msg = inCnx->eventList;
			if( !msg )
			{
				inCnx->state = kHTTPConnectionStateReadingRequest;
				continue;
			}
			
			LogHTTP( inCnx->ucat, inCnx->ucat, msg->header.buf, msg->header.len, msg->bodyPtr, msg->bodyLen );
			inCnx->state = kHTTPConnectionStateWritingEvent;
		}
		else if( inCnx->state == kHTTPConnectionStateWritingEvent )
		{

			msg = inCnx->eventList;
			err = HTTPMessageWriteMessage( msg, inCnx->transportDelegate.writev_f, inCnx->transportDelegate.context );
			err = _HTTPConnectionHandleIOError( inCnx, err, false );
			if( err == EWOULDBLOCK ) break;
			require_noerr_quiet( err, exit );
			
			if( ( inCnx->eventList = msg->next ) == NULL )
				  inCnx->eventNext = &inCnx->eventList;
			#if( COMPILER_HAS_BLOCKS )
			if( msg->completionBlock ) msg->completionBlock( msg );
			else
			#endif
			if( msg->completion ) msg->completion( msg );
			HTTPMessageReset( msg );
			CFRelease( msg );
			inCnx->state = kHTTPConnectionStatePreparingEvent;
		}
		else
		{

			dlogassert( "Bad state %d", inCnx->state );
			err = kInternalErr;
			goto exit;
		}
	}
	err = kNoErr;
	
exit:
	if( err )
	{
		HTTPConnectionStop( inCnx );
		if( inCnx->close_f ) inCnx->close_f( inCnx, inCnx->delegate.context );
	}
}

//===========================================================================================================================
//	_HTTPConnectionHandleIOError
//===========================================================================================================================

static OSStatus	_HTTPConnectionHandleIOError( HTTPConnectionRef inCnx, OSStatus inError, Boolean inRead )
{
	switch( inError )
	{
		case kReadWouldBlockErr:
			dispatch_resume_if_suspended( inCnx->readSource, &inCnx->readSuspended );
			inError = EWOULDBLOCK;
			break;
		
		case kWriteWouldBlockErr:
			dispatch_resume_if_suspended( inCnx->writeSource, &inCnx->writeSuspended );
			inError = EWOULDBLOCK;
			break;
		
		case EWOULDBLOCK:
			if( inRead )	dispatch_resume_if_suspended( inCnx->readSource, &inCnx->readSuspended );
			else			dispatch_resume_if_suspended( inCnx->writeSource, &inCnx->writeSuspended );
			break;
		
		case kWouldBlockErr:
			dispatch_resume_if_suspended( inCnx->readSource, &inCnx->readSuspended );
			dispatch_resume_if_suspended( inCnx->writeSource, &inCnx->writeSuspended );
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

//===========================================================================================================================
//	HTTPConnectionGetNextURLSegmentEx
//===========================================================================================================================

Boolean
	HTTPConnectionGetNextURLSegmentEx( 
		HTTPConnectionRef	inCnx, 
		HTTPMessageRef		inMsg, 
		Boolean				inDontSendResponse, 
		const char **		outPtr, 
		size_t *			outLen, 
		OSStatus *			outErr )
{
	HTTPHeader * const		hdr = &inMsg->header;
	Boolean					good;
	OSStatus				err;
	const char *			src;
	const char *			ptr;
	const char *			end;
	
	src = hdr->url.segmentPtr;
	end = hdr->url.segmentEnd;
	for( ptr = src; ( ptr < end ) && ( *ptr != '/' ); ++ptr ) {}
	good = (Boolean)( ptr != src );
	if( good )
	{
		*outPtr = src;
		*outLen = (size_t)( ptr - src );
		hdr->url.segmentPtr = ( ptr < end ) ? ( ptr + 1 ) : ptr;
	}
	else if( !inDontSendResponse )
	{
		ulog( inCnx->ucat, kLogLevelWarning, "### Bad URL segment: '%.*s'\n", (int) hdr->urlLen, hdr->urlPtr );
		err = HTTPConnectionSendStatusResponse( inCnx, kHTTPStatus_BadRequest, kPathErr );
		require_noerr( err, exit );
	}
	err = kNoErr;
	
exit:
	*outErr = err;
	return( good );
}

//===========================================================================================================================
//	HTTPConnectionInitResponse
//===========================================================================================================================

OSStatus	HTTPConnectionInitResponse( HTTPConnectionRef inCnx, HTTPStatus inStatusCode, OSStatus inError )
{
	return( HTTPConnectionInitResponseEx( inCnx, NULL, inStatusCode, inError ) );
}

//===========================================================================================================================
//	HTTPConnectionInitResponseEx
//===========================================================================================================================

OSStatus	HTTPConnectionInitResponseEx( HTTPConnectionRef inCnx, const char *inProtocol, HTTPStatus inStatusCode, OSStatus inError )
{
	OSStatus			err;
	char				str[ 64 ];
	const char *		ptr;
	
	HTTPHeader_InitResponseEx( &inCnx->responseMsg->header, inProtocol ? inProtocol : inCnx->delegate.httpProtocol, 
		inStatusCode, NULL, inError );
	ptr = HTTPMakeDateString( time( NULL ), str, sizeof( str ) );
	if( *ptr != '\0' ) HTTPHeader_SetField( &inCnx->responseMsg->header, kHTTPHeader_Date, "%s", ptr );
	if( inCnx->delegate.initResponse_f )
	{
		err = inCnx->delegate.initResponse_f( inCnx, inCnx->responseMsg, inCnx->delegate.context );
		require_noerr( err, exit );
	}
	inCnx->responseMsg->bodyLen = 0;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPConnectionSendBinaryBytes
//===========================================================================================================================

OSStatus
	HTTPConnectionSendBinaryBytes( 
		HTTPConnectionRef				inCnx, 
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
		msg->completion			= _HTTPConnectionSendBinaryCompletion;
	}
	
	err = HTTPConnectionSendEvent( inCnx, msg );
	require_noerr( err, exit );
	
exit:
	CFReleaseNullSafe( msg );
	return( err );
}

//===========================================================================================================================
//	_HTTPConnectionSendBinaryCompletion
//===========================================================================================================================

static void	_HTTPConnectionSendBinaryCompletion( HTTPMessageRef inMsg )
{
	inMsg->binaryCompletion_f( inMsg->status, inMsg->userContext1 );
}

//===========================================================================================================================
//	HTTPConnectionSendEvent
//===========================================================================================================================

OSStatus	HTTPConnectionSendEvent( HTTPConnectionRef inCnx, HTTPMessageRef inMsg )
{
	OSStatus		err;
	
	err = HTTPHeader_Commit( &inMsg->header );
	require_noerr( err, exit );
	
	inMsg->httpContext1 = inCnx;
	CFRetain( inCnx );
	CFRetain( inMsg );
	dispatch_async_f( inCnx->queue, inMsg, _HTTPConnectionSendEvent );
	
exit:
	return( err );
}

static void	_HTTPConnectionSendEvent( void *inContext )
{
	HTTPMessageRef const		msg = (HTTPMessageRef) inContext;
	HTTPConnectionRef const		cnx = (HTTPConnectionRef) msg->httpContext1;
	
	msg->next = NULL;
	msg->iov[ 0 ].iov_base = msg->header.buf;
	msg->iov[ 0 ].iov_len  = msg->header.len;
	msg->ion = 1;
	if( msg->bodyLen > 0 )
	{
		msg->iov[ 1 ].iov_base = msg->bodyPtr;
		msg->iov[ 1 ].iov_len  = msg->bodyLen;
		msg->ion = 2;
	}
	msg->iop = msg->iov;
	
	*cnx->eventNext = msg;
	 cnx->eventNext = &msg->next;
	
	if( cnx->state == kHTTPConnectionStateReadingRequest )
	{
		cnx->state = kHTTPConnectionStatePreparingEvent;
		_HTTPConnectionRunStateMachine( cnx );
	}
	CFRelease( cnx );
}

//===========================================================================================================================
//	HTTPConnectionSendResponse
//
//	Must be called from the connection queue.
//===========================================================================================================================

OSStatus	HTTPConnectionSendResponse( HTTPConnectionRef inCnx )
{
	HTTPMessageRef const		msg = inCnx->responseMsg;
	OSStatus					err;
	Boolean						run;
	
	check( DebugIsCurrentDispatchQueue( inCnx->queue ) );
	
	err = HTTPHeader_Commit( &msg->header );
	require_noerr( err, exit );
	LogHTTP( inCnx->ucat, inCnx->ucat, msg->header.buf, msg->header.len, msg->bodyPtr, msg->bodyLen );
	
	msg->iov[ 0 ].iov_base = msg->header.buf;
	msg->iov[ 0 ].iov_len  = msg->header.len;
	msg->ion = 1;
	if( msg->bodyLen > 0 )
	{
		msg->iov[ 1 ].iov_base = msg->bodyPtr;
		msg->iov[ 1 ].iov_len  = msg->bodyLen;
		msg->ion = 2;
	}
	msg->iop = msg->iov;
	
	run = ( inCnx->state == kHTTPConnectionStateProcessingRequest );
	inCnx->state = kHTTPConnectionStateWritingResponse;
	if( run ) _HTTPConnectionRunStateMachine( inCnx );
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPConnectionSendSimpleResponseEx
//
//	Must be called from the connection queue.
//===========================================================================================================================

OSStatus
	HTTPConnectionSendSimpleResponseEx( 
		HTTPConnectionRef	inCnx, 
		HTTPStatus			inStatus, 
		OSStatus			inError, 
		const char *		inContentType, 
		const void *		inBodyPtr, 
		size_t				inBodyLen )
{
	return( HTTPConnectionSendSimpleResponseEx2( inCnx, NULL, inStatus, inError, inContentType, inBodyPtr, inBodyLen ) );
}

//===========================================================================================================================
//	HTTPConnectionSendSimpleResponseEx2
//
//	Must be called from the connection queue.
//===========================================================================================================================

OSStatus
	HTTPConnectionSendSimpleResponseEx2( 
		HTTPConnectionRef	inCnx, 
		const char *		inProtocol, 
		HTTPStatus			inStatus, 
		OSStatus			inError, 
		const char *		inContentType, 
		const void *		inBodyPtr, 
		size_t				inBodyLen )
{
	OSStatus		err;
	
	err = HTTPConnectionInitResponseEx( inCnx, inProtocol, inStatus, inError );
	require_noerr( err, exit );
	
	err = HTTPMessageSetBody( inCnx->responseMsg, inContentType, inBodyPtr, inBodyLen );
	require_noerr( err, exit );
	
	err = HTTPConnectionSendResponse( inCnx );
	require_noerr( err, exit );
	
exit:
	if( err ) ulog( inCnx->ucat, kLogLevelWarning, "### Response failed: %#m\n", err );
	return( err );
}

//===========================================================================================================================
//	HTTPConnectionVerifyAuth
//===========================================================================================================================

OSStatus	HTTPConnectionVerifyAuth( HTTPConnectionRef inCnx, HTTPMessageRef inMsg, Boolean *outAllow )
{
	HTTPServerRef const			server = inCnx->server;
	OSStatus					err;
	Boolean						allow = false;
	HTTPAuthorizationInfo		authInfo;
	HTTPStatus					status;
	char						nonceStr[ 64 ];
	
	if( inCnx->authorized || !server->password )
	{
		inCnx->authorized = true;
		allow = true;
		err = kNoErr;
		goto exit;
	}
	if( inCnx->delegate.requiresAuth_f && !inCnx->delegate.requiresAuth_f( inCnx, inMsg, inCnx->delegate.context ) )
	{
		allow = true;
		err = kNoErr;
		goto exit;
	}
	if( !server->timedNonceInitialized )
	{
		err = RandomBytes( server->timedNonceKey, sizeof( server->timedNonceKey ) );
		require_noerr( err, exit );
		server->timedNonceInitialized = true;
	}
	
	memset( &authInfo, 0, sizeof( authInfo ) );
	authInfo.serverScheme			= kHTTPAuthorizationScheme_Digest;
	authInfo.serverPassword			= server->password;
	authInfo.serverTimedNonceKeyPtr	= server->timedNonceKey;
	authInfo.serverTimedNonceKeyLen	= sizeof( server->timedNonceKey );
	authInfo.headerPtr				= inMsg->header.buf;
	authInfo.headerLen				= inMsg->header.len;
	authInfo.requestMethodPtr		= inMsg->header.methodPtr;
	authInfo.requestMethodLen		= inMsg->header.methodLen;
	authInfo.requestURLPtr			= inMsg->header.urlPtr;
	authInfo.requestURLLen			= inMsg->header.urlLen;
	status = HTTPVerifyAuthorization( &authInfo );
	if( status == kHTTPStatus_OK )
	{
		inCnx->authorized = true;
		allow = true;
		err = kNoErr;
		goto exit;
	}
	else if( status == kHTTPStatus_Unauthorized )
	{
		// Some HTTP clients use separate connections for each auth attempt so we have to use a timed nonce instead 
		// of a stronger per-connection nonce.
		
		err = HTTPMakeTimedNonce( kHTTPTimedNonceETagPtr, kHTTPTimedNonceETagLen, 
			server->timedNonceKey, sizeof( server->timedNonceKey ), 
			nonceStr, sizeof( nonceStr ), NULL );
		require_noerr_action( err, statusExit, status = kHTTPStatus_InternalServerError );
		require_action( server->realm, statusExit, status = kHTTPStatus_InternalServerError );
		
		err = HTTPConnectionInitResponse( inCnx, kHTTPStatus_Unauthorized, kNoErr );
		require_noerr( err, exit );
		
		HTTPHeader_SetField( &inCnx->responseMsg->header, kHTTPHeader_ContentLength, "0" );
		HTTPHeader_SetField( &inCnx->responseMsg->header, kHTTPHeader_WWWAuthenticate, "Digest realm=\"%s\", nonce=\"%s\"", 
			server->realm, nonceStr );
		
		err = HTTPConnectionSendResponse( inCnx );
		require_noerr( err, exit );
		goto exit;
	}
	else
	{
		err = kNoErr;
	}
	
statusExit:
	err = HTTPConnectionSendStatusResponse( inCnx, status, err );
	require_noerr( err, exit );

exit:
	*outAllow = allow;
	return( err );
}

#if 0
#pragma mark -
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	HTTPServerTest
//===========================================================================================================================

OSStatus	HTTPServerTest( void );
OSStatus	HTTPServerTestInitConnection( HTTPConnectionRef inCnx, void *inContext );
OSStatus	HTTPServerTestHandleMessage( HTTPConnectionRef inCnx, HTTPMessageRef inMsg, void *inContext );

OSStatus	HTTPServerTest( void )
{
	OSStatus				err;
	HTTPServerDelegate		delegate;
	HTTPServerRef			server = NULL;
	CFTypeRef				tempObj;
	
	tempObj = NULL;
	
	HTTPServerDelegateInit( &delegate );
#if( HTTP_SERVER_TEST_GCM )
	delegate.initializeConnection_f	= HTTPServerTestInitConnection;
#endif
	delegate.handleMessage_f		= HTTPServerTestHandleMessage;
	
	err = HTTPServerCreate( &server, &delegate );
	require_noerr( err, exit );
	server->listenPort = 8000;
	
#if( !HTTP_SERVER_TEST_GCM )
	err = HTTPServerSetPropertyF( server, kCFObjectFlagDirect, kHTTPServerProperty_Password, NULL, "%s", "password" );
	require_noerr( err, exit );
	
	err = HTTPServerSetPropertyF( server, kCFObjectFlagDirect, kHTTPServerProperty_Realm, NULL, "%s", "realm" );
	require_noerr( err, exit );
#endif
	
	HTTPServerStart( server );
	while( CFRunLoopRunInMode( kCFRunLoopDefaultMode, 30.0, true ) != kCFRunLoopRunTimedOut ) {}
	HTTPServerStop( server );
	while( CFRunLoopRunInMode( kCFRunLoopDefaultMode, 2.0, true ) != kCFRunLoopRunTimedOut ) {}
	
exit:
	CFReleaseNullSafe( tempObj );
	CFReleaseNullSafe( server );
	return( err );
}

#if( HTTP_SERVER_TEST_GCM )
OSStatus	HTTPServerTestInitConnection( HTTPConnectionRef inCnx, void *inContext )
{
	OSStatus					err;
	NetTransportDelegate		delegate;
	
	(void) inContext;
	
	err = NetTransportGCMConfigure( &delegate, NULL, 
		(const uint8_t *) "0123456789ABCDEF", (const uint8_t *) "0123456789ABCDEF", 
		(const uint8_t *) "0123456789abcdef", (const uint8_t *) "0123456789abcdef" );
	require_noerr( err, exit );
	HTTPConnectionSetTransportDelegate( inCnx, &delegate );
	
exit:
	return( err );
}
#endif

OSStatus	HTTPServerTestHandleMessage( HTTPConnectionRef inCnx, HTTPMessageRef inMsg, void *inContext )
{
	OSStatus			err;
	Boolean				good;
	const char *		ptr;
	size_t				len;
	
	(void) inContext;
	
	good = inCnx->authorized;
	if( !good )
	{
		err = HTTPConnectionVerifyAuth( inCnx, inMsg, &good );
		require_noerr( err, exit );
		if( !good ) goto exit;
	}
	
	good = HTTPConnectionGetNextURLSegmentEx( inCnx, inMsg, true, &ptr, &len, &err );
	if( good && ( len == 4 ) && ( memcmp( ptr, "stop", 4 ) == 0 ) )
	{
		err = HTTPConnectionSendSimpleResponse( inCnx, kHTTPStatus_OK, kMIMEType_TextPlain, "quit", 4 );
		require_noerr( err, exit );
		
		HTTPServerControl( inCnx->server, 0, kHTTPServerCommand_StopServer, NULL );
		CFRelease( inCnx->server );
	}
	else
	{
		err = HTTPConnectionSendSimpleResponse( inCnx, kHTTPStatus_OK, kMIMEType_TextPlain, 
			inCnx->requestMsg->header.urlPtr, inCnx->requestMsg->header.urlLen );
		require_noerr( err, exit );
	}
	
exit:
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
