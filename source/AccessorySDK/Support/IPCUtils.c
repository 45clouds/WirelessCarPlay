/*
	File:    	IPCUtils.c
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
	
	Copyright (C) 2013-2014 Apple Inc. All Rights Reserved.
*/

#include "IPCUtils.h"

#include <netinet/tcp.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include "NetUtils.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

//===========================================================================================================================
//	Constants
//===========================================================================================================================

#define kIPCPort		3722

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static void		_IPCClientGetTypeID( void *inContext );
static void		_IPCClientFinalize( CFTypeRef inObj );
static void 	_IPCClientStart( void *inContext );
static void		_IPCClientStop( void *inContext );
static void		_IPCClientReadHandler( void *inContext );
static void		_IPCClientWriteHandler( void *inContext );
static void		_IPCClientWriteHandler2( IPCClientRef inClient );
static void		_IPCClientCancelHandler( void *inContext );
static void		_IPCClientErrorHandler( IPCClientRef client, OSStatus inError );
static void		_IPCClientCompleteMessage( IPCClientRef inClient, IPCMessageRef inMsg, OSStatus inStatus );
static void		_IPCClientSendMessage( void *inContext );

static void		_IPCServerGetTypeID( void *inContext );
static void		_IPCServerFinalize( CFTypeRef inCF );
static void		_IPCServerStop( void *inContext );
static void		_IPCServerAcceptConnection( void *inContext );
static void		_IPCServerCloseConnection( IPCServerConnectionRef inCnx, OSStatus inReason );
static void		_IPCServerListenerCanceled( void *inContext );

static CFTypeID	IPCServerConnectionGetTypeID( void );
static void		_IPCServerConnectionGetTypeID( void *inContext );
static OSStatus	_IPCServerConnectionCreate( IPCServerConnectionRef *outCnx, SocketRef inSock, IPCServerRef inServer );
static void		_IPCServerConnectionFinalize( CFTypeRef inCF );
static OSStatus	_IPCServerConnectionStart( IPCServerConnectionRef inCnx );
static void		_IPCServerConnectionStop( IPCServerConnectionRef inCnx, OSStatus inReason );
static void		_IPCServerConnectionReadHandler( void *inContext );
static void		_IPCServerConnectionWriteHandler( void *inContext );
static void		_IPCServerConnectionWriteHandler2( IPCServerConnectionRef inCnx );
static void		_IPCServerConnectionCancelHandler( void *inContext );
static void		_IPCServerConnectionSendMessage( void *inContext );
static void		_IPCServerConnectionCompleteMessage( IPCServerConnectionRef inCnx, IPCMessageRef inMsg );

static CFTypeID	IPCMessageGetTypeID( void );
static void		_IPCMessageGetTypeID( void *inContext );
static void		_IPCMessageFinalize( CFTypeRef inCF );
static void		_IPCMessageReset( IPCMessageRef inMsg );
static OSStatus	_IPCMessageReadMessage( SocketRef inSock, IPCMessageRef inMsg );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static dispatch_once_t			gIPCClientInitOnce = 0;
static CFTypeID					gIPCClientTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kIPCClientClass = 
{
	0,						// version
	"IPCClient",			// className
	NULL,					// init
	NULL,					// copy
	_IPCClientFinalize,		// finalize
	NULL,					// equal -- NULL means pointer equality.
	NULL,					// hash  -- NULL means pointer hash.
	NULL,					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

static dispatch_once_t			gIPCMessageInitOnce = 0;
static CFTypeID					gIPCMessageTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kIPCMessageClass = 
{
	0,						// version
	"IPCMessage",			// className
	NULL,					// init
	NULL,					// copy
	_IPCMessageFinalize,	// finalize
	NULL,					// equal -- NULL means pointer equality.
	NULL,					// hash  -- NULL means pointer hash.
	NULL,					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

static dispatch_once_t			gIPCServerInitOnce = 0;
static CFTypeID					gIPCServerTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kIPCServerClass = 
{
	0,						// version
	"IPCServer",			// className
	NULL,					// init
	NULL,					// copy
	_IPCServerFinalize,		// finalize
	NULL,					// equal -- NULL means pointer equality.
	NULL,					// hash  -- NULL means pointer hash.
	NULL,					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

static dispatch_once_t			gIPCServerConnectionInitOnce = 0;
static CFTypeID					gIPCServerConnectionTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kIPCServerConnectionClass = 
{
	0,								// version
	"IPCServerConnection",			// className
	NULL,							// init
	NULL,							// copy
	_IPCServerConnectionFinalize,	// finalize
	NULL,							// equal -- NULL means pointer equality.
	NULL,							// hash  -- NULL means pointer hash.
	NULL,							// copyFormattingDesc
	NULL,							// copyDebugDesc
	NULL,							// reclaim
	NULL							// refcount
};

ulog_define( IPCUtils, kLogLevelVerbose, kLogFlags_Default, "IPC", NULL );
#define ipc_dlog( LEVEL, ... )		dlogc( &log_category_from_name( IPCUtils ), (LEVEL), __VA_ARGS__ )
#define ipc_ulog( LEVEL, ... )		ulog( &log_category_from_name( IPCUtils ), (LEVEL), __VA_ARGS__ )

#if 0
#pragma mark == Client ==
#endif

//===========================================================================================================================
//	IPCClientGetTypeID
//===========================================================================================================================

CFTypeID	IPCClientGetTypeID( void )
{
	dispatch_once_f( &gIPCClientInitOnce, NULL, _IPCClientGetTypeID );
	return( gIPCClientTypeID );
}

static void _IPCClientGetTypeID( void *inContext )
{
	(void) inContext;
	
	gIPCClientTypeID = _CFRuntimeRegisterClass( &kIPCClientClass );
	check( gIPCClientTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	IPCClientCreate
//===========================================================================================================================

OSStatus	IPCClientCreate( IPCClientRef *outClient, const IPCClientDelegate *inDelegate )
{
	OSStatus			err;
	IPCClientRef		obj;
	size_t				extraLen;
	
	extraLen = sizeof( *obj ) - sizeof( obj->base );
	obj = (IPCClientRef) _CFRuntimeCreateInstance( NULL, IPCClientGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( obj, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) obj ) + sizeof( obj->base ), 0, extraLen );
	
	obj->sock		= kInvalidSocketRef;
	obj->writeNext	= &obj->writeList;
	obj->delegate	= *inDelegate;
	
	obj->queue = dispatch_queue_create( "IPCClient", NULL ); 
	require_action( obj->queue, exit, err = kUnknownErr );
	
	err = IPCMessageCreate( &obj->readMsg );
	require_noerr( err, exit );
	obj->readMsg->context = obj;
	
	*outClient = obj;
	obj = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( obj );
	return( err );
}

//===========================================================================================================================
//	_IPCClientFinalize
//===========================================================================================================================

static void	_IPCClientFinalize( CFTypeRef inObj )
{
	IPCClientRef const		client = (IPCClientRef) inObj;
	
	ForgetCF( &client->readMsg );
	dispatch_forget( &client->queue );
}

//===========================================================================================================================
//	IPCClientStart
//===========================================================================================================================

typedef struct
{
	IPCClientRef		client;
	SocketRef			sock;
	
}	IPCClientStartParams;

OSStatus	IPCClientStart( IPCClientRef inClient )
{
	OSStatus					err;
	SocketRef					sock;
	sockaddr_ip					sip;
	IPCClientStartParams *		params;
	
	sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	err = map_socket_creation_errno( sock );
	require_noerr( err, exit );
	
	memset( &sip.v4, 0, sizeof( sip.v4 ) );
	SIN_LEN_SET( &sip.v4 );
	sip.v4.sin_family		= AF_INET;
	sip.v4.sin_port			= htons( kIPCPort );
	sip.v4.sin_addr.s_addr	= htonl( INADDR_LOOPBACK );
	err = SocketConnect( sock, &sip, 3 );
	require_noerr( err, exit );
	
	params = (IPCClientStartParams *) malloc( sizeof( *params ) );
	require_action( params, exit, err = kNoMemoryErr );
	CFRetain( inClient );
	params->client = inClient;
	params->sock   = sock;
	
	dispatch_async_f( inClient->queue, params, _IPCClientStart );
	sock = kInvalidSocketRef;
	
exit:
	ForgetSocket( &sock );
	return( err );
}

static void _IPCClientStart( void *inContext )
{
	IPCClientStartParams * const		params = (IPCClientStartParams *) inContext;
	IPCClientRef const					client = params->client;
	OSStatus							err;
	
	// Set up a source to notify us when the socket is readable.
	
	client->readSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, params->sock, 0, client->queue );
	require_action( client->readSource, exit, err = kUnknownErr );
	++client->sockRefCount;
	CFRetain( client );
	dispatch_set_context( client->readSource, client );
	dispatch_source_set_event_handler_f( client->readSource, _IPCClientReadHandler );
	dispatch_source_set_cancel_handler_f( client->readSource, _IPCClientCancelHandler );
	dispatch_resume( client->readSource );
	
	// Set up a source to notify us when the socket is writable.
	
	client->writeSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_WRITE, params->sock, 0, client->queue );
	require_action( client->writeSource, exit, err = kUnknownErr );
	++client->sockRefCount;
	CFRetain( client );
	dispatch_set_context( client->writeSource, client );
	dispatch_source_set_event_handler_f( client->writeSource, _IPCClientWriteHandler );
	dispatch_source_set_cancel_handler_f( client->writeSource, _IPCClientCancelHandler );
	client->writeSuspended = true; // Don't resume until we get EWOULDBLOCK.
	
	client->sock = params->sock;
	client->started = true;
	err = kNoErr;
	
exit:
	if( err )
	{
		if( client->sockRefCount == 0 ) ForgetSocket( &params->sock );
		_IPCClientErrorHandler( client, err );
	}
	CFRelease( client );
	free( params );
}

//===========================================================================================================================
//	IPCClientStop
//===========================================================================================================================

void	IPCClientStop( IPCClientRef inClient )
{
	CFRetain( inClient );
	dispatch_async_f( inClient->queue, inClient, _IPCClientStop );
}

static void	_IPCClientStop( void *inContext )
{
	IPCClientRef const		client = (IPCClientRef) inContext;
	
	_IPCClientErrorHandler( client, kCanceledErr );
	CFRelease( client );
}

//===========================================================================================================================
//	_IPCClientReadHandler
//===========================================================================================================================

static void	_IPCClientReadHandler( void *inContext )
{
	IPCClientRef const		client = (IPCClientRef) inContext;
	OSStatus				err;
	uint32_t				xid;
	IPCMessageRef *			next;
	IPCMessageRef			curr;
	Boolean					found;
	
	err = _IPCMessageReadMessage( client->sock, client->readMsg );
	if( err == EWOULDBLOCK ) return;
	require_noerr_quiet( err, exit );
	
	found = false;
	xid = client->readMsg->header.xid;
	if( ( xid != 0 ) && ( client->readMsg->header.flags & kIPCFlag_Reply ) )
	{
		for( next = &client->waitList; ( curr = *next ) != NULL; next = &curr->waitNext )
		{
			if( xid == curr->header.xid )
			{
				*next = curr->waitNext;
				err = IPCMessageCreateCopy( &curr->replyMsg, client->readMsg );
				check_noerr( err );
				curr->status = err;
				dispatch_semaphore_signal( curr->replySem );
				CFRelease( curr );
				found = true;
				break;
			}
		}
	}
	if( !found && !( client->readMsg->header.flags & kIPCFlag_Reply ) && client->delegate.handleMessage_f )
	{
		err = client->delegate.handleMessage_f( client, client->readMsg );
		require_noerr_quiet( err, exit );
	}
	_IPCMessageReset( client->readMsg );
	
exit:
	if( err ) _IPCClientErrorHandler( client, err );
}

//===========================================================================================================================
//	_IPCClientWriteHandler
//===========================================================================================================================

static void	_IPCClientWriteHandler( void *inContext )
{
	IPCClientRef const		client = (IPCClientRef) inContext;
	
	if( !client->writeSuspended )
	{
		dispatch_suspend( client->writeSource ); // Disable writability notification until we get another EWOULDBLOCK.
		client->writeSuspended = true;
	}
	_IPCClientWriteHandler2( client );
}

//===========================================================================================================================
//	_IPCClientWriteHandler2
//===========================================================================================================================

static void	_IPCClientWriteHandler2( IPCClientRef inClient )
{
	OSStatus			err;
	IPCMessageRef		msg;
	
	msg = inClient->writeList;
	if( !msg ) return;
	
	err = SocketWriteData( inClient->sock, &msg->iop, &msg->ion );
	if( err == EWOULDBLOCK )	dispatch_resume_if_suspended( inClient->writeSource, &inClient->writeSuspended );
	else if( err )				_IPCClientErrorHandler( inClient, err );
	else						_IPCClientCompleteMessage( inClient, msg, kNoErr );
}

//===========================================================================================================================
//	_IPCClientCancelHandler
//===========================================================================================================================

static void	_IPCClientCancelHandler( void *inContext )
{
	IPCClientRef const		client = (IPCClientRef) inContext;
	
	if( --client->sockRefCount == 0 )
	{
		ForgetSocket( &client->sock );
	}
	CFRelease( client );
}

//===========================================================================================================================
//	_IPCClientErrorHandler
//===========================================================================================================================

static void	_IPCClientErrorHandler( IPCClientRef client, OSStatus inError )
{
	IPCMessageRef		msg;
	
	dispatch_source_forget( &client->readSource );
	dispatch_source_forget_ex( &client->writeSource, &client->writeSuspended );
	
	_IPCMessageReset( client->readMsg );
	while( ( msg = client->waitList ) != NULL )
	{
		client->waitList = msg->waitNext;
		msg->status = inError;
		dispatch_semaphore_signal( msg->replySem );
		CFRelease( msg );
	}
	while( ( msg = client->writeList ) != NULL )
	{
		_IPCClientCompleteMessage( client, msg, inError );
	}
	if( client->started )
	{
		client->started = false;
		if( client->delegate.handleStopped_f ) client->delegate.handleStopped_f( client, inError );
	}
}

//===========================================================================================================================
//	_IPCClientCompleteMessage
//===========================================================================================================================

static void	_IPCClientCompleteMessage( IPCClientRef inClient, IPCMessageRef inMsg, OSStatus inStatus )
{
	if( ( inClient->writeList = inMsg->next ) == NULL )
		  inClient->writeNext = &inClient->writeList;
	inMsg->status = inStatus;
	if( inMsg->completion ) inMsg->completion( inMsg );
	CFRelease( inMsg );
}

//===========================================================================================================================
//	IPCClientSendMessage
//===========================================================================================================================

OSStatus	IPCClientSendMessage( IPCClientRef inClient, IPCMessageRef inMsg )
{
	inMsg->iov[ 0 ].iov_base = &inMsg->header;
	inMsg->iov[ 0 ].iov_len  = sizeof( inMsg->header );
	inMsg->ion = 1;
	if( inMsg->bodyLen > 0 )
	{
		inMsg->iov[ 1 ].iov_base = (char *) inMsg->bodyPtr;
		inMsg->iov[ 1 ].iov_len  = inMsg->bodyLen;
		inMsg->ion = 2;
	}
	inMsg->iop = inMsg->iov;
	
	CFRetain( inMsg );
	CFRetain( inClient );
	inMsg->context = inClient;
	dispatch_async_f( inClient->queue, inMsg, _IPCClientSendMessage );
	return( kNoErr );
}

static void	_IPCClientSendMessage( void *inContext )
{
	IPCMessageRef const		msg    = (IPCMessageRef) inContext;
	IPCClientRef const		client = (IPCClientRef) msg->context;
	uint32_t				xid;
	
	if( !( msg->header.flags & kIPCFlag_Reply ) )
	{
		xid = client->lastXID + 1;
		if( xid == 0 ) ++xid;
		client->lastXID = xid;
		msg->header.xid = xid;
	}
	
	if( msg->replySem )
	{
		CFRetain( msg );
		msg->waitNext = client->waitList;
		client->waitList = msg;
	}
	
	msg->next = NULL;
	*client->writeNext = msg;
	 client->writeNext = &msg->next;
	_IPCClientWriteHandler2( client );
	CFRelease( client );
}

//===========================================================================================================================
//	IPCClientSendMessageWithReplySync
//===========================================================================================================================

OSStatus	IPCClientSendMessageWithReplySync( IPCClientRef inClient, IPCMessageRef inMsg, IPCMessageRef *outReply )
{
	OSStatus		err;
	
	inMsg->replySem = dispatch_semaphore_create( 0 );
	require_action( inMsg->replySem, exit, err = kUnknownErr );
	
	err = IPCClientSendMessage( inClient, inMsg );
	require_noerr( err, exit );
	
	dispatch_semaphore_wait( inMsg->replySem, DISPATCH_TIME_FOREVER );
	require_noerr_action( inMsg->status, exit, err = inMsg->status );
	
	*outReply = inMsg->replyMsg;
	inMsg->replyMsg = NULL;
	
exit:
	dispatch_forget( &inMsg->replySem );
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Server ==
#endif

//===========================================================================================================================
//	IPCServerGetTypeID
//===========================================================================================================================

CFTypeID	IPCServerGetTypeID( void )
{
	dispatch_once_f( &gIPCServerInitOnce, NULL, _IPCServerGetTypeID );
	return( gIPCServerTypeID );
}

static void _IPCServerGetTypeID( void *inContext )
{
	(void) inContext;
	
	gIPCServerTypeID = _CFRuntimeRegisterClass( &kIPCServerClass );
	check( gIPCServerTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	IPCServerCreate
//===========================================================================================================================

OSStatus	IPCServerCreate( IPCServerRef *outServer, const IPCServerDelegate *inDelegate )
{
	OSStatus			err;
	IPCServerRef		obj;
	size_t				extraLen;
	
	extraLen = sizeof( *obj ) - sizeof( obj->base );
	obj = (IPCServerRef) _CFRuntimeCreateInstance( NULL, IPCServerGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( obj, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) obj ) + sizeof( obj->base ), 0, extraLen );
	
	obj->delegate = *inDelegate;
	
	obj->queue = dispatch_queue_create( "IPCServer", NULL ); 
	require_action( obj->queue, exit, err = kUnknownErr );
	
	*outServer = obj;
	obj = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( obj );
	return( err );
}

//===========================================================================================================================
//	_IPCServerFinalize
//===========================================================================================================================

static void	_IPCServerFinalize( CFTypeRef inObj )
{
	IPCServerRef const		server = (IPCServerRef) inObj;
	
	check( !server->listener );
	dispatch_forget( &server->queue );
}

//===========================================================================================================================
//	IPCServerStart
//===========================================================================================================================

static void	_IPCServerStart( void *inContext );

OSStatus	IPCServerStart( IPCServerRef inServer )
{
	CFRetain( inServer );
	dispatch_async_f( inServer->queue, inServer, _IPCServerStart );
	return( kNoErr );
}

static void	_IPCServerStart( void *inContext )
{
	IPCServerRef const				server = (IPCServerRef) inContext;
	OSStatus						err;
	SocketRef						sock = kInvalidSocketRef;
	IPCServerListenerContext *		listener;
	dispatch_source_t				source;
	
	err = ServerSocketOpen( AF_INET, SOCK_STREAM, IPPROTO_TCP, kIPCPort, NULL, kSocketBufferSize_DontSet, &sock );
	require_noerr( err, exit );
	
	listener = (IPCServerListenerContext *) calloc( 1, sizeof( *listener ) );
	require_action( listener, exit, err = kNoMemoryErr );
	
	source = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, sock, 0, server->queue );
	if( !source ) free( listener );
	require_action( source, exit, err = kUnknownErr );
	
	CFRetain( server );
	listener->source	= source;
	listener->sock		= sock;
	listener->server	= server;
	server->listener	= listener;
	dispatch_set_context( source, listener );
	dispatch_source_set_event_handler_f( source, _IPCServerAcceptConnection );
	dispatch_source_set_cancel_handler_f( source, _IPCServerListenerCanceled );
	dispatch_resume( source );
	sock = kInvalidSocketRef;
	
	CFRetain( server );
	server->started = true;
	ipc_ulog( kLogLevelInfo, "Server started\n" );
	
exit:
	ForgetSocket( &sock );
	if( err )	_IPCServerStop( server );
	else		CFRelease( server );
}

//===========================================================================================================================
//	IPCServerStop
//===========================================================================================================================

OSStatus	IPCServerStop( IPCServerRef inServer )
{
	CFRetain( inServer );
	dispatch_async_f( inServer->queue, inServer, _IPCServerStop );
	return( kNoErr );
}

static void	_IPCServerStop( void *inContext )
{
	IPCServerRef const		server = (IPCServerRef) inContext;
	
	if( server->listener )
	{
		dispatch_source_forget( &server->listener->source );
		server->listener = NULL;
	}
	while( server->connections )
	{
		_IPCServerCloseConnection( server->connections, kEndingErr );
	}
	if( server->started )
	{
		server->started = false;
		CFRelease( server );
		ipc_ulog( kLogLevelTrace, "Server stopped\n" );
	}
	CFRelease( server );
}

//===========================================================================================================================
//	_IPCServerAcceptConnection
//===========================================================================================================================

static void	_IPCServerAcceptConnection( void *inContext )
{
	IPCServerListenerContext * const		listener	= (IPCServerListenerContext *) inContext;
	IPCServerRef const						server		= listener->server;
	SocketRef								newSock		= kInvalidSocketRef;
	IPCServerConnectionRef					cnx			= NULL;
	OSStatus								err;
	sockaddr_ip								sip;
	socklen_t								len;
	
	len = (socklen_t) sizeof( sip );
	newSock = accept( listener->sock, &sip.sa, &len );
	err = map_socket_creation_errno( newSock );
	require_noerr( err, exit );
	require_action( sip.v4.sin_addr.s_addr == htonl( INADDR_LOOPBACK ), exit, err = kAddressErr );
	
	err = _IPCServerConnectionCreate( &cnx, newSock, server );
	require_noerr( err, exit );
	newSock = kInvalidSocketRef;
	
	cnx->peerAddr = sip;
	cnx->next = server->connections;
	server->connections = cnx;
	
	err = _IPCServerConnectionStart( cnx );
	require_noerr( err, exit );
	cnx = NULL;
	ipc_ulog( kLogLevelInfo, "Accepted connection from %##a\n", &sip );
	
exit:
	ForgetSocket( &newSock );
	if( cnx ) _IPCServerCloseConnection( cnx, err );
	if( err ) ipc_ulog( kLogLevelWarning, "### Accept connection failed: %#m\n", err );
}

//===========================================================================================================================
//	_IPCServerCloseConnection
//===========================================================================================================================

static void	_IPCServerCloseConnection( IPCServerConnectionRef inCnx, OSStatus inReason )
{
	IPCServerRef const				server = inCnx->server;
	IPCServerConnectionRef *		next;
	IPCServerConnectionRef			curr;
	
	for( next = &server->connections; ( curr = *next ) != NULL; next = &curr->next )
	{
		if( curr == inCnx )
		{
			*next = curr->next;
			break;
		}
	}
	_IPCServerConnectionStop( inCnx, inReason );
	if( inCnx->peerAddr.sa.sa_family != AF_UNSPEC ) ipc_ulog( kLogLevelInfo, "Closing connection from %##a\n", &inCnx->peerAddr );
	CFRelease( inCnx );
}

//===========================================================================================================================
//	_IPCServerListenerCanceled
//===========================================================================================================================

static void	_IPCServerListenerCanceled( void *inContext )
{
	IPCServerListenerContext * const		listener = (IPCServerListenerContext *) inContext;
	
	ForgetSocket( &listener->sock );
	CFRelease( listener->server );
	free( listener );
}

#if 0
#pragma mark -
#pragma mark == Server Connection ==
#endif

//===========================================================================================================================
//	IPCServerConnectionGetTypeID
//===========================================================================================================================

static CFTypeID	IPCServerConnectionGetTypeID( void )
{
	dispatch_once_f( &gIPCServerConnectionInitOnce, NULL, _IPCServerConnectionGetTypeID );
	return( gIPCServerConnectionTypeID );
}

static void _IPCServerConnectionGetTypeID( void *inContext )
{
	(void) inContext;
	
	gIPCServerConnectionTypeID = _CFRuntimeRegisterClass( &kIPCServerConnectionClass );
	check( gIPCServerConnectionTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	_IPCServerConnectionCreate
//===========================================================================================================================

static OSStatus	_IPCServerConnectionCreate( IPCServerConnectionRef *outCnx, SocketRef inSock, IPCServerRef inServer )
{
	OSStatus					err;
	IPCServerConnectionRef		obj;
	size_t						extraLen;
	
	extraLen = sizeof( *obj ) - sizeof( obj->base );
	obj = (IPCServerConnectionRef) _CFRuntimeCreateInstance( NULL, IPCServerConnectionGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( obj, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) obj ) + sizeof( obj->base ), 0, extraLen );
	
	arc_safe_dispatch_retain( inServer->queue );
	obj->queue = inServer->queue;
	
	CFRetain( inServer );
	obj->server = inServer;
	obj->sock = inSock;
	obj->writeNext = &obj->writeList;
	
	err = IPCMessageCreate( &obj->readMsg );
	require_noerr( err, exit );
	obj->readMsg->context = obj;
	
	if( inServer->delegate.initConnection_f )
	{
		err = inServer->delegate.initConnection_f( obj );
		require_noerr( err, exit );
	}
	
	*outCnx = obj;
	obj = NULL;
	err = kNoErr;
	
exit:
	if( obj ) CFRelease( obj );
	return( err );
}

//===========================================================================================================================
//	_IPCServerConnectionFinalize
//===========================================================================================================================

static void	_IPCServerConnectionFinalize( CFTypeRef inCF )
{
	IPCServerConnectionRef const		cnx = (IPCServerConnectionRef) inCF;
	
	if( cnx->server->delegate.freeConnection_f ) cnx->server->delegate.freeConnection_f( cnx );
	ForgetCF( &cnx->server );
	dispatch_forget( &cnx->queue );
	check( cnx->readSource == NULL );
	check( cnx->writeSource == NULL );
	ForgetSocket( &cnx->sock );
	ForgetCF( &cnx->readMsg );
}

//===========================================================================================================================
//	_IPCServerConnectionStart
//===========================================================================================================================

static OSStatus	_IPCServerConnectionStart( IPCServerConnectionRef inCnx )
{
	OSStatus		err;
	int				option;
	
	// Disable SIGPIPE on this socket so we get EPIPE errors instead of terminating the process.
	
#if( defined( SO_NOSIGPIPE ) )
	setsockopt( inCnx->sock, SOL_SOCKET, SO_NOSIGPIPE, &(int){ 1 }, (socklen_t) sizeof( int ) );
#endif
	
	err = SocketMakeNonBlocking( inCnx->sock );
	check_noerr( err );
	
	// Disable nagle so responses we send are not delayed. We coalesce writes to minimize small writes anyway.
	
	option = 1;
	err = setsockopt( inCnx->sock, IPPROTO_TCP, TCP_NODELAY, (char *) &option, (socklen_t) sizeof( option ) );
	err = map_socket_noerr_errno( inCnx->sock, err );
	check_noerr( err );
	
	// Set up a source to notify us when the socket is readable.
	
	inCnx->readSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, inCnx->sock, 0, inCnx->queue );
	require_action( inCnx->readSource, exit, err = kUnknownErr );
	CFRetain( inCnx );
	dispatch_set_context( inCnx->readSource, inCnx );
	dispatch_source_set_event_handler_f( inCnx->readSource, _IPCServerConnectionReadHandler );
	dispatch_source_set_cancel_handler_f( inCnx->readSource, _IPCServerConnectionCancelHandler );
	dispatch_resume( inCnx->readSource );
	
	// Set up a source to notify us when the socket is writable.
	
	inCnx->writeSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_WRITE, inCnx->sock, 0, inCnx->queue );
	require_action( inCnx->writeSource, exit, err = kUnknownErr );
	CFRetain( inCnx );
	dispatch_set_context( inCnx->writeSource, inCnx );
	dispatch_source_set_event_handler_f( inCnx->writeSource, _IPCServerConnectionWriteHandler );
	dispatch_source_set_cancel_handler_f( inCnx->writeSource, _IPCServerConnectionCancelHandler );
	inCnx->writeSuspended = true; // Don't resume until we get EWOULDBLOCK.
	
exit:
	if( err ) _IPCServerConnectionStop( inCnx, err );
	return( err );
}

//===========================================================================================================================
//	_IPCServerConnectionStop
//===========================================================================================================================

static void	_IPCServerConnectionStop( IPCServerConnectionRef inCnx, OSStatus inReason )
{
	IPCMessageRef		msg;
	
	dispatch_source_forget( &inCnx->readSource );
	dispatch_source_forget_ex( &inCnx->writeSource, &inCnx->writeSuspended );
	
	_IPCMessageReset( inCnx->readMsg );
	while( ( msg = inCnx->waitList ) != NULL )
	{
		inCnx->waitList = msg->waitNext;
		msg->status = inReason;
		dispatch_semaphore_signal( msg->replySem );
		CFRelease( msg );
	}
	while( ( msg = inCnx->writeList ) != NULL )
	{
		_IPCServerConnectionCompleteMessage( inCnx, msg );
	}
}

//===========================================================================================================================
//	_IPCServerConnectionReadHandler
//===========================================================================================================================

static void	_IPCServerConnectionReadHandler( void *inContext )
{
	IPCServerConnectionRef const		cnx = (IPCServerConnectionRef) inContext;
	OSStatus							err;
	uint32_t							xid;
	IPCMessageRef *						next;
	IPCMessageRef						curr;
	Boolean								found;
	
	err = _IPCMessageReadMessage( cnx->sock, cnx->readMsg );
	if( err == EWOULDBLOCK ) return;
	require_noerr_quiet( err, exit );
	
	found = false;
	xid = cnx->readMsg->header.xid;
	if( ( xid != 0 ) && ( cnx->readMsg->header.flags & kIPCFlag_Reply ) )
	{
		for( next = &cnx->waitList; ( curr = *next ) != NULL; next = &curr->waitNext )
		{
			if( xid == curr->header.xid )
			{
				*next = curr->waitNext;
				err = IPCMessageCreateCopy( &curr->replyMsg, cnx->readMsg );
				check_noerr( err );
				curr->status = err;
				dispatch_semaphore_signal( curr->replySem );
				CFRelease( curr );
				found = true;
				break;
			}
		}
	}
	
	if( !found && !( cnx->readMsg->header.flags & kIPCFlag_Reply ) && cnx->server->delegate.handleMessage_f )
	{
		cnx->server->delegate.handleMessage_f( cnx, cnx->readMsg );
	}
	_IPCMessageReset( cnx->readMsg );
	
exit:
	if( err ) _IPCServerCloseConnection( cnx, err );
}

//===========================================================================================================================
//	_IPCServerConnectionWriteHandler
//===========================================================================================================================

static void	_IPCServerConnectionWriteHandler( void *inContext )
{
	IPCServerConnectionRef const		cnx = (IPCServerConnectionRef) inContext;
	
	if( !cnx->writeSuspended )
	{
		dispatch_suspend( cnx->writeSource ); // Disable writability notification until we get another EWOULDBLOCK.
		cnx->writeSuspended = true;
	}
	_IPCServerConnectionWriteHandler2( cnx );
}

//===========================================================================================================================
//	_IPCServerConnectionWriteHandler2
//===========================================================================================================================

static void	_IPCServerConnectionWriteHandler2( IPCServerConnectionRef inCnx )
{
	OSStatus			err;
	IPCMessageRef		msg;
	
	msg = inCnx->writeList;
	if( !msg ) return;
	
	err = SocketWriteData( inCnx->sock, &msg->iop, &msg->ion );
	if( err == EWOULDBLOCK )	dispatch_resume_if_suspended( inCnx->writeSource, &inCnx->writeSuspended );
	else if( err )				_IPCServerCloseConnection( inCnx, err );
	else						_IPCServerConnectionCompleteMessage( inCnx, msg );
}

//===========================================================================================================================
//	_IPCServerConnectionCancelHandler
//===========================================================================================================================

static void	_IPCServerConnectionCancelHandler( void *inContext )
{
	IPCServerConnectionRef const		cnx = (IPCServerConnectionRef) inContext;
	
	CFRelease( cnx );
}

//===========================================================================================================================
//	IPCServerConnectionSendMessage
//===========================================================================================================================

OSStatus	IPCServerConnectionSendMessage( IPCServerConnectionRef inCnx, IPCMessageRef inMsg )
{
	inMsg->iov[ 0 ].iov_base = &inMsg->header;
	inMsg->iov[ 0 ].iov_len  = sizeof( inMsg->header );
	inMsg->ion = 1;
	if( inMsg->bodyLen > 0 )
	{
		inMsg->iov[ 1 ].iov_base = (char *) inMsg->bodyPtr;
		inMsg->iov[ 1 ].iov_len  = inMsg->bodyLen;
		inMsg->ion = 2;
	}
	inMsg->iop = inMsg->iov;
	
	CFRetain( inMsg );
	CFRetain( inCnx );
	inMsg->context = inCnx;
	dispatch_async_f( inCnx->queue, inMsg, _IPCServerConnectionSendMessage );
	return( kNoErr );
}

static void	_IPCServerConnectionSendMessage( void *inContext )
{
	IPCMessageRef const					msg = (IPCMessageRef) inContext;
	IPCServerConnectionRef const		cnx = (IPCServerConnectionRef) msg->context;
	uint32_t							xid;
	
	if( !( msg->header.flags & kIPCFlag_Reply ) )
	{
		xid = cnx->lastXID + 1;
		if( xid == 0 ) ++xid;
		cnx->lastXID = xid;
		msg->header.xid = xid;
	}
	
	if( msg->replySem )
	{
		CFRetain( msg );
		msg->waitNext = cnx->waitList;
		cnx->waitList = msg;
	}
	
	msg->next = NULL;
	*cnx->writeNext = msg;
	 cnx->writeNext = &msg->next;
	_IPCServerConnectionWriteHandler2( cnx );
	CFRelease( cnx );
}

//===========================================================================================================================
//	IPCServerConnectionSendMessageWithReplySync
//===========================================================================================================================

OSStatus	IPCServerConnectionSendMessageWithReplySync( IPCServerConnectionRef inCnx, IPCMessageRef inMsg, IPCMessageRef *outReply )
{
	OSStatus		err;
	
	inMsg->replySem = dispatch_semaphore_create( 0 );
	require_action( inMsg->replySem, exit, err = kUnknownErr );
	
	err = IPCServerConnectionSendMessage( inCnx, inMsg );
	require_noerr( err, exit );
	
	dispatch_semaphore_wait( inMsg->replySem, DISPATCH_TIME_FOREVER );
	require_noerr_action( inMsg->status, exit, err = inMsg->status );
	
	*outReply = inMsg->replyMsg;
	inMsg->replyMsg = NULL;
	
exit:
	dispatch_forget( &inMsg->replySem );
	return( err );
}

//===========================================================================================================================
//	_IPCServerConnectionCompleteMessage
//===========================================================================================================================

static void	_IPCServerConnectionCompleteMessage( IPCServerConnectionRef inCnx, IPCMessageRef inMsg )
{
	if( ( inCnx->writeList = inMsg->next ) == NULL )
		  inCnx->writeNext = &inCnx->writeList;
	CFRelease( inMsg );
}

#if 0
#pragma mark -
#pragma mark == Message ==
#endif

//===========================================================================================================================
//	IPCMessageGetTypeID
//===========================================================================================================================

static CFTypeID	IPCMessageGetTypeID( void )
{
	dispatch_once_f( &gIPCMessageInitOnce, NULL, _IPCMessageGetTypeID );
	return( gIPCMessageTypeID );
}

static void _IPCMessageGetTypeID( void *inContext )
{
	(void) inContext;
	
	gIPCMessageTypeID = _CFRuntimeRegisterClass( &kIPCMessageClass );
	check( gIPCMessageTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	IPCMessageCreate
//===========================================================================================================================

OSStatus	IPCMessageCreate( IPCMessageRef *outMsg )
{
	OSStatus			err;
	IPCMessageRef		obj;
	size_t				extraLen;
	
	extraLen = sizeof( *obj ) - sizeof( obj->base );
	obj = (IPCMessageRef) _CFRuntimeCreateInstance( NULL, IPCMessageGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( obj, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) obj ) + sizeof( obj->base ), 0, extraLen );
	
	_IPCMessageReset( obj );
	
	*outMsg = obj;
	obj = NULL;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	IPCMessageCreateCopy
//===========================================================================================================================

OSStatus	IPCMessageCreateCopy( IPCMessageRef *outMsg, IPCMessageRef inMsg )
{
	OSStatus			err;
	IPCMessageRef		newMsg = NULL;
	
	err = IPCMessageCreate( &newMsg );
	require_noerr( err, exit );
	newMsg->header.status = inMsg->header.status;
	
	err = IPCMessageSetBodyLength( newMsg, inMsg->bodyLen );
	require_noerr( err, exit );
	memcpy( newMsg->bodyPtr, inMsg->bodyPtr, inMsg->bodyLen );
	
	*outMsg = newMsg;
	newMsg = NULL;
	
exit:
	CFReleaseNullSafe( newMsg );
	return( err );
}

//===========================================================================================================================
//	_IPCMessageFinalize
//===========================================================================================================================

static void	_IPCMessageFinalize( CFTypeRef inCF )
{
	IPCMessageRef const		obj = (IPCMessageRef) inCF;
	
	_IPCMessageReset( obj );
}

//===========================================================================================================================
//	_IPCMessageReset
//===========================================================================================================================

static void	_IPCMessageReset( IPCMessageRef inMsg )
{
	inMsg->headerRead	= false;
	inMsg->readOffset	= 0;
	inMsg->bodyPtr		= inMsg->smallBodyBuf;
	inMsg->bodyLen		= 0;
	ForgetMem( &inMsg->bigBodyBuf );
}

//===========================================================================================================================
//	IPCMessageSetBodyLength
//===========================================================================================================================

OSStatus	IPCMessageSetBodyLength( IPCMessageRef inMsg, size_t inLen )
{
	OSStatus		err;
	
	ForgetMem( &inMsg->bigBodyBuf );
	if( inLen <= sizeof( inMsg->smallBodyBuf ) )
	{
		inMsg->bodyPtr = inMsg->smallBodyBuf;
	}
	else
	{
		inMsg->bigBodyBuf = (uint8_t *) malloc( inLen );
		require_action( inMsg->bigBodyBuf, exit, err = kNoMemoryErr );
		inMsg->bodyPtr = inMsg->bigBodyBuf;
	}
	inMsg->bodyLen = inLen;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	IPCMessageSetContent
//===========================================================================================================================

OSStatus
	IPCMessageSetContent( 
		IPCMessageRef	inMsg, 
		uint32_t		inOpcode, 
		uint32_t		inFlags, 
		OSStatus		inStatus, 
		const void *	inBody, 
		size_t			inLen )
{
	OSStatus		err;
	
	inMsg->header.opcode = inOpcode;
	inMsg->header.flags  = inFlags;
	// inMsg->header.xid = xid; XID is set when the message is sent.
	inMsg->header.status = inStatus;
	inMsg->header.length = (uint32_t) inLen;
	
	err = IPCMessageSetBodyLength( inMsg, inLen );
	require_noerr( err, exit );
	if( inLen ) memcpy( inMsg->bodyPtr, inBody, inLen );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_IPCMessageReadMessage
//===========================================================================================================================

static OSStatus	_IPCMessageReadMessage( SocketRef inSock, IPCMessageRef inMsg )
{
	OSStatus		err;
	
	if( !inMsg->headerRead )
	{
		err = SocketReadData( inSock, &inMsg->header, sizeof( inMsg->header ), &inMsg->readOffset );
		require_noerr_quiet( err, exit );
		inMsg->headerRead = true;
		inMsg->readOffset = 0;
		
		err = IPCMessageSetBodyLength( inMsg, inMsg->header.length );
		require_noerr( err, exit );
	}
	
	err = SocketReadData( inSock, inMsg->bodyPtr, inMsg->bodyLen, &inMsg->readOffset );
	require_noerr_quiet( err, exit );
	
exit:
	return( err );
}
