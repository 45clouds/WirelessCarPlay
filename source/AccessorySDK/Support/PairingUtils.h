/*
	File:    	PairingUtils.h
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
/*!
	@header			Pairing API
	@discussion		APIs for performing cryptographic pairing between entities.
*/

#ifndef	__PairingUtils_h__
#define	__PairingUtils_h__

#include "CommonServices.h"
#include "LogUtils.h"

#include CF_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == Delegate ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		PairingFeatureFlags
	@abstract	Feature flags for pairing.
*/
typedef uint8_t		PairingFeatureFlags;
#define kPairingFeatureFlags_None				0
#define kPairingFeatureFlag_MFiPairSetup		( 1 << 0 ) // Supports MFi pair-setup.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		PairingFlags
	@abstract	Flags for controlling pairing.
*/
typedef uint32_t		PairingFlags;
#define kPairingFlags_None			0
#define kPairingFlag_MFi			( 1 <<  0 ) // For controller to require proof that accessory has an MFi auth IC.
#define kPairingFlag_Incorrect		( 1 << 16 ) // Indicates a previously entered setup code was incorrect.
#define kPairingFlag_Throttle		( 1 << 17 ) // Peer is throttling setup attempts. Retry later.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		PairingMiscConstants
	@abstract	Constants for pairing.
*/
#define kPairingMIMEType		"application/pairing+tlv8"

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingShowSetupCode_f
	@abstract	Generate a NUL-terminated setup code and show it to the user.
	@discussion	
	
	The setup code must be in the format XXX-XX-XXX where X is a 0-9 digit in ASCII (e.g. "123-45-678").
	If the setup code is being generated on-the-fly (recommended), it must come from a cryptographic random number generator.
	If the setup code is fixed (e.g. printed on a label and burnt into an EEPROM) then it must have been generated using 
	cryptographic random number generator during manufacturing (i.e. don't use a simple counter, manufacture date, region
	of origin, etc. since that can significantly improve an attackers ability to guess it).
*/
typedef OSStatus ( *PairingShowSetupCode_f )( PairingFlags inFlags, char *inBuffer, size_t inMaxLen, void *inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingHideSetupCode_f
	@abstract	Hide any setup code that may be visible for this session.
*/
typedef void ( *PairingHideSetupCode_f )( void *inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingPromptForSetupCode_f
	@abstract	Prompt the user for a setup code.
	@discussion	
	
	The expectation is that this callback will display a dialog and immediately return. When the user enters the setup code, 
	it should be set with PairingSessionSetSetupCode and then PairingSessionExchange should be called to resume the exchange 
	process. If the user cancels the setup code entry dialog, the pairing session can just be released.
	
	@param		inFlags				Flags for setup code prompt.
	@param		inDelaySeconds		< 0 means no delay. >= 0 means the UI must wait N seconds before trying again.
	@param		inContext			Context pointer provided by the delegate.
*/
typedef OSStatus ( *PairingPromptForSetupCode_f )( PairingFlags inFlags, int32_t inDelaySeconds, void *inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingCopyIdentity_f
	@abstract	Optionally copy the identifier, get the Ed25519 public key, and/or get the secret key of this device.
*/
typedef OSStatus
	( *PairingCopyIdentity_f )( 
		Boolean		inAllowCreate, 
		char **		outIdentifier, 
		uint8_t		outPK[ 32 ], 
		uint8_t		outSK[ 32 ], 
		void *		inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingFindPeer_f
	@abstract	Find a peer's Ed25519 public key by its identifier.
*/
typedef OSStatus
	( *PairingFindPeer_f )( 
		const void *	inIdentifierPtr, 
		size_t			inIdentifierLen, 
		uint8_t			outPK[ 32 ], 
		void *			inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSavePeer_f
	@abstract	Save a peer's Ed25519 public key.
*/
typedef OSStatus
	( *PairingSavePeer_f )( 
		const void *	inIdentifierPtr, 
		size_t			inIdentifierLen, 
		const uint8_t	inPK[ 32 ], 
		void *			inContext );

// PairingDelegate

typedef struct
{
	void *							context;
	PairingShowSetupCode_f			showSetupCode_f;
	PairingHideSetupCode_f			hideSetupCode_f;
	PairingPromptForSetupCode_f		promptForSetupCode_f;
	PairingCopyIdentity_f			copyIdentity_f;
	PairingFindPeer_f				findPeer_f;
	PairingSavePeer_f				savePeer_f;
	
}	PairingDelegate;

#define PairingDelegateInit( PTR )	memset( (PTR), 0, sizeof( PairingDelegate ) )

#if 0
#pragma mark -
#pragma mark == PairingSession ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		PairingSessionType
	@abstract	Type of pairing session.
*/
typedef uint32_t		PairingSessionType;
#define kPairingSessionType_None				0 // Used for accessing pairing info without actually pairing.
#define kPairingSessionType_SetupClient			1 // Client-side of pair-setup (side that enters setup code).
#define kPairingSessionType_SetupServer			2 // Server-side of pair-setup (side that displays setup code).
#define kPairingSessionType_VerifyClient		3 // Client-side of pair-verify.
#define kPairingSessionType_VerifyServer		4 // Server-side of pair-verify.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		PairingSession Constants
	@abstract	Constants related to pairing sessions.
*/
#define kPairingKey_Identifier		"identifier"	// [String] Identifier of the peer.
#define kPairingKey_Info			"info"			// [Dictionary Extra info about the peer.
#define kPairingKey_PublicKey		"pk"			// [Data] Public key of the peer.

// Constants for deriving session encryption keys.

#define kPairingControlKeySaltPtr			"Control-Salt"
#define kPairingControlKeySaltLen			sizeof_string( kPairingControlKeySaltPtr )
#define kPairingControlKeyReadInfoPtr		"Control-Read-Encryption-Key"
#define kPairingControlKeyReadInfoLen		sizeof_string( kPairingControlKeyReadInfoPtr )
#define kPairingControlKeyWriteInfoPtr		"Control-Write-Encryption-Key"
#define kPairingControlKeyWriteInfoLen		sizeof_string( kPairingControlKeyWriteInfoPtr )

// [Boolean] Indicates if the pairing peer has been registered by HomeKit.
#define kPairingKeychainInfoKey_HomeKitRegistered	CFSTR( "homeKitRegistered" )

// Keychain parameters and macros for AirPlay/CarPlay pairing.

#define kPairingKeychainAccessGroup_AirPlay			CFSTR( "com.apple.airplay.pairing" )
#define kPairingKeychainIdentityType_AirPlay		0x41504964 // 'APId'
#define kPairingKeychainIdentityLabel_AirPlay		CFSTR( "AirPlay Pairing Identity" )
#define kPairingKeychainPeerType_AirPlay			0x41505072 // 'APPr'
#define kPairingKeychainPeerLabel_AirPlay			CFSTR( "Paired AirPlay Accessory" )
#define kPairingKeychainFlags_AirPlay				( kPairingKeychainFlag_iCloudIdentity | kPairingKeychainFlag_iCloudPeers )

#define PairingSessionSetKeychainInfo_AirPlay( SESSION ) \
	PairingSessionSetKeychainInfo( (SESSION), \
		kPairingKeychainAccessGroup_AirPlay, \
		kPairingKeychainIdentityType_AirPlay, \
		kPairingKeychainIdentityLabel_AirPlay, \
		kPairingKeychainIdentityLabel_AirPlay, \
		kPairingKeychainPeerType_AirPlay, \
		kPairingKeychainPeerLabel_AirPlay, \
		kPairingKeychainPeerLabel_AirPlay, \
		kPairingKeychainFlags_AirPlay )

// Keychain parameters and macros for HomeKit pairing.

#define kPairingKeychainAccessGroup_HomeKit			CFSTR( "com.apple.hap.pairing" )
#define kPairingKeychainIdentityType_HomeKit		0x68617043 // 'hapC'
#define kPairingKeychainIdentityLabel_HomeKit		CFSTR( "HomeKit Pairing Identity" )
#define kPairingKeychainPeerType_HomeKit			0x68617041 // 'hapA'
#define kPairingKeychainPeerLabel_HomeKit			CFSTR( "Paired HomeKit Accessory" )
#define kPairingKeychainFlags_HomeKit				( kPairingKeychainFlag_iCloudIdentity | kPairingKeychainFlag_iCloudPeers )

#define PairingSessionSetKeychainInfo_HomeKit( SESSION ) \
	PairingSessionSetKeychainInfo( (SESSION), \
		kPairingKeychainAccessGroup_HomeKit, \
		kPairingKeychainIdentityType_HomeKit, \
		kPairingKeychainIdentityLabel_HomeKit, \
		kPairingKeychainIdentityLabel_HomeKit, \
		kPairingKeychainPeerType_HomeKit, \
		kPairingKeychainPeerLabel_HomeKit, \
		kPairingKeychainPeerLabel_HomeKit, \
		kPairingKeychainFlags_HomeKit )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		PairingSessionCreate
	@abstract	Creates a new pairing session. Use CFRelease to release when you're done with it.
*/
typedef struct PairingSessionPrivate *		PairingSessionRef;

CFTypeID	PairingSessionGetTypeID( void );
OSStatus	PairingSessionCreate( PairingSessionRef *outSession, const PairingDelegate *inDelegate, PairingSessionType inType );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionSetFlags
	@abstract	Set flags to control pairing.
*/
void	PairingSessionSetFlags( PairingSessionRef inSession, PairingFlags inFlags );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionSetIdentifier
	@abstract	Sets the identifier for this side of a pairing transaction.
*/
OSStatus	PairingSessionSetIdentifier( PairingSessionRef inSession, const void *inPtr, size_t inLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionSetKeychainInfo
	@abstract	Sets the strings and types used when getting and storing items in the Keychain.
	
	@param		inAccessGroup			Keychain access group to use for accessing items. Defaults to "com.apple.pairing".
	@param		inIdentityType			Type used for programmatic searches of pairing Keychain identities. Defaults to 'prId'.
	@param		inIdentityLabel			Prefix for the label of the identity Keychain item. Defaults to "Pairing Identity".
	@param		inIdentityDescription	Describes how the Keychain identity item is used. Defaults to "Pairing Identity".
	@param		inPeerType				Type used for programmatic searches of pairing Keychain identities. Defaults to 'prId'.
	@param		inPeerLabel				Prefix for the label of the peer Keychain item. Defaults to "Paired Peer".
	@param		inPeerDescription		Describes how Keychain peer item are used. Defaults to "Paired Peer".
	
*/
typedef uint32_t	PairingKeychainFlags;
#define kPairingKeychainFlags_None					0
#define kPairingKeychainFlag_iCloudIdentity			( 1 << 0 ) // Sync identity Keychain item to the iCloud Keychain.
#define kPairingKeychainFlag_iCloudPeers			( 1 << 1 ) // Sync peer Keychain items to the iCloud Keychain.
#define kPairingKeychainFlag_HighSecurity			( 1 << 3 ) // Only allow keys to be accessed when device is unlocked.

void
	PairingSessionSetKeychainInfo( 
		PairingSessionRef		inSession, 
		CFStringRef				inAccessGroup,			// May be NULL to not set.
		uint32_t				inIdentityType,			// May be 0 to not set.
		CFStringRef				inIdentityLabel,		// May be NULL to not set.
		CFStringRef				inIdentityDescription,	// May be NULL to not set.
		uint32_t				inPeerType,				// May be 0 to not set.
		CFStringRef				inPeerLabel,			// May be NULL to not set.
		CFStringRef				inPeerDescription,		// May be NULL to not set.
		PairingKeychainFlags	inFlags );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionSetLogging
	@abstract	Sets the log category to use for HTTP message logging.
*/
void	PairingSessionSetLogging( PairingSessionRef inSession, LogCategory *inLogCategory );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionSetMaxTries
	@abstract	Sets the maximum number of pairing attempts before the system is locked out and requires a reset.
	@discussion	Defaults to 0 to use an exponentially increasing delay instead of a fixed count.
*/
void	PairingSessionSetMaxTries( PairingSessionRef inSession, int inMaxTries );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionSetMTU
	@abstract	Sets the maximum number of bytes in the data output by PairingSessionExchange. Defaults to SIZE_MAX.
*/
OSStatus	PairingSessionSetMTU( PairingSessionRef me, size_t inMTU );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionCopyPeerIdentifier
	@abstract	Returns a malloc'd, null-terminated copy of the peer identifier string or NULL and kNotFoundErr if it's not set.
	@discussion	For clients, the peer identifier is only available after pair-setup has completed successfully.
				For servers, the peer identifier is only available after pair-verify has completed successfully.
*/
char *	PairingSessionCopyPeerIdentifier( PairingSessionRef inSession, size_t *outLen, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionSetSetupCode
	@abstract	Sets the setup code.
*/
OSStatus	PairingSessionSetSetupCode( PairingSessionRef inSession, const void *inPtr, size_t inLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionDeriveKey
	@abstract	Derives an encryption key from the pair-verify shared secret.
*/
OSStatus
	PairingSessionDeriveKey( 
		PairingSessionRef	inSession, 
		const void *		inSaltPtr, 
		size_t				inSaltLen, 
		const void *		inInfoPtr, 
		size_t				inInfoLen, 
		size_t				inKeyLen, 
		uint8_t *			outKey );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionExchange
	@abstract	Exchanges messages with a peer for pairing.
	
	@param		inInputPtr		Input data from the peer.
	@param		inInputLen		Number of bytes of input data.
	@param		outOutputPtr	Malloc'd response data to send to the peer. Caller must free when done.
	@param		outOutputLen	Number of bytes of response data.
	@param		outDone			Receives true if the exchange has completed successfully. Keep calling until error or done.
*/
OSStatus
	PairingSessionExchange( 
		PairingSessionRef	inSession, 
		const void *		inInputPtr, 
		size_t				inInputLen, 
		uint8_t **			outOutputPtr, 
		size_t *			outOutputLen, 
		Boolean *			outDone );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionDeleteIdentity
	@abstract	Deletes the pairing identity. This API is only used by test tools.
*/
OSStatus	PairingSessionDeleteIdentity( PairingSessionRef inSession );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionCopyIdentity
	@abstract	Copies the pairing identity.
	
	@param		inAllowCreate		If true and the identity doesn't exist, it'll be created.
	@param		outIdentifier		Receives malloc'd pointer to NUL-terminated identifier of identity. May be NULL.
	@param		outPK				Receives the 32-byte public key. May be NULL.
	@param		outSK				Optionally receives the 32-byte public key. May be NULL.
*/
OSStatus
	PairingSessionCopyIdentity( 
		PairingSessionRef	inSession, 
		Boolean				inAllowCreate, 
		char **				outIdentifier, 
		uint8_t *			outPK, 
		uint8_t *			outSK );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionCopyPeer
	@abstract	Search for a peer by identifier and returns a copy of it if found.
	@discussion	See kPairingKey_* for descriptions of the keys in each peer dictionary.
*/
CF_RETURNS_RETAINED
CFDictionaryRef
	PairingSessionCopyPeer( 
		PairingSessionRef	inSession, 
		const void *		inIdentifierPtr, 
		size_t				inIdentifierLen, 
		OSStatus *			outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionCopyPeers
	@abstract	Returns an array of peer dictionaries. Returns an empty array if there are no peers.
	@discussion	See kPairingKey_* for descriptions of the keys in each peer dictionary.
*/
CF_RETURNS_RETAINED
CFArrayRef	PairingSessionCopyPeers( PairingSessionRef inSession, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionDeletePeer
	@abstract	Deletes the pairing peer.
	
	@param		inIdentifierPtr		Ptr to UTF-8 of peer to remove. If NULL, all peers are removed.
	@param		inIdentifierLen		Number of bytes in inIdentifierPtr. May be kSizeCString if inIdentifierPtr is NUL terminated.
*/
OSStatus	PairingSessionDeletePeer( PairingSessionRef inSession, const void *inIdentifierPtr, size_t inIdentifierLen );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionFindPeer
	@abstract	Finds a peer in the persistent store.
	
	@param		inIdentifierPtr		Ptr to UTF-8 of peer to find.
	@param		inIdentifierLen		Number of bytes in inIdentifierPtr. May be kSizeCString if inIdentifierPtr is NUL terminated.
	@param		outPK				Receives the 32-byte public key.
*/
OSStatus	PairingSessionFindPeer( PairingSessionRef inSession, const void *inIdentifierPtr, size_t inIdentifierLen, uint8_t *outPK );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionSavePeer
	@abstract	Saves a peer to the persistent store.
	
	@param		inIdentifierPtr		Ptr to UTF-8 of peer to save.
	@param		inIdentifierLen		Number of bytes in inIdentifierPtr. May be kSizeCString if inIdentifierPtr is NUL terminated.
	@param		inPK				Public key of the peer.
*/
OSStatus
	PairingSessionSavePeer( 
		PairingSessionRef	inSession, 
		const void *		inIdentifierPtr, 
		size_t				inIdentifierLen, 
		const uint8_t *		inPK );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingSessionUpdatePeerInfo
	@abstract	Updates the info dictionary for a peer.
*/
OSStatus
	PairingSessionUpdatePeerInfo( 
		PairingSessionRef	inSession, 
		const void *		inIdentifierPtr, 
		size_t				inIdentifierLen, 
		CFDictionaryRef		inInfo );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	PairingUtilsTest
	@abstract	Unit test.
*/
OSStatus	PairingUtilsTest( int inPerf );

#ifdef __cplusplus
}
#endif

#endif // __PairingUtils_h__
