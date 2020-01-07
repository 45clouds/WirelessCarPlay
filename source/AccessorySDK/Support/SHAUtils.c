/*
	File:    	SHAUtils.c
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
	
	Portions Copyright (C) 2012-2014 Apple Inc. All Rights Reserved.
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
//	SHA-3 internals
//
//	Based on code from <https://github.com/floodyberry>.
//===========================================================================================================================

static const uint64_t		kSHA3RoundConstants[ 24 ] = 
{
	UINT64_C( 0x0000000000000001 ), UINT64_C( 0x0000000000008082 ),
	UINT64_C( 0x800000000000808a ), UINT64_C( 0x8000000080008000 ),
	UINT64_C( 0x000000000000808b ), UINT64_C( 0x0000000080000001 ),
	UINT64_C( 0x8000000080008081 ), UINT64_C( 0x8000000000008009 ),
	UINT64_C( 0x000000000000008a ), UINT64_C( 0x0000000000000088 ),
	UINT64_C( 0x0000000080008009 ), UINT64_C( 0x000000008000000a ),
	UINT64_C( 0x000000008000808b ), UINT64_C( 0x800000000000008b ),
	UINT64_C( 0x8000000000008089 ), UINT64_C( 0x8000000000008003 ),
	UINT64_C( 0x8000000000008002 ), UINT64_C( 0x8000000000000080 ),
	UINT64_C( 0x000000000000800a ), UINT64_C( 0x800000008000000a ),
	UINT64_C( 0x8000000080008081 ), UINT64_C( 0x8000000000008080 ),
	UINT64_C( 0x0000000080000001 ), UINT64_C( 0x8000000080008008 )
};

static void	_SHA3_Block( SHA3_CTX_compat *ctx, const uint8_t *in );

//===========================================================================================================================
//	SHA3_Init_compat
//===========================================================================================================================

int	SHA3_Init_compat( SHA3_CTX_compat *ctx )
{
	memset( ctx, 0, sizeof( *ctx ) );
	return( 0 );
}

//===========================================================================================================================
//	SHA3_Update_compat
//===========================================================================================================================

int	SHA3_Update_compat( SHA3_CTX_compat *ctx, const void *inData, size_t inLen )
{
	const uint8_t *		src = (const uint8_t *) inData;
	size_t				n;
	
	// Process any previously buffered data.
	
	if( ctx->buffered > 0 )
	{
		n = SHA3_BLOCK_SIZE - ctx->buffered;
		n = ( n < inLen ) ? n : inLen;
		memcpy( ctx->buffer + ctx->buffered, src, n );
		ctx->buffered += n;
		if( ctx->buffered < SHA3_BLOCK_SIZE ) goto exit;
		src   += n;
		inLen -= n;
		_SHA3_Block( ctx, ctx->buffer );
	}
	
	// Process complete blocks.
	
	while( inLen >= SHA3_BLOCK_SIZE )
	{
		_SHA3_Block( ctx, src );
		src   += SHA3_BLOCK_SIZE;
		inLen -= SHA3_BLOCK_SIZE;
	}
	
	// Buffer any remaining sub-block data.
	
	ctx->buffered = inLen;
	if( inLen > 0 ) memcpy( ctx->buffer, src, inLen );
	
exit:
	return( 0 );
}

//===========================================================================================================================
//	SHA3_Final_compat
//===========================================================================================================================

int	SHA3_Final_compat( uint8_t *outDigest, SHA3_CTX_compat *ctx )
{
	size_t		i;
	
	ctx->buffer[ ctx->buffered++ ] = 0x06;
	memset( ctx->buffer + ctx->buffered, 0, SHA3_BLOCK_SIZE - ctx->buffered );
	ctx->buffer[ SHA3_BLOCK_SIZE - 1 ] |= 0x80;
	_SHA3_Block( ctx, ctx->buffer );
	
	for( i = 0; i < SHA3_DIGEST_LENGTH; i += 8 )
	{
		WriteLittle64( &outDigest[ i ], ctx->state[ i / 8 ] );
	}
	MemZeroSecure( ctx, sizeof( *ctx ) );
	return( 0 );
}

//===========================================================================================================================
//	SHA3_compat
//===========================================================================================================================

uint8_t *	SHA3_compat( const void *inData, size_t inLen, uint8_t outDigest[ 64 ] )
{
	SHA3_CTX_compat		ctx;
	
	SHA3_Init_compat( &ctx );
	SHA3_Update_compat( &ctx, inData, inLen );
	SHA3_Final_compat( outDigest, &ctx );
	return( outDigest );
}

//===========================================================================================================================
//	_SHA3_Block
//===========================================================================================================================

static void	_SHA3_Block( SHA3_CTX_compat *ctx, const uint8_t *inSrc )
{
	uint64_t		s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
	uint64_t		s11, s12, s13, s14, s15, s16, s17, s18, s19, s20;
	uint64_t		s21, s22, s23, s24;
	uint64_t		t0, t1, t2, t3, t4, u0, u1, u2, u3, u4, v, w;
	size_t			i;
	
	s0  = ctx->state[  0 ] ^ ReadLittle64( &inSrc[ 0 ] );
	s1  = ctx->state[  1 ] ^ ReadLittle64( &inSrc[ 8 ] );
	s2  = ctx->state[  2 ] ^ ReadLittle64( &inSrc[ 16 ] );
	s3  = ctx->state[  3 ] ^ ReadLittle64( &inSrc[ 24 ] );
	s4  = ctx->state[  4 ] ^ ReadLittle64( &inSrc[ 32 ] );
	s5  = ctx->state[  5 ] ^ ReadLittle64( &inSrc[ 40 ] );
	s6  = ctx->state[  6 ] ^ ReadLittle64( &inSrc[ 48 ] );
	s7  = ctx->state[  7 ] ^ ReadLittle64( &inSrc[ 56 ] );
	s8  = ctx->state[  8 ] ^ ReadLittle64( &inSrc[ 64 ] );
	s9  = ctx->state[  9 ];
	s10 = ctx->state[ 10 ];
	s11 = ctx->state[ 11 ];
	s12 = ctx->state[ 12 ];
	s13 = ctx->state[ 13 ];
	s14 = ctx->state[ 14 ];
	s15 = ctx->state[ 15 ];
	s16 = ctx->state[ 16 ];
	s17 = ctx->state[ 17 ];
	s18 = ctx->state[ 18 ];
	s19 = ctx->state[ 19 ];
	s20 = ctx->state[ 20 ];
	s21 = ctx->state[ 21 ];
	s22 = ctx->state[ 22 ];
	s23 = ctx->state[ 23 ];
	s24 = ctx->state[ 24 ];
	
	for( i = 0; i < 24; ++i )
	{
		// theta: c = a[0,i] ^ a[1,i] ^ .. a[4,i]
		t0 = s0 ^ s5 ^ s10 ^ s15 ^ s20;
		t1 = s1 ^ s6 ^ s11 ^ s16 ^ s21;
		t2 = s2 ^ s7 ^ s12 ^ s17 ^ s22;
		t3 = s3 ^ s8 ^ s13 ^ s18 ^ s23;
		t4 = s4 ^ s9 ^ s14 ^ s19 ^ s24;
		
		// theta: d[i] = c[i+4] ^ rotl(c[i+1],1)
		u0 = t4 ^ ROTL64( t1, 1 );
		u1 = t0 ^ ROTL64( t2, 1 );
		u2 = t1 ^ ROTL64( t3, 1 );
		u3 = t2 ^ ROTL64( t4, 1 );
		u4 = t3 ^ ROTL64( t0, 1 );
		
		// theta: a[0,i], a[1,i], .. a[4,i] ^= d[i]
		s0 ^= u0; s5 ^= u0; s10 ^= u0; s15 ^= u0; s20 ^= u0;
		s1 ^= u1; s6 ^= u1; s11 ^= u1; s16 ^= u1; s21 ^= u1;
		s2 ^= u2; s7 ^= u2; s12 ^= u2; s17 ^= u2; s22 ^= u2;
		s3 ^= u3; s8 ^= u3; s13 ^= u3; s18 ^= u3; s23 ^= u3;
		s4 ^= u4; s9 ^= u4; s14 ^= u4; s19 ^= u4; s24 ^= u4;
		
		// rho pi: b[..] = rotl(a[..], ..)
		v   = s1;
		s1  = ROTL64( s6,  44 );
		s6  = ROTL64( s9,  20 );
		s9  = ROTL64( s22, 61 );
		s22 = ROTL64( s14, 39 );
		s14 = ROTL64( s20, 18 );
		s20 = ROTL64( s2,  62 );
		s2  = ROTL64( s12, 43 );
		s12 = ROTL64( s13, 25 );
		s13 = ROTL64( s19,  8 );
		s19 = ROTL64( s23, 56 );
		s23 = ROTL64( s15, 41 );
		s15 = ROTL64( s4,  27 );
		s4  = ROTL64( s24, 14 );
		s24 = ROTL64( s21,  2 );
		s21 = ROTL64( s8,  55 );
		s8  = ROTL64( s16, 45 );
		s16 = ROTL64( s5,  36 );
		s5  = ROTL64( s3,  28 );
		s3  = ROTL64( s18, 21 );
		s18 = ROTL64( s17, 15 );
		s17 = ROTL64( s11, 10 );
		s11 = ROTL64( s7,   6 );
		s7  = ROTL64( s10,  3 );
		s10 = ROTL64( v,    1 );
		
		// chi: a[i,j] ^= ~b[i,j+1] & b[i,j+2]
		v = s0;
		w = s1;
		s0 ^= (~w)  & s2;
		s1 ^= (~s2) & s3;
		s2 ^= (~s3) & s4;
		s3 ^= (~s4) & v;
		s4 ^= (~v)  & w;
		v = s5;
		w = s6;
		s5 ^= (~w)  & s7;
		s6 ^= (~s7) & s8;
		s7 ^= (~s8) & s9;
		s8 ^= (~s9) & v;
		s9 ^= (~v)  & w;
		v = s10;
		w = s11;
		s10 ^= (~w)   & s12;
		s11 ^= (~s12) & s13;
		s12 ^= (~s13) & s14;
		s13 ^= (~s14) & v;
		s14 ^= (~v)  & w;
		v = s15;
		w = s16;
		s15 ^= (~w)   & s17;
		s16 ^= (~s17) & s18;
		s17 ^= (~s18) & s19;
		s18 ^= (~s19) & v;
		s19 ^= (~v)   & w;
		v = s20;
		w = s21;
		s20 ^= (~w)   & s22;
		s21 ^= (~s22) & s23;
		s22 ^= (~s23) & s24;
		s23 ^= (~s24) & v;
		s24 ^= (~v)   & w;
		
		// iota: a[0,0] ^= round constant
		s0 ^= kSHA3RoundConstants[ i ];
	}
	
	ctx->state[  0 ] = s0;
	ctx->state[  1 ] = s1;
	ctx->state[  2 ] = s2;
	ctx->state[  3 ] = s3;
	ctx->state[  4 ] = s4;
	ctx->state[  5 ] = s5;
	ctx->state[  6 ] = s6;
	ctx->state[  7 ] = s7;
	ctx->state[  8 ] = s8;
	ctx->state[  9 ] = s9;
	ctx->state[ 10 ] = s10;
	ctx->state[ 11 ] = s11;
	ctx->state[ 12 ] = s12;
	ctx->state[ 13 ] = s13;
	ctx->state[ 14 ] = s14;
	ctx->state[ 15 ] = s15;
	ctx->state[ 16 ] = s16;
	ctx->state[ 17 ] = s17;
	ctx->state[ 18 ] = s18;
	ctx->state[ 19 ] = s19;
	ctx->state[ 20 ] = s20;
	ctx->state[ 21 ] = s21;
	ctx->state[ 22 ] = s22;
	ctx->state[ 23 ] = s23;
	ctx->state[ 24 ] = s24;
}

#if( !EXCLUDE_UNIT_TESTS )

static OSStatus	_SHA3_TestOne( const void *inMsg, size_t inLen, const void *inOutput );

//===========================================================================================================================
//	SHA3_Test
//===========================================================================================================================

OSStatus	SHA3_Test( void )
{
	OSStatus		err;
	
	// Test vectors from <http://en.wikipedia.org/wiki/SHA-3>.
	
	err = _SHA3_TestOne( "", kSizeCString, 
		"\xA6\x9F\x73\xCC\xA2\x3A\x9A\xC5\xC8\xB5\x67\xDC\x18\x5A\x75\x6E"
		"\x97\xC9\x82\x16\x4F\xE2\x58\x59\xE0\xD1\xDC\xC1\x47\x5C\x80\xA6"
		"\x15\xB2\x12\x3A\xF1\xF5\xF9\x4C\x11\xE3\xE9\x40\x2C\x3A\xC5\x58"
		"\xF5\x00\x19\x9D\x95\xB6\xD3\xE3\x01\x75\x85\x86\x28\x1D\xCD\x26" );
	require_noerr( err, exit );
	
	err = _SHA3_TestOne( "The quick brown fox jumps over the lazy dog", kSizeCString, 
		"\x01\xDE\xDD\x5D\xE4\xEF\x14\x64\x24\x45\xBA\x5F\x5B\x97\xC1\x5E"
		"\x47\xB9\xAD\x93\x13\x26\xE4\xB0\x72\x7C\xD9\x4C\xEF\xC4\x4F\xFF"
		"\x23\xF0\x7B\xF5\x43\x13\x99\x39\xB4\x91\x28\xCA\xF4\x36\xDC\x1B"
		"\xDE\xE5\x4F\xCB\x24\x02\x3A\x08\xD9\x40\x3F\x9B\x4B\xF0\xD4\x50" );
	require_noerr( err, exit );
	
	err = _SHA3_TestOne( "The quick brown fox jumps over the lazy dog.", kSizeCString, 
		"\x18\xF4\xF4\xBD\x41\x96\x03\xF9\x55\x38\x83\x70\x03\xD9\xD2\x54"
		"\xC2\x6C\x23\x76\x55\x65\x16\x22\x47\x48\x3F\x65\xC5\x03\x03\x59"
		"\x7B\xC9\xCE\x4D\x28\x9F\x21\xD1\xC2\xF1\xF4\x58\x82\x8E\x33\xDC"
		"\x44\x21\x00\x33\x1B\x35\xE7\xEB\x03\x1B\x5D\x38\xBA\x64\x60\xF8" );
	require_noerr( err, exit );
	
	// Test vectors from NIST's ShortMsgKAT_SHA3-512.txt.
	
	err = _SHA3_TestOne( "", 0, 
		"\xA6\x9F\x73\xCC\xA2\x3A\x9A\xC5\xC8\xB5\x67\xDC\x18\x5A\x75\x6E"
		"\x97\xC9\x82\x16\x4F\xE2\x58\x59\xE0\xD1\xDC\xC1\x47\x5C\x80\xA6"
		"\x15\xB2\x12\x3A\xF1\xF5\xF9\x4C\x11\xE3\xE9\x40\x2C\x3A\xC5\x58"
		"\xF5\x00\x19\x9D\x95\xB6\xD3\xE3\x01\x75\x85\x86\x28\x1D\xCD\x26" );
	require_noerr( err, exit );
	
	err = _SHA3_TestOne( "\xCC", 1, 
		"\x39\x39\xFC\xC8\xB5\x7B\x63\x61\x25\x42\xDA\x31\xA8\x34\xE5\xDC"
		"\xC3\x6E\x2E\xE0\xF6\x52\xAC\x72\xE0\x26\x24\xFA\x2E\x5A\xDE\xEC"
		"\xC7\xDD\x6B\xB3\x58\x02\x24\xB4\xD6\x13\x87\x06\xFC\x6E\x80\x59"
		"\x7B\x52\x80\x51\x23\x0B\x00\x62\x1C\xC2\xB2\x29\x99\xEA\xA2\x05" );
	require_noerr( err, exit );
	
	err = _SHA3_TestOne( "\x4A\x4F\x20\x24\x84\x51\x25\x26", 8, 
		"\x15\x0D\x78\x7D\x6E\xB4\x96\x70\xC2\xA4\xCC\xD1\x7E\x6C\xCE\x7A"
		"\x04\xC1\xFE\x30\xFC\xE0\x3D\x1E\xF2\x50\x17\x52\xD9\x2A\xE0\x4C"
		"\xB3\x45\xFD\x42\xE5\x10\x38\xC8\x3B\x2B\x4F\x8F\xD4\x38\xD1\xB4"
		"\xB5\x5C\xC5\x88\xC6\xB9\x13\x13\x2F\x1A\x65\x8F\xB1\x22\xCB\x52" );
	require_noerr( err, exit );
	
	err = _SHA3_TestOne( "\xEE\xD7\x42\x22\x27\x61\x3B\x6F\x53\xC9", 10, 
		"\x5A\x56\x6F\xB1\x81\xBE\x53\xA4\x10\x92\x75\x53\x7D\x80\xE5\xFD"
		"\x0F\x31\x4D\x68\x88\x45\x29\xCA\x66\xB8\xB0\xE9\xF2\x40\xA6\x73"
		"\xB6\x4B\x28\xFF\xFE\x4C\x1E\xC4\xA5\xCE\xF0\xF4\x30\x22\x9C\x57"
		"\x57\xEB\xD1\x72\xB4\xB0\xB6\x8A\x81\xD8\xC5\x8A\x9E\x96\xE1\x64" );
	require_noerr( err, exit );
	
	err = _SHA3_TestOne( 
		"\x3A\x3A\x81\x9C\x48\xEF\xDE\x2A\xD9\x14\xFB\xF0\x0E\x18\xAB\x6B"
		"\xC4\xF1\x45\x13\xAB\x27\xD0\xC1\x78\xA1\x88\xB6\x14\x31\xE7\xF5"
		"\x62\x3C\xB6\x6B\x23\x34\x67\x75\xD3\x86\xB5\x0E\x98\x2C\x49\x3A"
		"\xDB\xBF\xC5\x4B\x9A\x3C\xD3\x83\x38\x23\x36\xA1\xA0\xB2\x15\x0A"
		"\x15\x35\x8F\x33\x6D\x03\xAE\x18\xF6\x66\xC7\x57\x3D\x55\xC4\xFD"
		"\x18\x1C\x29\xE6\xCC\xFD\xE6\x3E\xA3\x5F\x0A\xDF\x58\x85\xCF\xC0"
		"\xA3\xD8\x4A\x2B\x2E\x4D\xD2\x44\x96\xDB\x78\x9E\x66\x31\x70\xCE"
		"\xF7\x47\x98\xAA\x1B\xBC\xD4\x57\x4E\xA0\xBB\xA4\x04\x89\xD7\x64"
		"\xB2\xF8\x3A\xAD\xC6\x6B\x14\x8B\x4A\x0C\xD9\x52\x46\xC1\x27\xD5"
		"\x87\x1C\x4F\x11\x41\x86\x90\xA5\xDD\xF0\x12\x46\xA0\xC8\x0A\x43"
		"\xC7\x00\x88\xB6\x18\x36\x39\xDC\xFD\xA4\x12\x5B\xD1\x13\xA8\xF4"
		"\x9E\xE2\x3E\xD3\x06\xFA\xAC\x57\x6C\x3F\xB0\xC1\xE2\x56\x67\x1D"
		"\x81\x7F\xC2\x53\x4A\x52\xF5\xB4\x39\xF7\x2E\x42\x4D\xE3\x76\xF4"
		"\xC5\x65\xCC\xA8\x23\x07\xDD\x9E\xF7\x6D\xA5\xB7\xC4\xEB\x7E\x08"
		"\x51\x72\xE3\x28\x80\x7C\x02\xD0\x11\xFF\xBF\x33\x78\x53\x78\xD7"
		"\x9D\xC2\x66\xF6\xA5\xBE\x6B\xB0\xE4\xA9\x2E\xCE\xEB\xAE\xB1", 
		255, 
		"\x6E\x8B\x8B\xD1\x95\xBD\xD5\x60\x68\x9A\xF2\x34\x8B\xDC\x74\xAB"
		"\x7C\xD0\x5E\xD8\xB9\xA5\x77\x11\xE9\xBE\x71\xE9\x72\x6F\xDA\x45"
		"\x91\xFE\xE1\x22\x05\xED\xAC\xAF\x82\xFF\xBB\xAF\x16\xDF\xF9\xE7"
		"\x02\xA7\x08\x86\x20\x80\x16\x6C\x2F\xF6\xBA\x37\x9B\xC7\xFF\xC2" );
	require_noerr( err, exit );
	
exit:
	printf( "SHA3_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

#if( TARGET_HAS_C_LIB_IO )

#include <stdio.h>

#include "StringUtils.h"

//===========================================================================================================================
//	SHA3_TestFile
//===========================================================================================================================

OSStatus	SHA3_TestFile( const char *inPath )
{
	OSStatus		err;
	FILE *			file;
	char			line[ 576 ];
	unsigned int	bitLen;
	int				n;
	uint8_t			msg[ 256 ];
	size_t			msgLen;
	uint8_t			digest[ 64 ];
	size_t			digestLen;
	
	file = fopen( inPath, "r" );
	err = map_global_value_errno( file, file );
	require_noerr( err, exit );
	
	while( fgets( line, (int) sizeof( line ), file ) )
	{
		if( ( *line == '\0' ) || ( *line == '#' ) ) continue;
		n = sscanf( line, "Len = %u", &bitLen );
		if( n != 1 ) continue;
		if( ( bitLen % 8 ) != 0 ) continue;
		
		*line = '\0';
		fgets( line, (int) sizeof( line ), file );
		require_action( strcmp_prefix( line, "Msg = " ) == 0, exit, err = kFormatErr );
		err = HexToData( line + 6, kSizeCString, kHexToData_DefaultFlags, msg, sizeof( msg ), &msgLen, NULL, NULL );
		require_noerr( err, exit );
		if( ( msgLen == 1 ) && ( bitLen == 0 ) ) msgLen = 0;
		require_action( msgLen == ( bitLen / 8 ), exit, err = kSizeErr );
		
		*line = '\0';
		fgets( line, (int) sizeof( line ), file );
		require_action( strcmp_prefix( line, "MD = " ) == 0, exit, err = kFormatErr );
		err = HexToData( line + 5, kSizeCString, kHexToData_DefaultFlags, digest, sizeof( digest ), &digestLen, NULL, NULL );
		require_noerr( err, exit );
		require_action( digestLen == 64, exit, err = kSizeErr );
		
		err = _SHA3_TestOne( msg, msgLen, digest );
		require_noerr( err, exit );
	}
	
exit:
	if( file ) fclose( file );
	return( err );
}
#endif // TARGET_HAS_C_LIB_IO

//===========================================================================================================================
//	_SHA3_TestOne
//===========================================================================================================================

static OSStatus	_SHA3_TestOne( const void *inMsg, size_t inLen, const void *inOutput )
{
	const uint8_t *		msg = (const uint8_t *) inMsg;
	OSStatus			err;
	uint8_t				digest[ 64 ];
	uint8_t *			ptr;
	size_t				i;
	SHA3_CTX_compat		ctx;
	
	if( inLen == kSizeCString ) inLen = strlen( (const char *) msg );
	
	// Test all-at-once API.
	
	ptr = SHA3_compat( msg, inLen, digest );
	require_action( ptr == digest, exit, err = -1 );
	require_action( memcmp( digest, inOutput, 64 ) == 0, exit, err = -1 );
	
	// Test byte-at-a-time via update API.
	
	memset( digest, 0, sizeof( digest ) );
	SHA3_Init_compat( &ctx );
	for( i = 0; i < inLen; ++i )
	{
		SHA3_Update_compat( &ctx, &msg[ i ], 1 );
	}
	SHA3_Final_compat( digest, &ctx );
	require_action( memcmp( digest, inOutput, 64 ) == 0, exit, err = -1 );
	
	err = kNoErr;
	
exit:
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS

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

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	PBKDF2_HMAC_SHA1
//===========================================================================================================================

void
	PBKDF2_HMAC_SHA1( 
		const void *	inPasswordPtr,	
		size_t			inPasswordLen, 
		const void *	inSaltPtr, 
		size_t			inSaltLen, 
		int				inIterations,
		size_t			inKeyLen, 
		uint8_t *		outKey )
{
	HMAC_SHA1_CTX		ctx;
	int					i, ii;
	uint8_t				counter[ 4 ];
	uint8_t				T[ 20 ], digest[ 20 ];
	size_t				offset, len;
	
	if( inPasswordLen	== kSizeCString ) inPasswordLen	= strlen( (const char *) inPasswordPtr );
	if( inSaltLen		== kSizeCString ) inSaltLen		= strlen( (const char *) inSaltPtr );
	check( inIterations > 0 );
	check( inSaltLen > 0 );
	check( inKeyLen > 0 );
	
	offset = 0;
	counter[ 0 ] = 0;
	counter[ 1 ] = 0;
	counter[ 2 ] = 0;
	counter[ 3 ] = 1;
	while( inKeyLen > 0 )
	{
		HMAC_SHA1_Init( &ctx, inPasswordPtr, inPasswordLen );
		HMAC_SHA1_Update( &ctx, inSaltPtr, inSaltLen );
		HMAC_SHA1_Update( &ctx, counter, 4 );
		HMAC_SHA1_Final( &ctx, T );
		for( i = 1; i < inIterations; ++i )
		{
			HMAC_SHA1_Init( &ctx, inPasswordPtr, inPasswordLen );
			HMAC_SHA1_Update( &ctx, ( i == 1 ) ? T : digest, 20 );
			HMAC_SHA1_Final( &ctx, digest );
			for( ii = 0; ii < 20; ++ii ) T[ ii ] ^= digest[ ii ];
		}
		len = Min( inKeyLen, 20 );
		memcpy( &outKey[ offset ], T, len );
		offset   += len;
		inKeyLen -= len;
		BigEndianIntegerIncrement( counter, 4 );
	}
}

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	PBKDF2_HMAC_SHA1_Test
//===========================================================================================================================

typedef struct
{
	const void *		passwordPtr;
	size_t				passwordLen;
	const void *		saltPtr;
	size_t				saltLen;
	int					count;
	const void *		outputPtr;
	size_t				outputLen;
	
}	PBKDF2_HMAC_SHA1_TestCase;

static const PBKDF2_HMAC_SHA1_TestCase		kPBKDF2_HMAC_SHA1_TestCases[] = 
{
	// Test vectors from RFC 6070.
	
	// Test Case 1
	{
		/* password */	"password", 
						8, 
		/* salt */		"salt", 
						4, 
		/* count */		1, 
		/* output */	"\x0C\x60\xC8\x0F\x96\x1F\x0E\x71\xF3\xA9\xB5\x24\xAF\x60\x12\x06\x2F\xE0\x37\xA6", 
						20
	}, 
	// Test Case 2
	{
		/* password */	"password", 
						8, 
		/* salt */		"salt", 
						4, 
		/* count */		2, 
		/* output */	"\xEA\x6C\x01\x4D\xC7\x2D\x6F\x8C\xCD\x1E\xD9\x2A\xCE\x1D\x41\xF0\xD8\xDE\x89\x57", 
						20
	}, 
	// Test Case 3
	{
		/* password */	"password", 
						8, 
		/* salt */		"salt", 
						4, 
		/* count */		4096, 
		/* output */	"\x4B\x00\x79\x01\xB7\x65\x48\x9A\xBE\xAD\x49\xD9\x26\xF7\x21\xD0\x65\xA4\x29\xC1", 
						20
	}, 
	#if 0 // Conditionalize out since it takes too long to run this during normal tests. I've verified it manually it.
	// Test Case 4
	{
		/* password */	"password", 
						8, 
		/* salt */		"salt", 
						4, 
		/* count */		16777216, 
		/* output */	"\xEE\xFE\x3D\x61\xCD\x4D\xA4\xE4\xE9\x94\x5B\x3D\x6B\xA2\x15\x8C\x26\x34\xE9\x84", 
						20
	}, 
	#endif
	// Test Case 5
	{
		/* password */	"passwordPASSWORDpassword", 
						24, 
		/* salt */		"saltSALTsaltSALTsaltSALTsaltSALTsalt", 
						36, 
		/* count */		4096, 
		/* output */	"\x3D\x2E\xEC\x4F\xE4\x1C\x84\x9B\x80\xC8\xD8\x36\x62\xC0\xE4\x4A\x8B\x29\x1A\x96\x4C\xF2\xF0\x70\x38", 
						25
	}, 
	// Test Case 5
	{
		/* password */	"pass\0word", 
						9, 
		/* salt */		"sa\0lt", 
						5, 
		/* count */		4096, 
		/* output */	"\x56\xFA\x6A\xA7\x55\x48\x09\x9D\xCC\x37\xD7\xF0\x34\x25\xE0\xC3", 
						16
	}, 
};

OSStatus	PBKDF2_HMAC_SHA1_Test( void )
{
	OSStatus								err;
	const PBKDF2_HMAC_SHA1_TestCase *		tc;
	size_t									i;
	uint8_t									output[ 25 ];
	
	for( i = 0; i < countof( kPBKDF2_HMAC_SHA1_TestCases ); ++i )
	{
		tc = &kPBKDF2_HMAC_SHA1_TestCases[ i ];
		memset( output, 0, sizeof( output ) );
		require_action( tc->outputLen <= sizeof( output ), exit, err = kInternalErr );
		PBKDF2_HMAC_SHA1( tc->passwordPtr, tc->passwordLen, tc->saltPtr, tc->saltLen, tc->count, tc->outputLen, output );
		require_action( memcmp( output, tc->outputPtr, tc->outputLen ) == 0, exit, err = kResponseErr );
	}
	err = kNoErr;
	
exit:
	printf( "PBKDF2_HMAC_SHA1_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
