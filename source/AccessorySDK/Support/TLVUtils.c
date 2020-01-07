/*
	File:    	TLVUtils.c
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
	
	Copyright (C) 2008-2015 Apple Inc. All Rights Reserved.
*/

#include "TLVUtils.h"

#include "CommonServices.h"
#include "DataBufferUtils.h"
#include "DebugServices.h"

//===========================================================================================================================
//	TLV8Get
//===========================================================================================================================

OSStatus
	TLV8Get( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t				inType, 
		const uint8_t **	outPtr, 
		size_t *			outLen, 
		const uint8_t **	outNext )
{
	OSStatus			err;
	uint8_t				type;
	const uint8_t *		ptr;
	size_t				len;
	
	while( ( err = TLV8GetNext( inSrc, inEnd, &type, &ptr, &len, &inSrc ) ) == kNoErr )
	{
		if( type == inType )
		{
			if( outPtr )  *outPtr  = ptr;
			if( outLen )  *outLen  = len;
			if( outNext ) *outNext = inSrc;
			break;
		}
	}
	return( err );
}

//===========================================================================================================================
//	TLV8GetBytes
//===========================================================================================================================

OSStatus
	TLV8GetBytes( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t				inType, 
		size_t				inMinLen, 
		size_t				inMaxLen, 
		void *				inBuffer, 
		size_t *			outLen, 
		const uint8_t **	outNext )
{
	OSStatus					err;
	uint8_t *					buf = (uint8_t *) inBuffer;
	uint8_t *					dst = buf;
	const uint8_t * const		lim = buf + inMaxLen;
	const uint8_t *				src2;
	uint8_t						type;
	const uint8_t *				ptr;
	size_t						len;
	
	err = TLV8Get( inSrc, inEnd, inType, &ptr, &len, &inSrc );
	require_noerr_quiet( err, exit );
	require_action_quiet( len <= ( (size_t)( lim - dst ) ), exit, err = kOverrunErr );
	memcpy( dst, ptr, len );
	dst += len;
	
	for( ;; )
	{
		err = TLV8GetNext( inSrc, inEnd, &type, &ptr, &len, &src2 );
		if( err ) break;
		if( type != inType ) break;
		inSrc = src2;
		if( len == 0 ) continue;
		require_action_quiet( len <= ( (size_t)( lim - dst ) ), exit, err = kOverrunErr );
		memcpy( dst, ptr, len );
		dst += len;
	}
	
	len = (size_t)( dst - buf );
	require_action_quiet( len >= inMinLen, exit, err = kUnderrunErr );
	if( outLen )	*outLen  = len;
	if( outNext )	*outNext = inSrc;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	TLV8GetSInt64
//===========================================================================================================================

int64_t
	TLV8GetSInt64( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t				inType, 
		OSStatus *			outErr, 
		const uint8_t **	outNext )
{
	int64_t				x = 0;
	OSStatus			err;
	const uint8_t *		ptr;
	size_t				len;
	
	err = TLV8Get( inSrc, inEnd, inType, &ptr, &len, outNext );
	require_noerr_quiet( err, exit );
	if(      len == 1 ) x = (int8_t) *ptr;
	else if( len == 2 ) x = (int16_t) ReadLittle16( ptr );
	else if( len == 4 ) x = (int32_t) ReadLittle32( ptr );
	else if( len == 8 ) x = (int64_t) ReadLittle64( ptr );
	else { err = kSizeErr; goto exit; }
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( x );
}

//===========================================================================================================================
//	TLV8GetUInt64
//===========================================================================================================================

uint64_t
	TLV8GetUInt64( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t				inType, 
		OSStatus *			outErr, 
		const uint8_t **	outNext )
{
	uint64_t			x = 0;
	OSStatus			err;
	const uint8_t *		ptr;
	size_t				len;
	
	err = TLV8Get( inSrc, inEnd, inType, &ptr, &len, outNext );
	require_noerr_quiet( err, exit );
	if(      len == 1 ) x = *ptr;
	else if( len == 2 ) x = ReadLittle16( ptr );
	else if( len == 4 ) x = ReadLittle32( ptr );
	else if( len == 8 ) x = ReadLittle64( ptr );
	else { err = kSizeErr; goto exit; }
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( x );
}

//===========================================================================================================================
//	TLV8GetNext
//===========================================================================================================================

OSStatus
	TLV8GetNext( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t *			outType, 
		const uint8_t **	outPtr, 
		size_t *			outLen, 
		const uint8_t **	outNext )
{
	OSStatus			err;
	const uint8_t *		ptr;
	size_t				len;
	const uint8_t *		next;
	
	// TLV8's have the following format: <1:type> <1:length> <length:data>
	
	require_action_quiet( inSrc != inEnd, exit, err = kNotFoundErr );
	require_action_quiet( inSrc < inEnd, exit, err = kParamErr );
	len = (size_t)( inEnd - inSrc );
	require_action_quiet( len >= 2, exit, err = kNotFoundErr );
	
	ptr  = inSrc + 2;
	len  = inSrc[ 1 ];
	next = ptr + len;
	require_action_quiet( ( next >= ptr ) && ( next <= inEnd ), exit, err = kUnderrunErr );
	
	*outType = inSrc[ 0 ];
	*outPtr  = ptr;
	*outLen  = len;
	if( outNext ) *outNext = next;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	TLV8CopyCoalesced
//===========================================================================================================================

uint8_t *
	TLV8CopyCoalesced( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t				inType, 
		size_t *			outLen, 
		const uint8_t **	outNext, 
		OSStatus *			outErr )
{
	uint8_t *			result = NULL;
	OSStatus			err;
	const uint8_t *		ptr;
	size_t				len;
	uint8_t *			storage;
	const uint8_t *		next;
	
	err = TLV8GetOrCopyCoalesced( inSrc, inEnd, inType, &ptr, &len, &storage, &next );
	require_noerr_quiet( err, exit );
	if( storage )
	{
		result = storage;
	}
	else
	{
		result = (uint8_t *) malloc( len ? len : 1 ); // Use 1 if length is 0 since malloc( 0 ) is undefined.
		require_action( result, exit, err = kNoMemoryErr );
		memcpy( result, ptr, len );
	}
	*outLen = len;
	if( outNext ) *outNext = inSrc;
	
exit:
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	TLV8GetOrCopyCoalesced
//===========================================================================================================================

OSStatus
	TLV8GetOrCopyCoalesced( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		uint8_t				inType, 
		const uint8_t **	outPtr, 
		size_t *			outLen, 
		uint8_t **			outStorage, 
		const uint8_t **	outNext )
{
	OSStatus			err;
	uint8_t *			storage = NULL;
	const uint8_t *		resultPtr;
	size_t				resultLen;
	const uint8_t *		ptr;
	size_t				len;
	uint8_t				type;
	const uint8_t *		src2;
	uint8_t *			tempPtr;
	size_t				tempLen;
	
	err = TLV8Get( inSrc, inEnd, inType, &ptr, &len, &inSrc );
	require_noerr_quiet( err, exit );
	resultPtr = ptr;
	resultLen = len;
	
	for( ;; )
	{
		err = TLV8GetNext( inSrc, inEnd, &type, &ptr, &len, &src2 );
		if( err ) break;
		if( type != inType ) break;
		inSrc = src2;
		if( len == 0 ) continue;
		
		if( resultLen > 0 )
		{
			tempLen = resultLen + len;
			tempPtr = (uint8_t *) malloc( tempLen );
			require_action( tempPtr, exit, err = kNoMemoryErr );
			memcpy( tempPtr, resultPtr, resultLen );
			memcpy( tempPtr + resultLen, ptr, len );
			
			if( storage ) free( storage );
			storage   = tempPtr;
			resultPtr = tempPtr;
			resultLen = tempLen;
		}
		else
		{
			resultPtr = ptr;
			resultLen = len;
		}
	}
	
	*outPtr		= resultPtr;
	*outLen		= resultLen;
	*outStorage	= storage;
	if( outNext ) *outNext = inSrc;
	storage		= NULL;
	err			= kNoErr;
	
exit:
	FreeNullSafe( storage );
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	TLV8BufferInit
//===========================================================================================================================

void	TLV8BufferInit( TLV8Buffer *inBuffer, size_t inMaxLen )
{
	inBuffer->ptr			= inBuffer->inlineBuffer;
	inBuffer->len			= 0;
	inBuffer->maxLen		= inMaxLen;
	inBuffer->mallocedPtr	= NULL;
}

//===========================================================================================================================
//	TLV8BufferFree
//===========================================================================================================================

void	TLV8BufferFree( TLV8Buffer *inBuffer )
{
	ForgetMem( &inBuffer->mallocedPtr );
}

//===========================================================================================================================
//	TLV8BufferAppend
//===========================================================================================================================

OSStatus	TLV8BufferAppend( TLV8Buffer *inBuffer, uint8_t inType, const void *inPtr, size_t inLen )
{
	const uint8_t *		src;
	const uint8_t *		end;
	OSStatus			err;
	size_t				len, newLen;
	uint8_t *			dst;
	uint8_t *			lim;
	
	DEBUG_USE_ONLY( lim );
	
	// Calculate the total size of the buffer after the new data is added.
	// This includes the space needed for extra headers if the data needs to be split across multiple items.
	
	if( inLen == kSizeCString ) inLen = strlen( (const char *) inPtr );
	len = ( inLen <= 255 ) ? ( 2 + inLen ) : ( ( 2 * ( ( inLen / 255 ) + ( ( inLen % 255 ) ? 1 : 0 ) ) ) + inLen );
	newLen = inBuffer->len + len;
	require_action( newLen <= inBuffer->maxLen, exit, err = kSizeErr );
	require_action( newLen >= inBuffer->len, exit, err = kOverrunErr );
	
	if( newLen <= sizeof( inBuffer->inlineBuffer ) )
	{
		dst = &inBuffer->inlineBuffer[ inBuffer->len ];
		lim = inBuffer->inlineBuffer + newLen;
	}
	else
	{
		// Data is too big to fit in the inline buffer so allocate a new buffer to hold everything.
		
		dst = (uint8_t *) malloc( newLen );
		require_action( dst, exit, err = kNoMemoryErr );
		if( inBuffer->mallocedPtr )
		{
			memcpy( dst, inBuffer->mallocedPtr, inBuffer->len );
			free( inBuffer->mallocedPtr );
		}
		else if( inBuffer->len > 0 )
		{
			memcpy( dst, inBuffer->inlineBuffer, inBuffer->len );
		}
		inBuffer->ptr = dst;
		inBuffer->mallocedPtr = dst;
		lim = dst + newLen;
		dst += inBuffer->len;
	}
	
	// Append the new data. This will split it into 255 byte items as needed.
	
	src = (const uint8_t *) inPtr;
	end = src + inLen;
	do
	{
		len = (size_t)( end - src );
		if( len > 255 ) len = 255;
		dst[ 0 ] = inType;
		dst[ 1 ] = (uint8_t ) len;
		if( len > 0 ) memcpy( &dst[ 2 ], src, len );
		src += len;
		dst += ( 2 + len );
		
	}	while( src != end );
	inBuffer->len = (size_t)( dst - inBuffer->ptr );
	check( dst == lim );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	TLV8BufferAppendSInt64
//===========================================================================================================================

OSStatus	TLV8BufferAppendSInt64( TLV8Buffer *inBuffer, uint8_t inType, int64_t x )
{
	uint8_t			buf[ 8 ];
	size_t			len;
	
	if(      ( x >= INT8_MIN )  && ( x <= INT8_MAX ) )	{ *buf = (uint8_t) x; len = 1; }
	else if( ( x >= INT16_MIN ) && ( x <= INT16_MAX ) )	{ WriteLittle16( buf, x ); len = 2; }
	else if( ( x >= INT32_MIN ) && ( x <= INT32_MAX ) )	{ WriteLittle32( buf, x ); len = 4; }
	else												{ WriteLittle64( buf, x ); len = 8; }
	return( TLV8BufferAppend( inBuffer, inType, buf, len ) );
}

//===========================================================================================================================
//	TLV8BufferAppendUInt64
//===========================================================================================================================

OSStatus	TLV8BufferAppendUInt64( TLV8Buffer *inBuffer, uint8_t inType, uint64_t x )
{
	uint8_t			buf[ 8 ];
	size_t			len;
	
	if(      x <= UINT8_MAX )	{ *buf = (uint8_t) x; len = 1; }
	else if( x <= UINT16_MAX )	{ WriteLittle16( buf, x ); len = 2; }
	else if( x <= UINT32_MAX )	{ WriteLittle32( buf, x ); len = 4; }
	else						{ WriteLittle64( buf, x ); len = 8; }
	return( TLV8BufferAppend( inBuffer, inType, buf, len ) );
}

//===========================================================================================================================
//	TLV8BufferDetach
//===========================================================================================================================

OSStatus	TLV8BufferDetach( TLV8Buffer *inBuffer, uint8_t **outPtr, size_t *outLen )
{
	OSStatus		err;
	uint8_t *		ptr;
	size_t			len;
	
	len = inBuffer->len;
	if( inBuffer->mallocedPtr )
	{
		ptr = inBuffer->mallocedPtr;
	}
	else
	{
		ptr = (uint8_t *) malloc( len ? len : 1 ); // malloc( 1 ) if buffer is empty since malloc( 0 ) is undefined.
		require_action( ptr, exit, err = kNoMemoryErr );
		if( len ) memcpy( ptr, inBuffer->ptr, len );
	}
	inBuffer->ptr			= inBuffer->inlineBuffer;
	inBuffer->len			= 0;
	inBuffer->mallocedPtr	= NULL;
	
	*outPtr = ptr;
	*outLen = len;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	TLV8MaxPayloadBytesForTotalBytes
//===========================================================================================================================

size_t	TLV8MaxPayloadBytesForTotalBytes( size_t inN )
{
	size_t		n, r;
	
	if( inN < 2 ) return( 0 );
	n = inN - ( 2 * ( inN / 257 ) );
	r = inN % 257;
	if(      r  > 1 ) n -= 2;
	else if( r != 0 ) n -= 1;
	return( n );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	TLV16Get
//===========================================================================================================================

OSStatus
	TLV16Get( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		Boolean				inBigEndian, 
		uint16_t			inType, 
		const uint8_t **	outPtr, 
		size_t *			outLen, 
		const uint8_t **	outNext )
{
	OSStatus			err;
	uint16_t			type;
	const uint8_t *		ptr;
	size_t				len;
	
	while( ( err = TLV16GetNext( inSrc, inEnd, inBigEndian, &type, &ptr, &len, &inSrc ) ) == kNoErr )
	{
		if( type == inType )
		{
			*outPtr = ptr;
			*outLen = len;
			break;
		}
	}
	if( outNext ) *outNext = inSrc;
	return( err );
}

//===========================================================================================================================
//	TLV16GetNext
//===========================================================================================================================

OSStatus
	TLV16GetNext( 
		const uint8_t *		inSrc, 
		const uint8_t *		inEnd, 
		Boolean				inBigEndian, 
		uint16_t *			outType, 
		const uint8_t **	outPtr, 
		size_t *			outLen, 
		const uint8_t **	outNext )
{
	OSStatus			err;
	const uint8_t *		ptr;
	uint16_t			type;
	size_t				len;
	const uint8_t *		next;
	
	// 16-bit TLV's have the following format: <2:type> <2:length> <length:data>
	
	require_action_quiet( inSrc != inEnd, exit, err = kNotFoundErr );
	require_action_quiet( inSrc < inEnd, exit, err = kParamErr );
	len = (size_t)( inEnd - inSrc );
	require_action_quiet( len >= 4, exit, err = kUnderrunErr );
	
	if( inBigEndian )
	{
		type = ReadBig16( &inSrc[ 0 ] );
		len  = ReadBig16( &inSrc[ 2 ] );
	}
	else
	{
		type = ReadLittle16( &inSrc[ 0 ] );
		len  = ReadLittle16( &inSrc[ 2 ] );
	}
	ptr  = inSrc + 4;
	next = ptr + len;
	require_action_quiet( ( next >= ptr ) && ( next <= inEnd ), exit, err = kUnderrunErr );
	
	*outType = type;
	*outPtr  = ptr;
	*outLen  = len;
	if( outNext ) *outNext = next;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	DataBuffer_AppendTLV16
//===========================================================================================================================

OSStatus	DataBuffer_AppendTLV16( DataBuffer *inDB, Boolean inBigEndian, uint16_t inType, const void *inData, size_t inLen )
{
	OSStatus			err;
	uint8_t *			dst;
	const uint8_t *		src;
	const uint8_t *		end;
	
	// 16-bit TLV's have the following format:
	//
	//		<2:type> <2:length> <length:data>
	
	if( inLen == kSizeCString ) inLen = strlen( (const char *) inData );
	require_action( inLen <= 0xFFFF, exit, err = kSizeErr );
	
	err = DataBuffer_Grow( inDB, 2 + 2 + inLen, &dst ); // Append <2:type> <2:len> <len:value>
	require_noerr( err, exit );
	
	if( inBigEndian )
	{
		WriteBig16( dst, inType ); dst += 2;
		WriteBig16( dst, inLen );  dst += 2;
	}
	else
	{
		WriteLittle16( dst, inType ); dst += 2;
		WriteLittle16( dst, inLen );  dst += 2;
	}
	
	src = (const uint8_t *) inData;
	end = src + inLen;
	while( src < end ) *dst++ = *src++;
	check( dst == DataBuffer_GetEnd( inDB ) );
	
exit:
	if( !inDB->firstErr ) inDB->firstErr = err;
	return( err );
}

#if 0
#pragma mark -
#endif

#if( !EXCLUDE_UNIT_TESTS )

#include "PrintFUtils.h"
#include "TestUtils.h"

static void	TLV8Test( TUTestContext *inTestCtx );
static void	TLV16Test( TUTestContext *inTestCtx );

//===========================================================================================================================
//	TLVUtilsTest
//===========================================================================================================================

void	TLVUtilsTest( void )
{
	TUPerformTest( TLV8Test );
	TUPerformTest( TLV16Test );
}

//===========================================================================================================================
//	TLV8Test
//===========================================================================================================================

static void	TLV8Test( TUTestContext *inTestCtx )
{
	OSStatus			err;
	const uint8_t *		src;
	const uint8_t *		src2	= NULL;
	const uint8_t *		end;
	uint8_t				type	= 0;
	const uint8_t *		ptr		= NULL;
	size_t				len		= 0;
	size_t				i;
	uint8_t *			storage	= NULL;
	TLV8Buffer			tlv8;
	uint8_t *			ptr2	= NULL;
	int64_t				s64;
	uint8_t				buf[ 32 ];
	
	TLV8BufferInit( &tlv8, SIZE_MAX	 );
	
	// TLV8GetNext
	
	#define kTLV8Test1		""
	src = (const uint8_t *) kTLV8Test1;
	end = src + sizeof_string( kTLV8Test1 );
	err = TLV8GetNext( src, end, &type, &ptr, &len, &src );
	tu_require( err == kNotFoundErr, exit );
	
	#define kTLV8Test2		"\x00"
	src = (const uint8_t *) kTLV8Test2;
	end = src + sizeof_string( kTLV8Test2 );
	err = TLV8GetNext( src, end, &type, &ptr, &len, &src );
	tu_require( err == kNotFoundErr, exit );
	
	#define kTLV8Test3		"\x00\x01"
	src = (const uint8_t *) kTLV8Test3;
	end = src + sizeof_string( kTLV8Test3 );
	err = TLV8GetNext( src, end, &type, &ptr, &len, &src );
	tu_require( err == kUnderrunErr, exit );
	
	#define kTLV8Test4		"\x11\x00"
	src = (const uint8_t *) kTLV8Test4;
	end = src + sizeof_string( kTLV8Test4 );
	err = TLV8GetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require_noerr( err, exit );
	tu_require( type == 0x11, exit );
	tu_require( ptr == src + 2, exit );
	tu_require( len == 0, exit );
	tu_require( src2 == end, exit );
	
	#define kTLV8Test5		"\x11\x01\xAA"
	src  = (const uint8_t *) kTLV8Test5;
	end  = src + sizeof_string( kTLV8Test5 );
	src2 = src;
	err = TLV8GetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require_noerr( err, exit );
	tu_require( type == 0x11, exit );
	tu_require( ptr == src + 2, exit );
	tu_require( len == 1, exit );
	tu_require( memcmp( ptr, "\xAA", len ) == 0, exit );
	tu_require( src2 == end, exit );
	err = TLV8GetNext( src2, end, &type, &ptr, &len, &src2 );
	tu_require( err == kNotFoundErr, exit );
	
	#define kTLV8Test6		"\x55\x01\x15\x22\x04\xBB\xCC\xDD\xEE"
	src = (const uint8_t *) kTLV8Test6;
	end = src + sizeof_string( kTLV8Test6 );
	err = TLV8GetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require_noerr( err, exit );
	tu_require( type == 0x55, exit );
	tu_require( ptr == src + 2, exit );
	tu_require( len == 1, exit );
	tu_require( memcmp( ptr, "\x15", len ) == 0, exit );
	src = src2;
	err = TLV8GetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require_noerr( err, exit );
	tu_require( type == 0x22, exit );
	tu_require( ptr == src + 2, exit );
	tu_require( len == 4, exit );
	tu_require( memcmp( ptr, "\xBB\xCC\xDD\xEE", len ) == 0, exit );
	tu_require( src2 == end, exit );
	
	// TLV8Get
	
	src = (const uint8_t *) kTLV8Test5;
	end = src + sizeof_string( kTLV8Test5 );
	err = TLV8Get( src, end, 0x99, &ptr, &len, &src2 );
	tu_require( err == kNotFoundErr, exit );
	err = TLV8Get( src, end, 0x11, &ptr, &len, &src2 );
	tu_require_noerr( err, exit );
	tu_require( ptr == src + 2, exit );
	tu_require( len == 1, exit );
	tu_require( memcmp( ptr, "\xAA", len ) == 0, exit );
	tu_require( src2 == end, exit );
	err = TLV8Get( src2, end, 0x11, &ptr, &len, &src2 );
	tu_require( err == kNotFoundErr, exit );
	
	src = (const uint8_t *) kTLV8Test6;
	end = src + sizeof_string( kTLV8Test6 );
	err = TLV8Get( src, end, 0x99, &ptr, &len, &src2 );
	tu_require( err == kNotFoundErr, exit );
	err = TLV8Get( src, end, 0x55, &ptr, &len, &src2 );
	tu_require_noerr( err, exit );
	tu_require( ptr == src + 2, exit );
	tu_require( len == 1, exit );
	tu_require( memcmp( ptr, "\x15", len ) == 0, exit );
	src = src2;
	err = TLV8Get( src, end, 0x22, &ptr, &len, &src2 );
	tu_require_noerr( err, exit );
	tu_require( ptr == src + 2, exit );
	tu_require( len == 4, exit );
	tu_require( memcmp( ptr, "\xBB\xCC\xDD\xEE", len ) == 0, exit );
	tu_require( src2 == end, exit );
	
	// TLV8GetBytes
	
	src = (const uint8_t *) kTLV8Test5;
	end = src + sizeof_string( kTLV8Test5 );
	err = TLV8GetBytes( src, end, 0x99, 0, sizeof( buf ), buf, &len, &src2 );
	tu_require( err == kNotFoundErr, exit );
	err = TLV8GetBytes( src, end, 0x11, 0, 1, buf, &len, &src2 );
	tu_require_noerr( err, exit );
	tu_require( len == 1, exit );
	tu_require( memcmp( buf, "\xAA", len ) == 0, exit );
	tu_require( src2 == end, exit );
	err = TLV8GetBytes( src2, end, 0x11, 0, 1, buf, &len, &src2 );
	tu_require( err == kNotFoundErr, exit );
	
	src = (const uint8_t *) kTLV8Test6;
	end = src + sizeof_string( kTLV8Test6 );
	err = TLV8GetBytes( src, end, 0x99, 0, sizeof( buf ), buf, &len, &src2 );
	tu_require( err == kNotFoundErr, exit );
	err = TLV8GetBytes( src, end, 0x55, 1, sizeof( buf ), buf, &len, &src2 );
	tu_require_noerr( err, exit );
	tu_require( len == 1, exit );
	tu_require( memcmp( buf, "\x15", len ) == 0, exit );
	src = src2;
	err = TLV8GetBytes( src, end, 0x22, 5, sizeof( buf ), buf, &len, &src2 );
	tu_require( err == kUnderrunErr, exit );
	err = TLV8GetBytes( src, end, 0x22, 2, sizeof( buf ), buf, &len, &src2 );
	tu_require( len == 4, exit );
	tu_require( memcmp( buf, "\xBB\xCC\xDD\xEE", len ) == 0, exit );
	tu_require( src2 == end, exit );
	
	// TLV8GetOrCopyCoalesced
	
	#define kTLV8Test7 \
		"\x01\x00" \
		"\x01\x00" \
		"\x01\x00"
	src = (const uint8_t *) kTLV8Test7;
	end = src + sizeof_string( kTLV8Test7 );
	ptr = NULL;
	len = 99;
	storage = NULL;
	err = TLV8GetOrCopyCoalesced( src, end, 0x01, &ptr, &len, &storage, &src2 );
	tu_require_noerr( err, exit );
	tu_require( ptr, exit );
	tu_require( len == 0, exit );
	tu_require( !storage, exit );
	tu_require( src2 == end, exit );
	ForgetMem( &storage );
	
	#define kTLV8Test8 \
		"\x01\x01\xAA" \
		"\x01\x00" \
		"\x01\x00"
	src = (const uint8_t *) kTLV8Test8;
	end = src + sizeof_string( kTLV8Test8 );
	ptr = NULL;
	len = 99;
	storage = NULL;
	err = TLV8GetOrCopyCoalesced( src, end, 0x01, &ptr, &len, &storage, &src2 );
	tu_require_noerr( err, exit );
	tu_require( ptr, exit );
	tu_require( len == 1, exit );
	tu_require( memcmp( ptr, "\xAA", len ) == 0, exit );
	tu_require( !storage, exit );
	tu_require( src2 == end, exit );
	ForgetMem( &storage );
	
	#define kTLV8Test9 \
		"\x01\x00" \
		"\x01\x01\xAA" \
		"\x01\x00"
	src = (const uint8_t *) kTLV8Test9;
	end = src + sizeof_string( kTLV8Test9 );
	ptr = NULL;
	len = 99;
	storage = NULL;
	err = TLV8GetOrCopyCoalesced( src, end, 0x01, &ptr, &len, &storage, &src2 );
	tu_require_noerr( err, exit );
	tu_require( ptr, exit );
	tu_require( len == 1, exit );
	tu_require( memcmp( ptr, "\xAA", len ) == 0, exit );
	tu_require( !storage, exit );
	tu_require( src2 == end, exit );
	ForgetMem( &storage );
	
	#define kTLV8Test10 \
		"\x01\x00" \
		"\x01\x01\xAA" \
		"\x01\x01\xBB" \
		"\x01\x00"
	src = (const uint8_t *) kTLV8Test10;
	end = src + sizeof_string( kTLV8Test10 );
	ptr = NULL;
	len = 99;
	storage = NULL;
	err = TLV8GetOrCopyCoalesced( src, end, 0x01, &ptr, &len, &storage, &src2 );
	tu_require_noerr( err, exit );
	tu_require( ptr, exit );
	tu_require( len == 2, exit );
	tu_require( memcmp( ptr, "\xAA\xBB", len ) == 0, exit );
	tu_require( storage, exit );
	tu_require( src2 == end, exit );
	ForgetMem( &storage );
	
	#define kTLV8Test11 \
		"\x01\x01\xAA" \
		"\x01\x02\xBB\xCC" \
		"\x01\x03\xDD\xEE\xFF"
	src = (const uint8_t *) kTLV8Test11;
	end = src + sizeof_string( kTLV8Test11 );
	ptr = NULL;
	len = 99;
	storage = NULL;
	err = TLV8GetOrCopyCoalesced( src, end, 0x01, &ptr, &len, &storage, &src2 );
	tu_require_noerr( err, exit );
	tu_require( ptr, exit );
	tu_require( len == 6, exit );
	tu_require( memcmp( ptr, "\xAA\xBB\xCC\xDD\xEE\xFF", len ) == 0, exit );
	tu_require( storage, exit );
	tu_require( src2 == end, exit );
	ForgetMem( &storage );
	
	#define kTLV8Test12 \
		"\x02\x01\xAA" \
		"\x03\x02\xBB\xCC" \
		"\x04\x03\xDD\xEE\xFF"
	src = (const uint8_t *) kTLV8Test12;
	end = src + sizeof_string( kTLV8Test12 );
	ptr = NULL;
	len = 99;
	storage = NULL;
	err = TLV8GetOrCopyCoalesced( src, end, 0x01, &ptr, &len, &storage, &src2 );
	tu_require( err == kNotFoundErr, exit );
	tu_require( !ptr, exit );
	tu_require( len == 99, exit );
	tu_require( !storage, exit );
	ForgetMem( &storage );
	
	#define kTLV8Test13 \
		"\x01\x01\xAA" \
		"\x01\x02\xBB\xCC" \
		"\x04\x03\xDD\xEE\xFF" \
		"\x01\x01\x11"
	src = (const uint8_t *) kTLV8Test13;
	end = src + sizeof_string( kTLV8Test13 );
	ptr = NULL;
	len = 99;
	storage = NULL;
	err = TLV8GetOrCopyCoalesced( src, end, 0x01, &ptr, &len, &storage, &src2 );
	tu_require_noerr( err, exit );
	tu_require( ptr, exit );
	tu_require( len == 3, exit );
	tu_require( memcmp( ptr, "\xAA\xBB\xCC", len ) == 0, exit );
	tu_require( storage, exit );
	tu_require( src2 == ( src + 7 ), exit );
	ForgetMem( &storage );
	err = TLV8GetOrCopyCoalesced( src2, end, 0x04, &ptr, &len, &storage, &src2 );
	tu_require_noerr( err, exit );
	tu_require( ptr == src + 9, exit );
	tu_require( len == 3, exit );
	tu_require( memcmp( ptr, "\xDD\xEE\xFF", len ) == 0, exit );
	tu_require( !storage, exit );
	tu_require( src2 == ( src + 12 ), exit );
	err = TLV8GetOrCopyCoalesced( src2, end, 0x01, &ptr, &len, &storage, &src2 );
	tu_require_noerr( err, exit );
	tu_require( ptr == src + 14, exit );
	tu_require( len == 1, exit );
	tu_require( memcmp( ptr, "\x11", len ) == 0, exit );
	tu_require( !storage, exit );
	tu_require( src2 == end, exit );
	err = TLV8GetOrCopyCoalesced( src2, end, 0x01, &ptr, &len, &storage, &src2 );
	tu_require( err != kNoErr, exit );
	tu_require( !storage, exit );
	tu_require( src2 == end, exit );
	
	// TLV8Buffer
	
	TLV8BufferInit( &tlv8, 256 );
	err = TLV8BufferAppend( &tlv8, 0x01, "\xAA", 1 );
	tu_require_noerr( err, exit );
	err = TLV8BufferAppend( &tlv8, 0x02, "value2", kSizeCString );
	tu_require_noerr( err, exit );
	tu_require( TLV8BufferGetLen( &tlv8 ) == 11, exit );
	ptr = TLV8BufferGetPtr( &tlv8 );
	tu_require( ptr == tlv8.inlineBuffer, exit );
	tu_require( !tlv8.mallocedPtr, exit );
	tu_require( memcmp( ptr, "\x01\x01\xAA\x02\x06value2", 11 ) == 0, exit );
	TLV8BufferFree( &tlv8 );
	
	TLV8BufferInit( &tlv8, 2000 );
	err = TLV8BufferAppend( &tlv8, 0x01, "value1", kSizeCString );
	tu_require_noerr( err, exit );
	err = TLV8BufferAppend( &tlv8, 0x02, "val2", kSizeCString );
	tu_require_noerr( err, exit );
	len = 1093;
	storage = (uint8_t *) malloc( len );
	require_action( storage, exit, err = kNoMemoryErr );
	for( i = 0; i < len; ++i ) storage[ i ] = (uint8_t)( i & 0xFF );
	err = TLV8BufferAppend( &tlv8, 0x03, storage, len );
	tu_require_noerr( err, exit );
	ForgetMem( &storage );
	tu_require( tlv8.mallocedPtr, exit );
	ptr = TLV8BufferGetPtr( &tlv8 );
	tu_require( ptr == tlv8.mallocedPtr, exit );
	tu_require( ptr != tlv8.inlineBuffer, exit );
	tu_require( TLV8BufferGetLen( &tlv8 ) == 1117, exit );
	tu_require( memcmp( ptr, "\x01\x06value1\x02\x04val2", 14 ) == 0, exit );
	ptr = &ptr[ 14 ];
	// 0-255
	tu_require( *ptr++ == 0x03, exit );
	tu_require( *ptr++ == 255, exit );
	for( i = 0; i < 255; ++i ) tu_require( *ptr++ == ( i & 0xFF ), exit );
	// 255-510
	tu_require( *ptr++ == 0x03, exit );
	tu_require( *ptr++ == 255, exit );
	for( ; i < 510; ++i ) tu_require( *ptr++ == ( i & 0xFF ), exit );
	// 510-766
	tu_require( *ptr++ == 0x03, exit );
	tu_require( *ptr++ == 255, exit );
	for( ; i < 765; ++i ) tu_require( *ptr++ == ( i & 0xFF ), exit );
	// 765-1020
	tu_require( *ptr++ == 0x03, exit );
	tu_require( *ptr++ == 255, exit );
	for( ; i < 1020; ++i ) tu_require( *ptr++ == ( i & 0xFF ), exit );
	// 1020-1093
	tu_require( *ptr++ == 0x03, exit );
	tu_require( *ptr++ == 73, exit );
	for( ; i < 1093; ++i ) tu_require( *ptr++ == ( i & 0xFF ), exit );
	tu_require( (size_t)( ptr - TLV8BufferGetPtr( &tlv8 ) ) == 1117, exit );
	
	// TLV8BufferDetach
	
	ptr2 = NULL;
	len = 0;
	err = TLV8BufferDetach( &tlv8, &ptr2, &len );
	tu_require_noerr( err, exit );
	tu_require( ptr2, exit );
	tu_require( ptr2 != tlv8.inlineBuffer, exit );
	tu_require( len == 1117, exit );
	tu_require( tlv8.ptr == tlv8.inlineBuffer, exit );
	tu_require( tlv8.len == 0, exit );
	tu_require( tlv8.maxLen == 2000, exit );
	tu_require( !tlv8.mallocedPtr, exit );
	tu_require( memcmp( ptr2, "\x01\x06value1\x02\x04val2", 14 ) == 0, exit );
	ptr = &ptr2[ 14 ];
	// 0-255
	tu_require( *ptr++ == 0x03, exit );
	tu_require( *ptr++ == 255, exit );
	for( i = 0; i < 255; ++i ) tu_require( *ptr++ == ( i & 0xFF ), exit );
	// 255-510
	tu_require( *ptr++ == 0x03, exit );
	tu_require( *ptr++ == 255, exit );
	for( ; i < 510; ++i ) tu_require( *ptr++ == ( i & 0xFF ), exit );
	// 510-766
	tu_require( *ptr++ == 0x03, exit );
	tu_require( *ptr++ == 255, exit );
	for( ; i < 765; ++i ) tu_require( *ptr++ == ( i & 0xFF ), exit );
	// 765-1020
	tu_require( *ptr++ == 0x03, exit );
	tu_require( *ptr++ == 255, exit );
	for( ; i < 1020; ++i ) tu_require( *ptr++ == ( i & 0xFF ), exit );
	// 1020-1093
	tu_require( *ptr++ == 0x03, exit );
	tu_require( *ptr++ == 73, exit );
	for( ; i < 1093; ++i ) tu_require( *ptr++ == ( i & 0xFF ), exit );
	tu_require( (size_t)( ptr - ptr2 ) == 1117, exit );
	ForgetMem( &ptr2 );
	TLV8BufferFree( &tlv8 );
	
	TLV8BufferInit( &tlv8, 256 );
	err = TLV8BufferAppend( &tlv8, 0x01, "\xAA", 1 );
	tu_require_noerr( err, exit );
	err = TLV8BufferAppend( &tlv8, 0x02, "value2", kSizeCString );
	tu_require_noerr( err, exit );
	err = TLV8BufferDetach( &tlv8, &ptr2, &len );
	tu_require_noerr( err, exit );
	tu_require( ptr2, exit );
	tu_require( ptr2 != tlv8.inlineBuffer, exit );
	tu_require( len == 11, exit );
	tu_require( tlv8.ptr == tlv8.inlineBuffer, exit );
	tu_require( tlv8.len == 0, exit );
	tu_require( tlv8.maxLen == 256, exit );
	tu_require( !tlv8.mallocedPtr, exit );
	tu_require( memcmp( ptr2, "\x01\x01\xAA\x02\x06value2", 11 ) == 0, exit );
	ForgetMem( &ptr2 );
	TLV8BufferFree( &tlv8 );
	
	// Integer tests
	
	TLV8BufferInit( &tlv8, 256 );
	err = TLV8BufferAppendUInt64( &tlv8, 0x01, 0 );
	tu_require_noerr( err, exit );
	tu_require( tlv8.len == 3, exit );
	tu_require( memcmp( tlv8.ptr, "\x01\x01\x00", tlv8.len ) == 0, exit );
	s64 = TLV8GetSInt64( tlv8.ptr, tlv8.ptr + tlv8.len, 0x01, &err, NULL );
	tu_require_noerr( err, exit );
	tu_require( s64 == 0, exit );
	
	TLV8BufferInit( &tlv8, 256 );
	err = TLV8BufferAppendSInt64( &tlv8, 0x01, -100 );
	tu_require_noerr( err, exit );
	tu_require( tlv8.len == 3, exit );
	tu_require( memcmp( tlv8.ptr, "\x01\x01\x9C", tlv8.len ) == 0, exit );
	s64 = TLV8GetSInt64( tlv8.ptr, tlv8.ptr + tlv8.len, 0x01, &err, NULL );
	tu_require_noerr( err, exit );
	tu_require( s64 == -100, exit );
	
	TLV8BufferInit( &tlv8, 256 );
	err = TLV8BufferAppendUInt64( &tlv8, 0x01, 250 );
	tu_require_noerr( err, exit );
	tu_require( tlv8.len == 3, exit );
	tu_require( memcmp( tlv8.ptr, "\x01\x01\xFA", tlv8.len ) == 0, exit );
	s64 = (int64_t) TLV8GetUInt64( tlv8.ptr, tlv8.ptr + tlv8.len, 0x01, &err, NULL );
	tu_require_noerr( err, exit );
	tu_require( s64 == 250, exit );
	
	TLV8BufferInit( &tlv8, 256 );
	err = TLV8BufferAppendSInt64( &tlv8, 0x01, -1000 );
	tu_require_noerr( err, exit );
	tu_require( tlv8.len == 4, exit );
	tu_require( memcmp( tlv8.ptr, "\x01\x02\x18\xFC", tlv8.len ) == 0, exit );
	s64 = TLV8GetSInt64( tlv8.ptr, tlv8.ptr + tlv8.len, 0x01, &err, NULL );
	tu_require_noerr( err, exit );
	tu_require( s64 == -1000, exit );
	
	TLV8BufferInit( &tlv8, 256 );
	err = TLV8BufferAppendUInt64( &tlv8, 0x01, 60000 );
	tu_require_noerr( err, exit );
	tu_require( tlv8.len == 4, exit );
	tu_require( memcmp( tlv8.ptr, "\x01\x02\x60\xEA", tlv8.len ) == 0, exit );
	s64 = (int64_t) TLV8GetUInt64( tlv8.ptr, tlv8.ptr + tlv8.len, 0x01, &err, NULL );
	tu_require_noerr( err, exit );
	tu_require( s64 == 60000, exit );
	
	TLV8BufferInit( &tlv8, 256 );
	err = TLV8BufferAppendUInt64( &tlv8, 0x01, 200000 );
	tu_require_noerr( err, exit );
	tu_require( tlv8.len == 6, exit );
	tu_require( memcmp( tlv8.ptr, "\x01\x04\x40\x0D\x03\x00", tlv8.len ) == 0, exit );
	s64 = (int64_t) TLV8GetUInt64( tlv8.ptr, tlv8.ptr + tlv8.len, 0x01, &err, NULL );
	tu_require_noerr( err, exit );
	tu_require( s64 == 200000, exit );
	
	// TLV8MaxPayloadBytesForTotalBytes
	
	tu_require( TLV8MaxPayloadBytesForTotalBytes( 0 ) == 0, exit );
	tu_require( TLV8MaxPayloadBytesForTotalBytes( 1 ) == 0, exit );
	for( i = 2;   i <= 257; ++i ) tu_require( TLV8MaxPayloadBytesForTotalBytes( i ) == ( i - 2 ), exit );
	tu_require( TLV8MaxPayloadBytesForTotalBytes( 258 ) == 255, exit );
	tu_require( TLV8MaxPayloadBytesForTotalBytes( 259 ) == 255, exit );
	for( i = 260; i <= 514; ++i ) tu_require( TLV8MaxPayloadBytesForTotalBytes( i ) == ( i - 4 ), exit );
	tu_require( TLV8MaxPayloadBytesForTotalBytes( 515 ) == 510, exit );
	tu_require( TLV8MaxPayloadBytesForTotalBytes( 516 ) == 510, exit );
	tu_require( TLV8MaxPayloadBytesForTotalBytes( 517 ) == 511, exit );
	tu_require( TLV8MaxPayloadBytesForTotalBytes( 518 ) == 512, exit );
	tu_require( TLV8MaxPayloadBytesForTotalBytes( 519 ) == 513, exit );
	
	// Print test
	
	if( TULogLevelEnabled( inTestCtx, kLogLevelVerbose ) )
	{
		TLV8BufferInit( &tlv8, 512 );
		err = TLV8BufferAppend( &tlv8, 0x00, "", 0 );
		tu_require_noerr( err, exit );
		err = TLV8BufferAppend( &tlv8, 0x01, "This is all text", kSizeCString );
		tu_require_noerr( err, exit );
		err = TLV8BufferAppend( &tlv8, 0x02, "\x11\x22\x33\xAA\xBB", 5 );
		tu_require_noerr( err, exit );
		err = TLV8BufferAppend( &tlv8, 0x03, 
			"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
			"\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF", 
			32 );
		tu_require_noerr( err, exit );
		err = TLV8BufferAppend( &tlv8, 0x04, "a", 1 );
		tu_require_noerr( err, exit );
		err = TLV8BufferAppend( &tlv8, 0x05, "\xAF", 1 );
		tu_require_noerr( err, exit );
		
		tu_require_noerr( err, exit );
		err = TLV8BufferAppend( &tlv8, 0x10, "", 0 );
		tu_require_noerr( err, exit );
		err = TLV8BufferAppend( &tlv8, 0x11, "This is all text", kSizeCString );
		tu_require_noerr( err, exit );
		err = TLV8BufferAppend( &tlv8, 0x12, "\x11\x22\x33\xAA\xBB", 5 );
		tu_require_noerr( err, exit );
		err = TLV8BufferAppend( &tlv8, 0x13, 
			"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
			"\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF", 
			32 );
		tu_require_noerr( err, exit );
		
		#define kTLVPrintTestDesciptors \
			"\x00" "1\0" \
			"\x01" "Item 2\0" \
			"\x02" "Test Item 3\0" \
			"\x03" "Another Item 4\0" \
			"\x04" "T 5\0" \
			"\x05" "Testing 6\0" \
			"\x00"
		
		ptr = TLV8BufferGetPtr( &tlv8 );
		len = TLV8BufferGetLen( &tlv8 );
		TULogF( inTestCtx, kLogLevelVerbose, "%{tlv8}\n%.1H\n", kTLVPrintTestDesciptors, 
			ptr, (int) len, ptr, (int) len, (int) len );
		TLV8BufferFree( &tlv8 );
	}
	
exit:
	ForgetMem( &storage );
	ForgetMem( &ptr2 );
	TLV8BufferFree( &tlv8 );
}

//===========================================================================================================================
//	TLV16Test
//===========================================================================================================================

static void	TLV16Test( TUTestContext *inTestCtx )
{
	OSStatus			err;
	const uint8_t *		src  = NULL;
	const uint8_t *		src2 = NULL;
	const uint8_t *		end  = NULL;
	uint16_t			type = 0;
	const uint8_t *		ptr  = NULL;
	size_t				len  = 0;
	
	// TLV16GetNext
	
	#define kTLV16Test1		""
	src = (const uint8_t *) kTLV16Test1;
	end = src + sizeof_string( kTLV16Test1 );
	err = TLV16BEGetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require( err == kNotFoundErr, exit );
	err = TLV16LEGetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require( err == kNotFoundErr, exit );
	
	#define kTLV16Test2		"\x00"
	src = (const uint8_t *) kTLV16Test2;
	end = src + sizeof_string( kTLV16Test2 );
	err = TLV16BEGetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require( err == kUnderrunErr, exit );
	err = TLV16LEGetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require( err == kUnderrunErr, exit );
	
	#define kTLV16Test3		"\x00\x00"
	src = (const uint8_t *) kTLV16Test3;
	end = src + sizeof_string( kTLV16Test3 );
	err = TLV16BEGetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require( err == kUnderrunErr, exit );
	err = TLV16LEGetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require( err == kUnderrunErr, exit );
	
	#define kTLV16Test4		"\x00\x00\x00"
	src = (const uint8_t *) kTLV16Test4;
	end = src + sizeof_string( kTLV16Test4 );
	err = TLV16BEGetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require( err == kUnderrunErr, exit );
	err = TLV16LEGetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require( err == kUnderrunErr, exit );
	
	#define kTLV16Test5		"\x00\x00\x00\x00"
	src = (const uint8_t *) kTLV16Test5;
	end = src + sizeof_string( kTLV16Test5 );
	err = TLV16BEGetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require_noerr( err, exit );
	tu_require( type == 0, exit );
	tu_require( ptr == ( src + 4 ), exit );
	tu_require( len == 0, exit );
	err = TLV16LEGetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require_noerr( err, exit );
	tu_require( type == 0, exit );
	tu_require( ptr == ( src + 4 ), exit );
	tu_require( len == 0, exit );
	
	#define kTLV16Test6		"\xAA\x11\x00\x04" "abcd"
	src = (const uint8_t *) kTLV16Test6;
	end = src + sizeof_string( kTLV16Test6 );
	err = TLV16BEGetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require_noerr( err, exit );
	tu_require( type == 0xAA11, exit );
	tu_require( ptr == ( src + 4 ), exit );
	tu_require( len == 4, exit );
	tu_require( memcmp( ptr, "abcd", len ) == 0, exit );
	
	#define kTLV16Test7		"\x11\xAA\x04\x00" "abcd"
	src = (const uint8_t *) kTLV16Test7;
	end = src + sizeof_string( kTLV16Test7 );
	err = TLV16LEGetNext( src, end, &type, &ptr, &len, &src2 );
	tu_require_noerr( err, exit );
	tu_require( type == 0xAA11, exit );
	tu_require( ptr == ( src + 4 ), exit );
	tu_require( len == 4, exit );
	tu_require( memcmp( ptr, "abcd", len ) == 0, exit );
	
exit:
	return;
}

#endif // !EXCLUDE_UNIT_TESTS
