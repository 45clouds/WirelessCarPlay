/*
	File:    	AESUtils.h
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
	
	Copyright (C) 2007-2015 Apple Inc. All Rights Reserved.
*/
/*!
	@header			AES API
	@discussion		APIs and platform interfaces for using the Advanced Encryption Standard (AES).
	
	Support is provided for the following cryptographic libraries:
	
	- AES reference implementation (rijndael-alg-*.c).
	- Apple's CommonCrypto.
	- Brian Gladman's AES.
	- Broadcom's WICED security API.
	- OpenSSL.
	- Windows CryptoAPI.
	
	If one of these libraries is not available, the AES_* APIs will need to be implemented for your platform.
*/

#ifndef	__AESUtils_h__
#define	__AESUtils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#if( !defined( AES_UTILS_USE_GLADMAN_AES ) )
	#define AES_UTILS_USE_GLADMAN_AES		0
#endif

// AES_UTILS_USE_COMMON_CRYPTO: 1=Use CommonCrypto. 0=Use some other crypto library.

#if( !defined( AES_UTILS_USE_COMMON_CRYPTO ) )
	#if( !AES_UTILS_USE_GLADMAN_AES && TARGET_OS_DARWIN )
		#define AES_UTILS_USE_COMMON_CRYPTO		1
	#else
		#define AES_UTILS_USE_COMMON_CRYPTO		0
	#endif
#endif

// AES_UTILS_USE_WICED: Broadcom's WICED security API.

#if( !defined( AES_UTILS_USE_WICED ) )
	#if( TARGET_PLATFORM_WICED )
		#define AES_UTILS_USE_WICED		1
	#else
		#define AES_UTILS_USE_WICED		0
	#endif
#endif

// AES_UTILS_USE_WINDOWS_API: 1=Use the Windows Cryptographic Service Provider API.

#if( !defined( AES_UTILS_USE_WINDOWS_API ) )
	#if( !AES_UTILS_USE_GLADMAN_AES && TARGET_OS_WINDOWS )
		#define AES_UTILS_USE_WINDOWS_API		1
	#else
		#define AES_UTILS_USE_WINDOWS_API		0
	#endif
#endif

// AES_UTILS_HAS_GCM


#if( !defined( AES_UTILS_HAS_GLADMAN_GCM ) )
	#if( __has_include( "gcm.h" ) )
		#define AES_UTILS_HAS_GLADMAN_GCM		1
	#else
		#define AES_UTILS_HAS_GLADMAN_GCM		0
	#endif
#endif

#if( AES_UTILS_HAS_GLADMAN_GCM )
	#define AES_UTILS_HAS_GCM		1
#endif
#if( !defined( AES_UTILS_HAS_GCM ) )
	#define AES_UTILS_HAS_GCM		0
#endif

#if( AES_UTILS_HAS_GLADMAN_GCM )
	#include "gcm.h"
#endif

// Compatibility.

#if( AES_UTILS_USE_COMMON_CRYPTO )
	#include <CommonCrypto/CommonCrypto.h>
#elif( AES_UTILS_USE_GLADMAN_AES )
	#include "aes.h"
#elif( AES_UTILS_USE_WICED )
	#include "wiced_security.h"
#elif( AES_UTILS_USE_WINDOWS_API )
	#include "Wincrypt.h"
#elif( !TARGET_NO_OPENSSL )
	#include <openssl/aes.h>
#else
	
	// Emulate the OpenSSL API with the rijndael-alg-fst.c API.
	
	#define AES_ENCRYPT			1
	#define AES_DECRYPT			0
	
	#define AES_BLOCK_SIZE		16
	
	typedef struct
	{
		uint32_t		key[ 4 * ( 14 + 1 ) ]; // Up to 14 rounds.
		
	}	AES_KEY;
	
	extern void rijndaelKeySetupEnc(uint32_t rk[/*44*/], const uint8_t cipherKey[]);
	extern void rijndaelKeySetupDec(uint32_t rk[/*44*/], const uint8_t cipherKey[]);
	extern void rijndaelEncrypt(const uint32_t rk[/*44*/], const uint8_t pt[16], uint8_t ct[16]);
	extern void rijndaelDecrypt(const uint32_t rk[/*44*/], const uint8_t ct[16], uint8_t pt[16]);
	
	STATIC_INLINE int AES_set_encrypt_key( const unsigned char *inUserKey, const int inBits, AES_KEY *inKey )
	{
		(void) inBits; // Assumes 128-bit.
		
		rijndaelKeySetupEnc( inKey->key, inUserKey );
		return( 0 );	
	}
	
	STATIC_INLINE int AES_set_decrypt_key( const unsigned char *inUserKey, const int inBits, AES_KEY *inKey )
	{
		(void) inBits; // Assumes 128-bit.
		
		rijndaelKeySetupDec( inKey->key, inUserKey );
		return( 0 );	
	}
	
	STATIC_INLINE void AES_encrypt( const unsigned char *inPlain, unsigned char *outCrypt, const AES_KEY *inKey )
	{
		rijndaelEncrypt( inKey->key, inPlain, outCrypt );
	}
	
	STATIC_INLINE void AES_decrypt( const unsigned char *inCrypt, unsigned char *outPlain, const AES_KEY *inKey )
	{
		rijndaelDecrypt( inKey->key, inCrypt, outPlain );
	}
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark -
#pragma mark == AES-CTR ==
#endif

//===========================================================================================================================
/*!	@group		AES 128-bit Counter Mode API
	@abstract	API to encrypt or decrypt using AES-128 in counter mode.
	@discussion
	
	Call AES_CTR_Init to initialize the context. Don't use the context until it has been initialized.
	Call AES_CTR_Update to encrypt or decrypt N bytes of input and generate N bytes of output.
	Call AES_CTR_Final to finalize the context. After finalizing, you must call AES_CTR_Init to use it again.
	
	See the unit test for an example of using it.
*/
#define kAES_CTR_Size		16

typedef struct
{
#if( AES_UTILS_USE_COMMON_CRYPTO )
	CCCryptorRef		cryptor;				// PRIVATE: Internal CommonCrypto ref.
#elif( AES_UTILS_USE_GLADMAN_AES )
	aes_encrypt_ctx		ctx;					// PRIVATE: Gladman AES context.
#elif( AES_UTILS_USE_WICED )
	aes_context_t 		ctx;					// PRIVATE: WICED AES context.
#elif( AES_UTILS_USE_WINDOWS_API )
	HCRYPTPROV			provider;				// PRIVATE: CryptoAPI provider.
	HCRYPTKEY			key;					// PRIVATE: CryptoAPI key.
#else
	AES_KEY				key;					// PRIVATE: Internal AES key.
#endif
	uint8_t				ctr[ kAES_CTR_Size ];	// PRIVATE: Big endian counter.
	uint8_t				buf[ kAES_CTR_Size ];	// PRIVATE: Keystream buffer.
	size_t				used;					// PRIVATE: Number of bytes of the keystream buffer that we've used.
	
}	AES_CTR_Context;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AES_CTR_Init
	@abstract	Initializes a context for AES-128 in counter mode. Must be called before other AES_CTR_* functions.
	
	@param		inContext	Context to be initialized.
	@param		inKey		16-byte key material to be used.
	@param		inNonce		16-byte nonce/IV to be used. This must be chosen such that a key/nonce pair is never used twice.
	
	@result		kNoErr if successful or an error code indicating failure.
*/
OSStatus
	AES_CTR_Init( 
		AES_CTR_Context *	inContext, 
		const uint8_t		inKey[ kAES_CTR_Size ], 
		const uint8_t		inNonce[ kAES_CTR_Size ] );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AES_CTR_Update
	@abstract	Encrypts or decrypts N bytes of data using AES-128 in counter mode.
	
	@param		inContext	Context previously initialized with AES_CTR_Init.
	@param		inSrc		Pointer to data to encrypt/decrypt. Must be at least inLen bytes.
	@param		inLen		Number of bytes to encrypt/decrypt.
	@param		inDst		Pointer to buffer where output data is stored. Must be at least inLen bytes.
							inDst may be equal to inSrc for in-place encryption/decryption, but they cannot otherwise overlap.
	
	@result		kNoErr if successful or an error code indicating failure.
*/
OSStatus	AES_CTR_Update( AES_CTR_Context *inContext, const void *inSrc, size_t inLen, void *inDst );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AES_CTR_Final
	@abstract	Finalizes a context for AES-128 in counter mode when no longer needed. Context must not be used after this.
	
	@param		inContext	Context to be finalized.
*/
void	AES_CTR_Final( AES_CTR_Context *inContext );
#define	AES_CTR_Forget( CTX, VALID_PTR ) \
	do { if( *(VALID_PTR) ) { AES_CTR_Final( (CTX) ); *(VALID_PTR) = false; } } while( 0 )

#if 0
#pragma mark -
#pragma mark == AES-CBC Frame ==
#endif

//===========================================================================================================================
/*!	@group		AES 128-bit CBC Frame Mode API
	@abstract	API to encrypt or decrypt using AES-128 in CBC frame mode.
	@discussion
	
	Call AES_CBCFrame_Init to initialize the context. Don't use the context until it has been initialized.
	Call AES_CBCFrame_Update to encrypt or decrypt N bytes of input and generate N bytes of output.
	Call AES_CBCFrame_Final to finalize the context. After finalizing, you must call AES_CBCFrame_Init to use it again.
	
	See the unit test for an example of using it.
*/
#define kAES_CBCFrame_Size		16

typedef struct
{
	// PRIVATE: don't touch any of these fields. Do everything with the API.
	
#if( AES_UTILS_USE_COMMON_CRYPTO )
	CCCryptorRef			cryptor;					// PRIVATE: Internal CommonCrypto ref.
#elif( AES_UTILS_USE_GLADMAN_AES )
	union
	{
		aes_encrypt_ctx		encrypt;					// PRIVATE: Gladman AES context for encryption.
		aes_decrypt_ctx		decrypt;					// PRIVATE: Gladman AES context for decryption.
		
	}	ctx;
	int						encrypt;
#elif( AES_UTILS_USE_WICED )
	aes_context_t 			ctx;						// PRIVATE: WICED AES context.
	int						encrypt;
#elif( AES_UTILS_USE_WINDOWS_API )
	HCRYPTPROV				provider;					// PRIVATE: CryptoAPI provider.
	HCRYPTKEY				key;						// PRIVATE: CryptoAPI key.
	Boolean					encrypt;
#else
	int						mode;						// PRIVATE: AES_ENCRYPT or AES_DECRYPT.
	AES_KEY					key;						// PRIVATE: Internal AES key.
#endif
	uint8_t					iv[ kAES_CBCFrame_Size ];	// PRIVATE: Initialization vector.
	
}	AES_CBCFrame_Context;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AES_CBCFrame_Init
	@abstract	Initializes a context for AES-128 in CBC frame mode. Must be called before other AES_CBCFrame_* functions.
	
	@param		inContext	Context to be initialized.
	@param		inKey		16-byte key material to be used.
	@param		inIV		16-byte initialization vector to be used.
	@param		inEncrypt	true to encrypt, false to decrypt.
	
	@result		kNoErr if successful or an error code indicating failure.
*/
OSStatus
	AES_CBCFrame_Init( 
		AES_CBCFrame_Context *	inContext, 
		const uint8_t			inKey[ kAES_CBCFrame_Size ], 
		const uint8_t			inIV[ kAES_CBCFrame_Size ], 
		Boolean					inEncrypt );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AES_CBCFrame_Update
	@abstract	Encrypts or decrypts N bytes of data using AES-128 in CBC frame mode.
	
	@param		inContext	Context previously initialized with AES_CBCFrame_Init.
	@param		inSrc		Pointer to data to encrypt/decrypt. Must be at least inLen bytes.
	@param		inLen		Number of bytes to encrypt/decrypt.
	@param		inDst		Pointer to buffer where output data is stored. Must be at least inLen bytes.
							inDst may be equal to inSrc for in-place encryption/decryption, but they cannot otherwise overlap.
	
	@result		kNoErr if successful or an error code indicating failure.
*/
OSStatus	AES_CBCFrame_Update( AES_CBCFrame_Context *inContext, const void *inSrc, size_t inLen, void *inDst );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AES_CBCFrame_Update2
	@abstract	Encrypts or decrypts 2 chunnks of data, N bytes each using AES-128 in CBC frame mode.
	
	@param		inContext	Context previously initialized with AES_CBCFrame_Init.
	@param		inSrc1		Pointer to first chunk of data to encrypt/decrypt. Must be at least inLen1 bytes.
	@param		inLen1		Number of bytes to encrypt/decrypt from inSrc1.
	@param		inSrc2		Pointer to second chunk of data to encrypt/decrypt. Must be at least inLen2 bytes.
	@param		inLen2		Number of bytes to encrypt/decrypt from inSrc2.
	@param		inDst		Pointer to buffer where output data is stored. Must be at least inLen1 + inLen2 bytes.
	
	@result		kNoErr if successful or an error code indicating failure.
*/
OSStatus
	AES_CBCFrame_Update2( 
		AES_CBCFrame_Context *	inContext, 
		const void *			inSrc1, 
		size_t					inLen1, 
		const void *			inSrc2, 
		size_t					inLen2, 
		void *					inDst );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AES_CBCFrame_Final
	@abstract	Finalizes a context for AES-128 in CBC frame mode when no longer needed. Context must not be used after this.
	
	@param		inContext	Context to be finalized.
*/
void	AES_CBCFrame_Final( AES_CBCFrame_Context *inContext );
#define	AES_CBCFrame_Forget( CTX, VALID_PTR ) \
	do { if( *(VALID_PTR) ) { AES_CBCFrame_Final( (CTX) ); *(VALID_PTR) = false; } } while( 0 )

#if 0
#pragma mark -
#pragma mark == AES-ECB ==
#endif

//===========================================================================================================================
/*!	@group		AES 128-bit ECB API
	@abstract	API to encrypt or decrypt using AES-128 in ECB mode.
	@discussion
	
	Call AES_ECB_Init to initialize the context. Don't use the context until it has been initialized.
	Call AES_ECB_Update to encrypt or decrypt N bytes of input and generate N bytes of output.
	Call AES_ECB_Final to finalize the context. After finalizing, you must call AES_ECB_Init to use it again.
	
	See the unit test for an example of using it.
*/
#define kAES_ECB_Size		16

#if( AES_UTILS_USE_COMMON_CRYPTO )
	#define kAES_ECB_Mode_Encrypt		kCCEncrypt
	#define kAES_ECB_Mode_Decrypt		kCCDecrypt
#elif( AES_UTILS_USE_GLADMAN_AES )
	#define kAES_ECB_Mode_Encrypt		1
	#define kAES_ECB_Mode_Decrypt		0
#elif( AES_UTILS_USE_WICED )
	#define kAES_ECB_Mode_Encrypt		AES_ENCRYPT
	#define kAES_ECB_Mode_Decrypt		AES_DECRYPT
#elif( AES_UTILS_USE_WINDOWS_API )
	#define kAES_ECB_Mode_Encrypt		1
	#define kAES_ECB_Mode_Decrypt		0
#else
	#define kAES_ECB_Mode_Encrypt		1
	#define kAES_ECB_Mode_Decrypt		0
	
	typedef void	( *AESCryptFunc )( const unsigned char *in, unsigned char *out, const AES_KEY *key );
#endif

typedef struct
{
#if( AES_UTILS_USE_COMMON_CRYPTO )
	CCCryptorRef		cryptor;		// PRIVATE: Internal CommonCrypto ref.
#elif( AES_UTILS_USE_GLADMAN_AES )
	union
	{
		aes_encrypt_ctx		encrypt;	// PRIVATE: Gladman AES context for encryption.
		aes_decrypt_ctx		decrypt;	// PRIVATE: Gladman AES context for decryption.
		
	}	ctx;
	uint32_t				encrypt;
#elif( AES_UTILS_USE_WICED )
	aes_context_t 			ctx;		// PRIVATE: WICED AES context.
	uint32_t				mode;
#elif( AES_UTILS_USE_WINDOWS_API )
	HCRYPTPROV				provider;	// PRIVATE: CryptoAPI provider.
	HCRYPTKEY				key;		// PRIVATE: CryptoAPI key.
	Boolean					encrypt;
#else
	AES_KEY					key;		// PRIVATE: Internal AES key.
	AESCryptFunc			cryptFunc;	// PRIVATE: Pointer to AES_encrypt to AES_decrypt.
#endif
	
}	AES_ECB_Context;

OSStatus	AES_ECB_Init( AES_ECB_Context *inContext, uint32_t inMode, const uint8_t inKey[ kAES_ECB_Size ] );
OSStatus	AES_ECB_Update( AES_ECB_Context *inContext, const void *inSrc, size_t inLen, void *inDst );
void		AES_ECB_Final( AES_ECB_Context *inContext );
#define		AES_ECB_Forget( CTX, VALID_PTR ) \
	do { if( *(VALID_PTR) ) { AES_ECB_Final( (CTX) ); *(VALID_PTR) = false; } } while( 0 )

#if 0
#pragma mark -
#pragma mark == AES-GCM ==
#endif

//===========================================================================================================================
/*!	@group		AES_GCM 128-bit API
	@abstract	API to perform authenticated encryption and decryption using AES-128 in GCM mode.
	@discussion
	
	Call AES_GCM_InitEx to initialize the context. Don't use the context until it has been initialized.
	Call AES_GCM_Final to finalize the context. After finalizing, you must call AES_GCM_InitEx to use it again.
	
	The general flow for sending a message:
	
		AES_GCM_InitMessage (provide per-message nonce or use kAES_CGM_Nonce_Auto to increment the nonce from AES_GCM_InitEx).
		AES_GCM_AddAAD (may repeat as many times as necessary to add each chunk of AAD).
		AES_GCM_Encrypt (may repeat as many times as necessary to add each chunk of data to encrypt).
		AES_GCM_FinalizeMessage (outputs a auth tag to send along with the message so it can be verified by the receiver).
	
	The general flow for receiving a message:
	
		AES_GCM_InitMessage (provide per-message nonce or use kAES_CGM_Nonce_Auto to increment the nonce from AES_GCM_InitEx).
		AES_GCM_AddAAD (may repeat as many times as necessary to add each chunk of AAD).
		AES_GCM_Decrypt (may repeat as many times as necessary to add each chunk of data to encrypt).
		AES_GCM_VerifyMessage (if this fails, reject the message).
	
	See the unit test for an example of using it.
	
	See <http://en.wikipedia.org/wiki/Galois/Counter_Mode> for more information.
*/
#if( AES_UTILS_HAS_GCM )

#define kAES_CGM_Size			16
#define kAES_CGM_Nonce_None		NULL // When passed to AES_GCM_InitEx it means the caller is using a per-message nonce.
#define kAES_CGM_Nonce_Auto		NULL // When passed to AES_GCM_Encrypt, it means use the internal, auto-incremented nonce.

typedef uint32_t		AES_GCM_Mode;
	#define kAES_GCM_Mode_Encrypt		1
	#define kAES_GCM_Mode_Decrypt		0

typedef struct
{
#if  ( AES_UTILS_HAS_GLADMAN_GCM )
	gcm_ctx				ctx;
#else
	#error "GCM enabled, but no implementation?"
#endif
	uint8_t				nonce[ kAES_CGM_Size ];
	
}	AES_GCM_Context;

// DEPRECATED: Use AES_GCM_InitEx and specify a mode instead.
OSStatus
	AES_GCM_Init( 
		AES_GCM_Context *	inContext, 
		const uint8_t		inKey[ kAES_CGM_Size ], 
		const uint8_t		inNonce[ kAES_CGM_Size ] ); // May be kAES_CGM_Nonce_None for per-message nonces.
OSStatus
	AES_GCM_InitEx( 
		AES_GCM_Context *	inContext, 
		AES_GCM_Mode		inMode, 
		const uint8_t		inKey[ kAES_CGM_Size ], 
		const uint8_t		inNonce[ kAES_CGM_Size ] ); // May be kAES_CGM_Nonce_None for per-message nonces.
OSStatus	AES_GCM_InitEx2( AES_GCM_Context *inContext, AES_GCM_Mode inMode, const void *inKeyPtr, size_t inKeyLen );
void		AES_GCM_Final( AES_GCM_Context *inContext );
#define		AES_GCM_Forget( CTX, VALID_PTR ) \
	do { if( *(VALID_PTR) ) { AES_GCM_Final( (CTX) ); *(VALID_PTR) = false; } } while( 0 )

OSStatus	AES_GCM_InitMessage( AES_GCM_Context *inContext, const uint8_t *inNonce );
OSStatus	AES_GCM_InitMessageEx( AES_GCM_Context *inContext, const void *inNoncePtr, size_t inNonceLen );
OSStatus	AES_GCM_FinalizeMessage( AES_GCM_Context *inContext, uint8_t outAuthTag[ kAES_CGM_Size ] );
OSStatus	AES_GCM_VerifyMessage( AES_GCM_Context *inContext, const uint8_t inAuthTag[ kAES_CGM_Size ] );

OSStatus	AES_GCM_AddAAD( AES_GCM_Context *inContext, const void *inPtr, size_t inLen );
OSStatus	AES_GCM_Encrypt( AES_GCM_Context *inContext, const void *inSrc, size_t inLen, void *inDst );
OSStatus	AES_GCM_Decrypt( AES_GCM_Context *inContext, const void *inSrc, size_t inLen, void *inDst );

OSStatus	AES_GCM_Test( int inPrint, int inPerf );

#endif // AES_UTILS_HAS_GCM

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AESUtils_Test
	@internal
	@abstract	Unit test.
*/
#if( !EXCLUDE_UNIT_TESTS )
	OSStatus	AESUtils_Test( int inPrint, int inPerf );
#endif

#ifdef __cplusplus
}
#endif

#endif // __AESUtils_h__
