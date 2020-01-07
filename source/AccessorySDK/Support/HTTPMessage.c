/*
	File:    	HTTPMessage.c
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

#include "HTTPMessage.h"

#include "CommonServices.h"
#include "HTTPUtils.h"
#include "NetUtils.h"
#include "StringUtils.h"
#include "URLUtils.h"

#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

#if( COMPILER_HAS_BLOCKS )
	#include <Block.h>
#endif
#if( TARGET_OS_POSIX )
	#include <sys/stat.h>
	#include <unistd.h>
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

#define kFileWriteBufferLen		( 1 * kBytesPerMegaByte )

static void		_HTTPMessageGetTypeID( void *inContext );
static void		_HTTPMessageFinalize( CFTypeRef inCF );
static OSStatus	_HTTPMessageReadChunked( HTTPMessageRef inMsg, NetTransportRead_f inRead_f, void *inRead_ctx );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static dispatch_once_t			gHTTPMessageInitOnce = 0;
static CFTypeID					gHTTPMessageTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kHTTPMessageClass = 
{
	0,						// version
	"HTTPMessage",			// className
	NULL,					// init
	NULL,					// copy
	_HTTPMessageFinalize,	// finalize
	NULL,					// equal -- NULL means pointer equality.
	NULL,					// hash  -- NULL means pointer hash.
	NULL,					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

//===========================================================================================================================
//	HTTPMessageGetTypeID
//===========================================================================================================================

CFTypeID	HTTPMessageGetTypeID( void )
{
	dispatch_once_f( &gHTTPMessageInitOnce, NULL, _HTTPMessageGetTypeID );
	return( gHTTPMessageTypeID );
}

static void _HTTPMessageGetTypeID( void *inContext )
{
	(void) inContext;
	
	gHTTPMessageTypeID = _CFRuntimeRegisterClass( &kHTTPMessageClass );
	check( gHTTPMessageTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	HTTPMessageCreate
//===========================================================================================================================

OSStatus	HTTPMessageCreate( HTTPMessageRef *outMessage )
{
	OSStatus			err;
	HTTPMessageRef		me;
	size_t				extraLen;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (HTTPMessageRef) _CFRuntimeCreateInstance( NULL, HTTPMessageGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->fileFD = kInvalidFD;
	
	me->maxBodyLen = kHTTPDefaultMaxBodyLen;
	HTTPMessageReset( me );
	
	*outMessage = me;
	me = NULL;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_HTTPMessageFinalize
//===========================================================================================================================

static void	_HTTPMessageFinalize( CFTypeRef inCF )
{
	HTTPMessageRef const		me = (HTTPMessageRef) inCF;
	
	HTTPMessageReset( me );
	ForgetMem( &me->requestHeader );
	ForgetPtrLen( &me->requestBodyPtr, &me->requestBodyLen );
#if( COMPILER_HAS_BLOCKS )
	ForgetBlock( &me->completionBlock );
#endif
}

//===========================================================================================================================
//	HTTPMessageReset
//===========================================================================================================================

void	HTTPMessageReset( HTTPMessageRef inMsg )
{
	inMsg->header.len	= 0;
	inMsg->headerRead	= false;
	inMsg->chunkState	= kHTTPChunkState_Invalid;
	inMsg->bodyPtr		= inMsg->smallBodyBuf;
	inMsg->bodyLen		= 0;
	inMsg->bodyOffset	= 0;
	ForgetMem( &inMsg->bigBodyBuf );
	inMsg->timeoutNanos	= kHTTPNoTimeout;
	if( inMsg->closeFD ) ForgetFD( &inMsg->fileFD );
	inMsg->fileFD		= kInvalidFD;
	inMsg->closeFD		= false;
}

//===========================================================================================================================
//	HTTPMessageInitRequest
//===========================================================================================================================

OSStatus	HTTPMessageInitRequest( HTTPMessageRef inMsg, const char *inProtocol, const char *inMethod, const char *inFormat, ... )
{
	OSStatus		err;
	va_list			args;
	
	va_start( args, inFormat );
	err = HTTPHeader_InitRequestV( &inMsg->header, inProtocol, inMethod, inFormat, args );
	va_end( args );
	return( err );
}

//===========================================================================================================================
//	HTTPMessageInitResponse
//===========================================================================================================================

OSStatus	HTTPMessageInitResponse( HTTPMessageRef inMsg, const char *inProtocol, int inStatusCode, OSStatus inError )
{
	return( HTTPHeader_InitResponseEx( &inMsg->header, inProtocol, inStatusCode, NULL, inError ) );
}

//===========================================================================================================================
//	HTTPMessageGetHeaderField
//===========================================================================================================================

OSStatus
	HTTPMessageGetHeaderField( 
		HTTPMessageRef	inMsg, 
		const char *	inName, 
		const char **	outNamePtr, 
		size_t *		outNameLen, 
		const char **	outValuePtr, 
		size_t *		outValueLen )
{
	return( HTTPGetHeaderField( inMsg->header.buf, inMsg->header.len, inName, outNamePtr, outNameLen, 
		outValuePtr, outValueLen, NULL ) );
}

//===========================================================================================================================
//	HTTPMessageSetHeaderField
//===========================================================================================================================

OSStatus	HTTPMessageSetHeaderField( HTTPMessageRef inMsg, const char *inName, const char *inFormat, ... )
{
	OSStatus		err;
	va_list			args;
	
	va_start( args, inFormat );
	err = HTTPHeader_SetFieldV( &inMsg->header, inName, inFormat, args );
	va_end( args );
	return( err );
}

//===========================================================================================================================
//	HTTPMessageReadMessageEx
//===========================================================================================================================

OSStatus	HTTPMessageReadMessageEx( HTTPMessageRef inMsg, NetTransportRead_f inRead_f, void *inRead_ctx )
{
	HTTPHeader * const		hdr = &inMsg->header;
	OSStatus				err;
	size_t					len;
	
	if( !inMsg->headerRead )
	{
		err = HTTPReadHeader( hdr, inRead_f, inRead_ctx );
		require_noerr_quiet( err, exit );
		inMsg->headerRead = true;
		
		inMsg->chunkState = HTTPIsChunked( hdr->buf, hdr->len ) ? kHTTPChunkState_ReadingHeader : kHTTPChunkState_Invalid;
		if( inMsg->chunkState == kHTTPChunkState_Invalid )
		{
			require_action( hdr->contentLength <= inMsg->maxBodyLen, exit, err = kSizeErr );
			err = HTTPMessageSetBodyLength( inMsg, (size_t) hdr->contentLength );
			require_noerr( err, exit );
		}
	}
	if( inMsg->chunkState != kHTTPChunkState_Invalid )
	{
		err = _HTTPMessageReadChunked( inMsg, inRead_f, inRead_ctx );
		require_noerr_quiet( err, exit );
	}
	else
	{
		len = hdr->extraDataLen;
		if( len > 0 )
		{
			len = Min( len, inMsg->bodyLen );
			memmove( inMsg->bodyPtr, hdr->extraDataPtr, len );
			hdr->extraDataPtr += len;
			hdr->extraDataLen -= len;
			inMsg->bodyOffset += len;
		}
	
		len = inMsg->bodyOffset;
		if( len < inMsg->bodyLen )
		{
			err = inRead_f( inMsg->bodyPtr + len, inMsg->bodyLen - len, &len, inRead_ctx );
			require_noerr_quiet( err, exit );
			inMsg->bodyOffset += len;
		}
		err = ( inMsg->bodyOffset == inMsg->bodyLen ) ? kNoErr : EWOULDBLOCK;
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	_HTTPMessageReadChunked
//===========================================================================================================================

static OSStatus	_HTTPMessageReadChunked( HTTPMessageRef inMsg, NetTransportRead_f inRead_f, void *inRead_ctx )
{
	HTTPHeader * const		hdr = &inMsg->header;
	OSStatus				err;
	const char *			linePtr;
	size_t					lineLen;
	int						n;
	uint64_t				chunkLen;
	size_t					len;
	uint8_t *				tmp;
	
	for( ;; )
	{
		// ReadingHeader
		
		if( inMsg->chunkState == kHTTPChunkState_ReadingHeader )
		{
			err = HTTPReadLine( hdr, inRead_f, inRead_ctx, &linePtr, &lineLen );
			require_noerr_quiet( err, exit );
			
			n = SNScanF( linePtr, lineLen, "%llx", &chunkLen );
			require_action_quiet( n == 1, exit, err = kMalformedErr );
			require_action( chunkLen <= SIZE_MAX, exit, err = kSizeErr );
			if( chunkLen > 0 )
			{
				len = inMsg->bodyLen + ( (size_t) chunkLen );
				require_action( len > inMsg->bodyLen, exit, err = kSizeErr ); // Detect wrap.
				if( len <= sizeof( inMsg->smallBodyBuf ) )
				{
					inMsg->bodyPtr = inMsg->smallBodyBuf;
				}
				else
				{
					tmp = (uint8_t *) realloc( inMsg->bigBodyBuf, len );
					require_action( tmp, exit, err = kNoMemoryErr );
					if( !inMsg->bigBodyBuf && ( inMsg->bodyOffset > 0 ) )
					{
						memmove( tmp, inMsg->bodyPtr, inMsg->bodyOffset );
					}
					inMsg->bigBodyBuf = tmp;
					inMsg->bodyPtr    = tmp;
				}
				inMsg->bodyLen = len;
				inMsg->chunkState = kHTTPChunkState_ReadingBody;
			}
			else
			{
				inMsg->chunkState = kHTTPChunkState_ReadingTrailer;
			}
		}
		
		// ReadingBody
		
		else if( inMsg->chunkState == kHTTPChunkState_ReadingBody )
		{
			if( hdr->extraDataLen > 0 )
			{
				len = inMsg->bodyLen - inMsg->bodyOffset;
				if( len > hdr->extraDataLen ) len = hdr->extraDataLen;
				memmove( &inMsg->bodyPtr[ inMsg->bodyOffset ], hdr->extraDataPtr, len );
				hdr->extraDataPtr += len;
				hdr->extraDataLen -= len;
				inMsg->bodyOffset += len;
			}
			
			len = inMsg->bodyOffset;
			if( len < inMsg->bodyLen )
			{
				err = inRead_f( inMsg->bodyPtr + len, inMsg->bodyLen - len, &len, inRead_ctx );
				require_noerr_quiet( err, exit );
				inMsg->bodyOffset += len;
			}
			if( inMsg->bodyOffset == inMsg->bodyLen )
			{
				inMsg->chunkState = kHTTPChunkState_ReadingBodyEnd;
			}
		}
		
		// ReadingBodyEnd
		
		else if( inMsg->chunkState == kHTTPChunkState_ReadingBodyEnd )
		{
			err = HTTPReadLine( hdr, inRead_f, inRead_ctx, &linePtr, &lineLen );
			require_noerr_quiet( err, exit );
			require_action_quiet( lineLen == 0, exit, err = kMalformedErr );
			
			inMsg->chunkState = kHTTPChunkState_ReadingHeader;
		}
		
		// ReadingTrailer
		
		else if( inMsg->chunkState == kHTTPChunkState_ReadingTrailer )
		{
			err = HTTPReadLine( hdr, inRead_f, inRead_ctx, &linePtr, &lineLen );
			require_noerr_quiet( err, exit );
			if( lineLen == 0 )
			{
				inMsg->chunkState = kHTTPChunkState_Done;
				break;
			}
			
			// $$$ TO DO: Consider appending trailer headers to normal headers.
		}
		
		// Done
		
		else if( inMsg->chunkState == kHTTPChunkState_Done )
		{
			break;	
		}
		else
		{
			dlogassert( "Bad HTTP chunk state: %d", inMsg->chunkState );
			err = kStateErr;
			goto exit;
		}
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPMessageWriteMessage
//===========================================================================================================================

OSStatus	HTTPMessageWriteMessage( HTTPMessageRef inMsg, NetTransportWriteV_f inWriteV_f, void *inWriteV_ctx )
{
	OSStatus		err;
	
	err = inWriteV_f( &inMsg->iop, &inMsg->ion, inWriteV_ctx );
	require_noerr_quiet( err, exit );
	
#if( TARGET_OS_POSIX )
	if( IsValidFD( inMsg->fileFD ) && ( inMsg->fileRemain > 0 ) )
	{
		size_t		len;
		ssize_t		n;
		
		check( inMsg->bodyPtr );
		len = ( inMsg->fileRemain <= ( (int64_t) inMsg->maxBodyLen ) ) ? ( (size_t) inMsg->fileRemain ) : inMsg->maxBodyLen;
		n = read( inMsg->fileFD, inMsg->bodyPtr, len );
		err = map_global_value_errno( n > 0, n );
		require_noerr_quiet( err, exit );
		inMsg->fileRemain -= n;
		check( inMsg->fileRemain >= 0 );
		
		inMsg->iov[ 0 ].iov_base = inMsg->bodyPtr;
		inMsg->iov[ 0 ].iov_len  = (size_t) n;
		inMsg->ion = 1;
		inMsg->iop = inMsg->iov;
		err = inWriteV_f( &inMsg->iop, &inMsg->ion, inWriteV_ctx );
		require_noerr_quiet( err, exit );
	}
#endif
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPMessageSetBody
//===========================================================================================================================

OSStatus	HTTPMessageSetBody( HTTPMessageRef inMsg, const char *inContentType, const void *inData, size_t inLen )
{
	OSStatus		err;
	
	err = inMsg->header.firstErr;
	require_noerr( err, exit );
	
	err = HTTPMessageSetBodyLength( inMsg, inLen );
	require_noerr( err, exit );
	if( inData && ( inData != inMsg->bodyPtr ) )  // Handle inData pointing to the buffer.
	{
		memmove( inMsg->bodyPtr, inData, inLen ); // memmove in case inData is in the middle of the buffer.
	}
	
	HTTPHeader_SetField( &inMsg->header, kHTTPHeader_ContentLength, "%zu", inLen );
	if( inContentType ) HTTPHeader_SetField( &inMsg->header, kHTTPHeader_ContentType, inContentType );
	
exit:
	if( err && !inMsg->header.firstErr ) inMsg->header.firstErr = err;
	return( err );
}

#if( TARGET_OS_POSIX )
//===========================================================================================================================
//	HTTPMessageSetBodyFileDescriptor
//===========================================================================================================================

OSStatus
	HTTPMessageSetBodyFileDescriptor( 
		HTTPMessageRef	inMsg, 
		FDRef			inFD, 
		int64_t			inByteOffset, 
		int64_t			inByteCount, 
		Boolean			inCloseWhenDone )
{
	OSStatus		err;
	size_t			len;
	uint8_t *		tmp;
	int64_t			startOffset;
	ssize_t			n;
	
	// Set up the start of the read and length.
	
	startOffset = lseek64( inFD, inByteOffset, ( inByteOffset < 0 ) ? SEEK_END : SEEK_SET );
	err = map_global_value_errno( startOffset != -1, startOffset );
	require_noerr_quiet( err, exit );
	
	if( inByteCount < 0 )
	{
		struct stat		sb;
		
		err = fstat( inFD, &sb );
		err = map_global_noerr_errno( err );
		require_noerr_quiet( err, exit );
		inByteCount = sb.st_size - startOffset;
	}
	HTTPHeader_SetField( &inMsg->header, kHTTPHeader_ContentLength, "%lld", inByteCount );
	
	// Set up read buffer.
	
	len = ( inByteCount <= kFileWriteBufferLen ) ? ( (size_t) inByteCount ) : kFileWriteBufferLen;
	if( len <= sizeof( inMsg->smallBodyBuf ) )
	{
		inMsg->bodyPtr = inMsg->smallBodyBuf;
	}
	else
	{
		tmp = (uint8_t *) realloc( inMsg->bigBodyBuf, len );
		require_action( tmp, exit, err = kNoMemoryErr );
		inMsg->bigBodyBuf = tmp;
		inMsg->bodyPtr    = tmp;
	}
	inMsg->maxBodyLen = len;
	
	// Read a buffer-full up-front to utilize any space after the HTTP header in the first TCP packet.
	
	if( len > 0 )
	{
		n = read( inFD, inMsg->bodyPtr, len );
		err = map_global_value_errno( n > 0, n );
		require_noerr_quiet( err, exit );
		inMsg->bodyLen = (size_t) n;
		inByteCount -= n;
	}
	else
	{
		inMsg->bodyLen = 0;
	}
	
	if( inMsg->closeFD ) ForgetFD( &inMsg->fileFD );
	inMsg->fileFD		= inFD;
	inMsg->closeFD		= inCloseWhenDone;
	inMsg->fileRemain	= inByteCount;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPMessageSetBodyFilePath
//===========================================================================================================================

OSStatus
	HTTPMessageSetBodyFilePath( 
		HTTPMessageRef	inMsg, 
		const char *	inPath, 
		int64_t			inByteOffset, 
		int64_t			inByteCount )
{
	OSStatus		err;
	FDRef			fd;
	
	fd = open( inPath, O_RDONLY );
	err = map_fd_creation_errno( fd );
	require_noerr_quiet( err, exit );
	
	err = HTTPMessageSetBodyFileDescriptor( inMsg, fd, inByteOffset, inByteCount, true );
	require_noerr( err, exit );
	fd = kInvalidFD;
	
exit:
	ForgetFD( &fd );
	return( err );
}
#endif // TARGET_OS_POSIX

//===========================================================================================================================
//	HTTPMessageSetBodyLength
//===========================================================================================================================

OSStatus	HTTPMessageSetBodyLength( HTTPMessageRef inMsg, size_t inLen )
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

#if( COMPILER_HAS_BLOCKS )
//===========================================================================================================================
//	HTTPMessageSetCompletionBlock
//===========================================================================================================================

static void	_HTTPMessageCompletionHandler( HTTPMessageRef inMsg );

void	HTTPMessageSetCompletionBlock( HTTPMessageRef inMsg, HTTPMessageCompletionBlock inBlock )
{
	ReplaceBlock( &inMsg->completionBlock, inBlock );
	inMsg->completion = inBlock ? _HTTPMessageCompletionHandler : NULL;
}

static void	_HTTPMessageCompletionHandler( HTTPMessageRef inMsg )
{
	HTTPMessageCompletionBlock const		block = inMsg->completionBlock;
	
	inMsg->completionBlock = NULL;
	block( inMsg );
	Block_release( block );
}
#endif

//===========================================================================================================================
//	HTTPMessageGetOrCopyFormVariable
//===========================================================================================================================

OSStatus
	HTTPMessageGetOrCopyFormVariable( 
		HTTPMessageRef	inMsg, 
		const char *	inName, 
		const char **	outValuePtr, 
		size_t *		outValueLen, 
		char **			outValueStorage )
{
	OSStatus			err;
	const char *		ptr;
	const char *		end;
	
	ptr = inMsg->header.url.queryPtr;
	end = ptr + inMsg->header.url.queryLen;
	err = URLGetOrCopyVariable( ptr, end, inName, outValuePtr, outValueLen, outValueStorage, NULL );
	if( err )
	{
		ptr = (const char *) inMsg->bodyPtr;
		end = ptr + inMsg->bodyLen;
		err = URLGetOrCopyVariable( ptr, end, inName, outValuePtr, outValueLen, outValueStorage, NULL );
	}
	return( err );
}

//===========================================================================================================================
//	HTTPMessageScanFFormVariable
//===========================================================================================================================

OSStatus
	HTTPMessageScanFFormVariable( 
		HTTPMessageRef	inMsg, 
		const char *	inName, 
		int *			outMatchCount, 
		const char *	inFormat, 
		... )
{
	OSStatus			err;
	const char *		valuePtr;
	size_t				valueLen;
	char *				valueBuf;
	va_list				args;
	
	err = HTTPMessageGetOrCopyFormVariable( inMsg, inName, &valuePtr, &valueLen, &valueBuf );
	require_noerr_quiet( err, exit );
	
	va_start( args, inFormat );
	*outMatchCount = VSNScanF( valuePtr, valueLen, inFormat, args );
	va_end( args );
	FreeNullSafe( valueBuf );
	
exit:
	return( err );
}
