/*
	File:    	SerialPortUtils.c
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
	
	Copyright (C) 2013-2015 Apple Inc. All Rights Reserved.
*/

#include "SerialPortUtils.h"

#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include "NetUtils.h"
#include "StringUtils.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

#if( TARGET_OS_DARWIN )
	#include <IOKit/serial/ioss.h>
#endif

//===========================================================================================================================
//	Internals
//===========================================================================================================================

#define kSerialStreamFlag_ReadLine		( 1 << 16 )		// Internal flag to indicate a request is a ReadLine request.

#if( defined( B230400 ) )
	#define BaudRateToConstant( X ) ( \
		( (X) == 9600 )		? B9600 : \
		( (X) == 19200 )	? B19200 : \
		( (X) == 38400 )	? B38400 : \
		( (X) == 57600 )	? B57600 : \
		( (X) == 115200 )	? B115200 : \
		( (X) == 230400 )	? B230400 : \
							  0 )
#else
	#define BaudRateToConstant( X ) ( \
		( (X) == 9600 )		? B9600 : \
		( (X) == 19200 )	? B19200 : \
		( (X) == 38400 )	? B38400 : \
		( (X) == 57600 )	? B57600 : \
		( (X) == 115200 )	? B115200 : \
							  0 )
#endif

typedef struct SerialStreamReadRequest *	SerialStreamReadRequestRef;
struct SerialStreamReadRequest
{
	SerialStreamReadRequestRef			next;			// Next request.
	SerialStreamRef						stream;			// Stream this request is for.
	SerialStreamFlags					flags;			// Flags for this request.
	size_t								minLen;			// Min bytes the caller is willing to accept.
	size_t								maxLen;			// Max bytes the callers buffer can hold.
	size_t								readLen;		// Number of bytes currently read so far.
	uint8_t *							bufferPtr;		// Ptr to read into. May be callers buffer or point to request->buffer.
	SerialStreamReadCompletion_f		completion;		// Function to call when done.
	void *								context;		// Context to pass to completion function.
	uint8_t								buffer[ 1 ];	// Variable length.
};

typedef struct SerialStreamWriteRequest *	SerialStreamWriteRequestRef;
struct SerialStreamWriteRequest
{
	SerialStreamWriteRequestRef			next;			// Next request.
	SerialStreamRef						stream;			// Stream this request is for.
	iovec_t								iov[ 1 ];		// iovec to describe a write when we've copied data into the request.
	int									ion;			// Number of items in "iop".
	iovec_t *							iop;			// Ptr items being processed. Updated as write happens.
	SerialStreamWriteCompletion_f		completion;		// Function to call when done.
	void *								context;		// Context to pass to completion function.
	uint8_t								buffer[ 1 ];	// Variable length.
};

struct SerialStreamPrivate
{
	CFRuntimeBase						base;			// CF type info. Must be first.
	dispatch_queue_t					queue;			// Queue to serialize operations.
	FDRef								serialFD;		// File descriptor for the serial port.
	int									serialRefCount;	// Number of references to serialFD.
	dispatch_source_t					readSource;		// GCD source for readability notification.
	Boolean								readSuspended;	// True if GCD read source has been suspended.
	uint8_t *							readBuffer;		// Buffer for reading.
	size_t								readMaxLen;		// Max number of bytes "readBuffer" can hold.
	size_t								readSrcOffset;	// Offset in "readBuffer" where read data is consumed.
	size_t								readDstOffset;	// Offset in "readBuffer" where incoming data is written.
	dispatch_source_t					writeSource;	// GCD source for writability notification.
	Boolean								writeSuspended;	// True if GCD write source has been suspended.
	SerialStreamReadRequestRef			readList;		// Ordered list of pending reads.
	SerialStreamReadRequestRef *		readNext;		// Ptr to append next read.
	SerialStreamWriteRequestRef			writeList;		// Ordered list of pending writes.
	SerialStreamWriteRequestRef *		writeNext;		// Ptr to append next write.
	
	// Settings
	
	char								serialPath[ PATH_MAX + 1 ]; // Path to the serial port.
	int									baudRate;		// Bits per second to run the serial port at.
	int									flowControl;	// Flow control to use.
};

typedef struct
{
	dispatch_semaphore_t		sem;
	OSStatus					err;
	char *						buf;
	size_t						len;
	
}	SerialStreamReadContext;

static void		_SerialStreamGetTypeID( void *inContext );
static void		_SerialStreamFinalize( CFTypeRef inCF );
static void		_SerialStreamInvalidate( void *inContext );
static void		_SerialStreamRead( void *inArg );
static void		_SerialStreamReadHandler( void *inContext );
static void		_SerialStreamReadCompleted( SerialStreamRef me, SerialStreamReadRequestRef inRequest, OSStatus inStatus );
static void		_SerialStreamReadLineSyncCompletion( OSStatus inStatus, void *inBuffer, size_t inLen, void *inContext );
static void		_SerialStreamReadSyncCompletion( OSStatus inStatus, void *inBuffer, size_t inLen, void *inContext );
static void		_SerialStreamWrite( void *inArg );
static void		_SerialStreamWriteHandler( void *inContext );
static void		_SerialStreamWriteCompleted( SerialStreamRef me, SerialStreamWriteRequestRef inRequest, OSStatus inStatus );
static void		_SerialStreamWriteSyncCompletion( OSStatus inStatus, void *inContext );
static void		_SerialStreamCancelHandler( void *inContext );
static void		_SerialStreamErrorHandler( SerialStreamRef me, OSStatus inError );
static OSStatus	_SerialStreamEnsureSetUp( SerialStreamRef me );

static const CFRuntimeClass		kSerialStreamClass = 
{
	0,						// version
	"SerialStream",			// className
	NULL,					// init
	NULL,					// copy
	_SerialStreamFinalize,	// finalize
	NULL,					// equal -- NULL means pointer equality.
	NULL,					// hash  -- NULL means pointer hash.
	NULL,					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

static dispatch_once_t		gSerialStreamInitOnce	= 0;
static CFTypeID				gSerialStreamTypeID		= _kCFRuntimeNotATypeID;

//===========================================================================================================================
//	Logging
//===========================================================================================================================

ulog_define( SerialUtils, kLogLevelTrace, kLogFlags_Default, "SerialUtils", NULL );
#define ss_dlog( LEVEL, ... )		dlogc( &log_category_from_name( SerialUtils ), (LEVEL), __VA_ARGS__ )
#define ss_ulog( LEVEL, ... )		ulog( &log_category_from_name( SerialUtils ), (LEVEL), __VA_ARGS__ )

//===========================================================================================================================
//	SerialStreamGetTypeID
//===========================================================================================================================

CFTypeID	SerialStreamGetTypeID( void )
{
	dispatch_once_f( &gSerialStreamInitOnce, NULL, _SerialStreamGetTypeID );
	return( gSerialStreamTypeID );
}

static void _SerialStreamGetTypeID( void *inContext )
{
	(void) inContext;
	
	gSerialStreamTypeID = _CFRuntimeRegisterClass( &kSerialStreamClass );
	check( gSerialStreamTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	SerialStreamCreate
//===========================================================================================================================

OSStatus	SerialStreamCreate( SerialStreamRef *outStream )
{
	OSStatus			err;
	SerialStreamRef		me;
	size_t				extraLen;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (SerialStreamRef) _CFRuntimeCreateInstance( NULL, SerialStreamGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->queue = dispatch_get_main_queue();
	arc_safe_dispatch_retain( me->queue );
	
	me->serialFD	= kInvalidFD;
	me->readNext	= &me->readList;
	me->writeNext	= &me->writeList;
	
	*outStream = me;
	me = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( me );
	return( err );
}

//===========================================================================================================================
//	_SerialStreamFinalize
//===========================================================================================================================

static void	_SerialStreamFinalize( CFTypeRef inCF )
{
	SerialStreamRef const		me = (SerialStreamRef) inCF;
	
	check( !IsValidFD( me->serialFD ) );
	check( !me->serialRefCount );
	check( !me->readSource );
	check( !me->readBuffer );
	check( !me->writeSource );
	check( !me->readList );
	check( !me->writeList );
	
	dispatch_forget( &me->queue );
}

//===========================================================================================================================
//	SerialStreamSetConfig
//===========================================================================================================================

OSStatus	SerialStreamSetConfig( SerialStreamRef me, const SerialStreamConfig *inConfig )
{
	OSStatus		err;
	
	require_action( inConfig->devicePath, exit, err = kPathErr );
	strlcpy( me->serialPath, inConfig->devicePath, sizeof( me->serialPath ) );
	me->baudRate	= inConfig->baudRate;
	me->flowControl	= inConfig->flowControl;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	SerialStreamSetDispatchQueue
//===========================================================================================================================

void	SerialStreamSetDispatchQueue( SerialStreamRef me, dispatch_queue_t inQueue )
{
	ReplaceDispatchQueue( &me->queue, inQueue );
}

//===========================================================================================================================
//	SerialStreamInvalidate
//===========================================================================================================================

void	SerialStreamInvalidate( SerialStreamRef me )
{
	CFRetain( me );
	dispatch_async_f( me->queue, me, _SerialStreamInvalidate );
}

static void	_SerialStreamInvalidate( void *inContext )
{
	SerialStreamRef const		me = (SerialStreamRef) inContext;
	
	_SerialStreamErrorHandler( me, kCanceledErr );
	CFRelease( me );
}

//===========================================================================================================================
//	SerialStreamRead
//===========================================================================================================================

OSStatus
	SerialStreamRead( 
		SerialStreamRef					me, 
		size_t							inMinLen, 
		size_t							inMaxLen, 
		void *							inBuffer, 
		SerialStreamReadCompletion_f	inCompletion, 
		void *							inContext )
{
	OSStatus						err;
	size_t							len;
	SerialStreamReadRequestRef		request;
	
	len = offsetof( struct SerialStreamReadRequest, buffer );
	if( !inBuffer ) len += inMaxLen;
	request = (SerialStreamReadRequestRef) malloc( len );
	require_action( request, exit, err = kNoMemoryErr );
	
	CFRetain( me );
	request->stream		= me;
	request->flags		= 0;
	request->minLen		= inMinLen;
	request->maxLen		= inMaxLen;
	request->readLen	= 0;
	request->bufferPtr	= inBuffer ? inBuffer : request->buffer;
	request->completion	= inCompletion;
	request->context	= inContext;
	dispatch_async_f( me->queue, request, _SerialStreamRead );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	SerialStreamReadLine
//===========================================================================================================================

OSStatus
	SerialStreamReadLine( 
		SerialStreamRef					me, 
		SerialStreamFlags				inFlags, 
		SerialStreamReadCompletion_f	inCompletion, 
		void *							inContext )
{
	OSStatus						err;
	SerialStreamReadRequestRef		request;
	
	request = (SerialStreamReadRequestRef) malloc( offsetof( struct SerialStreamReadRequest, buffer ) );
	require_action( request, exit, err = kNoMemoryErr );
	
	CFRetain( me );
	request->stream		= me;
	request->flags		= inFlags | kSerialStreamFlag_ReadLine;
	request->minLen		= 0;
	request->maxLen		= 0;
	request->readLen	= 0;
	request->bufferPtr	= NULL;
	request->completion	= inCompletion;
	request->context	= inContext;
	dispatch_async_f( me->queue, request, _SerialStreamRead );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_SerialStreamRead
//===========================================================================================================================

static void	_SerialStreamRead( void *inArg )
{
	SerialStreamReadRequestRef const		request	= (SerialStreamReadRequestRef) inArg;
	SerialStreamRef const					me		= request->stream;
	OSStatus								err;
	
	request->next = NULL;
	*me->readNext = request;
	me->readNext  = &request->next;
	
	err = _SerialStreamEnsureSetUp( me );
	require_noerr( err, exit );
	
	_SerialStreamReadHandler( me );
	
exit:
	if( err ) _SerialStreamErrorHandler( me, err );
}

//===========================================================================================================================
//	_SerialStreamReadHandler
//===========================================================================================================================

static void	_SerialStreamReadHandler( void *inContext )
{
	SerialStreamRef const			me = (SerialStreamRef) inContext;
	SerialStreamReadRequestRef		request;
	OSStatus						err;
	size_t							len, avail;
	uint8_t *						src;
	const uint8_t *					end;
	uint8_t *						ptr;
	uint8_t *						dst;
	ssize_t							n;
	Boolean							found;
	
	for( ;; )
	{
		request = me->readList;
		require_action_quiet( request, exit, err = kNoErr );
		
		avail = me->readDstOffset - me->readSrcOffset;
		if( request->flags & kSerialStreamFlag_ReadLine )
		{
			found = false;
			src = &me->readBuffer[ me->readSrcOffset ];
			if( request->flags & kSerialStreamFlags_CRorBell )
			{
				end = src + avail;
				for( ptr = src; ( ptr < end ) && ( *ptr != '\r' ) && ( *ptr != kASCII_BellChar ); ++ptr ) {}
				if( ptr < end )
				{
					me->readSrcOffset  += (size_t)( ( ptr + 1 ) - src );
					request->bufferPtr	= src;
					request->readLen	= (size_t)( ( *ptr == kASCII_BellChar ) ? ( ( ptr + 1 ) - src ) : ( ptr - src ) );
					found				= true;
				}
			}
			else
			{
				ptr = memchr( src, ( request->flags & kSerialStreamFlags_CR ) ? '\r' : '\n', avail );
				if( ptr )
				{
					me->readSrcOffset  += (size_t)( ( ptr + 1 ) - src );
					if( ( ptr > src ) && ( ptr[ -1 ] == '\r' ) ) --ptr;
					request->bufferPtr	= src;
					request->readLen	= (size_t)( ptr - src );
					found				= true;
				}
			}
			if( !found )
			{
				if( me->readSrcOffset == me->readDstOffset )
				{
					me->readSrcOffset = 0;
					me->readDstOffset = 0;
				}
				len = me->readMaxLen - me->readDstOffset;
				if( ( len == 0 ) && ( me->readSrcOffset > 0 ) )
				{
					len = me->readDstOffset - me->readSrcOffset;
					memmove( me->readBuffer, &me->readBuffer[ me->readSrcOffset ], len );
					len = me->readSrcOffset;
					me->readDstOffset -= len;
					me->readSrcOffset = 0;
				}
				else if( len == 0 )
				{
					require_action_quiet( !me->readBuffer, exit, err = kNoSpaceErr );
					me->readMaxLen = 4096;
					me->readBuffer = malloc( me->readMaxLen );
					require_action( me->readBuffer, exit, err = kNoMemoryErr );
					len = me->readMaxLen;
				}
				dst = &me->readBuffer[ me->readDstOffset ];
				do
				{
					n = read( me->serialFD, dst, len );
					err = map_global_value_errno( n >= 0, n );
					
				}	while( err == EINTR );
				if( n == 0 ) err = EWOULDBLOCK;
				if( err == EWOULDBLOCK ) goto exit;
				require_noerr_quiet( err, exit );
				me->readDstOffset += ( (size_t) n );
				continue;
			}
		}
		else
		{
			if( avail > 0 )
			{
				len = request->maxLen - request->readLen;
				if( len > avail ) len = avail;
				memcpy( &request->bufferPtr[ request->readLen ], &me->readBuffer[ me->readSrcOffset ], len );
				me->readSrcOffset += len;
				request->readLen  += len;
			}
			if( request->readLen < request->minLen )
			{
				len = request->maxLen - request->readLen;
				dst = &request->bufferPtr[ request->readLen ];
				do
				{
					n = read( me->serialFD, dst, len );
					err = map_global_value_errno( n >= 0, n );
					
				}	while( err == EINTR );
				if( n == 0 ) err = EWOULDBLOCK;
				if( err == EWOULDBLOCK ) goto exit;
				require_noerr( err, exit );
				request->readLen += ( (size_t) n );
				if( request->readLen < request->minLen ) continue;
			}
		}
		
		_SerialStreamReadCompleted( me, request, kNoErr );
	}
	
exit:
	if( err == EWOULDBLOCK )	dispatch_resume_if_suspended( me->readSource, &me->readSuspended );
	else if( !err )				dispatch_suspend_if_resumed( me->readSource, &me->readSuspended );
	else						_SerialStreamErrorHandler( me, err );
}

//===========================================================================================================================
//	_SerialStreamReadCompleted
//===========================================================================================================================

static void	_SerialStreamReadCompleted( SerialStreamRef me, SerialStreamReadRequestRef inRequest, OSStatus inStatus )
{
	if( ( me->readList = inRequest->next ) == NULL )
		  me->readNext = &me->readList;
	inRequest->completion( inStatus, inRequest->bufferPtr, inRequest->readLen, inRequest->context );
	CFRelease( inRequest->stream );
	free( inRequest );
}

//===========================================================================================================================
//	SerialStreamReadLineSync
//===========================================================================================================================

OSStatus	SerialStreamReadLineSync( SerialStreamRef me, SerialStreamFlags inFlags, char **outLine, size_t *outLen )
{
	SerialStreamReadContext		ctx;
	
	ctx.sem = dispatch_semaphore_create( 0 );
	require_action( ctx.sem, exit, ctx.err = kUnknownErr );
	
	ctx.buf = NULL;
	ctx.len = 0;
	ctx.err = SerialStreamReadLine( me, inFlags, _SerialStreamReadLineSyncCompletion, &ctx );
	require_noerr( ctx.err, exit );
	dispatch_semaphore_wait( ctx.sem, DISPATCH_TIME_FOREVER );
	
	*outLine = ctx.buf;
	if( outLen ) *outLen = ctx.len;
	
exit:
	dispatch_forget( &ctx.sem );
	return( ctx.err );
}

static void	_SerialStreamReadLineSyncCompletion( OSStatus inStatus, void *inBuffer, size_t inLen, void *inContext )
{
	SerialStreamReadContext * const		ctx = (SerialStreamReadContext *) inContext;
	char *								buf = NULL;
	
	if( !inStatus )
	{
		buf = (char *) malloc( inLen + 1 );
		check( buf );
		if( buf )
		{
			if( inLen > 0 ) memcpy( buf, inBuffer, inLen );
			buf[ inLen ] = '\0';
		}
		else
		{
			inStatus = kNoMemoryErr;
		}
	}
	
	ctx->err = inStatus;
	ctx->buf = buf;
	ctx->len = inLen;
	dispatch_semaphore_signal( ctx->sem );
}

//===========================================================================================================================
//	SerialStreamReadSync
//===========================================================================================================================

OSStatus
	SerialStreamReadSync( 
		SerialStreamRef	me, 
		size_t			inMinLen, 
		size_t			inMaxLen, 
		void *			inBuffer, 
		size_t *		outLen )
{
	SerialStreamReadContext		ctx;
	
	ctx.sem = dispatch_semaphore_create( 0 );
	require_action( ctx.sem, exit, ctx.err = kUnknownErr );
	
	ctx.err = SerialStreamRead( me, inMinLen, inMaxLen, inBuffer, _SerialStreamReadSyncCompletion, &ctx );
	require_noerr( ctx.err, exit );
	
	dispatch_semaphore_wait( ctx.sem, DISPATCH_TIME_FOREVER );
	require_noerr( ctx.err, exit );
	if( outLen ) *outLen = ctx.len;
	
exit:
	dispatch_forget( &ctx.sem );
	return( ctx.err );
}

static void	_SerialStreamReadSyncCompletion( OSStatus inStatus, void *inBuffer, size_t inLen, void *inContext )
{
	SerialStreamReadContext * const		ctx = (SerialStreamReadContext *) inContext;
	
	(void) inBuffer;
	
	ctx->err = inStatus;
	ctx->len = inLen;
	dispatch_semaphore_signal( ctx->sem );
}

//===========================================================================================================================
//	SerialStreamWrite
//===========================================================================================================================

OSStatus
	SerialStreamWrite( 
		SerialStreamRef					me, 
		SerialStreamFlags				inFlags, 
		iovec_t *						inArray, 
		int								inCount, 
		SerialStreamWriteCompletion_f	inCompletion, 
		void *							inContext )
{
	OSStatus						err;
	size_t							len;
	SerialStreamWriteRequestRef		request;
	int								i;
	size_t							offset;
	
	len = offsetof( struct SerialStreamWriteRequest, buffer );
	if( inFlags & kSerialStreamFlags_Copy )
	{
		for( i = 0; i < inCount; ++i ) len += inArray[ i ].iov_len;
		request = (SerialStreamWriteRequestRef) malloc( len );
		require_action( request, exit, err = kNoMemoryErr );
		
		offset = 0;
		for( i = 0; i < inCount; ++i )
		{
			len = inArray[ i ].iov_len;
			memcpy( &request->buffer[ offset ], inArray[ i ].iov_base, len );
			offset += len;
		}
		request->iov[ 0 ].iov_base = request->buffer;
		request->iov[ 0 ].iov_len  = offset;
		request->ion = 1;
		request->iop = request->iov;
	}
	else
	{
		request = (SerialStreamWriteRequestRef) malloc( len );
		require_action( request, exit, err = kNoMemoryErr );
		request->ion = inCount;
		request->iop = inArray;
	}
	
	CFRetain( me );
	request->stream		= me;
	request->completion	= inCompletion;
	request->context	= inContext;
	
	dispatch_async_f( me->queue, request, _SerialStreamWrite );
	err = kNoErr;
	
exit:
	return( err );
}

static void	_SerialStreamWrite( void *inArg )
{
	SerialStreamWriteRequestRef const		request	= (SerialStreamWriteRequestRef) inArg;
	SerialStreamRef const					me		= request->stream;
	OSStatus								err;
	
	request->next  = NULL;
	*me->writeNext = request;
	me->writeNext  = &request->next;
	
	err = _SerialStreamEnsureSetUp( me );
	require_noerr( err, exit );
	
	_SerialStreamWriteHandler( me );
	
exit:
	if( err ) _SerialStreamErrorHandler( me, err );
}

//===========================================================================================================================
//	_SerialStreamWriteHandler
//===========================================================================================================================

static void	_SerialStreamWriteHandler( void *inContext )
{
	SerialStreamRef const			me = (SerialStreamRef) inContext;
	SerialStreamWriteRequestRef		request;
	OSStatus						err;
	ssize_t							n;
	
	for( ;; )
	{
		request = me->writeList;
		require_action_quiet( request, exit, err = kNoErr );
		do
		{
			n = writev( me->serialFD, request->iop, request->ion );
			err = map_global_value_errno( n >= 0, n );
			
		}	while( err == EINTR );
		if( n == 0 ) err = EWOULDBLOCK;
		if( err == EWOULDBLOCK ) goto exit;
		require_noerr( err, exit );
		
		err = UpdateIOVec( &request->iop, &request->ion, (size_t) n );
		if( err == EWOULDBLOCK ) continue;
		require_noerr( err, exit );
		
		_SerialStreamWriteCompleted( me, request, kNoErr );
	}
	
exit:
	if( err == EWOULDBLOCK )	dispatch_resume_if_suspended( me->writeSource, &me->writeSuspended );
	else if( !err )				dispatch_suspend_if_resumed( me->writeSource, &me->writeSuspended );
	else						_SerialStreamErrorHandler( me, err );
}

//===========================================================================================================================
//	_SerialStreamWriteCompleted
//===========================================================================================================================

static void	_SerialStreamWriteCompleted( SerialStreamRef me, SerialStreamWriteRequestRef inRequest, OSStatus inStatus )
{
	if( ( me->writeList = inRequest->next ) == NULL )
		  me->writeNext = &me->writeList;
	if( inRequest->completion ) inRequest->completion( inStatus, inRequest->context );
	CFRelease( inRequest->stream );
	free( inRequest );
}

//===========================================================================================================================
//	SerialStreamWriteSync
//===========================================================================================================================

typedef struct
{
	dispatch_semaphore_t		sem;
	OSStatus					err;
	
}	SerialStreamWriteContext;

OSStatus	SerialStreamWriteSync( SerialStreamRef me, iovec_t *inArray, int inCount )
{
	SerialStreamWriteContext		ctx;
	
	ctx.sem = dispatch_semaphore_create( 0 );
	require_action( ctx.sem, exit, ctx.err = kUnknownErr );
	
	ctx.err = SerialStreamWrite( me, kSerialStreamFlags_None, inArray, inCount, _SerialStreamWriteSyncCompletion, &ctx );
	require_noerr( ctx.err, exit );
	
	dispatch_semaphore_wait( ctx.sem, DISPATCH_TIME_FOREVER );
	require_noerr( ctx.err, exit );
	
exit:
	dispatch_forget( &ctx.sem );
	return( ctx.err );
}

static void	_SerialStreamWriteSyncCompletion( OSStatus inStatus, void *inContext )
{
	SerialStreamWriteContext * const		ctx = (SerialStreamWriteContext *) inContext;
	
	ctx->err = inStatus;
	dispatch_semaphore_signal( ctx->sem );
}

//===========================================================================================================================
//	_SerialStreamCancelHandler
//===========================================================================================================================

static void	_SerialStreamCancelHandler( void *inContext )
{
	SerialStreamRef const		me = (SerialStreamRef) inContext;
	
	if( --me->serialRefCount == 0 ) ForgetSocket( &me->serialFD );
	CFRelease( me );
}

//===========================================================================================================================
//	_SerialStreamErrorHandler
//===========================================================================================================================

static void	_SerialStreamErrorHandler( SerialStreamRef me, OSStatus inError )
{
	SerialStreamReadRequestRef		readRequest;
	SerialStreamWriteRequestRef		writeRequest;
	
	dispatch_source_forget_ex( &me->readSource,  &me->readSuspended );
	dispatch_source_forget_ex( &me->writeSource, &me->writeSuspended );
	
	while( ( readRequest = me->readList ) != NULL )
	{
		readRequest->readLen = 0;
		_SerialStreamReadCompleted( me, readRequest, inError );
	}
	while( ( writeRequest = me->writeList ) != NULL )
	{
		_SerialStreamWriteCompleted( me, writeRequest, inError );
	}
	
	ForgetMem( &me->readBuffer );
}

//===========================================================================================================================
//	_SerialStreamEnsureSetUp
//===========================================================================================================================

static OSStatus	_SerialStreamEnsureSetUp( SerialStreamRef me )
{
	OSStatus			err;
	struct termios		tio;
	int					i;
	
	require_action_quiet( !IsValidFD( me->serialFD ), exit, err = kNoErr );
	
	// Set up the serial port. Note: this explicitly marks it non-blocking after opening because
	// the behavior of O_NONBLOCK outside of the context of the open call is not defined.
	
	ss_ulog( kLogLevelVerbose, "Opening serial port '%s'\n", me->serialPath );
	for( i = 1; i < 10; ++i )
	{
		me->serialFD = open( me->serialPath, O_RDWR | O_NOCTTY | O_NONBLOCK );
		err = map_fd_creation_errno( me->serialFD );
		if( !err ) break;
		ss_ulog( kLogLevelNotice, "### Open '%s' error %d of 10: %#m\n", me->serialPath, i, err );
		if( err == EBUSY ) { usleep( 100000 ); continue; }
		goto exit;
	}
	if( i > 1 ) ss_ulog( kLogLevelNotice, "Opened '%s' after %d attempts\n", me->serialPath, i );
	
	err = ioctl( me->serialFD, TIOCEXCL );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
	
	err = fcntl( me->serialFD, F_SETFL, fcntl( me->serialFD, F_GETFL, 0 ) | O_NONBLOCK );
	err = map_global_value_errno( err != -1, err );
	require_noerr( err, exit );
	
	// Configure the port.
	
	memset( &tio, 0, sizeof( tio ) );
	err = tcgetattr( me->serialFD, &tio );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
	
	tio.c_iflag &= ~ICRNL;							// Don't map CR to NL on input.
	tio.c_iflag &= ~( IXON | IXOFF | IXANY );		// Disable software flow control by default;
	tio.c_oflag &= ~OPOST;							// Disable output processing.
	tio.c_oflag &= ~ONLCR;							// Don't map CR to NL on output.
	tio.c_cflag |= CLOCAL;							// Ignore modem status lines.
	tio.c_cflag |= CREAD;							// Enable receiver.
#if( defined( CRTSCTS ) )
	tio.c_cflag &= ~CRTSCTS;						// Disable hardware flow control by default.
#endif
#if( defined( IHFLOW ) )
	tio.c_cflag &= ~IHFLOW;							// Disable input hardware flow control by default.
#endif
#if( defined( OHFLOW ) )
	tio.c_cflag &= ~OHFLOW;							// Disable output hardware flow control by default.
#endif
	
	if( me->flowControl == kSerialFlowControl_XOnOff )
	{
		tio.c_iflag |= ( IXON | IXOFF | IXANY );	// Enable Xon/Xoff flow control.
	}
	else if( me->flowControl == kSerialFlowControl_HW )
	{
		#if( defined( CRTSCTS ) )
			tio.c_cflag |= CRTSCTS;					// Enable hardware flow control.
		#endif
		#if( defined( IHFLOW ) )
			tio.c_cflag |= IHFLOW;					// Enable input hardware flow control.
		#endif
		#if( defined( OHFLOW ) )
			tio.c_cflag |= OHFLOW;					// Enable output hardware flow control.
		#endif
	}
	tio.c_cflag &= ~( PARENB | PARODD );			// No parity.
	tio.c_cflag &= ~CSTOPB;							// 1 stop bit.
	tio.c_cflag = ( tio.c_cflag & ~CSIZE ) | CS8;	// 8 data bits.
	tio.c_lflag &= ~ICANON;							// Disable canonical mode.
	tio.c_lflag &= ~( ECHO | ECHOE );				// Disable echo.
	tio.c_lflag &= ~ISIG;							// Disable input signal generation.
	tio.c_cflag |= HUPCL;							// Lower model control lines when last process closes device.
	tio.c_cc[ VMIN ]  = 0;							// Always wake up immediately if we receive a byte.
    tio.c_cc[ VTIME ] = 0;							// Don't delay to coalesce bytes.
	
#if( !defined( IOSSIOSPEED ) )
{
	int		speed;
	
	speed = BaudRateToConstant( me->baudRate );
	require_action( speed > 0, exit, err = kUnsupportedErr );
	cfsetispeed( &tio, speed );
	cfsetospeed( &tio, speed );
}
#endif
	
	err = tcsetattr( me->serialFD, TCSANOW, &tio );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
	
#if( defined( IOSSIOSPEED ) )
{
	speed_t		baudRate;
	
	baudRate = (speed_t) me->baudRate;
	err = ioctl( me->serialFD, IOSSIOSPEED, &baudRate );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
}
#endif
	
	// Set up a source to notify us when readable.
	
	me->readSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, (uintptr_t) me->serialFD, 0, me->queue );
	require_action( me->readSource, exit, err = kUnknownErr );
	++me->serialRefCount;
	CFRetain( me );
	dispatch_set_context( me->readSource, me );
	dispatch_source_set_event_handler_f( me->readSource, _SerialStreamReadHandler );
	dispatch_source_set_cancel_handler_f( me->readSource, _SerialStreamCancelHandler );
	me->readSuspended = true; // Don't resume until we get read request.
	
	// Set up a source to notify us when writable.
	
	me->writeSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_WRITE, (uintptr_t) me->serialFD, 0, me->queue );
	require_action( me->writeSource, exit, err = kUnknownErr );
	++me->serialRefCount;
	CFRetain( me );
	dispatch_set_context( me->writeSource, me );
	dispatch_source_set_event_handler_f( me->writeSource, _SerialStreamWriteHandler );
	dispatch_source_set_cancel_handler_f( me->writeSource, _SerialStreamCancelHandler );
	me->writeSuspended = true; // Don't resume until we get EWOULDBLOCK on a write.
	
exit:
	if( err )
	{
		if( me->serialRefCount == 0 ) ForgetFD( &me->serialFD );
		_SerialStreamErrorHandler( me, err );
	}
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	SerialStreamTest
//===========================================================================================================================

#if( !EXCLUDE_UNIT_TESTS )
OSStatus	SerialStreamTest( void );
OSStatus	SerialStreamTest( void )
{
	OSStatus				err;
	SerialStreamRef			stream;
	dispatch_queue_t		queue;
	SerialStreamConfig		config;
	iovec_t					iov[ 1 ];
	uint8_t					buf[ 128 ];
	
	err = SerialStreamCreate( &stream );
	require_noerr( err, exit );
	
	queue = dispatch_queue_create( "SerialStreamTest", NULL );
	require_action( queue, exit, err = kUnknownErr );
	SerialStreamSetDispatchQueue( stream, queue );
	arc_safe_dispatch_release( queue );
	
	SerialStreamConfigInit( &config );
	config.devicePath	= "/dev/cu.usbserial-A900exbk";
	config.baudRate		= 19200;
	err = SerialStreamSetConfig( stream, &config );
	require_noerr( err, exit );
	
	SETIOV( &iov[ 0 ], "\x02\x60", 2 );
	err = SerialStreamWriteSync( stream, iov, 1 );
	require_noerr( err, exit );
	
	memset( buf, 0, sizeof( buf ) );
	err = SerialStreamReadSync( stream, 7, 7, buf, NULL );
	require_noerr( err, exit );
	dlog( kLogLevelNotice, "%.3H\n", buf, 7, 7 );
	
	SerialStreamForget( &stream );
	usleep( 500000 );
	
exit:
	SerialStreamForget( &stream );
	printf( "SerialStreamTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
