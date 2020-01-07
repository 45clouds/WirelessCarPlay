/*
	File:    	NetTransportTLS.c
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
	
	Copyright (C) 2015 Apple Inc. All Rights Reserved.
*/

#include "NetTransportTLS.h"

#include <Security/Security.h>
#include <Security/SecureTransportPriv.h>

#include "NetUtils.h"

//===========================================================================================================================
//	Internals
//===========================================================================================================================

typedef struct
{
	SSLContextRef		sslCtx;
	SocketRef			sock;
	Boolean				handshakeComplete;
	Boolean				readWouldBlock;
	Boolean				writeWouldBlock;
	
}	NetTransportTLSContext;

static OSStatus	_NetTransportInitialize( SocketRef inSock, void *inContext );
static void		_NetTransportFinalize( void *inContext );
static OSStatus	_NetTransportRead( void *inBuffer, size_t inMaxLen, size_t *outLen, void *inContext );
static OSStatus	_NetTransportWriteV( iovec_t **ioArray, int *ioCount, void *inContext );
static OSStatus	_SecureTransportRead( SSLConnectionRef inCnx, void *inBuffer, size_t *ioLen );
static OSStatus	_SecureTransportWrite( SSLConnectionRef inCnx, const void *inBuffer, size_t *ioLen );
static OSStatus	_MapWouldBlockError( NetTransportTLSContext *ctx );

// Logging

#define LOG_HANDSHAKE_READ		0 // Log read data before the handshake completes.
#if( LOG_HANDSHAKE_READ )
	#define tls_handshake_read_dlog( LEVEL, ... )		dlog( (LEVEL), __VA_ARGS__ )
#else
	#define tls_handshake_read_dlog( LEVEL, ... )		do {} while( 0 )
#endif

#define LOG_HANDSHAKE_WRITE		0 // Log read data before the handshake completes.
#if( LOG_HANDSHAKE_WRITE )
	#define tls_handshake_write_dlog( LEVEL, ... )		dlog( (LEVEL), __VA_ARGS__ )
#else
	#define tls_handshake_write_dlog( LEVEL, ... )		do {} while( 0 )
#endif

#define LOG_PRE_READ			0 // Log read data before it's been decrypted.
#if( LOG_PRE_READ )
	#define tls_pre_read_dlog( LEVEL, ... )		dlog( (LEVEL), __VA_ARGS__ )
#else
	#define tls_pre_read_dlog( LEVEL, ... )		do {} while( 0 )
#endif

#define LOG_POST_READ			0 // Log read data after it's been decrypted.
#if( LOG_POST_READ )
	#define tls_post_read_dlog( LEVEL, ... )	dlog( (LEVEL), __VA_ARGS__ )
#else
	#define tls_post_read_dlog( LEVEL, ... )	do {} while( 0 )
#endif

#define LOG_PRE_WRITE			0 // Log data to write before it's been encrypted.
#if( LOG_PRE_WRITE )
	#define tls_pre_write_dlog( LEVEL, ... )	dlog( (LEVEL), __VA_ARGS__ )
#else
	#define tls_pre_write_dlog( LEVEL, ... )	do {} while( 0 )
#endif

#define LOG_POST_WRITE			0 // Log data written after it's been encrypted.
#if( LOG_POST_WRITE )
	#define tls_post_write_dlog( LEVEL, ... )	dlog( (LEVEL), __VA_ARGS__ )
#else
	#define tls_post_write_dlog( LEVEL, ... )	do {} while( 0 )
#endif

#define LOG_OTHER				0 // Log non-read/write messages.
#if( LOG_OTHER )
	#define tls_other_dlog( LEVEL, ... )	dlog( (LEVEL), __VA_ARGS__ )
#else
	#define tls_other_dlog( LEVEL, ... )	do {} while( 0 )
#endif

//===========================================================================================================================
//	NetTransportTLSConfigure
//===========================================================================================================================

OSStatus	NetTransportTLSConfigure( NetTransportDelegate *ioDelegate, Boolean inIsClient )
{
	OSStatus						err;
	NetTransportTLSContext *		ctx;
	
	ctx = (NetTransportTLSContext *) calloc( 1, sizeof( *ctx ) );
	require_action( ctx, exit, err = kNoMemoryErr );
	
	ctx->sslCtx = SSLCreateContext( NULL, inIsClient ? kSSLClientSide : kSSLServerSide, kSSLStreamType );
	require_action( ctx->sslCtx, exit, err = kUnknownErr );
	
	err = SSLSetConnection( ctx->sslCtx, ctx );
	require_noerr( err, exit );
	
	err = SSLSetIOFuncs( ctx->sslCtx, _SecureTransportRead, _SecureTransportWrite );
	require_noerr( err, exit );
	
	err = SSLSetProtocolVersionMin( ctx->sslCtx, kTLSProtocol12 );
	require_noerr( err, exit );
	
	NetTransportDelegateInit( ioDelegate );
	ioDelegate->context			= ctx;
	ioDelegate->initialize_f	= _NetTransportInitialize;
	ioDelegate->finalize_f		= _NetTransportFinalize;
	ioDelegate->read_f			= _NetTransportRead;
	ioDelegate->writev_f		= _NetTransportWriteV;
	ctx = NULL;
	err = kNoErr;
	
exit:
	if( ctx ) _NetTransportFinalize( ctx );
	return( err );
}

//===========================================================================================================================
//	NetTransportTLSPSKConfigure
//===========================================================================================================================

OSStatus
	NetTransportTLSPSKConfigure( 
		NetTransportDelegate *	ioDelegate,
		const void *			inPSKPtr, 
		size_t					inPSKLen, 
		const void *			inClientIdentityPtr, 
		size_t					inClientIdentityLen, 
		Boolean					inClient )
{
	OSStatus						err;
	NetTransportTLSContext *		ctx;
	
	ctx = (NetTransportTLSContext *) calloc( 1, sizeof( *ctx ) );
	require_action( ctx, exit, err = kNoMemoryErr );
	
	ctx->sslCtx = SSLCreateContext( NULL, inClient ? kSSLClientSide : kSSLServerSide, kSSLStreamType );
	require_action( ctx->sslCtx, exit, err = kUnknownErr );
	
	err = SSLSetConnection( ctx->sslCtx, ctx );
	require_noerr( err, exit );
	
	err = SSLSetIOFuncs( ctx->sslCtx, _SecureTransportRead, _SecureTransportWrite );
	require_noerr( err, exit );
	
	err = SSLSetProtocolVersionMin( ctx->sslCtx, kTLSProtocol12 );
	require_noerr( err, exit );
	
	err = SSLSetPSKSharedSecret( ctx->sslCtx, inPSKPtr, inPSKLen );
	require_noerr( err, exit );
	
	if( inClientIdentityPtr )
	{
		err = SSLSetPSKIdentity( ctx->sslCtx, inClientIdentityPtr, inClientIdentityLen );
		require_noerr( err, exit );
	}
	
	err = SSLSetEnabledCiphers( ctx->sslCtx, (const SSLCipherSuite [])
		{
			TLS_PSK_WITH_AES_128_CBC_SHA256, 
			TLS_PSK_WITH_AES_128_GCM_SHA256, 
		}, 2 );
	require_noerr( err, exit );
	
	NetTransportDelegateInit( ioDelegate );
	ioDelegate->context			= ctx;
	ioDelegate->initialize_f	= _NetTransportInitialize;
	ioDelegate->finalize_f		= _NetTransportFinalize;
	ioDelegate->read_f			= _NetTransportRead;
	ioDelegate->writev_f		= _NetTransportWriteV;
	ctx = NULL;
	err = kNoErr;
	
exit:
	if( ctx ) _NetTransportFinalize( ctx );
	return( err );
}

//===========================================================================================================================
//	_NetTransportInitialize
//===========================================================================================================================

static OSStatus	_NetTransportInitialize( SocketRef inSock, void *inContext )
{
	NetTransportTLSContext * const		ctx = (NetTransportTLSContext *) inContext;
	
	ctx->sock = inSock;
	return( kNoErr );
}

//===========================================================================================================================
//	_NetTransportFinalize
//===========================================================================================================================

static void	_NetTransportFinalize( void *inContext )
{
	NetTransportTLSContext * const		ctx = (NetTransportTLSContext *) inContext;
	
	if( ctx )
	{
		ForgetCF( &ctx->sslCtx );
		free( ctx );
	}
}

//===========================================================================================================================
//	_NetTransportReadMessage
//===========================================================================================================================

static OSStatus	_NetTransportRead( void *inBuffer, size_t inMaxLen, size_t *outLen, void *inContext )
{
	NetTransportTLSContext * const		ctx = (NetTransportTLSContext *) inContext;
	OSStatus							err;
	size_t								len = 0;
	
	if( !ctx->handshakeComplete )
	{
		err = SSLHandshake( ctx->sslCtx );
		if( err == errSSLWouldBlock ) err = _MapWouldBlockError( ctx );
		else if( err ) tls_other_dlog( kLogLevelMax, "### SSLHandshake for read failed: %#m\n", err );
		require_noerr_quiet( err, exit );
		ctx->handshakeComplete = true;
		tls_other_dlog( kLogLevelMax, "TLS handshake completed for read\n" );
	}
	
	err = SSLRead( ctx->sslCtx, inBuffer, inMaxLen, &len );
	if( len > 0 ) err = kNoErr;
	if(      err == errSSLWouldBlock )		err = _MapWouldBlockError( ctx );
	else if( err == errSSLClosedGraceful )	err = kConnectionErr;
	if( !err ) tls_post_read_dlog( kLogLevelMax, "Post read:\n%1.2H\n", inBuffer, (int) len, 256 );
	
exit:
	*outLen = len;
	return( err );
}

//===========================================================================================================================
//	_NetTransportWriteV
//===========================================================================================================================

static OSStatus	_NetTransportWriteV( iovec_t **ioArray, int *ioCount, void *inContext )
{
	NetTransportTLSContext * const		ctx = (NetTransportTLSContext *) inContext;
	OSStatus							err;
	size_t								len;
	
	if( !ctx->handshakeComplete )
	{
		err = SSLHandshake( ctx->sslCtx );
		if( err == errSSLWouldBlock ) err = _MapWouldBlockError( ctx );
		else if( err ) tls_other_dlog( kLogLevelMax, "### SSLHandshake for write failed: %#m\n", err );
		require_noerr_quiet( err, exit );
		ctx->handshakeComplete = true;
		tls_other_dlog( kLogLevelMax, "TLS handshake completed for write\n" );
	}
	
	while( *ioCount > 0 )
	{
		tls_pre_read_dlog( kLogLevelMax, "Pre write:\n%1.2H\n", ( *ioArray )->iov_base, (int)( *ioArray )->iov_len, 256 );
		len = 0;
		err = SSLWrite( ctx->sslCtx, ( *ioArray )->iov_base, ( *ioArray )->iov_len, &len );
		if( len > 0 ) err = kNoErr;
		if(      err == errSSLWouldBlock )		err = _MapWouldBlockError( ctx );
		else if( err == errSSLClosedGraceful )	err = kConnectionErr;
		require_noerr_quiet( err, exit );
		
		err = UpdateIOVec( ioArray, ioCount, len );
		if( !err ) break;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_SecureTransportRead
//===========================================================================================================================

static OSStatus	_SecureTransportRead( SSLConnectionRef inCnx, void *inBuffer, size_t *ioLen )
{
	NetTransportTLSContext * const		ctx = (NetTransportTLSContext *) inCnx;
	OSStatus							err;
	ssize_t								n;
	size_t								len = 0;
	
	do
	{
		n = read_compat( ctx->sock, inBuffer, *ioLen );
		err = map_socket_value_errno( ctx->sock, n >= 0, n );
		
	}	while( err == EINTR );
	if( n > 0 )
	{
		len = (size_t) n;
		if( ctx->handshakeComplete )	tls_pre_read_dlog( kLogLevelMax, "Pre read:\n%1.2H\n", inBuffer, (int) len, 256 );
		else							tls_handshake_read_dlog( kLogLevelMax, "Handshake read:\n%1.2H\n", inBuffer, (int) len, 256 );
		if( len < *ioLen )
		{
			ctx->readWouldBlock = true;
			err = errSSLWouldBlock;
		}
		else
		{
			err = kNoErr;
		}
	}
	else if( n == 0 )
	{
		err = errSSLClosedGraceful;
	}
	else if( err == EWOULDBLOCK )
	{
		ctx->readWouldBlock = true;
		err = errSSLWouldBlock;
	}
	else
	{
		check( err != kNoErr );
	}
	*ioLen = len;
	return( err );
}

//===========================================================================================================================
//	_SecureTransportWrite
//===========================================================================================================================

static OSStatus	_SecureTransportWrite( SSLConnectionRef inCnx, const void *inBuffer, size_t *ioLen )
{
	NetTransportTLSContext * const		ctx = (NetTransportTLSContext *) inCnx;
	OSStatus							err;
	ssize_t								n;
	size_t								len = 0;
	
	if( ctx->handshakeComplete )	tls_pre_read_dlog( kLogLevelMax, "Post write:\n%1.2H\n", inBuffer, (int) *ioLen, 256 );
	else							tls_handshake_write_dlog( kLogLevelMax, "Handshake write:\n%1.2H\n", inBuffer, (int) *ioLen, 256 );
	do
	{
		n = write_compat( ctx->sock, inBuffer, *ioLen );
		err = map_socket_value_errno( ctx->sock, n >= 0, n );
		
	}	while( err == EINTR );
	if( n > 0 )
	{
		len = (size_t) n;
		if( len < *ioLen )
		{
			ctx->writeWouldBlock = true;
			err = errSSLWouldBlock;
		}
		else
		{
			err = kNoErr;
		}
	}
	else if( n == 0 )
	{
		err = errSSLClosedGraceful;
	}
	else if( err == EWOULDBLOCK )
	{
		ctx->writeWouldBlock = true;
		err = errSSLWouldBlock;
	}
	else
	{
		check( err != kNoErr );
	}
	*ioLen = len;
	return( err );
}

//===========================================================================================================================
//	_MapWouldBlockError
//===========================================================================================================================

static OSStatus	_MapWouldBlockError( NetTransportTLSContext *ctx )
{
	if( ctx->readWouldBlock && ctx->writeWouldBlock )
	{
		ctx->readWouldBlock  = false;
		ctx->writeWouldBlock = false;
		return( kWouldBlockErr );
	}
	else if( ctx->readWouldBlock )
	{
		ctx->readWouldBlock = false;
		return( kReadWouldBlockErr );
	}
	else if( ctx->writeWouldBlock )
	{
		ctx->writeWouldBlock = false;
		return( kWriteWouldBlockErr );
	}
	return( EWOULDBLOCK );
}

#if 0
#pragma mark -
#endif

#if( !EXCLUDE_UNIT_TESTS )

#include "HTTPClient.h"
#include "HTTPServer.h"
#include "PrintFUtils.h"

//===========================================================================================================================
//	NetTransportTLSTest
//===========================================================================================================================

#define kTestServerDNS		"www.google.com"
#define kTestServerPort		443
#define kTestServerPath		"/"

OSStatus	NetTransportTLSTest( void );
OSStatus	NetTransportTLSTest( void )
{
	OSStatus					err;
	HTTPClientRef				client	= NULL;
	HTTPMessageRef				msg		= NULL;
	dispatch_queue_t			queue;
	NetTransportDelegate		delegate;
	
	err = HTTPClientCreate( &client );
	require_noerr( err, exit );
	
	queue = dispatch_queue_create( "NetTransportTLSTest", NULL );
	require_action( queue, exit, err = -1 );
	HTTPClientSetDispatchQueue( client, queue );
	dispatch_release( queue );
	
	err = HTTPClientSetDestination( client, kTestServerDNS, kTestServerPort );
	require_noerr( err, exit );
	
	err = NetTransportTLSConfigure( &delegate, true );
	require_noerr( err, exit );
	HTTPClientSetTransportDelegate( client, &delegate );
	
	err = HTTPMessageCreate( &msg );
	require_noerr( err, exit );
	msg->userContext1 = client;
	
	HTTPHeader_InitRequest( &msg->header, "GET", kTestServerPath, "HTTP/1.1" );
	HTTPHeader_SetField( &msg->header, "Host", kTestServerDNS );
	err = HTTPClientSendMessageSync( client, msg );
	if( IsHTTPOSStatus_Success( err ) ) err = kNoErr;
	require_noerr( err, exit );
	FPrintF( stderr, "==> %s -> %#m\n", kTestServerDNS, msg->header.statusCode );
	FPrintF( stderr, "%.*s%.*s\n", (int) msg->header.len, msg->header.buf, (int) msg->bodyLen, msg->bodyPtr );
	
exit:
	HTTPClientForget( &client );
	ForgetCF( &msg );
	printf( "NetTransportTLSTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

//===========================================================================================================================
//	NetTransportTLSPSKTest
//===========================================================================================================================

OSStatus		NetTransportTLSPSKTest( void );
static OSStatus	_NetTransportTLSPSKTest_ServerInitConnection( HTTPConnectionRef inCnx, void *inContext );
static OSStatus	_NetTransportTLSPSKTest_HandleMessage( HTTPConnectionRef inCnx, HTTPMessageRef inMsg, void *inContext );

OSStatus	NetTransportTLSPSKTest( void )
{
	OSStatus					err;
	HTTPServerRef				server	= NULL;
	HTTPClientRef				client	= NULL;
	HTTPMessageRef				msg		= NULL;
	dispatch_queue_t			queue;
	HTTPServerDelegate			serverDelegate;
	NetTransportDelegate		transportDelegate;
	
	// Setup server.
	
	HTTPServerDelegateInit( &serverDelegate );
	serverDelegate.initializeConnection_f	= _NetTransportTLSPSKTest_ServerInitConnection;
	serverDelegate.handleMessage_f			= _NetTransportTLSPSKTest_HandleMessage;
	err = HTTPServerCreate( &server, &serverDelegate );
	require_noerr( err, exit );
	server->listenPort = 8123;
	
	queue = dispatch_queue_create( "NetTransportTLSPSKServerTest", NULL );
	require_action( queue, exit, err = -1 );
	HTTPServerSetDispatchQueue( server, queue );
	dispatch_release( queue );
	
	HTTPServerStart( server );
	
	// Setup client.
	
	err = HTTPClientCreate( &client );
	require_noerr( err, exit );
	
	queue = dispatch_queue_create( "NetTransportTLSPSKClientTest", NULL );
	require_action( queue, exit, err = -1 );
	HTTPClientSetDispatchQueue( client, queue );
	dispatch_release( queue );
	
	err = HTTPClientSetDestination( client, "http://127.0.0.1", server->listenPort );
	require_noerr( err, exit );
	
	err = NetTransportTLSPSKConfigure( &transportDelegate, 
		"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F", 16, 
		"TestClientIdentity", 18, 
		true );
	require_noerr( err, exit );
	HTTPClientSetTransportDelegate( client, &transportDelegate );
	
	err = HTTPMessageCreate( &msg );
	require_noerr( err, exit );
	msg->userContext1 = client;
	HTTPHeader_InitRequest( &msg->header, "GET", "/test", "HTTP/1.1" );
	err = HTTPClientSendMessageSync( client, msg );
	if( IsHTTPOSStatus_Success( err ) ) err = kNoErr;
	require_noerr( err, exit );
	require_action( msg->header.statusCode == kHTTPStatus_OK, exit, err = kResponseErr );
	require_action( MemEqual( msg->bodyPtr, msg->bodyLen, "/test", 5 ), exit, err = kResponseErr );
	
exit:
	HTTPClientForget( &client );
	HTTPServerForget( &server );
	ForgetCF( &msg );
	printf( "NetTransportTLSTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

//===========================================================================================================================
//	_NetTransportTLSPSKTest_ServerInitConnection
//===========================================================================================================================

static OSStatus	_NetTransportTLSPSKTest_ServerInitConnection( HTTPConnectionRef inCnx, void *inContext )
{
	OSStatus					err;
	NetTransportDelegate		delegate;
	
	(void) inContext;
	
	NetTransportDelegateInit( &delegate );
	err = NetTransportTLSPSKConfigure( &delegate, 
		"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F", 16, 
		"TestClientIdentity", 18, 
		false );
	require_noerr( err, exit );
	HTTPConnectionSetTransportDelegate( inCnx, &delegate );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_NetTransportTLSPSKTest_HandleMessage
//===========================================================================================================================

static OSStatus	_NetTransportTLSPSKTest_HandleMessage( HTTPConnectionRef inCnx, HTTPMessageRef inMsg, void *inContext )
{
	(void) inContext;
	
	return( HTTPConnectionSendSimpleResponse( inCnx, kHTTPStatus_OK, kMIMEType_TextPlain, 
		inMsg->header.urlPtr, inMsg->header.urlLen ) );
}
#endif // !EXCLUDE_UNIT_TESTS
