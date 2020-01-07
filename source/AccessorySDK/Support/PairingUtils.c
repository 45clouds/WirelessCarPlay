/*
	File:    	PairingUtils.c
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

#include "PairingUtils.h"

#include "CFUtils.h"
#include "ChaCha20Poly1305.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "PrintFUtils.h"
#include "RandomNumberUtils.h"
#include "SHAUtils.h"
#include "SRPUtils.h"
#include "StringUtils.h"
#include "ThreadUtils.h"
#include "TickUtils.h"
#include "TLVUtils.h"
#include "UUIDUtils.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include CURVE25519_HEADER
#include ED25519_HEADER
#include LIBDISPATCH_HEADER

#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
	#include <Security/Security.h>
#endif

//===========================================================================================================================
//	Configuration
//===========================================================================================================================

// PAIRING_KEYCHAIN

#if( !defined( PAIRING_KEYCHAIN ) )
	#if( KEYCHAIN_ENABLED )
		#define PAIRING_KEYCHAIN		1
	#else
		#define PAIRING_KEYCHAIN		0
	#endif
#endif

#if( PAIRING_KEYCHAIN )
	#include "KeychainUtils.h"
#endif

// PAIRING_MFI_CLIENT: 1=Enable client-side code to support MFi pairing.

#if( !defined( PAIRING_MFI_CLIENT ) )
	#if( TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES )
		#define PAIRING_MFI_CLIENT		1
	#else
		#define PAIRING_MFI_CLIENT		0
	#endif
#endif

// PAIRING_MFI_SERVER: 1=Enable server-side code to support MFi pairing.

#if( !defined( PAIRING_MFI_SERVER ) )
	#if( TARGET_OS_WINDOWS )
		#define PAIRING_MFI_SERVER		0
	#else
		#define PAIRING_MFI_SERVER		1
	#endif
#endif

#if( PAIRING_MFI_CLIENT || PAIRING_MFI_SERVER )
	#include "MFiSAP.h"
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

#if( PAIRING_KEYCHAIN )
	#define kPairingKeychainTypeCode_Identity		0x70724964 // 'prId' (for use with kSecAttrType).
	#define kPairingKeychainTypeCode_Peer			0x70725065 // 'prPe' (for use with kSecAttrType).
#endif

#if( PAIRING_MFI_CLIENT || PAIRING_MFI_SERVER )
	#define kMFiPairSetupSaltPtr			"MFi-Pair-Setup-Salt"
	#define kMFiPairSetupSaltLen			sizeof_string( kMFiPairSetupSaltPtr )
	#define kMFiPairSetupInfoPtr			"MFi-Pair-Setup-Info"
	#define kMFiPairSetupInfoLen			sizeof_string( kMFiPairSetupInfoPtr )
#endif

#define kPairSetupSRPUsernamePtr			"Pair-Setup"
#define kPairSetupSRPUsernameLen			sizeof_string( kPairSetupSRPUsernamePtr )

#define kPairSetupEncryptSaltPtr			"Pair-Setup-Encrypt-Salt"
#define kPairSetupEncryptSaltLen			sizeof_string( kPairSetupEncryptSaltPtr )
#define kPairSetupEncryptInfoPtr			"Pair-Setup-Encrypt-Info"
#define kPairSetupEncryptInfoLen			sizeof_string( kPairSetupEncryptInfoPtr )

#define kPairSetupAccessorySignSaltPtr		"Pair-Setup-Accessory-Sign-Salt"
#define kPairSetupAccessorySignSaltLen		sizeof_string( kPairSetupAccessorySignSaltPtr )
#define kPairSetupAccessorySignInfoPtr		"Pair-Setup-Accessory-Sign-Info"
#define kPairSetupAccessorySignInfoLen		sizeof_string( kPairSetupAccessorySignInfoPtr )

#define kPairSetupControllerSignSaltPtr		"Pair-Setup-Controller-Sign-Salt"
#define kPairSetupControllerSignSaltLen		sizeof_string( kPairSetupControllerSignSaltPtr )
#define kPairSetupControllerSignInfoPtr		"Pair-Setup-Controller-Sign-Info"
#define kPairSetupControllerSignInfoLen		sizeof_string( kPairSetupControllerSignInfoPtr )

#define kPairVerifyECDHSaltPtr				"Pair-Verify-ECDH-Salt"
#define kPairVerifyECDHSaltLen				sizeof_string( kPairVerifyECDHSaltPtr )
#define kPairVerifyECDHInfoPtr				"Pair-Verify-ECDH-Info"
#define kPairVerifyECDHInfoLen				sizeof_string( kPairVerifyECDHInfoPtr )

#define kPairVerifyEncryptSaltPtr			"Pair-Verify-Encrypt-Salt"
#define kPairVerifyEncryptSaltLen			sizeof_string( kPairVerifyEncryptSaltPtr )
#define kPairVerifyEncryptInfoPtr			"Pair-Verify-Encrypt-Info"
#define kPairVerifyEncryptInfoLen			sizeof_string( kPairVerifyEncryptInfoPtr )

// TLV items

#define kMaxTLVSize						16000

#define kTLVType_Method					0x00 // Pairing method to use.
	#define kTLVMethod_PairSetup			0 // Pair-setup.
	#define kTLVMethod_MFiPairSetup			1 // MFi pair-setup.
	#define kTLVMethod_Verify				2 // Pair-verify.
#define kTLVType_Identifier				0x01 // Identifier of the peer.
#define kTLVType_Salt					0x02 // 16+ bytes of random salt.
#define kTLVType_PublicKey				0x03 // Curve25519, SRP public key, or signed Ed25519 key.
#define kTLVType_Proof					0x04 // SRP proof.
#define kTLVType_EncryptedData			0x05 // Encrypted bytes. Use AuthTag to authenticate.
#define kTLVType_State					0x06 // State of the pairing process.
#define kTLVType_Error					0x07 // Error code. Missing means no error.
	#define kTLVError_Reserved0				0x00 // Must not be used in any TLV.
	#define kTLVError_Unknown				0x01 // Generic error to handle unexpected errors.
	#define kTLVError_Authentication		0x02 // Setup code or signature verification failed.
	#define kTLVError_Backoff				0x03 // Client must look at <RetryDelay> TLV item and wait before retrying.
	#define kTLVError_UnknownPeer			0x04 // Peer is not paired.
	#define kTLVError_MaxPeers				0x05 // Server cannot accept any more pairings.
	#define kTLVError_MaxTries				0x06 // Server reached its maximum number of authentication attempts
#define kTLVType_RetryDelay				0x08 // Seconds to delay until retrying setup.
#define kTLVType_Certificate			0x09 // X.509 Certificate.
#define kTLVType_Signature				0x0A // Ed25519 or MFi auth IC signature.
#define kTLVType_ReservedB				0x0B // Reserved.
#define kTLVType_FragmentData			0x0C // Non-last fragment of data. If length is 0, it's an ack.
#define kTLVType_FragmentLast			0x0D // Last fragment of data.

#define kTLVDescriptors \
	"\x00" "Method\0" \
	"\x01" "Identifier\0" \
	"\x02" "Salt\0" \
	"\x03" "Public Key\0" \
	"\x04" "Proof\0" \
	"\x05" "EncryptedData\0" \
	"\x06" "State\0" \
	"\x07" "Error\0" \
	"\x08" "RetryDelay\0" \
	"\x09" "Certificate\0" \
	"\x0A" "Signature\0" \
	"\x0C" "FragmentData\0" \
	"\x0D" "FragmentLast\0" \
	"\x00"

#define PairingStatusToOSStatus( X ) ( \
	( (X) == kTLVError_Reserved0 )			? kValueErr : \
	( (X) == kTLVError_Unknown )			? kUnknownErr : \
	( (X) == kTLVError_Authentication )		? kAuthenticationErr : \
	( (X) == kTLVError_Backoff )			? kBackoffErr : \
	( (X) == kTLVError_UnknownPeer )		? kNotFoundErr : \
	( (X) == kTLVError_MaxPeers )			? kNoSpaceErr : \
	( (X) == kTLVError_MaxTries )			? kCountErr : \
											  kUnknownErr )

#define OSStatusToPairingStatus( X ) ( \
	( (X) == kUnknownErr )					? kTLVError_Unknown : \
	( (X) == kAuthenticationErr )			? kTLVError_Authentication : \
	( (X) == kBackoffErr )					? kTLVError_Backoff : \
	( (X) == kNotFoundErr )					? kTLVError_UnknownPeer : \
	( (X) == kNoSpaceErr )					? kTLVError_MaxPeers : \
	( (X) == kCountErr )					? kTLVError_MaxTries : \
											  kTLVError_Unknown )

// States

#define kPairingStateInvalid		0

#define kPairSetupStateM1			1 // Controller -> Accessory  -- Start Request
#define kPairSetupStateM2			2 // Accessory  -> Controller -- Start Response
#define kPairSetupStateM3			3 // Controller -> Accessory  -- Verify Request
#define kPairSetupStateM4			4 // Accessory  -> Controller -- Verify Response
#define kPairSetupStateM5			5 // Controller -> Accessory  -- Exchange Request
#define kPairSetupStateM6			6 // Accessory  -> Controller -- Exchange Response
#define kPairSetupStateDone			7

#define kPairVerifyStateM1			1 // Controller -> Accessory  -- Start Request
#define kPairVerifyStateM2			2 // Accessory  -> Controller -- Start Response
#define kPairVerifyStateM3			3 // Controller -> Accessory  -- Verify Request
#define kPairVerifyStateM4			4 // Accessory  -> Controller -- Verify Response
#define kPairVerifyStateDone		5

//===========================================================================================================================
//	Types
//===========================================================================================================================

struct PairingSessionPrivate
{
	CFRuntimeBase			base;					// CF type info. Must be first.
	LogCategory *			ucat;					// Category to use for all logging.
	PairingDelegate			delegate;				// Delegate to customize behavior.
	PairingSessionType		type;					// Type of pairing operation to perform.
	PairingFlags			flags;					// Flags to control pairing.
	size_t					mtuPayload;				// Max number of payload bytes per fragment.
	size_t					mtuTotal;				// Max number of bytes to output from PairingSessionExchange.
	uint8_t					state;					// Current state of the pairing process.
	uint8_t *				inputFragmentBuf;		// Input fragment being reassembled.
	size_t					inputFragmentLen;		// Number of bytes in inputFragmentBuf.
	uint8_t *				outputFragmentBuf;		// Output fragment being fragmented.
	size_t					outputFragmentLen;		// Number of bytes in outputFragmentBuf.
	size_t					outputFragmentOffset;	// Number of bytes already sent from outputFragmentBuf. 
	Boolean					outputDone;				// True if the output should be marked done on the last fragment.
	char *					activeIdentifierPtr;	// Malloc'd identifier ptr being used for the current exchange.
	size_t					activeIdentifierLen;	// Number of bytes in activeIdentifierPtr.
	char *					identifierPtr;			// Malloc'd identifier ptr set manually.
	size_t					identifierLen;			// Number of bytes in identifierPtr.
	uint8_t *				peerIdentifierPtr;		// Malloc'd peer identifier ptr.
	size_t					peerIdentifierLen;		// Number of bytes in peerIdentifierPtr.
	char *					setupCodePtr;			// Malloc'd setup code ptr.
	size_t					setupCodeLen;			// Number of bytes in setupCodePtr.
	Boolean					setupCodeFailed;		// True if we tried a setup code and it failed.
	Boolean					showingSetupCode;		// True if we're currently showing a setup code to the user.
	uint8_t					key[ 32 ];				// Key for encrypting/decrypting data.
	uint8_t					ourCurvePK[ 32 ];		// Our Curve25519 public key.
	uint8_t					ourCurveSK[ 32 ];		// Our Curve25519 secret key.
	uint8_t					ourEdPK[ 32 ];			// Our Ed25519 public key.
	uint8_t					ourEdSK[ 32 ];			// Our Ed25519 secret key.
	uint8_t					peerCurvePK[ 32 ];		// Peer's Curve25519 public key.
	uint8_t					peerEdPK[ 32 ];			// Peer's Ed25519 public key.
	uint8_t					sharedSecret[ 32 ];		// Curve25519 shared secret.
	
	// Pair-setup.
	
	SRPRef					srpCtx;					// SRP context for pair setup.
	uint8_t *				srpPKPtr;				// Malloc'd SRP public key from the peer.
	size_t					srpPKLen;				// Number of bytes in srpPKPtr.
	uint8_t *				srpSaltPtr;				// Malloc'd SRP salt from the peer.
	size_t					srpSaltLen;				// Number of bytes in srpSaltPtr.
	uint8_t *				srpSharedSecretPtr;		// SRP-derived shared secret.
	size_t					srpSharedSecretLen;		// Number of bytes in srpSharedSecretPtr.
	uint8_t					requestMethod;			// Method client requested. See kTLVMethod_*.
	
#if( PAIRING_KEYCHAIN )
	// Keychain support.
	
	CFStringRef				keychainAccessGroup;	// Keychain group used for identity and peer items.
	CFStringRef				keychainIdentityLabel;	// Prefix for the label of the identity Keychain item.
	uint32_t				keychainIdentityType;	// Type used for programmatic searches of pairing Keychain identities.
	CFStringRef				keychainIdentityDesc;	// Describes how the Keychain identity item is used.
	CFStringRef				keychainPeerLabel;		// Prefix for the label of peer Keychain items.
	uint32_t				keychainPeerType;		// Type used for programmatic searches of pairing Keychain peers.
	CFStringRef				keychainPeerDesc;		// Describes how Keychain peer items are used.
	PairingKeychainFlags	keychainFlags;			// Flags for controlling Keychain operations.
#endif
};

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static void	_PairingSessionGetTypeID( void *inContext );
static void	_PairingSessionFinalize( CFTypeRef inCF );
static void	_PairingSessionReset( PairingSessionRef inSession );

static OSStatus
	_ProgressInput( 
		PairingSessionRef	inSession, 
		const uint8_t **	ioInputPtr, 
		size_t *			ioInputLen, 
		uint8_t **			outInputStorage, 
		uint8_t **			outOutputPtr, 
		size_t *			outOutputLen, 
		Boolean *			outDone );
static OSStatus
	_ProgressOutput( 
		PairingSessionRef	me, 
		uint8_t *			inOutputPtr, 
		size_t				inOutputLen, 
		uint8_t **			outOutputPtr, 
		size_t *			outOutputLen, 
		Boolean *			ioDone );
static OSStatus
	_SetupClientExchange( 
		PairingSessionRef	inSession, 
		const void *		inInputPtr, 
		size_t				inInputLen, 
		uint8_t **			outOutputPtr, 
		size_t *			outOutputLen, 
		Boolean *			outDone );
static OSStatus
	_SetupServerExchange( 
		PairingSessionRef	inSession, 
		const void *		inInputPtr, 
		size_t				inInputLen, 
		uint8_t **			outOutputPtr, 
		size_t *			outOutputLen, 
		Boolean *			outDone );
static OSStatus
	_VerifyPairingClientExchange( 
		PairingSessionRef	inSession, 
		const void *		inInputPtr, 
		size_t				inInputLen, 
		uint8_t **			outOutputPtr, 
		size_t *			outOutputLen, 
		Boolean *			outDone );
static OSStatus
	_VerifyPairingServerExchange( 
		PairingSessionRef	inSession, 
		const void *		inInputPtr, 
		size_t				inInputLen, 
		uint8_t **			outOutputPtr, 
		size_t *			outOutputLen, 
		Boolean *			outDone );

static int32_t	_PairingThrottle( void );
static void		_PairingResetThrottle( void );

#if( PAIRING_KEYCHAIN )
	static OSStatus	_PairingSessionDeleteIdentity( PairingSessionRef inSession );
	static OSStatus
		_PairingSessionGetOrCreateIdentityKeychain( 
			PairingSessionRef	inSession, 
			Boolean				inAllowCreate, 
			char **				outIdentifier, 
			uint8_t *			outPK, 
			uint8_t *			outSK );
	static OSStatus
		_PairingSessionCopyIdentityKeychain( 
			PairingSessionRef	inSession, 
			char **				outIdentifier, 
			uint8_t *			outPK, 
			uint8_t *			outSK );
	static OSStatus
		_PairingSessionCreateIdentityKeychain( 
			PairingSessionRef	inSession, 
			char **				outIdentifier, 
			uint8_t *			outPK, 
			uint8_t *			outSK );
	static CFArrayRef
		_PairingSessionCopyPeers( 
			PairingSessionRef	inSession, 
			const void *		inIdentifierPtr, 
			size_t				inIdentifierLen, 
			OSStatus *			outErr );
	static OSStatus
		_PairingSessionDeletePeer( 
			PairingSessionRef	inSession, 
			const void *		inIdentifierPtr, 
			size_t				inIdentifierLen );
	static OSStatus
		_PairingSessionFindPeerKeychain( 
			PairingSessionRef	inSession, 
			const void *		inIdentifierPtr, 
			size_t				inIdentifierLen, 
			uint8_t *			outPK );
	static OSStatus
		_PairingSessionSavePeerKeychain( 
			PairingSessionRef	inSession, 
			const void *		inIdentifierPtr, 
			size_t				inIdentifierLen, 
			const uint8_t *		inPK );
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static dispatch_once_t			gPairingSessionInitOnce = 0;
static CFTypeID					gPairingSessionTypeID   = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kPairingSessionClass = 
{
	0,							// version
	"PairingSession",			// className
	NULL,						// init
	NULL,						// copy
	_PairingSessionFinalize,	// finalize
	NULL,						// equal -- NULL means pointer equality.
	NULL,						// hash  -- NULL means pointer hash.
	NULL,						// copyFormattingDesc
	NULL,						// copyDebugDesc
	NULL,						// reclaim
	NULL						// refcount
};

static pthread_mutex_t		gPairingGlobalLock			= PTHREAD_MUTEX_INITIALIZER;
static uint64_t				gPairingThrottleStartTicks	= 0;
static uint32_t				gPairingThrottleCounter		= 0;
static uint32_t				gPairingMaxTries			= 0;
static uint32_t				gPairingTries				= 0;

ulog_define( Pairing, kLogLevelNotice, kLogFlags_Default, "Pairing", NULL );
#define pair_ucat()								&log_category_from_name( Pairing )
#define pair_dlog( SESSION, LEVEL, ... )		dlogc( (SESSION)->ucat, (LEVEL), __VA_ARGS__ )
#define pair_ulog( SESSION, LEVEL, ... )		ulog( (SESSION)->ucat, (LEVEL), __VA_ARGS__ )

//===========================================================================================================================
//	PairingSessionGetTypeID
//===========================================================================================================================

CFTypeID	PairingSessionGetTypeID( void )
{
	dispatch_once_f( &gPairingSessionInitOnce, NULL, _PairingSessionGetTypeID );
	return( gPairingSessionTypeID );
}

static void _PairingSessionGetTypeID( void *inContext )
{
	(void) inContext;
	
	gPairingSessionTypeID = _CFRuntimeRegisterClass( &kPairingSessionClass );
	check( gPairingSessionTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	PairingSessionCreate
//===========================================================================================================================

OSStatus	PairingSessionCreate( PairingSessionRef *outSession, const PairingDelegate *inDelegate, PairingSessionType inType )
{
	OSStatus				err;
	PairingSessionRef		me;
	size_t					extraLen;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (PairingSessionRef) _CFRuntimeCreateInstance( NULL, PairingSessionGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->ucat		= pair_ucat();
	me->type		= inType;
	me->mtuPayload	= SIZE_MAX - 2;
	me->mtuTotal	= SIZE_MAX;
	if( inDelegate )	me->delegate = *inDelegate;
	else				PairingDelegateInit( &me->delegate );
	
#if( PAIRING_KEYCHAIN )	
	// Set up default Keychain info.
	
	PairingSessionSetKeychainInfo( me, CFSTR( "com.apple.pairing" ), 
		kPairingKeychainTypeCode_Identity,	CFSTR( "Pairing Identity" ),	CFSTR( "Pairing Identity" ), 
		kPairingKeychainTypeCode_Peer,		CFSTR( "Paired Peer" ),			CFSTR( "Paired Peer" ), 
		kPairingKeychainFlags_None );
#endif
	
	*outSession = me;
	me = NULL;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_PairingSessionFinalize
//===========================================================================================================================

static void	_PairingSessionFinalize( CFTypeRef inCF )
{
	PairingSessionRef const		me = (PairingSessionRef) inCF;
	
	if( me->showingSetupCode )
	{
		if( me->delegate.hideSetupCode_f ) me->delegate.hideSetupCode_f( me->delegate.context );
		me->showingSetupCode = false;
	}
	_PairingSessionReset( me );
	ForgetPtrLen( &me->identifierPtr, &me->identifierLen );
	ForgetPtrLen( &me->peerIdentifierPtr, &me->peerIdentifierLen );
	ForgetPtrLen( &me->setupCodePtr, &me->setupCodeLen );
#if( PAIRING_KEYCHAIN )
	ForgetCF( &me->keychainAccessGroup );
	ForgetCF( &me->keychainIdentityLabel );
	ForgetCF( &me->keychainIdentityDesc );
	ForgetCF( &me->keychainPeerLabel );
	ForgetCF( &me->keychainPeerDesc );
#endif
	MemZeroSecure( ( (uint8_t *) me ) + sizeof( me->base ), sizeof( *me ) - sizeof( me->base ) );
}

//===========================================================================================================================
//	_PairingSessionReset
//===========================================================================================================================

static void	_PairingSessionReset( PairingSessionRef me )
{
	me->state = kPairingStateInvalid;
	ForgetPtrLen( &me->inputFragmentBuf, &me->inputFragmentLen );
	ForgetPtrLen( &me->outputFragmentBuf, &me->outputFragmentLen );
	me->outputFragmentOffset = 0;
	me->outputDone = false;
	ForgetPtrLen( &me->activeIdentifierPtr, &me->activeIdentifierLen );
	MemZeroSecure( me->key, sizeof( me->key ) );
	SRPForget( &me->srpCtx );
	ForgetPtrLen( &me->srpPKPtr, &me->srpPKLen );
	ForgetPtrLen( &me->srpSaltPtr, &me->srpSaltLen );
	ForgetPtrLenSecure( &me->srpSharedSecretPtr, &me->srpSharedSecretLen );
	MemZeroSecure( me->ourCurveSK, sizeof( me->ourCurveSK ) );
	MemZeroSecure( me->ourEdSK, sizeof( me->ourEdSK ) );
	MemZeroSecure( me->sharedSecret, sizeof( me->sharedSecret ) );
}

//===========================================================================================================================
//	PairingSessionSetFlags
//===========================================================================================================================

void	PairingSessionSetFlags( PairingSessionRef me, PairingFlags inFlags )
{
	me->flags = inFlags;
}

//===========================================================================================================================
//	PairingSessionSetIdentifier
//===========================================================================================================================

OSStatus	PairingSessionSetIdentifier( PairingSessionRef me, const void *inPtr, size_t inLen )
{
	return( ReplaceString( &me->identifierPtr, &me->identifierLen, inPtr, inLen ) );
}

#if( PAIRING_KEYCHAIN )
//===========================================================================================================================
//	PairingSessionSetKeychainInfo
//===========================================================================================================================

void
	PairingSessionSetKeychainInfo( 
		PairingSessionRef		me, 
		CFStringRef				inAccessGroup, 
		uint32_t				inIdentityType, 
		CFStringRef				inIdentityLabel, 
		CFStringRef				inIdentityDesc, 
		uint32_t				inPeerType, 
		CFStringRef				inPeerLabel, 
		CFStringRef				inPeerDesc, 
		PairingKeychainFlags	inFlags )
{
	if( inAccessGroup )		ReplaceCF( &me->keychainAccessGroup, inAccessGroup );
	if( inIdentityType )	me->keychainIdentityType = inIdentityType;
	if( inIdentityLabel )	ReplaceCF( &me->keychainIdentityLabel, inIdentityLabel );
	if( inIdentityDesc )	ReplaceCF( &me->keychainIdentityDesc, inIdentityDesc );
	if( inPeerType )		me->keychainPeerType = inPeerType;
	if( inPeerLabel )		ReplaceCF( &me->keychainPeerLabel, inPeerLabel );
	if( inPeerDesc )		ReplaceCF( &me->keychainPeerDesc, inPeerDesc );
	me->keychainFlags = inFlags;
}
#endif

//===========================================================================================================================
//	PairingSessionSetLogging
//===========================================================================================================================

void	PairingSessionSetLogging( PairingSessionRef me, LogCategory *inLogCategory )
{
	me->ucat = inLogCategory;
}

//===========================================================================================================================
//	PairingSessionSetMaxTries
//===========================================================================================================================

void	PairingSessionSetMaxTries( PairingSessionRef me, int inMaxTries )
{
	(void) me;
	
	gPairingMaxTries = (uint32_t) inMaxTries;
}

//===========================================================================================================================
//	PairingSessionSetMTU
//===========================================================================================================================

OSStatus	PairingSessionSetMTU( PairingSessionRef me, size_t inMTU )
{
	OSStatus		err;
	size_t			len;
	
	len = TLV8MaxPayloadBytesForTotalBytes( inMTU );
	require_action( len > 0, exit, err = kSizeErr );
	
	me->mtuPayload = len;
	me->mtuTotal   = inMTU;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	PairingSessionCopyPeerIdentifier
//===========================================================================================================================

char *	PairingSessionCopyPeerIdentifier( PairingSessionRef me, size_t *outLen, OSStatus *outErr )
{
	char *			ptr = NULL;
	size_t			len = 0;
	OSStatus		err;
	
	require_action_quiet( me->peerIdentifierPtr, exit, err = kNotFoundErr );
	ptr = strndup( (const char *) me->peerIdentifierPtr, me->peerIdentifierLen );
	require_action( ptr, exit, err = kNoMemoryErr );
	len = me->peerIdentifierLen;
	err = kNoErr;
	
exit:
	if( outLen ) *outLen = len;
	if( outErr ) *outErr = err;
	return( ptr );
}

//===========================================================================================================================
//	PairingSessionSetSetupCode
//===========================================================================================================================

OSStatus	PairingSessionSetSetupCode( PairingSessionRef me, const void *inPtr, size_t inLen )
{
	return( ReplaceString( &me->setupCodePtr, &me->setupCodeLen, inPtr, inLen ) );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	PairingSessionDeriveKey
//===========================================================================================================================

OSStatus
	PairingSessionDeriveKey( 
		PairingSessionRef	me, 
		const void *		inSaltPtr, 
		size_t				inSaltLen, 
		const void *		inInfoPtr, 
		size_t				inInfoLen, 
		size_t				inKeyLen, 
		uint8_t *			outKey )
{
	OSStatus		err;
	
	require_action( 
		( ( me->type == kPairingSessionType_VerifyClient ) || 
		  ( me->type == kPairingSessionType_VerifyServer ) ) &&
		( me->state == kPairVerifyStateDone ), exit, err = kStateErr );
	
	HKDF_SHA512( me->sharedSecret, sizeof( me->sharedSecret ), inSaltPtr, inSaltLen, inInfoPtr, inInfoLen, inKeyLen, outKey );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	PairingSessionExchange
//===========================================================================================================================

OSStatus
	PairingSessionExchange( 
		PairingSessionRef	me, 
		const void *		inInputPtr, 
		size_t				inInputLen, 
		uint8_t **			outOutputPtr, 
		size_t *			outOutputLen, 
		Boolean *			outDone )
{
	const uint8_t *		inputPtr		= (const uint8_t *) inInputPtr;
	uint8_t *			inputStorage	= NULL;
	uint8_t *			outputPtr		= NULL;
	size_t				outputLen		= 0;
	OSStatus			err;
	
	err = _ProgressInput( me, &inputPtr, &inInputLen, &inputStorage, &outputPtr, &outputLen, outDone );
	require_noerr_quiet( err, exit );
	if( outputPtr )
	{
		*outOutputPtr = outputPtr;
		*outOutputLen = outputLen;
		outputPtr = NULL;
		goto exit;
	}
	
	switch( me->type )
	{
		case kPairingSessionType_SetupClient:
			err = _SetupClientExchange( me, inputPtr, inInputLen, &outputPtr, &outputLen, outDone );
			break;
		
		case kPairingSessionType_SetupServer:
			err = _SetupServerExchange( me, inputPtr, inInputLen, &outputPtr, &outputLen, outDone );
			break;
		
		case kPairingSessionType_VerifyClient:
			err = _VerifyPairingClientExchange( me, inputPtr, inInputLen, &outputPtr, &outputLen, outDone );
			break;
		
		case kPairingSessionType_VerifyServer:
			err = _VerifyPairingServerExchange( me, inputPtr, inInputLen, &outputPtr, &outputLen, outDone );
			break;
		
		default:
			pair_ulog( me, kLogLevelWarning, "### Bad pair type: %d\n", me->type );
			err = kStateErr;
			break;
	}
	require_noerr_quiet( err, exit );
	
	err = _ProgressOutput( me, outputPtr, outputLen, outOutputPtr, outOutputLen, outDone );
	require_noerr_quiet( err, exit );
	outputPtr = NULL;
	
exit:
	FreeNullSafe( inputStorage );
	FreeNullSafe( outputPtr );
	if( err && ( err != kAsyncNoErr ) ) _PairingSessionReset( me );
	return( err );
}

//===========================================================================================================================
//	_ProgressInput
//===========================================================================================================================

static OSStatus
	_ProgressInput( 
		PairingSessionRef	me, 
		const uint8_t **	ioInputPtr, 
		size_t *			ioInputLen, 
		uint8_t **			outInputStorage, 
		uint8_t **			outOutputPtr, 
		size_t *			outOutputLen, 
		Boolean *			outDone )
{
	const uint8_t * const		inputPtr		= *ioInputPtr;
	const uint8_t * const		inputEnd		= inputPtr + *ioInputLen;
	uint8_t *					inputStorage	= NULL;
	uint8_t *					outputPtr		= NULL;
	size_t						outputLen		= 0;
	uint8_t *					storage			= NULL;
	Boolean						done			= false;
	OSStatus					err;
	const uint8_t *				ptr;
	uint8_t *					tmp;
	size_t						len, newLen;
	Boolean						last = false;
	TLV8Buffer					tlv;
	
	TLV8BufferInit( &tlv, kMaxTLVSize );
	
	// Look for fragment data. If it's not found then it's not fragmented so exit and pass the data through unmodified.
	
	err = TLV8GetOrCopyCoalesced( inputPtr, inputEnd, kTLVType_FragmentData, &ptr, &len, &storage, NULL );
	if( err == kNotFoundErr )
	{
		err = TLV8GetOrCopyCoalesced( inputPtr, inputEnd, kTLVType_FragmentLast, &ptr, &len, &storage, NULL );
		last = true;
	}
	require_action_quiet( err != kNotFoundErr, exit, err = kNoErr );
	require_noerr_quiet( err, exit );
	
	// If it's 0-byte fragment then it's an ack to a previously sent fragment so send our next output fragment.
	
	if( len == 0 )
	{
		len = me->outputFragmentLen - me->outputFragmentOffset;
		len = Min( len, me->mtuPayload );
		require_action( len > 0, exit, err = kInternalErr );
		
		last = ( ( me->outputFragmentOffset + len ) == me->outputFragmentLen );
		err = TLV8BufferAppend( &tlv, last ? kTLVType_FragmentLast : kTLVType_FragmentData, 
			&me->outputFragmentBuf[ me->outputFragmentOffset ], len );
		require_noerr( err, exit );
		err = TLV8BufferDetach( &tlv, &outputPtr, &outputLen );
		require_noerr( err, exit );
		check( outputLen <= me->mtuTotal );
		
		if( last )
		{
			done = me->outputDone;
			free( me->outputFragmentBuf );
			me->outputFragmentBuf		= NULL;
			me->outputFragmentLen		= 0;
			me->outputFragmentOffset	= 0;
			me->outputDone				= false;
		}
		else
		{
			me->outputFragmentOffset += len;
		}
		goto exit;
	}
	
	// Append the new fragment.
	
	newLen = me->inputFragmentLen + len;
	require_action_quiet( newLen > me->inputFragmentLen, exit, err = kOverrunErr ); // Detect wrapping.
	require_action_quiet( newLen <= kMaxTLVSize, exit, err = kSizeErr );
	tmp = (uint8_t *) realloc( me->inputFragmentBuf, newLen );
	require_action( tmp, exit, err = kNoMemoryErr );
	memcpy( &tmp[ me->inputFragmentLen ], ptr, len );
	ForgetMem( &storage );
	
	if( last )
	{
		// It's the last fragment so return the reassembled data as a single input chunk.
		
		*ioInputPtr				= tmp;
		*ioInputLen				= newLen;
		inputStorage			= tmp;
		me->inputFragmentBuf	= NULL;
		me->inputFragmentLen	= 0;
	}
	else
	{
		// Save the partially reassembled data and output an empty fragment to indicate an ack.
		
		me->inputFragmentBuf = tmp;
		me->inputFragmentLen = newLen;
		
		err = TLV8BufferAppend( &tlv, kTLVType_FragmentData, NULL, 0 );
		require_noerr( err, exit );
		err = TLV8BufferDetach( &tlv, &outputPtr, &outputLen );
		require_noerr( err, exit );
		check( outputLen <= me->mtuTotal );
	}
	
exit:
	TLV8BufferFree( &tlv );
	ForgetMem( &storage );
	*outInputStorage	= inputStorage;
	*outOutputPtr		= outputPtr;
	*outOutputLen		= outputLen;
	*outDone			= done;
	return( err );
}

//===========================================================================================================================
//	_ProgressOutput
//===========================================================================================================================

static OSStatus
	_ProgressOutput( 
		PairingSessionRef	me, 
		uint8_t *			inOutputPtr, 
		size_t				inOutputLen, 
		uint8_t **			outOutputPtr, 
		size_t *			outOutputLen, 
		Boolean *			ioDone )
{
	OSStatus		err;
	TLV8Buffer		tlv;
	
	TLV8BufferInit( &tlv, kMaxTLVSize );
	
	// If the doesn't need to be fragmented then return it unmodified.
	
	if( inOutputLen <= me->mtuTotal )
	{
		*outOutputPtr = inOutputPtr;
		*outOutputLen = inOutputLen;
		err = kNoErr;
		goto exit;
	}
	
	// Save off the complete buffer for sending in fragments.
	
	require_action( !me->outputFragmentBuf && ( me->outputFragmentLen == 0 ), exit, err = kExecutionStateErr );
	me->outputFragmentBuf		= inOutputPtr;
	me->outputFragmentLen		= inOutputLen;
	me->outputFragmentOffset	= me->mtuPayload;
	me->outputDone				= *ioDone;
	
	// Return an MTU-sized fragment.
	
	err = TLV8BufferAppend( &tlv, kTLVType_FragmentData, inOutputPtr, me->mtuPayload );
	require_noerr( err, exit );
	err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
	require_noerr( err, exit );
	check( *outOutputLen <= me->mtuTotal );
	*ioDone = false;
	
exit:
	TLV8BufferFree( &tlv );
	return( err );
}

//===========================================================================================================================
//	_SetupClientExchange
//===========================================================================================================================

static OSStatus
	_SetupClientExchange( 
		PairingSessionRef	me, 
		const void *		inInputPtr, 
		size_t				inInputLen, 
		uint8_t **			outOutputPtr, 
		size_t *			outOutputLen, 
		Boolean *			outDone )
{
	const uint8_t * const		inputPtr	= (const uint8_t *) inInputPtr;
	const uint8_t * const		inputEnd	= inputPtr + inInputLen;
	Boolean						done		= false;
	OSStatus					err;
	TLV8Buffer					tlv, etlv;
	const uint8_t *				ptr;
	size_t						len;
	uint8_t *					storage		= NULL;
	uint8_t *					eptr		= NULL;
	size_t						elen;
	uint8_t *					eend;
	uint8_t *					clientPKPtr	= NULL;
	size_t						clientPKLen	= 0;
	uint8_t *					responsePtr	= NULL;
	size_t						responseLen	= 0;
	uint8_t						sig[ 64 ];
#if( PAIRING_MFI_CLIENT )
	const uint8_t *				sigPtr;
	size_t						sigLen;
	uint8_t *					sigStorage	= NULL;
	uint8_t						msg[ 32 ];
#endif
	PairingFlags				flags;
	uint64_t					u64;
	
	TLV8BufferInit( &tlv, kMaxTLVSize );
	TLV8BufferInit( &etlv, kMaxTLVSize );
	if( me->state == kPairingStateInvalid ) me->state = kPairSetupStateM1;
	
	if( inInputLen > 0 )
	{
		err = TLV8Get( inputPtr, inputEnd, kTLVType_State, &ptr, &len, NULL );
		require_noerr( err, exit );
		require_action( len == 1, exit, err = kSizeErr );
		require_action( *ptr == me->state, exit, err = kStateErr );
	}
	
	switch( me->state )
	{
		// M1: Controller -> Accessory -- Start Request.
		
		case kPairSetupStateM1:
			require_action( inInputLen == 0, exit, err = kParamErr );
			
			if( 0 ) {}
			#if( PAIRING_MFI_CLIENT )
			else if( me->flags & kPairingFlag_MFi )	u64 = kTLVMethod_MFiPairSetup;
			#else
			else if( me->flags & kPairingFlag_MFi )	{ dlogassert( "No MFi support" ); err = kUnsupportedErr; goto exit; }
			#endif
			else									u64 = kTLVMethod_PairSetup;
			err = TLV8BufferAppendUInt64( &tlv, kTLVType_Method, u64 );
			require_noerr( err, exit );
			err = TLV8BufferAppend( &tlv, kTLVType_State, &me->state, sizeof( me->state ) );
			require_noerr( err, exit );
			err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
			require_noerr( err, exit );
			
			me->state = kPairSetupStateM2;
			
			pair_ulog( me, kLogLevelTrace, "Pair-setup client M1 -- start request\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, *outOutputPtr, (int) *outOutputLen );
			break;
		
		// M2: Accessory -> Controller -- Start Response.
		
		case kPairSetupStateM2:
			pair_ulog( me, kLogLevelTrace, "Pair-setup client M2 -- start response\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, inInputPtr, (int) inInputLen );
			
			err = TLV8Get( inputPtr, inputEnd, kTLVType_Error, &ptr, &len, NULL );
			if( !err )
			{
				// If the accessory returned a back-off error then tell the delegate to retry at the specified time.
				
				require_action( len == 1, exit, err = kSizeErr );
				if( *ptr == kTLVError_Backoff )
				{
					u64 = TLV8GetUInt64( inputPtr, inputEnd, kTLVType_RetryDelay, &err, NULL );
					require_noerr( err, exit );
					require_action( u64 <= INT32_MAX, exit, err = kRangeErr );
					
					_PairingSessionReset( me );
					
					require_action( me->delegate.promptForSetupCode_f, exit, err = kNotPreparedErr );
					err = me->delegate.promptForSetupCode_f( kPairingFlag_Throttle, (int32_t) u64, me->delegate.context );
					require_noerr_quiet( err, exit );
					err = kAsyncNoErr;
					goto exit;
				}
				err = PairingStatusToOSStatus( *ptr );
				pair_ulog( me, kLogLevelWarning, "### Pair-setup client M2 bad status: 0x%X, %#m\n", *ptr, err );
				goto exit;
			}
			
			SRPForget( &me->srpCtx );
			err = SRPCreate( &me->srpCtx );
			require_noerr( err, exit );
			
			ForgetPtrLen( &me->srpSaltPtr, &me->srpSaltLen );
			me->srpSaltPtr = TLV8CopyCoalesced( inputPtr, inputEnd, kTLVType_Salt, &me->srpSaltLen, NULL, &err );
			require_noerr( err, exit );
			require_action( me->srpSaltLen >= 16, exit, err = kSizeErr );
			
			ForgetPtrLen( &me->srpPKPtr, &me->srpPKLen );
			me->srpPKPtr = TLV8CopyCoalesced( inputPtr, inputEnd, kTLVType_PublicKey, &me->srpPKLen, NULL, &err );
			require_noerr( err, exit );
			require_action( me->srpPKLen > 0, exit, err = kSizeErr );
			
			me->state = kPairSetupStateM3;
			
			// Ask the delegate to prompt the user and call us again after it sets it.
			
			if( !me->setupCodePtr || ( me->setupCodeLen == 0 ) )
			{
				flags = me->flags;
				if( me->setupCodeFailed ) flags |= kPairingFlag_Incorrect;
				require_action( me->delegate.promptForSetupCode_f, exit, err = kNotPreparedErr );
				err = me->delegate.promptForSetupCode_f( flags, -1, me->delegate.context );
				require_noerr_quiet( err, exit );
				require_action_quiet( me->setupCodePtr && ( me->setupCodeLen > 0 ), exit, err = kAsyncNoErr );
			}
			inInputLen = 0;
			
			// Fall through since we already have the setup code.
		
		// M3: Controller -> Accessory -- Verify Request.
		
		case kPairSetupStateM3:
			require_action( inInputLen == 0, 		exit, err = kParamErr );
			require_action( me->setupCodePtr,		exit, err = kNotPreparedErr );
			require_action( me->setupCodeLen > 0,	exit, err = kNotPreparedErr );
			require_action( me->srpCtx,				exit, err = kExecutionStateErr );
			require_action( me->srpPKPtr,			exit, err = kExecutionStateErr );
			require_action( me->srpPKLen > 0,		exit, err = kExecutionStateErr );
			require_action( me->srpSaltPtr,			exit, err = kExecutionStateErr );
			require_action( me->srpSaltLen > 0,		exit, err = kExecutionStateErr );
			
			ForgetPtrLen( &me->srpSharedSecretPtr, &me->srpSharedSecretLen );
			err = SRPClientStart( me->srpCtx, kSRPParameters_3072_SHA512, kPairSetupSRPUsernamePtr, kPairSetupSRPUsernameLen, 
				me->setupCodePtr, me->setupCodeLen, me->srpSaltPtr, me->srpSaltLen, me->srpPKPtr, me->srpPKLen, 
				&clientPKPtr, &clientPKLen, &me->srpSharedSecretPtr, &me->srpSharedSecretLen, &responsePtr, &responseLen );
			require_noerr( err, exit );
			ForgetPtrLen( &me->srpPKPtr, &me->srpPKLen );
			ForgetPtrLen( &me->srpSaltPtr, &me->srpSaltLen );
			
			err = TLV8BufferAppend( &tlv, kTLVType_State, &me->state, sizeof( me->state ) );
			require_noerr( err, exit );
			err = TLV8BufferAppend( &tlv, kTLVType_PublicKey, clientPKPtr, clientPKLen );
			require_noerr( err, exit );
			err = TLV8BufferAppend( &tlv, kTLVType_Proof, responsePtr, responseLen );
			require_noerr( err, exit );
			err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
			require_noerr( err, exit );
			
			me->state = kPairSetupStateM4;
			
			pair_ulog( me, kLogLevelTrace, "Pair-setup  client M3 -- verify request\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, *outOutputPtr, (int) *outOutputLen );
			break;
		
		// M4: Accessory -> Controller -- Verify Response.
		
		case kPairSetupStateM4:
			require_action( me->srpCtx,					exit, err = kExecutionStateErr );
			require_action( me->srpSharedSecretPtr,		exit, err = kExecutionStateErr );
			require_action( me->srpSharedSecretLen > 0,	exit, err = kExecutionStateErr );
			
			pair_ulog( me, kLogLevelTrace, "Pair-setup client M4 -- verify response\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, inInputPtr, (int) inInputLen );
			
			err = TLV8Get( inputPtr, inputEnd, kTLVType_Error, &ptr, &len, NULL );
			if( !err )
			{
				// If the accessory said the setup code was incorrect then tell the delegate so it can tell the user.
				
				require_action( len == 1, exit, err = kSizeErr );
				if( *ptr == kTLVError_Authentication )
				{
					pair_ulog( me, kLogLevelNotice, "### Pair-setup client bad setup code\n" );
					_PairingSessionReset( me );
					ForgetPtrLen( &me->setupCodePtr, &me->setupCodeLen );
					me->setupCodeFailed = true;
					
					require_action( me->delegate.promptForSetupCode_f, exit, err = kNotPreparedErr );
					err = me->delegate.promptForSetupCode_f( me->flags | kPairingFlag_Incorrect, -1, me->delegate.context );
					require_noerr_quiet( err, exit );
					err = kAsyncNoErr;
					goto exit;
				}
				err = PairingStatusToOSStatus( *ptr );
				pair_ulog( me, kLogLevelWarning, "### Pair-setup client M4 bad status: 0x%X, %#m\n", *ptr, err );
				goto exit;
			}
			
			err = TLV8GetOrCopyCoalesced( inputPtr, inputEnd, kTLVType_Proof, &ptr, &len, &storage, NULL );
			require_noerr( err, exit );
			err = SRPClientVerify( me->srpCtx, ptr, len );
			ForgetMem( &storage );
			require_noerr_action_quiet( err, exit, err = kAuthenticationErr );
			SRPForget( &me->srpCtx );
			
			HKDF_SHA512( me->srpSharedSecretPtr, me->srpSharedSecretLen, 
				kPairSetupEncryptSaltPtr, kPairSetupEncryptSaltLen, 
				kPairSetupEncryptInfoPtr, kPairSetupEncryptInfoLen, 
				sizeof( me->key ), me->key );
			
			#if( PAIRING_MFI_CLIENT )
				if( me->flags & kPairingFlag_MFi )
				{
					// Verify and decrypt sub-TLV.
					
					eptr = TLV8CopyCoalesced( inputPtr, inputEnd, kTLVType_EncryptedData, &elen, NULL, &err );
					require_noerr( err, exit );
					require_action( elen > 16, exit, err = kSizeErr );
					elen -= 16;
					eend = eptr + elen;
					err = chacha20_poly1305_decrypt_all_64x64( me->key, (const uint8_t *) "PS-Msg04", NULL, 0, 
						eptr, elen, eptr, eend );
					require_noerr( err, exit );
					
					// Verify the MFi auth IC signature of a challenge derived from the SRP shared secret.
					
					err = TLV8GetOrCopyCoalesced( eptr, eend, kTLVType_Signature, &sigPtr, &sigLen, &sigStorage, NULL );
					require_noerr( err, exit );
					err = TLV8GetOrCopyCoalesced( eptr, eend, kTLVType_Certificate, &ptr, &len, &storage, NULL );
					require_noerr( err, exit );
					
					check_compile_time_code( sizeof( msg ) >= 32 );
					HKDF_SHA512( me->srpSharedSecretPtr, me->srpSharedSecretLen, 
						kMFiPairSetupSaltPtr, kMFiPairSetupSaltLen, 
						kMFiPairSetupInfoPtr, kMFiPairSetupInfoLen, 
						32, msg );
					err = MFiPlatform_VerifySignature( msg, 32, sigPtr, sigLen, ptr, len );
					require_noerr( err, exit );
					ForgetMem( &eptr );
					ForgetMem( &sigStorage );
					ForgetMem( &storage );
				}
			#endif
			
			me->state = kPairSetupStateM5;
			
			// M5: Controller -> Accessory -- Exchange Request.
			
			ForgetPtrLen( &me->activeIdentifierPtr, &me->activeIdentifierLen );
			err = PairingSessionCopyIdentity( me, true, &me->activeIdentifierPtr, me->ourEdPK, me->ourEdSK );
			require_noerr_quiet( err, exit );
			me->activeIdentifierLen = strlen( me->activeIdentifierPtr );
			require_action( me->activeIdentifierLen > 0, exit, err = kIDErr );
			
			// Generate signature of controller's info.
			
			len = 32 + me->activeIdentifierLen + 32;
			storage = (uint8_t *) malloc( len );
			require_action( storage, exit, err = kNoMemoryErr );
			HKDF_SHA512( me->srpSharedSecretPtr, me->srpSharedSecretLen, 
				kPairSetupControllerSignSaltPtr, kPairSetupControllerSignSaltLen, 
				kPairSetupControllerSignInfoPtr, kPairSetupControllerSignInfoLen, 
				32, &storage[ 0 ] );
			memcpy( &storage[ 32 ], me->activeIdentifierPtr, me->activeIdentifierLen );
			memcpy( &storage[ 32 + me->activeIdentifierLen ], me->ourEdPK, 32 );
			Ed25519_sign( sig, storage, len, me->ourEdPK, me->ourEdSK );
			ForgetMem( &storage );
			
			// Build sub-TLV of controller's info and encrypt it.
			
			err = TLV8BufferAppend( &etlv, kTLVType_Identifier, me->activeIdentifierPtr, me->activeIdentifierLen );
			require_noerr( err, exit );
			err = TLV8BufferAppend( &etlv, kTLVType_PublicKey, me->ourEdPK, 32 );
			require_noerr( err, exit );
			err = TLV8BufferAppend( &etlv, kTLVType_Signature, sig, 64 );
			require_noerr( err, exit );
			
			storage = (uint8_t *) malloc( etlv.len + 16 );
			require_action( storage, exit, err = kNoMemoryErr );
			chacha20_poly1305_encrypt_all_64x64( me->key, (const uint8_t *) "PS-Msg05", NULL, 0, 
				etlv.ptr, etlv.len, storage, &storage[ etlv.len ] );
			err = TLV8BufferAppend( &tlv, kTLVType_EncryptedData, storage, etlv.len + 16 );
			require_noerr( err, exit );
			ForgetMem( &storage );
			
			err = TLV8BufferAppend( &tlv, kTLVType_State, &me->state, sizeof( me->state ) );
			require_noerr( err, exit );
			err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
			require_noerr( err, exit );
			
			me->state = kPairSetupStateM6;
			
			pair_ulog( me, kLogLevelTrace, "Pair-setup client M5 -- exchange request\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, *outOutputPtr, (int) *outOutputLen );
			break;
		
		// M6: Accessory -> Controller -- Exchange Response.
		
		case kPairSetupStateM6:
			require_action( me->srpSharedSecretPtr,		exit, err = kExecutionStateErr );
			require_action( me->srpSharedSecretLen > 0,	exit, err = kExecutionStateErr );
			
			pair_ulog( me, kLogLevelTrace, "Pair-setup client M6 -- exchange response\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, inInputPtr, (int) inInputLen );
			
			err = TLV8Get( inputPtr, inputEnd, kTLVType_Error, &ptr, &len, NULL );
			if( !err )
			{
				require_action( len == 1, exit, err = kSizeErr );
				err = PairingStatusToOSStatus( *ptr );
				pair_ulog( me, kLogLevelWarning, "### Pair-setup client M6 bad status: 0x%X, %#m\n", *ptr, err );
				goto exit;
			}
			
			// Verify and decrypt sub-TLV.
			
			eptr = TLV8CopyCoalesced( inputPtr, inputEnd, kTLVType_EncryptedData, &elen, NULL, &err );
			require_noerr( err, exit );
			require_action( elen > 16, exit, err = kSizeErr );
			elen -= 16;
			eend = eptr + elen;
			err = chacha20_poly1305_decrypt_all_64x64( me->key, (const uint8_t *) "PS-Msg06", NULL, 0, eptr, elen, eptr, eend );
			require_noerr( err, exit );
			
			// Verify signature of accessory's info.
			
			ForgetPtrLen( &me->peerIdentifierPtr, &me->peerIdentifierLen );
			me->peerIdentifierPtr = TLV8CopyCoalesced( eptr, eend, kTLVType_Identifier, &me->peerIdentifierLen, NULL, &err );
			require_noerr( err, exit );
			require_action( me->peerIdentifierLen > 0, exit, err = kSizeErr );
			
			err = TLV8GetBytes( eptr, eend, kTLVType_PublicKey, 32, 32, me->peerEdPK, NULL, NULL );
			require_noerr( err, exit );
			
			err = TLV8GetBytes( eptr, eend, kTLVType_Signature, 64, 64, sig, NULL, NULL );
			require_noerr( err, exit );
			
			len = 32 + me->peerIdentifierLen + 32;
			storage = (uint8_t *) malloc( len );
			require_action( storage, exit, err = kNoMemoryErr );
			HKDF_SHA512( me->srpSharedSecretPtr, me->srpSharedSecretLen, 
				kPairSetupAccessorySignSaltPtr, kPairSetupAccessorySignSaltLen, 
				kPairSetupAccessorySignInfoPtr, kPairSetupAccessorySignInfoLen, 
				32, &storage[ 0 ] );
			memcpy( &storage[ 32 ], me->peerIdentifierPtr, me->peerIdentifierLen );
			memcpy( &storage[ 32 + me->peerIdentifierLen ], me->peerEdPK, 32 );
			
			err = Ed25519_verify( storage, len, sig, me->peerEdPK );
			require_noerr_action_quiet( err, exit, err = kAuthenticationErr );
			ForgetMem( &storage );
			ForgetMem( &eptr );
			
			err = PairingSessionSavePeer( me, me->peerIdentifierPtr, me->peerIdentifierLen, me->peerEdPK );
			require_noerr_quiet( err, exit );
			
			_PairingSessionReset( me );
			ForgetPtrLen( &me->setupCodePtr, &me->setupCodeLen );
			me->setupCodeFailed = false;
			
			me->state = kPairSetupStateDone;
			*outOutputPtr = NULL;
			*outOutputLen = 0;
			done = true;
			pair_ulog( me, kLogLevelTrace, "Pair-setup client done -- server authenticated\n" );
			break;
		
		default:
			pair_ulog( me, kLogLevelWarning, "### Pair-setup client bad state: %d\n", me->state );
			err = kStateErr;
			goto exit;
	}
	err = kNoErr;
	
exit:
	*outDone = done;
	TLV8BufferFree( &tlv );
	TLV8BufferFree( &etlv );
	ForgetMem( &storage );
	ForgetMem( &eptr );
	ForgetPtrLen( &clientPKPtr, &clientPKLen );
	ForgetPtrLen( &responsePtr, &responseLen );
#if( PAIRING_MFI_CLIENT )
	ForgetMem( &sigStorage );
#endif
	if( err && ( err != kAsyncNoErr ) )
	{
		pair_ulog( me, kLogLevelWarning, "### Pair-setup client state %d failed: %#m\n%?{end}%1{tlv8}\n", 
			me->state, err, !log_category_enabled( me->ucat, kLogLevelInfo ), kTLVDescriptors, inInputPtr, (int) inInputLen );
	}
	return( err );
}

//===========================================================================================================================
//	_SetupServerExchange
//===========================================================================================================================

static OSStatus
	_SetupServerExchange( 
		PairingSessionRef	me, 
		const void *		inInputPtr, 
		size_t				inInputLen, 
		uint8_t **			outOutputPtr, 
		size_t *			outOutputLen, 
		Boolean *			outDone )
{
	const uint8_t * const		inputPtr		= (const uint8_t *) inInputPtr;
	const uint8_t * const		inputEnd		= inputPtr + inInputLen;
	Boolean						done     		= false;
	OSStatus					err;
	TLV8Buffer					tlv, etlv;
	const uint8_t *				ptr;
	size_t						len;
	uint8_t *					storage			= NULL;
	uint8_t *					eptr			= NULL;
	size_t						elen;
	uint8_t *					eend;
	uint8_t *					serverPKPtr		= NULL;
	size_t						serverPKLen		= 0;
	const uint8_t *				clientPKPtr;
	size_t						clientPKLen;
	uint8_t *					clientPKStorage	= NULL;
	const uint8_t *				proofPtr;
	size_t						proofLen;
	uint8_t *					proofStorage	= NULL;
	uint8_t *					responsePtr		= NULL;
	size_t						responseLen		= 0;
	char						tempStr[ 64 ];
	uint8_t						salt[ 16 ];
	uint8_t						sig[ 64 ];
#if( PAIRING_MFI_SERVER )
	uint8_t						msg[ 32 ];
#endif
	uint8_t						u8;
	int32_t						s32;
#if( PAIRING_MFI_SERVER )
	uint8_t						digest[ 20 ];
#endif
	
	TLV8BufferInit( &tlv, kMaxTLVSize );
	TLV8BufferInit( &etlv, kMaxTLVSize );
	
	err = TLV8Get( inputPtr, inputEnd, kTLVType_State, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( len == 1, exit, err = kSizeErr );
	if( *ptr == kPairSetupStateM1 ) _PairingSessionReset( me );
	if( me->state == kPairingStateInvalid ) me->state = kPairSetupStateM1;
	require_action( *ptr == me->state, exit, err = kStateErr );
	
	switch( me->state )
	{
		// M1: Controller -> Accessory -- Start Request.
		
		case kPairSetupStateM1:
			pair_ulog( me, kLogLevelTrace, "Pair-setup server M1 -- start request\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, inInputPtr, (int) inInputLen );
			
			err = TLV8GetBytes( inputPtr, inputEnd, kTLVType_Method, 1, 1, &u8, NULL, NULL );
			require_noerr( err, exit );
			if(      u8 == kTLVMethod_PairSetup ) {}
			#if( PAIRING_MFI_SERVER )
			else if( u8 == kTLVMethod_MFiPairSetup ) {}
			#endif
			else
			{
				pair_ulog( me, kLogLevelWarning, "### Pair-setup server unsupported method: %u\n", u8 );
				err = kUnsupportedErr;
				goto exit;
			}
			me->requestMethod = u8;
			
			s32 = _PairingThrottle();
			if( s32 >= 0 )
			{
				pair_ulog( me, kLogLevelWarning, "### Pair-setup server throttling for %d second(s)\n", s32 );
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_Error, kTLVError_Backoff );
				require_noerr( err, exit );
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_RetryDelay, (uint32_t) s32 );
				require_noerr( err, exit );
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_State, kPairSetupStateM2 );
				require_noerr( err, exit );
				err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
				require_noerr( err, exit );
				goto exit;
			}
			else if( s32 == kCountErr )
			{
				pair_ulog( me, kLogLevelWarning, "### Pair-setup server disabled after too many attempts\n" );
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_Error, kTLVError_MaxTries );
				require_noerr( err, exit );
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_State, kPairSetupStateM2 );
				require_noerr( err, exit );
				err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
				require_noerr( err, exit );
				goto exit;
			}
			
			me->state = kPairSetupStateM2;
			
			// M2: Accessory -> Controller -- Start Response.
			
			if( !me->setupCodePtr || ( me->setupCodeLen == 0 ) )
			{
				require_action( me->delegate.showSetupCode_f, exit, err = kNotPreparedErr );
				err = me->delegate.showSetupCode_f( kPairingFlags_None, tempStr, sizeof( tempStr ), me->delegate.context );
				require_noerr_quiet( err, exit );
				me->showingSetupCode = true;
				require_action( strlen( tempStr ) >= 4, exit, err = kSizeErr );
				
				err = PairingSessionSetSetupCode( me, tempStr, kSizeCString );
				require_noerr( err, exit );
			}
			
			SRPForget( &me->srpCtx );
			err = SRPCreate( &me->srpCtx );
			require_noerr( err, exit );
			
			err = SRPServerStart( me->srpCtx, kSRPParameters_3072_SHA512, kPairSetupSRPUsernamePtr, kPairSetupSRPUsernameLen, 
				me->setupCodePtr, me->setupCodeLen, sizeof( salt ), salt, &serverPKPtr, &serverPKLen );
			require_noerr( err, exit );
			
			err = TLV8BufferAppend( &tlv, kTLVType_State, &me->state, sizeof( me->state ) );
			require_noerr( err, exit );
			err = TLV8BufferAppend( &tlv, kTLVType_Salt, salt, sizeof( salt ) );
			require_noerr( err, exit );
			err = TLV8BufferAppend( &tlv, kTLVType_PublicKey, serverPKPtr, serverPKLen );
			require_noerr( err, exit );
			err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
			require_noerr( err, exit );
			
			me->state = kPairSetupStateM3;
			
			pair_ulog( me, kLogLevelTrace, "Pair-setup server M2 -- start response\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, *outOutputPtr, *outOutputLen );
			break;
		
		// M3: Controller -> Accessory -- Verify Request.
		
		case kPairSetupStateM3:
			require_action( me->srpCtx, exit, err = kExecutionStateErr );
			
			pair_ulog( me, kLogLevelTrace, "Pair-setup server M3 -- verify request\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, inInputPtr, (int) inInputLen );
			
			err = TLV8GetOrCopyCoalesced( inputPtr, inputEnd, kTLVType_PublicKey, &clientPKPtr, &clientPKLen, 
				&clientPKStorage, NULL );
			require_noerr( err, exit );
			
			err = TLV8GetOrCopyCoalesced( inputPtr, inputEnd, kTLVType_Proof, &proofPtr, &proofLen, &proofStorage, NULL );
			require_noerr( err, exit );
			
			ForgetPtrLenSecure( &me->srpSharedSecretPtr, &me->srpSharedSecretLen );
			err = SRPServerVerify( me->srpCtx, clientPKPtr, clientPKLen, proofPtr, proofLen, 
				&me->srpSharedSecretPtr, &me->srpSharedSecretLen, &responsePtr, &responseLen );
			if( err )
			{
				pair_ulog( me, kLogLevelNotice, "### Pair-setup server bad setup code\n" );
				
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_Error, kTLVError_Authentication );
				require_noerr( err, exit );
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_State, kPairSetupStateM4 );
				require_noerr( err, exit );
				err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
				require_noerr( err, exit );
				
				_PairingSessionReset( me );
				goto exit;
			}
			
			if( me->delegate.hideSetupCode_f ) me->delegate.hideSetupCode_f( me->delegate.context );
			me->showingSetupCode = false;
			ForgetPtrLen( &me->setupCodePtr, &me->setupCodeLen );
			
			me->state = kPairSetupStateM4;
			
			// M4: Accessory -> Controller -- Verify Response.
			
			err = TLV8BufferAppend( &tlv, kTLVType_State, &me->state, sizeof( me->state ) );
			require_noerr( err, exit );
			err = TLV8BufferAppend( &tlv, kTLVType_Proof, responsePtr, responseLen );
			require_noerr( err, exit );
			SRPForget( &me->srpCtx );
			
			HKDF_SHA512( me->srpSharedSecretPtr, me->srpSharedSecretLen, 
				kPairSetupEncryptSaltPtr, kPairSetupEncryptSaltLen, 
				kPairSetupEncryptInfoPtr, kPairSetupEncryptInfoLen, 
				sizeof( me->key ), me->key );
			
			#if( PAIRING_MFI_SERVER )
				if( me->requestMethod == kTLVMethod_MFiPairSetup )
				{
					// Use the MFi auth IC to sign a challenge derived from the SRP shared secret.
					
					check_compile_time_code( sizeof( msg ) >= 32 );
					HKDF_SHA512( me->srpSharedSecretPtr, me->srpSharedSecretLen, 
						kMFiPairSetupSaltPtr, kMFiPairSetupSaltLen, 
						kMFiPairSetupInfoPtr, kMFiPairSetupInfoLen, 
						32, msg );
					SHA1( msg, 32, digest );
					err = MFiPlatform_CreateSignature( digest, sizeof( digest ), &storage, &len );
					require_noerr( err, exit );
					err = TLV8BufferAppend( &etlv, kTLVType_Signature, storage, len );
					require_noerr( err, exit );
					ForgetMem( &storage );
					
					err = MFiPlatform_CopyCertificate( &storage, &len );
					require_noerr( err, exit );
					err = TLV8BufferAppend( &etlv, kTLVType_Certificate, storage, len );
					require_noerr( err, exit );
					ForgetMem( &storage );
					
					storage = (uint8_t *) malloc( etlv.len + 16 );
					require_action( storage, exit, err = kNoMemoryErr );
					chacha20_poly1305_encrypt_all_64x64( me->key, (const uint8_t *) "PS-Msg04", NULL, 0, 
						etlv.ptr, etlv.len, storage, &storage[ etlv.len ] );
					err = TLV8BufferAppend( &tlv, kTLVType_EncryptedData, storage, etlv.len + 16 );
					require_noerr( err, exit );
					ForgetMem( &storage );
				}
			#endif
			
			err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
			require_noerr( err, exit );
			
			me->state = kPairSetupStateM5;
			
			pair_ulog( me, kLogLevelTrace, "Pair-setup server M4 -- verify response\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, *outOutputPtr, *outOutputLen );
			break;
		
		// M5: Controller -> Accessory -- Exchange Request.
		
		case kPairSetupStateM5:
			require_action( me->srpSharedSecretPtr,		exit, err = kExecutionStateErr );
			require_action( me->srpSharedSecretLen > 0,	exit, err = kExecutionStateErr );
			
			pair_ulog( me, kLogLevelTrace, "Pair-setup server M5 -- exchange request\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, inInputPtr, (int) inInputLen );
			
			// Verify and decrypt sub-TLV.
			
			eptr = TLV8CopyCoalesced( inputPtr, inputEnd, kTLVType_EncryptedData, &elen, NULL, &err );
			require_noerr( err, exit );
			require_action( elen > 16, exit, err = kSizeErr );
			elen -= 16;
			eend = eptr + elen;
			err = chacha20_poly1305_decrypt_all_64x64( me->key, (const uint8_t *) "PS-Msg05", NULL, 0, eptr, elen, eptr, eend );
			require_noerr( err, exit );
			
			// Verify signature of controller's info.
			
			ForgetPtrLen( &me->peerIdentifierPtr, &me->peerIdentifierLen );
			me->peerIdentifierPtr = TLV8CopyCoalesced( eptr, eend, kTLVType_Identifier, &me->peerIdentifierLen, NULL, &err );
			require_noerr( err, exit );
			require_action( me->peerIdentifierLen > 0, exit, err = kSizeErr );
			
			err = TLV8GetBytes( eptr, eend, kTLVType_PublicKey, 32, 32, me->peerEdPK, NULL, NULL );
			require_noerr( err, exit );
			
			err = TLV8GetBytes( eptr, eend, kTLVType_Signature, 64, 64, sig, NULL, NULL );
			require_noerr( err, exit );
			
			len = 32 + me->peerIdentifierLen + 32;
			storage = (uint8_t *) malloc( len );
			require_action( storage, exit, err = kNoMemoryErr );
			HKDF_SHA512( me->srpSharedSecretPtr, me->srpSharedSecretLen, 
				kPairSetupControllerSignSaltPtr, kPairSetupControllerSignSaltLen, 
				kPairSetupControllerSignInfoPtr, kPairSetupControllerSignInfoLen, 
				32, &storage[ 0 ] );
			memcpy( &storage[ 32 ], me->peerIdentifierPtr, me->peerIdentifierLen );
			memcpy( &storage[ 32 + me->peerIdentifierLen ], me->peerEdPK, 32 );
			
			err = Ed25519_verify( storage, len, sig, me->peerEdPK );
			if( err )
			{
				pair_ulog( me, kLogLevelWarning, "### Pair-setup server bad signature: %#m\n", err );
				
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_Error, kTLVError_Authentication );
				require_noerr( err, exit );
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_State, kPairSetupStateM6 );
				require_noerr( err, exit );
				err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
				require_noerr( err, exit );
				
				_PairingSessionReset( me );
				goto exit;
			}
			ForgetMem( &storage );
			
			err = PairingSessionSavePeer( me, me->peerIdentifierPtr, me->peerIdentifierLen, me->peerEdPK );
			if( err )
			{
				pair_ulog( me, kLogLevelWarning, "### Pair-setup server save peer failed: %#m\n", err );
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_Error, OSStatusToPairingStatus( err ) );
				require_noerr( err, exit );
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_State, kPairSetupStateM6 );
				require_noerr( err, exit );
				err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
				require_noerr( err, exit );
				goto exit;
			}
			
			me->state = kPairSetupStateM6;
			
			// M6: Accessory -> Controller -- Exchange Response.
			
			ForgetPtrLen( &me->activeIdentifierPtr, &me->activeIdentifierLen );
			err = PairingSessionCopyIdentity( me, true, &me->activeIdentifierPtr, me->ourEdPK, me->ourEdSK );
			require_noerr_quiet( err, exit );
			me->activeIdentifierLen = strlen( me->activeIdentifierPtr );
			require_action( me->activeIdentifierLen > 0, exit, err = kIDErr );
			
			// Generate signature of accessory's info.
			
			len = 32 + me->activeIdentifierLen + 32;
			storage = (uint8_t *) malloc( len );
			require_action( storage, exit, err = kNoMemoryErr );
			HKDF_SHA512( me->srpSharedSecretPtr, me->srpSharedSecretLen, 
				kPairSetupAccessorySignSaltPtr, kPairSetupAccessorySignSaltLen, 
				kPairSetupAccessorySignInfoPtr, kPairSetupAccessorySignInfoLen, 
				32, &storage[ 0 ] );
			memcpy( &storage[ 32 ], me->activeIdentifierPtr, me->activeIdentifierLen );
			memcpy( &storage[ 32 + me->activeIdentifierLen ], me->ourEdPK, 32 );
			Ed25519_sign( sig, storage, len, me->ourEdPK, me->ourEdSK );
			ForgetMem( &storage );
			
			// Build sub-TLV of accessory's info and encrypt it.
			
			err = TLV8BufferAppend( &etlv, kTLVType_Identifier, me->activeIdentifierPtr, me->activeIdentifierLen );
			require_noerr( err, exit );
			err = TLV8BufferAppend( &etlv, kTLVType_PublicKey, me->ourEdPK, 32 );
			require_noerr( err, exit );
			err = TLV8BufferAppend( &etlv, kTLVType_Signature, sig, 64 );
			require_noerr( err, exit );
			
			storage = (uint8_t *) malloc( etlv.len + 16 );
			require_action( storage, exit, err = kNoMemoryErr );
			chacha20_poly1305_encrypt_all_64x64( me->key, (const uint8_t *) "PS-Msg06", NULL, 0, 
				etlv.ptr, etlv.len, storage, &storage[ etlv.len ] );
			err = TLV8BufferAppend( &tlv, kTLVType_EncryptedData, storage, etlv.len + 16 );
			require_noerr( err, exit );
			ForgetMem( &storage );
			
			err = TLV8BufferAppend( &tlv, kTLVType_State, &me->state, sizeof( me->state ) );
			require_noerr( err, exit );
			err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
			require_noerr( err, exit );
			
			me->state = kPairSetupStateDone;
			done = true;
			_PairingSessionReset( me );
			_PairingResetThrottle();
			
			pair_ulog( me, kLogLevelTrace, "Pair-setup server M6 -- exchange response\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, *outOutputPtr, *outOutputLen );
			pair_ulog( me, kLogLevelTrace, "Pair-setup server done -- client authenticated\n" );
			break;
		
		default:
			pair_ulog( me, kLogLevelWarning, "### Pair-setup server bad state: %d\n", me->state );
			err = kStateErr;
			goto exit;
	}
	err = kNoErr;
	
exit:
	*outDone = done;
	TLV8BufferFree( &tlv );
	TLV8BufferFree( &etlv );
	ForgetMem( &eptr );
	ForgetMem( &storage );
	ForgetPtrLen( &serverPKPtr, &serverPKLen );
	ForgetMem( &clientPKStorage );
	ForgetMem( &proofStorage );
	ForgetPtrLen( &responsePtr, &responseLen );
	if( err && ( err != kAsyncNoErr ) )
	{
		pair_ulog( me, kLogLevelWarning, "### Pair-setup server state %d failed: %#m\n%?{end}%1{tlv8}\n", 
			me->state, err, !log_category_enabled( me->ucat, kLogLevelInfo ), kTLVDescriptors, inInputPtr, (int) inInputLen );	
	}
	return( err );
}

//===========================================================================================================================
//	_VerifyPairingClientExchange
//===========================================================================================================================

static OSStatus
	_VerifyPairingClientExchange( 
		PairingSessionRef	me, 
		const void *		inInputPtr, 
		size_t				inInputLen, 
		uint8_t **			outOutputPtr, 
		size_t *			outOutputLen, 
		Boolean *			outDone )
{
	const uint8_t * const		inputPtr = (const uint8_t *) inInputPtr;
	const uint8_t * const		inputEnd = inputPtr + inInputLen;
	Boolean						done     = false;
	OSStatus					err;
	TLV8Buffer					tlv, etlv;
	const uint8_t *				ptr;
	size_t						len;
	uint8_t *					storage = NULL;
	uint8_t *					eptr	= NULL;
	size_t						elen;
	uint8_t *					eend;
	uint8_t						sig[ 64 ];
	
	TLV8BufferInit( &tlv, kMaxTLVSize );
	TLV8BufferInit( &etlv, kMaxTLVSize );
	if( me->state == kPairingStateInvalid ) me->state = kPairVerifyStateM1;
	
	if( inInputLen > 0 )
	{
		err = TLV8Get( inputPtr, inputEnd, kTLVType_State, &ptr, &len, NULL );
		require_noerr( err, exit );
		require_action( len == 1, exit, err = kSizeErr );
		require_action( *ptr == me->state, exit, err = kStateErr );
	}
	
	switch( me->state )
	{
		// M1: Controller -> Accessory -- Start Request.
		
		case kPairVerifyStateM1:
			require_action( inInputLen == 0, exit, err = kParamErr );
			
			// Generate new, random ECDH key pair.
			
			err = RandomBytes( me->ourCurveSK, sizeof( me->ourCurveSK ) );
			require_noerr( err, exit );
			HKDF_SHA512( me->ourCurveSK, sizeof( me->ourCurveSK ), 
				kPairVerifyECDHSaltPtr, kPairVerifyECDHSaltLen, 
				kPairVerifyECDHInfoPtr, kPairVerifyECDHInfoLen, 
				sizeof( me->ourCurveSK ), me->ourCurveSK );
			curve25519( me->ourCurvePK, me->ourCurveSK, NULL );
			
			err = TLV8BufferAppend( &tlv, kTLVType_State, &me->state, sizeof( me->state ) );
			require_noerr( err, exit );
			err = TLV8BufferAppend( &tlv, kTLVType_PublicKey, me->ourCurvePK, sizeof( me->ourCurvePK ) );
			require_noerr( err, exit );
			err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
			require_noerr( err, exit );
			
			pair_ulog( me, kLogLevelTrace, "Pair-verify client M1 -- start request\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, *outOutputPtr, (int) *outOutputLen );
			me->state = kPairVerifyStateM2;
			break;
		
		// M2: Accessory -> Controller -- Start Response.
		
		case kPairVerifyStateM2:
			pair_ulog( me, kLogLevelTrace, "Pair-verify client M2 -- start response\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, inInputPtr, (int) inInputLen );
			
			// Generate shared secret and derive encryption key.
			
			err = TLV8GetBytes( inputPtr, inputEnd, kTLVType_PublicKey, 32, 32, me->peerCurvePK, NULL, NULL );
			require_noerr( err, exit );
			curve25519( me->sharedSecret, me->ourCurveSK, me->peerCurvePK );
			
			HKDF_SHA512( me->sharedSecret, sizeof( me->sharedSecret ), 
				kPairVerifyEncryptSaltPtr, kPairVerifyEncryptSaltLen, 
				kPairVerifyEncryptInfoPtr, kPairVerifyEncryptInfoLen, 
				sizeof( me->key ), me->key );
			
			// Verify and decrypt sub-TLV.
			
			eptr = TLV8CopyCoalesced( inputPtr, inputEnd, kTLVType_EncryptedData, &elen, NULL, &err );
			require_noerr( err, exit );
			require_action( elen > 16, exit, err = kSizeErr );
			elen -= 16;
			eend = eptr + elen;
			err = chacha20_poly1305_decrypt_all_64x64( me->key, (const uint8_t *) "PV-Msg02", NULL, 0, eptr, elen, eptr, eend );
			require_noerr( err, exit );
			
			// Look up accessory's LTPK.
			
			ForgetPtrLen( &me->peerIdentifierPtr, &me->peerIdentifierLen );
			me->peerIdentifierPtr = TLV8CopyCoalesced( eptr, eend, kTLVType_Identifier, &me->peerIdentifierLen, NULL, &err );
			require_noerr( err, exit );
			require_action( me->peerIdentifierLen > 0, exit, err = kSizeErr );
			
			err = PairingSessionFindPeer( me, me->peerIdentifierPtr, me->peerIdentifierLen, me->peerEdPK );
			require_noerr_quiet( err, exit );
			ForgetMem( &storage );
			
			// Verify signature of accessory's info.
			
			err = TLV8GetBytes( eptr, eend, kTLVType_Signature, 64, 64, sig, NULL, NULL );
			require_noerr( err, exit );
			
			len = 32 + me->peerIdentifierLen + 32;
			storage = (uint8_t *) malloc( len );
			require_action( storage, exit, err = kNoMemoryErr );
			memcpy( &storage[  0 ], me->peerCurvePK, 32 );
			memcpy( &storage[ 32 ], me->peerIdentifierPtr, me->peerIdentifierLen );
			memcpy( &storage[ 32 + me->peerIdentifierLen ], me->ourCurvePK, 32 );
			err = Ed25519_verify( storage, len, sig, me->peerEdPK );
			require_noerr_action_quiet( err, exit, err = kAuthenticationErr );
			ForgetMem( &storage );
			
			me->state = kPairVerifyStateM3;
			
			// M3: Controller -> Accessory -- Finish Request.
			
			// Generate signature of controller's info.
			
			ForgetPtrLen( &me->activeIdentifierPtr, &me->activeIdentifierLen );
			err = PairingSessionCopyIdentity( me, false, &me->activeIdentifierPtr, me->ourEdPK, me->ourEdSK );
			require_noerr_quiet( err, exit );
			me->activeIdentifierLen = strlen( me->activeIdentifierPtr );
			require_action( me->activeIdentifierLen > 0, exit, err = kIDErr );
			
			len = 32 + me->activeIdentifierLen + 32;
			storage = (uint8_t *) malloc( len );
			require_action( storage, exit, err = kNoMemoryErr );
			memcpy( &storage[  0 ], me->ourCurvePK, 32 );
			memcpy( &storage[ 32 ], me->activeIdentifierPtr, me->activeIdentifierLen );
			memcpy( &storage[ 32 + me->activeIdentifierLen ], me->peerCurvePK, 32 );
			Ed25519_sign( sig, storage, len, me->ourEdPK, me->ourEdSK );
			ForgetMem( &storage );
			
			// Build sub-TLV of controller's info and encrypt it.
			
			err = TLV8BufferAppend( &etlv, kTLVType_Identifier, me->activeIdentifierPtr, me->activeIdentifierLen );
			require_noerr( err, exit );
			err = TLV8BufferAppend( &etlv, kTLVType_Signature, sig, 64 );
			require_noerr( err, exit );
			
			storage = (uint8_t *) malloc( etlv.len + 16 );
			require_action( storage, exit, err = kNoMemoryErr );
			chacha20_poly1305_encrypt_all_64x64( me->key, (const uint8_t *) "PV-Msg03", NULL, 0, 
				etlv.ptr, etlv.len, storage, &storage[ etlv.len ] );
			err = TLV8BufferAppend( &tlv, kTLVType_EncryptedData, storage, etlv.len + 16 );
			require_noerr( err, exit );
			ForgetMem( &storage );
			
			err = TLV8BufferAppend( &tlv, kTLVType_State, &me->state, sizeof( me->state ) );
			require_noerr( err, exit );
			err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
			require_noerr( err, exit );
			
			pair_ulog( me, kLogLevelTrace, "Pair-verify client M3 -- finish request\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, *outOutputPtr, (int) *outOutputLen );
			me->state = kPairVerifyStateM4;
			break;
		
		// M4: Accessory -> Controller -- Finish Response.
		
		case kPairVerifyStateM4:
			pair_ulog( me, kLogLevelTrace, "Pair-verify client M4 -- finish response\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, inInputPtr, (int) inInputLen );
			
			err = TLV8Get( inputPtr, inputEnd, kTLVType_Error, &ptr, &len, NULL );
			if( !err )
			{
				require_action( len == 1, exit, err = kSizeErr );
				err = PairingStatusToOSStatus( *ptr );
				require_noerr_quiet( err, exit );
			}
			
			*outOutputPtr = NULL;
			*outOutputLen = 0;
			me->state = kPairVerifyStateDone;
			done = true;
			pair_ulog( me, kLogLevelTrace, "Pair-verify client done\n" );
			break;
		
		default:
			pair_ulog( me, kLogLevelWarning, "### Pair-verify client bad state: %d\n", me->state );
			err = kStateErr;
			goto exit;
	}
	err = kNoErr;
	
exit:
	*outDone = done;
	TLV8BufferFree( &tlv );
	TLV8BufferFree( &etlv );
	ForgetMem( &storage );
	ForgetMem( &eptr );
	if( err )
	{
		pair_ulog( me, kLogLevelWarning, "### Pair-verify client state %d failed: %#m\n%?{end}%1{tlv8}\n", 
			me->state, err, !log_category_enabled( me->ucat, kLogLevelInfo ), kTLVDescriptors, inInputPtr, (int) inInputLen );
	}
	return( err );
}

//===========================================================================================================================
//	_VerifyPairingServerExchange
//===========================================================================================================================

static OSStatus
	_VerifyPairingServerExchange( 
		PairingSessionRef	me, 
		const void *		inInputPtr, 
		size_t				inInputLen, 
		uint8_t **			outOutputPtr, 
		size_t *			outOutputLen, 
		Boolean *			outDone )
{
	const uint8_t * const		inputPtr = (const uint8_t *) inInputPtr;
	const uint8_t * const		inputEnd = inputPtr + inInputLen;
	Boolean						done     = false;
	OSStatus					err;
	TLV8Buffer					tlv, etlv;
	const uint8_t *				ptr;
	size_t						len;
	uint8_t *					storage = NULL;
	uint8_t *					eptr	= NULL;
	size_t						elen;
	uint8_t *					eend;
	uint8_t						sig[ 64 ];
	
	TLV8BufferInit( &tlv, kMaxTLVSize );
	TLV8BufferInit( &etlv, kMaxTLVSize );
	
	err = TLV8Get( inputPtr, inputEnd, kTLVType_State, &ptr, &len, NULL );
	require_noerr( err, exit );
	require_action( len == 1, exit, err = kSizeErr );
	if( *ptr == kPairVerifyStateM1 ) _PairingSessionReset( me );
	if( me->state == kPairingStateInvalid ) me->state = kPairVerifyStateM1;
	require_action( *ptr == me->state, exit, err = kStateErr );
	
	switch( me->state )
	{
		// M1: Controller -> Accessory -- Start Request.
		
		case kPairVerifyStateM1:
			pair_ulog( me, kLogLevelTrace, "Pair-verify server M1 -- start request\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, inInputPtr, (int) inInputLen );
			
			// Generate new, random ECDH key pair.
			
			err = RandomBytes( me->ourCurveSK, sizeof( me->ourCurveSK ) );
			require_noerr( err, exit );
			HKDF_SHA512( me->ourCurveSK, sizeof( me->ourCurveSK ), 
				kPairVerifyECDHSaltPtr, kPairVerifyECDHSaltLen, 
				kPairVerifyECDHInfoPtr, kPairVerifyECDHInfoLen, 
				sizeof( me->ourCurveSK ), me->ourCurveSK );
			curve25519( me->ourCurvePK, me->ourCurveSK, NULL );
			
			// Generate shared secret.
			
			err = TLV8GetBytes( inputPtr, inputEnd, kTLVType_PublicKey, 32, 32, me->peerCurvePK, NULL, NULL );
			require_noerr( err, exit );
			curve25519( me->sharedSecret, me->ourCurveSK, me->peerCurvePK );
			
			// M2: Accessory -> Controller -- Start Response.
			
			// Generate signature of our info.
			
			ForgetPtrLen( &me->activeIdentifierPtr, &me->activeIdentifierLen );
			err = PairingSessionCopyIdentity( me, false, &me->activeIdentifierPtr, me->ourEdPK, me->ourEdSK );
			require_noerr_quiet( err, exit );
			me->activeIdentifierLen = strlen( me->activeIdentifierPtr );
			require_action( me->activeIdentifierLen > 0, exit, err = kIDErr );
			
			len = 32 + me->activeIdentifierLen + 32;
			storage = (uint8_t *) malloc( len );
			require_action( storage, exit, err = kNoMemoryErr );
			memcpy( &storage[  0 ], me->ourCurvePK, 32 );
			memcpy( &storage[ 32 ], me->activeIdentifierPtr, me->activeIdentifierLen );
			memcpy( &storage[ 32 + me->activeIdentifierLen ], me->peerCurvePK, 32 );
			Ed25519_sign( sig, storage, len, me->ourEdPK, me->ourEdSK );
			ForgetMem( &storage );
			
			// Build sub-TLV of accessory's info and encrypt it.
			
			err = TLV8BufferAppend( &etlv, kTLVType_Identifier, me->activeIdentifierPtr, me->activeIdentifierLen );
			require_noerr( err, exit );
			err = TLV8BufferAppend( &etlv, kTLVType_Signature, sig, 64 );
			require_noerr( err, exit );
			
			storage = (uint8_t *) malloc( etlv.len + 16 );
			require_action( storage, exit, err = kNoMemoryErr );
			HKDF_SHA512( me->sharedSecret, sizeof( me->sharedSecret ), 
				kPairVerifyEncryptSaltPtr, kPairVerifyEncryptSaltLen, 
				kPairVerifyEncryptInfoPtr, kPairVerifyEncryptInfoLen, 
				sizeof( me->key ), me->key );
			chacha20_poly1305_encrypt_all_64x64( me->key, (const uint8_t *) "PV-Msg02", NULL, 0, 
				etlv.ptr, etlv.len, storage, &storage[ etlv.len ] );
			err = TLV8BufferAppend( &tlv, kTLVType_EncryptedData, storage, etlv.len + 16 );
			require_noerr( err, exit );
			ForgetMem( &storage );
			
			me->state = kPairVerifyStateM2;
			
			err = TLV8BufferAppend( &tlv, kTLVType_State, &me->state, sizeof( me->state ) );
			require_noerr( err, exit );
			err = TLV8BufferAppend( &tlv, kTLVType_PublicKey, me->ourCurvePK, 32 );
			require_noerr( err, exit );
			err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
			require_noerr( err, exit );
			
			pair_ulog( me, kLogLevelTrace, "Pair-verify server M2 -- start response\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, *outOutputPtr, *outOutputLen );
			me->state = kPairVerifyStateM3;
			break;
		
		// M3: Controller -> Accessory -- Finish Request.
		
		case kPairVerifyStateM3:
			pair_ulog( me, kLogLevelTrace, "Pair-verify server M3 -- finish request\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, inInputPtr, (int) inInputLen );
			
			// Verify and decrypt sub-TLV.
			
			eptr = TLV8CopyCoalesced( inputPtr, inputEnd, kTLVType_EncryptedData, &elen, NULL, &err );
			require_noerr( err, exit );
			require_action( elen > 16, exit, err = kSizeErr );
			elen -= 16;
			eend = eptr + elen;
			err = chacha20_poly1305_decrypt_all_64x64( me->key, (const uint8_t *) "PV-Msg03", NULL, 0, eptr, elen, eptr, eend );
			if( err )
			{
				pair_ulog( me, kLogLevelWarning, "### Pair-verify server bad auth tag\n" );
				
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_Error, kTLVError_Authentication );
				require_noerr( err, exit );
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_State, kPairVerifyStateM4 );
				require_noerr( err, exit );
				err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
				require_noerr( err, exit );
				
				_PairingSessionReset( me );
				goto exit;
			}
			
			// Look up accessory's LTPK.
			
			ForgetPtrLen( &me->peerIdentifierPtr, &me->peerIdentifierLen );
			me->peerIdentifierPtr = TLV8CopyCoalesced( eptr, eend, kTLVType_Identifier, &me->peerIdentifierLen, NULL, &err );
			require_noerr( err, exit );
			require_action( me->peerIdentifierLen > 0, exit, err = kSizeErr );
			
			err = PairingSessionFindPeer( me, me->peerIdentifierPtr, me->peerIdentifierLen, me->peerEdPK );
			if( err )
			{
				pair_ulog( me, kLogLevelWarning, "### Pair-verify server unknown peer: %.*s\n", 
					(int) me->peerIdentifierLen, me->peerIdentifierPtr );
				
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_Error, kTLVError_Authentication );
				require_noerr( err, exit );
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_State, kPairVerifyStateM4 );
				require_noerr( err, exit );
				err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
				require_noerr( err, exit );
				
				_PairingSessionReset( me );
				goto exit;
			}
			ForgetMem( &storage );
			
			// Verify signature of controller's info.
			
			err = TLV8GetBytes( eptr, eend, kTLVType_Signature, 64, 64, sig, NULL, NULL );
			require_noerr( err, exit );
			
			len = 32 + me->peerIdentifierLen + 32;
			storage = (uint8_t *) malloc( len );
			require_action( storage, exit, err = kNoMemoryErr );
			memcpy( &storage[  0 ], me->peerCurvePK, 32 );
			memcpy( &storage[ 32 ], me->peerIdentifierPtr, me->peerIdentifierLen );
			memcpy( &storage[ 32 + me->peerIdentifierLen ], me->ourCurvePK, 32 );
			err = Ed25519_verify( storage, len, sig, me->peerEdPK );
			if( err )
			{
				pair_ulog( me, kLogLevelWarning, "### Pair-verify server bad signature: %#m\n", err );
				
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_Error, kTLVError_Authentication );
				require_noerr( err, exit );
				err = TLV8BufferAppendUInt64( &tlv, kTLVType_State, kPairVerifyStateM4 );
				require_noerr( err, exit );
				err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
				require_noerr( err, exit );
				
				_PairingSessionReset( me );
				goto exit;
			}
			ForgetMem( &storage );
			
			me->state = kPairVerifyStateM4;
			
			// M4: Accessory -> Controller -- Finish Response.
			
			err = TLV8BufferAppend( &tlv, kTLVType_State, &me->state, sizeof( me->state ) );
			require_noerr( err, exit );
			err = TLV8BufferDetach( &tlv, outOutputPtr, outOutputLen );
			require_noerr( err, exit );
			
			pair_ulog( me, kLogLevelTrace, "Pair-verify server M4 -- finish response\n%?{end}%1{tlv8}\n", 
				!log_category_enabled( me->ucat, kLogLevelVerbose ), kTLVDescriptors, *outOutputPtr, *outOutputLen );
			me->state = kPairVerifyStateDone;
			done = true;
			pair_ulog( me, kLogLevelTrace, "Pair-verify server done\n" );
			break;
		
		default:
			pair_ulog( me, kLogLevelWarning, "### Pair-verify server bad state: %d\n", me->state );
			err = kStateErr;
			goto exit;
	}
	err = kNoErr;
	
exit:
	*outDone = done;
	TLV8BufferFree( &tlv );
	TLV8BufferFree( &etlv );
	ForgetMem( &storage );
	ForgetMem( &eptr );
	if( err )
	{
		pair_ulog( me, kLogLevelWarning, "### Pair-verify server state %d failed: %#m\n%?{end}%1{tlv8}\n", 
			me->state, err, !log_category_enabled( me->ucat, kLogLevelInfo ), kTLVDescriptors, inInputPtr, (int) inInputLen );
	}
	return( err );
}

//===========================================================================================================================
//	_PairingThrottle
//===========================================================================================================================

static int32_t	_PairingThrottle( void )
{
	uint64_t		ticksPerSec, nowTicks, nextTicks, deltaTicks, secs;
	int32_t			result;
	
	pthread_mutex_lock( &gPairingGlobalLock );
	
	if( gPairingMaxTries > 0 )
	{
		require_action_quiet( gPairingTries < gPairingMaxTries, exit, result = kCountErr );
		++gPairingTries;
		result = kSkipErr;
		goto exit;
	}
	
	ticksPerSec = UpTicksPerSecond();
	nowTicks = UpTicks();
	if( gPairingThrottleStartTicks == 0 ) gPairingThrottleStartTicks = nowTicks;
	nextTicks = gPairingThrottleStartTicks + ( gPairingThrottleCounter * ticksPerSec );
	if( nextTicks > nowTicks )
	{
		deltaTicks = nextTicks - nowTicks;
		secs = deltaTicks / ticksPerSec;
		if( deltaTicks % ticksPerSec ) ++secs;
		result = (int32_t) secs;
	}
	else
	{
		gPairingThrottleCounter = gPairingThrottleCounter ? ( gPairingThrottleCounter * 2 ) : 1;
		if( gPairingThrottleCounter > 10800 ) gPairingThrottleCounter = 10800; // 3 hour cap.
		result = kSkipErr;
	}
	
exit:
	pthread_mutex_unlock( &gPairingGlobalLock );
	return( result );
}

//===========================================================================================================================
//	_PairingResetThrottle
//===========================================================================================================================

static void	_PairingResetThrottle( void )
{
	pthread_mutex_lock( &gPairingGlobalLock );
		gPairingThrottleStartTicks	= 0;
		gPairingThrottleCounter		= 0;
		gPairingTries				= 0;
	pthread_mutex_unlock( &gPairingGlobalLock );
}

#if 0
#pragma mark -
#endif

#if( PAIRING_KEYCHAIN )
//===========================================================================================================================
//	PairingSessionDeleteIdentity
//===========================================================================================================================

OSStatus	PairingSessionDeleteIdentity( PairingSessionRef me )
{
	OSStatus		err;
	
	pthread_mutex_lock( &gPairingGlobalLock );
	err = _PairingSessionDeleteIdentity( me );
	pthread_mutex_unlock( &gPairingGlobalLock );
	return( err );
}

#if( KEYCHAIN_LITE_ENABLED )
//===========================================================================================================================
//	_PairingSessionDeleteIdentity
//===========================================================================================================================

static OSStatus	_PairingSessionDeleteIdentity( PairingSessionRef me )
{
	return( KeychainDeleteFormatted( 
		"{"
			"%kO=%O"	// class
			"%kO=%i"	// type
			"%kO=%O"	// service (user-visible "where").
			"%kO=%O"	// matchLimit
		"}", 
		kSecClass,			kSecClassGenericPassword, 
		kSecAttrType,		me->keychainIdentityType, 
		kSecAttrService,	me->keychainIdentityLabel, 
		kSecMatchLimit,		kSecMatchLimitAll ) );
}
#else
//===========================================================================================================================
//	_PairingSessionDeleteIdentity
//===========================================================================================================================

static OSStatus	_PairingSessionDeleteIdentity( PairingSessionRef me )
{
	OSStatus			err;
	CFArrayRef			items;
	CFIndex				i, n;
	CFDictionaryRef		attrs;
	uint32_t			type;
	CFDataRef			persistentRef;
	
	// Work around <radar:8782844> (SecItemDelete and kSecMatchLimitAll) by getting all and deleting individually.
	
	items = (CFArrayRef) KeychainCopyMatchingFormatted( NULL, 
		"{"
			"%kO=%O" // class
			#if( !TARGET_IPHONE_SIMULATOR )
			"%kO=%O" // accessGroup
			#endif
			"%kO=%i" // type
			"%kO=%O" // synchronizable
			"%kO=%O" // returnAttributes
			"%kO=%O" // returnRef
			"%kO=%O" // matchLimit
		"}", 
		kSecClass,					kSecClassGenericPassword, 
		#if( !TARGET_IPHONE_SIMULATOR )
		kSecAttrAccessGroup,		me->keychainAccessGroup, 
		#endif
		kSecAttrType,				me->keychainIdentityType, 
		kSecAttrSynchronizable,		kSecAttrSynchronizableAny, 
		kSecReturnAttributes,		kCFBooleanTrue, 
		kSecReturnPersistentRef,	kCFBooleanTrue, 
		kSecMatchLimit,				kSecMatchLimitAll );
	n = items ? CFArrayGetCount( items ) : 0;
	for( i = 0; i < n; ++i )
	{
		attrs = CFArrayGetCFDictionaryAtIndex( items, i, NULL );
		check( attrs );
		if( !attrs ) continue;
		
		type = (uint32_t) CFDictionaryGetInt64( attrs, kSecAttrType, &err );
		check_noerr( err );
		if( type != me->keychainIdentityType ) continue;
		
		persistentRef = CFDictionaryGetCFData( attrs, kSecValuePersistentRef, &err );
		check_noerr( err );
		if( err ) continue;
		
		err = KeychainDeleteItemByPersistentRef( persistentRef, attrs );
		check_noerr( err );
	}
	CFReleaseNullSafe( items );
	return( kNoErr );
}
#endif // KEYCHAIN_LITE_ENABLED
#endif // PAIRING_KEYCHAIN

//===========================================================================================================================
//	PairingSessionCopyIdentity
//===========================================================================================================================

OSStatus
	PairingSessionCopyIdentity( 
		PairingSessionRef	me, 
		Boolean				inAllowCreate, 
		char **				outIdentifier, 
		uint8_t *			outPK, 
		uint8_t *			outSK )
{
	OSStatus		err;
	
	if( me->delegate.copyIdentity_f )
	{
		err = me->delegate.copyIdentity_f( inAllowCreate, outIdentifier, outPK, outSK, me->delegate.context );
		require_noerr_quiet( err, exit );
	}
	else
	{
		#if( PAIRING_KEYCHAIN )
			err = _PairingSessionGetOrCreateIdentityKeychain( me, inAllowCreate, outIdentifier, outPK, outSK );
			require_noerr_quiet( err, exit );
		#else
			dlogassert( "No copyIdentity_f" );
			err = kNotPreparedErr;
			goto exit;
		#endif
	}
	
exit:
	return( err );
}

#if( PAIRING_KEYCHAIN )
//===========================================================================================================================
//	_PairingSessionGetOrCreateIdentityKeychain
//===========================================================================================================================

static OSStatus
	_PairingSessionGetOrCreateIdentityKeychain( 
		PairingSessionRef	me, 
		Boolean				inAllowCreate, 
		char **				outIdentifier, 
		uint8_t *			outPK, 
		uint8_t *			outSK )
{
	OSStatus		err;
	int				tries, maxTries;
	
	pthread_mutex_lock( &gPairingGlobalLock );
	
	// Retry on transient failures since the identity may be created by other processes and may collide with us.
	// This is mainly an issue with debug tools since production code should be properly serialized between processes.
	
	err = kUnknownErr;
	maxTries = 10;
	for( tries = 1; tries <= maxTries; ++tries )
	{
		if( tries != 1 ) usleep( 20000 );
		err = _PairingSessionCopyIdentityKeychain( me, outIdentifier, outPK, outSK );
		if( !err ) goto exit;
		if( err == errSecAuthFailed ) break;
		require_quiet( inAllowCreate, exit );
		
		err = _PairingSessionCreateIdentityKeychain( me, outIdentifier, outPK, outSK );
		if( !err ) goto exit;
		pair_ulog( me, kLogLevelInfo, "### Create %@ failed (try %d of %d): %#m\n", 
			me->keychainIdentityLabel, tries, maxTries, err );
	}
	pair_ulog( me, kLogLevelWarning, "### Failed to create %@ after %d tries: %#m\n", 
		me->keychainIdentityLabel, maxTries, err );
	
exit:
	pthread_mutex_unlock( &gPairingGlobalLock );
	return( err );
}

//===========================================================================================================================
//	_PairingSessionCopyIdentityKeychain
//
//	Assumes global pairing lock is held.
//===========================================================================================================================

static OSStatus	_PairingSessionCopyIdentityKeychain( PairingSessionRef me, char **outIdentifier, uint8_t *outPK, uint8_t *outSK )
{
	OSStatus			err;
	CFDictionaryRef		identity;
	size_t				len;
	char *				identifier;
	char				pkAndSKHex[ 132 ];
	const char *		src;
	const char *		end;
	
	identity = (CFDictionaryRef) KeychainCopyMatchingFormatted( &err, 
		"{"
			"%kO=%O" // class
			#if( !TARGET_IPHONE_SIMULATOR )
			"%kO=%O" // accessGroup
			#endif
			"%kO=%i" // type
			"%kO=%O" // synchronizable
			"%kO=%O" // returnAttributes
			"%kO=%O" // returnData
		"}", 
		kSecClass,				kSecClassGenericPassword, 
		#if( !TARGET_IPHONE_SIMULATOR )
		kSecAttrAccessGroup,	me->keychainAccessGroup, 
		#endif
		kSecAttrType,			me->keychainIdentityType, 
		kSecAttrSynchronizable,	kSecAttrSynchronizableAny, 
		kSecReturnAttributes,	kCFBooleanTrue, 
		kSecReturnData,			( outPK || outSK ) ? kCFBooleanTrue : NULL );
	require_noerr_quiet( err, exit );
	
	if( outIdentifier )
	{
		identifier = CFDictionaryCopyCString( identity, kSecAttrAccount, &err );
		require_noerr( err, exit );
		*outIdentifier = identifier;
	}
	
	// Parse the hex public and secret key in the format: <hex pk> "+" <hex sk>.
	
	if( outPK || outSK )
	{
		len = 0;
		CFDictionaryGetData( identity, kSecValueData, pkAndSKHex, sizeof( pkAndSKHex ), &len, &err );
		src = pkAndSKHex;
		end = src + len;
		err = HexToData( src, len, kHexToData_DefaultFlags, outPK, 32, &len, NULL, &src );
		require_noerr( err, exit );
		require_action( len == 32, exit, err = kSizeErr );
		
		if( outSK )
		{
			require_action( ( src < end ) && ( *src == '+' ), exit, err = kMalformedErr );
			++src;
			err = HexToData( src, (size_t)( end - src ), kHexToData_DefaultFlags, outSK, 32, &len, NULL, NULL );
			require_noerr( err, exit );
			require_action( len == 32, exit, err = kSizeErr );
		}
	}
	
exit:
	CFReleaseNullSafe( identity );
	MemZeroSecure( pkAndSKHex, sizeof( pkAndSKHex ) );
	return( err );
}

//===========================================================================================================================
//	_PairingSessionCreateIdentityKeychain
//
//	Assumes global pairing lock is held.
//===========================================================================================================================

static OSStatus	_PairingSessionCreateIdentityKeychain( PairingSessionRef me, char **outIdentifier, uint8_t *outPK, uint8_t *outSK )
{
	OSStatus			err;
	uint8_t				uuid[ 16 ];
	const char *		identifierPtr;
	size_t				identifierLen;
	char				identifierBuf[ 64 ];
	uint8_t				pk[ 32 ];
	uint8_t				sk[ 32 ];
	char				pkAndSKHex[ 132 ];
	char *				label;
	char *				identifier;
	
	// Delete any existing identity first to self-repair and avoid dups.
	
	_PairingSessionDeleteIdentity( me );
	
	// Create a new identity and store it in the Keychain.
	
	identifierPtr = me->identifierPtr;
	identifierLen = me->identifierLen;
	if( !identifierPtr || ( identifierLen == 0 ) )
	{
		UUIDGet( uuid );
		UUIDtoCString( uuid, false, identifierBuf );
		identifierPtr = identifierBuf;
		identifierLen = strlen( identifierBuf );
	}
	Ed25519_make_key_pair( pk, sk );
	SNPrintF( pkAndSKHex, sizeof( pkAndSKHex ), "%.3H+%.3H", pk, 32, 32, sk, 32, 32 );
	
	label = NULL;
	ASPrintF( &label, "%@: %.*s", me->keychainIdentityLabel, (int) identifierLen, identifierPtr );
	require_action( label, exit, err = kNoMemoryErr );
	
	err = KeychainAddFormatted( NULL, 
		"{"
			"%kO=%O"	// class
			#if( !TARGET_IPHONE_SIMULATOR )
			"%kO=%O"	// accessGroup
			#endif
			"%kO=%O"	// accessible
			"%kO=%i"	// type (4-char-code type)
			"%kO=%s"	// label (user-visible name)
			"%kO=%O"	// description (user-visible kind)
			"%kO=%.*s"	// account (identifier)
			"%kO=%O"	// service (user-visible "where").
			"%kO=%D"	// password (hex public+secret keys)
			"%kO=%O"	// synchronizable
		"}", 
		kSecClass,				kSecClassGenericPassword, 
		#if( !TARGET_IPHONE_SIMULATOR )
		kSecAttrAccessGroup,	me->keychainAccessGroup, 
		#endif
		kSecAttrAccessible,		kSecAttrAccessibleAlways_compat,
		kSecAttrType,			me->keychainIdentityType, 
		kSecAttrLabel,			label, 
		kSecAttrDescription,	me->keychainIdentityDesc, 
		kSecAttrAccount,		(int) identifierLen, identifierPtr, 
		kSecAttrService,		me->keychainIdentityLabel, 
		kSecValueData,			pkAndSKHex, (int) strlen( pkAndSKHex ), 
		kSecAttrSynchronizable,	( me->keychainFlags & kPairingKeychainFlag_iCloudIdentity ) ? kCFBooleanTrue : NULL );
	free( label );
	require_noerr_quiet( err, exit );
	
	if( outIdentifier )
	{
		identifier = strndup( identifierPtr, identifierLen );
		require_action( identifier, exit, err = kNoMemoryErr );
		*outIdentifier = identifier;
	}
	if( outPK ) memcpy( outPK, pk, 32 );
	if( outSK ) memcpy( outSK, sk, 32 );
	pair_ulog( me, kLogLevelNotice, "Created %@: %.*s\n", me->keychainIdentityLabel, (int) identifierLen, identifierPtr );
	
exit:
	MemZeroSecure( sk, sizeof( sk ) );
	MemZeroSecure( pkAndSKHex, sizeof( pkAndSKHex ) );
	return( err );
}

//===========================================================================================================================
//	PairingSessionCopyPeer
//===========================================================================================================================

CFDictionaryRef
	PairingSessionCopyPeer( 
		PairingSessionRef	me, 
		const void *		inIdentifierPtr, 
		size_t				inIdentifierLen, 
		OSStatus *			outErr )
{
	CFDictionaryRef		result = NULL;
	OSStatus			err;
	CFArrayRef			peers;
	CFDictionaryRef		peer;
	
	peers = _PairingSessionCopyPeers( me, inIdentifierPtr, inIdentifierLen, &err );
	require_noerr_quiet( err, exit );
	require_action_quiet( CFArrayGetCount( peers ) > 0, exit, err = kNotFoundErr );
	
	peer = CFArrayGetCFDictionaryAtIndex( peers, 0, &err );
	require_noerr( err, exit );
	CFRetain( peer );
	result = peer;
	
exit:
	CFReleaseNullSafe( peers );
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	PairingSessionCopyPeers
//===========================================================================================================================

CFArrayRef	PairingSessionCopyPeers( PairingSessionRef me, OSStatus *outErr )
{
	return( _PairingSessionCopyPeers( me, NULL, 0, outErr ) );
}

static CFArrayRef
	_PairingSessionCopyPeers( 
		PairingSessionRef	me, 
		const void *		inIdentifierPtr, 
		size_t				inIdentifierLen, 
		OSStatus *			outErr )
{
	CFArrayRef					peers		= NULL;
	CFMutableArrayRef			tempPeers	= NULL;
	OSStatus					err;
	CFArrayRef					results;
	CFIndex						i, n;
	CFDictionaryRef				attrs;
	CFStringRef					identifier;
	CFDataRef					data = NULL;
	CFDataRef					data2;
	CFDictionaryRef				info;
	CFMutableDictionaryRef		peer;
	uint8_t						pk[ 32 ];
	size_t						len;
	
	if( inIdentifierLen == kSizeCString ) inIdentifierLen = strlen( (const char *) inIdentifierPtr );
	
	pthread_mutex_lock( &gPairingGlobalLock );
	
	// Note: Keychain APIs prevent getting all items and secret data. So get all attributes then get secret data individually.
	
	results = (CFArrayRef) KeychainCopyMatchingFormatted( &err, 
		"{"
			"%kO=%O"	// class
			#if( !TARGET_IPHONE_SIMULATOR )
			"%kO=%O"	// accessGroup
			#endif
			"%kO=%i"	// type
			"%kO=%?.*s"	// account (identifier)
			"%kO=%O"	// synchronizable
			"%kO=%O"	// returnAttributes
			"%kO=%O"	// matchLimit
		"}", 
		kSecClass,				kSecClassGenericPassword, 
		#if( !TARGET_IPHONE_SIMULATOR )
		kSecAttrAccessGroup,	me->keychainAccessGroup, 
		#endif
		kSecAttrType,			me->keychainPeerType, 
		kSecAttrAccount,		( inIdentifierPtr && ( inIdentifierLen > 0 ) ), (int) inIdentifierLen, inIdentifierPtr, 
		kSecAttrSynchronizable,	kSecAttrSynchronizableAny, 
		kSecReturnAttributes,	kCFBooleanTrue, 
		kSecMatchLimit,			kSecMatchLimitAll );
	if( err == errSecItemNotFound ) err = kNoErr;
	require_noerr_quiet( err, exit );
	
	tempPeers = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( tempPeers, exit, err = kNoMemoryErr );
	
	n = results ? CFArrayGetCount( results ) : 0;
	for( i = 0; i < n; ++i )
	{
		attrs = CFArrayGetCFDictionaryAtIndex( results, i, &err );
		check_noerr( err );
		if( err ) continue;
		
		peer = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( peer, exit, err = kNoMemoryErr );
		
		// Identifier
		
		identifier = CFDictionaryGetCFString( attrs, kSecAttrAccount, &err );
		require_noerr( err, skip );
		CFDictionarySetValue( peer, CFSTR( kPairingKey_Identifier ), identifier );
		
		// Public Key
		
		data = (CFDataRef) KeychainCopyMatchingFormatted( &err, 
			"{"
				"%kO=%O" // class
				#if( !TARGET_IPHONE_SIMULATOR )
				"%kO=%O" // accessGroup
				#endif
				"%kO=%i" // type
				"%kO=%O" // synchronizable
				"%kO=%O" // account (identifier)
				"%kO=%O" // returnData
			"}", 
			kSecClass,				kSecClassGenericPassword, 
			#if( !TARGET_IPHONE_SIMULATOR )
			kSecAttrAccessGroup,	me->keychainAccessGroup, 
			#endif
			kSecAttrType,			me->keychainPeerType, 
			kSecAttrSynchronizable,	kSecAttrSynchronizableAny, 
			kSecAttrAccount,		identifier, 
			kSecReturnData,			kCFBooleanTrue );
		if( err ) CFRelease( peer );
		require_noerr_quiet( err, skip );
		
		len = 0;
		err = HexToData( CFDataGetBytePtr( data ), (size_t) CFDataGetLength( data ), kHexToData_DefaultFlags, 
			pk, sizeof( pk ), &len, NULL, NULL );
		require_noerr( err, skip );
		require( len == 32, skip );
		CFDictionarySetData( peer, CFSTR( kPairingKey_PublicKey ), pk, len );
		
		// Info
		
		data2 = CFDictionaryGetCFData( attrs, kSecAttrGeneric, NULL );
		if( data2 )
		{
			info = (CFDictionaryRef) CFPropertyListCreateWithData( NULL, data2, 0, NULL, NULL );
			if( info && !CFIsType( info, CFDictionary ) )
			{
				dlogassert( "Bad pairing info type: %@", info );
				CFRelease( info );
				info = NULL;
			}
			if( info )
			{
				CFDictionarySetValue( peer, CFSTR( kPairingKey_Info ), info );
				CFRelease( info );
			}
		}
		
		CFArrayAppendValue( tempPeers, peer );
		
	skip:
		CFRelease( peer );
		ForgetCF( &data );
	}
	
	peers = tempPeers;
	tempPeers = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( tempPeers );
	CFReleaseNullSafe( results );
	ForgetCF( &data );
	if( outErr ) *outErr = err;
	pthread_mutex_unlock( &gPairingGlobalLock );
	return( peers );
}

//===========================================================================================================================
//	PairingSessionDeletePeer
//===========================================================================================================================

OSStatus	PairingSessionDeletePeer( PairingSessionRef me, const void *inIdentifierPtr, size_t inIdentifierLen )
{
	OSStatus		err;
	
	pthread_mutex_lock( &gPairingGlobalLock );
	err = _PairingSessionDeletePeer( me, inIdentifierPtr, inIdentifierLen );
	pthread_mutex_unlock( &gPairingGlobalLock );
	return( err );
}

#if( KEYCHAIN_LITE_ENABLED )
//===========================================================================================================================
//	_PairingSessionDeletePeer
//===========================================================================================================================

static OSStatus	_PairingSessionDeletePeer( PairingSessionRef me, const void *inIdentifierPtr, size_t inIdentifierLen )
{
	return( KeychainDeleteFormatted( 
		"{"
			"%kO=%O"	// class
			"%kO=%i"	// type
			"%kO=%?.*s"	// account (identifier)
			"%kO=%O"	// matchLimit
		"}", 
		kSecClass,			kSecClassGenericPassword, 
		kSecAttrType,		me->keychainPeerType, 
		kSecAttrAccount,	( inIdentifierPtr && ( inIdentifierLen > 0 ) ), (int) inIdentifierLen, inIdentifierPtr, 
		kSecMatchLimit,		kSecMatchLimitAll ) );
}
#else
//===========================================================================================================================
//	_PairingSessionDeletePeer
//===========================================================================================================================

static OSStatus	_PairingSessionDeletePeer( PairingSessionRef me, const void *inIdentifierPtr, size_t inIdentifierLen )
{
	OSStatus			err;
	CFArrayRef			items;
	CFIndex				i, n;
	CFDictionaryRef		attrs;
	uint32_t			type;
	char *				cptr;
	CFDataRef			persistentRef;
	Boolean				b;
	
	if( inIdentifierLen == kSizeCString ) inIdentifierLen = strlen( (const char *) inIdentifierPtr );
	
	// Work around <radar:8782844> (SecItemDelete and kSecMatchLimitAll) by getting all and deleting individually.
	
	items = (CFArrayRef) KeychainCopyMatchingFormatted( NULL, 
		"{"
			"%kO=%O" // class
			#if( !TARGET_IPHONE_SIMULATOR )
			"%kO=%O" // accessGroup
			#endif
			"%kO=%i" // type
			"%kO=%O" // synchronizable
			"%kO=%O" // returnAttributes
			"%kO=%O" // returnRef
			"%kO=%O" // matchLimit
		"}", 
		kSecClass,					kSecClassGenericPassword, 
		#if( !TARGET_IPHONE_SIMULATOR )
		kSecAttrAccessGroup,		me->keychainAccessGroup, 
		#endif
		kSecAttrType,				me->keychainPeerType, 
		kSecAttrSynchronizable,		kSecAttrSynchronizableAny, 
		kSecReturnAttributes,		kCFBooleanTrue, 
		kSecReturnPersistentRef,	kCFBooleanTrue, 
		kSecMatchLimit,				kSecMatchLimitAll );
	n = items ? CFArrayGetCount( items ) : 0;
	for( i = 0; i < n; ++i )
	{
		attrs = CFArrayGetCFDictionaryAtIndex( items, i, NULL );
		check( attrs );
		if( !attrs ) continue;
		
		type = (uint32_t) CFDictionaryGetInt64( attrs, kSecAttrType, &err );
		check_noerr( err );
		if( type != me->keychainPeerType ) continue;
		
		if( inIdentifierPtr )
		{
			cptr = CFDictionaryCopyCString( attrs, kSecAttrAccount, &err );
			check_noerr( err );
			if( err ) continue;
			b = ( strnicmpx( (const char *) inIdentifierPtr, inIdentifierLen, cptr ) == 0 );
			free( cptr );
			if( !b ) continue;
		}
		
		persistentRef = CFDictionaryGetCFData( attrs, kSecValuePersistentRef, &err );
		check_noerr( err );
		if( err ) continue;
		
		err = KeychainDeleteItemByPersistentRef( persistentRef, attrs );
		check_noerr( err );
	}
	CFReleaseNullSafe( items );
	return( kNoErr );
}
#endif // KEYCHAIN_LITE_ENABLED
#endif // PAIRING_KEYCHAIN

//===========================================================================================================================
//	PairingSessionFindPeer
//===========================================================================================================================

OSStatus	PairingSessionFindPeer( PairingSessionRef me, const void *inIdentifierPtr, size_t inIdentifierLen, uint8_t *outPK )
{
	OSStatus		err;
	
	if( me->delegate.findPeer_f )
	{
		err = me->delegate.findPeer_f( inIdentifierPtr, inIdentifierLen, outPK, me->delegate.context );
		require_noerr_action_quiet( err, exit, err = kNotFoundErr );
	}
	else
	{
		#if( PAIRING_KEYCHAIN )
			err = _PairingSessionFindPeerKeychain( me, inIdentifierPtr, inIdentifierLen, outPK );
			require_noerr_action_quiet( err, exit, err = kNotFoundErr );
		#else
			dlogassert( "No findPeer_f" );
			err = kNotPreparedErr;
			goto exit;
		#endif
	}
	
exit:
	return( err );
}

#if( PAIRING_KEYCHAIN )
//===========================================================================================================================
//	_PairingSessionFindPeerKeychain
//===========================================================================================================================

static OSStatus
	_PairingSessionFindPeerKeychain( 
		PairingSessionRef	me, 
		const void *		inIdentifierPtr, 
		size_t				inIdentifierLen, 
		uint8_t *			outPK )
{
	OSStatus		err;
	CFDataRef		data;
	size_t			len;
	
	pthread_mutex_lock( &gPairingGlobalLock );
	
	if( inIdentifierLen == kSizeCString ) inIdentifierLen = strlen( (const char *) inIdentifierPtr );
	data = (CFDataRef) KeychainCopyMatchingFormatted( &err, 
		"{"
			"%kO=%O"	// class
			#if( !TARGET_IPHONE_SIMULATOR )
			"%kO=%O"	// accessGroup
			#endif
			"%kO=%i"	// type
			"%kO=%O"	// synchronizable
			"%kO=%.*s"	// account (identifier)
			"%kO=%O"	// returnData
		"}", 
		kSecClass,				kSecClassGenericPassword, 
		#if( !TARGET_IPHONE_SIMULATOR )
		kSecAttrAccessGroup,	me->keychainAccessGroup, 
		#endif
		kSecAttrType,			me->keychainPeerType, 
		kSecAttrSynchronizable,	kSecAttrSynchronizableAny, 
		kSecAttrAccount,		(int) inIdentifierLen, inIdentifierPtr, 
		kSecReturnData,			kCFBooleanTrue );
	require_noerr_quiet( err, exit );
	
	len = 0;
	err = HexToData( CFDataGetBytePtr( data ), (size_t) CFDataGetLength( data ), kHexToData_DefaultFlags, 
		outPK, 32, &len, NULL, NULL );
	require_noerr( err, exit );
	require_action( len == 32, exit, err = kSizeErr );
	
exit:
	CFReleaseNullSafe( data );
	pthread_mutex_unlock( &gPairingGlobalLock );
	return( err );
}
#endif // PAIRING_KEYCHAIN

//===========================================================================================================================
//	PairingSessionSavePeer
//===========================================================================================================================

OSStatus
	PairingSessionSavePeer( 
		PairingSessionRef	me, 
		const void *		inIdentifierPtr, 
		size_t				inIdentifierLen, 
		const uint8_t *		inPK )
{
	OSStatus		err;
	
	if( inIdentifierLen == kSizeCString ) inIdentifierLen = strlen( (const char *) inIdentifierPtr );
	
	if( me->delegate.savePeer_f )
	{
		err = me->delegate.savePeer_f( inIdentifierPtr, inIdentifierLen, inPK, me->delegate.context );
		require_noerr_quiet( err, exit );
	}
	else
	{
		#if( PAIRING_KEYCHAIN )
			err = _PairingSessionSavePeerKeychain( me, inIdentifierPtr, inIdentifierLen, inPK );
			require_noerr_quiet( err, exit );
		#else
			dlogassert( "No savePeer_f" );
			err = kNotPreparedErr;
			goto exit;
		#endif
	}
	
exit:
	return( err );
}

#if( PAIRING_KEYCHAIN )
//===========================================================================================================================
//	_PairingSessionSavePeerKeychain
//===========================================================================================================================

static OSStatus
	_PairingSessionSavePeerKeychain( 
		PairingSessionRef	me, 
		const void *		inIdentifierPtr, 
		size_t				inIdentifierLen, 
		const uint8_t *		inPK )
{
	OSStatus					err;
	CFMutableDictionaryRef		infoDict;
	CFDataRef					infoData = NULL;
	char						hex[ ( 32 * 2 ) + 1 ];
	char *						label;
	
	pthread_mutex_lock( &gPairingGlobalLock );
	
	infoDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( infoDict, exit, err = kNoMemoryErr );
	if( CFDictionaryGetCount( infoDict ) > 0 )
	{
		infoData = CFPropertyListCreateData( NULL, infoDict, kCFPropertyListBinaryFormat_v1_0, 0, NULL );
		CFRelease( infoDict );
		require_action( infoData, exit, err = kUnknownErr );
	}
	else
	{
		CFRelease( infoDict );
	}
	
	// Delete any existing peer first to self-repair and avoid dups.
	
	_PairingSessionDeletePeer( me, inIdentifierPtr, inIdentifierLen );
	
	// Create the new Keychain entry.
	
	label = NULL;
	ASPrintF( &label, "%@: %.*s", me->keychainPeerLabel, (int) inIdentifierLen, inIdentifierPtr );
	require_action( label, exit, err = kNoMemoryErr );
	
	DataToHexCString( inPK, 32, hex );
	
	err = KeychainAddFormatted( NULL, 
		"{"
			"%kO=%O"	// class
			#if( !TARGET_IPHONE_SIMULATOR )
			"%kO=%O"	// accessGroup
			#endif
			"%kO=%O"	// accessible
			"%kO=%i"	// type (4-char-code type)
			"%kO=%s"	// label (user-visible name)
			"%kO=%O"	// description (user-visible kind)
			"%kO=%.*s"	// account (identifier)
			"%kO=%O"	// service (user-visible "where")
			"%kO=%O"	// generic (custom info)
			"%kO=%D"	// valueData (hex public key)
			"%kO=%O"	// synchronizable
		"}", 
		kSecClass,				kSecClassGenericPassword, 
		#if( !TARGET_IPHONE_SIMULATOR )
		kSecAttrAccessGroup,	me->keychainAccessGroup, 
		#endif
		kSecAttrAccessible,		( me->keychainFlags & kPairingKeychainFlag_HighSecurity ) ? 
									kSecAttrAccessibleWhenUnlocked : 
									kSecAttrAccessibleAlways_compat, 
		kSecAttrType,			me->keychainPeerType, 
		kSecAttrLabel,			label, 
		kSecAttrDescription,	me->keychainPeerDesc, 
		kSecAttrAccount,		(int) inIdentifierLen, inIdentifierPtr, 
		kSecAttrService,		me->keychainPeerLabel, 
		kSecAttrGeneric,		infoData, 
		kSecValueData,			hex, 32 * 2, 
		kSecAttrSynchronizable,	( me->keychainFlags & kPairingKeychainFlag_iCloudPeers ) ? kCFBooleanTrue : NULL );
	free( label );
	require_quiet( err != errSecAuthFailed, exit );
	require_noerr( err, exit );
	
exit:
	if( err ) pair_ulog( me, kLogLevelWarning, "### Save %@ %.*s failed: %#m\n", 
		me->keychainPeerLabel, (int) inIdentifierLen, inIdentifierPtr, err );
	CFReleaseNullSafe( infoData );
	pthread_mutex_unlock( &gPairingGlobalLock );
	return( err );
}

//===========================================================================================================================
//	PairingSessionUpdatePeerInfo
//===========================================================================================================================

OSStatus
	PairingSessionUpdatePeerInfo( 
		PairingSessionRef	me, 
		const void *		inIdentifierPtr, 
		size_t				inIdentifierLen, 
		CFDictionaryRef		inInfo )
{
	OSStatus					err;
	CFMutableDictionaryRef		query = NULL;
	CFDataRef					infoData;
	
	if( inIdentifierLen == kSizeCString ) inIdentifierLen = strlen( (const char *) inIdentifierPtr );
	
	pthread_mutex_lock( &gPairingGlobalLock );
	
	err = CFPropertyListCreateFormatted( NULL, &query, 
		"{"
			"%kO=%O"	// class
			#if( !TARGET_IPHONE_SIMULATOR )
			"%kO=%O"	// accessGroup
			#endif
			"%kO=%i"	// type
			"%kO=%.*s"	// account (identifier)
			"%kO=%O"	// synchronizable
		"}", 
		kSecClass,				kSecClassGenericPassword, 
		#if( !TARGET_IPHONE_SIMULATOR )
		kSecAttrAccessGroup,	me->keychainAccessGroup, 
		#endif
		kSecAttrType,			me->keychainPeerType, 
		kSecAttrAccount,		(int) inIdentifierLen, inIdentifierPtr, 
		kSecAttrSynchronizable,	kSecAttrSynchronizableAny );
	require_noerr( err, exit );
	
	infoData = CFPropertyListCreateData( NULL, inInfo, kCFPropertyListBinaryFormat_v1_0, 0, NULL );
	require_action( infoData, exit, err = kUnknownErr );
	
	err = KeychainUpdateFormatted( query, 
		"{"
			"%kO=%O" // generic (custom info)
		"}", 
		kSecAttrGeneric, infoData );
	CFRelease( infoData );
	require_noerr_quiet( err, exit );
	
exit:
	if( err ) pair_ulog( me, kLogLevelWarning, "### Update %@ %.*s info failed: %#m\n", 
		me->keychainPeerLabel, (int) inIdentifierLen, inIdentifierPtr, err );
	CFReleaseNullSafe( query );
	pthread_mutex_unlock( &gPairingGlobalLock );
	return( err );
}
#endif // PAIRING_KEYCHAIN

#if 0
#pragma mark -
#pragma mark == Testing ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	Testing
//===========================================================================================================================

#if( !defined( MFI_PAIR_SETUP_TEST ) )
	#if( PAIRING_MFI_CLIENT && PAIRING_MFI_SERVER )
		#define MFI_PAIR_SETUP_TEST		0
	#else
		#define MFI_PAIR_SETUP_TEST		0
	#endif
#endif

#define kPairingTestClientPK \
	"\xF0\x40\x3C\xD1\xF5\xC5\x22\xF7\xAE\xFF\x9C\xD2\x4B\x98\xE8\xD3" \
	"\xC7\xA1\xB6\x62\x48\x4A\x51\x36\x99\x27\x05\x44\x07\xB6\x74\x33"
#define kPairingTestClientSK \
	"\xB3\xC0\xAE\x54\x79\x58\xC3\xE7\x51\xD6\x74\x94\xC0\x26\x04\xC8" \
	"\xEB\x94\x57\x2F\x9E\x21\xAF\x8A\x6C\xC6\x6C\x37\xDF\x91\xB5\xD3"

#define kPairingTestServerPK \
	"\xEC\x10\x03\xEB\x70\x5F\xEA\x0E\x4A\xEB\x71\x66\x09\xDA\x18\x84" \
	"\xC9\x77\xEF\x53\xC4\x8A\x7A\xA6\x92\x8A\xBA\x44\xF5\x3B\x8D\xC1"
#define kPairingTestServerSK \
	"\x35\xA6\xBC\x91\x97\x97\x0E\xDB\xE0\x09\xC8\x4E\x6D\xAB\xE6\x13" \
	"\xBD\x05\x01\x1D\x03\x30\xE6\xAC\x6F\xA4\x03\xAF\x15\xD7\x27\x40"

typedef struct
{
	PairingSessionRef		session;
	Boolean					client;
	const void *			param1;
	int						testNum;
	Boolean					stop;
	
}	PairingTestContext;

static OSStatus
	_PairingUtilsTestPairSetupSetupCode( 
		const char *	inSetupCode1, 
		const char *	inSetupCode2, 
		int				inCount, 
		size_t			inMTU, 
		Boolean			inMFI, 
		Boolean			inKeychain );
static OSStatus	_PairingUtilsTest_ShowSetupCode( PairingFlags inFlags, char *inBuffer, size_t inMaxLen, void *inContext );
static OSStatus	_PairingUtilsTest_PromptForSetupCode( PairingFlags inFlags, int32_t inDelaySeconds, void *inContext );
static OSStatus	_PairingUtilsTestPairVerify( int inTestNum, size_t inMTU );

static OSStatus
	_PairingUtilsTest_CopyIdentity( 
		Boolean		inAllowCreate, 
		char **		outIdentifier, 
		uint8_t		outPK[ 32 ], 
		uint8_t		outSK[ 32 ], 
		void *		inContext );
static OSStatus
	_PairingUtilsTest_FindPeer( 
		const void *	inIdentifierPtr, 
		size_t			inIdentifierLen, 
		uint8_t			outPK[ 32 ], 
		void *			inContext );
static OSStatus
	_PairingUtilsTest_SavePeer( 
		const void *	inIdentifierPtr, 
		size_t			inIdentifierLen, 
		const uint8_t	inPK[ 32 ], 
		void *			inContext );
static OSStatus	_PairingUtilsTest_Cleanup( void );

ulog_define( PairingTest, kLogLevelError, kLogFlags_None, "PairingTest", NULL );

//===========================================================================================================================
//	PairingUtilsTest
//===========================================================================================================================

OSStatus	PairingUtilsTest( int inPerf )
{
	OSStatus		err;
	uint64_t		ticks;
	size_t			mtu;
	
	err = _PairingUtilsTest_Cleanup();
	require_noerr( err, exit );
	
#if( MFI_PAIR_SETUP_TEST )
	// MFi+PIN PIN-based Pair-Setup
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1235", 1, SIZE_MAX, true, false );
	require_action( err != kNoErr, exit, err = -1 );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, SIZE_MAX, true, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "Testing1", "Testing1", 1, SIZE_MAX, true, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "12345", 1, SIZE_MAX, true, false );
	require_action( err != kNoErr, exit, err = -1 );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, 3, true, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, 16, true, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, 255, true, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, 256, true, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, 300, true, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, 500, true, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, 512, true, false );
	require_noerr( err, exit );
#endif
	
	// PIN-based Pair-Setup
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1235", 1, SIZE_MAX, false, false );
	require_action( err != kNoErr, exit, err = -1 );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, SIZE_MAX, false, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "Testing1", "Testing1", 1, SIZE_MAX, false, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "12345", 1, SIZE_MAX, false, false );
	require_action( err != kNoErr, exit, err = -1 );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, 3, false, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, 16, false, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, 255, false, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, 256, false, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, 300, false, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, 500, false, false );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, 512, false, false );
	require_noerr( err, exit );
	
	// Pair-Verify
	
	err = _PairingUtilsTestPairVerify( 1, SIZE_MAX );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairVerify( 2, SIZE_MAX );
	require_noerr( err, exit );
	
	err = _PairingUtilsTestPairVerify( 3, SIZE_MAX );
	require_noerr( err, exit );
	
	for( mtu = 3; mtu < 200; ++mtu )
	{
		err = _PairingUtilsTestPairVerify( 4, mtu );
		require_noerr( err, exit );
	}
	
#if( PAIRING_KEYCHAIN )
	// Keychain
	
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, SIZE_MAX, false, true );
	require_noerr( err, exit );
	err = _PairingUtilsTestPairSetupSetupCode( "1234", "1234", 1, SIZE_MAX, false, true );
	require_noerr( err, exit );
	
	err = _PairingUtilsTest_Cleanup();
	require_noerr( err, exit );
#endif
	
	// Performance tests.
	
	if( inPerf )
	{
		ticks = UpTicks();
		err = _PairingUtilsTestPairSetupSetupCode( "ABCD", "ABCD", 10, SIZE_MAX, false, false );
		require_noerr( err, exit );
		ticks = UpTicksToMilliseconds( UpTicks() - ticks );
		printf( "PairingUtilsTest PIN performance: %llu ms (%llu ms per setup)\n", 
			(unsigned long long) ticks, (unsigned long long)( ticks / 10 ) );
	}
	
exit:
	printf( "PairingUtilsTest: %s\n", !err ? "PASSED" : "FAILED" );
	_PairingUtilsTest_Cleanup();
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_PairingUtilsTestPairSetupSetupCode
//===========================================================================================================================

static OSStatus
	_PairingUtilsTestPairSetupSetupCode( 
		const char *	inSetupCode1, 
		const char *	inSetupCode2, 
		int				inCount, 
		size_t			inMTU, 
		Boolean			inMFI, 
		Boolean			inKeychain )
{
	OSStatus				err;
	PairingDelegate			clientDelegate;
	PairingDelegate			serverDelegate;
	PairingTestContext		clientCtx	= { NULL, true,  inSetupCode1, 0, false };
	PairingTestContext		serverCtx	= { NULL, false, inSetupCode2, 0, false };
	uint8_t *				clientPtr	= NULL;
	size_t					clientLen	= 0;
	Boolean					clientDone	= false;
	uint8_t *				serverPtr	= NULL;
	size_t					serverLen	= 0;
	Boolean					serverDone	= false;
	int						i;
	char *					identifier	= NULL;
	size_t					len;
	
	PairingDelegateInit( &clientDelegate );
	clientDelegate.context				= &clientCtx;
	clientDelegate.promptForSetupCode_f	= _PairingUtilsTest_PromptForSetupCode;
	if( !inKeychain )
	{
		clientDelegate.copyIdentity_f	= _PairingUtilsTest_CopyIdentity;
		clientDelegate.savePeer_f		= _PairingUtilsTest_SavePeer;
	}
	err = PairingSessionCreate( &clientCtx.session, &clientDelegate, kPairingSessionType_SetupClient );
	require_noerr( err, exit );
	if( inMFI ) PairingSessionSetFlags( clientCtx.session, kPairingFlag_MFi );
	PairingSessionSetLogging( clientCtx.session, &log_category_from_name( PairingTest ) );
	err = PairingSessionSetIdentifier( clientCtx.session, "TestClient", kSizeCString );
	require_noerr( err, exit );
	err = PairingSessionSetMTU( clientCtx.session, inMTU );
	require_noerr( err, exit );
#if( PAIRING_KEYCHAIN )
	PairingSessionSetKeychainInfo( clientCtx.session, 
		CFSTR( "PairingUtilsTestAccessGroup" ), 
		0x50544954, // 'PTIT' 
		CFSTR( "PairingUtilsTestIdentityLabel" ), 
		CFSTR( "PairingUtilsTestIdentityDescription" ), 
		0x50545054, // 'PTPT' 
		CFSTR( "PairingUtilsTestPeerLabel" ), 
		CFSTR( "PairingUtilsTestPeerDescription" ), 
		0 );
#endif
	
	PairingDelegateInit( &serverDelegate );
	serverDelegate.context				= &serverCtx;
	serverDelegate.showSetupCode_f		= _PairingUtilsTest_ShowSetupCode;
	if( !inKeychain )
	{
		serverDelegate.copyIdentity_f	= _PairingUtilsTest_CopyIdentity;
		serverDelegate.savePeer_f		= _PairingUtilsTest_SavePeer;
	}
	err = PairingSessionCreate( &serverCtx.session, &serverDelegate, kPairingSessionType_SetupServer );
	require_noerr( err, exit );
	PairingSessionSetLogging( serverCtx.session, &log_category_from_name( PairingTest ) );
	err = PairingSessionSetIdentifier( serverCtx.session, "TestServer", kSizeCString );
	require_noerr( err, exit );
	err = PairingSessionSetMTU( serverCtx.session, inMTU );
	require_noerr( err, exit );
#if( PAIRING_KEYCHAIN )
	PairingSessionSetKeychainInfo( serverCtx.session, 
		CFSTR( "PairingUtilsTestAccessGroup" ), 
		0x50544954, // 'PTIT' 
		CFSTR( "PairingUtilsTestIdentityLabel" ), 
		CFSTR( "PairingUtilsTestIdentityDescription" ), 
		0x50545054, // 'PTPT' 
		CFSTR( "PairingUtilsTestPeerLabel" ), 
		CFSTR( "PairingUtilsTestPeerDescription" ), 
		0 );
#endif
	
	for( i = 0; i < inCount; ++i )
	{
		clientCtx.session->state	= kPairingStateInvalid;
		serverCtx.session->state	= kPairingStateInvalid;
		clientCtx.stop				= false;
		serverCtx.stop				= false;
		clientDone					= false;
		serverDone					= false;
		do
		{
			if( !clientDone )
			{
				err = PairingSessionExchange( clientCtx.session, serverPtr, serverLen, &clientPtr, &clientLen, &clientDone );
				ForgetPtrLen( &serverPtr, &serverLen );
				if( err == kAsyncNoErr ) continue;
				require_noerr_quiet( err, exit );
			}
			if( !serverDone )
			{
				err = PairingSessionExchange( serverCtx.session, clientPtr, clientLen, &serverPtr, &serverLen, &serverDone );
				ForgetPtrLen( &clientPtr, &clientLen );
				require_noerr_quiet( err, exit );
			}
			
		}	while( ( !clientCtx.stop && !serverCtx.stop ) && ( !clientDone || !serverDone ) );
		require_noerr_quiet( err, exit );
		
		identifier = PairingSessionCopyPeerIdentifier( clientCtx.session, &len, &err );
		require_noerr( err, exit );
		require_action( identifier, exit, err = kUnknownErr );
		if( inKeychain )	require_action( MemEqual( identifier, len, "TestClient", 10 ), exit, err = kUnknownErr );
		else				require_action( MemEqual( identifier, len, "TestServer", 10 ), exit, err = kUnknownErr );
		require_action( identifier[ len ] == '\0', exit, err = kUnknownErr );
		ForgetMem( &identifier );
		
		identifier = PairingSessionCopyPeerIdentifier( serverCtx.session, &len, &err );
		require_noerr( err, exit );
		require_action( identifier, exit, err = kUnknownErr );
		require_action( MemEqual( identifier, len, "TestClient", 10 ), exit, err = kUnknownErr );
		require_action( identifier[ len ] == '\0', exit, err = kUnknownErr );
		ForgetMem( &identifier );
	}
	
exit:
	if( clientCtx.session ) pair_ulog( clientCtx.session, kLogLevelTrace, "\n" );
	FreeNullSafe( clientPtr );
	FreeNullSafe( serverPtr );
	CFReleaseNullSafe( clientCtx.session );
	CFReleaseNullSafe( serverCtx.session );
	FreeNullSafe( identifier );
	return( err );
}

//===========================================================================================================================
//	_PairingUtilsTestPairVerify
//===========================================================================================================================

static OSStatus	_PairingUtilsTestPairVerify( int inTestNum, size_t inMTU )
{
	OSStatus				err;
	PairingDelegate			clientDelegate;
	PairingDelegate			serverDelegate;
	PairingTestContext		clientCtx	= { NULL, true,  NULL, 0, false };
	PairingTestContext		serverCtx	= { NULL, false, NULL, 0, false };
	uint8_t *				clientPtr	= NULL;
	size_t					clientLen	= 0;
	Boolean					clientDone	= false;
	uint8_t *				serverPtr	= NULL;
	size_t					serverLen	= 0;
	Boolean					serverDone	= false;
	Boolean					shouldFail	= false;
	
	clientCtx.testNum = inTestNum;
	serverCtx.testNum = inTestNum;
	if(      inTestNum == 2 ) shouldFail = true;
	else if( inTestNum == 3 ) shouldFail = true;
	
	PairingDelegateInit( &clientDelegate );
	clientDelegate.context			= &clientCtx;
	clientDelegate.copyIdentity_f	= _PairingUtilsTest_CopyIdentity;
	clientDelegate.findPeer_f		= _PairingUtilsTest_FindPeer;
	err = PairingSessionCreate( &clientCtx.session, &clientDelegate, kPairingSessionType_VerifyClient );
	require_noerr( err, exit );
	PairingSessionSetLogging( clientCtx.session, &log_category_from_name( PairingTest ) );
	err = PairingSessionSetIdentifier( clientCtx.session, "TestClient", kSizeCString );
	require_noerr( err, exit );
	err = PairingSessionSetMTU( clientCtx.session, inMTU );
	require_noerr( err, exit );
	
	PairingDelegateInit( &serverDelegate );
	serverDelegate.context			= &serverCtx;
	serverDelegate.copyIdentity_f	= _PairingUtilsTest_CopyIdentity;
	serverDelegate.findPeer_f		= _PairingUtilsTest_FindPeer;
	err = PairingSessionCreate( &serverCtx.session, &serverDelegate, kPairingSessionType_VerifyServer );
	require_noerr( err, exit );
	PairingSessionSetLogging( serverCtx.session, &log_category_from_name( PairingTest ) );
	err = PairingSessionSetIdentifier( serverCtx.session, "TestServer", kSizeCString );
	require_noerr( err, exit );
	err = PairingSessionSetMTU( serverCtx.session, inMTU );
	require_noerr( err, exit );
	
	do
	{
		if( !clientDone )
		{
			err = PairingSessionExchange( clientCtx.session, serverPtr, serverLen, &clientPtr, &clientLen, &clientDone );
			ForgetPtrLen( &serverPtr, &serverLen );
			require_noerr_quiet( err, exit );
		}
		if( !serverDone )
		{
			err = PairingSessionExchange( serverCtx.session, clientPtr, clientLen, &serverPtr, &serverLen, &serverDone );
			ForgetPtrLen( &clientPtr, &clientLen );
			require_noerr_quiet( err, exit );
		}
		
	}	while( ( !clientCtx.stop && !serverCtx.stop ) && ( !clientDone || !serverDone ) );
	
exit:
	if( clientCtx.session ) pair_ulog( clientCtx.session, kLogLevelTrace, "\n" );
	FreeNullSafe( clientPtr );
	FreeNullSafe( serverPtr );
	CFReleaseNullSafe( clientCtx.session );
	CFReleaseNullSafe( serverCtx.session );
	return( shouldFail ? ( err ? kNoErr : kUnknownErr ) : err );
}

//===========================================================================================================================
//	_PairingUtilsTest_ShowSetupCode
//===========================================================================================================================

static OSStatus	_PairingUtilsTest_ShowSetupCode( PairingFlags inFlags, char *inBuffer, size_t inMaxLen, void *inContext )
{
	PairingTestContext * const		ctx = (PairingTestContext *) inContext;
	
	(void) inFlags;
	
	strlcpy( inBuffer, (const char *) ctx->param1, inMaxLen );
	return( kNoErr );
}

//===========================================================================================================================
//	_PairingUtilsTest_PromptForSetupCode
//===========================================================================================================================

static OSStatus	_PairingUtilsTest_PromptForSetupCode( PairingFlags inFlags, int32_t inDelaySeconds, void *inContext )
{
	PairingTestContext * const		ctx = (PairingTestContext *) inContext;
	OSStatus						err;
	
	if( inDelaySeconds > 0 )
	{
		sleep( (unsigned int) inDelaySeconds );
	}
	if( inFlags & kPairingFlag_Incorrect )
	{
		ctx->stop = true;
		err = kNoErr;
		goto exit;
	}
	
	err = PairingSessionSetSetupCode( ctx->session, ctx->param1, kSizeCString );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_PairingUtilsTest_CopyIdentity
//===========================================================================================================================

static OSStatus
	_PairingUtilsTest_CopyIdentity( 
		Boolean		inAllowCreate, 
		char **		outIdentifier, 
		uint8_t		outPK[ 32 ], 
		uint8_t		outSK[ 32 ], 
		void *		inContext )
{
	PairingTestContext * const		ctx = (PairingTestContext *) inContext;
	OSStatus						err;
	char *							identifier;
	
	(void) inAllowCreate;
	
	if( outIdentifier )
	{
		if( ctx->client )	identifier = strdup( "TestClient" );
		else				identifier = strdup( "TestServer" );
		require_action( identifier, exit, err = kNoMemoryErr );
		*outIdentifier = identifier;
	}
	if( outPK )
	{
		if( ctx->client )	memcpy( outPK, kPairingTestClientPK, 32 );
		else				memcpy( outPK, kPairingTestServerPK, 32 );
	}
	if( outSK )
	{
		if( ctx->client )	memcpy( outSK, kPairingTestClientSK, 32 );
		else				memcpy( outSK, kPairingTestServerSK, 32 );
	}
	if( ctx->testNum == 2 ) if( outSK ) outSK[ 22 ] ^= 0x01;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_PairingUtilsTest_FindPeer
//===========================================================================================================================

static OSStatus
	_PairingUtilsTest_FindPeer( 
		const void *	inIdentifierPtr, 
		size_t			inIdentifierLen, 
		uint8_t			outPK[ 32 ], 
		void *			inContext )
{
	PairingTestContext * const		ctx = (PairingTestContext *) inContext;
	OSStatus						err;
	
	require_action_quiet( ctx->testNum != 3, exit, err = kNotFoundErr );
	
	if( ctx->client )
	{
		require_action( strncmpx( inIdentifierPtr, inIdentifierLen, "TestServer" ) == 0, exit, err = kIDErr );
		memcpy( outPK, kPairingTestServerPK, 32 );
	}
	else
	{
		require_action( strncmpx( inIdentifierPtr, inIdentifierLen, "TestClient" ) == 0, exit, err = kIDErr );
		memcpy( outPK, kPairingTestClientPK, 32 );
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_PairingUtilsTest_SavePeer
//===========================================================================================================================

static OSStatus
	_PairingUtilsTest_SavePeer( 
		const void *	inIdentifierPtr, 
		size_t			inIdentifierLen, 
		const uint8_t	inPK[ 32 ], 
		void *			inContext )
{
	PairingTestContext * const		ctx = (PairingTestContext *) inContext;
	OSStatus						err;
	
	if( ctx->client )
	{
		require_action( strncmpx( inIdentifierPtr, inIdentifierLen, "TestServer" ) == 0, exit, err = kIDErr );
		require_action( memcmp( inPK, kPairingTestServerPK, 32 ) == 0, exit, err = kMismatchErr );
	}
	else
	{
		require_action( strncmpx( inIdentifierPtr, inIdentifierLen, "TestClient" ) == 0, exit, err = kIDErr );
		require_action( memcmp( inPK, kPairingTestClientPK, 32 ) == 0, exit, err = kMismatchErr );
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_PairingUtilsTest_Cleanup
//===========================================================================================================================

static OSStatus	_PairingUtilsTest_Cleanup( void )
{
	OSStatus				err;
	PairingDelegate			delegate;
	PairingSessionRef		session = NULL;
	
	// Cleanup client stuff.
	
	PairingDelegateInit( &delegate );
	err = PairingSessionCreate( &session, &delegate, kPairingSessionType_SetupClient );
	require_noerr( err, exit );
	PairingSessionSetLogging( session, &log_category_from_name( PairingTest ) );
	err = PairingSessionSetIdentifier( session, "TestClient", kSizeCString );
	require_noerr( err, exit );
#if( PAIRING_KEYCHAIN )
	PairingSessionSetKeychainInfo( session, 
		CFSTR( "PairingUtilsTestAccessGroup" ), 
		0x50544954, // 'PTIT' 
		CFSTR( "PairingUtilsTestIdentityLabel" ), 
		CFSTR( "PairingUtilsTestIdentityDescription" ), 
		0x50545054, // 'PTPT' 
		CFSTR( "PairingUtilsTestPeerLabel" ), 
		CFSTR( "PairingUtilsTestPeerDescription" ), 
		0 );
#endif
	PairingSessionDeleteIdentity( session );
	PairingSessionDeletePeer( session, NULL, 0 );
	CFRelease( session );
	session = NULL;
	
	// Cleanup server stuff.
	
	PairingDelegateInit( &delegate );
	err = PairingSessionCreate( &session, &delegate, kPairingSessionType_SetupClient );
	require_noerr( err, exit );
	PairingSessionSetLogging( session, &log_category_from_name( PairingTest ) );
	err = PairingSessionSetIdentifier( session, "TestServer", kSizeCString );
	require_noerr( err, exit );
#if( PAIRING_KEYCHAIN )
	PairingSessionSetKeychainInfo( session, 
		CFSTR( "PairingUtilsTestAccessGroup" ), 
		0x50544954, // 'PTIT' 
		CFSTR( "PairingUtilsTestIdentityLabel" ), 
		CFSTR( "PairingUtilsTestIdentityDescription" ), 
		0x50545054, // 'PTPT' 
		CFSTR( "PairingUtilsTestPeerLabel" ), 
		CFSTR( "PairingUtilsTestPeerDescription" ), 
		0 );
#endif
	PairingSessionDeleteIdentity( session );
	PairingSessionDeletePeer( session, NULL, 0 );
	CFRelease( session );
	session = NULL;
	
exit:
	CFReleaseNullSafe( session );
	return( err );
}

#endif // !EXCLUDE_UNIT_TESTS
