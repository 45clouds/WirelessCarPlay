/*
	File:    	SHAUtils.c
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	410.12
	
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
	
	Portions Copyright (C) 2012-2014 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if( !defined( _CRT_SECURE_NO_DEPRECATE ) )
	#define _CRT_SECURE_NO_DEPRECATE		1
#endif

#include "SHAUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"

//===========================================================================================================================
//	SHA-1 internals
//
//	Based on code from libtom.
//===========================================================================================================================

#define SHA1_BLOCK_SIZE		64

static void	_SHA1_Compress( SHA_CTX_compat *ctx, const uint8_t *inPtr );

//===========================================================================================================================
//	SHA1_Init_compat
//===========================================================================================================================

int	SHA1_Init_compat( SHA_CTX_compat *ctx )
{
	ctx->length = 0;
	ctx->state[ 0 ] = UINT32_C( 0x67452301 );
	ctx->state[ 1 ] = UINT32_C( 0xefcdab89 );
	ctx->state[ 2 ] = UINT32_C( 0x98badcfe );
	ctx->state[ 3 ] = UINT32_C( 0x10325476 );
	ctx->state[ 4 ] = UINT32_C( 0xc3d2e1f0 );
	ctx->curlen = 0;
	return( 0 );
}

//===========================================================================================================================
//	SHA1_Update_compat
//===========================================================================================================================

int	SHA1_Update_compat( SHA_CTX_compat *ctx, const void *inData, size_t inLen )
{
	const uint8_t *		src = (const uint8_t *) inData;
	size_t				n;
	
	while( inLen > 0 )
	{
		if( ( ctx->curlen == 0 ) && ( inLen >= SHA1_BLOCK_SIZE ) )
		{
			_SHA1_Compress( ctx, src );
			ctx->length += ( SHA1_BLOCK_SIZE * 8 );
			src			+= SHA1_BLOCK_SIZE;
			inLen		-= SHA1_BLOCK_SIZE;
		}
		else
		{
			n = Min( inLen, SHA1_BLOCK_SIZE - ctx->curlen );
			memcpy( ctx->buf + ctx->curlen, src, n );
			ctx->curlen += n;
			src			+= n;
			inLen		-= n;
			if( ctx->curlen == SHA1_BLOCK_SIZE )
			{
				_SHA1_Compress( ctx, ctx->buf );
				ctx->length += ( SHA1_BLOCK_SIZE * 8 );
				ctx->curlen = 0;
			}
		}
	}
	return( 0 );
}

//===========================================================================================================================
//	SHA1_Final_compat
//===========================================================================================================================

int	SHA1_Final_compat( unsigned char *outDigest, SHA_CTX_compat *ctx )
{
	int		i;
	 
	ctx->length += ctx->curlen * 8;
	ctx->buf[ ctx->curlen++ ] = 0x80;
	
	// If length > 56 bytes, append zeros then compress. Then fall back to padding zeros and length encoding like normal.
	if( ctx->curlen > 56 )
	{
		while( ctx->curlen < 64 ) ctx->buf[ ctx->curlen++ ] = 0;
		_SHA1_Compress( ctx, ctx->buf );
		ctx->curlen = 0;
	}

	// Pad up to 56 bytes of zeros.
	while( ctx->curlen < 56 ) ctx->buf[ ctx->curlen++ ] = 0;
	
	// Store length.
	WriteBig64( ctx->buf + 56, ctx->length );
	_SHA1_Compress( ctx, ctx->buf );

	// Copy output.
	for( i = 0; i < 5; ++i )
	{
		WriteBig32( outDigest + ( 4 * i ), ctx->state[ i ] );
	}
	MemZeroSecure( ctx, sizeof( *ctx ) );
	return( 0 );
}

//===========================================================================================================================
//	SHA1_compat
//===========================================================================================================================

unsigned char *	SHA1_compat( const void *inData, size_t inLen, unsigned char *outDigest )
{
	SHA_CTX_compat		ctx;
	
	SHA1_Init_compat( &ctx );
	SHA1_Update_compat( &ctx, inData, inLen );
	SHA1_Final_compat( outDigest, &ctx );
	return( outDigest );
}

//===========================================================================================================================
//	_SHA1_Compress
//===========================================================================================================================

#define SHA1_F0( x, y, z )				(z ^ ( x & ( y ^ z ) ) )
#define SHA1_F1( x, y, z )				(x ^ y ^ z )
#define SHA1_F2( x, y, z )				( ( x & y ) | ( z & ( x | y ) ) )
#define SHA1_F3( x, y, z )				(x ^ y ^ z )
#define SHA1_FF0( a, b, c, d, e, i )	e = ( ROTL32( a, 5 ) + SHA1_F0( b, c, d ) + e + W[ i ] + UINT32_C( 0x5a827999 ) ); b = ROTL32( b, 30);
#define SHA1_FF1( a, b, c, d, e, i )	e = ( ROTL32( a, 5 ) + SHA1_F1( b, c, d ) + e + W[ i ] + UINT32_C( 0x6ed9eba1 ) ); b = ROTL32( b, 30);
#define SHA1_FF2( a, b, c, d, e, i )	e = ( ROTL32( a, 5 ) + SHA1_F2( b, c, d ) + e + W[ i ] + UINT32_C( 0x8f1bbcdc ) ); b = ROTL32( b, 30);
#define SHA1_FF3( a, b, c, d, e, i )	e = ( ROTL32( a, 5 ) + SHA1_F3( b, c, d ) + e + W[ i ] + UINT32_C( 0xca62c1d6 ) ); b = ROTL32( b, 30);

static void	_SHA1_Compress( SHA_CTX_compat *ctx, const uint8_t *inPtr )
{
	uint32_t		a, b, c, d, e, W[ 80 ], i, tmp;
	
	// Copy the state into 512-bits into W[0..15].
	for( i = 0; i < 16; ++i )
	{
		W[ i ] = ReadBig32( inPtr );
		inPtr += 4;
	}
	
	// Copy state
	a = ctx->state[ 0 ];
	b = ctx->state[ 1 ];
	c = ctx->state[ 2 ];
	d = ctx->state[ 3 ];
	e = ctx->state[ 4 ];
	
	// Expand it
	for( i = 16; i < 80; ++i )
	{
		tmp = W[ i-3 ] ^ W[ i-8 ] ^ W[ i-14 ] ^ W[ i-16 ];
		W[ i ] = ROTL32( tmp, 1 ); 
	}
	
	// Compress
	// Round 1
	for( i = 0; i < 20; )
	{
		SHA1_FF0( a, b, c, d, e, i++ );
		SHA1_FF0( e, a, b, c, d, i++ );
		SHA1_FF0( d, e, a, b, c, i++ );
		SHA1_FF0( c, d, e, a, b, i++ );
		SHA1_FF0( b, c, d, e, a, i++ );
	}
	
	// Round 2
	for( ; i < 40; )
	{ 
		SHA1_FF1( a, b, c, d, e, i++ );
		SHA1_FF1( e, a, b, c, d, i++ );
		SHA1_FF1( d, e, a, b, c, i++ );
		SHA1_FF1( c, d, e, a, b, i++ );
		SHA1_FF1( b, c, d, e, a, i++ );
	}
	
	// Round 3
	for( ; i < 60; )
	{ 
		SHA1_FF2( a, b, c, d, e, i++ );
		SHA1_FF2( e, a, b, c, d, i++ );
		SHA1_FF2( d, e, a, b, c, i++ );
		SHA1_FF2( c, d, e, a, b, i++ );
		SHA1_FF2( b, c, d, e, a, i++ );
	}
	
	// Round 4
	for( ; i < 80; )
	{ 
		SHA1_FF3( a, b, c, d, e, i++ );
		SHA1_FF3( e, a, b, c, d, i++ );
		SHA1_FF3( d, e, a, b, c, i++ );
		SHA1_FF3( c, d, e, a, b, i++ );
		SHA1_FF3( b, c, d, e, a, i++ );
	}
	
	// Store
	ctx->state[ 0 ] = ctx->state[ 0 ] + a;
	ctx->state[ 1 ] = ctx->state[ 1 ] + b;
	ctx->state[ 2 ] = ctx->state[ 2 ] + c;
	ctx->state[ 3 ] = ctx->state[ 3 ] + d;
	ctx->state[ 4 ] = ctx->state[ 4 ] + e;
}

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	SHA1_Test
//===========================================================================================================================

// "abc"
#define kSHA1_NISTTestVector1Result		"\xa9\x99\x3e\x36\x47\x06\x81\x6a\xba\x3e\x25\x71\x78\x50\xc2\x6c\x9c\xd0\xd8\x9d"

// "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
#define kSHA1_NISTTestVector2Result		"\x84\x98\x3E\x44\x1C\x3B\xD2\x6E\xBA\xAE\x4A\xA1\xF9\x51\x29\xE5\xE5\x46\x70\xF1"

// 1,000,000 of 'a'
#define kSHA1_NISTTestVector3Result		"\x34\xaa\x97\x3c\xd4\xc4\xda\xa4\xf6\x1e\xeb\x2b\xdb\xad\x27\x31\x65\x34\x01\x6f"

OSStatus	SHA1_Test( void )
{
	OSStatus			err;
	uint8_t				digest[ SHA_DIGEST_LENGTH ];
	unsigned char *		ptr;
	uint8_t *			buf = NULL;
	size_t				len, i;
	SHA_CTX_compat		ctx;
	
	// Test vectors from NIST: <http://www.nsrl.nist.gov/testdata/>.
	
	memset( digest, 0, sizeof( digest ) );
	ptr = SHA1_compat( "abc", 3, digest );
	require_action( ptr == digest, exit, err = -1 );
	require_action( memcmp( digest, kSHA1_NISTTestVector1Result, SHA_DIGEST_LENGTH ) == 0, exit, err = -1 );
	
	memset( digest, 0, sizeof( digest ) );
	ptr = SHA1_compat( "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, digest );
	require_action( ptr == digest, exit, err = -1 );
	require_action( memcmp( digest, kSHA1_NISTTestVector2Result, SHA_DIGEST_LENGTH ) == 0, exit, err = -1 );
	
	memset( digest, 0, sizeof( digest ) );
	len = 1000000;
	buf = (uint8_t *) malloc( len );
	require_action( buf, exit, err = -1 );
	memset( buf, 'a', len );
	ptr = SHA1_compat( buf, len, digest );
	require_action( ptr == digest, exit, err = -1 );
	require_action( memcmp( digest, kSHA1_NISTTestVector3Result, SHA_DIGEST_LENGTH ) == 0, exit, err = -1 );
	
	// Partial update tests.
	
	memset( digest, 0, sizeof( digest ) );
	SHA1_Init_compat( &ctx );
	SHA1_Update_compat( &ctx, &buf[ 0 ], 1 );
	SHA1_Update_compat( &ctx, &buf[ 1 ], 4 );
	SHA1_Update_compat( &ctx, &buf[ 5 ], 100 );
	SHA1_Update_compat( &ctx, &buf[ 105 ], len - 105 );
	SHA1_Final_compat( digest, &ctx );
	require_action( memcmp( digest, kSHA1_NISTTestVector3Result, SHA_DIGEST_LENGTH ) == 0, exit, err = -1 );
	
	memset( digest, 0, sizeof( digest ) );
	SHA1_Init_compat( &ctx );
	for( i = 0; i < len; ++i ) SHA1_Update_compat( &ctx, &buf[ i ], 1 );
	SHA1_Final_compat( digest, &ctx );
	require_action( memcmp( digest, kSHA1_NISTTestVector3Result, SHA_DIGEST_LENGTH ) == 0, exit, err = -1 );
	
	err = kNoErr;
	
exit:
	if( buf ) free( buf );
	printf( "SHA1_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // EXCLUDE_UNIT_TESTS

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	SHA-512 internals
//
//	Based on code from libtom.
//===========================================================================================================================

#define SHA512_BLOCK_SIZE	128

static const uint64_t		K[ 80 ] = 
{
	UINT64_C( 0x428a2f98d728ae22 ), UINT64_C( 0x7137449123ef65cd ), UINT64_C( 0xb5c0fbcfec4d3b2f ), UINT64_C( 0xe9b5dba58189dbbc ),
	UINT64_C( 0x3956c25bf348b538 ), UINT64_C( 0x59f111f1b605d019 ), UINT64_C( 0x923f82a4af194f9b ), UINT64_C( 0xab1c5ed5da6d8118 ),
	UINT64_C( 0xd807aa98a3030242 ), UINT64_C( 0x12835b0145706fbe ), UINT64_C( 0x243185be4ee4b28c ), UINT64_C( 0x550c7dc3d5ffb4e2 ),
	UINT64_C( 0x72be5d74f27b896f ), UINT64_C( 0x80deb1fe3b1696b1 ), UINT64_C( 0x9bdc06a725c71235 ), UINT64_C( 0xc19bf174cf692694 ),
	UINT64_C( 0xe49b69c19ef14ad2 ), UINT64_C( 0xefbe4786384f25e3 ), UINT64_C( 0x0fc19dc68b8cd5b5 ), UINT64_C( 0x240ca1cc77ac9c65 ),
	UINT64_C( 0x2de92c6f592b0275 ), UINT64_C( 0x4a7484aa6ea6e483 ), UINT64_C( 0x5cb0a9dcbd41fbd4 ), UINT64_C( 0x76f988da831153b5 ),
	UINT64_C( 0x983e5152ee66dfab ), UINT64_C( 0xa831c66d2db43210 ), UINT64_C( 0xb00327c898fb213f ), UINT64_C( 0xbf597fc7beef0ee4 ),
	UINT64_C( 0xc6e00bf33da88fc2 ), UINT64_C( 0xd5a79147930aa725 ), UINT64_C( 0x06ca6351e003826f ), UINT64_C( 0x142929670a0e6e70 ),
	UINT64_C( 0x27b70a8546d22ffc ), UINT64_C( 0x2e1b21385c26c926 ), UINT64_C( 0x4d2c6dfc5ac42aed ), UINT64_C( 0x53380d139d95b3df ),
	UINT64_C( 0x650a73548baf63de ), UINT64_C( 0x766a0abb3c77b2a8 ), UINT64_C( 0x81c2c92e47edaee6 ), UINT64_C( 0x92722c851482353b ),
	UINT64_C( 0xa2bfe8a14cf10364 ), UINT64_C( 0xa81a664bbc423001 ), UINT64_C( 0xc24b8b70d0f89791 ), UINT64_C( 0xc76c51a30654be30 ),
	UINT64_C( 0xd192e819d6ef5218 ), UINT64_C( 0xd69906245565a910 ), UINT64_C( 0xf40e35855771202a ), UINT64_C( 0x106aa07032bbd1b8 ),
	UINT64_C( 0x19a4c116b8d2d0c8 ), UINT64_C( 0x1e376c085141ab53 ), UINT64_C( 0x2748774cdf8eeb99 ), UINT64_C( 0x34b0bcb5e19b48a8 ),
	UINT64_C( 0x391c0cb3c5c95a63 ), UINT64_C( 0x4ed8aa4ae3418acb ), UINT64_C( 0x5b9cca4f7763e373 ), UINT64_C( 0x682e6ff3d6b2b8a3 ),
	UINT64_C( 0x748f82ee5defb2fc ), UINT64_C( 0x78a5636f43172f60 ), UINT64_C( 0x84c87814a1f0ab72 ), UINT64_C( 0x8cc702081a6439ec ),
	UINT64_C( 0x90befffa23631e28 ), UINT64_C( 0xa4506cebde82bde9 ), UINT64_C( 0xbef9a3f7b2c67915 ), UINT64_C( 0xc67178f2e372532b ),
	UINT64_C( 0xca273eceea26619c ), UINT64_C( 0xd186b8c721c0c207 ), UINT64_C( 0xeada7dd6cde0eb1e ), UINT64_C( 0xf57d4f7fee6ed178 ),
	UINT64_C( 0x06f067aa72176fba ), UINT64_C( 0x0a637dc5a2c898a6 ), UINT64_C( 0x113f9804bef90dae ), UINT64_C( 0x1b710b35131c471b ),
	UINT64_C( 0x28db77f523047d84 ), UINT64_C( 0x32caab7b40c72493 ), UINT64_C( 0x3c9ebe0a15c9bebc ), UINT64_C( 0x431d67c49c100d4c ),
	UINT64_C( 0x4cc5d4becb3e42b6 ), UINT64_C( 0x597f299cfc657e2a ), UINT64_C( 0x5fcb6fab3ad6faec ), UINT64_C( 0x6c44198c4a475817 )
};

static void	_SHA512_Compress( SHA512_CTX_compat *ctx, const uint8_t *inPtr );

//===========================================================================================================================
//	SHA512_Init_compat
//===========================================================================================================================

int	SHA512_Init_compat( SHA512_CTX_compat *ctx )
{
	ctx->length = 0;
	ctx->state[ 0 ] = UINT64_C( 0x6a09e667f3bcc908 );
	ctx->state[ 1 ] = UINT64_C( 0xbb67ae8584caa73b );
	ctx->state[ 2 ] = UINT64_C( 0x3c6ef372fe94f82b );
	ctx->state[ 3 ] = UINT64_C( 0xa54ff53a5f1d36f1 );
	ctx->state[ 4 ] = UINT64_C( 0x510e527fade682d1 );
	ctx->state[ 5 ] = UINT64_C( 0x9b05688c2b3e6c1f );
	ctx->state[ 6 ] = UINT64_C( 0x1f83d9abfb41bd6b );
	ctx->state[ 7 ] = UINT64_C( 0x5be0cd19137e2179 );
	ctx->curlen = 0;
	return( 0 );
}

//===========================================================================================================================
//	SHA512_Update_compat
//===========================================================================================================================

int	SHA512_Update_compat( SHA512_CTX_compat *ctx, const void *inData, size_t inLen )
{
	const uint8_t *		src = (const uint8_t *) inData;
	size_t				n;
	
	while( inLen > 0 )
	{
		if( ( ctx->curlen == 0 ) && ( inLen >= SHA512_BLOCK_SIZE ) )
		{
			_SHA512_Compress( ctx, src );
			ctx->length += ( SHA512_BLOCK_SIZE * 8 );
			src			+= SHA512_BLOCK_SIZE;
			inLen		-= SHA512_BLOCK_SIZE;
		}
		else
		{
			n = Min( inLen, SHA512_BLOCK_SIZE - ctx->curlen );
			memcpy( ctx->buf + ctx->curlen, src, n );
			ctx->curlen += n;
			src			+= n;
			inLen		-= n;
			if( ctx->curlen == SHA512_BLOCK_SIZE )
			{
				_SHA512_Compress( ctx, ctx->buf );
				ctx->length += ( SHA512_BLOCK_SIZE * 8 );
				ctx->curlen = 0;
			}
		}
	}
	return( 0 );
}

//===========================================================================================================================
//	SHA512_Final_compat
//===========================================================================================================================

int	SHA512_Final_compat( unsigned char *outDigest, SHA512_CTX_compat *ctx )
{
	int		i;
	
	ctx->length += ( ctx->curlen * UINT64_C( 8 ) );
	ctx->buf[ ctx->curlen++ ] = 0x80;
	
	// If length > 112 bytes, append zeros then compress. Then fall back to padding zeros and length encoding like normal.
	if( ctx->curlen > 112 )
	{
		while( ctx->curlen < 128 ) ctx->buf[ ctx->curlen++ ] = 0;
		_SHA512_Compress( ctx, ctx->buf );
		ctx->curlen = 0;
	}
	
	// Pad up to 120 bytes of zeroes.
	// Note: that from 112 to 120 is the 64 MSB of the length. We assume that you won't hash 2^64 bits of data.
	while( ctx->curlen < 120 ) ctx->buf[ ctx->curlen++ ] = 0;
	
	// Store length
	WriteBig64( ctx->buf + 120, ctx->length );
	_SHA512_Compress( ctx, ctx->buf );
	
	// Copy output
	for( i = 0; i < 8; ++i )
	{
		WriteBig64( outDigest + ( 8 * i ), ctx->state[ i ] );
	}
	MemZeroSecure( ctx, sizeof( *ctx ) );
	return( 0 );
}

//===========================================================================================================================
//	SHA512_compat
//===========================================================================================================================

unsigned char *	SHA512_compat( const void *inData, size_t inLen, unsigned char *outDigest )
{
	SHA512_CTX_compat		ctx;
	
	SHA512_Init_compat( &ctx );
	SHA512_Update_compat( &ctx, inData, inLen );
	SHA512_Final_compat( outDigest, &ctx );
	return( outDigest );
}

//===========================================================================================================================
//	_SHA512_Compress
//===========================================================================================================================

#define SHA512_Ch(x,y,z)		(z ^ (x & (y ^ z)))
#define SHA512_Maj(x,y,z)		(((x | y) & z) | (x & y)) 
#define SHA512_S(x, n)			ROTR64(x, n)
#define SHA512_R(x, n)			(((x) & UINT64_C(0xFFFFFFFFFFFFFFFF)) >> ((uint64_t) n))
#define SHA512_Sigma0(x)		(SHA512_S(x, 28) ^ SHA512_S(x, 34) ^ SHA512_S(x, 39))
#define SHA512_Sigma1(x)		(SHA512_S(x, 14) ^ SHA512_S(x, 18) ^ SHA512_S(x, 41))
#define SHA512_Gamma0(x)		(SHA512_S(x,  1) ^ SHA512_S(x,  8) ^ SHA512_R(x,  7))
#define SHA512_Gamma1(x)		(SHA512_S(x, 19) ^ SHA512_S(x, 61) ^ SHA512_R(x,  6))
#define SHA512_RND( a, b, c, d, e, f, g, h, i ) \
	 t0 = h + SHA512_Sigma1( e ) + SHA512_Ch( e, f, g ) + K[ i ] + W[ i ]; \
	 t1 = SHA512_Sigma0( a ) + SHA512_Maj( a, b, c); \
	 d += t0; \
	 h  = t0 + t1;

static void	_SHA512_Compress( SHA512_CTX_compat *ctx, const uint8_t *inPtr )
{
	uint64_t		S[ 8] , W[ 80 ], t0, t1;
	int				i;
	
	// Copy state into S
	for( i = 0; i < 8; ++i )
	{
		S[ i ] = ctx->state[ i ];
	}
	
	// Copy the state into 1024-bits into W[0..15]
	for( i = 0; i < 16; ++i )
	{
		W[ i ] = ReadBig64( inPtr );
		inPtr += 8;
	}
	
	// Fill W[16..79]
	for( i = 16; i < 80; ++i )
	{
		W[ i ] = SHA512_Gamma1( W[ i-2 ] ) + W[ i-7 ] + SHA512_Gamma0( W[ i-15 ] )  + W[ i-16 ];
	}		
	
	// Compress
	for( i = 0; i < 80; i += 8 )
	{
		 SHA512_RND( S[ 0 ], S[ 1 ], S[ 2 ], S[ 3 ], S[ 4 ], S[ 5 ], S[ 6 ], S[ 7 ], i+0 );
		 SHA512_RND( S[ 7 ], S[ 0 ], S[ 1 ], S[ 2 ], S[ 3 ], S[ 4 ], S[ 5 ], S[ 6 ], i+1 );
		 SHA512_RND( S[ 6 ], S[ 7 ], S[ 0 ], S[ 1 ], S[ 2 ], S[ 3 ], S[ 4 ], S[ 5 ], i+2 );
		 SHA512_RND( S[ 5 ], S[ 6 ], S[ 7 ], S[ 0 ], S[ 1 ], S[ 2 ], S[ 3 ], S[ 4 ], i+3 );
		 SHA512_RND( S[ 4 ], S[ 5 ], S[ 6 ], S[ 7 ], S[ 0 ], S[ 1 ], S[ 2 ], S[ 3 ], i+4 );
		 SHA512_RND( S[ 3 ], S[ 4 ], S[ 5 ], S[ 6 ], S[ 7 ], S[ 0 ], S[ 1 ], S[ 2 ], i+5 );
		 SHA512_RND( S[ 2 ], S[ 3 ], S[ 4 ], S[ 5 ], S[ 6 ], S[ 7 ], S[ 0 ], S[ 1 ], i+6 );
		 SHA512_RND( S[ 1 ], S[ 2 ], S[ 3 ], S[ 4 ], S[ 5 ], S[ 6 ], S[ 7 ], S[ 0 ], i+7 );
	}
	
	// Feedback
	for( i = 0; i < 8; ++i )
	{
		ctx->state[ i ] += S[ i ];
	}
}

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	SHA512_Test
//===========================================================================================================================

// "abc"
#define kSHA512_LibTomTestVector1Result \
	"\xdd\xaf\x35\xa1\x93\x61\x7a\xba\xcc\x41\x73\x49\xae\x20\x41\x31" \
	"\x12\xe6\xfa\x4e\x89\xa9\x7e\xa2\x0a\x9e\xee\xe6\x4b\x55\xd3\x9a" \
	"\x21\x92\x99\x2a\x27\x4f\xc1\xa8\x36\xba\x3c\x23\xa3\xfe\xeb\xbd" \
	"\x45\x4d\x44\x23\x64\x3c\xe8\x0e\x2a\x9a\xc9\x4f\xa5\x4c\xa4\x9f"

// "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"
#define kSHA512_LibTomTestVector2Result \
	"\x8e\x95\x9b\x75\xda\xe3\x13\xda\x8c\xf4\xf7\x28\x14\xfc\x14\x3f" \
	"\x8f\x77\x79\xc6\xeb\x9f\x7f\xa1\x72\x99\xae\xad\xb6\x88\x90\x18" \
	"\x50\x1d\x28\x9e\x49\x00\xf7\xe4\x33\x1b\x99\xde\xc4\xb5\x43\x3a" \
	"\xc7\xd3\x29\xee\xb6\xdd\x26\x54\x5e\x96\xe5\x5b\x87\x4b\xe9\x09"

// Hash of "The quick brown fox jumps over the lazy dog"
#define kSHA512_WikipediaTestVector3Result \
	"\x91\xEA\x12\x45\xF2\x0D\x46\xAE\x9A\x03\x7A\x98\x9F\x54\xF1\xF7" \
	"\x90\xF0\xA4\x76\x07\xEE\xB8\xA1\x4D\x12\x89\x0C\xEA\x77\xA1\xBB" \
	"\xC6\xC7\xED\x9C\xF2\x05\xE6\x7B\x7F\x2B\x8F\xD4\xC7\xDF\xD3\xA7" \
	"\xA8\x61\x7E\x45\xF3\xC4\x63\xD4\x81\xC7\xE5\x86\xC3\x9A\xC1\xED"

// 1,000,000 of 'a'
#define kSHA512_TestVector4Result \
	"\xE7\x18\x48\x3D\x0C\xE7\x69\x64\x4E\x2E\x42\xC7\xBC\x15\xB4\x63" \
	"\x8E\x1F\x98\xB1\x3B\x20\x44\x28\x56\x32\xA8\x03\xAF\xA9\x73\xEB" \
	"\xDE\x0F\xF2\x44\x87\x7E\xA6\x0A\x4C\xB0\x43\x2C\xE5\x77\xC3\x1B" \
	"\xEB\x00\x9C\x5C\x2C\x49\xAA\x2E\x4E\xAD\xB2\x17\xAD\x8C\xC0\x9B"

OSStatus	SHA512_Test( void )
{
	OSStatus				err;
	uint8_t					digest[ SHA512_DIGEST_LENGTH ];
	unsigned char *			ptr;
	uint8_t *				buf = NULL;
	size_t					len, i;
	SHA512_CTX_compat		ctx;
	
	// Test vectors from libtom.
	
	memset( digest, 0, sizeof( digest ) );
	ptr = SHA512_compat( "abc", 3, digest );
	require_action( ptr == digest, exit, err = -1 );
	require_action( memcmp( digest, kSHA512_LibTomTestVector1Result, SHA512_DIGEST_LENGTH ) == 0, exit, err = -1 );
	
	memset( digest, 0, sizeof( digest ) );
	ptr = SHA512_compat( "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", 112, digest );
	require_action( ptr == digest, exit, err = -1 );
	require_action( memcmp( digest, kSHA512_LibTomTestVector2Result, SHA512_DIGEST_LENGTH ) == 0, exit, err = -1 );
	
	// Test vector from NIST.
	
	memset( digest, 0, sizeof( digest ) );
	ptr = SHA512_compat( "The quick brown fox jumps over the lazy dog.", 44, digest );
	require_action( ptr == digest, exit, err = -1 );
	require_action( memcmp( digest, kSHA512_WikipediaTestVector3Result, SHA512_DIGEST_LENGTH ) == 0, exit, err = -1 );
	
	// Test vector against CommonCrypto.
	
	memset( digest, 0, sizeof( digest ) );
	len = 1000000;
	buf = (uint8_t *) malloc( len );
	require_action( buf, exit, err = -1 );
	memset( buf, 'a', len );
	ptr = SHA512_compat( buf, len, digest );
	require_action( ptr == digest, exit, err = -1 );
	require_action( memcmp( digest, kSHA512_TestVector4Result, SHA512_DIGEST_LENGTH ) == 0, exit, err = -1 );
	
	// Partial update tests.
	
	memset( digest, 0, sizeof( digest ) );
	SHA512_Init_compat( &ctx );
	SHA512_Update_compat( &ctx, &buf[ 0 ], 1 );
	SHA512_Update_compat( &ctx, &buf[ 1 ], 4 );
	SHA512_Update_compat( &ctx, &buf[ 5 ], 100 );
	SHA512_Update_compat( &ctx, &buf[ 105 ], len - 105 );
	SHA512_Final_compat( digest, &ctx );
	require_action( memcmp( digest, kSHA512_TestVector4Result, SHA512_DIGEST_LENGTH ) == 0, exit, err = -1 );
	
	memset( digest, 0, sizeof( digest ) );
	SHA512_Init_compat( &ctx );
	for( i = 0; i < len; ++i ) SHA512_Update_compat( &ctx, &buf[ i ], 1 );
	SHA512_Final_compat( digest, &ctx );
	require_action( memcmp( digest, kSHA512_TestVector4Result, SHA512_DIGEST_LENGTH ) == 0, exit, err = -1 );
	
	err = kNoErr;
	
exit:
	if( buf ) free( buf );
	printf( "SHA512_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // EXCLUDE_UNIT_TESTS

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	HMAC_SHA1_Init
//===========================================================================================================================

void	HMAC_SHA1_Init( HMAC_SHA1_CTX *ctx, const void *inKeyPtr, size_t inKeyLen )
{
	const uint8_t *		keyPtr = (const uint8_t *) inKeyPtr;
	uint8_t				ipad[ SHA1_BLOCK_SIZE ];
	uint8_t				tempKey[ SHA_DIGEST_LENGTH ];
	size_t				i;
	uint8_t				b;
	
	if( inKeyLen > SHA1_BLOCK_SIZE )
	{
		SHA1_Init( &ctx->shaCtx );
		SHA1_Update( &ctx->shaCtx, inKeyPtr, inKeyLen );
		SHA1_Final( tempKey, &ctx->shaCtx );
		keyPtr   = tempKey;
		inKeyLen = sizeof( tempKey );
	}
	for( i = 0; i < inKeyLen; ++i )
	{
		b = keyPtr[ i ];
		ipad[ i ]		= b ^ 0x36;
		ctx->opad[ i ]	= b ^ 0x5C;
	}
	for( ; i < SHA1_BLOCK_SIZE; ++i )
	{
		ipad[ i ]		= 0x36;
		ctx->opad[ i ]	= 0x5C;
	}
	SHA1_Init( &ctx->shaCtx );
	SHA1_Update( &ctx->shaCtx, ipad, sizeof( ipad ) );
}

//===========================================================================================================================
//	HMAC_SHA1_Update
//===========================================================================================================================

void	HMAC_SHA1_Update( HMAC_SHA1_CTX *ctx, const void *inPtr, size_t inLen )
{
	SHA1_Update( &ctx->shaCtx, inPtr, inLen );
}

//===========================================================================================================================
//	HMAC_SHA1_Final
//===========================================================================================================================

void	HMAC_SHA1_Final( HMAC_SHA1_CTX *ctx, uint8_t *outDigest )
{
	SHA1_Final( outDigest, &ctx->shaCtx );
	SHA1_Init( &ctx->shaCtx );
	SHA1_Update( &ctx->shaCtx, ctx->opad, sizeof( ctx->opad ) );
	SHA1_Update( &ctx->shaCtx, outDigest, SHA_DIGEST_LENGTH );
	SHA1_Final( outDigest, &ctx->shaCtx );
}

//===========================================================================================================================
//	HMAC_SHA1
//===========================================================================================================================

void	HMAC_SHA1( const void *inKeyPtr, size_t inKeyLen, const void *inMsgPtr, size_t inMsgLen, uint8_t *outDigest )
{
	HMAC_SHA1_CTX		ctx;
	
	HMAC_SHA1_Init( &ctx, inKeyPtr, inKeyLen );
	HMAC_SHA1_Update( &ctx, inMsgPtr, inMsgLen );
	HMAC_SHA1_Final( &ctx, outDigest );
}

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	HMAC_SHA1_Test
//===========================================================================================================================

typedef struct
{
	const void *		keyPtr;
	size_t				keyLen;
	const void *		dataPtr;
	size_t				dataLen;
	const void *		digestPtr;
	size_t				digestLen;
	
}	HMAC_SHA1_TestCase;

static const HMAC_SHA1_TestCase		kHMAC_SHA1_TestCases[] = 
{
	// Test vectors from <http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/HMAC_SHA1.pdf>.
	
	// Test Case 1
	{
		/* key */		"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
						"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
						"\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F"
						"\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F", 
						64, 
		/* data */		"Sample message for keylen=blocklen", 
						34, 
		/* digest */	"\x5F\xD5\x96\xEE\x78\xD5\x55\x3C\x8F\xF4\xE7\x2D\x26\x6D\xFD\x19\x23\x66\xDA\x29", 
						20
	}, 
	// Test Case 2
	{
		/* key */		"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13", 
						20, 
		/* data */		"Sample message for keylen<blocklen", 
						34, 
		/* digest */	"\x4C\x99\xFF\x0C\xB1\xB3\x1B\xD3\x3F\x84\x31\xDB\xAF\x4D\x17\xFC\xD3\x56\xA8\x07", 
						20
	}, 
	// Test Case 3
	{
		/* key */		"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
						"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
						"\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F"
						"\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F"
						"\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4A\x4B\x4C\x4D\x4E\x4F"
						"\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5A\x5B\x5C\x5D\x5E\x5F"
						"\x60\x61\x62\x63", 
						100, 
		/* data */		"Sample message for keylen=blocklen", 
						34, 
		/* digest */	"\x2D\x51\xB2\xF7\x75\x0E\x41\x05\x84\x66\x2E\x38\xF1\x33\x43\x5F\x4C\x4F\xD4\x2A", 
						20
	}, 
	// Test Case 4
	{
		/* key */		"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
						"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
						"\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F"
						"\x30", 
						49, 
		/* data */		"Sample message for keylen<blocklen, with truncated tag", 
						54, 
		/* digest */	"\xFE\x35\x29\x56\x5C\xD8\xE2\x8C\x5F\xA7\x9E\xAC", 
						12
	}, 
};


OSStatus	HMAC_SHA1_Test( void )
{
	OSStatus						err;
	const HMAC_SHA1_TestCase *		tc;
	size_t							i;
	uint8_t							digest[ SHA_DIGEST_LENGTH ];
	
	for( i = 0; i < countof( kHMAC_SHA1_TestCases ); ++i )
	{
		tc = &kHMAC_SHA1_TestCases[ i ];
		memset( digest, 0, sizeof( digest ) );
		HMAC_SHA1( tc->keyPtr, tc->keyLen, tc->dataPtr, tc->dataLen, digest );
		require_action( memcmp( digest, tc->digestPtr, tc->digestLen ) == 0, exit, err = kResponseErr );
	}
	err = kNoErr;
	
exit:
	printf( "HMAC_SHA1_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	HMAC_SHA512_Init
//===========================================================================================================================

void	HMAC_SHA512_Init( HMAC_SHA512_CTX *ctx, const void *inKeyPtr, size_t inKeyLen )
{
	const uint8_t *		keyPtr = (const uint8_t *) inKeyPtr;
	uint8_t				ipad[ SHA512_BLOCK_SIZE ];
	uint8_t				tempKey[ SHA512_DIGEST_LENGTH ];
	size_t				i;
	uint8_t				b;
	
	if( inKeyLen > SHA512_BLOCK_SIZE )
	{
		SHA512_Init( &ctx->shaCtx );
		SHA512_Update( &ctx->shaCtx, inKeyPtr, inKeyLen );
		SHA512_Final( tempKey, &ctx->shaCtx );
		keyPtr   = tempKey;
		inKeyLen = sizeof( tempKey );
	}
	for( i = 0; i < inKeyLen; ++i )
	{
		b = keyPtr[ i ];
		ipad[ i ]		= b ^ 0x36;
		ctx->opad[ i ]	= b ^ 0x5C;
	}
	for( ; i < SHA512_BLOCK_SIZE; ++i )
	{
		ipad[ i ]		= 0x36;
		ctx->opad[ i ]	= 0x5C;
	}
	SHA512_Init( &ctx->shaCtx );
	SHA512_Update( &ctx->shaCtx, ipad, sizeof( ipad ) );
}

//===========================================================================================================================
//	HMAC_SHA512_Update
//===========================================================================================================================

void	HMAC_SHA512_Update( HMAC_SHA512_CTX *ctx, const void *inPtr, size_t inLen )
{
	SHA512_Update( &ctx->shaCtx, inPtr, inLen );
}

//===========================================================================================================================
//	HMAC_SHA512_Final
//===========================================================================================================================

void	HMAC_SHA512_Final( HMAC_SHA512_CTX *ctx, uint8_t *outDigest )
{
	SHA512_Final( outDigest, &ctx->shaCtx );
	SHA512_Init( &ctx->shaCtx );
	SHA512_Update( &ctx->shaCtx, ctx->opad, sizeof( ctx->opad ) );
	SHA512_Update( &ctx->shaCtx, outDigest, SHA512_DIGEST_LENGTH );
	SHA512_Final( outDigest, &ctx->shaCtx );
}

//===========================================================================================================================
//	HMAC_SHA512
//===========================================================================================================================

void	HMAC_SHA512( const void *inKeyPtr, size_t inKeyLen, const void *inMsgPtr, size_t inMsgLen, uint8_t *outDigest )
{
	HMAC_SHA512_CTX		ctx;
	
	HMAC_SHA512_Init( &ctx, inKeyPtr, inKeyLen );
	HMAC_SHA512_Update( &ctx, inMsgPtr, inMsgLen );
	HMAC_SHA512_Final( &ctx, outDigest );
}

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	HMAC_SHA512_Test
//===========================================================================================================================

typedef struct
{
	const void *		keyPtr;
	size_t				keyLen;
	const void *		dataPtr;
	size_t				dataLen;
	const void *		digestPtr;
	size_t				digestLen;
	
}	HMAC_SHA512_TestCase;

static const HMAC_SHA512_TestCase		kHMAC_SHA512_TestCases[] = 
{
	// Test vectors from RFC 4231.
	
	// Test Case 1
	{
		/* key */		"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b", 
						20, 
		/* data */		"\x48\x69\x20\x54\x68\x65\x72\x65", 
						8, 
		/* digest */	"\x87\xAA\x7C\xDE\xA5\xEF\x61\x9D\x4F\xF0\xB4\x24\x1A\x1D\x6C\xB0"
						"\x23\x79\xF4\xE2\xCE\x4E\xC2\x78\x7A\xD0\xB3\x05\x45\xE1\x7C\xDE"
						"\xDA\xA8\x33\xB7\xD6\xB8\xA7\x02\x03\x8B\x27\x4E\xAE\xA3\xF4\xE4"
						"\xBE\x9D\x91\x4E\xEB\x61\xF1\x70\x2E\x69\x6C\x20\x3A\x12\x68\x54", 
						64
	}, 
	// Test Case 2
	{
		/* key */		"\x4A\x65\x66\x65", 
						4, 
		/* data */		"\x77\x68\x61\x74\x20\x64\x6F\x20\x79\x61\x20\x77\x61\x6E\x74\x20"
						"\x66\x6F\x72\x20\x6E\x6F\x74\x68\x69\x6E\x67\x3F", 
						28, 
		/* digest */	"\x16\x4B\x7A\x7B\xFC\xF8\x19\xE2\xE3\x95\xFB\xE7\x3B\x56\xE0\xA3"
						"\x87\xBD\x64\x22\x2E\x83\x1F\xD6\x10\x27\x0C\xD7\xEA\x25\x05\x54"
						"\x97\x58\xBF\x75\xC0\x5A\x99\x4A\x6D\x03\x4F\x65\xF8\xF0\xE6\xFD"
						"\xCA\xEA\xB1\xA3\x4D\x4A\x6B\x4B\x63\x6E\x07\x0A\x38\xBC\xE7\x37", 
						64
	}, 
	// Test Case 3
	{
		/* key */		"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA", 
						20, 
		/* data */		"\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
						"\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
						"\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
						"\xDD\xDD", 
						50, 
		/* digest */	"\xFA\x73\xB0\x08\x9D\x56\xA2\x84\xEF\xB0\xF0\x75\x6C\x89\x0B\xE9"
						"\xB1\xB5\xDB\xDD\x8E\xE8\x1A\x36\x55\xF8\x3E\x33\xB2\x27\x9D\x39"
						"\xBF\x3E\x84\x82\x79\xA7\x22\xC8\x06\xB4\x85\xA4\x7E\x67\xC8\x07"
						"\xB9\x46\xA3\x37\xBE\xE8\x94\x26\x74\x27\x88\x59\xE1\x32\x92\xFB", 
						64
	}, 
	// Test Case 4
	{
		/* key */		"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
						"\x11\x12\x13\x14\x15\x16\x17\x18\x19", 
						25, 
		/* data */		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
						"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
						"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
						"\xCD\xCD", 
						50, 
		/* digest */	"\xB0\xBA\x46\x56\x37\x45\x8C\x69\x90\xE5\xA8\xC5\xF6\x1D\x4A\xF7"
						"\xE5\x76\xD9\x7F\xF9\x4B\x87\x2D\xE7\x6F\x80\x50\x36\x1E\xE3\xDB"
						"\xA9\x1C\xA5\xC1\x1A\xA2\x5E\xB4\xD6\x79\x27\x5C\xC5\x78\x80\x63"
						"\xA5\xF1\x97\x41\x12\x0C\x4F\x2D\xE2\xAD\xEB\xEB\x10\xA2\x98\xDD", 
						64
	}, 
	// Test Case 5
	{
		/* key */		"\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C", 
						20, 
		/* data */		"\x54\x65\x73\x74\x20\x57\x69\x74\x68\x20\x54\x72\x75\x6E\x63\x61\x74\x69\x6F\x6E", 
						20, 
		/* digest */	"\x41\x5F\xAD\x62\x71\x58\x0A\x53\x1D\x41\x79\xBC\x89\x1D\x87\xA6", 
						16
	}, 
	// Test Case 6
	{
		/* key */		"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA", 
						131, 
		/* data */		"\x54\x65\x73\x74\x20\x55\x73\x69\x6E\x67\x20\x4C\x61\x72\x67\x65"
						"\x72\x20\x54\x68\x61\x6E\x20\x42\x6C\x6F\x63\x6B\x2D\x53\x69\x7A"
						"\x65\x20\x4B\x65\x79\x20\x2D\x20\x48\x61\x73\x68\x20\x4B\x65\x79"
						"\x20\x46\x69\x72\x73\x74", 
						54, 
		/* digest */	"\x80\xB2\x42\x63\xC7\xC1\xA3\xEB\xB7\x14\x93\xC1\xDD\x7B\xE8\xB4"
						"\x9B\x46\xD1\xF4\x1B\x4A\xEE\xC1\x12\x1B\x01\x37\x83\xF8\xF3\x52"
						"\x6B\x56\xD0\x37\xE0\x5F\x25\x98\xBD\x0F\xD2\x21\x5D\x6A\x1E\x52"
						"\x95\xE6\x4F\x73\xF6\x3F\x0A\xEC\x8B\x91\x5A\x98\x5D\x78\x65\x98", 
						64
	}, 
	// Test Case 7
	{
		/* key */		"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA", 
						131, 
		/* data */		"\x54\x68\x69\x73\x20\x69\x73\x20\x61\x20\x74\x65\x73\x74\x20\x75"
						"\x73\x69\x6E\x67\x20\x61\x20\x6C\x61\x72\x67\x65\x72\x20\x74\x68"
						"\x61\x6E\x20\x62\x6C\x6F\x63\x6B\x2D\x73\x69\x7A\x65\x20\x6B\x65"
						"\x79\x20\x61\x6E\x64\x20\x61\x20\x6C\x61\x72\x67\x65\x72\x20\x74"
						"\x68\x61\x6E\x20\x62\x6C\x6F\x63\x6B\x2D\x73\x69\x7A\x65\x20\x64"
						"\x61\x74\x61\x2E\x20\x54\x68\x65\x20\x6B\x65\x79\x20\x6E\x65\x65"
						"\x64\x73\x20\x74\x6F\x20\x62\x65\x20\x68\x61\x73\x68\x65\x64\x20"
						"\x62\x65\x66\x6F\x72\x65\x20\x62\x65\x69\x6E\x67\x20\x75\x73\x65"
						"\x64\x20\x62\x79\x20\x74\x68\x65\x20\x48\x4D\x41\x43\x20\x61\x6C"
						"\x67\x6F\x72\x69\x74\x68\x6D\x2E", 
						152, 
		/* digest */	"\xE3\x7B\x6A\x77\x5D\xC8\x7D\xBA\xA4\xDF\xA9\xF9\x6E\x5E\x3F\xFD"
						"\xDE\xBD\x71\xF8\x86\x72\x89\x86\x5D\xF5\xA3\x2D\x20\xCD\xC9\x44"
						"\xB6\x02\x2C\xAC\x3C\x49\x82\xB1\x0D\x5E\xEB\x55\xC3\xE4\xDE\x15"
						"\x13\x46\x76\xFB\x6D\xE0\x44\x60\x65\xC9\x74\x40\xFA\x8C\x6A\x58", 
						64
	}, 
};

OSStatus	HMAC_SHA512_Test( void )
{
	OSStatus							err;
	const HMAC_SHA512_TestCase *		tc;
	size_t								i;
	uint8_t								digest[ SHA512_DIGEST_LENGTH ];
	
	for( i = 0; i < countof( kHMAC_SHA512_TestCases ); ++i )
	{
		tc = &kHMAC_SHA512_TestCases[ i ];
		memset( digest, 0, sizeof( digest ) );
		HMAC_SHA512( tc->keyPtr, tc->keyLen, tc->dataPtr, tc->dataLen, digest );
		require_action( memcmp( digest, tc->digestPtr, tc->digestLen ) == 0, exit, err = kResponseErr );
	}
	err = kNoErr;
	
exit:
	printf( "HMAC_SHA512_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	HKDF_SHA512_compat
//===========================================================================================================================

void
	HKDF_SHA512_compat( 
		const void *	inInputKeyPtr,	size_t inInputKeyLen, 
		const void *	inSaltPtr,		size_t inSaltLen, 
		const void *	inInfoPtr,		size_t inInfoLen, 
		size_t			inOutputLen, 	uint8_t *outKey )
{
	uint8_t				nullSalt[ SHA512_DIGEST_LENGTH ];
	uint8_t				key[ SHA512_DIGEST_LENGTH ];
	HMAC_SHA512_CTX		hmacCtx;
	size_t				i, n, offset, Tlen;
	uint8_t				T[ SHA512_DIGEST_LENGTH ];
	uint8_t				b;
	
	// Extract phase.
	
	if( inSaltLen == 0 )
	{
		memset( nullSalt, 0, sizeof( nullSalt ) );
		inSaltPtr = nullSalt;
		inSaltLen = sizeof( nullSalt );
	}
	HMAC_SHA512( inSaltPtr, inSaltLen, inInputKeyPtr, inInputKeyLen, key );
	
	// Expand phase.
	
	n = ( inOutputLen / SHA512_DIGEST_LENGTH ) + ( ( inOutputLen % SHA512_DIGEST_LENGTH ) ? 1 : 0 );
	check( n <= 255 );
	Tlen = 0;
	offset = 0;
	for( i = 1; i <= n; ++i )
	{
		HMAC_SHA512_Init( &hmacCtx, key, SHA512_DIGEST_LENGTH );
		HMAC_SHA512_Update( &hmacCtx, T, Tlen );
		HMAC_SHA512_Update( &hmacCtx, inInfoPtr, inInfoLen );
		b = (uint8_t) i;
		HMAC_SHA512_Update( &hmacCtx, &b, 1 );
		HMAC_SHA512_Final( &hmacCtx, T );
		memcpy( &outKey[ offset ], T, ( i != n ) ? sizeof( T ) : ( inOutputLen - offset ) );
		offset += sizeof( T );
		Tlen = sizeof( T );
	}
}

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	HKDF_SHA512_Test
//===========================================================================================================================

typedef struct
{
	const void *		ikmPtr;
	size_t				ikmLen;
	const void *		saltPtr;
	size_t				saltLen;
	const void *		infoPtr;
	size_t				infoLen;
	const void *		keyPtr;
	size_t				keyLen;
	
}	HKDF_SHA512_TestCase;

static const HKDF_SHA512_TestCase		kHKDF_SHA512_TestCases[] = 
{
	// Input test vectors from RFC 5869, but updated for SHA-512.
	
	// Test Case 1
	{
		// IKM
		"\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B", 
		22, 
		// Salt
		"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C", 
		13, 
		// Info
		"\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9", 
		10, 
		// Key
		"\x83\x23\x90\x08\x6C\xDA\x71\xFB\x47\x62\x5B\xB5\xCE\xB1\x68\xE4"
		"\xC8\xE2\x6A\x1A\x16\xED\x34\xD9\xFC\x7F\xE9\x2C\x14\x81\x57\x93"
		"\x38\xDA\x36\x2C\xB8\xD9\xF9\x25\xD7\xCB", 
		42
	}, 
	// Test Case 2
	{
		// IKM
		"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
		"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
		"\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F"
		"\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F"
		"\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4A\x4B\x4C\x4D\x4E\x4F", 
		80, 
		// Salt
		"\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6A\x6B\x6C\x6D\x6E\x6F"
		"\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7A\x7B\x7C\x7D\x7E\x7F"
		"\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8A\x8B\x8C\x8D\x8E\x8F"
		"\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F"
		"\xA0\xA1\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xAA\xAB\xAC\xAD\xAE\xAF", 
		80, 
		// Info
		"\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF"
		"\xC0\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xCA\xCB\xCC\xCD\xCE\xCF"
		"\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD7\xD8\xD9\xDA\xDB\xDC\xDD\xDE\xDF"
		"\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xEA\xEB\xEC\xED\xEE\xEF"
		"\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF", 
		80, 
		// Key
		"\xCE\x6C\x97\x19\x28\x05\xB3\x46\xE6\x16\x1E\x82\x1E\xD1\x65\x67"
		"\x3B\x84\xF4\x00\xA2\xB5\x14\xB2\xFE\x23\xD8\x4C\xD1\x89\xDD\xF1"
		"\xB6\x95\xB4\x8C\xBD\x1C\x83\x88\x44\x11\x37\xB3\xCE\x28\xF1\x6A"
		"\xA6\x4B\xA3\x3B\xA4\x66\xB2\x4D\xF6\xCF\xCB\x02\x1E\xCF\xF2\x35"
		"\xF6\xA2\x05\x6C\xE3\xAF\x1D\xE4\x4D\x57\x20\x97\xA8\x50\x5D\x9E"
		"\x7A\x93", 
		82
	}, 
	// Test Case 3
	{
		// IKM
		"\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B", 
		22, 
		// Salt
		"", 
		0, 
		// Info
		"", 
		0, 
		// Key
		"\xF5\xFA\x02\xB1\x82\x98\xA7\x2A\x8C\x23\x89\x8A\x87\x03\x47\x2C"
		"\x6E\xB1\x79\xDC\x20\x4C\x03\x42\x5C\x97\x0E\x3B\x16\x4B\xF9\x0F"
		"\xFF\x22\xD0\x48\x36\xD0\xE2\x34\x3B\xAC", 
		42
	}
};

OSStatus	HKDF_SHA512_Test( void )
{
	OSStatus							err;
	const HKDF_SHA512_TestCase *		tc;
	size_t								i;
	uint8_t								key[ 128 ];
	
	for( i = 0; i < countof( kHKDF_SHA512_TestCases ); ++i )
	{
		tc = &kHKDF_SHA512_TestCases[ i ];
		memset( key, 0, sizeof( key ) );
		HKDF_SHA512_compat( tc->ikmPtr, tc->ikmLen, tc->saltPtr, tc->saltLen, tc->infoPtr, tc->infoLen, tc->keyLen, key );
		require_action( memcmp( key, tc->keyPtr, tc->keyLen ) == 0, exit, err = kResponseErr );
	}
	err = kNoErr;
	
exit:
	printf( "HKDF_SHA512_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
