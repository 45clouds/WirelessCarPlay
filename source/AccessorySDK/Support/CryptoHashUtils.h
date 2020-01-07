/*
	File:    	CryptoHashUtils.h
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
	 
#ifndef __CryptoHashUtils_h__
#define __CryptoHashUtils_h__

#include "CommonServices.h"
#include "SHAUtils.h"

#include MD5_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == Hash ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@defgroup	CryptoHashDescriptor
	@brief		Descriptor for a particular hash function.
*/
typedef const struct CryptoHashDescriptorPrivate *		CryptoHashDescriptorRef;

extern CryptoHashDescriptorRef				kCryptoHashDescriptor_MD5;
extern CryptoHashDescriptorRef				kCryptoHashDescriptor_SHA1;
extern CryptoHashDescriptorRef				kCryptoHashDescriptor_SHA512;
extern CryptoHashDescriptorRef				kCryptoHashDescriptor_SHA3;

#define kCryptoHashDigestLength_MD5			16
#define kCryptoHashDigestLength_SHA1		20
#define kCryptoHashDigestLength_SHA512		64
#define kCryptoHashDigestLength_SHA3		64

#define kCryptoHashMaxBlockLen				128
#define kCryptoHashMaxDigestLen				64

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		CryptoHashContext
	@brief		Context/state for a hash instance.
*/
typedef struct
{
	CryptoHashDescriptorRef		desc;
	union
	{
		MD5_CTX					md5;
		SHA_CTX					sha1;
		SHA512_CTX				sha512;
		SHA3_CTX				sha3;
		
	}	state;
	
}	CryptoHashContext;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			CryptoHashInit
	@brief		Initializes the context for the hash.
*/
void	CryptoHashInit( CryptoHashContext *ctx, CryptoHashDescriptorRef inDesc );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			CryptoHashUpdate
	@brief		Updates the hash state with the specified data.
*/
void	CryptoHashUpdate( CryptoHashContext *ctx, const void *inData, size_t inLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			CryptoHashFinal
	@brief		Finalizes the hash state and produces the digest.
*/
void	CryptoHashFinal( CryptoHashContext *ctx, uint8_t *inDigestBuffer );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			CryptoHashOneShot
	@brief		Performs CryptoHashInit, CryptoHashUpdate, and CryptoHashFinal in one function call.
*/
void	CryptoHashOneShot( CryptoHashDescriptorRef inDesc, const void *inData, size_t inLen, uint8_t *inDigestBuffer );

#if 0
#pragma mark -
#pragma mark == HMAC ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		CryptoHMACContext
	@brief		Context/state for an HMAC instance.
*/
typedef struct
{
	CryptoHashContext		hashCtx;
	uint8_t					opad[ kCryptoHashMaxBlockLen ];
	
}	CryptoHMACContext;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			CryptoHMACInit
	@brief		Initializes the context for HMAC.
*/
void	CryptoHMACInit( CryptoHMACContext *ctx, CryptoHashDescriptorRef inDesc, const void *inKeyPtr, size_t inKeyLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			CryptoHMACUpdate
	@brief		Updates the HMAC state with the specified data.
*/
void	CryptoHMACUpdate( CryptoHMACContext *ctx, const void *inPtr, size_t inLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			CryptoHashFinal
	@brief		Finalizes the hash state and produces the digest.
*/
void	CryptoHMACFinal( CryptoHMACContext *ctx, uint8_t *outDigest );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			CryptoHMACOneShot
	@brief		Performs CryptoHMACInit, CryptoHMACUpdate, and CryptoHMACFinal in one function call.
*/
void
	CryptoHMACOneShot( 
		CryptoHashDescriptorRef	inDesc, 
		const void *			inKeyPtr, 
		size_t					inKeyLen, 
		const void *			inMsgPtr, 
		size_t					inMsgLen, 
		uint8_t *				outDigest );

#if 0
#pragma mark -
#pragma mark == HKDF ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			CryptoHKDF
	@brief		Derives a key according to HKDF. See <https://tools.ietf.org/html/rfc5869>.
*/
void
	CryptoHKDF( 
		CryptoHashDescriptorRef	inDesc, 
		const void *			inInputKeyPtr,	size_t inInputKeyLen, 
		const void *			inSaltPtr,		size_t inSaltLen, 
		const void *			inInfoPtr,		size_t inInfoLen, 
		size_t					inOutputLen, 	uint8_t *outKey );

#if 0
#pragma mark -
#pragma mark == Testing ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			CryptoHashUtilsTest
	@brief		Unit test.
*/
void	CryptoHashUtilsTest( void );

#ifdef __cplusplus
}
#endif

#endif // __CryptoHashUtils_h__
