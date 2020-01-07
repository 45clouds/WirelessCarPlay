/*
	File:    	SHAUtils.h
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
	
	Copyright (C) 2012-2015 Apple Inc. All Rights Reserved.
*/
	 
#ifndef __SHAUtils_h__
#define __SHAUtils_h__

#include "CommonServices.h"

#if( TARGET_HAS_MOCANA_SSL )
	#include "mtypes.h"
	#include "merrors.h"
	#include "hw_accel.h"
	#include "sha1.h"
#elif( !TARGET_HAS_COMMON_CRYPTO && !TARGET_HAS_SHA_UTILS && !TARGET_NO_OPENSSL )
	#include <openssl/sha.h>
#endif
#if( TARGET_PLATFORM_WICED )
	#include "wiced_security.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

//===========================================================================================================================
//	SHA-1
//===========================================================================================================================

typedef struct
{
	uint64_t		length;
	uint32_t		state[ 5 ];
	size_t			curlen;
	uint8_t			buf[ 64 ];
	
}	SHA_CTX_compat;

int	SHA1_Init_compat( SHA_CTX_compat *ctx );
int	SHA1_Update_compat( SHA_CTX_compat *ctx, const void *inData, size_t inLen );
int	SHA1_Final_compat( unsigned char *outDigest, SHA_CTX_compat *ctx );
unsigned char *	SHA1_compat( const void *inData, size_t inLen, unsigned char *outDigest );

OSStatus	SHA1_Test( void );

//===========================================================================================================================
//	SHA-512
//===========================================================================================================================

typedef struct
{
	uint64_t  		length;
	uint64_t		state[ 8 ];
	size_t			curlen;
	uint8_t			buf[ 128 ];
	
}	SHA512_CTX_compat;

int	SHA512_Init_compat( SHA512_CTX_compat *ctx );
int	SHA512_Update_compat( SHA512_CTX_compat *ctx, const void *inData, size_t inLen );
int	SHA512_Final_compat( unsigned char *outDigest, SHA512_CTX_compat *ctx );
unsigned char *	SHA512_compat( const void *inData, size_t inLen, unsigned char *outDigest );

OSStatus	SHA512_Test( void );

//===========================================================================================================================
//	SHA-3 (Keccak)
//===========================================================================================================================

#define SHA3_BLOCK_SIZE			72
#define SHA3_DIGEST_LENGTH		64

typedef struct
{
	uint64_t		state[ 25 ];
	size_t			buffered;
	uint8_t			buffer[ SHA3_BLOCK_SIZE ];
	
}	SHA3_CTX_compat;

int	SHA3_Init_compat( SHA3_CTX_compat *ctx );
int	SHA3_Update_compat( SHA3_CTX_compat *ctx, const void *inData, size_t inLen );
int	SHA3_Final_compat( unsigned char *outDigest, SHA3_CTX_compat *ctx );
uint8_t *	SHA3_compat( const void *inData, size_t inLen, uint8_t outDigest[ 64 ] );

OSStatus	SHA3_Test( void );
OSStatus	SHA3_TestFile( const char *inPath );

//===========================================================================================================================
//	HMAC_SHA1
//===========================================================================================================================

typedef struct
{
	SHA_CTX		shaCtx;
	uint8_t		opad[ 64 ];
	
}	HMAC_SHA1_CTX;

void	HMAC_SHA1_Init( HMAC_SHA1_CTX *ctx, const void *inKeyPtr, size_t inKeyLen );
void	HMAC_SHA1_Update( HMAC_SHA1_CTX *ctx, const void *inPtr, size_t inLen );
void	HMAC_SHA1_Final( HMAC_SHA1_CTX *ctx, uint8_t *outDigest );
void	HMAC_SHA1( const void *inKeyPtr, size_t inKeyLen, const void *inMsgPtr, size_t inMsgLen, uint8_t *outDigest );

OSStatus	HMAC_SHA1_Test( void );

//===========================================================================================================================
//	HMAC_SHA512
//===========================================================================================================================

typedef struct
{
	SHA512_CTX		shaCtx;
	uint8_t			opad[ 128 ];
	
}	HMAC_SHA512_CTX;

void	HMAC_SHA512_Init( HMAC_SHA512_CTX *ctx, const void *inKeyPtr, size_t inKeyLen );
void	HMAC_SHA512_Update( HMAC_SHA512_CTX *ctx, const void *inPtr, size_t inLen );
void	HMAC_SHA512_Final( HMAC_SHA512_CTX *ctx, uint8_t *outDigest );
void	HMAC_SHA512( const void *inKeyPtr, size_t inKeyLen, const void *inMsgPtr, size_t inMsgLen, uint8_t *outDigest );

OSStatus	HMAC_SHA512_Test( void );

//===========================================================================================================================
//	HKDF_SHA512_compat
//===========================================================================================================================

void
	HKDF_SHA512_compat( 
		const void *	inInputKeyPtr,	size_t inInputKeyLen, 
		const void *	inSaltPtr,		size_t inSaltLen, 
		const void *	inInfoPtr,		size_t inInfoLen, 
		size_t			inOutputKeyLen, uint8_t *outKey );

OSStatus	HKDF_SHA512_Test( void );

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
		uint8_t *		outKey );

OSStatus	PBKDF2_HMAC_SHA1_Test( void );

#ifdef __cplusplus
}
#endif

#endif // __SHAUtils_h__
