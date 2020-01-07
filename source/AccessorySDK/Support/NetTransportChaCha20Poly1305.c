/*
	File:    	NetTransportChaCha20Poly1305.c
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
	
	Copyright (C) 2014 Apple Inc. All Rights Reserved.
*/

#include "NetTransportChaCha20Poly1305.h"

#include "ChaCha20Poly1305.h"
#include "HTTPClient.h"
#include "NetUtils.h"

//===========================================================================================================================
//	Internals
//===========================================================================================================================

#define kMaxMessageReadSize			( 16 * 1024 )	// Max size message we can read.
#define kMaxMessageWriteSize		1024			// Max size message we will write.

typedef enum
{
	kNTState_ReadingHeader	= 1, 
	kNTState_ReadingBody	= 2, 
	kNTState_ReadingAuthTag	= 3
	
}	NTState;

typedef struct
{
	SocketRef					sock;
	LogCategory *				ucat;
	
	NTState						readState;
	size_t						readSize;
	size_t						readOffset;
	uint8_t						readHeader[ 2 ];
	uint8_t						readBody[ kMaxMessageReadSize ];
	uint8_t						readAuthTag[ 16 ];
	uint8_t *					readBufferedPtr;
	uint8_t *					readBufferedEnd;
	chacha20_poly1305_state		readCtx;
	uint8_t						readKey[ 32 ];
	uint8_t						readNonce[ 8 ];
	
	uint8_t						writeBuffer[ 2 + kMaxMessageWriteSize + 16 ];
	uint8_t *					writeBufferedPtr;
	uint8_t *					writeBufferedEnd;
	chacha20_poly1305_state		writeCtx;
	uint8_t						writeKey[ 32 ];
	uint8_t						writeNonce[ 8 ];
	
}	NTContext;

static OSStatus	_NetTransportInitialize( SocketRef inSock, void *inContext );
static void		_NetTransportFinalize( void *inContext );
static OSStatus	_NetTransportRead( void *inBuffer, size_t inMaxLen, size_t *outLen, void *inContext );
static OSStatus	_NetTransportWriteV( iovec_t **ioArray, int *ioCount, void *inContext );

ulog_define( NetTransportChaCha20Poly1305, kLogLevelNotice, kLogFlags_Default, "NetTransportChaCha20Poly1305", NULL );
#define nt_ucat()					&log_category_from_name( NetTransportChaCha20Poly1305 )
#define nt_ulog( LEVEL, ... )		ulog( nt_ucat(), (LEVEL), __VA_ARGS__ )
#define nt_dlog( LEVEL, ... )		dlogc( nt_ucat(), (LEVEL), __VA_ARGS__ )

// Logging

#define LOG_PRE_READ		0 // Log read data before it's been decrypted.
#if( LOG_PRE_READ )
	#define nt_pre_read_dlog( LEVEL, ... )		nt_dlog( (LEVEL), __VA_ARGS__ )
#else
	#define nt_pre_read_dlog( LEVEL, ... )		do {} while( 0 )
#endif

#define LOG_POST_READ		0 // Log read data after it's been decrypted.
#if( LOG_POST_READ )
	#define nt_post_read_dlog( LEVEL, ... )		nt_dlog( (LEVEL), __VA_ARGS__ )
#else
	#define nt_post_read_dlog( LEVEL, ... )		do {} while( 0 )
#endif

#define LOG_PRE_WRITE		0 // Log data to write before it's been encrypted.
#if( LOG_PRE_WRITE )
	#define nt_pre_write_dlog( LEVEL, ... )		nt_dlog( (LEVEL), __VA_ARGS__ )
#else
	#define nt_pre_write_dlog( LEVEL, ... )		do {} while( 0 )
#endif

#define LOG_POST_WRITE		0 // Log data written after it's been encrypted.
#if( LOG_POST_WRITE )
	#define nt_post_write_dlog( LEVEL, ... )	nt_dlog( (LEVEL), __VA_ARGS__ )
#else
	#define nt_post_write_dlog( LEVEL, ... )	do {} while( 0 )
#endif

//===========================================================================================================================
//	NetTransportChaCha20Poly1305Configure
//===========================================================================================================================

OSStatus
	NetTransportChaCha20Poly1305Configure( 
		NetTransportDelegate *	ioDelegate, 
		LogCategory *			inCategory, 
		const uint8_t			inReadKey[ 32 ], 
		const uint8_t			inReadNonce[ 8 ], 
		const uint8_t			inWriteKey[ 32 ], 
		const uint8_t			inWriteNonce[ 8 ] )
{
	OSStatus		err;
	NTContext *		ctx;
	
	ctx = (NTContext *) calloc( 1, sizeof( *ctx ) );
	require_action( ctx, exit, err = kNoMemoryErr );
	
	ctx->sock		= kInvalidSocketRef;
	ctx->ucat		= inCategory ? inCategory : nt_ucat();
	ctx->readState	= kNTState_ReadingHeader;
	memcpy( ctx->readKey, inReadKey, 32 );
	if( inReadNonce ) memcpy( ctx->readNonce, inReadNonce, 8 );
	memcpy( ctx->writeKey, inWriteKey, 32 );
	if( inWriteNonce ) memcpy( ctx->writeNonce, inWriteNonce, 8 );
	
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
	NTContext * const		ctx = (NTContext *) inContext;
	
	ctx->sock = inSock;
	return( kNoErr );
}

//===========================================================================================================================
//	_NetTransportFinalize
//===========================================================================================================================

static void	_NetTransportFinalize( void *inContext )
{
	NTContext * const		ctx = (NTContext *) inContext;
	
	if( ctx )
	{
		MemZeroSecure( ctx, sizeof( *ctx ) );
		free( ctx );
	}
}

//===========================================================================================================================
//	_NetTransportRead
//===========================================================================================================================

static OSStatus	_NetTransportRead( void *inBuffer, size_t inMaxLen, size_t *outLen, void *inContext )
{
	NTContext * const		ctx = (NTContext *) inContext;
	uint8_t *				dst = (uint8_t *) inBuffer;
	OSStatus				err;
	size_t					len;
	
	for( ;; )
	{
		// If there's any decrypted data buffered from a previous read, use that first.
		
		len = (size_t)( ctx->readBufferedEnd - ctx->readBufferedPtr );
		if( len > 0 )
		{
			if( len > inMaxLen ) len = inMaxLen;
			memcpy( dst, ctx->readBufferedPtr, len );
			ctx->readBufferedPtr += len;
			dst += len;
			inMaxLen -= len;
		}
		if( inMaxLen == 0 ) break;
		
		// Read, verify, and decrypt message.
		
		if( ctx->readState == kNTState_ReadingHeader )
		{
			err = SocketReadData( ctx->sock, ctx->readHeader, sizeof( ctx->readHeader ), &ctx->readOffset );
			require_noerr_quiet( err, exit );
			ctx->readSize = ReadLittle16( ctx->readHeader );
			require_action( ctx->readSize <= kMaxMessageReadSize, exit, err = kSizeErr; 
				nt_ulog( kLogLevelWarning, "### NTCP bad size: %zu / %H\n", ctx->readSize, ctx->readHeader, 2, 2 ) );
			
			ctx->readOffset	= 0;
			ctx->readState	= kNTState_ReadingBody;
		}
		if( ctx->readState == kNTState_ReadingBody )
		{
			err = SocketReadData( ctx->sock, ctx->readBody, ctx->readSize, &ctx->readOffset );
			require_noerr_quiet( err, exit );
			
			ctx->readOffset	= 0;
			ctx->readState	= kNTState_ReadingAuthTag;
		}
		if( ctx->readState == kNTState_ReadingAuthTag )
		{
			err = SocketReadData( ctx->sock, ctx->readAuthTag, sizeof( ctx->readAuthTag ), &ctx->readOffset );
			require_noerr_quiet( err, exit );
			nt_pre_read_dlog( kLogLevelMax, "-- Pre-decrypt read header: %.3H, auth tag: %.3H\n%1.1H\n", 
				ctx->readHeader, 2, 2, ctx->readAuthTag, 16, 16, ctx->readBody, (int) ctx->readSize, 256 );
			
			chacha20_poly1305_init_64x64( &ctx->readCtx, ctx->readKey, ctx->readNonce );
			chacha20_poly1305_add_aad( &ctx->readCtx, ctx->readHeader, sizeof( ctx->readHeader ) );
			len = chacha20_poly1305_decrypt( &ctx->readCtx, ctx->readBody, ctx->readSize, ctx->readBody );
			len += chacha20_poly1305_verify( &ctx->readCtx, &ctx->readBody[ len ], ctx->readAuthTag, &err );
			require_noerr_action( err, exit, 
				nt_ulog( kLogLevelWarning, "### NTCP verify failed: %#m\n", err ) );
			require_action( len == ctx->readSize, exit, err = kInternalErr; 
				nt_ulog( kLogLevelWarning, "### NTCP verify len failed: %zu vs %zu\n", len, ctx->readSize ) );
			LittleEndianIntegerIncrement( ctx->readNonce, sizeof( ctx->readNonce ) );
			nt_post_read_dlog( kLogLevelMax, "-- Post-decrypt read\n%1.1H\n", ctx->readBody, (int) ctx->readSize, 256 );
			
			ctx->readBufferedPtr	= ctx->readBody;
			ctx->readBufferedEnd	= ctx->readBody + ctx->readSize;
			ctx->readOffset			= 0;
			ctx->readState			= kNTState_ReadingHeader;
		}
	}
	err = kNoErr;
	
exit:
	len = (size_t)( dst - ( (uint8_t *) inBuffer ) );
	if( ( ( err == EWOULDBLOCK ) || ( err == kConnectionErr ) ) && ( len > 0 ) ) err = kNoErr;
	if( outLen ) *outLen = len;
	return( err );
}

//===========================================================================================================================
//	_NetTransportWriteV
//===========================================================================================================================

static OSStatus	_NetTransportWriteV( iovec_t **ioArray, int *ioCount, void *inContext )
{
	NTContext * const			ctx = (NTContext *) inContext;
	OSStatus					err;
	size_t						len, totalLen, n;
	const uint8_t *				ptr;
	uint8_t *					dst;
	const uint8_t * const		lim = ctx->writeBuffer + 2 + kMaxMessageWriteSize;
	iovec_t						iov[ 1 ];
	int							ion;
	iovec_t *					iop = *ioArray;
	iovec_t * const				ioe = iop + *ioCount;
	iovec_t *					iot;
	uint8_t						authTag[ 16 ];
	
	for( ;; )
	{
		// If there's any encrypted data buffered, use that first.
		
		len = (size_t)( ctx->writeBufferedEnd - ctx->writeBufferedPtr );
		if( len > 0 )
		{
			SETIOV( iov, ctx->writeBufferedPtr, len );
			iot = iov;
			ion = 1;
			err = SocketWriteData( ctx->sock, &iot, &ion );
			ctx->writeBufferedPtr += ( len - iov[ 0 ].iov_len );
			require_noerr_quiet( err, exit );
			continue;
		}
		
		// Count the number of bytes we can fit in our buffer.
		
		ptr = ctx->writeBuffer + 2;
		totalLen = 0;
		for( iot = iop; iot < ioe; ++iot )
		{
			len = (size_t)( lim - ptr );
			if( len < iot->iov_len )
			{
				totalLen += len;
				break;
			}
			totalLen += iot->iov_len;
			ptr += iot->iov_len;
		}
		if( totalLen == 0 )
		{
			err = kNoErr;
			break;
		}
		
		// Encrypt and authenticate the message.
		
		chacha20_poly1305_init_64x64( &ctx->writeCtx, ctx->writeKey, ctx->writeNonce );
		
		dst = ctx->writeBuffer;
		WriteLittle16( dst, (uint16_t) totalLen );
		chacha20_poly1305_add_aad( &ctx->writeCtx, dst, 2 );
		dst += 2;
		nt_post_write_dlog( kLogLevelMax, "-- Write header: %.3H\n", ctx->writeBuffer, 2, 2 );
		
		ptr = dst;
		for( ; iop < ioe; ++iop )
		{
			len = (size_t)( lim - ptr );
			if( len < iop->iov_len )
			{
				nt_pre_write_dlog( kLogLevelMax, "-- Pre-encrypt write body end:\n%1.1H\n", iop->iov_base, (int) len, 256 );
				n = chacha20_poly1305_encrypt( &ctx->writeCtx, iop->iov_base, len, dst );
				nt_post_write_dlog( kLogLevelMax, "-- Post-encrypt write body end:\n%1.1H\n", dst, (int) n, 256 );
				
				iop->iov_base	 = ( (uint8_t *) iop->iov_base ) + len;
				iop->iov_len	-= len;
				dst				+= n;
				break;
			}
			
			nt_pre_write_dlog( kLogLevelMax, "-- Pre-encrypt write body:\n%1.1H\n", iop->iov_base, (int) iop->iov_len, 256 );
			n = chacha20_poly1305_encrypt( &ctx->writeCtx, iop->iov_base, iop->iov_len, dst );
			nt_post_write_dlog( kLogLevelMax, "-- Post-encrypt write body:\n%1.1H\n", dst, (int) n, 256 );
			ptr	+= iop->iov_len;
			dst	+= n;
		}
		n = chacha20_poly1305_final( &ctx->writeCtx, dst, authTag );
		if( n > 0 ) nt_post_write_dlog( kLogLevelMax, "-- Post-encrypt write body final:\n%1.1H\n", dst, (int) n, 256 );
		dst += n;
		require_action( dst == ( ctx->writeBuffer + 2 + totalLen ), exit, err = kInternalErr );
		require_action( dst <= lim, exit, err = kInternalErr );
		memcpy( dst, authTag, sizeof( authTag ) );
		nt_post_write_dlog( kLogLevelMax, "-- Write auth tag: %.3H\n", dst, 16, 16 );
		dst += sizeof( authTag );
		LittleEndianIntegerIncrement( ctx->writeNonce, sizeof( ctx->writeNonce ) );
		
		ctx->writeBufferedPtr = ctx->writeBuffer;
		ctx->writeBufferedEnd = dst;
	}
	
exit:
	*ioArray = iop;
	*ioCount = (int)( ioe - iop );
	return( err );
}
