/*
	File:    	AirPlayReceiverServerPriv.h
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
	
	Copyright (C) 2012-2015 Apple Inc. All Rights Reserved.
*/

#ifndef	__AirPlayReceiverServerPriv_h__
#define	__AirPlayReceiverServerPriv_h__

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "APAdvertiser.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include "dns_sd.h"
#include LIBDISPATCH_HEADER

#include <CoreUtils/CommonServices.h>
#include <CoreUtils/DataBufferUtils.h>
#include <CoreUtils/DebugServices.h>
#include <CoreUtils/HTTPServer.h>

#if( TARGET_OS_POSIX )
	#include <CoreUtils/NetworkChangeListener.h>
#endif
	#include <CoreUtils/MFiSAP.h>
	#include <CoreUtils/PairingUtils.h>

#ifdef __cplusplus
extern "C" {
#endif

//===========================================================================================================================
//	Server Internals
//===========================================================================================================================

extern AirPlayReceiverServerRef		gAirPlayReceiverServer;
extern AirPlayCompressionType		gAirPlayAudioCompressionType;

struct AirPlayReceiverServerPrivate
{
	CFRuntimeBase						base;			// CF type info. Must be first.
	void *								platformPtr;	// Pointer to the platform-specific data.
	dispatch_queue_t					queue;			// Internal queue used by the server.
	AirPlayReceiverServerDelegate		delegate;		// Hooks for delegating functionality to external code.
	
	// Advertiser
	APAdvertiserRef				advertiser;				// advertiser object
	
	// Bonjour
	
	Boolean						advertisingRestartPending;// True if we're waiting until playback stops to restart advertising.
	
	// Servers

	HTTPServerRef				httpServer;				// HTTP server
	dispatch_queue_t			httpQueue;				// Internal queue used by the http servers and connections.
#if( AIRPLAY_HTTP_SERVER_LEGACY )
	HTTPServerRef				httpServerLegacy;		// HTTP server to support legacy BTLE clients that use hardcoded port 5000.
#endif
	uint8_t						httpTimedNonceKey[ 16 ];
	
	Boolean						playing;				// True if we're currently playing.
	Boolean						serversStarted;			// True if the network servers have been started.
	Boolean						started;				// True if we've been started. Prefs may still disable network servers.
	
	// Settings
	
	Boolean						denyInterruptions;		// True if hijacking is disabled.
	Boolean						deviceActivated;		// True if device is activated.
	uint8_t						deviceID[ 6 ];			// Globally unique device ID (i.e. primary MAC address).
	char						name[ 64 ];				// Name that people will see in the AirPlay pop up (e.g. "Kitchen Apple TV").
	int							overscanOverride;		// -1=Compensate if the TV is overscanned. 0=Don't compensate. 1=Always compensate.
	Boolean						pairAll;				// True if pairing is required for all connections.
	Boolean						pairPIN;				// PIN required to pair.
	char						pairPINStr[ 8 ];		// PIN to use for pairing.
	uint64_t					pairPINLastTicks;		// Last time PIN was generated.
	Boolean						qosDisabled;			// If true, don't use QoS.
	int							timeoutDataSecs;		// Timeout for data (defaults to kAirPlayDataTimeoutSecs).
	CFDictionaryRef				audioStreamOptions;		// Collection of vendor-specific audio stream options.
	CFDictionaryRef				screenStreamOptions;	// Collection of vendor-specific screen stream options.
	char *						configFilePath;			// Path to airplay.conf/airplay.ini
	CFDictionaryRef				config;
};
	
typedef struct  AirPlayReceiverLogsPrivate  *	AirPlayReceiverLogsRef;
struct AirPlayReceiverLogsPrivate
{
	CFRuntimeBase				base;			// CF type info. Must be first.
	AirPlayReceiverServerRef	server;			// Server that initiated the request.
	Boolean						pending;		// True if log retrieval is in progress.
	int							status;			// Status of the most recently completed logs request.
	unsigned int				requestID;		// Unique ID of the current logs request.
	DataBuffer *				dataBuffer;		// Buffer to store the current logs request result.
};

struct AirPlayReceiverConnectionPrivate
{
	AirPlayReceiverServerRef	server;
	HTTPConnectionRef			httpCnx;			// Underlying HTTP connection for this connection.
	AirPlayReceiverSessionRef	session;			// Session this connection is associated with.
	
#if( TARGET_OS_POSIX )
	NetworkChangeListenerRef	networkChangeListener;
#endif
	
	uint64_t					clientDeviceID;
	uint8_t						clientInterfaceMACAddress[ 6 ];
	char						clientName[ 128 ];
	uint64_t					clientSessionID;
	uint32_t					clientVersion;
	Boolean						didAnnounce;
	Boolean						didAudioSetup;
	Boolean						didScreenSetup;
	Boolean						didRecord;
	
	char						ifName[ IF_NAMESIZE + 1 ];	// Name of the interface the connection was accepted on.
	
	Boolean						httpAuthentication_IsAuthenticated;
	
	AirPlayCompressionType		compressionType;
	uint32_t					framesPerPacket;
	
	AirPlayReceiverLogsRef		logs;
	
	MFiSAPRef					MFiSAP;
	Boolean						MFiSAPDone;
	PairingSessionRef			pairSetupSessionHomeKit;
	PairingSessionRef			pairVerifySessionHomeKit;

	Boolean						pairingVerified;
	int							pairingCount;
	int							pairDerive;
	uint8_t						encryptionKey[ 16 ];
	uint8_t						encryptionIV[ 16 ];
	Boolean						usingEncryption;
	
	uint32_t					minLatency, maxLatency;
	
};

#if 0
#pragma mark
#pragma mark == Server Utils ==
#endif

//===========================================================================================================================
//	Server Utils
//===========================================================================================================================

CF_RETURNS_RETAINED CFDictionaryRef
	AirPlayCopyServerInfo(
		AirPlayReceiverSessionRef inSession,
		CFArrayRef inProperties,
		uint8_t *inMACAddr,
		OSStatus *outErr );

uint64_t			AirPlayGetDeviceID( uint8_t *outDeviceID );
OSStatus			AirPlayGetDeviceName( char *inBuffer, size_t inMaxLen );
AirPlayFeatures		AirPlayGetFeatures( void );
OSStatus			AirPlayGetMinimumClientOSBuildVersion( char *inBuffer, size_t inMaxLen );
	OSStatus		AirPlayCopyHomeKitPairingIdentity( char ** outIdentifier, uint8_t outPK[ 32 ] );
AirPlayStatusFlags	AirPlayGetStatusFlags( void );
void				AirPlayReceiverServerSetLogLevel( void );
void				AirPlayReceiverServerSetLogPath( void );

#ifdef __cplusplus
}
#endif

#endif // __AirPlayReceiverServerPriv_h__
