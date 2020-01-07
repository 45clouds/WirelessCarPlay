/*
	File:    	CryptoHashUtils.c
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

#include "CryptoHashUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static void	_MD5Init( CryptoHashContext *ctx );
static void _MD5Update( CryptoHashContext *ctx, const void *inData, size_t inLen );
static void	_MD5Final( CryptoHashContext *ctx, uint8_t *inDigestBuffer );

static void	_SHA1Init( CryptoHashContext *ctx );
static void _SHA1Update( CryptoHashContext *ctx, const void *inData, size_t inLen );
static void	_SHA1Final( CryptoHashContext *ctx, uint8_t *inDigestBuffer );

static void	_SHA512Init( CryptoHashContext *ctx );
static void _SHA512Update( CryptoHashContext *ctx, const void *inData, size_t inLen );
static void	_SHA512Final( CryptoHashContext *ctx, uint8_t *inDigestBuffer );

static void	_SHA3Init( CryptoHashContext *ctx );
static void _SHA3Update( CryptoHashContext *ctx, const void *inData, size_t inLen );
static void	_SHA3Final( CryptoHashContext *ctx, uint8_t *inDigestBuffer );

//===========================================================================================================================
//	Descriptors
//===========================================================================================================================

typedef void ( *CryptoHashInit_f )( CryptoHashContext *ctx );
typedef void ( *CryptoHashUpdate_f )( CryptoHashContext *ctx, const void *inData, size_t inLen );
typedef void ( *CryptoHashFinal_f )( CryptoHashContext *ctx, uint8_t *inDigestBuffer );

struct CryptoHashDescriptorPrivate
{
	CryptoHashInit_f		init_f;
	CryptoHashUpdate_f		update_f;
	CryptoHashFinal_f		final_f;
	size_t					digestLen;
	size_t					blockLen;
};

// MD5

const struct CryptoHashDescriptorPrivate		_kCryptoHashDescriptor_MD5 = 
{
	_MD5Init, _MD5Update, _MD5Final, 16, 64
};
CryptoHashDescriptorRef							kCryptoHashDescriptor_MD5 = &_kCryptoHashDescriptor_MD5;

// SHA-1

const struct CryptoHashDescriptorPrivate		_kCryptoHashDescriptor_SHA1 = 
{
	_SHA1Init, _SHA1Update, _SHA1Final, SHA_DIGEST_LENGTH, 64
};
CryptoHashDescriptorRef							kCryptoHashDescriptor_SHA1 = &_kCryptoHashDescriptor_SHA1;

// SHA-512

const struct CryptoHashDescriptorPrivate		_kCryptoHashDescriptor_SHA512 = 
{
	_SHA512Init, _SHA512Update, _SHA512Final, SHA512_DIGEST_LENGTH, 128
};
CryptoHashDescriptorRef							kCryptoHashDescriptor_SHA512 = &_kCryptoHashDescriptor_SHA512;

// SHA-3

const struct CryptoHashDescriptorPrivate		_kCryptoHashDescriptor_SHA3 = 
{
	_SHA3Init, _SHA3Update, _SHA3Final, SHA3_DIGEST_LENGTH, SHA3_BLOCK_SIZE
};
CryptoHashDescriptorRef							kCryptoHashDescriptor_SHA3 = &_kCryptoHashDescriptor_SHA3;

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	CryptoHashInit
//===========================================================================================================================

void	CryptoHashInit( CryptoHashContext *ctx, CryptoHashDescriptorRef inDesc )
{
	ctx->desc = inDesc;
	inDesc->init_f( ctx );
}

//===========================================================================================================================
//	CryptoHashUpdate
//===========================================================================================================================

void	CryptoHashUpdate( CryptoHashContext *ctx, const void *inData, size_t inLen )
{
	ctx->desc->update_f( ctx, inData, inLen );
}

//===========================================================================================================================
//	CryptoHashFinal
//===========================================================================================================================

void	CryptoHashFinal( CryptoHashContext *ctx, uint8_t *inDigestBuffer )
{
	ctx->desc->final_f( ctx, inDigestBuffer );
}

//===========================================================================================================================
//	CryptoHashOneShot
//===========================================================================================================================

void	CryptoHashOneShot( CryptoHashDescriptorRef inDesc, const void *inData, size_t inLen, uint8_t *inDigestBuffer )
{
	CryptoHashContext		ctx;
	
	ctx.desc = inDesc;
	inDesc->init_f( &ctx );
	inDesc->update_f( &ctx, inData, inLen );
	inDesc->final_f( &ctx, inDigestBuffer );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	CryptoHMACInit
//===========================================================================================================================

void	CryptoHMACInit( CryptoHMACContext *ctx, CryptoHashDescriptorRef inDesc, const void *inKeyPtr, size_t inKeyLen )
{
	const uint8_t *		keyPtr = (const uint8_t *) inKeyPtr;
	uint8_t				ipad[ kCryptoHashMaxBlockLen ];
	uint8_t				tempKey[ kCryptoHashMaxDigestLen ];
	size_t				i;
	uint8_t				b;
	
	check( inDesc->blockLen <= kCryptoHashMaxBlockLen );
	check( inDesc->digestLen <= kCryptoHashMaxDigestLen );
	
	if( inKeyLen > inDesc->blockLen )
	{
		CryptoHashInit( &ctx->hashCtx, inDesc );
		CryptoHashUpdate( &ctx->hashCtx, inKeyPtr, inKeyLen );
		CryptoHashFinal( &ctx->hashCtx, tempKey );
		keyPtr   = tempKey;
		inKeyLen = inDesc->digestLen;
	}
	for( i = 0; i < inKeyLen; ++i )
	{
		b = keyPtr[ i ];
		ipad[ i ]		= b ^ 0x36;
		ctx->opad[ i ]	= b ^ 0x5C;
	}
	for( ; i < inDesc->blockLen; ++i )
	{
		ipad[ i ]		= 0x36;
		ctx->opad[ i ]	= 0x5C;
	}
	CryptoHashInit( &ctx->hashCtx, inDesc );
	CryptoHashUpdate( &ctx->hashCtx, ipad, inDesc->blockLen );
}

//===========================================================================================================================
//	CryptoHMACUpdate
//===========================================================================================================================

void	CryptoHMACUpdate( CryptoHMACContext *ctx, const void *inPtr, size_t inLen )
{
	CryptoHashUpdate( &ctx->hashCtx, inPtr, inLen );
}

//===========================================================================================================================
//	CryptoHMACFinal
//===========================================================================================================================

void	CryptoHMACFinal( CryptoHMACContext *ctx, uint8_t *outDigest )
{
	CryptoHashDescriptorRef		desc;
	
	desc = ctx->hashCtx.desc;
	CryptoHashFinal( &ctx->hashCtx, outDigest );
	CryptoHashInit( &ctx->hashCtx, desc );
	CryptoHashUpdate( &ctx->hashCtx, ctx->opad, desc->blockLen );
	CryptoHashUpdate( &ctx->hashCtx, outDigest, desc->digestLen );
	CryptoHashFinal( &ctx->hashCtx, outDigest );
}

//===========================================================================================================================
//	CryptoHMACOneShot
//===========================================================================================================================

void
	CryptoHMACOneShot( 
		CryptoHashDescriptorRef	inDesc, 
		const void *			inKeyPtr, 
		size_t					inKeyLen, 
		const void *			inMsgPtr, 
		size_t					inMsgLen, 
		uint8_t *				outDigest )
{
	CryptoHMACContext		ctx;
	
	CryptoHMACInit( &ctx, inDesc, inKeyPtr, inKeyLen );
	CryptoHMACUpdate( &ctx, inMsgPtr, inMsgLen );
	CryptoHMACFinal( &ctx, outDigest );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	CryptoHKDF
//===========================================================================================================================

void
	CryptoHKDF( 
		CryptoHashDescriptorRef	inDesc, 
		const void *			inInputKeyPtr,	size_t inInputKeyLen, 
		const void *			inSaltPtr,		size_t inSaltLen, 
		const void *			inInfoPtr,		size_t inInfoLen, 
		size_t					inOutputLen, 	uint8_t *outKey )
{
	uint8_t					nullSalt[ kCryptoHashMaxDigestLen ];
	uint8_t					key[ kCryptoHashMaxDigestLen ];
	CryptoHMACContext		hmacCtx;
	size_t					i, n, offset, Tlen;
	uint8_t					T[ kCryptoHashMaxDigestLen ];
	uint8_t					b;
	
	check( inDesc->digestLen <= kCryptoHashMaxDigestLen );
	
	// Extract phase.
	
	if( inSaltLen == 0 )
	{
		memset( nullSalt, 0, inDesc->digestLen );
		inSaltPtr = nullSalt;
		inSaltLen = inDesc->digestLen;
	}
	CryptoHMACOneShot( inDesc, inSaltPtr, inSaltLen, inInputKeyPtr, inInputKeyLen, key );
	
	// Expand phase.
	
	n = ( inOutputLen / inDesc->digestLen ) + ( ( inOutputLen % inDesc->digestLen ) ? 1 : 0 );
	check( n <= 255 );
	Tlen = 0;
	offset = 0;
	for( i = 1; i <= n; ++i )
	{
		CryptoHMACInit( &hmacCtx, inDesc, key, inDesc->digestLen );
		CryptoHMACUpdate( &hmacCtx, T, Tlen );
		CryptoHMACUpdate( &hmacCtx, inInfoPtr, inInfoLen );
		b = (uint8_t) i;
		CryptoHMACUpdate( &hmacCtx, &b, 1 );
		CryptoHMACFinal( &hmacCtx, T );
		memcpy( &outKey[ offset ], T, ( i != n ) ? inDesc->digestLen : ( inOutputLen - offset ) );
		offset += inDesc->digestLen;
		Tlen = inDesc->digestLen;
	}
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	MD5
//===========================================================================================================================

static void	_MD5Init( CryptoHashContext *ctx )
{
	check( ctx->desc == kCryptoHashDescriptor_MD5 );
	MD5_Init( &ctx->state.md5 );
}

static void _MD5Update( CryptoHashContext *ctx, const void *inData, size_t inLen )
{
	check( ctx->desc == kCryptoHashDescriptor_MD5 );
	MD5_Update( &ctx->state.md5, inData, inLen );
}

static void	_MD5Final( CryptoHashContext *ctx, uint8_t *inDigestBuffer )
{
	check( ctx->desc == kCryptoHashDescriptor_MD5 );
	MD5_Final( inDigestBuffer, &ctx->state.md5 );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	SHA1
//===========================================================================================================================

static void	_SHA1Init( CryptoHashContext *ctx )
{
	check( ctx->desc == kCryptoHashDescriptor_SHA1 );
	SHA1_Init( &ctx->state.sha1 );
}

static void _SHA1Update( CryptoHashContext *ctx, const void *inData, size_t inLen )
{
	check( ctx->desc == kCryptoHashDescriptor_SHA1 );
	SHA1_Update( &ctx->state.sha1, inData, inLen );
}

static void	_SHA1Final( CryptoHashContext *ctx, uint8_t *inDigestBuffer )
{
	check( ctx->desc == kCryptoHashDescriptor_SHA1 );
	SHA1_Final( inDigestBuffer, &ctx->state.sha1 );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	SHA512
//===========================================================================================================================

static void	_SHA512Init( CryptoHashContext *ctx )
{
	check( ctx->desc == kCryptoHashDescriptor_SHA512 );
	SHA512_Init( &ctx->state.sha512 );
}

static void _SHA512Update( CryptoHashContext *ctx, const void *inData, size_t inLen )
{
	check( ctx->desc == kCryptoHashDescriptor_SHA512 );
	SHA512_Update( &ctx->state.sha512, inData, inLen );
}

static void	_SHA512Final( CryptoHashContext *ctx, uint8_t *inDigestBuffer )
{
	check( ctx->desc == kCryptoHashDescriptor_SHA512 );
	SHA512_Final( inDigestBuffer, &ctx->state.sha512 );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	SHA3
//===========================================================================================================================

static void	_SHA3Init( CryptoHashContext *ctx )
{
	check( ctx->desc == kCryptoHashDescriptor_SHA3 );
	SHA3_Init( &ctx->state.sha3 );
}

static void _SHA3Update( CryptoHashContext *ctx, const void *inData, size_t inLen )
{
	check( ctx->desc == kCryptoHashDescriptor_SHA3 );
	SHA3_Update( &ctx->state.sha3, inData, inLen );
}

static void	_SHA3Final( CryptoHashContext *ctx, uint8_t *inDigestBuffer )
{
	check( ctx->desc == kCryptoHashDescriptor_SHA3 );
	SHA3_Final( inDigestBuffer, &ctx->state.sha3 );
}

#if 0
#pragma mark -
#pragma mark == Testing ==
#endif

//===========================================================================================================================
//	Testing
//===========================================================================================================================

#if( !EXCLUDE_UNIT_TESTS )

#include "TestUtils.h"

//===========================================================================================================================
//	Test Vectors
//===========================================================================================================================

#define kSrcFlag_None		0
#define kSrcFlag_Repeat		( 1U << 0 )

typedef struct
{
	CryptoHashDescriptorRef		desc;
	const char *				srcPtr;
	size_t						srcLen;
	uint32_t					srcFlags;
	const char *				digestPtr;
	size_t						digestLen;
	
}	CryptoHashTestVector;

static const CryptoHashTestVector		kCryptoHashTestVectors[] = 
{
	// MD5 Tests from RFC 1321.
	
	{
		&_kCryptoHashDescriptor_MD5, 
		"", 
		kSizeCString, kSrcFlag_None, 
		"\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04\xe9\x80\x09\x98\xec\xf8\x42\x7e", 
		16
	},
	{
		&_kCryptoHashDescriptor_MD5, 
		"a", 
		kSizeCString, kSrcFlag_None, 
		"\x0c\xc1\x75\xb9\xc0\xf1\xb6\xa8\x31\xc3\x99\xe2\x69\x77\x26\x61", 
		16
	},
	{
		&_kCryptoHashDescriptor_MD5, 
		"abc", 
		kSizeCString, kSrcFlag_None, 
		"\x90\x01\x50\x98\x3c\xd2\x4f\xb0\xd6\x96\x3f\x7d\x28\xe1\x7f\x72", 
		16
	},
	{
		&_kCryptoHashDescriptor_MD5, 
		"message digest", 
		kSizeCString, kSrcFlag_None, 
		"\xf9\x6b\x69\x7d\x7c\xb7\x93\x8d\x52\x5a\x2f\x31\xaa\xf1\x61\xd0", 
		16
	},
	{
		&_kCryptoHashDescriptor_MD5, 
		"abcdefghijklmnopqrstuvwxyz", 
		kSizeCString, kSrcFlag_None, 
		"\xc3\xfc\xd3\xd7\x61\x92\xe4\x00\x7d\xfb\x49\x6c\xca\x67\xe1\x3b", 
		16
	},
	{
		&_kCryptoHashDescriptor_MD5, 
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 
		kSizeCString, kSrcFlag_None, 
		"\xd1\x74\xab\x98\xd2\x77\xd9\xf5\xa5\x61\x1c\x2c\x9f\x41\x9d\x9f", 
		16
	},
	{
		&_kCryptoHashDescriptor_MD5, 
		"12345678901234567890123456789012345678901234567890123456789012345678901234567890", 
		kSizeCString, kSrcFlag_None, 
		"\x57\xed\xf4\xa2\x2b\xe3\xc9\x55\xac\x49\xda\x2e\x21\x07\xb6\x7a", 
		16
	},
	
	// SHA-1 tests from NIST.
	
	{
		&_kCryptoHashDescriptor_SHA1, 
		"abc", 
		kSizeCString, kSrcFlag_None, 
		"\xa9\x99\x3e\x36\x47\x06\x81\x6a\xba\x3e\x25\x71\x78\x50\xc2\x6c\x9c\xd0\xd8\x9d", 
		20
	},
	{
		&_kCryptoHashDescriptor_SHA1, 
		"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 
		kSizeCString, kSrcFlag_None, 
		"\x84\x98\x3E\x44\x1C\x3B\xD2\x6E\xBA\xAE\x4A\xA1\xF9\x51\x29\xE5\xE5\x46\x70\xF1", 
		20
	},
	{
		&_kCryptoHashDescriptor_SHA1, 
		"a", 
		1000000, kSrcFlag_Repeat, 
		"\x34\xaa\x97\x3c\xd4\xc4\xda\xa4\xf6\x1e\xeb\x2b\xdb\xad\x27\x31\x65\x34\x01\x6f", 
		20
	},
	
	// SHA-512 tests from LibTom.
	
	{
		&_kCryptoHashDescriptor_SHA512, 
		"abc", 
		kSizeCString, kSrcFlag_None, 
		"\xdd\xaf\x35\xa1\x93\x61\x7a\xba\xcc\x41\x73\x49\xae\x20\x41\x31"
		"\x12\xe6\xfa\x4e\x89\xa9\x7e\xa2\x0a\x9e\xee\xe6\x4b\x55\xd3\x9a"
		"\x21\x92\x99\x2a\x27\x4f\xc1\xa8\x36\xba\x3c\x23\xa3\xfe\xeb\xbd"
		"\x45\x4d\x44\x23\x64\x3c\xe8\x0e\x2a\x9a\xc9\x4f\xa5\x4c\xa4\x9f", 
		64
	},
	{
		&_kCryptoHashDescriptor_SHA512, 
		"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", 
		kSizeCString, kSrcFlag_None, 
		"\x8e\x95\x9b\x75\xda\xe3\x13\xda\x8c\xf4\xf7\x28\x14\xfc\x14\x3f"
		"\x8f\x77\x79\xc6\xeb\x9f\x7f\xa1\x72\x99\xae\xad\xb6\x88\x90\x18"
		"\x50\x1d\x28\x9e\x49\x00\xf7\xe4\x33\x1b\x99\xde\xc4\xb5\x43\x3a"
		"\xc7\xd3\x29\xee\xb6\xdd\x26\x54\x5e\x96\xe5\x5b\x87\x4b\xe9\x09",
		64
	},
	
	// SHA-512 tests from Wikipedia.
	
	{
		&_kCryptoHashDescriptor_SHA512, 
		"The quick brown fox jumps over the lazy dog.", 
		kSizeCString, kSrcFlag_None, 
		"\x91\xEA\x12\x45\xF2\x0D\x46\xAE\x9A\x03\x7A\x98\x9F\x54\xF1\xF7"
		"\x90\xF0\xA4\x76\x07\xEE\xB8\xA1\x4D\x12\x89\x0C\xEA\x77\xA1\xBB"
		"\xC6\xC7\xED\x9C\xF2\x05\xE6\x7B\x7F\x2B\x8F\xD4\xC7\xDF\xD3\xA7"
		"\xA8\x61\x7E\x45\xF3\xC4\x63\xD4\x81\xC7\xE5\x86\xC3\x9A\xC1\xED",
		64
	},
	{
		&_kCryptoHashDescriptor_SHA512, 
		"a", 
		1000000, kSrcFlag_Repeat, 
		"\xE7\x18\x48\x3D\x0C\xE7\x69\x64\x4E\x2E\x42\xC7\xBC\x15\xB4\x63"
		"\x8E\x1F\x98\xB1\x3B\x20\x44\x28\x56\x32\xA8\x03\xAF\xA9\x73\xEB"
		"\xDE\x0F\xF2\x44\x87\x7E\xA6\x0A\x4C\xB0\x43\x2C\xE5\x77\xC3\x1B"
		"\xEB\x00\x9C\x5C\x2C\x49\xAA\x2E\x4E\xAD\xB2\x17\xAD\x8C\xC0\x9B",
		64
	},
	
	// SHA-3 tests from <http://en.wikipedia.org/wiki/SHA-3>.
	
	{
		&_kCryptoHashDescriptor_SHA3, 
		"", 
		kSizeCString, kSrcFlag_None, 
		"\xA6\x9F\x73\xCC\xA2\x3A\x9A\xC5\xC8\xB5\x67\xDC\x18\x5A\x75\x6E"
		"\x97\xC9\x82\x16\x4F\xE2\x58\x59\xE0\xD1\xDC\xC1\x47\x5C\x80\xA6"
		"\x15\xB2\x12\x3A\xF1\xF5\xF9\x4C\x11\xE3\xE9\x40\x2C\x3A\xC5\x58"
		"\xF5\x00\x19\x9D\x95\xB6\xD3\xE3\x01\x75\x85\x86\x28\x1D\xCD\x26",
		64
	},
	{
		&_kCryptoHashDescriptor_SHA3, 
		"The quick brown fox jumps over the lazy dog", 
		kSizeCString, kSrcFlag_None, 
		"\x01\xDE\xDD\x5D\xE4\xEF\x14\x64\x24\x45\xBA\x5F\x5B\x97\xC1\x5E"
		"\x47\xB9\xAD\x93\x13\x26\xE4\xB0\x72\x7C\xD9\x4C\xEF\xC4\x4F\xFF"
		"\x23\xF0\x7B\xF5\x43\x13\x99\x39\xB4\x91\x28\xCA\xF4\x36\xDC\x1B"
		"\xDE\xE5\x4F\xCB\x24\x02\x3A\x08\xD9\x40\x3F\x9B\x4B\xF0\xD4\x50",
		64
	},
	{
		&_kCryptoHashDescriptor_SHA3, 
		"The quick brown fox jumps over the lazy dog.", 
		kSizeCString, kSrcFlag_None, 
		"\x18\xF4\xF4\xBD\x41\x96\x03\xF9\x55\x38\x83\x70\x03\xD9\xD2\x54"
		"\xC2\x6C\x23\x76\x55\x65\x16\x22\x47\x48\x3F\x65\xC5\x03\x03\x59"
		"\x7B\xC9\xCE\x4D\x28\x9F\x21\xD1\xC2\xF1\xF4\x58\x82\x8E\x33\xDC"
		"\x44\x21\x00\x33\x1B\x35\xE7\xEB\x03\x1B\x5D\x38\xBA\x64\x60\xF8",
		64
	},
	
	// SHA-3 tests from NIST's ShortMsgKAT_SHA3-512.txt.
	
	{
		&_kCryptoHashDescriptor_SHA3, 
		"\xCC", 
		1, kSrcFlag_None, 
		"\x39\x39\xFC\xC8\xB5\x7B\x63\x61\x25\x42\xDA\x31\xA8\x34\xE5\xDC"
		"\xC3\x6E\x2E\xE0\xF6\x52\xAC\x72\xE0\x26\x24\xFA\x2E\x5A\xDE\xEC"
		"\xC7\xDD\x6B\xB3\x58\x02\x24\xB4\xD6\x13\x87\x06\xFC\x6E\x80\x59"
		"\x7B\x52\x80\x51\x23\x0B\x00\x62\x1C\xC2\xB2\x29\x99\xEA\xA2\x05",
		64
	},
	{
		&_kCryptoHashDescriptor_SHA3, 
		"\x4A\x4F\x20\x24\x84\x51\x25\x26", 
		8, kSrcFlag_None, 
		"\x15\x0D\x78\x7D\x6E\xB4\x96\x70\xC2\xA4\xCC\xD1\x7E\x6C\xCE\x7A"
		"\x04\xC1\xFE\x30\xFC\xE0\x3D\x1E\xF2\x50\x17\x52\xD9\x2A\xE0\x4C"
		"\xB3\x45\xFD\x42\xE5\x10\x38\xC8\x3B\x2B\x4F\x8F\xD4\x38\xD1\xB4"
		"\xB5\x5C\xC5\x88\xC6\xB9\x13\x13\x2F\x1A\x65\x8F\xB1\x22\xCB\x52",
		64
	},
	{
		&_kCryptoHashDescriptor_SHA3, 
		"\xEE\xD7\x42\x22\x27\x61\x3B\x6F\x53\xC9", 
		10, kSrcFlag_None, 
		"\x5A\x56\x6F\xB1\x81\xBE\x53\xA4\x10\x92\x75\x53\x7D\x80\xE5\xFD"
		"\x0F\x31\x4D\x68\x88\x45\x29\xCA\x66\xB8\xB0\xE9\xF2\x40\xA6\x73"
		"\xB6\x4B\x28\xFF\xFE\x4C\x1E\xC4\xA5\xCE\xF0\xF4\x30\x22\x9C\x57"
		"\x57\xEB\xD1\x72\xB4\xB0\xB6\x8A\x81\xD8\xC5\x8A\x9E\x96\xE1\x64",
		64
	},
	{
		&_kCryptoHashDescriptor_SHA3, 
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
		255, kSrcFlag_None, 
		"\x6E\x8B\x8B\xD1\x95\xBD\xD5\x60\x68\x9A\xF2\x34\x8B\xDC\x74\xAB"
		"\x7C\xD0\x5E\xD8\xB9\xA5\x77\x11\xE9\xBE\x71\xE9\x72\x6F\xDA\x45"
		"\x91\xFE\xE1\x22\x05\xED\xAC\xAF\x82\xFF\xBB\xAF\x16\xDF\xF9\xE7"
		"\x02\xA7\x08\x86\x20\x80\x16\x6C\x2F\xF6\xBA\x37\x9B\xC7\xFF\xC2",
		64
	},
};

//===========================================================================================================================
//	HMAC
//===========================================================================================================================

typedef struct
{
	CryptoHashDescriptorRef		desc;
	const void *				keyPtr;
	size_t						keyLen;
	const void *				dataPtr;
	size_t						dataLen;
	const void *				digestPtr;
	size_t						digestLen;
	
}	CryptoHMACTestVector;

static const CryptoHMACTestVector		kCryptoHMACTestVectors[] = 
{
	// Test vectors from <http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/HMAC_SHA1.pdf>.
	
	// Test Case 1
	{
		/* desc */		&_kCryptoHashDescriptor_SHA1, 
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
		/* desc */		&_kCryptoHashDescriptor_SHA1, 
		/* key */		"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13", 
						20, 
		/* data */		"Sample message for keylen<blocklen", 
						34, 
		/* digest */	"\x4C\x99\xFF\x0C\xB1\xB3\x1B\xD3\x3F\x84\x31\xDB\xAF\x4D\x17\xFC\xD3\x56\xA8\x07", 
						20
	}, 
	// Test Case 3
	{
		/* desc */		&_kCryptoHashDescriptor_SHA1, 
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
		/* desc */		&_kCryptoHashDescriptor_SHA1, 
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
	
	// SHA-512 test vectors from RFC 4231.
	
	// Test Case 1
	{
		/* desc */		&_kCryptoHashDescriptor_SHA512, 
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
		/* desc */		&_kCryptoHashDescriptor_SHA512, 
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
		/* desc */		&_kCryptoHashDescriptor_SHA512, 
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
		/* desc */		&_kCryptoHashDescriptor_SHA512, 
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
		/* desc */		&_kCryptoHashDescriptor_SHA512, 
		/* key */		"\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C", 
						20, 
		/* data */		"\x54\x65\x73\x74\x20\x57\x69\x74\x68\x20\x54\x72\x75\x6E\x63\x61\x74\x69\x6F\x6E", 
						20, 
		/* digest */	"\x41\x5F\xAD\x62\x71\x58\x0A\x53\x1D\x41\x79\xBC\x89\x1D\x87\xA6", 
						16
	}, 
	// Test Case 6
	{
		/* desc */		&_kCryptoHashDescriptor_SHA512, 
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
		/* desc */		&_kCryptoHashDescriptor_SHA512, 
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
	
	// SHA-3 test vectors converted from the SHA-512 versions in RFC 4231.
	
	// Test Case 1
	{
		/* desc */		&_kCryptoHashDescriptor_SHA3, 
		/* key */		"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b", 
						20, 
		/* data */		"\x48\x69\x20\x54\x68\x65\x72\x65", 
						8, 
		/* digest */	"\xEB\x3F\xBD\x4B\x2E\xAA\xB8\xF5\xC5\x04\xBD\x3A\x41\x46\x5A\xAC"
						"\xEC\x15\x77\x0A\x7C\xAB\xAC\x53\x1E\x48\x2F\x86\x0B\x5E\xC7\xBA"
						"\x47\xCC\xB2\xC6\xF2\xAF\xCE\x8F\x88\xD2\x2B\x6D\xC6\x13\x80\xF2"
						"\x3A\x66\x8F\xD3\x88\x8B\xB8\x05\x37\xC0\xA0\xB8\x64\x07\x68\x9E",
						64
	}, 
	// Test Case 2
	{
		/* desc */		&_kCryptoHashDescriptor_SHA3, 
		/* key */		"\x4A\x65\x66\x65", 
						4, 
		/* data */		"\x77\x68\x61\x74\x20\x64\x6F\x20\x79\x61\x20\x77\x61\x6E\x74\x20"
						"\x66\x6F\x72\x20\x6E\x6F\x74\x68\x69\x6E\x67\x3F", 
						28, 
		/* digest */	"\x5A\x4B\xFE\xAB\x61\x66\x42\x7C\x7A\x36\x47\xB7\x47\x29\x2B\x83"
						"\x84\x53\x7C\xDB\x89\xAF\xB3\xBF\x56\x65\xE4\xC5\xE7\x09\x35\x0B"
						"\x28\x7B\xAE\xC9\x21\xFD\x7C\xA0\xEE\x7A\x0C\x31\xD0\x22\xA9\x5E"
						"\x1F\xC9\x2B\xA9\xD7\x7D\xF8\x83\x96\x02\x75\xBE\xB4\xE6\x20\x24",
						64
	}, 
	// Test Case 3
	{
		/* desc */		&_kCryptoHashDescriptor_SHA3, 
		/* key */		"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
						"\xAA\xAA\xAA\xAA", 
						20, 
		/* data */		"\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
						"\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
						"\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
						"\xDD\xDD", 
						50, 
		/* digest */	"\x30\x9E\x99\xF9\xEC\x07\x5E\xC6\xC6\xD4\x75\xED\xA1\x18\x06\x87"
						"\xFC\xF1\x53\x11\x95\x80\x2A\x99\xB5\x67\x74\x49\xA8\x62\x51\x82"
						"\x85\x1C\xB3\x32\xAF\xB6\xA8\x9C\x41\x13\x25\xFB\xCB\xCD\x42\xAF"
						"\xCB\x7B\x6E\x5A\xAB\x7E\xA4\x2C\x66\x0F\x97\xFD\x85\x84\xBF\x03",
						64
	}, 
	// Test Case 4
	{
		/* desc */		&_kCryptoHashDescriptor_SHA3, 
		/* key */		"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
						"\x11\x12\x13\x14\x15\x16\x17\x18\x19", 
						25, 
		/* data */		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
						"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
						"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
						"\xCD\xCD", 
						50, 
		/* digest */	"\xB2\x7E\xAB\x1D\x6E\x8D\x87\x46\x1C\x29\xF7\xF5\x73\x9D\xD5\x8E"
						"\x98\xAA\x35\xF8\xE8\x23\xAD\x38\xC5\x49\x2A\x20\x88\xFA\x02\x81"
						"\x99\x3B\xBF\xFF\x9A\x0E\x9C\x6B\xF1\x21\xAE\x9E\xC9\xBB\x09\xD8"
						"\x4A\x5E\xBA\xC8\x17\x18\x2E\xA9\x74\x67\x3F\xB1\x33\xCA\x0D\x1D",
						64
	}, 
	// Test Case 5
	{
		/* desc */		&_kCryptoHashDescriptor_SHA3, 
		/* key */		"\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C", 
						20, 
		/* data */		"\x54\x65\x73\x74\x20\x57\x69\x74\x68\x20\x54\x72\x75\x6E\x63\x61\x74\x69\x6F\x6E", 
						20, 
		/* digest */	"\x0F\xA7\x47\x59\x48\xF4\x3F\x48\xCA\x05\x16\x67\x1E\x18\x97\x8C",
						16
	}, 
	// Test Case 6
	{
		/* desc */		&_kCryptoHashDescriptor_SHA3, 
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
		/* digest */	"\x00\xF7\x51\xA9\xE5\x06\x95\xB0\x90\xED\x69\x11\xA4\xB6\x55\x24"
						"\x95\x1C\xDC\x15\xA7\x3A\x5D\x58\xBB\x55\x21\x5E\xA2\xCD\x83\x9A"
						"\xC7\x9D\x2B\x44\xA3\x9B\xAF\xAB\x27\xE8\x3F\xDE\x9E\x11\xF6\x34"
						"\x0B\x11\xD9\x91\xB1\xB9\x1B\xF2\xEE\xE7\xFC\x87\x24\x26\xC3\xA4",
						64
	}, 
	// Test Case 7
	{
		/* desc */		&_kCryptoHashDescriptor_SHA3, 
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
		/* digest */	"\x38\xA4\x56\xA0\x04\xBD\x10\xD3\x2C\x9A\xB8\x33\x66\x84\x11\x28"
						"\x62\xC3\xDB\x61\xAD\xCC\xA3\x18\x29\x35\x5E\xAF\x46\xFD\x5C\x73"
						"\xD0\x6A\x1F\x0D\x13\xFE\xC9\xA6\x52\xFB\x38\x11\xB5\x77\xB1\xB1"
						"\xD1\xB9\x78\x9F\x97\xAE\x5B\x83\xC6\xF4\x4D\xFC\xF1\xD6\x7E\xBA",
						64
	}, 
};

//===========================================================================================================================
//	HKDF
//===========================================================================================================================

typedef struct
{
	CryptoHashDescriptorRef		desc;
	const void *				ikmPtr;
	size_t						ikmLen;
	const void *				saltPtr;
	size_t						saltLen;
	const void *				infoPtr;
	size_t						infoLen;
	const void *				keyPtr;
	size_t						keyLen;
	
}	CryptoHKDFTestVector;

static const CryptoHKDFTestVector		kCryptoHKDFTestVectors[] = 
{
	// Input test vectors from RFC 5869, but updated for SHA-512.
	
	// Test Case 1
	{
		// Desc
		&_kCryptoHashDescriptor_SHA512, 
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
		// Desc
		&_kCryptoHashDescriptor_SHA512, 
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
		// Desc
		&_kCryptoHashDescriptor_SHA512, 
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
	},
	// Test Case 4
	{
		// Desc
		&_kCryptoHashDescriptor_SHA1, 
		// IKM
		"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b", 
		11, 
		// Salt
		"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c", 
		13, 
		// Info
		"\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9", 
		10, 
		// Key
		"\x08\x5A\x01\xEA\x1B\x10\xF3\x69\x33\x06\x8B\x56\xEF\xA5\xAD\x81"
		"\xA4\xF1\x4B\x82\x2F\x5B\x09\x15\x68\xA9\xCD\xD4\xF1\x55\xFD\xA2"
		"\xC2\x2E\x42\x24\x78\xD3\x05\xF3\xF8\x96",
		42
	},
	// Test Case 5
	{
		// Desc
		&_kCryptoHashDescriptor_SHA1, 
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
		"\x0B\xD7\x70\xA7\x4D\x11\x60\xF7\xC9\xF1\x2C\xD5\x91\x2A\x06\xEB"
		"\xFF\x6A\xDC\xAE\x89\x9D\x92\x19\x1F\xE4\x30\x56\x73\xBA\x2F\xFE"
		"\x8F\xA3\xF1\xA4\xE5\xAD\x79\xF3\xF3\x34\xB3\xB2\x02\xB2\x17\x3C"
		"\x48\x6E\xA3\x7C\xE3\xD3\x97\xED\x03\x4C\x7F\x9D\xFE\xB1\x5C\x5E"
		"\x92\x73\x36\xD0\x44\x1F\x4C\x43\x00\xE2\xCF\xF0\xD0\x90\x0B\x52"
		"\xD3\xB4",
		82
	},
	
	// Input test vectors from RFC 5869, but updated for SHA-3.
	
	// Test Case 1
	{
		// Desc
		&_kCryptoHashDescriptor_SHA3, 
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
		"\x40\xE9\xF1\x7E\x9B\xF2\xEF\x99\x42\x5C\x2B\x23\xCC\xDF\x20\xA0"
		"\x18\xEA\x55\x13\xF9\xAE\x68\xE1\xEA\x8C\x62\x6D\xEB\x57\xDF\xA4"
		"\xD5\x6C\x27\xCC\xF2\xA2\xA2\x44\x88\xA5",
		42
	}, 
	// Test Case 2
	{
		// Desc
		&_kCryptoHashDescriptor_SHA3, 
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
		"\x3A\xDF\x31\x01\x12\x45\xF8\x2C\xC6\xB5\xC3\xB2\xEA\x31\xFE\x2A"
		"\x9B\x85\x5B\x42\x5C\x3E\xCD\xD8\xDA\x4A\x3F\xC5\xD0\xC3\x56\x3F"
		"\x63\xBB\xDE\xDF\x7C\xA9\x12\xD2\xE9\x8C\xBC\x85\x3D\x97\x80\x66"
		"\xAB\x17\x7F\x19\xA7\x34\x9E\x39\x82\x54\x9B\x82\xA3\x07\xE2\x11"
		"\x38\x91\x69\x1F\x25\x36\xCE\x45\xEB\x5D\xDF\x9B\x51\x75\x85\x9C"
		"\xE8\xD5",
		82
	}, 
	// Test Case 3
	{
		// Desc
		&_kCryptoHashDescriptor_SHA3, 
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
		"\x38\xBD\x71\xE4\x5B\x39\x7B\x77\x5B\x56\x33\x65\xA3\x32\x58\xA6"
		"\xFD\x83\xAB\xC1\xE8\x6A\xCF\x04\x2F\x07\x23\xC2\xB6\x8E\xBF\x07"
		"\x3A\x75\xC3\x4C\x69\x32\x88\x35\xEE\x4C",
		42
	},
};

static void		CryptoHashTest( TUTestContext *inTestCtx );
static OSStatus	CryptoHashTestOne( TUTestContext *inTestCtx, const CryptoHashTestVector *inTV );
static void		CryptoHMACTest( TUTestContext *inTestCtx );
static void		CryptoHKDFTest( TUTestContext *inTestCtx );

//===========================================================================================================================
//	CryptoHashUtilsTest
//===========================================================================================================================

void	CryptoHashUtilsTest( void )
{
	TUPerformTest( CryptoHashTest );
	TUPerformTest( CryptoHMACTest );
	TUPerformTest( CryptoHKDFTest );
}

//===========================================================================================================================
//	CryptoHashTest
//===========================================================================================================================

static void	CryptoHashTest( TUTestContext *inTestCtx )
{
	OSStatus		err;
	size_t			i;
	
	for( i = 0; i < countof( kCryptoHashTestVectors ); ++i )
	{
		err = CryptoHashTestOne( inTestCtx, &kCryptoHashTestVectors[ i ] );
		tu_require_noerr( err, exit );
	}
	
exit:
	return;
}

//===========================================================================================================================
//	CryptoHashTestOne
//===========================================================================================================================

static OSStatus	CryptoHashTestOne( TUTestContext *inTestCtx, const CryptoHashTestVector *inTV )
{
	OSStatus				err;
	uint8_t					digest[ kCryptoHashMaxDigestLen ];
	char *					srcBuf = NULL;
	const char *			srcPtr;
	size_t					srcLen;
	size_t					i;
	CryptoHashContext		ctx;
	
	tu_require_action( inTV->digestLen <= sizeof( digest ), exit, err = kSizeErr );
	
	srcLen = ( inTV->srcLen == kSizeCString ) ? strlen( inTV->srcPtr ) : inTV->srcLen;
	if( inTV->srcFlags & kSrcFlag_Repeat )
	{
		srcBuf = (char *) malloc( inTV->srcLen );
		tu_require_action( srcBuf, exit, err = kSizeErr );
		memset( srcBuf, *inTV->srcPtr, srcLen );
		srcPtr = srcBuf;
	}
	else
	{
		srcPtr = inTV->srcPtr;
	}
	
	// OneShot
	
	memset( digest, 0, sizeof( digest ) );
	CryptoHashOneShot( inTV->desc, srcPtr, srcLen, digest );
	tu_require_action( memcmp( digest, inTV->digestPtr, inTV->digestLen ) == 0, exit, err = kSizeErr );
	
	// Byte at a time.
	
	CryptoHashInit( &ctx, inTV->desc );
	for( i = 0; i < srcLen; ++i )
	{
		CryptoHashUpdate( &ctx, &srcPtr[ i ], 1 );
	}
	memset( digest, 0, sizeof( digest ) );
	CryptoHashFinal( &ctx, digest );
	tu_require_action( memcmp( digest, inTV->digestPtr, inTV->digestLen ) == 0, exit, err = kSizeErr );
	
	err = kNoErr;
	
exit:
	FreeNullSafe( srcBuf );
	return( err );
}

//===========================================================================================================================
//	CryptoHMACTest
//===========================================================================================================================

static void	CryptoHMACTest( TUTestContext *inTestCtx )
{
	const CryptoHMACTestVector *		tv;
	size_t								i;
	uint8_t								digest[ kCryptoHashMaxDigestLen ];
	
	for( i = 0; i < countof( kCryptoHMACTestVectors ); ++i )
	{
		tv = &kCryptoHMACTestVectors[ i ];
		memset( digest, 0, sizeof( digest ) );
		CryptoHMACOneShot( tv->desc, tv->keyPtr, tv->keyLen, tv->dataPtr, tv->dataLen, digest );
		tu_require( memcmp( digest, tv->digestPtr, tv->digestLen ) == 0, exit );
	}
	
exit:
	return;
}

//===========================================================================================================================
//	CryptoHKDFTest
//===========================================================================================================================

static void	CryptoHKDFTest( TUTestContext *inTestCtx )
{
	const CryptoHKDFTestVector *		tv;
	size_t								i;
	uint8_t								key[ 128 ];
	
	for( i = 0; i < countof( kCryptoHKDFTestVectors ); ++i )
	{
		tv = &kCryptoHKDFTestVectors[ i ];
		memset( key, 0, sizeof( key ) );
		CryptoHKDF( tv->desc, tv->ikmPtr, tv->ikmLen, tv->saltPtr, tv->saltLen, tv->infoPtr, tv->infoLen, tv->keyLen, key );
		tu_require( memcmp( key, tv->keyPtr, tv->keyLen ) == 0, exit );
	}
	
exit:
	return;
}

#endif // !EXCLUDE_UNIT_TESTS
