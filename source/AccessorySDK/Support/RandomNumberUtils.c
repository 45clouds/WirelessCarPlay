/*
	File:    	RandomNumberUtils.c
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
	
	Copyright (C) 2001-2015 Apple Inc. All Rights Reserved.
*/

#include "RandomNumberUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"

#if( TARGET_HAS_STD_C_LIB )
	#include <stdlib.h>
#endif

#if( TARGET_HAS_MOCANA_SSL )
	#include "Networking/SSL/mtypes.h"
	
	#include "Networking/SSL/merrors.h"
	#include "Networking/SSL/random.h"
#endif

#if( TARGET_PLATFORM_WICED && defined( _STM32x_ ) )
	#include "stm32f2xx_rng.h"
#endif

#if( TARGET_OS_LINUX || ( TARGET_OS_QNX && TARGET_NO_OPENSSL ) )
	#include <fcntl.h>
	#include <pthread.h>
#endif

#if( TARGET_OS_QNX && !TARGET_NO_OPENSSL )
	#include <openssl/rand.h>
#endif


#if( TARGET_OS_DARWIN )
//===========================================================================================================================
//	RandomBytes
//===========================================================================================================================

OSStatus	RandomBytes( void *inBuffer, size_t inByteCount )
{
	return( CCRandomGenerateBytes( inBuffer, inByteCount ) );
}
#endif

#if( TARGET_HAS_MOCANA_SSL )
//===========================================================================================================================
//	RandomBytes
//===========================================================================================================================

static randomContext *		gRandomContext = NULL;

OSStatus	RandomBytes( void *inBuffer, size_t inByteCount )
{
	uint8_t * const		buf = (uint8_t *) inBuffer;
	OSStatus			err;
	
	if( gRandomContext == NULL )
	{
		err = RANDOM_acquireContext( &gRandomContext );
		require_noerr( err, exit );
	}
	
	err = RANDOM_numberGenerator( gRandomContext, buf, (sbyte4) inByteCount );
	require_noerr( err, exit );
	
exit:
	return( err );
}
#endif

#if( TARGET_PLATFORM_WICED && defined( __STM32F2xx_RNG_H ) )
//===========================================================================================================================
//	RandomBytes
//
//	STM32F2xx implementation.
//===========================================================================================================================

OSStatus	RandomBytes( void *inBuffer, size_t inByteCount )
{
	uint8_t * const		buf = (uint8_t *) inBuffer;
	size_t				offset;
	size_t				len;
	uint32_t			r;
	
	// Enable RNG clock source and peripheral.
	
	RCC_AHB2PeriphClockCmd( RCC_AHB2Periph_RNG, ENABLE );
	RNG_Cmd( ENABLE );
	
	for( offset = 0; ( inByteCount - offset ) >= 4; )
	{
		while( RNG_GetFlagStatus( RNG_FLAG_DRDY ) == RESET ) {} // Wait for RNG to be ready.
		r = RNG_GetRandomNumber();
		buf[ offset++ ] = (uint8_t)( ( r >> 24 ) & 0xFF );
		buf[ offset++ ] = (uint8_t)( ( r >> 16 ) & 0xFF );
		buf[ offset++ ] = (uint8_t)( ( r >>  8 ) & 0xFF );
		buf[ offset++ ] = (uint8_t)(   r         & 0xFF );
	}
	
	len = inByteCount - offset;
	if( len == 3 )
	{
		while( RNG_GetFlagStatus( RNG_FLAG_DRDY ) == RESET ) {} // Wait for RNG to be ready.
		r = RNG_GetRandomNumber();
		buf[ offset++ ] = (uint8_t)( ( r >> 24 ) & 0xFF );
		buf[ offset++ ] = (uint8_t)( ( r >> 16 ) & 0xFF );
		buf[ offset   ] = (uint8_t)( ( r >>  8 ) & 0xFF );
	}
	else if( len == 2 )
	{
		while( RNG_GetFlagStatus( RNG_FLAG_DRDY ) == RESET ) {} // Wait for RNG to be ready.
		r = RNG_GetRandomNumber();
		buf[ offset++ ] = (uint8_t)( ( r >> 24 ) & 0xFF );
		buf[ offset   ] = (uint8_t)( ( r >> 16 ) & 0xFF );
	}
	else if( len == 1 )
	{
		while( RNG_GetFlagStatus( RNG_FLAG_DRDY ) == RESET ) {} // Wait for RNG to be ready.
		r = RNG_GetRandomNumber();
		buf[ offset ] = (uint8_t)( ( r >> 24 ) & 0xFF );
	}
	return( kNoErr );
}
#endif

#if( TARGET_OS_POSIX && !TARGET_OS_DARWIN && !TARGET_OS_LINUX && !TARGET_OS_QNX )
//===========================================================================================================================
//	RandomBytes
//===========================================================================================================================

OSStatus	RandomBytes( void *inBuffer, size_t inByteCount )
{
	uint8_t * const		buf = (uint8_t *) inBuffer;
	size_t				offset;
	size_t				len;
	uint32_t			r;
	
	for( offset = 0; ( inByteCount - offset ) >= 4; )
	{
		r = arc4random();
		buf[ offset++ ] = (uint8_t)( ( r >> 24 ) & 0xFF );
		buf[ offset++ ] = (uint8_t)( ( r >> 16 ) & 0xFF );
		buf[ offset++ ] = (uint8_t)( ( r >>  8 ) & 0xFF );
		buf[ offset++ ] = (uint8_t)(   r         & 0xFF );
	}
	
	len = inByteCount - offset;
	if( len == 3 )
	{
		r = arc4random();
		buf[ offset++ ] = (uint8_t)( ( r >> 24 ) & 0xFF );
		buf[ offset++ ] = (uint8_t)( ( r >> 16 ) & 0xFF );
		buf[ offset   ] = (uint8_t)( ( r >>  8 ) & 0xFF );
	}
	else if( len == 2 )
	{
		r = arc4random();
		buf[ offset++ ] = (uint8_t)( ( r >> 24 ) & 0xFF );
		buf[ offset   ] = (uint8_t)( ( r >> 16 ) & 0xFF );
	}
	else if( len == 1 )
	{
		r = arc4random();
		buf[ offset   ] = (uint8_t)( ( r >> 24 ) & 0xFF );
	}
	return( kNoErr );
}
#endif

#if( TARGET_OS_LINUX || ( TARGET_OS_QNX && TARGET_NO_OPENSSL ) )
//===========================================================================================================================
//	RandomBytes
//===========================================================================================================================

OSStatus	RandomBytes( void *inBuffer, size_t inByteCount )
{
	static pthread_mutex_t		sRandomLock = PTHREAD_MUTEX_INITIALIZER;
	static int					sRandomFD = -1;
	uint8_t *					dst;
	ssize_t						n;
	
	pthread_mutex_lock( &sRandomLock );
	while( sRandomFD < 0 )
	{
		sRandomFD = open( "/dev/urandom", O_RDONLY );
		if( sRandomFD < 0 ) { dlogassert( "open urandom error: %#m", errno ); sleep( 1 ); continue; }
		break;
	}
	pthread_mutex_unlock( &sRandomLock );
	
	dst = (uint8_t *) inBuffer;
	while( inByteCount > 0 )
	{
		n = read( sRandomFD, dst, inByteCount );
		if( n < 0 ) { dlogassert( "read urandom error: %#m", errno ); sleep( 1 ); continue; }
		dst += n;
		inByteCount -= n;
	}
	return( kNoErr );
}
#endif

#if( TARGET_OS_QNX && !TARGET_NO_OPENSSL )
//===========================================================================================================================
//	RandomBytes
//===========================================================================================================================

OSStatus	RandomBytes( void *inBuffer, size_t inByteCount )
{
	return( RAND_bytes( (unsigned char *) inBuffer, (int) inByteCount ) ? kNoErr : kUnknownErr );
}
#endif

#if( TARGET_OS_WINDOWS )
//===========================================================================================================================
//	RandomBytes
//===========================================================================================================================

#define RtlGenRandom		SystemFunction036
#ifdef __cplusplus
extern "C"
#endif
BOOLEAN NTAPI RtlGenRandom(PVOID RandomBuffer, ULONG RandomBufferLength);

OSStatus	RandomBytes( void *inBuffer, size_t inByteCount )
{
	OSStatus		err;
	
	err = RtlGenRandom( inBuffer, (ULONG) inByteCount ) ? kNoErr : kUnknownErr;
	check_noerr( err );
	return( err );
}
#endif

//===========================================================================================================================
//	RandomString
//===========================================================================================================================

char *	RandomString( const char *inCharSet, size_t inCharSetSize, size_t inMinChars, size_t inMaxChars, char *outString )
{
	uint32_t		r;
	char *			ptr;
	char *			end;
	
	check( inMinChars <= inMaxChars );
	
	RandomBytes( &r, sizeof( r ) );
	ptr = outString;
	end = ptr + ( inMinChars + ( r % ( ( inMaxChars - inMinChars ) + 1 ) ) );
	while( ptr < end )
	{
		RandomBytes( &r, sizeof( r ) );
		*ptr++ = inCharSet[ r % inCharSetSize ];
	}
	*ptr = '\0';
	return( outString );
}

#if 0
#pragma mark -
#endif

#if( !EXCLUDE_UNIT_TESTS )

#include "TestUtils.h"

static void	RandomBytesTest( TUTestContext *inTestCtx );

//===========================================================================================================================
//	RandomNumberUtilsTest
//===========================================================================================================================

void	RandomNumberUtilsTest( void )
{
	TUPerformTest( RandomBytesTest );
}

//===========================================================================================================================
//	RandomBytesTest
//===========================================================================================================================

static void	RandomBytesTest( TUTestContext *inTestCtx )
{
	OSStatus		err;
	uint8_t			buf[ 16 ];
	uint8_t			buf2[ 16 ];
	size_t			i, n;
	
	// Make sure at least 1/2 of the bytes were changed.
	
	memset( buf, 0, sizeof( buf ) );
	err = RandomBytes( buf, sizeof( buf ) );
	tu_require_noerr( err, exit );
	n = 0;
	for( i = 0; i < countof( buf ); ++i ) { if( buf[ i ] != 0 ) ++n; }
	tu_require( n >= ( countof( buf ) / 2 ), exit );
	
	// Make sure it doesn't return the same bytes twice.
	
	for( i = 0; i < countof( buf ); ++i ) buf2[ i ] = buf[ i ];
	err = RandomBytes( buf2, sizeof( buf2 ) );
	tu_require_noerr( err, exit );
	tu_require( memcmp( buf, buf2, sizeof( buf ) ) != 0, exit );
	
exit:
	return;
}
#endif // !EXCLUDE_UNIT_TESTS
