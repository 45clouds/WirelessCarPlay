/*
	File:    	SRPUtils.c
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
	
	Copyright (C) 2014 Apple Inc. All Rights Reserved.
*/

#include "SRPUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"
#include "MiscUtils.h"
#include "RandomNumberUtils.h"
#include "StringUtils.h"

#if( TARGET_HAS_LIBSRP )
	#include "cstr.h"
	#include "srp.h"
#endif

#if 0
#pragma mark == corecrypto ==
#endif

#if( TARGET_HAS_CORECRYPTO_SRP )

#if( COMPILER_CLANG )
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wgnu-union-cast"
#endif

//===========================================================================================================================
//	Internals
//===========================================================================================================================

struct SRPParametersPrivate
{
	uint8_t		value;
};

static const struct SRPParametersPrivate		_kSRPParameters_3072_SHA512 = { 1 };
SRPParametersRef const		kSRPParameters_3072_SHA512 = &_kSRPParameters_3072_SHA512;

struct SRPPrivate_corecrypto
{
	ccsrp_ctx *		srpCtx;
	size_t			srpLen;
	char *			username;
	uint8_t *		saltPtr;
	size_t			saltLen;
	uint8_t *		verifierPtr;
	size_t			verifierLen;
};

static void	_SRPCleanup_corecrypto( SRPRef_corecrypto inRef );

//===========================================================================================================================
//	SRPCreate_corecrypto
//===========================================================================================================================

OSStatus	SRPCreate_corecrypto( SRPRef_corecrypto *outRef )
{
	OSStatus				err;
	SRPRef_corecrypto		obj;
	
	obj = (SRPRef_corecrypto) calloc( 1, sizeof( *obj ) );
	require_action( obj, exit, err = kNoMemoryErr );
	
	*outRef = obj;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	SRPDelete_corecrypto
//===========================================================================================================================

void	SRPDelete_corecrypto( SRPRef_corecrypto inRef )
{
	if( inRef )
	{
		_SRPCleanup_corecrypto( inRef );
		free( inRef );
	}
}

//===========================================================================================================================
//	_SRPCleanup_corecrypto
//===========================================================================================================================

static void	_SRPCleanup_corecrypto( SRPRef_corecrypto inRef )
{
	ForgetPtrLenSecure( &inRef->srpCtx, &inRef->srpLen );
	ForgetMem( &inRef->username );
	ForgetPtrLenSecure( &inRef->saltPtr, &inRef->saltLen );
	ForgetPtrLenSecure( &inRef->verifierPtr, &inRef->verifierLen );
}

//===========================================================================================================================
//	SRPClientStart_corecrypto
//===========================================================================================================================

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
		size_t *			outResponseLen )
{
	OSStatus							err;
	const struct ccdigest_info *		di;
	ccsrp_const_gp_t					gp;
	size_t								pkLen;
	size_t								digestLen;
	uint8_t *							pkPtr			= NULL;
	char *								username		= NULL;
	uint8_t *							responsePtr		= NULL;
	uint8_t *							sharedSecretPtr;
	
	if( inParams == kSRPParameters_3072_SHA512 )
	{
		di = ccsha512_di();
		gp = ccsrp_gp_rfc5054_3072();
	}
	else
	{
		dlogassert( "Bad parameters" );
		err = kUnsupportedErr;
		goto exit;
	}
	
	inClient->srpLen = ccsrp_sizeof_srp( di, gp );
	inClient->srpCtx = (ccsrp_ctx *) calloc( 1, inClient->srpLen );
	require_action( inClient->srpCtx, exit, err = kNoMemoryErr );
	ccsrp_ctx_init( inClient->srpCtx, di, gp );
	
	pkLen = ccsrp_ctx_sizeof_n( inClient->srpCtx );
	require_action_quiet( inServerPKLen == pkLen, exit, err = kSizeErr );
	pkPtr = (uint8_t *) malloc( pkLen );
	require_action( pkPtr, exit, err = kNoMemoryErr );
	err = ccsrp_client_start_authentication( inClient->srpCtx, ccrng_system_get(), pkPtr );
	require_noerr_action( err, exit, err = kUnknownErr );
	
	if( inUsernameLen == kSizeCString ) inUsernameLen = strlen( (const char *) inUsernamePtr );
	username = (char *) strndup( inUsernamePtr, inUsernameLen );
	require_action( username, exit, err = kNoMemoryErr );
	
	digestLen = ccsrp_ctx_keysize( inClient->srpCtx );
	responsePtr = (uint8_t *) malloc( digestLen );
	require_action( responsePtr, exit, err = kNoMemoryErr );
	
	if( inPasswordLen == kSizeCString ) inPasswordLen = strlen( (const char *) inPasswordPtr );
	err = ccsrp_client_process_challenge( inClient->srpCtx, username, inPasswordLen, inPasswordPtr, inSaltLen, inSaltPtr, 
		inServerPKPtr, responsePtr );
	require_noerr_action_quiet( err, exit, err = kAuthenticationErr );
	
	sharedSecretPtr = (uint8_t *) malloc( digestLen );
	require_action( sharedSecretPtr, exit, err = kNoMemoryErr );
	memcpy( sharedSecretPtr, ccsrp_ctx_K( inClient->srpCtx ), digestLen );
	
	*outClientPKPtr		= pkPtr;
	*outClientPKLen		= pkLen;
	*outSharedSecretPtr	= sharedSecretPtr;
	*outSharedSecretLen	= digestLen;
	*outResponsePtr		= responsePtr;
	*outResponseLen 	= digestLen;
	pkPtr				= NULL;
	responsePtr			= NULL;
	
exit:
	FreeNullSafe( pkPtr );
	FreeNullSafe( username );
	FreeNullSafe( responsePtr );
	if( err ) _SRPCleanup_corecrypto( inClient );
	return( err );
}

//===========================================================================================================================
//	SRPClientVerify_corecrypto
//===========================================================================================================================

OSStatus	SRPClientVerify_corecrypto( SRPRef_corecrypto inClient, const void *inResponsePtr, size_t inResponseLen )
{
	OSStatus		err;
	
	require_action( inClient->srpCtx, exit, err = kNotPreparedErr );
	
	require_action_quiet( inResponseLen == ccsrp_ctx_keysize( inClient->srpCtx ), exit, err = kSizeErr );
	err = ccsrp_client_verify_session( inClient->srpCtx, inResponsePtr ) ? kNoErr : kAuthenticationErr;
	
exit:
	_SRPCleanup_corecrypto( inClient );
	return( err );
}

//===========================================================================================================================
//	SRPServerStart_corecrypto
//===========================================================================================================================

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
		size_t *			outServerPKLen )
{
	OSStatus							err;
	const struct ccdigest_info *		di;
	ccsrp_const_gp_t					gp;
	struct ccrng_state *				rng;
	size_t								pkLen;
	uint8_t *							pkPtr = NULL;
	
	if( inParams == kSRPParameters_3072_SHA512 )
	{
		di = ccsha512_di();
		gp = ccsrp_gp_rfc5054_3072();
	}
	else
	{
		dlogassert( "Bad parameters" );
		err = kUnsupportedErr;
		goto exit;
	}
	
	inServer->srpLen = ccsrp_sizeof_srp( di, gp );
	inServer->srpCtx = (ccsrp_ctx *) calloc( 1, inServer->srpLen );
	require_action( inServer->srpCtx, exit, err = kNoMemoryErr );
	ccsrp_ctx_init( inServer->srpCtx, di, gp );
	
	pkLen = ccsrp_ctx_sizeof_n( inServer->srpCtx );
	inServer->verifierPtr = (uint8_t *) malloc( pkLen );
	require_action( inServer->verifierPtr, exit, err = kNoMemoryErr );
	
	if( inUsernameLen == kSizeCString ) inUsernameLen = strlen( (const char *) inUsernamePtr );
	inServer->username = strndup( inUsernamePtr, inUsernameLen );
	require_action( inServer->username, exit, err = kNoMemoryErr );
	
	inServer->saltPtr = (uint8_t *) malloc( inSaltLen );
	require_action( inServer->saltPtr, exit, err = kNoMemoryErr );
	inServer->saltLen = inSaltLen;
	
	rng = ccrng_system_get();
	if( inPasswordLen == kSizeCString ) inPasswordLen = strlen( (const char *) inPasswordPtr );
	err = ccsrp_generate_salt_and_verification( inServer->srpCtx, rng, inServer->username, inPasswordLen, inPasswordPtr, 
		inSaltLen, inServer->saltPtr, inServer->verifierPtr );
	require_noerr_action( err, exit, err = kUnknownErr );
	memcpy( outSalt, inServer->saltPtr, inSaltLen );
	
	pkPtr = (uint8_t *) malloc( pkLen );
	require_action( pkPtr, exit, err = kNoMemoryErr );
	err = ccsrp_server_generate_public_key( inServer->srpCtx, rng, inServer->verifierPtr, pkPtr );
	require_noerr_action( err, exit, err = kUnknownErr );
	
	*outServerPKPtr = pkPtr;
	*outServerPKLen = pkLen;
	pkPtr = NULL;
	
exit:
	FreeNullSafe( pkPtr );
	if( err ) _SRPCleanup_corecrypto( inServer );
	return( err );
}

//===========================================================================================================================
//	SRPServerVerify_corecrypto
//===========================================================================================================================

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
		size_t *			outResponseLen )
{
	OSStatus		err;
	size_t			digestLen;
	uint8_t *		responsePtr = NULL;
	uint8_t *		sharedSecretPtr;
	
	require_action( inServer->srpCtx, exit, err = kNotPreparedErr );
	require_action( inServer->username, exit, err = kNotPreparedErr );
	require_action( inServer->saltPtr, exit, err = kNotPreparedErr );
	require_action( inServer->saltLen > 0, exit, err = kNotPreparedErr );
	require_action( inClientPKLen == ccsrp_ctx_sizeof_n( inServer->srpCtx ), exit, err = kSizeErr );
	digestLen = ccsrp_ctx_keysize( inServer->srpCtx );
	require_action( inResponseLen == digestLen, exit, err = kSizeErr );
	
	err = ccsrp_server_compute_session( inServer->srpCtx, inServer->username, 
		inServer->saltLen, inServer->saltPtr, inClientPKPtr );
	require_noerr_action_quiet( err, exit, err = kAuthenticationErr );
	
	responsePtr = (uint8_t *) malloc( digestLen );
	require_action( responsePtr, exit, err = kNoMemoryErr );
	err = ccsrp_server_verify_session( inServer->srpCtx, inResponsePtr, responsePtr ) ? kNoErr : kAuthenticationErr;
	require_noerr_quiet( err, exit );
	
	sharedSecretPtr = (uint8_t *) malloc( digestLen );
	require_action( sharedSecretPtr, exit, err = kNoMemoryErr );
	memcpy( sharedSecretPtr, ccsrp_ctx_K( inServer->srpCtx ), digestLen );
	
	*outSharedSecretPtr	= sharedSecretPtr;
	*outSharedSecretLen	= digestLen;
	*outResponsePtr		= responsePtr;
	*outResponseLen		= digestLen;
	responsePtr			= NULL;
	
exit:
	FreeNullSafe( responsePtr );
	_SRPCleanup_corecrypto( inServer );
	return( err );
}

#if( COMPILER_CLANG )
	#pragma clang diagnostic pop
#endif
	
#endif // TARGET_HAS_CORECRYPTO_SRP

#if( TARGET_HAS_LIBSRP )

#if 0
#pragma mark -
#pragma mark == libsrp ==
#endif

//===========================================================================================================================
//	Internals
//===========================================================================================================================

struct SRPParametersPrivate
{
	const void *	generatorPtr;
	size_t			generatorLen;
	const void *	modulusPtr;
	size_t			modulusLen;
};

// SRP parameters from RFC-5054 (3072-bit group).

static const uint8_t		kSRPGenerator5[] = { 5 };
static const uint8_t		kSRPModulus3072[] =
{
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34, 
	0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1, 0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, 
	0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD, 
	0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37, 
	0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45, 0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, 
	0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B, 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED, 
	0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, 0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6, 
	0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D, 0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05, 
	0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A, 0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F, 
	0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96, 0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB, 
	0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D, 0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04, 
	0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x18, 0x21, 0x7C, 0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B, 
	0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03, 0x9B, 0x27, 0x83, 0xA2, 0xEC, 0x07, 0xA2, 0x8F, 
	0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9, 0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18, 
	0x39, 0x95, 0x49, 0x7C, 0xEA, 0x95, 0x6A, 0xE5, 0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10, 
	0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAA, 0xC4, 0x2D, 0xAD, 0x33, 0x17, 0x0D, 0x04, 0x50, 0x7A, 0x33, 
	0xA8, 0x55, 0x21, 0xAB, 0xDF, 0x1C, 0xBA, 0x64, 0xEC, 0xFB, 0x85, 0x04, 0x58, 0xDB, 0xEF, 0x0A, 
	0x8A, 0xEA, 0x71, 0x57, 0x5D, 0x06, 0x0C, 0x7D, 0xB3, 0x97, 0x0F, 0x85, 0xA6, 0xE1, 0xE4, 0xC7, 
	0xAB, 0xF5, 0xAE, 0x8C, 0xDB, 0x09, 0x33, 0xD7, 0x1E, 0x8C, 0x94, 0xE0, 0x4A, 0x25, 0x61, 0x9D, 
	0xCE, 0xE3, 0xD2, 0x26, 0x1A, 0xD2, 0xEE, 0x6B, 0xF1, 0x2F, 0xFA, 0x06, 0xD9, 0x8A, 0x08, 0x64, 
	0xD8, 0x76, 0x02, 0x73, 0x3E, 0xC8, 0x6A, 0x64, 0x52, 0x1F, 0x2B, 0x18, 0x17, 0x7B, 0x20, 0x0C, 
	0xBB, 0xE1, 0x17, 0x57, 0x7A, 0x61, 0x5D, 0x6C, 0x77, 0x09, 0x88, 0xC0, 0xBA, 0xD9, 0x46, 0xE2, 
	0x08, 0xE2, 0x4F, 0xA0, 0x74, 0xE5, 0xAB, 0x31, 0x43, 0xDB, 0x5B, 0xFC, 0xE0, 0xFD, 0x10, 0x8E, 
	0x4B, 0x82, 0xD1, 0x20, 0xA9, 0x3A, 0xD2, 0xCA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const struct SRPParametersPrivate		_kSRPParameters_3072_SHA512 = 
{
	kSRPGenerator5,  sizeof( kSRPGenerator5 ), 
	kSRPModulus3072, sizeof( kSRPModulus3072 )
};
SRPParametersRef const		kSRPParameters_3072_SHA512 = &_kSRPParameters_3072_SHA512;

struct SRPPrivate_libsrp
{
	SRP *		srpCtx;
};

//===========================================================================================================================
//	SRPCreate_libsrp
//===========================================================================================================================

OSStatus	SRPCreate_libsrp( SRPRef_libsrp *outRef )
{
	OSStatus			err;
	SRPRef_libsrp		obj;
	
	obj = (SRPRef_libsrp) calloc( 1, sizeof( *obj ) );
	require_action( obj, exit, err = kNoMemoryErr );
	
	*outRef = obj;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	SRPDelete_libsrp
//===========================================================================================================================

void	SRPDelete_libsrp( SRPRef_libsrp inRef )
{
	SRP_forget( &inRef->srpCtx );
	free( inRef );
}

//===========================================================================================================================
//	SRPClientStart_libsrp
//===========================================================================================================================

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
		size_t *			outResponseLen )
{
	OSStatus		err;
	cstr *			clientPKData		= NULL;
	cstr *			sharedSecretData	= NULL;
	cstr *			responseData		= NULL;
	
	SRP_forget( &inClient->srpCtx );
	inClient->srpCtx = SRP_new( SRP6a_client_method(), &kSRPHashDescriptor_SHA512 );
	require_action( inClient->srpCtx, exit, err = kNoMemoryErr );
	
	if( inUsernameLen == kSizeCString ) inUsernameLen = strlen( (const char *) inUsernamePtr );
	err = SRP_set_user_raw( inClient->srpCtx, (const unsigned char *) inUsernamePtr, (int) inUsernameLen );
	require_noerr_action( err, exit, err = kUnknownErr );
	
	err = SRP_set_params( inClient->srpCtx, inParams->modulusPtr, (int) inParams->modulusLen, 
		inParams->generatorPtr, (int) inParams->generatorLen, inSaltPtr, (int) inSaltLen );
	require_noerr_action( err, exit, err = kUnknownErr );
	
	if( inPasswordLen == kSizeCString ) inPasswordLen = strlen( (const char *) inPasswordPtr );
	err = SRP_set_auth_password_raw( inClient->srpCtx, (const unsigned char *) inPasswordPtr, (int) inPasswordLen );
	require_noerr_action( err, exit, err = kUnknownErr );
	
	err = SRP_gen_pub( inClient->srpCtx, &clientPKData );
	require_noerr_action( err, exit, err = kUnknownErr );
	require_action( clientPKData->length > 0, exit, err = kSizeErr );
	
	err = SRP_compute_key( inClient->srpCtx, &sharedSecretData, inServerPKPtr, (int) inServerPKLen );
	require_noerr_action( err, exit, err = kUnknownErr );
	require_action( sharedSecretData->length > 0, exit, err = kSizeErr );
	
	err = SRP_respond( inClient->srpCtx, &responseData );
	require_noerr_action( err, exit, err = kUnknownErr );
	require_action( responseData->length > 0, exit, err = kSizeErr );
	
	cstr_detach( clientPKData, outClientPKPtr, outClientPKLen );
	cstr_detach( sharedSecretData, outSharedSecretPtr, outSharedSecretLen );
	cstr_detach( responseData, outResponsePtr, outResponseLen );
	
exit:
	SRP_cstr_forget( &clientPKData );
	SRP_cstr_clear_forget( &sharedSecretData );
	SRP_cstr_forget( &responseData );
	return( err );
}

//===========================================================================================================================
//	SRPClientVerify_libsrp
//===========================================================================================================================

OSStatus	SRPClientVerify_libsrp( SRPRef_libsrp inClient, const void *inResponsePtr, size_t inResponseLen )
{
	OSStatus		err;
	
	require_action( inClient->srpCtx, exit, err = kNotPreparedErr );
	
	err = SRP_verify( inClient->srpCtx, inResponsePtr, (int) inResponseLen );
	require_noerr_action_quiet( err, exit, err = kAuthenticationErr );
	
exit:
	SRP_forget( &inClient->srpCtx );
	return( err );
}

//===========================================================================================================================
//	SRPServerStart_libsrp
//===========================================================================================================================

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
		size_t *			outServerPKLen )
{
	OSStatus		err;
	cstr *			serverPKData = NULL;
	
	SRP_forget( &inServer->srpCtx );
	inServer->srpCtx = SRP_new( SRP6a_server_method(), &kSRPHashDescriptor_SHA512 );
	require_action( inServer->srpCtx, exit, err = kNoMemoryErr );
	
	if( inUsernameLen == kSizeCString ) inUsernameLen = strlen( (const char *) inUsernamePtr );
	err = SRP_set_user_raw( inServer->srpCtx, inUsernamePtr, (int) inUsernameLen );
	require_noerr_action( err, exit, err = kUnknownErr );
	
	require_action( inSaltLen >= 16, exit, err = kParamErr );
	err = RandomBytes( outSalt, inSaltLen );
	require_noerr( err, exit );
	err = SRP_set_params( inServer->srpCtx, inParams->modulusPtr, (int) inParams->modulusLen, 
		inParams->generatorPtr, (int) inParams->generatorLen, (const uint8_t *) outSalt, (int) inSaltLen );
	require_noerr_action( err, exit, err = kUnknownErr );
	
	if( inPasswordLen == kSizeCString ) inPasswordLen = strlen( (const char *) inPasswordPtr );
	err = SRP_set_auth_password_raw( inServer->srpCtx, (const unsigned char *) inPasswordPtr, (int) inPasswordLen );
	require_noerr_action( err, exit, err = kUnknownErr );
	
	err = SRP_gen_pub( inServer->srpCtx, &serverPKData );
	require_noerr_action( err, exit, err = kUnknownErr );
	
	cstr_detach( serverPKData, outServerPKPtr, outServerPKLen );
	
exit:
	SRP_cstr_forget( &serverPKData );
	return( err );
}

//===========================================================================================================================
//	SRPServerVerify_libsrp
//===========================================================================================================================

OSStatus
	SRPServerVerify_libsrp( 
		SRPRef_libsrp	inServer, 
		const void *	inClientPKPtr, 
		size_t			inClientPKLen, 
		const void *	inResponsePtr, 
		size_t			inResponseLen, 
		uint8_t **		outSharedSecretPtr, 
		size_t *		outSharedSecretLen, 
		uint8_t **		outResponsePtr, 
		size_t *		outResponseLen )
{
	OSStatus		err;
	cstr *			sharedSecretData	= NULL;
	cstr *			responseData		= NULL;
	
	require_action( inServer->srpCtx, exit, err = kNotPreparedErr );
	
	err = SRP_compute_key( inServer->srpCtx, &sharedSecretData, (const uint8_t *) inClientPKPtr, (int) inClientPKLen );
	require_noerr_action( err, exit, err = kUnknownErr );
	require_action( sharedSecretData->length > 0, exit, err = kSizeErr );
	
	err = SRP_verify( inServer->srpCtx, inResponsePtr, (int) inResponseLen );
	require_noerr_action_quiet( err, exit, err = kAuthenticationErr );
	
	err = SRP_respond( inServer->srpCtx, &responseData );
	require_noerr_action( err, exit, err = kUnknownErr );
	require_action( responseData->length > 0, exit, err = kSizeErr );
	
	cstr_detach( sharedSecretData, outSharedSecretPtr, outSharedSecretLen );
	cstr_detach( responseData, outResponsePtr, outResponseLen );
	
exit:
	SRP_cstr_clear_forget( &sharedSecretData );
	SRP_cstr_forget( &responseData );
	SRP_forget( &inServer->srpCtx );
	return( err );
}

#endif // TARGET_HAS_LIBSRP

#if 0
#pragma mark -
#pragma mark == Testing ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
static OSStatus
	_SRPTestOne( 
		const char *	inClientUser, 
		const char *	inClientPassword, 
		const char *	inServerUser, 
		const char *	inServerPassword );

//===========================================================================================================================
//	SRPUtilsTest
//===========================================================================================================================

OSStatus	SRPUtilsTest( void )
{
	OSStatus			err;
	char				user[ 64 ], pass[ 64 ];
	int					i;
	
	for( i = 0; i < 10; ++i )
	{
		RandomString( kAlphaNumericCharSet, sizeof( kAlphaNumericCharSet ), 1, sizeof( user ) - 1, user );
		RandomString( kAlphaNumericCharSet, sizeof( kAlphaNumericCharSet ), 1, sizeof( pass ) - 1, pass );
		err = _SRPTestOne( user, pass, user, pass );
		require_noerr( err, exit );
	}
	
	err = _SRPTestOne( "user", "pass", "user1", "pass" ) ? kNoErr : -1;
	require_noerr( err, exit );
	
	err = _SRPTestOne( "user", "pass", "user", "pass1" ) ? kNoErr : -1;
	require_noerr( err, exit );
	
	err = _SRPTestOne( "user", "pass", "", "pass" ) ? kNoErr : -1;
	require_noerr( err, exit );
	
	err = _SRPTestOne( "user", "pass", "user", "" ) ? kNoErr : -1;
	require_noerr( err, exit );
	
exit:
	printf( "SRPUtilsTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

//===========================================================================================================================
//	_SRPTestOne
//===========================================================================================================================

static OSStatus
	_SRPTestOne( 
		const char *	inClientUser, 
		const char *	inClientPassword, 
		const char *	inServerUser, 
		const char *	inServerPassword )
{
	OSStatus		err;
	SRPRef			client					= NULL;
	SRPRef			server					= NULL;
	uint8_t			salt[ 16 ];
	uint8_t *		serverPKPtr				= NULL;
	size_t			serverPKLen;
	uint8_t *		serverSharedSecretPtr	= NULL;
	size_t			serverSharedSecretLen;
	uint8_t *		serverResponsePtr		= NULL;
	size_t			serverResponseLen;
	uint8_t *		clientPKPtr				= NULL;
	size_t			clientPKLen;
	uint8_t *		clientSharedSecretPtr	= NULL;
	size_t			clientSharedSecretLen;
	uint8_t *		clientResponsePtr		= NULL;
	size_t			clientResponseLen;
	
	err = SRPCreate( &client );
	require_noerr( err, exit );
	
	err = SRPCreate( &server );
	require_noerr( err, exit );
	
	memset( salt, 'a', sizeof( salt ) );
	err = SRPServerStart( server, kSRPParameters_3072_SHA512, inServerUser, kSizeCString, 
		inServerPassword, kSizeCString, sizeof( salt ), salt, &serverPKPtr, &serverPKLen );
	require_noerr( err, exit );
	
	err = SRPClientStart( client, kSRPParameters_3072_SHA512, inClientUser, kSizeCString, 
		inClientPassword, kSizeCString, salt, sizeof( salt ), serverPKPtr, serverPKLen, &clientPKPtr, &clientPKLen, 
		&clientSharedSecretPtr, &clientSharedSecretLen, &clientResponsePtr, &clientResponseLen );
	require_noerr( err, exit );
	
	err = SRPServerVerify( server, clientPKPtr, clientPKLen, clientResponsePtr, clientResponseLen, 
		&serverSharedSecretPtr, &serverSharedSecretLen, &serverResponsePtr, &serverResponseLen );
	require_noerr_quiet( err, exit );
	
	err = SRPClientVerify( client, serverResponsePtr, serverResponseLen );
	require_noerr_quiet( err, exit );
	
	require_action( MemEqual( clientSharedSecretPtr, clientSharedSecretLen, serverSharedSecretPtr, serverSharedSecretLen ), 
		exit, err = -1 );
	
exit:
	SRPForget( &client );
	SRPForget( &server );
	FreeNullSafe( serverPKPtr );
	FreeNullSafe( serverSharedSecretPtr );
	FreeNullSafe( serverResponsePtr );
	FreeNullSafe( clientPKPtr );
	FreeNullSafe( clientSharedSecretPtr );
	FreeNullSafe( clientResponsePtr );
	return( err );
}

#endif // !EXCLUDE_UNIT_TESTS
