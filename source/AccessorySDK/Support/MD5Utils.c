/*
	File:    	MD5Utils.c
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
	
	Copyright (C) 2007-2013 Apple Inc. All Rights Reserved.
*/

#include "CommonServices.h"	// Include early for TARGET_*, etc. definitions.
#include "DebugServices.h"	// Include early for DEBUG_*, etc. definitions.

#if( TARGET_HAS_STD_C_LIB )
	#include <stddef.h>
	#include <string.h>
#endif

#include "MD5Utils.h"

/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 *
 * Changed so as no longer to depend on Colin Plumb's `usual.h' header
 * definitions; now uses stuff from dpkg's config.h.
 *  - Ian Jackson <ijackson@nyx.cs.du.edu>.
 * Still in the public domain.
 */

#if TARGET_RT_BIG_ENDIAN
static void	byteSwap(uint32_t *buf, unsigned words);
static void	byteSwap(uint32_t *buf, unsigned words)
{
	uint8_t *p = (uint8_t *)buf;

	do {
		*buf++ = (uint32_t)(p[3] << 8 | p[2]) << 16 | (p[1] << 8 | p[0]);
		p += 4;
	} while (--words);
}
#else
#define byteSwap(buf,words)
#endif

static void	MD5Transform( uint32_t buf[ 4 ], uint32_t const in[ 16 ], int ver);

/*
 * Helper routine for one-shot MD5's.
 */ 
void	MD5OneShot( const void *inSourcePtr, size_t inSourceSize, uint8_t outKey[ 16 ] )
{
	MD5Context		context;
	
	MD5Init( &context );
	MD5Update( &context, inSourcePtr, inSourceSize );
	MD5Final( outKey, &context );
}

void	MD5OneShot_V1( const void *inSourcePtr, size_t inSourceSize, uint8_t outKey[ 16 ] )
{
	MD5Context		context;
	
	MD5Init( &context );
	MD5Update_V1( &context, inSourcePtr, inSourceSize );
	MD5Final_V1( outKey, &context );
}

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
void	MD5Init(MD5Context *ctx)
{
	ctx->buf[0] = 0x67452301;
	ctx->buf[1] = 0xefcdab89;
	ctx->buf[2] = 0x98badcfe;
	ctx->buf[3] = 0x10325476;

	ctx->bytes[0] = 0;
	ctx->bytes[1] = 0;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
static void	MD5Update_Internal(MD5Context *ctx, void const *inBuf, size_t len, int ver)
{
	uint32_t 				t;
	uint8_t const *		buf;
	
	buf = ( uint8_t const * ) inBuf;
	
	/* Update byte count */

	t = ctx->bytes[0];
	if ((ctx->bytes[0] = (uint32_t)(t + len)) < t)
		ctx->bytes[1]++;	/* Carry from low to high */

	t = 64 - (t & 0x3f);	/* Space available in ctx->in (at least 1) */
	if (t > len) {
		memcpy((uint8_t *)ctx->in + 64 - t, buf, len);
		return;
	}
	/* First chunk is an odd size */
	memcpy((uint8_t *)ctx->in + 64 - t, buf, t);
	byteSwap(ctx->in, 16);
	MD5Transform(ctx->buf, ctx->in, ver);
	buf += t;
	len -= t;

	/* Process data in 64-byte chunks */
	while (len >= 64) {
		memcpy(ctx->in, buf, 64);
		byteSwap(ctx->in, 16);
		MD5Transform(ctx->buf, ctx->in, ver);
		buf += 64;
		len -= 64;
	}

	/* Handle any remaining bytes of data. */
	memcpy(ctx->in, buf, len);
}

void	MD5Update(MD5Context *ctx, const void *inBuf, size_t len)
{
	MD5Update_Internal(ctx,inBuf,len,0);
}

void	MD5Update_V1(MD5Context *ctx, const void *inBuf, size_t len)
{
	MD5Update_Internal(ctx,inBuf,len,1);
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
static void	MD5Final_Internal(uint8_t digest[16], MD5Context *ctx, int ver)
{
	int count = ctx->bytes[0] & 0x3f;	/* Number of bytes in ctx->in */
	uint8_t *p = (uint8_t *)ctx->in + count;

	/* Set the first char of padding to 0x80.  There is always room. */
	*p++ = 0x80;

	/* Bytes of padding needed to make 56 bytes (-8..55) */
	count = 56 - 1 - count;

	if (count < 0) {	/* Padding forces an extra block */
		memset(p, 0, count + 8);
		byteSwap(ctx->in, 16);
		MD5Transform(ctx->buf, ctx->in, ver);
		p = (uint8_t *)ctx->in;
		count = 56;
	}
	memset(p, 0, count);
	byteSwap(ctx->in, 14);

	/* Append length in bits and transform */
	ctx->in[14] = ctx->bytes[0] << 3;
	ctx->in[15] = ctx->bytes[1] << 3 | ctx->bytes[0] >> 29;
	MD5Transform(ctx->buf, ctx->in, ver);

	byteSwap(ctx->buf, 4);
	memcpy(digest, ctx->buf, 16);
	MemZeroSecure(ctx, sizeof(*ctx));
}

void	MD5Final(uint8_t digest[16], MD5Context *ctx)
{
	MD5Final_Internal(digest, ctx, 0);
}

void	MD5Final_V1(uint8_t digest[16], MD5Context *ctx)
{
	MD5Final_Internal(digest, ctx, 1);
}

/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f,w,x,y,z,in,s) \
	 (w += f(x,y,z) + in, w = (w<<s | w>>(32-s)) + x)

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void	MD5Transform(uint32_t buf[4], uint32_t const in[16], int ver)
{
	register uint32_t a, b, c, d;

	a = buf[0];
	b = buf[1];
	c = buf[2];
	d = buf[3];

	MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
	MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
	MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
	MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
	MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
	MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
	MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
	MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
	MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
	MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
	MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
	MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
	MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
	MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
	MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
	MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

	MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
	MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
	MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
	MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
	MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
	MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
	MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
	MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
	MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
	MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
	MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
	
	if ( ver == 1 )
	{
		MD5STEP(F2, b, c, d, a, in[8] + 0x445a14ed, 20);
	}
	else
	{
		MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
	}

	MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
	MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
	MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
	MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

	MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
	MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
	MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
	MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
	MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
	MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
	MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
	MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
	MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
	MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
	MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
	MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
	MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
	MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
	MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
	MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

	MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
	MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
	MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
	MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
	MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
	MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
	MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
	MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
	MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
	MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
	MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
	MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
	MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
	MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
	MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
	MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

	buf[0] += a;
	buf[1] += b;
	buf[2] += c;
	buf[3] += d;
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	MD5UtilsTest
//===========================================================================================================================

#define strcpy_literal( DST, SRC )		memcpy( DST, SRC, sizeof( SRC ) )

OSStatus	MD5UtilsTest( void )
{
	OSStatus		err;
	char			str[ 256 ];
	uint8_t			buf[ 32 ];
	uint8_t *		ptr;
	uint8_t *		end;
	
	end = buf + sizeof( buf );
	
	// Tests from RFC 1321.
	
	memset( buf, 'A', sizeof( buf ) );
	strcpy_literal( str, "" );
	MD5OneShot( str, strlen( str ), buf );
	require_action( memcmp( buf, "\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04\xe9\x80\x09\x98\xec\xf8\x42\x7e", 16 ) == 0, exit, err = kResponseErr );
	for( ptr = buf + 16; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	memset( buf, 'A', sizeof( buf ) );
	strcpy_literal( str, "a" );
	MD5OneShot( str, strlen( str ), buf );
	require_action( memcmp( buf, "\x0c\xc1\x75\xb9\xc0\xf1\xb6\xa8\x31\xc3\x99\xe2\x69\x77\x26\x61", 16 ) == 0, exit, err = kResponseErr );
	for( ptr = buf + 16; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	memset( buf, 'A', sizeof( buf ) );
	strcpy_literal( str, "abc" );
	MD5OneShot( str, strlen( str ), buf );
	require_action( memcmp( buf, "\x90\x01\x50\x98\x3c\xd2\x4f\xb0\xd6\x96\x3f\x7d\x28\xe1\x7f\x72", 16 ) == 0, exit, err = kResponseErr );
	for( ptr = buf + 16; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	memset( buf, 'A', sizeof( buf ) );
	strcpy_literal( str, "message digest" );
	MD5OneShot( str, strlen( str ), buf );
	require_action( memcmp( buf, "\xf9\x6b\x69\x7d\x7c\xb7\x93\x8d\x52\x5a\x2f\x31\xaa\xf1\x61\xd0", 16 ) == 0, exit, err = kResponseErr );
	for( ptr = buf + 16; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	memset( buf, 'A', sizeof( buf ) );
	strcpy_literal( str, "abcdefghijklmnopqrstuvwxyz" );
	MD5OneShot( str, strlen( str ), buf );
	require_action( memcmp( buf, "\xc3\xfc\xd3\xd7\x61\x92\xe4\x00\x7d\xfb\x49\x6c\xca\x67\xe1\x3b", 16 ) == 0, exit, err = kResponseErr );
	for( ptr = buf + 16; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	memset( buf, 'A', sizeof( buf ) );
	strcpy_literal( str, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" );
	MD5OneShot( str, strlen( str ), buf );
	require_action( memcmp( buf, "\xd1\x74\xab\x98\xd2\x77\xd9\xf5\xa5\x61\x1c\x2c\x9f\x41\x9d\x9f", 16 ) == 0, exit, err = kResponseErr );
	for( ptr = buf + 16; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	memset( buf, 'A', sizeof( buf ) );
	strcpy_literal( str, "12345678901234567890123456789012345678901234567890123456789012345678901234567890" );
	MD5OneShot( str, strlen( str ), buf );
	require_action( memcmp( buf, "\x57\xed\xf4\xa2\x2b\xe3\xc9\x55\xac\x49\xda\x2e\x21\x07\xb6\x7a", 16 ) == 0, exit, err = kResponseErr );
	for( ptr = buf + 16; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	// Tests from RFC 1321 using the tweaked MD5 hashing code.
	
	memset( buf, 'A', sizeof( buf ) );
	strcpy_literal( str, "" );
	MD5OneShot_V1( str, strlen( str ), buf );
	require_action( memcmp( buf, "\x7D\x2E\x48\xF5\xDC\x1E\x13\x46\xC8\xD1\x6F\xB6\x27\x3E\x2C\xAA", 16 ) == 0, exit, err = kResponseErr );
	for( ptr = buf + 16; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	memset( buf, 'A', sizeof( buf ) );
	strcpy_literal( str, "a" );
	MD5OneShot_V1( str, strlen( str ), buf );
	require_action( memcmp( buf, "\x28\x4D\xB2\x59\x9F\xB7\x10\xF3\x95\x15\x36\x08\x80\x31\x17\x60", 16 ) == 0, exit, err = kResponseErr );
	for( ptr = buf + 16; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	memset( buf, 'A', sizeof( buf ) );
	strcpy_literal( str, "abc" );
	MD5OneShot_V1( str, strlen( str ), buf );
	require_action( memcmp( buf, "\xB2\x39\x67\x7B\xEA\x70\x0B\x54\x9F\xE3\xA5\x87\x0E\x6C\xEB\xC2", 16 ) == 0, exit, err = kResponseErr );
	for( ptr = buf + 16; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	memset( buf, 'A', sizeof( buf ) );
	strcpy_literal( str, "message digest" );
	MD5OneShot_V1( str, strlen( str ), buf );
	require_action( memcmp( buf, "\x5D\xEC\x84\xB4\xE7\x7D\xAC\x4E\x0B\xC4\xCF\x34\x54\xB0\x0F\x55", 16 ) == 0, exit, err = kResponseErr );
	for( ptr = buf + 16; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	memset( buf, 'A', sizeof( buf ) );
	strcpy_literal( str, "abcdefghijklmnopqrstuvwxyz" );
	MD5OneShot_V1( str, strlen( str ), buf );
	require_action( memcmp( buf, "\x07\xEC\x78\xA9\xEE\x9E\x3E\xC5\x10\x11\xB1\x23\xE6\x9A\x29\x6E", 16 ) == 0, exit, err = kResponseErr );
	for( ptr = buf + 16; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	memset( buf, 'A', sizeof( buf ) );
	strcpy_literal( str, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" );
	MD5OneShot_V1( str, strlen( str ), buf );
	require_action( memcmp( buf, "\xDB\x7C\x53\xC9\x1C\x6F\x68\xA6\xDE\x2F\xD0\xE5\xFA\xE0\x82\x47", 16 ) == 0, exit, err = kResponseErr );
	for( ptr = buf + 16; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	memset( buf, 'A', sizeof( buf ) );
	strcpy_literal( str, "12345678901234567890123456789012345678901234567890123456789012345678901234567890" );
	MD5OneShot_V1( str, strlen( str ), buf );
	require_action( memcmp( buf, "\xE8\x13\x83\x2F\xE7\x2A\x76\xC1\xAE\x8A\x6F\x7A\x34\xFF\x1C\xFC", 16 ) == 0, exit, err = kResponseErr );
	for( ptr = buf + 16; ptr < end; ++ptr ) { require_action( *ptr == 'A', exit, err = kResponseErr ); }
	
	err = kNoErr;
	
exit:
	printf( "MD5UtilsTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
