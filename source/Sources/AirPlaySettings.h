/*
	File:    	AirPlaySettings.h
	Package: 	CarPlay Communications Plug-in.
	Abstract: 	n/a 
	Version: 	280.33.8
	
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
	
	Copyright (C) 2010-2015 Apple Inc. All Rights Reserved.
*/

#ifndef	__AirPlaySettings_h__
#define	__AirPlaySettings_h__

#if( CFLITE_ENABLED ) // Note: Can't use CF_HEADER here because we can't rely on CommonServices.h to define it.
	#include <CoreUtils/CFCompat.h>
#else
	#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		AirPlay Preferences
	@abstract	Keys for managing AirPlay preferences.
*/

#define kAirPlayPrefAppID							"com.apple.airplay"
#define kAirPlayNotification_PrefsChanged			"com.apple.airplay.prefsChanged"

#define kAirPlayPrefKey_AudioBufferMainAltWiFiMs	"audioBufferMainAltWiFiMs"			// [Integer] Buffer for main/alt audio in milliseconds for WiFi connections.
#define kAirPlayPrefKey_AudioBufferMainAltWiredMs	"audioBufferMainAltWiredMs"			// [Integer] Buffer for main/alt audio in milliseconds for wired connections.
#define kAirPlayPrefKey_AudioBufferMainHighMs		"audioBufferMainHighMs"				// [Integer] Buffer for high latency main audio in milliseconds.
#define kAirPlayPrefKey_BonjourDisabled				"bonjourDisabled"					// [Boolean] DEBUG: True=Disable Bonjour advertising.
#define kAirPlayPrefKey_CaptureAudio				"captureAudio"						// [Boolean] DEBUG: True=Capture audio to disk.
#define kAirPlayPrefKey_CaptureStream				"captureStream"						// [Boolean] DEBUG: True=Capture sent audio/video to disk.
#define kAirPlayPrefKey_CarPlayDiagnosticModeEnabled	"carPlayDiagnosticModeEnabled"	// [Boolean] True if AirPlay receiver is enabled. False turns off AirPlay.
#define kAirPlayPrefKey_DenyInterruptions			"denyInterruptions"					// [Boolean] True means attempts to AirPlay are rejected if already AirPlaying.
#define kAirPlayPrefKey_Enabled						"enabled"							// [Boolean] True if AirPlay receiver is enabled. False turns off AirPlay.
#define kAirPlayPrefKey_Name						"name"								// [String]  Name to advertise the AirPlay services. Uses system name if missing or empty.
#define kAirPlayPrefKey_OverscanOverride			"overscanOverride"					// [Integer] 0=Overscan off, 1=Overscan on. -1/Missing=Ask TV.
#define kAirPlayPrefKey_P2PAllow					"p2pAllow"							// [Boolean] Allows or prevents P2P interfaces.
#define kAirPlayPrefKey_P2PPair						"p2pPair"							// [Boolean] False=Don't try/require pairing over P2P. True=Initiate/require pairing over P2P.
#define kAirPlayPrefKey_P2PPreferredForAudio		"p2pPreferredForAudio"				// [Boolean[ Prefer P2P over Infra for AirPlay Audio.
#define kAirPlayPrefKey_P2PSolo						"p2pSolo"							// [Boolean] Shows P2P devices even if not found on infrastructure.
#define kAirPlayPrefKey_PairAll						"pairAll"							// [Boolean] False=Don't try/require pairing over non-P2P. True=Initiate/require pairing over non-P2P.
#define kAirPlayPrefKey_PairPIN						"pairPIN"							// [Boolean] PIN required to pair.
#define kAirPlayPrefKey_PairPINClientShadow			"pairPINClientShadow"				// [Boolean] Shadow copy of "pairPIN" for the client side.
#define kAirPlayPrefKey_PairPINServerShadow			"pairPINServerShadow"				// [Boolean] Shadow copy of "pairPIN" for the server side.
#define kAirPlayPrefKey_PINFixed					"pinFixed"							// [Boolean] DEBUG: If non-empty, use this PIN every time for QA automation.
#define kAirPlayPrefKey_PINMode						"pinMode"							// [Boolean] Require a random PIN for each new AirPlay session.
#define kAirPlayPrefKey_PlayPassword				"playPassword"						// [String]  Password to use AirPlay. No password if missing or empty.
#define kAirPlayPrefKey_QoSDisabled					"qosDisabled"						// [Boolean] True to disable all use of QoS by AirPlay.
#define kAirPlayPrefKey_RemoteLog					"remoteLog"							// [Integer] DEBUG: 0=Off, 1=Log every frame, 2=Log every second.
#define kAirPlayPrefKey_RespectVideoTimestamps		"respectVideoTimestamps"			// [Boolean] True=Display at specified time. False=Display immediately.
#define kAirPlayPrefKey_Seed						"seed"								// [Number]  0-255 Seed number that's incremented with each config change.
#define kAirPlayPrefKey_ShowHUD						"showHUD"							// [Boolean] DEBUG: True=Show HUD on screen.
#define kAirPlayPrefKey_TimeoutDataSecs				"timeoutDataSecs"					// [Number]  DEBUG: Override for the normal data timeout.

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

// Hooks

typedef CFTypeRef	( *AirPlaySettings_CopyValueFunc )( CFStringRef inKey, OSStatus *outErr );
extern AirPlaySettings_CopyValueFunc		gAirPlaySettings_CopyValueFunc;

typedef OSStatus	( *AirPlaySettings_SetValueFunc )( CFStringRef inKey, CFTypeRef inValue );
extern AirPlaySettings_SetValueFunc			gAirPlaySettings_SetValueFunc;

typedef OSStatus	( *AirPlaySettings_RemoveValueFunc )( CFStringRef inKey );
extern AirPlaySettings_RemoveValueFunc		gAirPlaySettings_RemoveValueFunc;

// Gets

CFArrayRef	AirPlaySettings_CopyKeys( OSStatus *outErr );
CFTypeRef	AirPlaySettings_CopyValue( CFDictionaryRef *ioSettings, CFStringRef inKey, OSStatus *outErr );
CFTypeRef	AirPlaySettings_CopyValueEx( CFDictionaryRef *ioSettings, CFStringRef inKey, CFTypeID inType, OSStatus *outErr );
#define		AirPlaySettings_CopyCFArray(      IO_SETTINGS, KEY, OUT_ERR )	( (CFArrayRef)      AirPlaySettings_CopyValueEx( (IO_SETTINGS), (KEY), CFArrayGetTypeID(),      OUT_ERR ) )
#define		AirPlaySettings_CopyCFBoolean(    IO_SETTINGS, KEY, OUT_ERR )	( (CFBooleanRef)    AirPlaySettings_CopyValueEx( (IO_SETTINGS), (KEY), CFBooleanGetTypeID(),    OUT_ERR ) )
#define		AirPlaySettings_CopyCFData(       IO_SETTINGS, KEY, OUT_ERR )	( (CFDataRef)       AirPlaySettings_CopyValueEx( (IO_SETTINGS), (KEY), CFDataGetTypeID(),       OUT_ERR ) )
#define		AirPlaySettings_CopyCFDate(       IO_SETTINGS, KEY, OUT_ERR )	( (CFDateRef)       AirPlaySettings_CopyValueEx( (IO_SETTINGS), (KEY), CFDateGetTypeID(),       OUT_ERR ) )
#define		AirPlaySettings_CopyCFDictionary( IO_SETTINGS, KEY, OUT_ERR )	( (CFDictionaryRef) AirPlaySettings_CopyValueEx( (IO_SETTINGS), (KEY), CFDictionaryGetTypeID(), OUT_ERR ) )
#define		AirPlaySettings_CopyCFNumber(     IO_SETTINGS, KEY, OUT_ERR )	( (CFNumberRef)     AirPlaySettings_CopyValueEx( (IO_SETTINGS), (KEY), CFNumberGetTypeID(),     OUT_ERR ) )
#define		AirPlaySettings_CopyCFString(     IO_SETTINGS, KEY, OUT_ERR )	( (CFStringRef)     AirPlaySettings_CopyValueEx( (IO_SETTINGS), (KEY), CFStringGetTypeID(),     OUT_ERR ) )

#define AirPlaySettings_GetBoolean( IO_SETTINGS, KEY, ERR_PTR )	\
	( ( AirPlaySettings_GetInt64( (IO_SETTINGS), (KEY), (ERR_PTR) ) != 0 ) ? true : false )
char *
	AirPlaySettings_GetCString( 
		CFDictionaryRef *	ioSettings, 
		CFStringRef			inKey, 
		char *				inBuf, 
		size_t				inMaxLen, 
		OSStatus *			outErr );
double	AirPlaySettings_GetDouble( CFDictionaryRef *ioSettings, CFStringRef inKey, OSStatus *outErr );
int64_t	AirPlaySettings_GetInt64( CFDictionaryRef *ioSettings, CFStringRef inKey, OSStatus *outErr );

// Sets

#define		AirPlaySettings_SetBoolean( KEY, VALUE ) \
	AirPlaySettings_SetValue( (KEY), (VALUE) ? kCFBooleanTrue : kCFBooleanFalse )
OSStatus	AirPlaySettings_SetCString( CFStringRef inKey, const char *inStr, size_t inLen );
OSStatus	AirPlaySettings_SetDouble( CFStringRef inKey, double inValue );
OSStatus	AirPlaySettings_SetInt64( CFStringRef inKey, int64_t inValue );
OSStatus	AirPlaySettings_SetNumber( CFStringRef inKey, CFNumberType inType, const void *inValue );
OSStatus	AirPlaySettings_SetValue( CFStringRef inKey, CFTypeRef inValue );

OSStatus	AirPlaySettings_RemoveValue( CFStringRef inKey );
void		AirPlaySettings_Synchronize( void );

#ifdef __cplusplus
}
#endif

#endif	// __AirPlaySettings_h__
