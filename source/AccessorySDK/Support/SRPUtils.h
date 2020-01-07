/*
	File:    	SRPUtils.h
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
	
	Copyright (C) 2014-2015 Apple Inc. All Rights Reserved.
*/
/*!
	@file	SRP API
	@brief	APIs for using the Secure Remote Password (SRP) protocol.
*/

#ifndef	__SRPUtils_h__
#define	__SRPUtils_h__

#include "CommonServices.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SRPUTILS_DOC_ONLY		0 // Do-nothing conditional to wrap prototypes for documenting the API.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn		SRPCreate
	@brief	Creates a new SRP object for a single client or server transaction of the SRP protocol.
*/
#if( SRPUTILS_DOC_ONLY )
OSStatus	SRPCreate( SRPRef *outRef );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn		SRPDelete
	@brief	Deletes an SRP object created with SRPCreate. It cannot be used after being deleted.
*/
#if( SRPUTILS_DOC_ONLY )
void	SRPDelete( SRPRef inRef );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn		SRPClientStart
	@brief	Starts the client side of the SRP protocol.
	
	@param	inClient			SRP object created with SRPCreate.
	@param	inParams			Configuration parameters for SRP. See kSRPParameters_*. These parameters must match what
								the server expects.
	
	@param	inUsernamePtr		Ptr to the username identifier. This is usually a username string (e.g. "tom").
								If it's NUL terminated, you can pass kSizeCString as the length.
								If it's not NUL terminated, you must pass the number of bytes in inUsernameLen.
	@param	inUsernameLen		Number of bytes in inUsernamePtr. May be kSizeCString if inUsernamePtr is NUL terminated.
	
	@param	inPasswordPtr		Ptr to the password. This is what will be verified between client and server.
								If it's NUL terminated, you can pass kSizeCString as the length.
								If it's not NUL terminated, you must pass the number of bytes in inPasswordLen.
	@param	inPasswordLen		Number of bytes in inPasswordPtr. May be kSizeCString if inPasswordPtr is NUL terminated.
	
	@param	inSaltPtr			Ptr to bytes to use as salt. This should be random bytes generated by the server 
								(it comes from SRPServerStart() if the server is using this library). It should be send from
								the server to the client as part of its first response message. It must be at least 16 bytes.
	@param	inSaltLen			Number of bytes in inSaltPtr.
	
	@param	inServerPKPtr		Bytes of the server's SRP public key. This is a dynamic public key that is newly generated 
								by the server for each SRP transaction (it comes from SRPServerStart() if the server is using 
								this library).
	@param	inServerPKLen		Number of bytes in inServerPKPtr.
	
	@param	outClientPKPtr		Ptr to receive a malloc'd public key for this client. This is a dynamic public key that is 
								newly generated for each SRP transaction. The caller must send this to the server for the 
								server to verify this SRP transaction (it's passed to SRPServerVerify() if the server is
								using this library). The caller must free this memory with free() when it's done with it.
	@param	outClientPKLen		Ptr to receive the number of bytes in the memory returned in outClientPKPtr.
	
	@param	outSharedSecretPtr	Ptr to receive a malloc'd shared secret. This shared secret is the same as the shared secret
								that the server will compute (via SRPServerVerify if the server is using this library). 
								The bytes of the shared secret can used with a key derivation function, such as HKDF, to 
								generate keys for use with symmetric encryption algorithms, such as AES. The caller must 
								free this memory with free() when it's done with it. It's good practice to zero this memory 
								with MemZeroSecure before freeing to reduce the chance of secret key material lingering in 
								the heap for attackers to scavenge.
	@param	outSharedSecretLen	Ptr to receive the number of bytes in the memory returned in outSharedSecretPtr.
	
	@param	outResponsePtr		Ptr to receive a malloc'd response to the SRP challenge from the server (sometimes called 
								an SRP proof). The caller must send this to the server for the server to verify this SRP 
								transaction (it's passed to SRPServerVerify() if the server is using this library).
	@param	outResponseLen		Ptr to receive the number of bytes in the memory returned in outResponsePtr.
*/
#if( SRPUTILS_DOC_ONLY )
OSStatus
	SRPClientStart( 
		SRPRef				inClient, 
		SRPParametersRef	inParams, 
		const void *		inUsernamePtr, 
		size_t				inUsernameLen, 
		const void *		inPasswordPtr, 
		size_t				inPasswordLen, 
		const void *		inSaltPtr, 
		size_t				inSaltLen, 
		const void *		inServerPKPtr, 
		size_t				inServerPKLen, 
		uint8_t **			outClientPKPtr, 
		size_t *			outClientPKLen, 
		uint8_t **			outSharedSecretPtr, 
		size_t *			outSharedSecretLen, 
		uint8_t **			outResponsePtr, 
		size_t *			outResponseLen );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn		SRPClientVerify
	@brief	Performs the server verification step for the client side of the SRP protocol.
	
	@param	inClient			SRP object created with SRPCreate.
	@param	inResponsePtr		Ptr to the SRP response from the server. This comes from SRPServerVerify() if the server is 
								using this library).
	@param	inResponseLen		Number of bytes in inResponsePtr.
	
	@result	Returns kNoErr on success. Anything else indicates a failure.
*/
#if( SRPUTILS_DOC_ONLY )
OSStatus	SRPClientVerify( SRPRef inClient, const void *inResponsePtr, size_t inResponseLen );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn		SRPServerStart
	@brief	Starts the server side of the SRP protocol.
	
	@param	inServer			SRP object created with SRPCreate.
	@param	inParams			Configuration parameters for SRP. See kSRPParameters_*. These parameters must match what
								the client used when it started the transaction.
	
	@param	inUsernamePtr		Ptr to the username identifier. This is usually a username string (e.g. "tom").
								If it's NUL terminated, you can pass kSizeCString as the length.
								If it's not NUL terminated, you must pass the number of bytes in inUsernameLen.
								This normally comes from the client in the message that starts the SRP transaction.
	@param	inUsernameLen		Number of bytes in inUsernamePtr. May be kSizeCString if inUsernamePtr is NUL terminated.
	
	@param	inPasswordPtr		Ptr to the password. This is what will be verified between client and server.
								If it's NUL terminated, you can pass kSizeCString as the length.
								If it's not NUL terminated, you must pass the number of bytes in inPasswordLen.
	@param	inPasswordLen		Number of bytes in inPasswordPtr. May be kSizeCString if inPasswordPtr is NUL terminated.
	
	@param	inSaltLen			Number of bytes of random salt to generate. This must be 16 or larger.
	@param	outSalt				Ptr to a buffer to be filled in with "inSaltLen" of random data. This must be sent to the
								the client (it's passed to SRPClientVerify if the client is using this library).
	
	@param	outServerPKPtr		Ptr to receive a malloc'd public key for this server. This is a dynamic public key that is 
								newly generated for each SRP transaction. The caller must send this to the client for the 
								client to verify this SRP transaction (it's passed to SRPClientVerify() if the server is
								using this library). The caller must free this memory with free() when it's done with it.
	@param	outServerPKLen		Ptr to receive the number of bytes in the memory returned in outServerPKPtr.
*/
#if( SRPUTILS_DOC_ONLY )
OSStatus
	SRPServerStart( 
		SRPRef 				inServer, 
		SRPParametersRef	inParams, 
		const void *		inUsernamePtr, 
		size_t				inUsernameLen, 
		const void *		inPasswordPtr, 
		size_t				inPasswordLen, 
		size_t				inSaltLen, 
		uint8_t *			outSalt, 
		uint8_t **			outServerPKPtr, 
		size_t *			outServerPKLen );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn		SRPServerVerify
	@brief	Performs the client verification step for the server side of the SRP protocol.
	
	@param	inServer			SRP object created with SRPCreate.
	
	@param	inClientPKPtr		Bytes of the client's SRP public key. This is a dynamic public key that is newly generated 
								by the client for each SRP transaction (it comes from SRPClientStart() if the client is using 
								this library).
	@param	inClientPKLen		Number of bytes in inClientPKPtr.
	
	@param	inResponsePtr		Ptr to the SRP response from the client. This comes from SRPClientStart() if the client is 
								using this library).
	@param	inResponseLen		Number of bytes in inResponsePtr.
	
	@param	outSharedSecretPtr	Ptr to receive a malloc'd shared secret. This shared secret is the same as the shared secret
								that the client will compute (via SRClientVerify if the client is using this library). 
								The bytes of the shared secret can used with a key derivation function, such as HKDF, to 
								generate keys for use with symmetric encryption algorithms, such as AES. The caller must 
								free this memory with free() when it's done with it. It's good practice to zero this memory 
								with MemZeroSecure before freeing to reduce the chance of secret key material lingering in 
								the heap for attackers to scavenge.
	@param	outSharedSecretLen	Ptr to receive the number of bytes in the memory returned in outSharedSecretPtr.
	
	@param	outResponsePtr		Ptr to receive a malloc'd response to the SRP challenge from the client (sometimes called 
								an SRP proof). The caller must send this to the client for the client to verify this SRP 
								transaction (it's passed to SRPClientVerify() if the client is using this library).
	@param	outResponseLen		Ptr to receive the number of bytes in the memory returned in outResponsePtr.
	
	@result	Returns kNoErr on success. Anything else indicates a failure.
*/
#if( SRPUTILS_DOC_ONLY )
OSStatus
	SRPServerVerify( 
		SRPRef				inServer, 
		const void *		inClientPKPtr, 
		size_t				inClientPKLen, 
		const void *		inResponsePtr, 
		size_t				inResponseLen, 
		uint8_t **			outSharedSecretPtr, 
		size_t *			outSharedSecretLen, 
		uint8_t **			outResponsePtr, 
		size_t *			outResponseLen );
#endif

//===========================================================================================================================
//	Configuration
//===========================================================================================================================

#if( !defined( TARGET_HAS_CORECRYPTO_SRP ) )
		#define TARGET_HAS_CORECRYPTO_SRP		0
#endif
#if( !defined( TARGET_HAS_LIBSRP ) )
	#if( TARGET_HAS_CORECRYPTO_SRP )
		#define TARGET_HAS_LIBSRP			0
	#else
		#define TARGET_HAS_LIBSRP			1
	#endif
#endif
#if( TARGET_HAS_CORECRYPTO_SRP )
	#define SRPRef						SRPRef_corecrypto
	
	#define SRPCreate( OUT_REF )		SRPCreate_corecrypto( (OUT_REF) )
	#define SRPDelete( REF )			SRPDelete_corecrypto( (REF) )
	#define SRPForget( X )				SRPForget_corecrypto( (X) )
	
	#define SRPClientStart( CLIENT, PARAMS, USER_PTR, USER_LEN, PASS_PTR, PASS_LEN, SALT_PTR, SALT_LEN, \
				SERVER_PK_PTR, SERVER_PK_LEN, OUT_CLIENT_PK_PTR, OUT_CLIENT_PK_LEN, \
				OUT_SHARED_SECRET_PTR, OUT_SHARED_SECRET_LEN, OUT_RESPONSE_PTR, OUT_RESPONSE_LEN ) \
			SRPClientStart_corecrypto( (CLIENT), (PARAMS), (USER_PTR), (USER_LEN), (PASS_PTR), (PASS_LEN), \
				(SALT_PTR), (SALT_LEN), (SERVER_PK_PTR), (SERVER_PK_LEN), \
				(OUT_CLIENT_PK_PTR), (OUT_CLIENT_PK_LEN), (OUT_SHARED_SECRET_PTR), (OUT_SHARED_SECRET_LEN), \
				(OUT_RESPONSE_PTR), (OUT_RESPONSE_LEN) )
	
	#define SRPClientVerify( CLIENT, RESPONSE_PTR, RESPONSE_LEN ) \
		SRPClientVerify_corecrypto( (CLIENT), (RESPONSE_PTR), (RESPONSE_LEN) )
	
	#define SRPServerStart( SERVER, PARAMS, USER_PTR, USER_LEN, PASS_PTR, PASS_LEN, SALT_LEN, \
				OUT_SALT, OUT_SERVER_PK_PTR, OUT_SERVER_PK_LEN ) \
			SRPServerStart_corecrypto( (SERVER), (PARAMS), (USER_PTR), (USER_LEN), (PASS_PTR), (PASS_LEN), (SALT_LEN), \
				(OUT_SALT), (OUT_SERVER_PK_PTR), (OUT_SERVER_PK_LEN) )
	
	#define SRPServerVerify( SERVER, CLIENT_PK_PTR, CLIENT_PK_LEN, RESPONSE_PTR, RESPONSE_LEN, \
				OUT_SHARED_SECRET_PTR, OUT_SHARED_SECRET_LEN, OUT_RESPONSE_PTR, OUT_RESPONSE_LEN ) \
			SRPServerVerify_corecrypto( (SERVER), (CLIENT_PK_PTR), (CLIENT_PK_LEN), (RESPONSE_PTR), (RESPONSE_LEN), \
				(OUT_SHARED_SECRET_PTR), (OUT_SHARED_SECRET_LEN), (OUT_RESPONSE_PTR), (OUT_RESPONSE_LEN) )
#elif( TARGET_HAS_LIBSRP )
	#define SRPRef						SRPRef_libsrp
	
	#define SRPCreate( OUT_REF )		SRPCreate_libsrp( (OUT_REF) )
	#define SRPDelete( REF )			SRPDelete_libsrp( (REF) )
	#define SRPForget( X )				SRPForget_libsrp( (X) )
	
	#define SRPClientStart( CLIENT, PARAMS, USER_PTR, USER_LEN, PASS_PTR, PASS_LEN, SALT_PTR, SALT_LEN, \
				SERVER_PK_PTR, SERVER_PK_LEN, OUT_CLIENT_PK_PTR, OUT_CLIENT_PK_LEN, \
				OUT_SHARED_SECRET_PTR, OUT_SHARED_SECRET_LEN, OUT_RESPONSE_PTR, OUT_RESPONSE_LEN ) \
			SRPClientStart_libsrp( (CLIENT), (PARAMS), (USER_PTR), (USER_LEN), (PASS_PTR), (PASS_LEN), \
				(SALT_PTR), (SALT_LEN), (SERVER_PK_PTR), (SERVER_PK_LEN), \
				(OUT_CLIENT_PK_PTR), (OUT_CLIENT_PK_LEN), (OUT_SHARED_SECRET_PTR), (OUT_SHARED_SECRET_LEN), \
				(OUT_RESPONSE_PTR), (OUT_RESPONSE_LEN) )
	
	#define SRPClientVerify( CLIENT, RESPONSE_PTR, RESPONSE_LEN ) \
		SRPClientVerify_libsrp( (CLIENT), (RESPONSE_PTR), (RESPONSE_LEN) )
	
	#define SRPServerStart( SERVER, PARAMS, USER_PTR, USER_LEN, PASS_PTR, PASS_LEN, SALT_LEN, \
				OUT_SALT, OUT_SERVER_PK_PTR, OUT_SERVER_PK_LEN ) \
			SRPServerStart_libsrp( (SERVER), (PARAMS), (USER_PTR), (USER_LEN), (PASS_PTR), (PASS_LEN), (SALT_LEN), \
				(OUT_SALT), (OUT_SERVER_PK_PTR), (OUT_SERVER_PK_LEN) )
	
	#define SRPServerVerify( SERVER, CLIENT_PK_PTR, CLIENT_PK_LEN, RESPONSE_PTR, RESPONSE_LEN, \
				OUT_SHARED_SECRET_PTR, OUT_SHARED_SECRET_LEN, OUT_RESPONSE_PTR, OUT_RESPONSE_LEN ) \
			SRPServerVerify_libsrp( (SERVER), (CLIENT_PK_PTR), (CLIENT_PK_LEN), (RESPONSE_PTR), (RESPONSE_LEN), \
				(OUT_SHARED_SECRET_PTR), (OUT_SHARED_SECRET_LEN), (OUT_RESPONSE_PTR), (OUT_RESPONSE_LEN) )
#endif

//===========================================================================================================================
//	Parameters
//===========================================================================================================================

typedef const struct SRPParametersPrivate *		SRPParametersRef;

extern SRPParametersRef const		kSRPParameters_3072_SHA512;	// RFC-5054 3072-bit group using SHA-512.

#if( TARGET_HAS_LIBSRP )
//===========================================================================================================================
//	libsrp
//===========================================================================================================================

typedef struct SRPPrivate_libsrp *		SRPRef_libsrp;

OSStatus	SRPCreate_libsrp( SRPRef_libsrp *outRef );
void		SRPDelete_libsrp( SRPRef_libsrp inRef );
#define 	SRPForget_libsrp( X )	ForgetCustom( X, SRPDelete_libsrp )

OSStatus
	SRPClientStart_libsrp( 
		SRPRef_libsrp		inClient, 
		SRPParametersRef	inParams, 
		const void *		inUsernamePtr, 
		size_t				inUsernameLen, 
		const void *		inPasswordPtr, 
		size_t				inPasswordLen, 
		const void *		inSaltPtr, 
		size_t				inSaltLen, 
		const void *		inServerPKPtr, 
		size_t				inServerPKLen, 
		uint8_t **			outClientPKPtr, 
		size_t *			outClientPKLen, 
		uint8_t **			outSharedSecretPtr, 
		size_t *			outSharedSecretLen, 
		uint8_t **			outResponsePtr, 
		size_t *			outResponseLen );

OSStatus	SRPClientVerify_libsrp( SRPRef_libsrp inClient, const void *inResponsePtr, size_t inResponseLen );

OSStatus
	SRPServerStart_libsrp( 
		SRPRef_libsrp 		inServer, 
		SRPParametersRef	inParams, 
		const void *		inUsernamePtr, 
		size_t				inUsernameLen, 
		const void *		inPasswordPtr, 
		size_t				inPasswordLen, 
		size_t				inSaltLen, 
		uint8_t *			outSalt, 
		uint8_t **			outServerPKPtr, 
		size_t *			outServerPKLen );

OSStatus
	SRPServerVerify_libsrp( 
		SRPRef_libsrp		inServer, 
		const void *		inClientPKPtr, 
		size_t				inClientPKLen, 
		const void *		inResponsePtr, 
		size_t				inResponseLen, 
		uint8_t **			outSharedSecretPtr, 
		size_t *			outSharedSecretLen, 
		uint8_t **			outResponsePtr, 
		size_t *			outResponseLen );

#endif // TARGET_HAS_LIBSRP

#if( TARGET_HAS_CORECRYPTO_SRP )
//===========================================================================================================================
//	corecrypto
//===========================================================================================================================

typedef struct SRPPrivate_corecrypto *		SRPRef_corecrypto;

OSStatus	SRPCreate_corecrypto( SRPRef_corecrypto *outRef );
void		SRPDelete_corecrypto( SRPRef_corecrypto inRef );
#define 	SRPForget_corecrypto( X )	ForgetCustom( X, SRPDelete_corecrypto )

OSStatus
	SRPClientStart_corecrypto( 
		SRPRef_corecrypto	inClient, 
		SRPParametersRef	inParams, 
		const void *		inUsernamePtr, 
		size_t				inUsernameLen, 
		const void *		inPasswordPtr, 
		size_t				inPasswordLen, 
		const void *		inSaltPtr, 
		size_t				inSaltLen, 
		const void *		inServerPKPtr, 
		size_t				inServerPKLen, 
		uint8_t **			outClientPKPtr, 
		size_t *			outClientPKLen, 
		uint8_t **			outSharedSecretPtr, 
		size_t *			outSharedSecretLen, 
		uint8_t **			outResponsePtr, 
		size_t *			outResponseLen );

OSStatus	SRPClientVerify_corecrypto( SRPRef_corecrypto inClient, const void *inResponsePtr, size_t inResponseLen );

OSStatus
	SRPServerStart_corecrypto( 
		SRPRef_corecrypto 	inServer, 
		SRPParametersRef	inParams, 
		const void *		inUsernamePtr, 
		size_t				inUsernameLen, 
		const void *		inPasswordPtr, 
		size_t				inPasswordLen, 
		size_t				inSaltLen, 
		uint8_t *			outSalt, 
		uint8_t **			outServerPKPtr, 
		size_t *			outServerPKLen );

OSStatus
	SRPServerVerify_corecrypto( 
		SRPRef_corecrypto	inServer, 
		const void *		inClientPKPtr, 
		size_t				inClientPKLen, 
		const void *		inResponsePtr, 
		size_t				inResponseLen, 
		uint8_t **			outSharedSecretPtr, 
		size_t *			outSharedSecretLen, 
		uint8_t **			outResponsePtr, 
		size_t *			outResponseLen );

#endif // TARGET_HAS_CORECRYPTO_SRP

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn		SRPUtilsTest
	@brief	Unit test.
*/
OSStatus	SRPUtilsTest( void );

#ifdef __cplusplus
}
#endif

#endif // __SRPUtils_h__
