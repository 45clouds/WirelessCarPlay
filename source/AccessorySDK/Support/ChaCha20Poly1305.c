/*
	File:    	ChaCha20Poly1305.c
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
	
	Implements ChaCha20 encryption, Poly1305 MAC, and ChaCha20-Poly1305 AEAD APIs.
*/

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if( !defined( _CRT_SECURE_NO_DEPRECATE ) )
	#define _CRT_SECURE_NO_DEPRECATE		1
#endif

#include "ChaCha20Poly1305.h"

#include "CommonServices.h"
#include "DebugServices.h"

#if 0
#pragma mark == chacha20 ==
#endif

//===========================================================================================================================
//	chacha20
//
//	Based on DJB's public domain chacha20 code: <http://cr.yp.to/chacha.html>.
//===========================================================================================================================

#if( TARGET_HAS_NEON || ( TARGET_HAS_SSE >= SSE_VERSION( 2, 0 ) ) )
	#define CHACHA20_SIMD		1
#else
	#define CHACHA20_SIMD		0
#endif

#define CHACHA20_QUARTERROUND( a, b, c, d ) \
	a += b; d = ROTL32( d ^ a, 16 ); \
	c += d; b = ROTL32( b ^ c, 12 ); \
	a += b; d = ROTL32( d ^ a,  8 ); \
	c += d; b = ROTL32( b ^ c,  7 );

// "expand 32-byte k", as 4 little endian 32-bit unsigned integers.
#if( CHACHA20_SIMD )
__attribute__( ( aligned( 16 ) ) )
#endif
static const uint32_t		kChaCha20Constants[ 4 ] = { 0x61707865, 0x3320646e, 0x79622d32, 0x6b206574 };

static void	_chacha20_xor( chacha20_state *inState, uint8_t *inDst, const uint8_t *inSrc, size_t inLen );

//===========================================================================================================================
//	chacha20_all_64x64
//===========================================================================================================================

void
	chacha20_all_64x64( 
		const uint8_t inKey[ 32 ], const uint8_t inNonce[ 8 ], uint64_t inCounter, 
		const void *inSrc, size_t inLen, void *inDst )
{
	chacha20_state		state;
	
	chacha20_init_64x64( &state, inKey, inNonce, inCounter );
	_chacha20_xor( &state, (uint8_t *) inDst, (const uint8_t *) inSrc, inLen );
	MemZeroSecure( &state, sizeof( state ) );
}

//===========================================================================================================================
//	chacha20_all_96x32
//===========================================================================================================================

void
	chacha20_all_96x32( 
		const uint8_t inKey[ 32 ], const uint8_t inNonce[ 12 ], uint32_t inCounter, 
		const void *inSrc, size_t inLen, void *inDst )
{
	chacha20_state		state;
	
	chacha20_init_96x32( &state, inKey, inNonce, inCounter );
	_chacha20_xor( &state, (uint8_t *) inDst, (const uint8_t *) inSrc, inLen );
	MemZeroSecure( &state, sizeof( state ) );
}

//===========================================================================================================================
//	chacha20_init_64x64
//===========================================================================================================================

void	chacha20_init_64x64( chacha20_state *inState, const uint8_t inKey[ 32 ], const uint8_t inNonce[ 8 ], uint64_t inCounter )
{
	inState->state[  0 ] = kChaCha20Constants[ 0 ];
	inState->state[  1 ] = kChaCha20Constants[ 1 ];
	inState->state[  2 ] = kChaCha20Constants[ 2 ];
	inState->state[  3 ] = kChaCha20Constants[ 3 ];
	
	inState->state[  4 ] = ReadLittle32( inKey +  0 );
	inState->state[  5 ] = ReadLittle32( inKey +  4 );
	inState->state[  6 ] = ReadLittle32( inKey +  8 );
	inState->state[  7 ] = ReadLittle32( inKey + 12 );
	
	inState->state[  8 ] = ReadLittle32( inKey + 16 );
	inState->state[  9 ] = ReadLittle32( inKey + 20 );
	inState->state[ 10 ] = ReadLittle32( inKey + 24 );
	inState->state[ 11 ] = ReadLittle32( inKey + 28 );
	
	inState->state[ 12 ] = (uint32_t)( inCounter & UINT32_C( 0xFFFFFFFF ) );
	inState->state[ 13 ] = (uint32_t)( inCounter >> 32 );
	if( inNonce )
	{
		inState->state[ 14 ] = ReadLittle32( inNonce + 0 );
		inState->state[ 15 ] = ReadLittle32( inNonce + 4 );
	}
	else
	{
		inState->state[ 14 ] = 0;
		inState->state[ 15 ] = 0;
	}
	inState->leftover = 0;
}

//===========================================================================================================================
//	chacha20_init_96x32
//===========================================================================================================================

void	chacha20_init_96x32( chacha20_state *inState, const uint8_t inKey[ 32 ], const uint8_t inNonce[ 12 ], uint32_t inCounter )
{
	inState->state[  0 ] = kChaCha20Constants[ 0 ];
	inState->state[  1 ] = kChaCha20Constants[ 1 ];
	inState->state[  2 ] = kChaCha20Constants[ 2 ];
	inState->state[  3 ] = kChaCha20Constants[ 3 ];
	
	inState->state[  4 ] = ReadLittle32( inKey +  0 );
	inState->state[  5 ] = ReadLittle32( inKey +  4 );
	inState->state[  6 ] = ReadLittle32( inKey +  8 );
	inState->state[  7 ] = ReadLittle32( inKey + 12 );
	
	inState->state[  8 ] = ReadLittle32( inKey + 16 );
	inState->state[  9 ] = ReadLittle32( inKey + 20 );
	inState->state[ 10 ] = ReadLittle32( inKey + 24 );
	inState->state[ 11 ] = ReadLittle32( inKey + 28 );
	
	inState->state[ 12 ] = inCounter;
	if( inNonce )
	{
		inState->state[ 13 ] = ReadLittle32( inNonce + 0 );
		inState->state[ 14 ] = ReadLittle32( inNonce + 4 );
		inState->state[ 15 ] = ReadLittle32( inNonce + 8 );
	}
	else
	{
		inState->state[ 13 ] = 0;
		inState->state[ 14 ] = 0;
		inState->state[ 15 ] = 0;
	}
	inState->leftover = 0;
}

//===========================================================================================================================
//	chacha20_update
//===========================================================================================================================

size_t	chacha20_update( chacha20_state *inState, const void *inSrc, size_t inLen, void *inDst )
{
	const uint8_t *		src = (const uint8_t *) inSrc;
	uint8_t *			dst = (uint8_t *) inDst;
	size_t				i, j, n;
	
	j = inState->leftover;
	if( j )
	{
		n = CHACHA_BLOCKBYTES - j;
		if( n > inLen ) n = inLen;
		for( i = 0; i < n; ++i ) inState->buffer[ j++ ] = src[ i ];
		src += n;
		inLen -= n;
		if( j == CHACHA_BLOCKBYTES )
		{
			_chacha20_xor( inState, dst, inState->buffer, CHACHA_BLOCKBYTES );
			dst += CHACHA_BLOCKBYTES;
			j = 0;
		}
		inState->leftover = j;
	}
	if( inLen >= CHACHA_BLOCKBYTES )
	{
		n = inLen & ~( (size_t)( CHACHA_BLOCKBYTES - 1 ) );
		_chacha20_xor( inState, dst, src, n );
		src += n;
		dst += n;
		inLen &= ( CHACHA_BLOCKBYTES - 1 );
	}
	if( inLen )
	{
		for( i = 0; i < inLen; ++i ) inState->buffer[ i ] = src[ i ];
		inState->leftover = inLen;
	}
	return( (size_t)( dst - ( (uint8_t *) inDst ) ) );
}

//===========================================================================================================================
//	chacha20_final
//===========================================================================================================================

size_t	chacha20_final( chacha20_state *inState, void *inDst )
{
	size_t		n;
	
	n = inState->leftover;
	if( n ) _chacha20_xor( inState, (uint8_t *) inDst, inState->buffer, n );
	MemZeroSecure( inState, sizeof( *inState ) );
	return( n );
}

#if( !CHACHA20_SIMD )
//===========================================================================================================================
//	_chacha20_xor
//===========================================================================================================================

static void	_chacha20_xor( chacha20_state *inState, uint8_t *inDst, const uint8_t *inSrc, size_t inLen )
{
	uint32_t		x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
	uint32_t		j0, j1, j2, j3, j4, j5, j6, j7, j8, j9, j10, j11, j12, j13, j14, j15;
	uint8_t *		ctarget = inDst;
	uint8_t			tmp[ CHACHA_BLOCKBYTES ];
	size_t			i;
	
	j0 = inState->state[ 0 ];
	j1 = inState->state[ 1 ];
	j2 = inState->state[ 2 ];
	j3 = inState->state[ 3 ];
	j4 = inState->state[ 4 ];
	j5 = inState->state[ 5 ];
	j6 = inState->state[ 6 ];
	j7 = inState->state[ 7 ];
	j8 = inState->state[ 8 ];
	j9 = inState->state[ 9 ];
	j10 = inState->state[ 10 ];
	j11 = inState->state[ 11 ];
	j12 = inState->state[ 12 ];
	j13 = inState->state[ 13 ];
	j14 = inState->state[ 14 ];
	j15 = inState->state[ 15 ];
	
	for( ;; )
	{
		if( inLen < CHACHA_BLOCKBYTES )
		{
			for( i = 0; i < inLen; ++i ) tmp[ i ] = inSrc[ i ];
			inSrc	= tmp;
			ctarget	= inDst;
			inDst	= tmp;
		}
		 x0 = j0;
		 x1 = j1;
		 x2 = j2;
		 x3 = j3;
		 x4 = j4;
		 x5 = j5;
		 x6 = j6;
		 x7 = j7;
		 x8 = j8;
		 x9 = j9;
		x10 = j10;
		x11 = j11;
		x12 = j12;
		x13 = j13;
		x14 = j14;
		x15 = j15;
		for( i = 20; i > 0; i -= 2 )
		{
			CHACHA20_QUARTERROUND( x0, x4,  x8, x12 )
			CHACHA20_QUARTERROUND( x1, x5,  x9, x13 )
			CHACHA20_QUARTERROUND( x2, x6, x10, x14 )
			CHACHA20_QUARTERROUND( x3, x7, x11, x15 )
			CHACHA20_QUARTERROUND( x0, x5, x10, x15 )
			CHACHA20_QUARTERROUND( x1, x6, x11, x12 )
			CHACHA20_QUARTERROUND( x2, x7,  x8, x13 )
			CHACHA20_QUARTERROUND( x3, x4,  x9, x14 )
		}
		 x0 += j0;
		 x1 += j1;
		 x2 += j2;
		 x3 += j3;
		 x4 += j4;
		 x5 += j5;
		 x6 += j6;
		 x7 += j7;
		 x8 += j8;
		 x9 += j9;
		x10 += j10;
		x11 += j11;
		x12 += j12;
		x13 += j13;
		x14 += j14;
		x15 += j15;
		
		 x0 ^= ReadLittle32( inSrc +  0 );
		 x1 ^= ReadLittle32( inSrc +  4 );
		 x2 ^= ReadLittle32( inSrc +  8 );
		 x3 ^= ReadLittle32( inSrc + 12 );
		 x4 ^= ReadLittle32( inSrc + 16 );
		 x5 ^= ReadLittle32( inSrc + 20 );
		 x6 ^= ReadLittle32( inSrc + 24 );
		 x7 ^= ReadLittle32( inSrc + 28 );
		 x8 ^= ReadLittle32( inSrc + 32 );
		 x9 ^= ReadLittle32( inSrc + 36 );
		x10 ^= ReadLittle32( inSrc + 40 );
		x11 ^= ReadLittle32( inSrc + 44 );
		x12 ^= ReadLittle32( inSrc + 48 );
		x13 ^= ReadLittle32( inSrc + 52 );
		x14 ^= ReadLittle32( inSrc + 56 );
		x15 ^= ReadLittle32( inSrc + 60 );
		
		if( !++j12 ) ++j13; // Stopping at 2^70 bytes per nonce is the caller's responsibility.
		
		WriteLittle32( inDst +  0,  x0 );
		WriteLittle32( inDst +  4,  x1 );
		WriteLittle32( inDst +  8,  x2 );
		WriteLittle32( inDst + 12,  x3 );
		WriteLittle32( inDst + 16,  x4 );
		WriteLittle32( inDst + 20,  x5 );
		WriteLittle32( inDst + 24,  x6 );
		WriteLittle32( inDst + 28,  x7 );
		WriteLittle32( inDst + 32,  x8 );
		WriteLittle32( inDst + 36,  x9 );
		WriteLittle32( inDst + 40, x10 );
		WriteLittle32( inDst + 44, x11 );
		WriteLittle32( inDst + 48, x12 );
		WriteLittle32( inDst + 52, x13 );
		WriteLittle32( inDst + 56, x14 );
		WriteLittle32( inDst + 60, x15 );
		
		if( inLen <= CHACHA_BLOCKBYTES )
		{
			if( inLen < CHACHA_BLOCKBYTES )
			{
				for( i = 0; i < inLen; ++i ) ctarget[ i ] = inDst[ i ];
			}
			inState->state[ 12 ] = j12;
			inState->state[ 13 ] = j13;
			return;
		}
		inLen -= CHACHA_BLOCKBYTES;
		inDst += CHACHA_BLOCKBYTES;
		inSrc += CHACHA_BLOCKBYTES;
	}
}
#endif // !CHACHA20_SIMD

#if( CHACHA20_SIMD )
//===========================================================================================================================
//	_chacha20_xor
//
//	Based on public domain implementation by Ted Krovetz (ted@krovetz.net).
//===========================================================================================================================

#ifndef CHACHA_RNDS
#define CHACHA_RNDS 20	/* 8 (high speed), 20 (conservative), 12 (middle) */
#endif

// This implementation is designed for Neon and SSE machines. The following specify how to do certain vector operations 
// efficiently on each architecture, using intrinsics. This implementation supports parallel processing of multiple blocks,
// including potentially using general-purpose registers.

#if( TARGET_HAS_NEON )
	#include <arm_neon.h>
	#define GPR_TOO			1
	#define VBPI			2
	#define ONE				(uint32x4_t)vsetq_lane_u32(1,vdupq_n_u32(0),0)
	#define LOAD(m)			(uint32x4_t)vld1q_u8((uint8_t *)(m))
	#define STORE(m,r)		vst1q_u8((uint8_t *)(m),(uint8x16_t)(r))
	#define ROTV1(x)		(uint32x4_t)vextq_u32((uint32x4_t)x,(uint32x4_t)x,1)
	#define ROTV2(x)		(uint32x4_t)vextq_u32((uint32x4_t)x,(uint32x4_t)x,2)
	#define ROTV3(x)		(uint32x4_t)vextq_u32((uint32x4_t)x,(uint32x4_t)x,3)
	#define ROTW16(x)		(uint32x4_t)vrev32q_u16((uint16x8_t)x)
	#if COMPILER_CLANG
		#define ROTW7(x)	(x << ((uint32x4_t){ 7, 7, 7, 7})) ^ (x >> ((uint32x4_t){25,25,25,25}))
		#define ROTW8(x)	(x << ((uint32x4_t){ 8, 8, 8, 8})) ^ (x >> ((uint32x4_t){24,24,24,24}))
		#define ROTW12(x)	(x << ((uint32x4_t){12,12,12,12})) ^ (x >> ((uint32x4_t){20,20,20,20}))
	#else
		#define ROTW7(x)	(uint32x4_t)vsriq_n_u32(vshlq_n_u32((uint32x4_t)x,7),(uint32x4_t)x,25)
		#define ROTW8(x)	(uint32x4_t)vsriq_n_u32(vshlq_n_u32((uint32x4_t)x,8),(uint32x4_t)x,24)
		#define ROTW12(x)	(uint32x4_t)vsriq_n_u32(vshlq_n_u32((uint32x4_t)x,12),(uint32x4_t)x,20)
	#endif
#elif( TARGET_HAS_SSE >= SSE_VERSION( 2, 0 ) )
	#include <emmintrin.h>
	#define GPR_TOO			0
	#if COMPILER_CLANG
		#define VBPI		4
	#else
		#define VBPI		3
	#endif
	#define ONE				(uint32x4_t)_mm_set_epi32(0,0,0,1)
	#define LOAD(m)			(uint32x4_t)_mm_loadu_si128((__m128i*)(m))
	#define STORE(m,r)		_mm_storeu_si128((__m128i*)(m), (__m128i) (r))
	#define ROTV1(x)		(uint32x4_t)_mm_shuffle_epi32((__m128i)x,_MM_SHUFFLE(0,3,2,1))
	#define ROTV2(x)		(uint32x4_t)_mm_shuffle_epi32((__m128i)x,_MM_SHUFFLE(1,0,3,2))
	#define ROTV3(x)		(uint32x4_t)_mm_shuffle_epi32((__m128i)x,_MM_SHUFFLE(2,1,0,3))
	#define ROTW7(x)		(uint32x4_t)(_mm_slli_epi32((__m128i)x, 7) ^ _mm_srli_epi32((__m128i)x,25))
	#define ROTW12(x)		(uint32x4_t)(_mm_slli_epi32((__m128i)x,12) ^ _mm_srli_epi32((__m128i)x,20))
	#if( TARGET_HAS_SSSE >= SSSE_VERSION( 3 ) )
		#include <tmmintrin.h>
		#define ROTW8(x)	(uint32x4_t)_mm_shuffle_epi8((__m128i)x,_mm_set_epi8(14,13,12,15,10,9,8,11,6,5,4,7,2,1,0,3))
		#define ROTW16(x)	(uint32x4_t)_mm_shuffle_epi8((__m128i)x,_mm_set_epi8(13,12,15,14,9,8,11,10,5,4,7,6,1,0,3,2))
	#else
		#define ROTW8(x)	(uint32x4_t)(_mm_slli_epi32((__m128i)x, 8) ^ _mm_srli_epi32((__m128i)x,24))
		#define ROTW16(x)	(uint32x4_t)(_mm_slli_epi32((__m128i)x,16) ^ _mm_srli_epi32((__m128i)x,16))
	#endif
#else
	#error "SIMD implementation not supported on this platform"
#endif

#ifndef REVV_BE
#define REVV_BE(x)	(x)
#endif

#ifndef REVW_BE
#define REVW_BE(x)	(x)
#endif

#define BPI			(VBPI + GPR_TOO) // Blocks computed per loop iteration.

#define DQROUND_VECTORS(a,b,c,d) \
	a += b; d ^= a; d = ROTW16(d); \
	c += d; b ^= c; b = ROTW12(b); \
	a += b; d ^= a; d = ROTW8(d); \
	c += d; b ^= c; b = ROTW7(b); \
	b = ROTV1(b); c = ROTV2(c);  d = ROTV3(d); \
	a += b; d ^= a; d = ROTW16(d); \
	c += d; b ^= c; b = ROTW12(b); \
	a += b; d ^= a; d = ROTW8(d); \
	c += d; b ^= c; b = ROTW7(b); \
	b = ROTV3(b); c = ROTV2(c); d = ROTV1(d);

#define QROUND_WORDS(a,b,c,d) \
	a = a+b; d ^= a; d = d<<16 | d>>16; \
	c = c+d; b ^= c; b = b<<12 | b>>20; \
	a = a+b; d ^= a; d = d<< 8 | d>>24; \
	c = c+d; b ^= c; b = b<< 7 | b>>25;

#define WRITE_XOR(in, op, d, v0, v1, v2, v3) \
	STORE(op + d + 0, LOAD(in + d + 0) ^ REVV_BE(v0)); \
	STORE(op + d + 4, LOAD(in + d + 4) ^ REVV_BE(v1)); \
	STORE(op + d + 8, LOAD(in + d + 8) ^ REVV_BE(v2)); \
	STORE(op + d +12, LOAD(in + d +12) ^ REVV_BE(v3));

static void	_chacha20_xor( chacha20_state *inState, uint8_t *out, const uint8_t *in, size_t inlen )
{
	size_t iters, i;
	unsigned *op=(unsigned *)out, *ip=(unsigned *)in, *kp;
#if GPR_TOO
	unsigned *np = (unsigned*) (const unsigned char *) &inState->state[14];
#endif
	uint32x4_t s0, s1, s2, s3;
	kp = &inState->state[4];
	s0 = LOAD(kChaCha20Constants);
	s1 = LOAD(&((uint32x4_t*)kp)[0]);
	s2 = LOAD(&((uint32x4_t*)kp)[1]);
	s3 = LOAD(&inState->state[12]);

	for (iters = 0; iters < inlen/(BPI*64); iters++) {
#if GPR_TOO
	register unsigned x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
#endif
#if VBPI > 2
	uint32x4_t v8,v9,v10,v11;
#endif
#if VBPI > 3
	uint32x4_t v12,v13,v14,v15;
#endif

	uint32x4_t v0,v1,v2,v3,v4,v5,v6,v7;
	v4 = v0 = s0; v5 = v1 = s1; v6 = v2 = s2; v3 = s3;
	v7 = v3 + ONE;
#if VBPI > 2
	v8 = v4; v9 = v5; v10 = v6;
	v11 =  v7 + ONE;
#endif
#if VBPI > 3
	v12 = v8; v13 = v9; v14 = v10;
	v15 = v11 + ONE;
#endif
#if GPR_TOO
	x0 = kChaCha20Constants[0]; x1 = kChaCha20Constants[1];
	x2 = kChaCha20Constants[2]; x3 = kChaCha20Constants[3];
	x4 = kp[0]; x5 = kp[1]; x6  = kp[2]; x7  = kp[3];
	x8 = kp[4]; x9 = kp[5]; x10 = kp[6]; x11 = kp[7];
	x12 = (inState->state[12])+((unsigned)(BPI*iters+(BPI-1))); x13 = inState->state[13]; x14 = np[0]; x15 = np[1];
#endif
	for (i = CHACHA_RNDS/2; i; i--) {
		DQROUND_VECTORS(v0,v1,v2,v3)
		DQROUND_VECTORS(v4,v5,v6,v7)
#if VBPI > 2
		DQROUND_VECTORS(v8,v9,v10,v11)
#endif
#if VBPI > 3
		DQROUND_VECTORS(v12,v13,v14,v15)
#endif
#if GPR_TOO
		QROUND_WORDS( x0, x4, x8,x12)
		QROUND_WORDS( x1, x5, x9,x13)
		QROUND_WORDS( x2, x6,x10,x14)
		QROUND_WORDS( x3, x7,x11,x15)
		QROUND_WORDS( x0, x5,x10,x15)
		QROUND_WORDS( x1, x6,x11,x12)
		QROUND_WORDS( x2, x7, x8,x13)
		QROUND_WORDS( x3, x4, x9,x14)
#endif
	}

	WRITE_XOR(ip, op, 0, v0+s0, v1+s1, v2+s2, v3+s3)
	s3 += ONE;
	WRITE_XOR(ip, op, 16, v4+s0, v5+s1, v6+s2, v7+s3)
	s3 += ONE;
#if VBPI > 2
	WRITE_XOR(ip, op, 32, v8+s0, v9+s1, v10+s2, v11+s3)
	s3 += ONE;
#endif
#if VBPI > 3
	WRITE_XOR(ip, op, 48, v12+s0, v13+s1, v14+s2, v15+s3)
	s3 += ONE;
#endif
	ip += VBPI*16;
	op += VBPI*16;
#if GPR_TOO
	op[0]  = REVW_BE(REVW_BE(ip[0])  ^ (x0  + kChaCha20Constants[0]));
	op[1]  = REVW_BE(REVW_BE(ip[1])  ^ (x1  + kChaCha20Constants[1]));
	op[2]  = REVW_BE(REVW_BE(ip[2])  ^ (x2  + kChaCha20Constants[2]));
	op[3]  = REVW_BE(REVW_BE(ip[3])  ^ (x3  + kChaCha20Constants[3]));
	op[4]  = REVW_BE(REVW_BE(ip[4])  ^ (x4  + kp[0]));
	op[5]  = REVW_BE(REVW_BE(ip[5])  ^ (x5  + kp[1]));
	op[6]  = REVW_BE(REVW_BE(ip[6])  ^ (x6  + kp[2]));
	op[7]  = REVW_BE(REVW_BE(ip[7])  ^ (x7  + kp[3]));
	op[8]  = REVW_BE(REVW_BE(ip[8])  ^ (x8  + kp[4]));
	op[9]  = REVW_BE(REVW_BE(ip[9])  ^ (x9  + kp[5]));
	op[10] = REVW_BE(REVW_BE(ip[10]) ^ (x10 + kp[6]));
	op[11] = REVW_BE(REVW_BE(ip[11]) ^ (x11 + kp[7]));
	op[12] = REVW_BE(REVW_BE(ip[12]) ^ (x12 + (inState->state[12])+((unsigned)(BPI*iters+(BPI-1)))));
	op[13] = REVW_BE(REVW_BE(ip[13]) ^ (x13 + (inState->state[13])));
	op[14] = REVW_BE(REVW_BE(ip[14]) ^ (x14 + np[0]));
	op[15] = REVW_BE(REVW_BE(ip[15]) ^ (x15 + np[1]));
	s3 += ONE;
	ip += 16;
	op += 16;
#endif
	}

	for (iters = inlen%(BPI*64)/64; iters != 0; iters--) {
		uint32x4_t v0 = s0, v1 = s1, v2 = s2, v3 = s3;
		for (i = CHACHA_RNDS/2; i; i--) {
			DQROUND_VECTORS(v0,v1,v2,v3);
		}
		WRITE_XOR(ip, op, 0, v0+s0, v1+s1, v2+s2, v3+s3)
		s3 += ONE;
		ip += 16;
		op += 16;
	}

	inlen = inlen % 64;
	if (inlen) {
		__attribute__ ((aligned (16))) uint32x4_t buf[4];
		uint32x4_t v0,v1,v2,v3;
		v0 = s0; v1 = s1; v2 = s2; v3 = s3;
		for (i = CHACHA_RNDS/2; i; i--) {
			DQROUND_VECTORS(v0,v1,v2,v3);
		}

		if (inlen >= 16) {
			STORE(op + 0, LOAD(ip + 0) ^ REVV_BE(v0 + s0));
			if (inlen >= 32) {
				STORE(op + 4, LOAD(ip + 4) ^ REVV_BE(v1 + s1));
				if (inlen >= 48) {
					STORE(op + 8, LOAD(ip + 8) ^ REVV_BE(v2 + s2));
					buf[3] = REVV_BE(v3 + s3);
				} else {
					buf[2] = REVV_BE(v2 + s2);
				}
			} else {
				buf[1] = REVV_BE(v1 + s1);
			}
		} else {
			buf[0] = REVV_BE(v0 + s0);
		}

		for (i=inlen & ~((size_t)15); i<inlen; i++) {
			((char *)op)[i] = ((char *)ip)[i] ^ ((char *)buf)[i];
		}
	}
	inState->state[12] = s3[0];
	inState->state[13] = s3[1];
}
#endif // CHACHA20_SIMD

#if( !EXCLUDE_UNIT_TESTS )

#include "PrintFUtils.h"
#include "StringUtils.h"
#include "TickUtils.h"

//===========================================================================================================================
//	chacha20_test
//===========================================================================================================================

OSStatus	chacha20_test( int inPerf );
OSStatus	chacha20_test( int inPerf )
{
	// Block Function Test Vector #1 from <https://tools.ietf.org/html/rfc7539>
	
	static const uint8_t		kBlockTest1_Key[] = 
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kBlockTest1_Nonce[] = 
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kBlockTest1_Input[] = 
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kBlockTest1_Output[] = 
	{
		0x76, 0xb8, 0xe0, 0xad, 0xa0, 0xf1, 0x3d, 0x90, 0x40, 0x5d, 0x6a, 0xe5, 0x53, 0x86, 0xbd, 0x28, 
		0xbd, 0xd2, 0x19, 0xb8, 0xa0, 0x8d, 0xed, 0x1a, 0xa8, 0x36, 0xef, 0xcc, 0x8b, 0x77, 0x0d, 0xc7, 
		0xda, 0x41, 0x59, 0x7c, 0x51, 0x57, 0x48, 0x8d, 0x77, 0x24, 0xe0, 0x3f, 0xb8, 0xd8, 0x4a, 0x37, 
		0x6a, 0x43, 0xb8, 0xf4, 0x15, 0x18, 0xa1, 0x1c, 0xc3, 0x87, 0xb6, 0x69, 0xb2, 0xee, 0x65, 0x86
	};
	
	// Block Function Test Vector #2 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kBlockTest2_Key[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kBlockTest2_Nonce[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kBlockTest2_Input[] = 
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kBlockTest2_Output[] =
	{
		0x9F, 0x07, 0xE7, 0xBE, 0x55, 0x51, 0x38, 0x7A, 0x98, 0xBA, 0x97, 0x7C, 0x73, 0x2D, 0x08, 0x0D, 
		0xCB, 0x0F, 0x29, 0xA0, 0x48, 0xE3, 0x65, 0x69, 0x12, 0xC6, 0x53, 0x3E, 0x32, 0xEE, 0x7A, 0xED, 
		0x29, 0xB7, 0x21, 0x76, 0x9C, 0xE6, 0x4E, 0x43, 0xD5, 0x71, 0x33, 0xB0, 0x74, 0xD8, 0x39, 0xD5, 
		0x31, 0xED, 0x1F, 0x28, 0x51, 0x0A, 0xFB, 0x45, 0xAC, 0xE1, 0x0A, 0x1F, 0x4B, 0x79, 0x4D, 0x6F
	};
	
	// Block Function Test Vector #3 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kBlockTest3_Key[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
	};
	static const uint8_t		kBlockTest3_Nonce[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kBlockTest3_Input[] = 
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kBlockTest3_Output[] =
	{
		0x3A, 0xEB, 0x52, 0x24, 0xEC, 0xF8, 0x49, 0x92, 0x9B, 0x9D, 0x82, 0x8D, 0xB1, 0xCE, 0xD4, 0xDD, 
		0x83, 0x20, 0x25, 0xE8, 0x01, 0x8B, 0x81, 0x60, 0xB8, 0x22, 0x84, 0xF3, 0xC9, 0x49, 0xAA, 0x5A, 
		0x8E, 0xCA, 0x00, 0xBB, 0xB4, 0xA7, 0x3B, 0xDA, 0xD1, 0x92, 0xB5, 0xC4, 0x2F, 0x73, 0xF2, 0xFD, 
		0x4E, 0x27, 0x36, 0x44, 0xC8, 0xB3, 0x61, 0x25, 0xA6, 0x4A, 0xDD, 0xEB, 0x00, 0x6C, 0x13, 0xA0
	};
	
	// Block Function Test Vector #4 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kBlockTest4_Key[] =
	{
		0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kBlockTest4_Nonce[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kBlockTest4_Input[] = 
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kBlockTest4_Output[] =
	{
		0x72, 0xD5, 0x4D, 0xFB, 0xF1, 0x2E, 0xC4, 0x4B, 0x36, 0x26, 0x92, 0xDF, 0x94, 0x13, 0x7F, 0x32, 
		0x8F, 0xEA, 0x8D, 0xA7, 0x39, 0x90, 0x26, 0x5E, 0xC1, 0xBB, 0xBE, 0xA1, 0xAE, 0x9A, 0xF0, 0xCA, 
		0x13, 0xB2, 0x5A, 0xA2, 0x6C, 0xB4, 0xA6, 0x48, 0xCB, 0x9B, 0x9D, 0x1B, 0xE6, 0x5B, 0x2C, 0x09, 
		0x24, 0xA6, 0x6C, 0x54, 0xD5, 0x45, 0xEC, 0x1B, 0x73, 0x74, 0xF4, 0x87, 0x2E, 0x99, 0xF0, 0x96
	};
	
	// Block Function Test Vector #5 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kBlockTest5_Key[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kBlockTest5_Nonce[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02
	};
	static const uint8_t		kBlockTest5_Input[] = 
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kBlockTest5_Output[] =
	{
		0xC2, 0xC6, 0x4D, 0x37, 0x8C, 0xD5, 0x36, 0x37, 0x4A, 0xE2, 0x04, 0xB9, 0xEF, 0x93, 0x3F, 0xCD, 
		0x1A, 0x8B, 0x22, 0x88, 0xB3, 0xDF, 0xA4, 0x96, 0x72, 0xAB, 0x76, 0x5B, 0x54, 0xEE, 0x27, 0xC7, 
		0x8A, 0x97, 0x0E, 0x0E, 0x95, 0x5C, 0x14, 0xF3, 0xA8, 0x8E, 0x74, 0x1B, 0x97, 0xC2, 0x86, 0xF7, 
		0x5F, 0x8F, 0xC2, 0x99, 0xE8, 0x14, 0x83, 0x62, 0xFA, 0x19, 0x8A, 0x39, 0x53, 0x1B, 0xED, 0x6D
	};
	
	// Note: Encryption Test Vector #1 from <https://tools.ietf.org/html/rfc7539> is the same Test Vector #1 above.
	
	// Encryption Test Vector #2 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kEncryptionTest2_Key[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
	};
	static const uint8_t		kEncryptionTest2_Nonce[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02
	};
	static const uint8_t		kEncryptionTest2_Input[] =
	{
		0x41, 0x6E, 0x79, 0x20, 0x73, 0x75, 0x62, 0x6D, 0x69, 0x73, 0x73, 0x69, 0x6F, 0x6E, 0x20, 0x74, 
		0x6F, 0x20, 0x74, 0x68, 0x65, 0x20, 0x49, 0x45, 0x54, 0x46, 0x20, 0x69, 0x6E, 0x74, 0x65, 0x6E, 
		0x64, 0x65, 0x64, 0x20, 0x62, 0x79, 0x20, 0x74, 0x68, 0x65, 0x20, 0x43, 0x6F, 0x6E, 0x74, 0x72, 
		0x69, 0x62, 0x75, 0x74, 0x6F, 0x72, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x70, 0x75, 0x62, 0x6C, 0x69, 
		0x63, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x61, 0x73, 0x20, 0x61, 0x6C, 0x6C, 0x20, 0x6F, 0x72, 
		0x20, 0x70, 0x61, 0x72, 0x74, 0x20, 0x6F, 0x66, 0x20, 0x61, 0x6E, 0x20, 0x49, 0x45, 0x54, 0x46, 
		0x20, 0x49, 0x6E, 0x74, 0x65, 0x72, 0x6E, 0x65, 0x74, 0x2D, 0x44, 0x72, 0x61, 0x66, 0x74, 0x20, 
		0x6F, 0x72, 0x20, 0x52, 0x46, 0x43, 0x20, 0x61, 0x6E, 0x64, 0x20, 0x61, 0x6E, 0x79, 0x20, 0x73, 
		0x74, 0x61, 0x74, 0x65, 0x6D, 0x65, 0x6E, 0x74, 0x20, 0x6D, 0x61, 0x64, 0x65, 0x20, 0x77, 0x69, 
		0x74, 0x68, 0x69, 0x6E, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6F, 0x6E, 0x74, 0x65, 0x78, 0x74, 
		0x20, 0x6F, 0x66, 0x20, 0x61, 0x6E, 0x20, 0x49, 0x45, 0x54, 0x46, 0x20, 0x61, 0x63, 0x74, 0x69, 
		0x76, 0x69, 0x74, 0x79, 0x20, 0x69, 0x73, 0x20, 0x63, 0x6F, 0x6E, 0x73, 0x69, 0x64, 0x65, 0x72, 
		0x65, 0x64, 0x20, 0x61, 0x6E, 0x20, 0x22, 0x49, 0x45, 0x54, 0x46, 0x20, 0x43, 0x6F, 0x6E, 0x74, 
		0x72, 0x69, 0x62, 0x75, 0x74, 0x69, 0x6F, 0x6E, 0x22, 0x2E, 0x20, 0x53, 0x75, 0x63, 0x68, 0x20, 
		0x73, 0x74, 0x61, 0x74, 0x65, 0x6D, 0x65, 0x6E, 0x74, 0x73, 0x20, 0x69, 0x6E, 0x63, 0x6C, 0x75, 
		0x64, 0x65, 0x20, 0x6F, 0x72, 0x61, 0x6C, 0x20, 0x73, 0x74, 0x61, 0x74, 0x65, 0x6D, 0x65, 0x6E, 
		0x74, 0x73, 0x20, 0x69, 0x6E, 0x20, 0x49, 0x45, 0x54, 0x46, 0x20, 0x73, 0x65, 0x73, 0x73, 0x69, 
		0x6F, 0x6E, 0x73, 0x2C, 0x20, 0x61, 0x73, 0x20, 0x77, 0x65, 0x6C, 0x6C, 0x20, 0x61, 0x73, 0x20, 
		0x77, 0x72, 0x69, 0x74, 0x74, 0x65, 0x6E, 0x20, 0x61, 0x6E, 0x64, 0x20, 0x65, 0x6C, 0x65, 0x63, 
		0x74, 0x72, 0x6F, 0x6E, 0x69, 0x63, 0x20, 0x63, 0x6F, 0x6D, 0x6D, 0x75, 0x6E, 0x69, 0x63, 0x61, 
		0x74, 0x69, 0x6F, 0x6E, 0x73, 0x20, 0x6D, 0x61, 0x64, 0x65, 0x20, 0x61, 0x74, 0x20, 0x61, 0x6E, 
		0x79, 0x20, 0x74, 0x69, 0x6D, 0x65, 0x20, 0x6F, 0x72, 0x20, 0x70, 0x6C, 0x61, 0x63, 0x65, 0x2C, 
		0x20, 0x77, 0x68, 0x69, 0x63, 0x68, 0x20, 0x61, 0x72, 0x65, 0x20, 0x61, 0x64, 0x64, 0x72, 0x65, 
		0x73, 0x73, 0x65, 0x64, 0x20, 0x74, 0x6F
	};
	static const uint8_t		kEncryptionTest2_Output[] =
	{
		0xA3, 0xFB, 0xF0, 0x7D, 0xF3, 0xFA, 0x2F, 0xDE, 0x4F, 0x37, 0x6C, 0xA2, 0x3E, 0x82, 0x73, 0x70, 
		0x41, 0x60, 0x5D, 0x9F, 0x4F, 0x4F, 0x57, 0xBD, 0x8C, 0xFF, 0x2C, 0x1D, 0x4B, 0x79, 0x55, 0xEC, 
		0x2A, 0x97, 0x94, 0x8B, 0xD3, 0x72, 0x29, 0x15, 0xC8, 0xF3, 0xD3, 0x37, 0xF7, 0xD3, 0x70, 0x05, 
		0x0E, 0x9E, 0x96, 0xD6, 0x47, 0xB7, 0xC3, 0x9F, 0x56, 0xE0, 0x31, 0xCA, 0x5E, 0xB6, 0x25, 0x0D, 
		0x40, 0x42, 0xE0, 0x27, 0x85, 0xEC, 0xEC, 0xFA, 0x4B, 0x4B, 0xB5, 0xE8, 0xEA, 0xD0, 0x44, 0x0E, 
		0x20, 0xB6, 0xE8, 0xDB, 0x09, 0xD8, 0x81, 0xA7, 0xC6, 0x13, 0x2F, 0x42, 0x0E, 0x52, 0x79, 0x50, 
		0x42, 0xBD, 0xFA, 0x77, 0x73, 0xD8, 0xA9, 0x05, 0x14, 0x47, 0xB3, 0x29, 0x1C, 0xE1, 0x41, 0x1C, 
		0x68, 0x04, 0x65, 0x55, 0x2A, 0xA6, 0xC4, 0x05, 0xB7, 0x76, 0x4D, 0x5E, 0x87, 0xBE, 0xA8, 0x5A, 
		0xD0, 0x0F, 0x84, 0x49, 0xED, 0x8F, 0x72, 0xD0, 0xD6, 0x62, 0xAB, 0x05, 0x26, 0x91, 0xCA, 0x66, 
		0x42, 0x4B, 0xC8, 0x6D, 0x2D, 0xF8, 0x0E, 0xA4, 0x1F, 0x43, 0xAB, 0xF9, 0x37, 0xD3, 0x25, 0x9D, 
		0xC4, 0xB2, 0xD0, 0xDF, 0xB4, 0x8A, 0x6C, 0x91, 0x39, 0xDD, 0xD7, 0xF7, 0x69, 0x66, 0xE9, 0x28, 
		0xE6, 0x35, 0x55, 0x3B, 0xA7, 0x6C, 0x5C, 0x87, 0x9D, 0x7B, 0x35, 0xD4, 0x9E, 0xB2, 0xE6, 0x2B, 
		0x08, 0x71, 0xCD, 0xAC, 0x63, 0x89, 0x39, 0xE2, 0x5E, 0x8A, 0x1E, 0x0E, 0xF9, 0xD5, 0x28, 0x0F, 
		0xA8, 0xCA, 0x32, 0x8B, 0x35, 0x1C, 0x3C, 0x76, 0x59, 0x89, 0xCB, 0xCF, 0x3D, 0xAA, 0x8B, 0x6C, 
		0xCC, 0x3A, 0xAF, 0x9F, 0x39, 0x79, 0xC9, 0x2B, 0x37, 0x20, 0xFC, 0x88, 0xDC, 0x95, 0xED, 0x84, 
		0xA1, 0xBE, 0x05, 0x9C, 0x64, 0x99, 0xB9, 0xFD, 0xA2, 0x36, 0xE7, 0xE8, 0x18, 0xB0, 0x4B, 0x0B, 
		0xC3, 0x9C, 0x1E, 0x87, 0x6B, 0x19, 0x3B, 0xFE, 0x55, 0x69, 0x75, 0x3F, 0x88, 0x12, 0x8C, 0xC0, 
		0x8A, 0xAA, 0x9B, 0x63, 0xD1, 0xA1, 0x6F, 0x80, 0xEF, 0x25, 0x54, 0xD7, 0x18, 0x9C, 0x41, 0x1F, 
		0x58, 0x69, 0xCA, 0x52, 0xC5, 0xB8, 0x3F, 0xA3, 0x6F, 0xF2, 0x16, 0xB9, 0xC1, 0xD3, 0x00, 0x62, 
		0xBE, 0xBC, 0xFD, 0x2D, 0xC5, 0xBC, 0xE0, 0x91, 0x19, 0x34, 0xFD, 0xA7, 0x9A, 0x86, 0xF6, 0xE6, 
		0x98, 0xCE, 0xD7, 0x59, 0xC3, 0xFF, 0x9B, 0x64, 0x77, 0x33, 0x8F, 0x3D, 0xA4, 0xF9, 0xCD, 0x85, 
		0x14, 0xEA, 0x99, 0x82, 0xCC, 0xAF, 0xB3, 0x41, 0xB2, 0x38, 0x4D, 0xD9, 0x02, 0xF3, 0xD1, 0xAB, 
		0x7A, 0xC6, 0x1D, 0xD2, 0x9C, 0x6F, 0x21, 0xBA, 0x5B, 0x86, 0x2F, 0x37, 0x30, 0xE3, 0x7C, 0xFD, 
		0xC4, 0xFD, 0x80, 0x6C, 0x22, 0xF2, 0x21
	};
	
	// Encryption Test Vector #3 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kEncryptionTest3_Key[] =
	{
		0x1C, 0x92, 0x40, 0xA5, 0xEB, 0x55, 0xD3, 0x8A, 0xF3, 0x33, 0x88, 0x86, 0x04, 0xF6, 0xB5, 0xF0, 
		0x47, 0x39, 0x17, 0xC1, 0x40, 0x2B, 0x80, 0x09, 0x9D, 0xCA, 0x5C, 0xBC, 0x20, 0x70, 0x75, 0xC0
	};
	static const uint8_t		kEncryptionTest3_Nonce[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02
	};
	static const uint8_t		kEncryptionTest3_Input[] =
	{
		0x27, 0x54, 0x77, 0x61, 0x73, 0x20, 0x62, 0x72, 0x69, 0x6C, 0x6C, 0x69, 0x67, 0x2C, 0x20, 0x61, 
		0x6E, 0x64, 0x20, 0x74, 0x68, 0x65, 0x20, 0x73, 0x6C, 0x69, 0x74, 0x68, 0x79, 0x20, 0x74, 0x6F, 
		0x76, 0x65, 0x73, 0x0A, 0x44, 0x69, 0x64, 0x20, 0x67, 0x79, 0x72, 0x65, 0x20, 0x61, 0x6E, 0x64, 
		0x20, 0x67, 0x69, 0x6D, 0x62, 0x6C, 0x65, 0x20, 0x69, 0x6E, 0x20, 0x74, 0x68, 0x65, 0x20, 0x77, 
		0x61, 0x62, 0x65, 0x3A, 0x0A, 0x41, 0x6C, 0x6C, 0x20, 0x6D, 0x69, 0x6D, 0x73, 0x79, 0x20, 0x77, 
		0x65, 0x72, 0x65, 0x20, 0x74, 0x68, 0x65, 0x20, 0x62, 0x6F, 0x72, 0x6F, 0x67, 0x6F, 0x76, 0x65, 
		0x73, 0x2C, 0x0A, 0x41, 0x6E, 0x64, 0x20, 0x74, 0x68, 0x65, 0x20, 0x6D, 0x6F, 0x6D, 0x65, 0x20, 
		0x72, 0x61, 0x74, 0x68, 0x73, 0x20, 0x6F, 0x75, 0x74, 0x67, 0x72, 0x61, 0x62, 0x65, 0x2E
	};
	static const uint8_t		kEncryptionTest3_Output[] =
	{
		0x62, 0xE6, 0x34, 0x7F, 0x95, 0xED, 0x87, 0xA4, 0x5F, 0xFA, 0xE7, 0x42, 0x6F, 0x27, 0xA1, 0xDF, 
		0x5F, 0xB6, 0x91, 0x10, 0x04, 0x4C, 0x0D, 0x73, 0x11, 0x8E, 0xFF, 0xA9, 0x5B, 0x01, 0xE5, 0xCF, 
		0x16, 0x6D, 0x3D, 0xF2, 0xD7, 0x21, 0xCA, 0xF9, 0xB2, 0x1E, 0x5F, 0xB1, 0x4C, 0x61, 0x68, 0x71, 
		0xFD, 0x84, 0xC5, 0x4F, 0x9D, 0x65, 0xB2, 0x83, 0x19, 0x6C, 0x7F, 0xE4, 0xF6, 0x05, 0x53, 0xEB, 
		0xF3, 0x9C, 0x64, 0x02, 0xC4, 0x22, 0x34, 0xE3, 0x2A, 0x35, 0x6B, 0x3E, 0x76, 0x43, 0x12, 0xA6, 
		0x1A, 0x55, 0x32, 0x05, 0x57, 0x16, 0xEA, 0xD6, 0x96, 0x25, 0x68, 0xF8, 0x7D, 0x3F, 0x3F, 0x77, 
		0x04, 0xC6, 0xA8, 0xD1, 0xBC, 0xD1, 0xBF, 0x4D, 0x50, 0xD6, 0x15, 0x4B, 0x6D, 0xA7, 0x31, 0xB1, 
		0x87, 0xB5, 0x8D, 0xFD, 0x72, 0x8A, 0xFA, 0x36, 0x75, 0x7A, 0x79, 0x7A, 0xC1, 0x88, 0xD1
	};
	
	OSStatus			err;
	chacha20_state		state;
	uint32_t			a, b, c, d;
	uint8_t				result[ 512 ];
	size_t				i, n;
	uint64_t			ticks;
	double				secs;
	size_t				len;
	uint8_t *			buf;
	
	// Quarter round test from <https://tools.ietf.org/html/rfc7539>.
	
	a = 0x11111111;
	b = 0x01020304;
	c = 0x9b8d6f43;
	d = 0x01234567;
	CHACHA20_QUARTERROUND( a, b, c, d );
	require_action( a == 0xea2a92f4, exit, err = -1 );
	require_action( b == 0xcb1cf8ce, exit, err = -1 );
	require_action( c == 0x4581472e, exit, err = -1 );
	require_action( d == 0x5881c4bb, exit, err = -1 );
	
	// Test vector from <https://tools.ietf.org/html/rfc7539>.
	
	memset( result, 0, sizeof( result ) );
	chacha20_all_64x64( (const uint8_t *) 
		"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
		"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f", 
		(const uint8_t *) "\x00\x00\x00\x4a\x00\x00\x00\x00", 1, 
		"Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.", 
		114, result );
	require_action( memcmp( result, 
		"\x6e\x2e\x35\x9a\x25\x68\xf9\x80\x41\xba\x07\x28\xdd\x0d\x69\x81"
		"\xe9\x7e\x7a\xec\x1d\x43\x60\xc2\x0a\x27\xaf\xcc\xfd\x9f\xae\x0b"
		"\xf9\x1b\x65\xc5\x52\x47\x33\xab\x8f\x59\x3d\xab\xcd\x62\xb3\x57"
		"\x16\x39\xd6\x24\xe6\x51\x52\xab\x8f\x53\x0c\x35\x9f\x08\x61\xd8"
		"\x07\xca\x0d\xbf\x50\x0d\x6a\x61\x56\xa3\x8e\x08\x8a\x22\xb6\x5e"
		"\x52\xbc\x51\x4d\x16\xcc\xf8\x06\x81\x8c\xe9\x1a\xb7\x79\x37\x36"
		"\x5a\xf9\x0b\xbf\x74\xa3\x5b\xe6\xb4\x0b\x8e\xed\xf2\x78\x5e\x42"
		"\x87\x4d", 114 ) == 0, exit, err = -1 );
	
	// Block Test Vector #1
	
	memset( result, 0, sizeof( result ) );
	chacha20_all_64x64( kBlockTest1_Key, kBlockTest1_Nonce, 0, kBlockTest1_Input, sizeof( kBlockTest1_Input ), result );
	require_action( memcmp( result, kBlockTest1_Output, sizeof( kBlockTest1_Output ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	chacha20_init_64x64( &state, kBlockTest1_Key, kBlockTest1_Nonce, 0 );
	n = 0;
	for( i = 0; i < sizeof( kBlockTest1_Output ); ++i )
	{
		n += chacha20_update( &state, &kBlockTest1_Input[ i ], 1, &result[ n ] );
	}
	n += chacha20_final( &state, &result[ n ] );
	require_action( n == sizeof( kBlockTest1_Output ), exit, err = -1 );
	require_action( memcmp( result, kBlockTest1_Output, sizeof( kBlockTest1_Output ) ) == 0, exit, err = -1 );
	
	// Block Test Vector #2
	
	memset( result, 0, sizeof( result ) );
	chacha20_all_64x64( kBlockTest2_Key, kBlockTest2_Nonce, 1, kBlockTest2_Input, sizeof( kBlockTest2_Output ), result );
	require_action( memcmp( result, kBlockTest2_Output, sizeof( kBlockTest2_Output ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	chacha20_init_64x64( &state, kBlockTest2_Key, kBlockTest2_Nonce, 1 );
	n = 0;
	for( i = 0; i < sizeof( kBlockTest2_Output ); ++i )
	{
		n += chacha20_update( &state, &kBlockTest2_Input[ i ], 1, &result[ n ] );
	}
	n += chacha20_final( &state, &result[ n ] );
	require_action( n == sizeof( kBlockTest2_Output ), exit, err = -1 );
	require_action( memcmp( result, kBlockTest2_Output, sizeof( kBlockTest2_Output ) ) == 0, exit, err = -1 );
	
	// Block Test Vector #3
	
	memset( result, 0, sizeof( result ) );
	chacha20_all_64x64( kBlockTest3_Key, kBlockTest3_Nonce, 1, kBlockTest3_Input, sizeof( kBlockTest3_Output ), result );
	require_action( memcmp( result, kBlockTest3_Output, sizeof( kBlockTest3_Output ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	chacha20_init_64x64( &state, kBlockTest3_Key, kBlockTest3_Nonce, 1 );
	n = 0;
	for( i = 0; i < sizeof( kBlockTest3_Output ); ++i )
	{
		n += chacha20_update( &state, &kBlockTest3_Input[ i ], 1, &result[ n ] );
	}
	n += chacha20_final( &state, &result[ n ] );
	require_action( n == sizeof( kBlockTest3_Output ), exit, err = -1 );
	require_action( memcmp( result, kBlockTest3_Output, sizeof( kBlockTest3_Output ) ) == 0, exit, err = -1 );
	
	// Block Test Vector #4
	
	memset( result, 0, sizeof( result ) );
	chacha20_all_64x64( kBlockTest4_Key, kBlockTest4_Nonce, 2, kBlockTest4_Input, sizeof( kBlockTest4_Output ), result );
	require_action( memcmp( result, kBlockTest4_Output, sizeof( kBlockTest4_Output ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	chacha20_init_64x64( &state, kBlockTest4_Key, kBlockTest4_Nonce, 2 );
	n = 0;
	for( i = 0; i < sizeof( kBlockTest4_Output ); ++i )
	{
		n += chacha20_update( &state, &kBlockTest4_Input[ i ], 1, &result[ n ] );
	}
	n += chacha20_final( &state, &result[ n ] );
	require_action( n == sizeof( kBlockTest4_Output ), exit, err = -1 );
	require_action( memcmp( result, kBlockTest4_Output, sizeof( kBlockTest4_Output ) ) == 0, exit, err = -1 );
	
	// Block Test Vector #5
	
	memset( result, 0, sizeof( result ) );
	chacha20_all_96x32( kBlockTest5_Key, kBlockTest5_Nonce, 0, kBlockTest5_Input, sizeof( kBlockTest5_Output ), result );
	require_action( memcmp( result, kBlockTest5_Output, sizeof( kBlockTest5_Output ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	chacha20_init_96x32( &state, kBlockTest5_Key, kBlockTest5_Nonce, 0 );
	n = 0;
	for( i = 0; i < sizeof( kBlockTest5_Output ); ++i )
	{
		n += chacha20_update( &state, &kBlockTest5_Input[ i ], 1, &result[ n ] );
	}
	n += chacha20_final( &state, &result[ n ] );
	require_action( n == sizeof( kBlockTest5_Output ), exit, err = -1 );
	require_action( memcmp( result, kBlockTest5_Output, sizeof( kBlockTest5_Output ) ) == 0, exit, err = -1 );
	
	// Note: Encryption Test Vector #1 is the same Block Test Vector #1 above.
	
	// Encryption Test Vector #2
	
	check_compile_time_code( sizeof( kEncryptionTest2_Input ) == sizeof( kEncryptionTest2_Output ) );
	memset( result, 0, sizeof( result ) );
	chacha20_all_64x64( kEncryptionTest2_Key, kEncryptionTest2_Nonce, 1, kEncryptionTest2_Input, 
		sizeof( kEncryptionTest2_Output ), result );
	require_action( memcmp( result, kEncryptionTest2_Output, sizeof( kEncryptionTest2_Output ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	chacha20_init_64x64( &state, kEncryptionTest2_Key, kEncryptionTest2_Nonce, 1 );
	n = 0;
	for( i = 0; i < sizeof( kEncryptionTest2_Output ); ++i )
	{
		n += chacha20_update( &state, &kEncryptionTest2_Input[ i ], 1, &result[ n ] );
	}
	n += chacha20_final( &state, &result[ n ] );
	require_action( n == sizeof( kEncryptionTest2_Output ), exit, err = -1 );
	require_action( memcmp( result, kEncryptionTest2_Output, sizeof( kEncryptionTest2_Output ) ) == 0, exit, err = -1 );
	
	// Encryption Test Vector #3
	
	check_compile_time_code( sizeof( kEncryptionTest3_Input ) == sizeof( kEncryptionTest3_Output ) );
	memset( result, 0, sizeof( result ) );
	chacha20_all_96x32( kEncryptionTest3_Key, kEncryptionTest3_Nonce, 42, kEncryptionTest3_Input, 
		sizeof( kEncryptionTest3_Output ), result );
	require_action( memcmp( result, kEncryptionTest3_Output, sizeof( kEncryptionTest3_Output ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	chacha20_init_96x32( &state, kEncryptionTest3_Key, kEncryptionTest3_Nonce, 42 );
	n = 0;
	for( i = 0; i < sizeof( kEncryptionTest3_Output ); ++i )
	{
		n += chacha20_update( &state, &kEncryptionTest3_Input[ i ], 1, &result[ n ] );
	}
	n += chacha20_final( &state, &result[ n ] );
	require_action( n == sizeof( kEncryptionTest3_Output ), exit, err = -1 );
	require_action( memcmp( result, kEncryptionTest3_Output, sizeof( kEncryptionTest3_Output ) ) == 0, exit, err = -1 );
	
	if( inPerf )
	{
		// Small performance test.
		
		len = 1500;
		buf = (uint8_t *) malloc( len );
		require_action( buf, exit, err = kNoMemoryErr );
		memset( buf, 'a', len );
		
		ticks = UpTicks();
		for( i = 0; i < 10000; ++i )
		{
			chacha20_all_64x64( kBlockTest1_Key, kBlockTest1_Nonce, 0, buf, len, buf );
		}
		ticks = UpTicks() - ticks;
		secs = ( (double) ticks ) / UpTicksPerSecond();
		FPrintF( stderr, "\tchacha20 (%zu bytes): %f (%f µs, %.2f MB/sec): %.3H...\n", 
			len, secs, ( 1000000 * secs ) / i, ( i * len ) / ( secs * 1048576.0 ), buf, 16, 16 );
		free( buf );
		
		// Medium performance test.
		
		len = 50000;
		buf = (uint8_t *) malloc( len );
		require_action( buf, exit, err = kNoMemoryErr );
		memset( buf, 'a', len );
		
		ticks = UpTicks();
		for( i = 0; i < 1000; ++i )
		{
			chacha20_all_64x64( kBlockTest1_Key, kBlockTest1_Nonce, 0, buf, len, buf );
		}
		ticks = UpTicks() - ticks;
		secs = ( (double) ticks ) / UpTicksPerSecond();
		FPrintF( stderr, "\tchacha20 (%zu bytes): %f (%f µs, %.2f MB/sec): %.3H\n", 
			len, secs, ( 1000000 * secs ) / i, ( i * len ) / ( secs * 1048576.0 ), buf, 16, 16 );
		free( buf );
		
		// Big performance test.
		
		len = 5000000;
		buf = (uint8_t *) malloc( len );
		require_action( buf, exit, err = kNoMemoryErr );
		memset( buf, 'a', len );
		
		ticks = UpTicks();
		for( i = 0; i < 10; ++i )
		{
			chacha20_all_64x64( kBlockTest1_Key, kBlockTest1_Nonce, 0, buf, len, buf );
		}
		ticks = UpTicks() - ticks;
		secs = ( (double) ticks ) / UpTicksPerSecond();
		FPrintF( stderr, "\tchacha20 (%zu bytes): %f (%f µs, %.2f MB/sec): %.3H\n", 
			len, secs, ( 1000000 * secs ) / i, ( i * len ) / ( secs * 1048576.0 ), buf, 16, 16 );
		free( buf );
	}
	err = kNoErr;
	
exit:
	printf( "chacha20_test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS

#if 0
#pragma mark -
#pragma mark == poly1305 ==
#endif

//===========================================================================================================================
//	poly1305
//
//	Based on floodyberry's Poly1305 code: <https://github.com/floodyberry/poly1305-donna>.
//	Based on DJB's Poly1305: <http://cr.yp.to/mac.html>.
//===========================================================================================================================

#define U8TO32_LE( PTR )			ReadLittle32( (PTR) )
#define U32TO8_LE( PTR, VALUE )		WriteLittle32( (PTR), (VALUE) )
#define mul32x32_64(a,b)			((uint64_t)(a) * (b))

static void _poly1305_update(poly1305_state *state, const uint8_t *in, size_t len);

void poly1305_init(poly1305_state *state, const uint8_t key[32])
{
	uint32_t t0,t1,t2,t3;
	size_t i;

	t0 = U8TO32_LE(key+0);
	t1 = U8TO32_LE(key+4);
	t2 = U8TO32_LE(key+8);
	t3 = U8TO32_LE(key+12);

	/* precompute multipliers */
	state->r0 = t0 & 0x3ffffff; t0 >>= 26; t0 |= t1 << 6;
	state->r1 = t0 & 0x3ffff03; t1 >>= 20; t1 |= t2 << 12;
	state->r2 = t1 & 0x3ffc0ff; t2 >>= 14; t2 |= t3 << 18;
	state->r3 = t2 & 0x3f03fff; t3 >>= 8;
	state->r4 = t3 & 0x00fffff;

	state->s1 = state->r1 * 5;
	state->s2 = state->r2 * 5;
	state->s3 = state->r3 * 5;
	state->s4 = state->r4 * 5;

	/* init state */
	state->h0 = 0;
	state->h1 = 0;
	state->h2 = 0;
	state->h3 = 0;
	state->h4 = 0;

	state->buf_used = 0;
	for (i = 0; i < 16; ++i)
		state->key[i] = key[i + 16];
}

void poly1305_update(poly1305_state *state, const uint8_t *in, size_t in_len)
{
	size_t i, n;

	if (state->buf_used) {
		n = 16 - state->buf_used;
		if (n > in_len)
			n = in_len;
		for (i = 0; i < n; i++)
			state->buf[state->buf_used + i] = in[i];
		state->buf_used += n;
		in_len -= n;
		in += n;

		if (state->buf_used == 16) {
			_poly1305_update(state, state->buf, 16);
			state->buf_used = 0;
		}
	}

	if (in_len >= 16) {
		n = in_len & ~((size_t)0xf);
		_poly1305_update(state, in, n);
		in += n;
		in_len &= 0xf;
	}

	if (in_len) {
		for (i = 0; i < in_len; i++)
			state->buf[i] = in[i];
		state->buf_used = in_len;
	}
}

void poly1305_final(poly1305_state *state, uint8_t mac[16])
{
	uint64_t f0,f1,f2,f3;
	uint32_t g0,g1,g2,g3,g4;
	uint32_t b, nb;

	if (state->buf_used)
		_poly1305_update(state, state->buf, state->buf_used);

	                    b = state->h0 >> 26; state->h0 = state->h0 & 0x3ffffff;
	state->h1 +=     b; b = state->h1 >> 26; state->h1 = state->h1 & 0x3ffffff;
	state->h2 +=     b; b = state->h2 >> 26; state->h2 = state->h2 & 0x3ffffff;
	state->h3 +=     b; b = state->h3 >> 26; state->h3 = state->h3 & 0x3ffffff;
	state->h4 +=     b; b = state->h4 >> 26; state->h4 = state->h4 & 0x3ffffff;
	state->h0 += b * 5;

	g0 = state->h0 + 5; b = g0 >> 26; g0 &= 0x3ffffff;
	g1 = state->h1 + b; b = g1 >> 26; g1 &= 0x3ffffff;
	g2 = state->h2 + b; b = g2 >> 26; g2 &= 0x3ffffff;
	g3 = state->h3 + b; b = g3 >> 26; g3 &= 0x3ffffff;
	g4 = state->h4 + b - (1 << 26);

	b = (g4 >> 31) - 1;
	nb = ~b;
	state->h0 = (state->h0 & nb) | (g0 & b);
	state->h1 = (state->h1 & nb) | (g1 & b);
	state->h2 = (state->h2 & nb) | (g2 & b);
	state->h3 = (state->h3 & nb) | (g3 & b);
	state->h4 = (state->h4 & nb) | (g4 & b);

	f0 = ((state->h0      ) | (state->h1 << 26)) + (uint64_t)U8TO32_LE(&state->key[0]);
	f1 = ((state->h1 >>  6) | (state->h2 << 20)) + (uint64_t)U8TO32_LE(&state->key[4]);
	f2 = ((state->h2 >> 12) | (state->h3 << 14)) + (uint64_t)U8TO32_LE(&state->key[8]);
	f3 = ((state->h3 >> 18) | (state->h4 <<  8)) + (uint64_t)U8TO32_LE(&state->key[12]);

	static_analyzer_mem_zeroed(mac, 16); // Remove when <radar:15309659> is fixed.
	U32TO8_LE(&mac[ 0], f0); f1 += (f0 >> 32);
	U32TO8_LE(&mac[ 4], f1); f2 += (f1 >> 32);
	U32TO8_LE(&mac[ 8], f2); f3 += (f2 >> 32);
	U32TO8_LE(&mac[12], f3);
}

static void _poly1305_update(poly1305_state *state, const uint8_t *in, size_t len)
{
	uint32_t t0,t1,t2,t3;
	uint64_t t[5];
	uint32_t b;
	uint64_t c;
	size_t j;
	uint8_t mp[16];

	if (len < 16)
		goto poly1305_donna_atmost15bytes;

poly1305_donna_16bytes:
	t0 = U8TO32_LE(in);
	t1 = U8TO32_LE(in+4);
	t2 = U8TO32_LE(in+8);
	t3 = U8TO32_LE(in+12);

	in += 16;
	len -= 16;

	state->h0 += t0 & 0x3ffffff;
	state->h1 += ((((uint64_t)t1 << 32) | t0) >> 26) & 0x3ffffff;
	state->h2 += ((((uint64_t)t2 << 32) | t1) >> 20) & 0x3ffffff;
	state->h3 += ((((uint64_t)t3 << 32) | t2) >> 14) & 0x3ffffff;
	state->h4 += (t3 >> 8) | (1 << 24);

poly1305_donna_mul:
	t[0] = mul32x32_64(state->h0,state->r0) +
	       mul32x32_64(state->h1,state->s4) +
	       mul32x32_64(state->h2,state->s3) +
	       mul32x32_64(state->h3,state->s2) +
	       mul32x32_64(state->h4,state->s1);
	t[1] = mul32x32_64(state->h0,state->r1) +
	       mul32x32_64(state->h1,state->r0) +
	       mul32x32_64(state->h2,state->s4) +
	       mul32x32_64(state->h3,state->s3) +
	       mul32x32_64(state->h4,state->s2);
	t[2] = mul32x32_64(state->h0,state->r2) +
	       mul32x32_64(state->h1,state->r1) +
	       mul32x32_64(state->h2,state->r0) +
	       mul32x32_64(state->h3,state->s4) +
	       mul32x32_64(state->h4,state->s3);
	t[3] = mul32x32_64(state->h0,state->r3) +
	       mul32x32_64(state->h1,state->r2) +
	       mul32x32_64(state->h2,state->r1) +
	       mul32x32_64(state->h3,state->r0) +
	       mul32x32_64(state->h4,state->s4);
	t[4] = mul32x32_64(state->h0,state->r4) +
	       mul32x32_64(state->h1,state->r3) +
	       mul32x32_64(state->h2,state->r2) +
	       mul32x32_64(state->h3,state->r1) +
	       mul32x32_64(state->h4,state->r0);

	           state->h0 = (uint32_t)t[0] & 0x3ffffff; c =           (t[0] >> 26);
	t[1] += c; state->h1 = (uint32_t)t[1] & 0x3ffffff; b = (uint32_t)(t[1] >> 26);
	t[2] += b; state->h2 = (uint32_t)t[2] & 0x3ffffff; b = (uint32_t)(t[2] >> 26);
	t[3] += b; state->h3 = (uint32_t)t[3] & 0x3ffffff; b = (uint32_t)(t[3] >> 26);
	t[4] += b; state->h4 = (uint32_t)t[4] & 0x3ffffff; b = (uint32_t)(t[4] >> 26);
	state->h0 += b * 5;

	if (len >= 16)
		goto poly1305_donna_16bytes;

	/* final bytes */
poly1305_donna_atmost15bytes:
	if (!len)
		return;

	for (j = 0; j < len; j++)
		mp[j] = in[j];
	mp[j++] = 1;
	for (; j < 16; j++)
		mp[j] = 0;
	len = 0;

	t0 = U8TO32_LE(mp+0);
	t1 = U8TO32_LE(mp+4);
	t2 = U8TO32_LE(mp+8);
	t3 = U8TO32_LE(mp+12);

	state->h0 += t0 & 0x3ffffff;
	state->h1 += ((((uint64_t)t1 << 32) | t0) >> 26) & 0x3ffffff;
	state->h2 += ((((uint64_t)t2 << 32) | t1) >> 20) & 0x3ffffff;
	state->h3 += ((((uint64_t)t3 << 32) | t2) >> 14) & 0x3ffffff;
	state->h4 += (t3 >> 8);

	goto poly1305_donna_mul;
}

void	poly1305(uint8_t out[16], const uint8_t *m, size_t inlen, const uint8_t key[32])
{
	poly1305_state state;
	
	poly1305_init(&state, key);
	poly1305_update(&state, m, inlen);
	poly1305_final(&state, out);
}

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	poly1305_test
//===========================================================================================================================

OSStatus	poly1305_test( int inPerf );
OSStatus	poly1305_test( int inPerf )
{
	// Test from "Testing: C++ vs. onetimeauth" section of "Cryptography in NaCl".
	
	static const uint8_t		kPoly1305_NaCl_Key[] = 
	{
		0xee, 0xa6, 0xa7, 0x25, 0x1c, 0x1e, 0x72, 0x91,  0x6d, 0x11, 0xc2, 0xcb, 0x21, 0x4d, 0x3c, 0x25, 
		0x25, 0x39, 0x12, 0x1d, 0x8e, 0x23, 0x4e, 0x65,  0x2d, 0x65, 0x1f, 0xa4, 0xc8, 0xcf, 0xf8, 0x80
	};
	static const uint8_t		kPoly1305_NaCl_Msg[ 131 ] = 
	{
		0x8e, 0x99, 0x3b, 0x9f, 0x48, 0x68, 0x12, 0x73, 0xc2, 0x96, 0x50, 0xba, 0x32, 0xfc, 0x76, 0xce, 
		0x48, 0x33, 0x2e, 0xa7, 0x16, 0x4d, 0x96, 0xa4, 0x47, 0x6f, 0xb8, 0xc5, 0x31, 0xa1, 0x18, 0x6a, 
		0xc0, 0xdf, 0xc1, 0x7c, 0x98, 0xdc, 0xe8, 0x7b, 0x4d, 0xa7, 0xf0, 0x11, 0xec, 0x48, 0xc9, 0x72, 
		0x71, 0xd2, 0xc2, 0x0f, 0x9b, 0x92, 0x8f, 0xe2, 0x27, 0x0d, 0x6f, 0xb8, 0x63, 0xd5, 0x17, 0x38, 
		0xb4, 0x8e, 0xee, 0xe3, 0x14, 0xa7, 0xcc, 0x8a, 0xb9, 0x32, 0x16, 0x45, 0x48, 0xe5, 0x26, 0xae, 
		0x90, 0x22, 0x43, 0x68, 0x51, 0x7a, 0xcf, 0xea, 0xbd, 0x6b, 0xb3, 0x73, 0x2b, 0xc0, 0xe9, 0xda, 
		0x99, 0x83, 0x2b, 0x61, 0xca, 0x01, 0xb6, 0xde, 0x56, 0x24, 0x4a, 0x9e, 0x88, 0xd5, 0xf9, 0xb3, 
		0x79, 0x73, 0xf6, 0x22, 0xa4, 0x3d, 0x14, 0xa6, 0x59, 0x9b, 0x1f, 0x65, 0x4c, 0xb4, 0x5a, 0x74, 
		0xe3, 0x55, 0xa5
	};
	static const uint8_t		kPoly1305_NaCl_Tag[] = 
	{
		0xf3, 0xff, 0xc7, 0x70, 0x3f, 0x94, 0x00, 0xe5, 0x2a, 0x7d, 0xfb, 0x4b, 0x3d, 0x33, 0x05, 0xd9
	};
	
	// Test Vector #1 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kPoly1305Test1_Key[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kPoly1305Test1_Input[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kPoly1305Test1_Tag[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	};
	
	// Test Vector #2 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kPoly1305Test2_Key[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x36, 0xE5, 0xF6, 0xB5, 0xC5, 0xE0, 0x60, 0x70, 0xF0, 0xEF, 0xCA, 0x96, 0x22, 0x7A, 0x86, 0x3E
	};
	static const uint8_t		kPoly1305Test2_Input[] =
	{
		0x41, 0x6E, 0x79, 0x20, 0x73, 0x75, 0x62, 0x6D, 0x69, 0x73, 0x73, 0x69, 0x6F, 0x6E, 0x20, 0x74, 
		0x6F, 0x20, 0x74, 0x68, 0x65, 0x20, 0x49, 0x45, 0x54, 0x46, 0x20, 0x69, 0x6E, 0x74, 0x65, 0x6E, 
		0x64, 0x65, 0x64, 0x20, 0x62, 0x79, 0x20, 0x74, 0x68, 0x65, 0x20, 0x43, 0x6F, 0x6E, 0x74, 0x72, 
		0x69, 0x62, 0x75, 0x74, 0x6F, 0x72, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x70, 0x75, 0x62, 0x6C, 0x69, 
		0x63, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x61, 0x73, 0x20, 0x61, 0x6C, 0x6C, 0x20, 0x6F, 0x72, 
		0x20, 0x70, 0x61, 0x72, 0x74, 0x20, 0x6F, 0x66, 0x20, 0x61, 0x6E, 0x20, 0x49, 0x45, 0x54, 0x46, 
		0x20, 0x49, 0x6E, 0x74, 0x65, 0x72, 0x6E, 0x65, 0x74, 0x2D, 0x44, 0x72, 0x61, 0x66, 0x74, 0x20, 
		0x6F, 0x72, 0x20, 0x52, 0x46, 0x43, 0x20, 0x61, 0x6E, 0x64, 0x20, 0x61, 0x6E, 0x79, 0x20, 0x73, 
		0x74, 0x61, 0x74, 0x65, 0x6D, 0x65, 0x6E, 0x74, 0x20, 0x6D, 0x61, 0x64, 0x65, 0x20, 0x77, 0x69, 
		0x74, 0x68, 0x69, 0x6E, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6F, 0x6E, 0x74, 0x65, 0x78, 0x74, 
		0x20, 0x6F, 0x66, 0x20, 0x61, 0x6E, 0x20, 0x49, 0x45, 0x54, 0x46, 0x20, 0x61, 0x63, 0x74, 0x69, 
		0x76, 0x69, 0x74, 0x79, 0x20, 0x69, 0x73, 0x20, 0x63, 0x6F, 0x6E, 0x73, 0x69, 0x64, 0x65, 0x72, 
		0x65, 0x64, 0x20, 0x61, 0x6E, 0x20, 0x22, 0x49, 0x45, 0x54, 0x46, 0x20, 0x43, 0x6F, 0x6E, 0x74, 
		0x72, 0x69, 0x62, 0x75, 0x74, 0x69, 0x6F, 0x6E, 0x22, 0x2E, 0x20, 0x53, 0x75, 0x63, 0x68, 0x20, 
		0x73, 0x74, 0x61, 0x74, 0x65, 0x6D, 0x65, 0x6E, 0x74, 0x73, 0x20, 0x69, 0x6E, 0x63, 0x6C, 0x75, 
		0x64, 0x65, 0x20, 0x6F, 0x72, 0x61, 0x6C, 0x20, 0x73, 0x74, 0x61, 0x74, 0x65, 0x6D, 0x65, 0x6E, 
		0x74, 0x73, 0x20, 0x69, 0x6E, 0x20, 0x49, 0x45, 0x54, 0x46, 0x20, 0x73, 0x65, 0x73, 0x73, 0x69, 
		0x6F, 0x6E, 0x73, 0x2C, 0x20, 0x61, 0x73, 0x20, 0x77, 0x65, 0x6C, 0x6C, 0x20, 0x61, 0x73, 0x20, 
		0x77, 0x72, 0x69, 0x74, 0x74, 0x65, 0x6E, 0x20, 0x61, 0x6E, 0x64, 0x20, 0x65, 0x6C, 0x65, 0x63, 
		0x74, 0x72, 0x6F, 0x6E, 0x69, 0x63, 0x20, 0x63, 0x6F, 0x6D, 0x6D, 0x75, 0x6E, 0x69, 0x63, 0x61, 
		0x74, 0x69, 0x6F, 0x6E, 0x73, 0x20, 0x6D, 0x61, 0x64, 0x65, 0x20, 0x61, 0x74, 0x20, 0x61, 0x6E, 
		0x79, 0x20, 0x74, 0x69, 0x6D, 0x65, 0x20, 0x6F, 0x72, 0x20, 0x70, 0x6C, 0x61, 0x63, 0x65, 0x2C, 
		0x20, 0x77, 0x68, 0x69, 0x63, 0x68, 0x20, 0x61, 0x72, 0x65, 0x20, 0x61, 0x64, 0x64, 0x72, 0x65, 
		0x73, 0x73, 0x65, 0x64, 0x20, 0x74, 0x6F
	};
	static const uint8_t		kPoly1305Test2_Tag[] =
	{
		0x36, 0xE5, 0xF6, 0xB5, 0xC5, 0xE0, 0x60, 0x70, 0xF0, 0xEF, 0xCA, 0x96, 0x22, 0x7A, 0x86, 0x3E
	};
	
	// Test Vector #3 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kPoly1305Test3_Key[] =
	{
		0x36, 0xE5, 0xF6, 0xB5, 0xC5, 0xE0, 0x60, 0x70, 0xF0, 0xEF, 0xCA, 0x96, 0x22, 0x7A, 0x86, 0x3E, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kPoly1305Test3_Input[] =
	{
		0x41, 0x6E, 0x79, 0x20, 0x73, 0x75, 0x62, 0x6D, 0x69, 0x73, 0x73, 0x69, 0x6F, 0x6E, 0x20, 0x74, 
		0x6F, 0x20, 0x74, 0x68, 0x65, 0x20, 0x49, 0x45, 0x54, 0x46, 0x20, 0x69, 0x6E, 0x74, 0x65, 0x6E, 
		0x64, 0x65, 0x64, 0x20, 0x62, 0x79, 0x20, 0x74, 0x68, 0x65, 0x20, 0x43, 0x6F, 0x6E, 0x74, 0x72, 
		0x69, 0x62, 0x75, 0x74, 0x6F, 0x72, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x70, 0x75, 0x62, 0x6C, 0x69, 
		0x63, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x61, 0x73, 0x20, 0x61, 0x6C, 0x6C, 0x20, 0x6F, 0x72, 
		0x20, 0x70, 0x61, 0x72, 0x74, 0x20, 0x6F, 0x66, 0x20, 0x61, 0x6E, 0x20, 0x49, 0x45, 0x54, 0x46, 
		0x20, 0x49, 0x6E, 0x74, 0x65, 0x72, 0x6E, 0x65, 0x74, 0x2D, 0x44, 0x72, 0x61, 0x66, 0x74, 0x20, 
		0x6F, 0x72, 0x20, 0x52, 0x46, 0x43, 0x20, 0x61, 0x6E, 0x64, 0x20, 0x61, 0x6E, 0x79, 0x20, 0x73, 
		0x74, 0x61, 0x74, 0x65, 0x6D, 0x65, 0x6E, 0x74, 0x20, 0x6D, 0x61, 0x64, 0x65, 0x20, 0x77, 0x69, 
		0x74, 0x68, 0x69, 0x6E, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6F, 0x6E, 0x74, 0x65, 0x78, 0x74, 
		0x20, 0x6F, 0x66, 0x20, 0x61, 0x6E, 0x20, 0x49, 0x45, 0x54, 0x46, 0x20, 0x61, 0x63, 0x74, 0x69, 
		0x76, 0x69, 0x74, 0x79, 0x20, 0x69, 0x73, 0x20, 0x63, 0x6F, 0x6E, 0x73, 0x69, 0x64, 0x65, 0x72, 
		0x65, 0x64, 0x20, 0x61, 0x6E, 0x20, 0x22, 0x49, 0x45, 0x54, 0x46, 0x20, 0x43, 0x6F, 0x6E, 0x74, 
		0x72, 0x69, 0x62, 0x75, 0x74, 0x69, 0x6F, 0x6E, 0x22, 0x2E, 0x20, 0x53, 0x75, 0x63, 0x68, 0x20, 
		0x73, 0x74, 0x61, 0x74, 0x65, 0x6D, 0x65, 0x6E, 0x74, 0x73, 0x20, 0x69, 0x6E, 0x63, 0x6C, 0x75, 
		0x64, 0x65, 0x20, 0x6F, 0x72, 0x61, 0x6C, 0x20, 0x73, 0x74, 0x61, 0x74, 0x65, 0x6D, 0x65, 0x6E, 
		0x74, 0x73, 0x20, 0x69, 0x6E, 0x20, 0x49, 0x45, 0x54, 0x46, 0x20, 0x73, 0x65, 0x73, 0x73, 0x69, 
		0x6F, 0x6E, 0x73, 0x2C, 0x20, 0x61, 0x73, 0x20, 0x77, 0x65, 0x6C, 0x6C, 0x20, 0x61, 0x73, 0x20, 
		0x77, 0x72, 0x69, 0x74, 0x74, 0x65, 0x6E, 0x20, 0x61, 0x6E, 0x64, 0x20, 0x65, 0x6C, 0x65, 0x63, 
		0x74, 0x72, 0x6F, 0x6E, 0x69, 0x63, 0x20, 0x63, 0x6F, 0x6D, 0x6D, 0x75, 0x6E, 0x69, 0x63, 0x61, 
		0x74, 0x69, 0x6F, 0x6E, 0x73, 0x20, 0x6D, 0x61, 0x64, 0x65, 0x20, 0x61, 0x74, 0x20, 0x61, 0x6E, 
		0x79, 0x20, 0x74, 0x69, 0x6D, 0x65, 0x20, 0x6F, 0x72, 0x20, 0x70, 0x6C, 0x61, 0x63, 0x65, 0x2C, 
		0x20, 0x77, 0x68, 0x69, 0x63, 0x68, 0x20, 0x61, 0x72, 0x65, 0x20, 0x61, 0x64, 0x64, 0x72, 0x65, 
		0x73, 0x73, 0x65, 0x64, 0x20, 0x74, 0x6F
	};
	static const uint8_t		kPoly1305Test3_Tag[] =
	{
		0xF3, 0x47, 0x7E, 0x7C, 0xD9, 0x54, 0x17, 0xAF, 0x89, 0xA6, 0xB8, 0x79, 0x4C, 0x31, 0x0C, 0xF0
	};
	
	// Test Vector #4 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kPoly1305Test4_Key[] =
	{
		0x1C, 0x92, 0x40, 0xA5, 0xEB, 0x55, 0xD3, 0x8A, 0xF3, 0x33, 0x88, 0x86, 0x04, 0xF6, 0xB5, 0xF0, 
		0x47, 0x39, 0x17, 0xC1, 0x40, 0x2B, 0x80, 0x09, 0x9D, 0xCA, 0x5C, 0xBC, 0x20, 0x70, 0x75, 0xC0
	};
	static const uint8_t		kPoly1305Test4_Input[] =
	{
		0x27, 0x54, 0x77, 0x61, 0x73, 0x20, 0x62, 0x72, 0x69, 0x6C, 0x6C, 0x69, 0x67, 0x2C, 0x20, 0x61, 
		0x6E, 0x64, 0x20, 0x74, 0x68, 0x65, 0x20, 0x73, 0x6C, 0x69, 0x74, 0x68, 0x79, 0x20, 0x74, 0x6F, 
		0x76, 0x65, 0x73, 0x0A, 0x44, 0x69, 0x64, 0x20, 0x67, 0x79, 0x72, 0x65, 0x20, 0x61, 0x6E, 0x64, 
		0x20, 0x67, 0x69, 0x6D, 0x62, 0x6C, 0x65, 0x20, 0x69, 0x6E, 0x20, 0x74, 0x68, 0x65, 0x20, 0x77, 
		0x61, 0x62, 0x65, 0x3A, 0x0A, 0x41, 0x6C, 0x6C, 0x20, 0x6D, 0x69, 0x6D, 0x73, 0x79, 0x20, 0x77, 
		0x65, 0x72, 0x65, 0x20, 0x74, 0x68, 0x65, 0x20, 0x62, 0x6F, 0x72, 0x6F, 0x67, 0x6F, 0x76, 0x65, 
		0x73, 0x2C, 0x0A, 0x41, 0x6E, 0x64, 0x20, 0x74, 0x68, 0x65, 0x20, 0x6D, 0x6F, 0x6D, 0x65, 0x20, 
		0x72, 0x61, 0x74, 0x68, 0x73, 0x20, 0x6F, 0x75, 0x74, 0x67, 0x72, 0x61, 0x62, 0x65, 0x2E
	};
	static const uint8_t		kPoly1305Test4_Tag[] =
	{
		0x45, 0x41, 0x66, 0x9A, 0x7E, 0xAA, 0xEE, 0x61, 0xE7, 0x08, 0xDC, 0x7C, 0xBC, 0xC5, 0xEB, 0x62
	};
	
	// Test Vector #5 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kPoly1305Test5_Key[] =
	{
		0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kPoly1305Test5_Input[] =
	{
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	static const uint8_t		kPoly1305Test5_Tag[] =
	{
		0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	
	// Test Vector #6 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kPoly1305Test6_Key[] =
	{
		0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	static const uint8_t		kPoly1305Test6_Input[] =
	{
		0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kPoly1305Test6_Tag[] =
	{
		0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	
	// Test Vector #7 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kPoly1305Test7_Key[] =
	{
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kPoly1305Test7_Input[] =
	{
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xF0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kPoly1305Test7_Tag[] =
	{
		0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	
	// Test Vector #8 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kPoly1305Test8_Key[] =
	{
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kPoly1305Test8_Input[] =
	{
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
		0xFB, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 
		0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01
	};
	static const uint8_t		kPoly1305Test8_Tag[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	
	// Test Vector #9 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kPoly1305Test9_Key[] =
	{
		0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kPoly1305Test9_Input[] =
	{
		0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	static const uint8_t		kPoly1305Test9_Tag[] =
	{
		0xFA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	
	// Test Vector #10 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kPoly1305Test10_Key[] =
	{
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kPoly1305Test10_Input[] =
	{
		0xE3, 0x35, 0x94, 0xD7, 0x50, 0x5E, 0x43, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x33, 0x94, 0xD7, 0x50, 0x5E, 0x43, 0x79, 0xCD, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kPoly1305Test10_Tag[] =
	{
		0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	
	// Test Vector #11 from <https://tools.ietf.org/html/rfc7539>.
	
	static const uint8_t		kPoly1305Test11_Key[] =
	{
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kPoly1305Test11_Input[] =
	{
		0xE3, 0x35, 0x94, 0xD7, 0x50, 0x5E, 0x43, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x33, 0x94, 0xD7, 0x50, 0x5E, 0x43, 0x79, 0xCD, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t		kPoly1305Test11_Tag[] =
	{
		0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	
	OSStatus			err;
	uint8_t				result[ 16 ];
	uint64_t			ticks;
	poly1305_state		state;
	size_t				i;
	double				d;
	size_t				len;
	uint8_t *			buf;
	
	// Test vector from <https://tools.ietf.org/html/rfc7539>.
	
	memset( result, 0, sizeof( result ) );
	poly1305( result, (const uint8_t *) "Cryptographic Forum Research Group", 34, (const uint8_t *) 
		"\x85\xD6\xBE\x78\x57\x55\x6D\x33\x7F\x44\x52\xFE\x42\xD5\x06\xA8"
		"\x01\x03\x80\x8A\xFB\x0D\xB2\xFD\x4A\xBF\xF6\xAF\x41\x49\xF5\x1B" );
	require_action( memcmp( result, "\xa8\x06\x1d\xc1\x30\x51\x36\xc6\xc2\x2b\x8b\xaf\x0c\x01\x27\xa9", 16 ) == 0, exit, err = -1 );
	
	// NaCl
	
	memset( result, 0, sizeof( result ) );
	poly1305( result, kPoly1305_NaCl_Msg, sizeof( kPoly1305_NaCl_Msg ), kPoly1305_NaCl_Key );
	require_action( memcmp( result, kPoly1305_NaCl_Tag, sizeof( kPoly1305_NaCl_Tag ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	poly1305_init( &state, kPoly1305_NaCl_Key );
	for( i = 0; i < sizeof( kPoly1305_NaCl_Msg ); ++i )
	{
		poly1305_update( &state, &kPoly1305_NaCl_Msg[ i ], 1 );
	}
	poly1305_final( &state, result );
	require_action( memcmp( result, kPoly1305_NaCl_Tag, sizeof( kPoly1305_NaCl_Tag ) ) == 0, exit, err = -1 );
	
	// Test Vector #1 from <https://tools.ietf.org/html/rfc7539>
	
	memset( result, 0, sizeof( result ) );
	poly1305( result, kPoly1305Test1_Input, sizeof( kPoly1305Test1_Input ), kPoly1305Test1_Key );
	require_action( memcmp( result, kPoly1305Test1_Tag, sizeof( kPoly1305Test1_Tag ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	poly1305_init( &state, kPoly1305Test1_Key );
	for( i = 0; i < sizeof( kPoly1305Test1_Input ); ++i )
	{
		poly1305_update( &state, &kPoly1305Test1_Input[ i ], 1 );
	}
	poly1305_final( &state, result );
	require_action( memcmp( result, kPoly1305Test1_Tag, sizeof( kPoly1305Test1_Tag ) ) == 0, exit, err = -1 );
	
	// Test Vector #2 from <https://tools.ietf.org/html/rfc7539>
	
	memset( result, 0, sizeof( result ) );
	poly1305( result, kPoly1305Test2_Input, sizeof( kPoly1305Test2_Input ), kPoly1305Test2_Key );
	require_action( memcmp( result, kPoly1305Test2_Tag, sizeof( kPoly1305Test2_Tag ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	poly1305_init( &state, kPoly1305Test2_Key );
	for( i = 0; i < sizeof( kPoly1305Test2_Input ); ++i )
	{
		poly1305_update( &state, &kPoly1305Test2_Input[ i ], 1 );
	}
	poly1305_final( &state, result );
	require_action( memcmp( result, kPoly1305Test2_Tag, sizeof( kPoly1305Test2_Tag ) ) == 0, exit, err = -1 );
	
	// Test Vector #3 from <https://tools.ietf.org/html/rfc7539>
	
	memset( result, 0, sizeof( result ) );
	poly1305( result, kPoly1305Test3_Input, sizeof( kPoly1305Test3_Input ), kPoly1305Test3_Key );
	require_action( memcmp( result, kPoly1305Test3_Tag, sizeof( kPoly1305Test3_Tag ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	poly1305_init( &state, kPoly1305Test3_Key );
	for( i = 0; i < sizeof( kPoly1305Test3_Input ); ++i )
	{
		poly1305_update( &state, &kPoly1305Test3_Input[ i ], 1 );
	}
	poly1305_final( &state, result );
	require_action( memcmp( result, kPoly1305Test3_Tag, sizeof( kPoly1305Test3_Tag ) ) == 0, exit, err = -1 );
	
	// Test Vector #4 from <https://tools.ietf.org/html/rfc7539>
	
	memset( result, 0, sizeof( result ) );
	poly1305( result, kPoly1305Test4_Input, sizeof( kPoly1305Test4_Input ), kPoly1305Test4_Key );
	require_action( memcmp( result, kPoly1305Test4_Tag, sizeof( kPoly1305Test4_Tag ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	poly1305_init( &state, kPoly1305Test4_Key );
	for( i = 0; i < sizeof( kPoly1305Test4_Input ); ++i )
	{
		poly1305_update( &state, &kPoly1305Test4_Input[ i ], 1 );
	}
	poly1305_final( &state, result );
	require_action( memcmp( result, kPoly1305Test4_Tag, sizeof( kPoly1305Test4_Tag ) ) == 0, exit, err = -1 );
	
	// Test Vector #5 from <https://tools.ietf.org/html/rfc7539>
	
	memset( result, 0, sizeof( result ) );
	poly1305( result, kPoly1305Test5_Input, sizeof( kPoly1305Test5_Input ), kPoly1305Test5_Key );
	require_action( memcmp( result, kPoly1305Test5_Tag, sizeof( kPoly1305Test5_Tag ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	poly1305_init( &state, kPoly1305Test5_Key );
	for( i = 0; i < sizeof( kPoly1305Test5_Input ); ++i )
	{
		poly1305_update( &state, &kPoly1305Test5_Input[ i ], 1 );
	}
	poly1305_final( &state, result );
	require_action( memcmp( result, kPoly1305Test5_Tag, sizeof( kPoly1305Test5_Tag ) ) == 0, exit, err = -1 );
	
	// Test Vector #6 from <https://tools.ietf.org/html/rfc7539>
	
	memset( result, 0, sizeof( result ) );
	poly1305( result, kPoly1305Test6_Input, sizeof( kPoly1305Test6_Input ), kPoly1305Test6_Key );
	require_action( memcmp( result, kPoly1305Test6_Tag, sizeof( kPoly1305Test6_Tag ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	poly1305_init( &state, kPoly1305Test6_Key );
	for( i = 0; i < sizeof( kPoly1305Test6_Input ); ++i )
	{
		poly1305_update( &state, &kPoly1305Test6_Input[ i ], 1 );
	}
	poly1305_final( &state, result );
	require_action( memcmp( result, kPoly1305Test6_Tag, sizeof( kPoly1305Test6_Tag ) ) == 0, exit, err = -1 );
	
	// Test Vector #7 from <https://tools.ietf.org/html/rfc7539>
	
	memset( result, 0, sizeof( result ) );
	poly1305( result, kPoly1305Test7_Input, sizeof( kPoly1305Test7_Input ), kPoly1305Test7_Key );
	require_action( memcmp( result, kPoly1305Test7_Tag, sizeof( kPoly1305Test7_Tag ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	poly1305_init( &state, kPoly1305Test7_Key );
	for( i = 0; i < sizeof( kPoly1305Test7_Input ); ++i )
	{
		poly1305_update( &state, &kPoly1305Test7_Input[ i ], 1 );
	}
	poly1305_final( &state, result );
	require_action( memcmp( result, kPoly1305Test7_Tag, sizeof( kPoly1305Test7_Tag ) ) == 0, exit, err = -1 );
	
	// Test Vector #8 from <https://tools.ietf.org/html/rfc7539>
	
	memset( result, 0, sizeof( result ) );
	poly1305( result, kPoly1305Test8_Input, sizeof( kPoly1305Test8_Input ), kPoly1305Test8_Key );
	require_action( memcmp( result, kPoly1305Test8_Tag, sizeof( kPoly1305Test8_Tag ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	poly1305_init( &state, kPoly1305Test8_Key );
	for( i = 0; i < sizeof( kPoly1305Test8_Input ); ++i )
	{
		poly1305_update( &state, &kPoly1305Test8_Input[ i ], 1 );
	}
	poly1305_final( &state, result );
	require_action( memcmp( result, kPoly1305Test8_Tag, sizeof( kPoly1305Test8_Tag ) ) == 0, exit, err = -1 );
	
	// Test Vector #9 from <https://tools.ietf.org/html/rfc7539>
	
	memset( result, 0, sizeof( result ) );
	poly1305( result, kPoly1305Test9_Input, sizeof( kPoly1305Test9_Input ), kPoly1305Test9_Key );
	require_action( memcmp( result, kPoly1305Test9_Tag, sizeof( kPoly1305Test9_Tag ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	poly1305_init( &state, kPoly1305Test9_Key );
	for( i = 0; i < sizeof( kPoly1305Test9_Input ); ++i )
	{
		poly1305_update( &state, &kPoly1305Test9_Input[ i ], 1 );
	}
	poly1305_final( &state, result );
	require_action( memcmp( result, kPoly1305Test9_Tag, sizeof( kPoly1305Test9_Tag ) ) == 0, exit, err = -1 );
	
	// Test Vector #10 from <https://tools.ietf.org/html/rfc7539>
	
	memset( result, 0, sizeof( result ) );
	poly1305( result, kPoly1305Test10_Input, sizeof( kPoly1305Test10_Input ), kPoly1305Test10_Key );
	require_action( memcmp( result, kPoly1305Test10_Tag, sizeof( kPoly1305Test10_Tag ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	poly1305_init( &state, kPoly1305Test10_Key );
	for( i = 0; i < sizeof( kPoly1305Test10_Input ); ++i )
	{
		poly1305_update( &state, &kPoly1305Test10_Input[ i ], 1 );
	}
	poly1305_final( &state, result );
	require_action( memcmp( result, kPoly1305Test10_Tag, sizeof( kPoly1305Test10_Tag ) ) == 0, exit, err = -1 );
	
	// Test Vector #11 from <https://tools.ietf.org/html/rfc7539>
	
	memset( result, 0, sizeof( result ) );
	poly1305( result, kPoly1305Test11_Input, sizeof( kPoly1305Test11_Input ), kPoly1305Test11_Key );
	require_action( memcmp( result, kPoly1305Test11_Tag, sizeof( kPoly1305Test11_Tag ) ) == 0, exit, err = -1 );
	
	memset( result, 0, sizeof( result ) );
	poly1305_init( &state, kPoly1305Test11_Key );
	for( i = 0; i < sizeof( kPoly1305Test11_Input ); ++i )
	{
		poly1305_update( &state, &kPoly1305Test11_Input[ i ], 1 );
	}
	poly1305_final( &state, result );
	require_action( memcmp( result, kPoly1305Test11_Tag, sizeof( kPoly1305Test11_Tag ) ) == 0, exit, err = -1 );
	
	if( inPerf )
	{
		// Small performance test.
		
		len = 1500;
		buf = (uint8_t *) malloc( len );
		require_action( buf, exit, err = kNoMemoryErr );
		memset( buf, 'a', len );
		
		ticks = UpTicks();
		for( i = 0; i < 100000; ++i )
		{
			if( ++buf[ 0 ] == 0 ) { if( ++buf[ 1 ] == 0 ) { if( ++buf[ 2 ] == 0 ) ++buf[ 3 ]; } }
			poly1305( result, buf, len, kPoly1305_NaCl_Key );
		}
		ticks = UpTicks() - ticks;
		d = ( (double) ticks ) / UpTicksPerSecond();
		FPrintF( stderr, "\tpoly1305 (%zu bytes): %f (%f µs, %.2f MB/sec): %.3H\n", 
			len, d, ( 1000000 * d ) / i, ( i * len ) / ( d * 1048576.0 ), result, 16, 16 );
		free( buf );
		
		// Medium performance test.
		
		len = 50000;
		buf = (uint8_t *) malloc( len );
		require_action( buf, exit, err = kNoMemoryErr );
		memset( buf, 'a', len );
		
		ticks = UpTicks();
		for( i = 0; i < 10000; ++i )
		{
			if( ++buf[ 0 ] == 0 ) { if( ++buf[ 1 ] == 0 ) { if( ++buf[ 2 ] == 0 ) ++buf[ 3 ]; } }
			poly1305( result, buf, len, kPoly1305_NaCl_Key );
		}
		ticks = UpTicks() - ticks;
		d = ( (double) ticks ) / UpTicksPerSecond();
		FPrintF( stderr, "\tpoly1305 (%zu bytes): %f (%f µs, %.2f MB/sec): %.3H\n", 
			len, d, ( 1000000 * d ) / i, ( i * len ) / ( d * 1048576.0 ), result, 16, 16 );
		free( buf );
		
		// Big performance test.
		
		len = 5000000;
		buf = (uint8_t *) malloc( len );
		require_action( buf, exit, err = kNoMemoryErr );
		memset( buf, 'a', len );
		
		ticks = UpTicks();
		for( i = 0; i < 100; ++i )
		{
			if( ++buf[ 0 ] == 0 ) { if( ++buf[ 1 ] == 0 ) { if( ++buf[ 2 ] == 0 ) ++buf[ 3 ]; } }
			poly1305( result, buf, len, kPoly1305_NaCl_Key );
		}
		ticks = UpTicks() - ticks;
		d = ( (double) ticks ) / UpTicksPerSecond();
		FPrintF( stderr, "\tpoly1305 (%zu bytes): %f (%f µs, %.2f MB/sec): %.3H\n", 
			len, d, ( 1000000 * d ) / i, ( i * len ) / ( d * 1048576.0 ), result, 16, 16 );
		free( buf );
	}
	err = kNoErr;
	
exit:
	printf( "poly1305_test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS

#if 0
#pragma mark -
#pragma mark == chacha20-poly1305 ==
#endif

//===========================================================================================================================
//	chacha20-poly1305 AEAD algorithm
//
//	See <https://tools.ietf.org/html/rfc7539> for full documentation. To summarize
//
//	Key is 32 bytes. Must not be reused with the same nonce.
//	Nonce is 96 bits. The first 32 bits are a constant value. This code use a constant value of 0.
//	Poly1305 key is derived from first 32 bytes of encrypting 64 zeros with ChaCha20 with key, nonce, and a counter of 0.
//	Data starts being encrypted/decrypted with ChaCha20 with key, nonce, and a counter of 1.
//	Encrypted/decrypted output data is the same size as the input, non-AAD data.
//	16-byte auth tag is generated with Poly1305 on the data formatted as follows:
//
//		<n:AAD>
//		<0-15:padding1 of zeros to bring total length to a multiple of 16>
//		<n:encrypted data>
//		<0-15:padding1 of zeros to bring total length to a multiple of 16>
//		<8:little endian AAD length in bytes>
//		<8:little endian encrypted length in bytes>.
//===========================================================================================================================

static const uint8_t	kZero64[ 64 ] = 
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

//===========================================================================================================================
//	chacha20_poly1305_init
//===========================================================================================================================

void	chacha20_poly1305_init_64x64( chacha20_poly1305_state *inState, const uint8_t inKey[ 32 ], const uint8_t inNonce[ 8 ] )
{
	uint8_t		block[ 64 ];
	
	// Use the first 32 bytes of the keystream as the poly1305 key and discard the remainder of the block.
	// See <https://tools.ietf.org/html/rfc7539> for full documentation on the AEAD construction used here.
	
	chacha20_init_64x64( &inState->chacha20, inKey, inNonce, 0 );
	_chacha20_xor( &inState->chacha20, block, kZero64, sizeof( kZero64 ) );
	poly1305_init( &inState->poly1305, block );
	inState->aadLen  = 0;
	inState->dataLen = 0;
	inState->padded  = false;
}

void	chacha20_poly1305_init_96x32( chacha20_poly1305_state *inState, const uint8_t inKey[ 32 ], const uint8_t inNonce[ 12 ] )
{
	uint8_t		block[ 64 ];
	
	// Use the first 32 bytes of the keystream as the poly1305 key and discard the remainder of the block.
	// See <https://tools.ietf.org/html/rfc7539> for full documentation on the AEAD construction used here.
	
	chacha20_init_96x32( &inState->chacha20, inKey, inNonce, 0 );
	_chacha20_xor( &inState->chacha20, block, kZero64, sizeof( kZero64 ) );
	poly1305_init( &inState->poly1305, block );
	inState->aadLen  = 0;
	inState->dataLen = 0;
	inState->padded  = false;
}

//===========================================================================================================================
//	chacha20_poly1305_add_aad
//===========================================================================================================================

void	chacha20_poly1305_add_aad( chacha20_poly1305_state *inState, const void *inSrc, size_t inLen )
{
	check( !inState->padded );
	poly1305_update( &inState->poly1305, inSrc, inLen );
	inState->aadLen += inLen;
}

//===========================================================================================================================
//	chacha20_poly1305_encrypt
//===========================================================================================================================

size_t	chacha20_poly1305_encrypt( chacha20_poly1305_state *inState, const void *inSrc, size_t inLen, void *inDst )
{
	size_t		n;
	
	// MAC padding1 before processing non-AAD data.
	
	if( !inState->padded )
	{
		n = inState->aadLen & 15;
		if( n > 0 ) poly1305_update( &inState->poly1305, kZero64, 16 - n );
		inState->padded = true;
	}
	
	n = chacha20_update( &inState->chacha20, inSrc, inLen, inDst );
	if( n )
	{
		poly1305_update( &inState->poly1305, inDst, n );
		inState->dataLen += n;
	}
	return( n );
}

//===========================================================================================================================
//	chacha20_poly1305_decrypt
//===========================================================================================================================

size_t	chacha20_poly1305_decrypt( chacha20_poly1305_state *inState, const void *inSrc, size_t inLen, void *inDst )
{
	size_t		n;
	
	// MAC padding1 before processing non-AAD data.
	
	if( !inState->padded )
	{
		n = inState->aadLen & 15;
		if( n > 0 ) poly1305_update( &inState->poly1305, kZero64, 16 - n );
		inState->padded = true;
	}
	
	poly1305_update( &inState->poly1305, inSrc, inLen );
	n = chacha20_update( &inState->chacha20, inSrc, inLen, inDst );
	inState->dataLen += n;
	return( n );
}

//===========================================================================================================================
//	chacha20_poly1305_final
//===========================================================================================================================

size_t	chacha20_poly1305_final( chacha20_poly1305_state *inState, void *inDst, uint8_t outAuthTag[ 16 ] )
{
	size_t		nFinal, n;
	uint8_t		buf[ 16 ];
	
	// MAC padding1 before processing non-AAD data (only happens here if there was no non-AAD data).
	
	if( !inState->padded )
	{
		n = inState->aadLen & 15;
		if( n > 0 ) poly1305_update( &inState->poly1305, kZero64, 16 - n );
	}
	
	// Encrypt any non-block-sized data at the end that may have been buffered by chacha20.
	
	nFinal = chacha20_final( &inState->chacha20, inDst );
	if( nFinal > 0 )
	{
		poly1305_update( &inState->poly1305, inDst, nFinal );
		inState->dataLen += nFinal;
	}
	
	// MAC padding2 after non-AAD data.
	
	n = inState->dataLen & 15;
	if( n > 0 ) poly1305_update( &inState->poly1305, kZero64, 16 - n );
	
	// MAC AAD and data lengths.
	
	static_analyzer_mem_zeroed( buf, 16 ); // Remove when <radar:15309659> is fixed.
	WriteLittle64( &buf[ 0 ], (uint64_t) inState->aadLen );
	WriteLittle64( &buf[ 8 ], (uint64_t) inState->dataLen );
	poly1305_update( &inState->poly1305, buf, 16 );
	poly1305_final( &inState->poly1305, outAuthTag );
	return( nFinal );
}

//===========================================================================================================================
//	chacha20_poly1305_final_verify
//===========================================================================================================================

size_t	chacha20_poly1305_verify( chacha20_poly1305_state *inState, void *inDst, const uint8_t inAuthTag[ 16 ], OSStatus *outErr )
{
	size_t		nFinal, n;
	uint8_t		buf[ 16 ];
	
	// MAC padding1 before processing non-AAD data (only happens here if there was no non-AAD data).
	
	if( !inState->padded )
	{
		n = inState->aadLen & 15;
		if( n > 0 ) poly1305_update( &inState->poly1305, kZero64, 16 - n );
	}
	
	// Decrypt any non-block-sized data at the end that may have been buffered by chacha20.
	// Note: any pre-decrypted data was already MAC'd before decrypting so we don't need to MAC it here.
	
	nFinal = chacha20_final( &inState->chacha20, inDst );
	if( nFinal > 0 ) inState->dataLen += nFinal;
	
	// MAC padding2 after non-AAD data.
	
	n = inState->dataLen & 15;
	if( n > 0 ) poly1305_update( &inState->poly1305, kZero64, 16 - n );
	
	// MAC AAD and data lengths.
	
	static_analyzer_mem_zeroed( buf, 16 ); // Remove when <radar:15309659> is fixed.
	WriteLittle64( &buf[ 0 ], (uint64_t) inState->aadLen );
	WriteLittle64( &buf[ 8 ], (uint64_t) inState->dataLen );
	poly1305_update( &inState->poly1305, buf, 16 );
	poly1305_final( &inState->poly1305, buf );
	
	*outErr = ( memcmp_constant_time( buf, inAuthTag, 16 ) == 0 ) ? kNoErr : kAuthenticationErr;
	return( nFinal );
}

//===========================================================================================================================
//	chacha20_poly1305_encrypt_all
//===========================================================================================================================

static void
	_chacha20_poly1305_encrypt_all( 
		const uint8_t	inKey[ 32 ], 
		const uint8_t *	inNoncePtr, 
		size_t			inNonceLen, 
		const void *	inAADPtr, 
		size_t			inAADLen, 
		const void *	inPlaintextPtr, 
		size_t			inPlaintextLen, 
		void *			inCiphertextBuf, 
		uint8_t			outAuthTag[ 16 ] );

void
	chacha20_poly1305_encrypt_all_64x64( 
		const uint8_t	inKey[ 32 ], 
		const uint8_t	inNonce[ 8 ], 
		const void *	inAADPtr, 
		size_t			inAADLen, 
		const void *	inPlaintextPtr, 
		size_t			inPlaintextLen, 
		void *			inCiphertextBuf, 
		uint8_t			outAuthTag[ 16 ] )
{
	_chacha20_poly1305_encrypt_all( inKey, inNonce, 8, inAADPtr, inAADLen, inPlaintextPtr, inPlaintextLen, 
		inCiphertextBuf, outAuthTag );
}

void
	chacha20_poly1305_encrypt_all_96x32( 
		const uint8_t	inKey[ 32 ], 
		const uint8_t	inNonce[ 12 ], 
		const void *	inAADPtr, 
		size_t			inAADLen, 
		const void *	inPlaintextPtr, 
		size_t			inPlaintextLen, 
		void *			inCiphertextBuf, 
		uint8_t			outAuthTag[ 16 ] )
{
	_chacha20_poly1305_encrypt_all( inKey, inNonce, 12, inAADPtr, inAADLen, inPlaintextPtr, inPlaintextLen, 
		inCiphertextBuf, outAuthTag );
}

static void
	_chacha20_poly1305_encrypt_all( 
		const uint8_t	inKey[ 32 ], 
		const uint8_t *	inNoncePtr, 
		size_t			inNonceLen, 
		const void *	inAADPtr, 
		size_t			inAADLen, 
		const void *	inPlaintextPtr, 
		size_t			inPlaintextLen, 
		void *			inCiphertextBuf, 
		uint8_t			outAuthTag[ 16 ] )
{
	uint8_t * const				ciphertextPtr = (uint8_t *) inCiphertextBuf;
	chacha20_poly1305_state		state;
	size_t						n;
	uint8_t						buf[ 16 ];
	
	if(      inNonceLen == 8 )	chacha20_poly1305_init_64x64( &state, inKey, inNoncePtr );
	else if( inNonceLen == 12 )	chacha20_poly1305_init_96x32( &state, inKey, inNoncePtr );
	else { DEBUG_HALT(); }
	
	// Encrypt data.
	
	n = chacha20_update( &state.chacha20, inPlaintextPtr, inPlaintextLen, ciphertextPtr );
	n += chacha20_final( &state.chacha20, &ciphertextPtr[ n ] );
	check( n == inPlaintextLen );
	
	// Generate auth tag.
	
	if( inAADLen > 0 ) poly1305_update( &state.poly1305, inAADPtr, inAADLen );
	n = inAADLen & 15;
	if( n > 0 ) poly1305_update( &state.poly1305, kZero64, 16 - n );
	
	if( inPlaintextLen > 0 ) poly1305_update( &state.poly1305, ciphertextPtr, inPlaintextLen );
	n = inPlaintextLen & 15;
	if( n > 0 ) poly1305_update( &state.poly1305, kZero64, 16 - n );
	
	static_analyzer_mem_zeroed( buf, 16 ); // Remove when <radar:15309659> is fixed.
	WriteLittle64( &buf[ 0 ], (uint64_t) inAADLen );
	WriteLittle64( &buf[ 8 ], (uint64_t) inPlaintextLen );
	poly1305_update( &state.poly1305, buf, 16 );
	poly1305_final( &state.poly1305, outAuthTag );
}

//===========================================================================================================================
//	chacha20_poly1305_decrypt_all
//===========================================================================================================================

static OSStatus
	_chacha20_poly1305_decrypt_all( 
		const uint8_t	inKey[ 32 ], 
		const uint8_t *	inNoncePtr, 
		size_t			inNonceLen, 
		const void *	inAADPtr, 
		size_t			inAADLen, 
		const void *	inCiphertextPtr, 
		size_t			inCiphertextLen, 
		void *			inPlaintextBuf, 
		const uint8_t	inAuthTag[ 16 ] );

OSStatus
	chacha20_poly1305_decrypt_all_64x64( 
		const uint8_t	inKey[ 32 ], 
		const uint8_t	inNonce[ 8 ], 
		const void *	inAADPtr, 
		size_t			inAADLen, 
		const void *	inCiphertextPtr, 
		size_t			inCiphertextLen, 
		void *			inPlaintextBuf, 
		const uint8_t	inAuthTag[ 16 ] )
{
	return( _chacha20_poly1305_decrypt_all( inKey, inNonce, 8, inAADPtr, inAADLen, inCiphertextPtr, inCiphertextLen, 
		inPlaintextBuf, inAuthTag ) );
}

OSStatus
	chacha20_poly1305_decrypt_all_96x32( 
		const uint8_t	inKey[ 32 ], 
		const uint8_t	inNonce[ 12 ], 
		const void *	inAADPtr, 
		size_t			inAADLen, 
		const void *	inCiphertextPtr, 
		size_t			inCiphertextLen, 
		void *			inPlaintextBuf, 
		const uint8_t	inAuthTag[ 16 ] )
{
	return( _chacha20_poly1305_decrypt_all( inKey, inNonce, 12, inAADPtr, inAADLen, inCiphertextPtr, inCiphertextLen, 
		inPlaintextBuf, inAuthTag ) );
}

static OSStatus
	_chacha20_poly1305_decrypt_all( 
		const uint8_t	inKey[ 32 ], 
		const uint8_t *	inNoncePtr, 
		size_t			inNonceLen, 
		const void *	inAADPtr, 
		size_t			inAADLen, 
		const void *	inCiphertextPtr, 
		size_t			inCiphertextLen, 
		void *			inPlaintextBuf, 
		const uint8_t	inAuthTag[ 16 ] )
{
	uint8_t * const				plaintextPtr = (uint8_t *) inPlaintextBuf;
	chacha20_poly1305_state		state;
	uint8_t						buf[ 16 ];
	OSStatus					err;
	size_t						n;
	
	if(      inNonceLen == 8 )	chacha20_poly1305_init_64x64( &state, inKey, inNoncePtr );
	else if( inNonceLen == 12 )	chacha20_poly1305_init_96x32( &state, inKey, inNoncePtr );
	else { DEBUG_HALT(); }
	
	// Verify auth tag.
	
	if( inAADLen > 0 ) poly1305_update( &state.poly1305, inAADPtr, inAADLen );
	n = inAADLen & 15;
	if( n > 0 ) poly1305_update( &state.poly1305, kZero64, 16 - n );
	
	if( inCiphertextLen > 0 ) poly1305_update( &state.poly1305, inCiphertextPtr, inCiphertextLen );
	n = inCiphertextLen & 15;
	if( n > 0 ) poly1305_update( &state.poly1305, kZero64, 16 - n );
	
	static_analyzer_mem_zeroed( buf, 16 ); // Remove when <radar:15309659> is fixed.
	WriteLittle64( &buf[ 0 ], (uint64_t) inAADLen );
	WriteLittle64( &buf[ 8 ], (uint64_t) inCiphertextLen );
	poly1305_update( &state.poly1305, buf, 16 );
	poly1305_final( &state.poly1305, buf );
	require_action_quiet( memcmp_constant_time( buf, inAuthTag, 16 ) == 0, exit, err = kAuthenticationErr );
	
	// Decrypt data.
	
	n = chacha20_update( &state.chacha20, inCiphertextPtr, inCiphertextLen, plaintextPtr );
	n += chacha20_final( &state.chacha20, &plaintextPtr[ n ] );
	check( n == inCiphertextLen );
	err = kNoErr;
	
exit:
	return( err );
}

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	chacha20_poly1305_test
//===========================================================================================================================

typedef struct
{
	const char *		label;
	const char *		key;
	const char *		nonce;
	const char *		aad;
	const char *		pt;
	const char *		ct;
	const char *		tag;
	
}	chacha20_poly1305_test_vector;

static OSStatus	chacha20_poly1305_test_one( const chacha20_poly1305_test_vector *inVector, int inPrint );

OSStatus	chacha20_poly1305_test( int inPrint, int inPerf );
OSStatus	chacha20_poly1305_test( int inPrint, int inPerf )
{
	static const chacha20_poly1305_test_vector		kTests[] = 
	{
		// Test vector from <https://tools.ietf.org/html/rfc7539>.
		{
			/* label */	"nir-cfrg #1 (96x32)", 
			/* key */	"1c 92 40 a5 eb 55 d3 8a f3 33 88 86 04 f6 b5 f0"
						"47 39 17 c1 40 2b 80 09 9d ca 5c bc 20 70 75 c0", 
			/* nonce */	"00 00 00 00 01 02 03 04 05 06 07 08", 
			/* aad */	"f3 33 88 86 00 00 00 00 00 00 4e 91", 
			/* pt */	"49 6e 74 65 72 6e 65 74 2d 44 72 61 66 74 73 20"
						"61 72 65 20 64 72 61 66 74 20 64 6f 63 75 6d 65"
						"6e 74 73 20 76 61 6c 69 64 20 66 6f 72 20 61 20"
						"6d 61 78 69 6d 75 6d 20 6f 66 20 73 69 78 20 6d"
						"6f 6e 74 68 73 20 61 6e 64 20 6d 61 79 20 62 65"
						"20 75 70 64 61 74 65 64 2c 20 72 65 70 6c 61 63"
						"65 64 2c 20 6f 72 20 6f 62 73 6f 6c 65 74 65 64"
						"20 62 79 20 6f 74 68 65 72 20 64 6f 63 75 6d 65"
						"6e 74 73 20 61 74 20 61 6e 79 20 74 69 6d 65 2e"
						"20 49 74 20 69 73 20 69 6e 61 70 70 72 6f 70 72"
						"69 61 74 65 20 74 6f 20 75 73 65 20 49 6e 74 65"
						"72 6e 65 74 2d 44 72 61 66 74 73 20 61 73 20 72"
						"65 66 65 72 65 6e 63 65 20 6d 61 74 65 72 69 61"
						"6c 20 6f 72 20 74 6f 20 63 69 74 65 20 74 68 65"
						"6d 20 6f 74 68 65 72 20 74 68 61 6e 20 61 73 20"
						"2f e2 80 9c 77 6f 72 6b 20 69 6e 20 70 72 6f 67"
						"72 65 73 73 2e 2f e2 80 9d", 
			/* ct */	"64 a0 86 15 75 86 1a f4 60 f0 62 c7 9b e6 43 bd"
						"5e 80 5c fd 34 5c f3 89 f1 08 67 0a c7 6c 8c b2"
						"4c 6c fc 18 75 5d 43 ee a0 9e e9 4e 38 2d 26 b0"
						"bd b7 b7 3c 32 1b 01 00 d4 f0 3b 7f 35 58 94 cf"
						"33 2f 83 0e 71 0b 97 ce 98 c8 a8 4a bd 0b 94 81"
						"14 ad 17 6e 00 8d 33 bd 60 f9 82 b1 ff 37 c8 55"
						"97 97 a0 6e f4 f0 ef 61 c1 86 32 4e 2b 35 06 38"
						"36 06 90 7b 6a 7c 02 b0 f9 f6 15 7b 53 c8 67 e4"
						"b9 16 6c 76 7b 80 4d 46 a5 9b 52 16 cd e7 a4 e9"
						"90 40 c5 a4 04 33 22 5e e2 82 a1 b0 a0 6c 52 3e"
						"af 45 34 d7 f8 3f a1 15 5b 00 47 71 8c bc 54 6a"
						"0d 07 2b 04 b3 56 4e ea 1b 42 22 73 f5 48 27 1a"
						"0b b2 31 60 53 fa 76 99 19 55 eb d6 31 59 43 4e"
						"ce bb 4e 46 6d ae 5a 10 73 a6 72 76 27 09 7a 10"
						"49 e6 17 d9 1d 36 10 94 fa 68 f0 ff 77 98 71 30"
						"30 5b ea ba 2e da 04 df 99 7b 71 4d 6c 6f 2c 29"
						"a6 ad 5c b4 02 2b 02 70 9b", 
			/* tag */	"ee ad 9d 67 89 0c bb 22 39 23 36 fe a1 85 1f 38"
		},
		// Test vector from <https://tools.ietf.org/html/rfc7539>.
		{
			/* label */	"nir-cfrg #1 (64x64)", 
			/* key */	"1c 92 40 a5 eb 55 d3 8a f3 33 88 86 04 f6 b5 f0"
						"47 39 17 c1 40 2b 80 09 9d ca 5c bc 20 70 75 c0", 
			/* nonce */	"01 02 03 04 05 06 07 08", 
			/* aad */	"f3 33 88 86 00 00 00 00 00 00 4e 91", 
			/* pt */	"49 6e 74 65 72 6e 65 74 2d 44 72 61 66 74 73 20"
						"61 72 65 20 64 72 61 66 74 20 64 6f 63 75 6d 65"
						"6e 74 73 20 76 61 6c 69 64 20 66 6f 72 20 61 20"
						"6d 61 78 69 6d 75 6d 20 6f 66 20 73 69 78 20 6d"
						"6f 6e 74 68 73 20 61 6e 64 20 6d 61 79 20 62 65"
						"20 75 70 64 61 74 65 64 2c 20 72 65 70 6c 61 63"
						"65 64 2c 20 6f 72 20 6f 62 73 6f 6c 65 74 65 64"
						"20 62 79 20 6f 74 68 65 72 20 64 6f 63 75 6d 65"
						"6e 74 73 20 61 74 20 61 6e 79 20 74 69 6d 65 2e"
						"20 49 74 20 69 73 20 69 6e 61 70 70 72 6f 70 72"
						"69 61 74 65 20 74 6f 20 75 73 65 20 49 6e 74 65"
						"72 6e 65 74 2d 44 72 61 66 74 73 20 61 73 20 72"
						"65 66 65 72 65 6e 63 65 20 6d 61 74 65 72 69 61"
						"6c 20 6f 72 20 74 6f 20 63 69 74 65 20 74 68 65"
						"6d 20 6f 74 68 65 72 20 74 68 61 6e 20 61 73 20"
						"2f e2 80 9c 77 6f 72 6b 20 69 6e 20 70 72 6f 67"
						"72 65 73 73 2e 2f e2 80 9d", 
			/* ct */	"64 a0 86 15 75 86 1a f4 60 f0 62 c7 9b e6 43 bd"
						"5e 80 5c fd 34 5c f3 89 f1 08 67 0a c7 6c 8c b2"
						"4c 6c fc 18 75 5d 43 ee a0 9e e9 4e 38 2d 26 b0"
						"bd b7 b7 3c 32 1b 01 00 d4 f0 3b 7f 35 58 94 cf"
						"33 2f 83 0e 71 0b 97 ce 98 c8 a8 4a bd 0b 94 81"
						"14 ad 17 6e 00 8d 33 bd 60 f9 82 b1 ff 37 c8 55"
						"97 97 a0 6e f4 f0 ef 61 c1 86 32 4e 2b 35 06 38"
						"36 06 90 7b 6a 7c 02 b0 f9 f6 15 7b 53 c8 67 e4"
						"b9 16 6c 76 7b 80 4d 46 a5 9b 52 16 cd e7 a4 e9"
						"90 40 c5 a4 04 33 22 5e e2 82 a1 b0 a0 6c 52 3e"
						"af 45 34 d7 f8 3f a1 15 5b 00 47 71 8c bc 54 6a"
						"0d 07 2b 04 b3 56 4e ea 1b 42 22 73 f5 48 27 1a"
						"0b b2 31 60 53 fa 76 99 19 55 eb d6 31 59 43 4e"
						"ce bb 4e 46 6d ae 5a 10 73 a6 72 76 27 09 7a 10"
						"49 e6 17 d9 1d 36 10 94 fa 68 f0 ff 77 98 71 30"
						"30 5b ea ba 2e da 04 df 99 7b 71 4d 6c 6f 2c 29"
						"a6 ad 5c b4 02 2b 02 70 9b", 
			/* tag */	"ee ad 9d 67 89 0c bb 22 39 23 36 fe a1 85 1f 38"
		}
	};
	static const uint8_t		kTestKey[ 32 ] = 
	{
		0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x33, 0x32, 0x2D, 0x62, 0x79, 0x74, 0x65, 0x20, 
		0x6B, 0x65, 0x79, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x50, 0x6F, 0x6C, 0x79, 0x31, 0x33, 0x30, 0x35
	};
	static const uint8_t		kTestNonce[ 8 ] = 
	{
		0xf3, 0xff, 0xc7, 0x70, 0x3f, 0x94, 0x00, 0xe5
	};
	
	OSStatus					err;
	size_t						i, n;
	chacha20_poly1305_state		state;
	uint64_t					ticks;
	double						d;
	size_t						len;
	uint8_t *					buf = NULL;
	uint8_t *					buf2 = NULL;
	uint8_t						mac[ 16 ];
	
	for( i = 0; i < countof( kTests ); ++i )
	{
		err = chacha20_poly1305_test_one( &kTests[ i ], inPrint );
		require_noerr( err, exit );
	}
	
	if( inPerf )
	{
		// Small performance test (encrypt all-at-once API).
		
		len = 1500;
		buf = (uint8_t *) malloc( len );
		require_action( buf, exit, err = kNoMemoryErr );
		memset( buf, 'a', len );
		
		buf2 = (uint8_t *) malloc( len );
		require_action( buf2, exit, err = kNoMemoryErr );
		memset( buf2, 'b', len );
		
		ticks = UpTicks();
		for( i = 0; i < 100000; ++i )
		{
			chacha20_poly1305_encrypt_all_64x64( kTestKey, kTestNonce, NULL, 0, buf, len, buf2, mac );
		}
		ticks = UpTicks() - ticks;
		d = ( (double) ticks ) / UpTicksPerSecond();
		FPrintF( stderr, "\tchacha20_poly1305 encrypt all (%zu bytes): %f (%f µs, %.2f MB/sec), BUF: %.3H, MAC: %.3H\n", 
			len, d, ( 1000000 * d ) / i, ( i * len ) / ( d * 1048576.0 ), buf2, 16, 16, mac, 16, 16 );
		
		// Small performance test (decrypt all-at-once API).
		
		ticks = UpTicks();
		for( i = 0; i < 100000; ++i )
		{
			err = chacha20_poly1305_decrypt_all_64x64( kTestKey, kTestNonce, NULL, 0, buf2, len, buf, mac );
			require_noerr( err, exit );
		}
		ticks = UpTicks() - ticks;
		d = ( (double) ticks ) / UpTicksPerSecond();
		FPrintF( stderr, "\tchacha20_poly1305 decrypt all good (%zu bytes): %f (%f µs, %.2f MB/sec), BUF: %.3H, MAC: %.3H\n", 
			len, d, ( 1000000 * d ) / i, ( i * len ) / ( d * 1048576.0 ), buf, 16, 16, mac, 16, 16 );
		
		// Small performance test (decrypt all-at-once API when bad).
		
		mac[ 14 ] ^= 1;
		ticks = UpTicks();
		for( i = 0; i < 100000; ++i )
		{
			err = chacha20_poly1305_decrypt_all_64x64( kTestKey, kTestNonce, NULL, 0, buf2, len, buf, mac );
			require_action( err, exit, err = kResponseErr );
		}
		ticks = UpTicks() - ticks;
		d = ( (double) ticks ) / UpTicksPerSecond();
		FPrintF( stderr, "\tchacha20_poly1305 decrypt all bad (%zu bytes): %f (%f µs, %.2f MB/sec), BUF: %.3H, MAC: %.3H\n", 
			len, d, ( 1000000 * d ) / i, ( i * len ) / ( d * 1048576.0 ), buf, 16, 16, mac, 16, 16 );
		
		// Small performance encrypt test.
		
		ticks = UpTicks();
		for( i = 0; i < 100000; ++i )
		{
			chacha20_poly1305_init_64x64( &state, kTestKey, kTestNonce );
			n = chacha20_poly1305_encrypt( &state, buf, len, buf2 );
			chacha20_poly1305_final( &state, &buf2[ n ], mac );
		}
		ticks = UpTicks() - ticks;
		d = ( (double) ticks ) / UpTicksPerSecond();
		FPrintF( stderr, "\tchacha20_poly1305 encrypt (%zu bytes): %f (%f µs, %.2f MB/sec), BUF: %.3H, MAC: %.3H\n", 
			len, d, ( 1000000 * d ) / i, ( i * len ) / ( d * 1048576.0 ), buf2, 16, 16, mac, 16, 16 );
		
		// Small performance decrypt test when good.
		
		ticks = UpTicks();
		for( i = 0; i < 100000; ++i )
		{
			chacha20_poly1305_init_64x64( &state, kTestKey, kTestNonce );
			n = chacha20_poly1305_decrypt( &state, buf2, len, buf );
			chacha20_poly1305_verify( &state, &buf[ n ], mac, &err );
			require_noerr( err, exit );
		}
		ticks = UpTicks() - ticks;
		d = ( (double) ticks ) / UpTicksPerSecond();
		FPrintF( stderr, "\tchacha20_poly1305 decrypt good (%zu bytes): %f (%f µs, %.2f MB/sec), BUF: %.3H, MAC: %.3H\n", 
			len, d, ( 1000000 * d ) / i, ( i * len ) / ( d * 1048576.0 ), buf, 16, 16, mac, 16, 16 );
		
		// Small performance decrypt test when bad.
		
		mac[ 14 ] ^= 1;
		ticks = UpTicks();
		for( i = 0; i < 100000; ++i )
		{
			chacha20_poly1305_init_64x64( &state, kTestKey, kTestNonce );
			n = chacha20_poly1305_decrypt( &state, buf2, len, buf );
			chacha20_poly1305_verify( &state, &buf[ n ], mac, &err );
			require_action( err, exit, err = kResponseErr );
		}
		ticks = UpTicks() - ticks;
		d = ( (double) ticks ) / UpTicksPerSecond();
		FPrintF( stderr, "\tchacha20_poly1305 decrypt bad (%zu bytes): %f (%f µs, %.2f MB/sec), BUF: %.3H, MAC: %.3H\n", 
			len, d, ( 1000000 * d ) / i, ( i * len ) / ( d * 1048576.0 ), buf, 16, 16, mac, 16, 16 );
		
		ForgetMem( &buf );
		ForgetMem( &buf2 );
		
		// Medium performance test.
		
		len = 50000;
		buf = (uint8_t *) malloc( len );
		require_action( buf, exit, err = kNoMemoryErr );
		memset( buf, 'a', len );
		
		ticks = UpTicks();
		for( i = 0; i < 10000; ++i )
		{
			chacha20_poly1305_init_64x64( &state, kTestKey, kTestNonce );
			n = chacha20_poly1305_encrypt( &state, buf, len, buf );
			chacha20_poly1305_final( &state, &buf[ n ], mac );
		}
		ticks = UpTicks() - ticks;
		d = ( (double) ticks ) / UpTicksPerSecond();
		FPrintF( stderr, "\tchacha20_poly1305 (%zu bytes): %f (%f µs, %.2f MB/sec), BUF: %.3H, MAC: %.3H\n", 
			len, d, ( 1000000 * d ) / i, ( i * len ) / ( d * 1048576.0 ), buf, 16, 16, mac, 16, 16 );
		ForgetMem( &buf );
		
		// Big performance test.
		
		len = 5000000;
		buf = (uint8_t *) malloc( len );
		require_action( buf, exit, err = kNoMemoryErr );
		memset( buf, 'a', len );
		
		ticks = UpTicks();
		for( i = 0; i < 100; ++i )
		{
			chacha20_poly1305_init_64x64( &state, kTestKey, kTestNonce );
			n = chacha20_poly1305_encrypt( &state, buf, len, buf );
			chacha20_poly1305_final( &state, &buf[ n ], mac );
		}
		ticks = UpTicks() - ticks;
		d = ( (double) ticks ) / UpTicksPerSecond();
		FPrintF( stderr, "\tchacha20_poly1305 (%zu bytes): %f (%f µs, %.2f MB/sec), BUF: %.3H, MAC: %.3H\n", 
			len, d, ( 1000000 * d ) / i, ( i * len ) / ( d * 1048576.0 ), buf, 16, 16, mac, 16, 16 );
		ForgetMem( &buf );
	}
	err = kNoErr;
	
exit:
	ForgetMem( &buf );
	ForgetMem( &buf2 );
	printf( "chacha20_poly1305_test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

//===========================================================================================================================
//	chacha20_poly1305_test_one
//===========================================================================================================================

static OSStatus	chacha20_poly1305_test_one( const chacha20_poly1305_test_vector *inVector, int inPrint )
{
	OSStatus					err;
	uint8_t *					keyPtr		= NULL;
	size_t						keyLen;
	uint8_t *					noncePtr	= NULL;
	size_t						nonceLen;
	Boolean						use96;
	uint8_t *					aadPtr		= NULL;
	size_t						aadLen;
	uint8_t *					ptPtr		= NULL;
	size_t						ptLen;
	uint8_t *					ctPtr		= NULL;
	size_t						ctLen;
	uint8_t *					tagPtr		= NULL;
	size_t						tagLen;
	chacha20_poly1305_state		state;
	size_t						i, n;
	uint8_t *					buf			= NULL;
	uint8_t						tag[ 16 ];
	
	err = HexToDataCopy( inVector->key, kSizeCString, kHexToData_DefaultFlags, &keyPtr, &keyLen, NULL );
	require_noerr( err, exit );
	require_action( keyLen == 32, exit, err = kSizeErr );
	
	err = HexToDataCopy( inVector->nonce, kSizeCString, kHexToData_DefaultFlags, &noncePtr, &nonceLen, NULL );
	require_noerr( err, exit );
	if(      nonceLen == 12 ) use96 = true;
	else if( nonceLen ==  8 ) use96 = false;
	else { dlogassert( "Bad nonce size" ); err = kSizeErr; goto exit; }
	
	err = HexToDataCopy( inVector->aad, kSizeCString, kHexToData_DefaultFlags, &aadPtr, &aadLen, NULL );
	require_noerr( err, exit );
	
	err = HexToDataCopy( inVector->pt, kSizeCString, kHexToData_DefaultFlags, &ptPtr, &ptLen, NULL );
	require_noerr( err, exit );
	
	err = HexToDataCopy( inVector->ct, kSizeCString, kHexToData_DefaultFlags, &ctPtr, &ctLen, NULL );
	require_noerr( err, exit );
	
	err = HexToDataCopy( inVector->tag, kSizeCString, kHexToData_DefaultFlags, &tagPtr, &tagLen, NULL );
	require_noerr( err, exit );
	require_action( tagLen == 16, exit, err = kSizeErr );
	
	buf = (uint8_t *) malloc( ctLen ? ctLen : 1 );
	require_action( buf, exit, err = kNoMemoryErr );
	
	// All-at-once test using the one-shot API.
	
	memset( buf, 'a', ctLen );
	memset( tag, 'b', sizeof( tag ) );
	if( use96 )	chacha20_poly1305_encrypt_all_96x32( keyPtr, noncePtr, aadPtr, aadLen, ptPtr, ptLen, buf, tag );
	else		chacha20_poly1305_encrypt_all_64x64( keyPtr, noncePtr, aadPtr, aadLen, ptPtr, ptLen, buf, tag );
	require_action( memcmp( buf, ctPtr, ctLen ) == 0, exit, err = kMismatchErr );
	require_action( memcmp( tag, tagPtr, tagLen ) == 0, exit, err = kMismatchErr );

	if( use96 )	err = chacha20_poly1305_decrypt_all_96x32( keyPtr, noncePtr, aadPtr, aadLen, ctPtr, ctLen, buf, tagPtr );
	else		err = chacha20_poly1305_decrypt_all_64x64( keyPtr, noncePtr, aadPtr, aadLen, ctPtr, ctLen, buf, tagPtr );
	require_noerr( err, exit );
	require_action( memcmp( buf, ptPtr, ptLen ) == 0, exit, err = kMismatchErr );
	
	// All-at-once test using the update API.
	
	memset( buf, 'a', ctLen );
	memset( tag, 'b', sizeof( tag ) );
	if( use96 )	chacha20_poly1305_init_96x32( &state, keyPtr, noncePtr );
	else		chacha20_poly1305_init_64x64( &state, keyPtr, noncePtr );
	chacha20_poly1305_add_aad( &state, aadPtr, aadLen );
	n  = chacha20_poly1305_encrypt( &state, ptPtr, ptLen, buf );
	n += chacha20_poly1305_final( &state, &buf[ n ], tag );
	if( inPrint )
	{
		FPrintF( stderr, "\t--> %s\n", inVector->label );
		FPrintF( stderr, "\tKEY:   %.3H\n", keyPtr, (int) keyLen, (int) keyLen );
		FPrintF( stderr, "\tNONCE: %.3H\n", noncePtr, (int) nonceLen, (int) nonceLen );
		FPrintF( stderr, "\tAAD:   %.3H\n", aadPtr, (int) aadLen, (int) aadLen );
		FPrintF( stderr, "\tCT:    %.3H\n", ctPtr, (int) ctLen, (int) ctLen );
		FPrintF( stderr, "\tCT ?:  %.3H\n", buf, (int) n, (int) n );
		FPrintF( stderr, "\tTAG:   %.3H\n", tagPtr, (int) tagLen, (int) tagLen );
		FPrintF( stderr, "\tTAG ?: %.3H\n", tag, 16, 16 );
		FPrintF( stderr, "\tPT:    %.3H\n", ptPtr, (int) ptLen, (int) ptLen );
	}
	require_action( n == ctLen, exit, err = kSizeErr );
	require_action( memcmp( buf, ctPtr, ctLen ) == 0, exit, err = kMismatchErr );
	require_action( memcmp( tag, tagPtr, tagLen ) == 0, exit, err = kMismatchErr );
	
	memset( buf, 'a', ctLen );
	memset( tag, 'b', sizeof( tag ) );
	if( use96 )	chacha20_poly1305_init_96x32( &state, keyPtr, noncePtr );
	else		chacha20_poly1305_init_64x64( &state, keyPtr, noncePtr );
	chacha20_poly1305_add_aad( &state, aadPtr, aadLen );
	n = chacha20_poly1305_decrypt( &state, ctPtr, ctLen, buf );
	err = -1;
	n += chacha20_poly1305_verify( &state, &buf[ n ], tagPtr, &err );
	if( inPrint )
	{
		FPrintF( stderr, "\tPT ?:  %.3H\n", buf, (int) n, (int) n );
		FPrintF( stderr, "\n" );
	}
	require_noerr( err, exit );
	require_action( n == ptLen, exit, err = kSizeErr );
	require_action( memcmp( buf, ptPtr, ptLen ) == 0, exit, err = kMismatchErr );
	
	// Byte-by-byte test using the update API.
	
	memset( buf, 'a', ctLen );
	memset( tag, 'b', sizeof( tag ) );
	if( use96 )	chacha20_poly1305_init_96x32( &state, keyPtr, noncePtr );
	else		chacha20_poly1305_init_64x64( &state, keyPtr, noncePtr );
	for( i = 0; i < aadLen; ++i ) chacha20_poly1305_add_aad( &state, &aadPtr[ i ], 1 );
	n = 0;
	for( i = 0; i < ptLen; ++i ) n += chacha20_poly1305_encrypt( &state, &ptPtr[ i ], 1, &buf[ n ] );
	n += chacha20_poly1305_final( &state, &buf[ n ], tag );
	require_action( n == ctLen, exit, err = kSizeErr );
	require_action( memcmp( buf, ctPtr, ctLen ) == 0, exit, err = kMismatchErr );
	require_action( memcmp( tag, tagPtr, tagLen ) == 0, exit, err = kMismatchErr );
	
	memset( buf, 'a', ctLen );
	memset( tag, 'b', sizeof( tag ) );
	if( use96 )	chacha20_poly1305_init_96x32( &state, keyPtr, noncePtr );
	else		chacha20_poly1305_init_64x64( &state, keyPtr, noncePtr );
	for( i = 0; i < aadLen; ++i ) chacha20_poly1305_add_aad( &state, &aadPtr[ i ], 1 );
	n = 0;
	for( i = 0; i < ctLen; ++i ) n += chacha20_poly1305_decrypt( &state, &ctPtr[ i ], 1, &buf[ n ] );
	err = -1;
	n += chacha20_poly1305_verify( &state, &buf[ n ], tagPtr, &err );
	require_noerr( err, exit );
	require_action( n == ptLen, exit, err = kSizeErr );
	require_action( memcmp( buf, ptPtr, ptLen ) == 0, exit, err = kMismatchErr );
	
exit:
	FreeNullSafe( keyPtr );
	FreeNullSafe( noncePtr );
	FreeNullSafe( aadPtr );
	FreeNullSafe( ptPtr );
	FreeNullSafe( ctPtr );
	FreeNullSafe( tagPtr );
	FreeNullSafe( buf );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	ChaCha20Poly1305Test
//===========================================================================================================================

OSStatus	ChaCha20Poly1305Test( int inPrint, int inPerf )
{
	OSStatus			err;
	
	err = chacha20_test( inPerf );
	require_noerr( err, exit );
	
	err = poly1305_test( inPerf );
	require_noerr( err, exit );
	
	err = chacha20_poly1305_test( inPrint, inPerf );
	require_noerr( err, exit );
	
exit:
	printf( "ChaCha20Poly1305Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS
