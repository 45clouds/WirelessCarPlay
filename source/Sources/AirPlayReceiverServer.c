/*
	File:    	AirPlayReceiverServer.c
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

#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverServerPriv.h"

#include <CoreUtils/Base64Utils.h>
#include <CoreUtils/NetTransportChaCha20Poly1305.h>
#include <CoreUtils/NetUtils.h>
#include <CoreUtils/RandomNumberUtils.h>
#include <CoreUtils/StringUtils.h>
#include <CoreUtils/SystemUtils.h>
#include <CoreUtils/ThreadUtils.h>
#include <CoreUtils/TickUtils.h>
#include <CoreUtils/UUIDUtils.h>

#include "AirPlayCommon.h"
#include "AirPlaySettings.h"
#include "AirPlayReceiverSession.h"
#include "AirPlayReceiverSessionPriv.h"
#include "AirPlayUtils.h"
#include "AirPlayVersion.h"

	#include "APAdvertiser.h"
	#include "APAdvertiserInfo.h"
#if( 0 || ( TARGET_OS_POSIX && DEBUG ) )
	#include <CoreUtils/DebugIPCUtils.h>
#endif
#if( TARGET_OS_POSIX )
	#include <signal.h>
#endif

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include "dns_sd.h"
#include LIBDISPATCH_HEADER

	#include <CoreUtils/HIDUtils.h>
	#include <CoreUtils/ScreenUtils.h>
#if( TARGET_OS_BSD )
	#include <net/if_media.h>
	#include <net/if.h>
#endif
	#include <CoreUtils/MFiSAP.h>
	#include <CoreUtils/PairingUtils.h>

typedef enum
{
	kAirPlayHTTPConnectionTypeAny	= 0,
	kAirPlayHTTPConnectionTypeAudio	= 1,
}	AirPlayHTTPConnectionTypes;

#if 0
#pragma mark == Prototypes ==
#endif
#include <glib.h>
//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static void	_FinalizeLogs( CFTypeRef inCF );

	static char *
		AirPlayGetConfigCString( 
			CFStringRef			inKey,
			char *				inBuffer, 
			size_t				inMaxLen, 
			OSStatus *			outErr );
	static void _ProcessLimitedUIElementsConfigArray( const void *value, void *context );
	static void _ProcessLimitedUIElementsConfigArrayDictionaryEntry( const void *key, const void *value, void *context );
static OSStatus	AirPlayGetDeviceModel( char *inBuffer, size_t inMaxLen );

static void _GetTypeID( void *inContext );
static void	_GlobalInitialize( void *inContext );

#if( 0 || ( TARGET_OS_POSIX && DEBUG ) )
	static OSStatus	_HandleDebug( CFDictionaryRef inRequest, CFDictionaryRef *outResponse, void *inContext );
	static OSStatus	AirPlayReceiverServerDebugShow( AirPlayReceiverServerRef inServer, int inVerbose, DataBuffer *inDataBuf );
	typedef struct
	{
		AirPlayReceiverServerRef		server;
		int								verbose;
		DataBuffer *					dataBuf;
	} AirPlayReceiverServerDebugShowContext;
	static void	_AirPlayReceiverServerDebugShow( void *inContext );
#endif

	static OSStatus	_InitializeConfig( AirPlayReceiverServerRef inServer );
static void	_Finalize( CFTypeRef inCF );
static void	_UpdatePrefs( AirPlayReceiverServerRef inServer );

static OSStatus _CreateAdvertiserInfo( AirPlayReceiverServerRef inServer, APAdvertiserInfoRef *outAdvertiserInfo );
static OSStatus	_RestartAdvertising( AirPlayReceiverServerRef inServer );
static OSStatus	_EnsureStartedAdvertising( AirPlayReceiverServerRef inServer );
static OSStatus	_EnsureStoppedAdvertising( AirPlayReceiverServerRef inServer, const char *inReason );
static OSStatus	_UpdateAdvertising( AirPlayReceiverServerRef inServer );

static HTTPServerRef _CreateHTTPServerForPort( AirPlayReceiverServerRef inServer, int inListenPort );
static void	_StartServers( AirPlayReceiverServerRef inServer );
static void	_StopServers( AirPlayReceiverServerRef inServer );
static void	_UpdateServers( AirPlayReceiverServerRef inServer );

static Boolean _IsConnectionActive( HTTPConnectionRef inConnection );
static HTTPConnectionRef _FindActiveConnection( AirPlayReceiverServerRef inServer );
static void _HijackConnections( AirPlayReceiverServerRef inServer, HTTPConnectionRef inHijacker );
static void _RemoveAllConnections( AirPlayReceiverServerRef inServer );
static void	_DestroyConnection( HTTPConnectionRef inCnx );
static int	_GetListenPort( AirPlayReceiverServerRef inServer );

static void
	_HandleHTTPConnectionCreated( 
		HTTPServerRef		inServer, 
		HTTPConnectionRef	inCnx, 
		void *				inCnxExtraPtr, 
		void *				inContext );
static OSStatus	_HandleHTTPConnectionInitialize( HTTPConnectionRef inCnx, void *inContext );
static void		_HandleHTTPConnectionFinalize( HTTPConnectionRef inCnx, void *inContext );
static void		_HandleHTTPConnectionClose( HTTPConnectionRef inCnx, void *inContext );
static OSStatus	_HandleHTTPConnectionMessage( HTTPConnectionRef inCnx, HTTPMessageRef inMsg, void *inContext );

	static HTTPStatus	_requestProcessAuthSetup( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessCommand( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessFeedback( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );
	static HTTPStatus	_requestProcessFlush( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessGetLog( AirPlayReceiverConnectionRef inCnx );
	static HTTPStatus	_requestProcessGetLogs( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest );
static HTTPStatus	_requestProcessInfo( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessOptions( AirPlayReceiverConnectionRef inCnx );
	static HTTPStatus	_requestProcessPairSetup( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );
	static HTTPStatus	_requestProcessPairVerify( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessGetParameter( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessSetParameter( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessSetParameterText( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessSetProperty( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessRecord( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessSetup( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessSetupPlist( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessTearDown( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );

static OSStatus		_requestCreateSession( AirPlayReceiverConnectionRef inCnx, Boolean inUseEvents );
static OSStatus
	_requestDecryptKey(
		AirPlayReceiverConnectionRef	inCnx,
		CFDictionaryRef					inRequestParams,
		AirPlayEncryptionType			inType,
		uint8_t							inKeyBuf[ 16 ],
		uint8_t							inIVBuf[ 16 ] );
	static void			_requestReportIfIncompatibleSender( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus
	_requestSendPlistResponse(
		HTTPConnectionRef	inCnx,
		HTTPMessageRef		inMsg,
		CFPropertyListRef	inPlist,
		OSStatus *			outErr );
#if( TARGET_OS_POSIX )
	static void			_requestNetworkChangeListenerHandleEvent( uint32_t inEvent, void *inContext );
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static dispatch_once_t			gAirPlayReceiverServerInitOnce	= 0;
static CFTypeID					gAirPlayReceiverServerTypeID	= _kCFRuntimeNotATypeID;
AirPlayReceiverServerRef		gAirPlayReceiverServer			= NULL;
static const CFRuntimeClass		kAirPlayReceiverServerClass = 
{
	0,							// version
	"AirPlayReceiverServer",	// className
	NULL,						// init
	NULL,						// copy
	_Finalize,					// finalize
	NULL,						// equal -- NULL means pointer equality.
	NULL,						// hash  -- NULL means pointer hash.
	NULL,						// copyFormattingDesc
	NULL,						// copyDebugDesc
	NULL,						// reclaim
	NULL						// refcount
};

static dispatch_once_t			gAirPlayReceiverLogsInitOnce	= 0;
static CFTypeID					gAirPlayReceiverLogsTypeID		= _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kAirPlayReceiverLogsClass =
{
	0,							// version
	"AirPlayReceiverLogs",		// className
	NULL,						// init
	NULL,						// copy
	_FinalizeLogs,				// finalize
	NULL,						// equal -- NULL means pointer equality.
	NULL,						// hash  -- NULL means pointer hash.
	NULL,						// copyFormattingDesc
	NULL,						// copyDebugDesc
	NULL,						// reclaim
	NULL						// refcount
};

static dispatch_once_t			gAirPlayReceiverInitOnce		= 0;

AirPlayCompressionType				gAirPlayAudioCompressionType	= kAirPlayCompressionType_Undefined;

	#define PairingDescription( cnx )				( (cnx)->pairingVerified ? " (Paired)" : "" )
	
		#define PairingRequired( inCnx, aprCnx )		( (aprCnx)->server->pairAll )

	#define AirPlayReceiverConnectionDidSetup( aprCnx )	( (aprCnx)->didAudioSetup || (aprCnx)->didScreenSetup )

// sync extension for HTTPServer. 
#define HTTPServerControlSync( SERVER, FLAGS, COMMAND, PARAMS ) \
	CFObjectSetProperty( (SERVER), (SERVER)->queue, _HTTPServerControl, (FLAGS), \
		(COMMAND), NULL, (PARAMS) )
		
#define HTTPServerStartSync( X )			HTTPServerControlSync( (X), 0, kHTTPServerCommand_StartServer, NULL )
#define HTTPServerStopSync( X )				HTTPServerControlSync( (X), 0, kHTTPServerCommand_StopServer, NULL )

ulog_define( AirPlayReceiverServer, kLogLevelNotice, kLogFlags_Default, "AirPlay",  NULL );
#define aprs_ucat()					&log_category_from_name( AirPlayReceiverServer )
#define aprs_ulog( LEVEL, ... )		ulog( aprs_ucat(), (LEVEL), __VA_ARGS__ )
#define aprs_dlog( LEVEL, ... )		dlogc( aprs_ucat(), (LEVEL), __VA_ARGS__ )

ulog_define( AirPlayReceiverServerHTTP, kLogLevelNotice, kLogFlags_Default, "AirPlay",  NULL );
#define aprs_http_ucat()					&log_category_from_name( AirPlayReceiverServerHTTP )
#define aprs_http_ulog( LEVEL, ... )		ulog( aprs_http_ucat(), (LEVEL), __VA_ARGS__ )


//===========================================================================================================================
//	AirPlayCopyServerInfo
//===========================================================================================================================

CFDictionaryRef
	AirPlayCopyServerInfo(
		AirPlayReceiverSessionRef inSession,
		CFArrayRef inProperties,
		uint8_t *inMACAddr,
		OSStatus *outErr )
{
	CFDictionaryRef				result = NULL;
	OSStatus					err;
	CFMutableDictionaryRef		info;
	uint8_t						buf[ 32 ];
	char						macStr[ 128 ];
	char						str[ PATH_MAX + 1 ];
	uint64_t					u64;
	CFTypeRef					obj;
	CFDataRef					data;
	char *						cptr;
	
	info = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( info, exit, err = kNoMemoryErr );
	
	// AudioFormats
	
	obj = CFDictionaryGetValue( gAirPlayReceiverServer->config, CFSTR( kAirPlayProperty_AudioFormats ) );
	CFRetainNullSafe( obj );
	if( !obj )
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
			CFSTR( kAirPlayProperty_AudioFormats ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayProperty_AudioFormats ), obj );
		CFRelease( obj );
	}
	
	// AudioLatencies
	
	obj = CFDictionaryGetValue( gAirPlayReceiverServer->config, CFSTR( kAirPlayProperty_AudioLatencies ) );
	CFRetainNullSafe( obj );
	if( !obj )
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
														CFSTR( kAirPlayProperty_AudioLatencies ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayProperty_AudioLatencies ), obj );
		CFRelease( obj );
	}
	
	// BluetoothIDs
	
	obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect, 
		CFSTR( kAirPlayProperty_BluetoothIDs ), NULL, NULL );
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayProperty_BluetoothIDs ), obj );
		CFRelease( obj );
	}
	
	// DeviceID
	
	AirPlayGetDeviceID( buf );
	MACAddressToCString( buf, str );
	CFDictionarySetCString( info, CFSTR( kAirPlayKey_DeviceID ), str, kSizeCString );
	
	// Displays
	
	if( inSession )
	{
		obj = AirPlayReceiverSessionPlatformCopyProperty( inSession, kCFObjectFlagDirect, 
			CFSTR( kAirPlayProperty_Displays ), NULL, NULL );
		if( obj )
		{
			CFDictionarySetValue( info, CFSTR( kAirPlayKey_Displays ), obj );
			CFRelease( obj );
		}
	}
	
	// Extended Features
	
	obj = CFDictionaryGetValue( gAirPlayReceiverServer->config, CFSTR( kAirPlayProperty_ExtendedFeatures ) );
	CFRetainNullSafe( obj );
	if( !obj )
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
														CFSTR( kAirPlayProperty_ExtendedFeatures ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayProperty_ExtendedFeatures ), obj );
		CFRelease( obj );
	}
	
	// Features
	
	u64 = AirPlayGetFeatures();
	if( u64 != 0 ) CFDictionarySetInt64( info, CFSTR( kAirPlayKey_Features ), u64 );
	
	// FirmwareRevision
	
	obj = NULL;
	*str = '\0';
	AirPlayGetConfigCString( CFSTR( kAirPlayKey_FirmwareRevision ), str, sizeof( str ), NULL );
	if( *str != '\0' ) CFDictionarySetCString( info, CFSTR( kAirPlayKey_FirmwareRevision ), str, kSizeCString );
	else
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect, 
			CFSTR( kAirPlayKey_FirmwareRevision ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_FirmwareRevision ), obj );
		CFRelease( obj );
	}
	
	// HardwareRevision
	
	obj = NULL;
	*str = '\0';
	AirPlayGetConfigCString( CFSTR( kAirPlayKey_HardwareRevision ), str, sizeof( str ), NULL );
	if( *str != '\0' ) CFDictionarySetCString( info, CFSTR( kAirPlayKey_HardwareRevision ), str, kSizeCString );
	else
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect, 
			CFSTR( kAirPlayKey_HardwareRevision ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_HardwareRevision ), obj );
		CFRelease( obj );
	}
	
	// HID
	
	if( inSession )
	{
		obj = AirPlayReceiverSessionPlatformCopyProperty( inSession, kCFObjectFlagDirect, 
			CFSTR( kAirPlayProperty_HIDDevices ), NULL, NULL );
		if( obj )
		{
			CFDictionarySetValue( info, CFSTR( kAirPlayKey_HIDDevices ), obj );
			CFRelease( obj );
		}
	}
	
	// HIDLanguages
	
	obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
		CFSTR( kAirPlayKey_HIDLanguages ), NULL, NULL );
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_HIDLanguages ), obj );
		CFRelease( obj );
	}
	
	// Interface MAC Address
	
	if( inMACAddr )
	{
		MACAddressToCString( inMACAddr, macStr );
		CFDictionarySetCString( info, CFSTR( kAirPlayKey_MACAddress ), macStr, kSizeCString );
	}
	
	// Supports statistics as part of the keep alive body. 
	
	CFDictionarySetBoolean( info, CFSTR( kAirPlayKey_KeepAliveSendStatsAsBody ), true );
	
	// Supports receiving UDP beacon as keep alive.

	CFDictionarySetBoolean( info, CFSTR( kAirPlayKey_KeepAliveLowPower ), true );

	// LimitedUIElements
	
	obj = CFDictionaryGetValue( gAirPlayReceiverServer->config, CFSTR( kAirPlayKey_LimitedUIElements ) );
	if( obj )
	{
		if( CFIsType( obj, CFArray ) )
		{
			CFMutableArrayRef	elements = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			CFArrayApplyFunction( (CFArrayRef) obj, CFRangeMake( 0, CFArrayGetCount( (CFArrayRef) obj ) ),
				_ProcessLimitedUIElementsConfigArray, elements);
			obj = elements;
		}
		else
		{
			obj = NULL;
		}
	}
	if( !obj )
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
			CFSTR( kAirPlayKey_LimitedUIElements ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_LimitedUIElements ), obj );
		CFRelease( obj );
	}
	
	// LimitedUI
	obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
		CFSTR( kAirPlayKey_LimitedUI ), NULL, NULL );
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_LimitedUI ), obj );
		CFRelease( obj );
	}
	
	// Manufacturer
	
	obj = NULL;
	*str = '\0';
	AirPlayGetConfigCString( CFSTR( kAirPlayKey_Manufacturer ), str, sizeof( str ), NULL );
	if( *str != '\0' ) CFDictionarySetCString( info, CFSTR( kAirPlayKey_Manufacturer ), str, kSizeCString );
	else
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect, 
			CFSTR( kAirPlayKey_Manufacturer ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_Manufacturer ), obj );
		CFRelease( obj );
	}
	
	// Model
	
	obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
		CFSTR( kAirPlayKey_Model ), NULL, NULL );
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_Model ), obj );
		CFRelease( obj );
	}
	else
	{
		*str = '\0';
		AirPlayGetDeviceModel( str, sizeof( str ) );
		CFDictionarySetCString( info, CFSTR( kAirPlayKey_Model ), str, kSizeCString );
	}
	
	// Modes
	
	if( inSession )
	{
		obj = AirPlayReceiverSessionPlatformCopyProperty( inSession, kCFObjectFlagDirect, 
			CFSTR( kAirPlayProperty_Modes ), NULL, NULL );
		if( obj )
		{
			CFDictionarySetValue( info, CFSTR( kAirPlayProperty_Modes ), obj );
			CFRelease( obj );
		}
	}
	
	// Name
	
	*str = '\0';
	AirPlayGetDeviceName( str, sizeof( str ) );
	CFDictionarySetCString( info, CFSTR( kAirPlayKey_Name ), str, kSizeCString );
	
	// ProtocolVersion
	
	if( strcmp( kAirPlayProtocolVersionStr, "1.0" ) != 0 )
	{
		CFDictionarySetCString( info, CFSTR( kAirPlayKey_ProtocolVersion ), kAirPlayProtocolVersionStr, kSizeCString );
	}
	
	// NightMode

	obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
		CFSTR( kAirPlayKey_NightMode ), NULL, NULL );
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_NightMode ), obj );
		CFRelease( obj );
	}
	
	// OEMIcon
	
	if( inSession )
	{
		obj = NULL;
		*str = '\0';
		AirPlayGetConfigCString( CFSTR( kAirPlayProperty_OEMIconPath ), str, sizeof( str ), NULL );
		if( *str != '\0' ) obj = CFDataCreateWithFilePath( str, NULL );
		if( !obj )
		{
			obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
				CFSTR( kAirPlayProperty_OEMIcon ), NULL, NULL );
		}
		if( obj )
		{
			CFDictionarySetValue( info, CFSTR( kAirPlayProperty_OEMIcon ), obj );
			CFRelease( obj );
		}
		
		obj = CFDictionaryGetValue( gAirPlayReceiverServer->config, CFSTR( kAirPlayProperty_OEMIcons ) );
		CFRetainNullSafe( obj );
		if( !obj )
		{
			obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
				CFSTR( kAirPlayProperty_OEMIcons ), NULL, NULL );
		}
		if( obj )
		{
			CFDictionarySetValue( info, CFSTR( kAirPlayProperty_OEMIcons ), obj );
			CFRelease( obj );
		}
		
		obj = NULL;
		*str = '\0';
		AirPlayGetConfigCString( CFSTR( kAirPlayProperty_OEMIconLabel ), str, sizeof( str ), NULL );
		if( *str != '\0' ) CFDictionarySetCString( info, CFSTR( kAirPlayProperty_OEMIconLabel ), str, kSizeCString );
		else
		{
			obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
				CFSTR( kAirPlayProperty_OEMIconLabel ), NULL, NULL );
		}
		if( obj )
		{
			CFDictionarySetValue( info, CFSTR( kAirPlayProperty_OEMIconLabel ), obj );
			CFRelease( obj );
		}

		obj = CFDictionaryGetValue( gAirPlayReceiverServer->config, CFSTR( kAirPlayProperty_OEMIconVisible ) );
		CFRetainNullSafe( obj );
		if( !obj )
		{
			obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
															CFSTR( kAirPlayProperty_OEMIconVisible ), NULL, NULL );
		}
		if( obj )
		{
			CFDictionarySetValue( info, CFSTR( kAirPlayProperty_OEMIconVisible ), obj );
			CFRelease( obj );
		}
	}
	
	// OS Info
		
	obj = CFDictionaryGetValue( gAirPlayReceiverServer->config, CFSTR( kAirPlayKey_OSInfo ) );
	if( CFIsType( obj, CFString ) ) CFRetainNullSafe( obj );
	else obj = NULL;
	if( !obj )
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
			CFSTR( kAirPlayKey_OSInfo ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_OSInfo ), obj );
		CFRelease( obj );
	}

	err = AirPlayCopyHomeKitPairingIdentity( &cptr, NULL );
	if( !err )	
	{
		CFDictionarySetCString( info, CFSTR( kAirPlayKey_PublicHKID ), cptr, kSizeCString );
		free( cptr );
	}
	
	// RightHandDrive
	
	obj = CFDictionaryGetValue( gAirPlayReceiverServer->config, CFSTR( kAirPlayKey_RightHandDrive ) );
	CFRetainNullSafe( obj );
	if( !obj )
	{
		obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
			CFSTR( kAirPlayKey_RightHandDrive ), NULL, NULL );
	}
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_RightHandDrive ), obj );
		CFRelease( obj );
	}
	
	// SourceVersion
	
	CFDictionarySetValue( info, CFSTR( kAirPlayKey_SourceVersion ), CFSTR( kAirPlaySourceVersionStr ) );
	
	// StatusFlags
	
	u64 = AirPlayGetStatusFlags();
	if( u64 != 0 ) CFDictionarySetInt64( info, CFSTR( kAirPlayKey_StatusFlags ), u64 );
	
	// TXT records
	
	if( inProperties )
	{
		CFRange		range;
		
		range = CFRangeMake( 0, CFArrayGetCount( inProperties ) );
		if( CFArrayContainsValue( inProperties, range, CFSTR( kAirPlayKey_TXTAirPlay ) ) )
		{
			APAdvertiserInfoRef advertiserInfo = NULL;
			data = NULL;
			
			err = APAdvertiserCopyProperty( gAirPlayReceiverServer->advertiser, kAPAdvertiserProperty_AdvertiserInfo,
				kCFAllocatorDefault, &advertiserInfo );
			require_noerr( err, exit );
            require_action( advertiserInfo, exit, err = kInternalErr );
			
			err = APAdvertiserInfoCopyAirPlayData( advertiserInfo, &data );
			require_noerr_action( err, exit, CFReleaseNullSafe( advertiserInfo ) );

			if( data )
			{
				CFDictionarySetValue( info, CFSTR( kAirPlayKey_TXTAirPlay ), data );
				CFRelease( data );
			}
			CFReleaseNullSafe( advertiserInfo );
		}
	}
	
	// Vehicle information.
	
	obj = AirPlayReceiverServerPlatformCopyProperty( gAirPlayReceiverServer, kCFObjectFlagDirect,
		CFSTR( kAirPlayKey_VehicleInformation ), NULL, NULL );
	if( obj )
	{
		CFDictionarySetValue( info, CFSTR( kAirPlayKey_VehicleInformation ), obj );
		CFRelease( obj );
	}
	
	result = info;
	info = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( info );
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	AirPlayGetConfigCString
//===========================================================================================================================

static char *
	AirPlayGetConfigCString( 
		CFStringRef			inKey,
		char *				inBuffer, 
		size_t				inMaxLen, 
		OSStatus *			outErr )
{
	char *				result = "";
	OSStatus			err;
	
	require_action( gAirPlayReceiverServer, exit, err = kNotInitializedErr );
	
#if( defined( AIRPLAY_FIRMWARE_REVISION ) )
	if( CFEqual( inKey, CFSTR( kAirPlayKey_FirmwareRevision ) ) )
	{
		strlcpy( inBuffer, AIRPLAY_FIRMWARE_REVISION, inMaxLen );
		if( inMaxLen > 0 ) result = inBuffer;
		err = kNoErr;
		goto exit;
	}
#endif
#if( defined( AIRPLAY_HARDWARE_REVISION ) )
	if( CFEqual( inKey, CFSTR( kAirPlayKey_HardwareRevision ) ) )
	{
		strlcpy( inBuffer, AIRPLAY_HARDWARE_REVISION, inMaxLen );
		if( inMaxLen > 0 ) result = inBuffer;
		err = kNoErr;
		goto exit;
	}
#endif
#if( defined( AIRPLAY_MANUFACTURER ) )
	if( CFEqual( inKey, CFSTR( kAirPlayKey_Manufacturer ) ) )
	{
		strlcpy( inBuffer, AIRPLAY_MANUFACTURER, inMaxLen );
		if( inMaxLen > 0 ) result = inBuffer;
		err = kNoErr;
		goto exit;
	}
#endif
#if( defined( AIRPLAY_OEM_ICON_PATH ) )
	if( CFEqual( inKey, CFSTR( kAirPlayProperty_OEMIconPath ) ) )
	{
		strlcpy( inBuffer, AIRPLAY_OEM_ICON_PATH, inMaxLen );
		if( inMaxLen > 0 ) result = inBuffer;
		err = kNoErr;
		goto exit;
	}
#endif
#if( defined( AIRPLAY_OEM_ICON_LABEL ) )
	if( CFEqual( inKey, CFSTR( kAirPlayProperty_OEMIconLabel ) ) )
	{
		strlcpy( inBuffer, AIRPLAY_OEM_ICON_LABEL, inMaxLen );
		if( inMaxLen > 0 ) result = inBuffer;
		err = kNoErr;
		goto exit;
	}
#endif
	
	require_action_quiet( gAirPlayReceiverServer->config, exit, err = kNotFoundErr );
	
	result = CFDictionaryGetCString( gAirPlayReceiverServer->config, inKey, inBuffer, inMaxLen, &err );
	require_noerr_quiet( err, exit );
	
exit:
	if( outErr )		*outErr = err;
	return( result );
}

//===========================================================================================================================
//	_ProcessLimitedUIElementsConfigArray
//===========================================================================================================================

static void _ProcessLimitedUIElementsConfigArray( const void *value, void *context )
{
	// If the config came from an INI file, the array element(s) will be dictionaries, otherwise they will be strings
	CFMutableArrayRef elements = (CFMutableArrayRef) context;
	if( CFIsType( (CFTypeRef) value, CFDictionary ) )
		CFDictionaryApplyFunction( (CFDictionaryRef) value, _ProcessLimitedUIElementsConfigArrayDictionaryEntry, context );
	else if( CFIsType( (CFTypeRef) value, CFString ) )
		CFArrayAppendValue( elements, value );
}

//===========================================================================================================================
//	_ProcessLimitedUIElementsConfigArrayDictionaryEntry
//===========================================================================================================================

static void _ProcessLimitedUIElementsConfigArrayDictionaryEntry( const void *key, const void *value, void *context )
{
	// If the config came from an INI file, the dictionary key will be the element and the value will be "0" or "1"
	CFMutableArrayRef elements = (CFMutableArrayRef) context;
	if( CFIsType( (CFTypeRef) value, CFString ) && CFStringGetIntValue( (CFStringRef) value) )
		CFArrayAppendValue( elements, key );
}

//===========================================================================================================================
//	AirPlayGetDeviceID
//===========================================================================================================================

uint64_t	AirPlayGetDeviceID( uint8_t *outDeviceID )
{
	if( outDeviceID )	memcpy( outDeviceID, gAirPlayReceiverServer->deviceID, 6 );
	return( ReadBig48( gAirPlayReceiverServer->deviceID ) );
}

//===========================================================================================================================
//	AirPlayGetDeviceName
//===========================================================================================================================

OSStatus	AirPlayGetDeviceName( char *inBuffer, size_t inMaxLen )
{
	OSStatus		err;
	
	require_action( inMaxLen > 0, exit, err = kSizeErr );
	
	*inBuffer = '\0';
		strlcpy( inBuffer, "CarPlay", inMaxLen );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayGetDeviceModel
//===========================================================================================================================

static OSStatus	AirPlayGetDeviceModel( char *inBuffer, size_t inMaxLen )
{
	OSStatus		err;
	
	require_action( inMaxLen > 0, exit, err = kSizeErr );
	
	*inBuffer = '\0';
	AirPlayGetConfigCString( CFSTR( kAirPlayKey_Model ), inBuffer, inMaxLen, NULL );
#if( defined( AIRPLAY_DEVICE_MODEL ) )
	if( *inBuffer == '\0' ) strlcpy( inBuffer, AIRPLAY_DEVICE_MODEL, inMaxLen );	// Use hard-coded model if provided.
#endif
	if( *inBuffer == '\0' ) strlcpy( inBuffer, "AirPlayGeneric1,1", inMaxLen );		// If no model, use a default.
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayGetFeatures
//===========================================================================================================================

AirPlayFeatures	AirPlayGetFeatures( void )
{
	AirPlayFeatures		features = 0;
	OSStatus			err;
	Boolean				b;
	
	// Mark as used to avoid special case for conditionalized cases.
	
	(void) err;
	(void) b;
	
	features |= kAirPlayFeature_Audio;
	features |= kAirPlayFeature_AudioAES_128_MFi_SAPv1;
	features |= kAirPlayFeature_AudioPCM;
	features |= kAirPlayFeature_AudioAAC_LC;
	features |= kAirPlayFeature_AudioUnencrypted;
	features |= kAirPlayFeature_Screen;
	features |= kAirPlayFeature_Rotate;
	features |= kAirPlayFeature_UnifiedBonjour;
	{
		features |= kAirPlayFeature_Car;
		features |= kAirPlayFeature_CarPlayControl;
	}
	{
		features |= kAirPlayFeature_HKPairingAndEncrypt;
	}
	
	features |= AirPlayReceiverServerPlatformGetInt64( gAirPlayReceiverServer, CFSTR( kAirPlayProperty_Features ), NULL, NULL );
	return( features );
}

//===========================================================================================================================
//	AirPlayGetMinimumClientOSBuildVersion
//===========================================================================================================================

OSStatus	AirPlayGetMinimumClientOSBuildVersion( char *inBuffer, size_t inMaxLen )
{
	OSStatus	err;
	
	AirPlayGetConfigCString( CFSTR( kAirPlayKey_ClientOSBuildVersionMin ), inBuffer, inMaxLen, &err );
	
	return( err );
}

//===========================================================================================================================
//	AirPlayGetPairingPublicKeyID
//===========================================================================================================================

OSStatus	AirPlayCopyHomeKitPairingIdentity( char ** outIdentifier, uint8_t outPK[ 32 ] )
{
	OSStatus				err;
	PairingSessionRef		pairingSession;
	
	err = PairingSessionCreate( &pairingSession, NULL, kPairingSessionType_None );
	require_noerr( err, exit );
	
	PairingSessionSetKeychainInfo_AirPlay( pairingSession );
	PairingSessionSetLogging( pairingSession, aprs_ucat() );
	err = PairingSessionCopyIdentity( pairingSession, true, outIdentifier, outPK, NULL );
	CFRelease( pairingSession );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayGetStatusFlags
//===========================================================================================================================

AirPlayStatusFlags	AirPlayGetStatusFlags( void )
{
	AirPlayStatusFlags		flags = 0;
	
	if( gAirPlayReceiverServer->pairPIN ) flags |= kAirPlayStatusFlag_PairPIN;
	flags |= (AirPlayStatusFlags) AirPlayReceiverServerPlatformGetInt64( gAirPlayReceiverServer, 
		CFSTR( kAirPlayProperty_StatusFlags ), NULL, NULL );
	return( flags );
}

//===========================================================================================================================
//	AirPlayReceiverServerSetLogLevel
//===========================================================================================================================

void AirPlayReceiverServerSetLogLevel( void )
{
#if( DEBUG || 0 )
	LogControl(
               "?AirPlayReceiverCore:level=info"
               ",AirPlayScreenServerCore:level=trace"
               ",AirPlayReceiverSessionScreenCore:level=trace"
               ",AirTunesServerCore:level=info"
               );
	dlog_control( "?DebugServices.*:level=info" );
#endif
}

//===========================================================================================================================
//	AirPlayReceiverServerSetLogPath
//===========================================================================================================================

void AirPlayReceiverServerSetLogPath( void )
{
#if( AIRPLAY_LOG_TO_FILE_PRIMARY )
	LogControl( kAirPlayPrimaryLogConfig );
#elif( AIRPLAY_LOG_TO_FILE_SECONDARY )
	LogControl( kAirPlaySecondaryLogConfig );
#endif
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	AirPlayReceiverServerGetTypeID
//===========================================================================================================================

EXPORT_GLOBAL
CFTypeID	AirPlayReceiverServerGetTypeID( void )
{
	dispatch_once_f( &gAirPlayReceiverServerInitOnce, NULL, _GetTypeID );
	return( gAirPlayReceiverServerTypeID );
}

//===========================================================================================================================
//	AirPlayReceiverServerCreate
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus	AirPlayReceiverServerCreateWithConfigFilePath( CFStringRef inConfigFilePath, AirPlayReceiverServerRef *outServer )
{
	OSStatus						err;
	AirPlayReceiverServerRef		me;
	size_t							extraLen;
	int								i;
	uint64_t						u64;
	
	dispatch_once_f( &gAirPlayReceiverInitOnce, NULL, _GlobalInitialize );
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (AirPlayReceiverServerRef) _CFRuntimeCreateInstance( NULL, AirPlayReceiverServerGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->overscanOverride = -1; // Default to auto.
	me->timeoutDataSecs  = kAirPlayDataTimeoutSecs;
	
	me->queue = dispatch_queue_create( "AirPlayReceiverServerQueue", 0 );
	me->httpQueue = dispatch_queue_create( "AirPlayReceiverServerHTTPQueue", 0 );
	
	for( i = 1; i < 10; ++i )
	{
		u64 = GetPrimaryMACAddress( me->deviceID, NULL );
		if( u64 != 0 ) break;
		sleep( 1 );
	}
	
	if( !inConfigFilePath )
	{
		me->configFilePath = strdup( AIRPLAY_CONFIG_FILE_PATH );
	}
	else
	{
		err = CFStringCopyUTF8CString( inConfigFilePath, &me->configFilePath );
		require_noerr( err, exit );
	}
	_InitializeConfig( me );
		
	RandomBytes( me->httpTimedNonceKey, sizeof( me->httpTimedNonceKey ) );
	
	err = APAdvertiserCreate( &me->advertiser );
	require_noerr( err, exit );
	
	err = AirPlayReceiverServerPlatformInitialize( me );
	require_noerr( err, exit );
	
	*outServer = me;
	gAirPlayReceiverServer = me;
	g_message("%s : gAirPlayReceiverServer = %p\n",__func__,gAirPlayReceiverServer );
	me = NULL;
	err = kNoErr;
	
exit:
	if( me ) CFRelease( me );
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_FinalizeLogs
//===========================================================================================================================

static void	_FinalizeLogs( CFTypeRef inCF )
{
	AirPlayReceiverLogsRef		inLogs = (AirPlayReceiverLogsRef) inCF;
	
	ForgetCF( &inLogs->server );
	if( inLogs->dataBuffer )
	{
		DataBuffer_Free( inLogs->dataBuffer );
		inLogs->dataBuffer = NULL;
	}
}

//===========================================================================================================================
//	_GetTypeIDLogs
//===========================================================================================================================

static void	_GetTypeIDLogs( void *inContext )
{
	(void) inContext;
	
	gAirPlayReceiverLogsTypeID = _CFRuntimeRegisterClass( &kAirPlayReceiverLogsClass );
	check( gAirPlayReceiverLogsTypeID != _kCFRuntimeNotATypeID );
}


//===========================================================================================================================
//	AirPlayReceiverLogsGetTypeID
//===========================================================================================================================

static CFTypeID	AirPlayReceiverLogsGetTypeID( void )
{
	dispatch_once_f( &gAirPlayReceiverLogsInitOnce, NULL, _GetTypeIDLogs );
	return( gAirPlayReceiverLogsTypeID );
}

//===========================================================================================================================
//	AirPlayReceiverLogsCreate
//===========================================================================================================================

static OSStatus	AirPlayReceiverLogsCreate( AirPlayReceiverServerRef inServer, AirPlayReceiverLogsRef *outLogs )
{
	OSStatus					err;
	AirPlayReceiverLogsRef		me;
	size_t						extraLen;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (AirPlayReceiverLogsRef) _CFRuntimeCreateInstance( NULL, AirPlayReceiverLogsGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );	
	
	me->server = inServer;
	CFRetain( inServer );
	
	*outLogs = me;
	me = NULL;
	err = kNoErr;
	
exit:
	if( me ) CFRelease( me );
	return( err );
}

EXPORT_GLOBAL
OSStatus	AirPlayReceiverServerCreate( AirPlayReceiverServerRef *outServer )
{
	return AirPlayReceiverServerCreateWithConfigFilePath( NULL, outServer );
}

//===========================================================================================================================
//	AirPlayReceiverServerSetDelegate
//===========================================================================================================================

EXPORT_GLOBAL
void	AirPlayReceiverServerSetDelegate( AirPlayReceiverServerRef inServer, const AirPlayReceiverServerDelegate *inDelegate )
{
	inServer->delegate = *inDelegate;
}

//===========================================================================================================================
//	AirPlayReceiverServerGetDispatchQueue
//===========================================================================================================================

EXPORT_GLOBAL
dispatch_queue_t	AirPlayReceiverServerGetDispatchQueue( AirPlayReceiverServerRef inServer )
{
	return( inServer->queue );
}

//===========================================================================================================================
//	AirPlayReceiverServerControl
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
	AirPlayReceiverServerControl( 
		CFTypeRef			inServer, 
		uint32_t			inFlags, 
		CFStringRef			inCommand, 
		CFTypeRef			inQualifier, 
		CFDictionaryRef		inParams, 
		CFDictionaryRef *	outParams )
{
	AirPlayReceiverServerRef const		server = (AirPlayReceiverServerRef) inServer;
	OSStatus							err;
	
	if( 0 ) {}
	
	// GetLogs

	else if( server->delegate.control_f && CFEqual( inCommand, CFSTR( kAirPlayCommand_GetLogs ) ) )
	{
		aprs_ulog( kLogLevelNotice, "Get Log Archive\n" );
		err = server->delegate.control_f( server, inCommand, inQualifier, inParams, outParams, server->delegate.context );
		goto exit;
	}
	
	// UpdateAdvertising
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_UpdateAdvertising ) ) )
	{
		err = _UpdateAdvertising( server );
		require_noerr( err, exit );
	}
	
	// PrefsChanged
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayEvent_PrefsChanged ) ) )
	{
		if( server->started )
		{
			aprs_ulog( kLogLevelNotice, "Prefs changed\n" );
			_UpdatePrefs( server );
		}
	}
	
	// StartServer / StopServer
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_StartServer ) ) )
	{
		server->started = true;
		_UpdatePrefs( server );
	}
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_StopServer ) ) )
	{
		if( server->started )
		{
			server->started = false;
			_StopServers( server );
		}
	}
	
	// SessionDied
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_SessionDied ) ) )
	{
		aprs_ulog( kLogLevelNotice, "### Failure: %#m\n", kTimeoutErr );
		_RemoveAllConnections( server );
	}
	
	// Other
	
	else
	{
		err = AirPlayReceiverServerPlatformControl( server, inFlags, inCommand, inQualifier, inParams, outParams );
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverServerCopyProperty
//===========================================================================================================================

EXPORT_GLOBAL
CFTypeRef
	AirPlayReceiverServerCopyProperty( 
		CFTypeRef	inServer, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		OSStatus *	outErr )
{
	AirPlayReceiverServerRef const		server = (AirPlayReceiverServerRef) inServer;
	OSStatus							err;
	CFTypeRef							value = NULL;
	
	if( 0 ) {}
	
	// Playing
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_Playing ) ) )
	{
		value = server->playing ? kCFBooleanTrue : kCFBooleanFalse;
		CFRetain( value );
	}
	
	// Source version
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_SourceVersion ) ) )
	{
		value = CFSTR( kAirPlaySourceVersionStr );
		CFRetain( value );
	}
	
	// Other
	
	else
	{
		value = AirPlayReceiverServerPlatformCopyProperty( server, inFlags, inProperty, inQualifier, &err );
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	AirPlayReceiverServerSetProperty
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
	AirPlayReceiverServerSetProperty( 
		CFTypeRef	inServer, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		CFTypeRef	inValue )
{
	AirPlayReceiverServerRef const		server = (AirPlayReceiverServerRef) inServer;
	OSStatus							err;
	
	if( 0 ) {}
	
	// DeviceID
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_DeviceID ) ) )
	{
		CFGetData( inValue, server->deviceID, sizeof( server->deviceID ), NULL, NULL );
	}
	
	// InterfaceName
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_InterfaceName ) ) )
	{
		char			ifname[ IF_NAMESIZE + 1 ];
		uint32_t		ifindex = 0;
		CFNumberRef		ifindexObj = NULL;
		
		CFGetCString( inValue, ifname, sizeof( ifname ) );
		if( *ifname != '\0' )
		{
			ifindex = if_nametoindex( ifname );
			require_action_quiet( ifindex != 0, exit, err = kNotFoundErr );
		}
		
		ifindexObj = CFNumberCreateInt64( ifindex );
		require_action( ifindexObj, exit, err = kNoMemoryErr );
		
		err = APAdvertiserSetProperty( server->advertiser, kAPAdvertiserProperty_InterfaceIndex, ifindexObj );
		CFRelease( ifindexObj );
		require_noerr( err, exit );
		
		err = _RestartAdvertising( server );
		require_noerr( err, exit );
	}
	
	// Playing
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_Playing ) ) )
	{
		g_message("%s : server = %p\n",__func__, server );
		server->playing = CFGetBoolean( inValue, NULL );
		
		// If we updated Bonjour while we were playing and had to defer it, do it now that we've stopped playing.
		
		if( !server->playing && server->started && server->serversStarted && server->advertisingRestartPending )
		{
			err = _RestartAdvertising( server );
			require_noerr( err, exit );
		}
	}
	
	// Other
	
	else
	{
		err = AirPlayReceiverServerPlatformSetProperty( server, inFlags, inProperty, inQualifier, inValue );
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_GetTypeID
//===========================================================================================================================

static void _GetTypeID( void *inContext )
{
	(void) inContext;
	
	gAirPlayReceiverServerTypeID = _CFRuntimeRegisterClass( &kAirPlayReceiverServerClass );
	check( gAirPlayReceiverServerTypeID != _kCFRuntimeNotATypeID );
}

#if( 0 || ( TARGET_OS_POSIX && DEBUG ) )
//===========================================================================================================================
//	AirPlayReceiverServerDebugShow
//===========================================================================================================================

static OSStatus	AirPlayReceiverServerDebugShow( AirPlayReceiverServerRef inServer, int inVerbose, DataBuffer *inDataBuf )
{
	AirPlayReceiverServerDebugShowContext		context = { inServer, inVerbose, inDataBuf };

	dispatch_sync_f( inServer->queue, &context, _AirPlayReceiverServerDebugShow );
	return( kNoErr );
}

//===========================================================================================================================
//	AirPlayReceiverServerDebugShow
//===========================================================================================================================

static void	_AirPlayReceiverServerDebugShow( void *inContext )
{
	AirPlayReceiverServerDebugShowContext *		context = (AirPlayReceiverServerDebugShowContext *) inContext;

	DataBuffer_AppendF( context->dataBuf, "\n" );
	DataBuffer_AppendF( context->dataBuf, "+-+ AirPlay Server state +-+\n" );
	DataBuffer_AppendF( context->dataBuf, "\n" );
	
	DataBuffer_AppendF( context->dataBuf, "DeviceActivated=%s", context->server->deviceActivated ? "yes" : "no" );
	DataBuffer_AppendF( context->dataBuf, " playing=%s", context->server->playing ? "yes" : "no" );
	DataBuffer_AppendF( context->dataBuf, "\n" );
	
	APAdvertiserDebugShow( context->server->advertiser, context->verbose, context->dataBuf );
}

//===========================================================================================================================
//	_HandleDebug
//===========================================================================================================================

static OSStatus	_HandleDebug( CFDictionaryRef inRequest, CFDictionaryRef *outResponse, void *inContext )
{
	OSStatus					err;
	CFStringRef					opcode;
	DataBuffer					dataBuf;
	CFMutableDictionaryRef		response;
	
	(void) inContext;
	
	DataBuffer_Init( &dataBuf, NULL, 0, 10 * kBytesPerMegaByte );
	response = NULL;
	
	opcode = (CFStringRef) CFDictionaryGetValue( inRequest, kDebugIPCKey_Command );
	require_action_quiet( opcode, exit, err = kNotHandledErr );
	require_action( CFIsType( opcode, CFString ), exit, err = kTypeErr );
	
	if( 0 ) {} // Empty if to simplify else if's below.
	
	// Show
	
	else if( CFEqual( opcode, kDebugIPCOpCode_Show ) )
	{
		int		verbose;
		
		verbose = (int) CFDictionaryGetInt64( inRequest, CFSTR( "verbose" ), NULL );
		err = AirPlayReceiverServerDebugShow( gAirPlayReceiverServer, verbose, &dataBuf );
		require_noerr( err, exit );
		
		err = CFPropertyListCreateFormatted( NULL, &response, "{%kO=%.*s}",
											kDebugIPCKey_Value, (int) dataBuf.bufferLen, dataBuf.bufferPtr );
		require_noerr( err, exit );
	}
	
	// Other
	
	else
	{
		aprs_dlog( kLogLevelNotice, "### Unsupported debug command: %@\n", opcode );
		err = kNotHandledErr;
		goto exit;
	}
	
	if( response ) CFDictionarySetValue( response, kDebugIPCKey_ResponseType, opcode );
	*outResponse = response;
	response = NULL;
	err = kNoErr;
	
exit:
	DataBuffer_Free( &dataBuf );
	if( response ) CFRelease( response );
	return( err );
}
#endif //( 0 || ( TARGET_OS_POSIX && DEBUG ) )


//===========================================================================================================================
//	_GlobalInitialize
//
//	Perform one-time initialization of things global to the entire process.
//===========================================================================================================================

static void	_GlobalInitialize( void *inContext )
{
	(void) inContext;
	
#if( TARGET_OS_POSIX )
	signal( SIGPIPE, SIG_IGN ); // Ignore SIGPIPE signals so we get EPIPE errors from APIs instead of a signal.
#endif
	MFiPlatform_Initialize(); // Initialize at app startup to cache cert to speed up first time connect.
	
	// Setup logging.
	
#if( LOGUTILS_CF_PREFERENCES )
	LogSetAppID( CFSTR( kAirPlayPrefAppID ) );
#endif
	
	AirPlayReceiverServerSetLogLevel();
	
#if  ( TARGET_OS_POSIX && DEBUG )
	DebugIPC_EnsureInitialized( _HandleDebug, NULL );
#endif
}

//===========================================================================================================================
//	_InitializeConfig
//===========================================================================================================================

static OSStatus	_InitializeConfig( AirPlayReceiverServerRef inServer )
{
	OSStatus			err;
	CFDataRef			data;
#if TARGET_OS_WIN32 && UNICODE
	int					unicodeFlags, requiredBytes;
	CFMutableDataRef	tempData;
#endif
	CFDictionaryRef		config = NULL;
	CFArrayRef			array;
	CFIndex				i, n;
	CFDictionaryRef		dict;
	
	(void) inServer;
	
	// Read the config file (if it exists). Try binary plist format first since it has a unique signature to detect a
	// valid file. If it's a binary plist then parse it as an INI file and convert it to a dictionary.
	
	data = CFDataCreateWithFilePath( inServer->configFilePath, NULL );
	require_action_quiet( data, exit, err = kNotFoundErr );
	
#if TARGET_OS_WIN32 && UNICODE
	unicodeFlags = IS_TEXT_UNICODE_REVERSE_SIGNATURE | IS_TEXT_UNICODE_SIGNATURE;
	IsTextUnicode( CFDataGetBytePtr( data ), CFDataGetLength( data ), &unicodeFlags );

	// The return value of IsTextUnicode isn't reliable... all we're interested in is the presence of a BOM,
	// which is correctly reported via the output flags regardless of the return value 
	if ( unicodeFlags )
	{
		// WideCharToMultiByte only supports native endian
		require_action( !( unicodeFlags & IS_TEXT_UNICODE_REVERSE_SIGNATURE ), exit, err = kUnsupportedDataErr );

		requiredBytes = WideCharToMultiByte( CP_UTF8, 0, ( (LPCWSTR) CFDataGetBytePtr( data ) ) + 1, (int) ( CFDataGetLength( data ) / sizeof( WCHAR ) ) - 1, NULL, 0, NULL, NULL );
		require_action( requiredBytes, exit, err = kUnsupportedDataErr );

		tempData = CFDataCreateMutable( kCFAllocatorDefault, requiredBytes );
		require_action( tempData, exit, err = kNoMemoryErr );

		CFDataSetLength( tempData, requiredBytes );
		WideCharToMultiByte( CP_UTF8, 0, ( (LPCWSTR) CFDataGetBytePtr( data ) ) + 1, (int) ( CFDataGetLength( data ) / sizeof( WCHAR ) ) - 1, (LPSTR) CFDataGetMutableBytePtr( tempData ), requiredBytes, NULL, NULL );
		CFRelease( data );
		data = tempData;
		tempData = NULL;
	}
#endif
	
	config = (CFDictionaryRef) CFPropertyListCreateWithData( NULL, data, kCFPropertyListImmutable, NULL, NULL );
	if( !config )
	{
		config = CFDictionaryCreateWithINIBytes( CFDataGetBytePtr( data ), (size_t) CFDataGetLength( data ), 
			kINIFlag_MergeGlobals, CFSTR( kAirPlayKey_Name ), NULL );
	}
	CFRelease( data );
	if( config && !CFIsType( config, CFDictionary ) )
	{
		dlogassert( "Bad type for config file: %s", inServer->configFilePath );
		CFRelease( config );
		config = NULL;
	}
	
	// Register static HID devices.
	
	array = config ? CFDictionaryGetCFArray( config, CFSTR( kAirPlayKey_HIDDevices ), NULL ) : NULL;
	if( !array ) array = config ? CFDictionaryGetCFArray( config, CFSTR( kAirPlayKey_HIDDevice ), NULL ) : NULL;
	n = array ? CFArrayGetCount( array ) : 0;
	for( i = 0; i < n; ++i )
	{
		HIDDeviceRef		hid;
		
		dict = CFArrayGetCFDictionaryAtIndex( array, i, NULL );
		check( dict );
		if( !dict ) continue;
		
		err = HIDDeviceCreateVirtual( &hid, dict );
		check_noerr( err );
		if( err ) continue;
		
		err = HIDRegisterDevice( hid );
		CFRelease( hid );
		check_noerr( err );
	}
	
	// Register static displays.
	
	array = config ? CFDictionaryGetCFArray( config, CFSTR( kAirPlayKey_Displays ), NULL ) : NULL;
	if( !array ) array = config ? CFDictionaryGetCFArray( config, CFSTR( kAirPlayKey_Display ), NULL ) : NULL;
	n = array ? CFArrayGetCount( array ) : 0;
	for( i = 0; i < n; ++i )
	{
		ScreenRef		screen;
		
		dict = CFArrayGetCFDictionaryAtIndex( array, i, NULL );
		check( dict );
		if( !dict ) continue;
		
		err = ScreenCreate( &screen, dict );
		check_noerr( err );
		if( err ) continue;
		
		err = ScreenRegister( screen );
		CFRelease( screen );
		check_noerr( err );
	}

	// Retrieve vendor specific screen stream options
	
	if( config )
	{
		inServer->screenStreamOptions = CFDictionaryGetCFDictionary( config, CFSTR( kAirPlayKey_ScreenStreamOptions ), NULL );
		if( !inServer->screenStreamOptions )
		{
			// INI named sections are read into an array.
			array = CFDictionaryGetCFArray( config, CFSTR( kAirPlayKey_ScreenStreamOptions ), NULL );
			if( array ) inServer->screenStreamOptions = CFArrayGetCFDictionaryAtIndex( array, 0, NULL );
		}
		CFRetainNullSafe( inServer->screenStreamOptions );
	}

	// Retrieve vendor specific audio stream options

	if( config )
	{
		inServer->audioStreamOptions = CFDictionaryGetCFDictionary( config, CFSTR( kAirPlayKey_AudioStreamOptions ), NULL );
		if( !inServer->audioStreamOptions )
		{
			// INI named sections are read into an array.
			array = CFDictionaryGetCFArray( config, CFSTR( kAirPlayKey_AudioStreamOptions ), NULL );
			if( array ) inServer->audioStreamOptions = CFArrayGetCFDictionaryAtIndex( array, 0, NULL );
		}
		CFRetainNullSafe( inServer->audioStreamOptions );
	}
	
	// Save off the config dictionary for future use.
	
	inServer->config = config;
	config = NULL;
	err = kNoErr;
	
exit:
	// There is a significant amount of code that assumes there is always a config dictionary, so create an empty one in case of error.
	if( err ) inServer->config = CFDictionaryCreate( NULL, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	
	CFReleaseNullSafe( config );
	return( err );
}

//===========================================================================================================================
//	_Finalize
//===========================================================================================================================

static void	_Finalize( CFTypeRef inCF )
{
	OSStatus							err = kNoErr;
	AirPlayReceiverServerRef const		me = (AirPlayReceiverServerRef) inCF;
	
	_StopServers( me );
	AirPlayReceiverServerPlatformFinalize( me );
	
	err = APAdvertiserInvalidate( me->advertiser );
	check_noerr( err );
	ForgetCF( &me->advertiser );
	
	dispatch_forget( &me->queue );
	dispatch_forget( &me->httpQueue );
	ForgetCF( &me->audioStreamOptions );
	ForgetCF( &me->screenStreamOptions );
	ForgetMem( &me->configFilePath );
	ForgetCF( &me->config );
	g_message("%s : gAirPlayReceiverServer = %p\n",__func__,gAirPlayReceiverServer );
	gAirPlayReceiverServer = NULL;
}

//===========================================================================================================================
//	_UpdatePrefs
//===========================================================================================================================

static void	_UpdatePrefs( AirPlayReceiverServerRef inServer )
{
	OSStatus		err;
	Boolean			b;
	int				i;
	char			cstr[ 64 ];
	Boolean			restartAdvertising = false;
	Boolean			updateServers  = false;
	
	AirPlaySettings_Synchronize();
	
	// Enabled. If disabled, stop the servers and skip all the rest.
	
	b = AirPlaySettings_GetBoolean( NULL, CFSTR( kAirPlayPrefKey_Enabled ), &err );
	if( err ) b = true;
	if( !b )
	{
		_StopServers( inServer );
		goto exit;
	}
	
	// DenyInterruptions
	
	b = AirPlaySettings_GetBoolean( NULL, CFSTR( kAirPlayPrefKey_DenyInterruptions ), NULL );
	if( b != inServer->denyInterruptions )
	{
		aprs_dlog( kLogLevelNotice, "Deny Interruptions: %s -> %s\n", !b ? "yes" : "no", b ? "yes" : "no" );
		inServer->denyInterruptions = b;
		
	}
	
	// Name
	
	*cstr = '\0';
	AirPlayGetDeviceName( cstr, sizeof( cstr ) );
	if( strcmp( cstr, inServer->name ) != 0 )
	{
		aprs_ulog( kLogLevelNotice, "Name changed '%s' -> '%s'\n", inServer->name, cstr );
		strlcpy( inServer->name, cstr, sizeof( inServer->name ) );
		restartAdvertising = true;
		updateServers  = true;
	}
	
	// OverscanOverride
	
	i = (int) AirPlaySettings_GetInt64( NULL, CFSTR( kAirPlayPrefKey_OverscanOverride ), &err );
	if( err ) i = -1;
	if( i != inServer->overscanOverride )
	{
		aprs_dlog( kLogLevelNotice, "Overscan override: %s -> %s\n", 
			( inServer->overscanOverride  < 0 ) ? "auto" :
			( inServer->overscanOverride == 0 ) ? "no"   :
												  "yes", 
			( i  < 0 ) ? "auto" :
			( i == 0 ) ? "no"   :
						 "yes" );
		inServer->overscanOverride = i;
	}
	
	// PairAll
	
	b = AirPlaySettings_GetBoolean( NULL, CFSTR( kAirPlayPrefKey_PairAll ), NULL );
	if( b != inServer->pairAll )
	{
		aprs_dlog( kLogLevelNotice, "Pair all: %s -> %s\n", !b ? "yes" : "no", b ? "yes" : "no" );
		inServer->pairAll = b;
		updateServers = true;
	}
	
	// PairPIN
	
	b = AirPlaySettings_GetBoolean( NULL, CFSTR( kAirPlayPrefKey_PairPIN ), &err );
	if( b != inServer->pairPIN )
	{
		aprs_dlog( kLogLevelNotice, "Pair PIN: %s -> %s\n", !b ? "yes" : "no", b ? "yes" : "no" );
		inServer->pairPIN = b;
		updateServers = true;
		
			b = AirPlaySettings_GetBoolean( NULL, CFSTR( kAirPlayPrefKey_PairPINServerShadow ), NULL );
			if( b != inServer->pairPIN )
			{
				PairingSessionRef				pairingSessionHomeKit;
				
				aprs_ulog( kLogLevelNotice, "Deleting server pairing peers on pref change\n" );
				
				err = PairingSessionCreate( &pairingSessionHomeKit, NULL, kPairingSessionType_None );
				check_noerr( err );
				if( !err )
				{
					PairingSessionSetKeychainInfo_AirPlay( pairingSessionHomeKit );
					PairingSessionDeletePeer( pairingSessionHomeKit, NULL, 0 );
					CFRelease( pairingSessionHomeKit );
				}
				AirPlaySettings_SetBoolean( CFSTR( kAirPlayPrefKey_PairPINServerShadow ), inServer->pairPIN );
				AirPlaySettings_Synchronize();
			}
	}
	
	// QoSDisabled
	
	b = AirPlaySettings_GetBoolean( NULL, CFSTR( kAirPlayPrefKey_QoSDisabled ), NULL );
	if( b != inServer->qosDisabled )
	{
		aprs_dlog( kLogLevelNotice, "QoS disabled: %s -> %s\n", !b ? "yes" : "no", b ? "yes" : "no" );
		inServer->qosDisabled = b;
		updateServers = true;
	}
	
	// TimeoutData
	
	inServer->timeoutDataSecs = (int) AirPlaySettings_GetInt64( NULL, CFSTR( kAirPlayPrefKey_TimeoutDataSecs ), &err );
	if( err || ( inServer->timeoutDataSecs <= 0 ) ) inServer->timeoutDataSecs = kAirPlayDataTimeoutSecs;
	
	
	// Tell the platform so it can re-read any prefs it might have.
	
	AirPlayReceiverServerPlatformControl( inServer, kCFObjectFlagDirect, CFSTR( kAirPlayCommand_UpdatePrefs ), NULL, NULL, NULL );
	
	// Tell the advertiser to re-read any prefs that it might have.
	
	err = APAdvertiserUpdatePreferences( inServer->advertiser, false );
	
	// Finally, act on any settings changes.
	
	if( !inServer->serversStarted )
	{
		_StartServers( inServer );
	}
	else
	{
		if( restartAdvertising )
		{
			_RestartAdvertising( inServer );
		}
		if( updateServers )
		{
			_UpdateServers( inServer );
		}
	}
	
exit:
	return;
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_RestartAdvertising
//===========================================================================================================================

static OSStatus	_RestartAdvertising( AirPlayReceiverServerRef inServer )
{
	OSStatus			err = kNoErr;
	APAdvertiserMode	mode;
	
	inServer->advertisingRestartPending = false;
	
	// Ignore if we've been disabled.
	
	if( !inServer->started )
	{
		aprs_dlog( kLogLevelNotice, "Ignoring advertising restart while disabled.\n" );
		goto exit;
	}
	
	// Only restart advertising if we're not playing because some clients may stop playing if they see us go away.
	// If we're playing, just mark the advertising restart as pending and we'll process it when we stop playing.
	
	if( inServer->playing )
	{
		aprs_dlog( kLogLevelNotice, "Deferring advertising restart until we've stopped playing.\n" );
		inServer->advertisingRestartPending = true;
		goto exit;
	}
	
	// Ignore if advertiser hasn't been started.
	
	err = APAdvertiserGetMode( inServer->advertiser, &mode );
	require_noerr( err, exit );
	
	if( mode == kAPAdvertiserMode_None )
	{
		aprs_dlog( kLogLevelNotice, "Ignoring advertising restart since advertising wasn't started.\n" );
		goto exit;
	}
	
	// Some clients stop resolves after ~2 minutes to reduce multicast traffic so if we changed something in the 
	// TXT record, such as the password state, those clients wouldn't be able to detect that state change.
	// To work around this, completely remove the Bonjour service, wait 2 seconds to give time for Bonjour to 
	// flush out the removal then re-add the Bonjour service so client will re-resolve it.
	
	err = _EnsureStoppedAdvertising( inServer, "restart" );
	require_noerr( err, exit );
	
	aprs_dlog( kLogLevelNotice, "Waiting for 2 seconds for the advertiser to remove services.\n" );
	sleep( 2 );
	
	aprs_dlog( kLogLevelNotice, "Restarting to advertise.\n" );
	err = _UpdateAdvertising( inServer );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_EnsureStartedAdvertising
//===========================================================================================================================

static OSStatus	_EnsureStartedAdvertising( AirPlayReceiverServerRef inServer )
{
	OSStatus			err = kNoErr;
	CFNumberRef			port = NULL;
	APAdvertiserMode	mode = kAPAdvertiserMode_None;
	
	err = APAdvertiserGetMode( inServer->advertiser, &mode );
	require_noerr( err, exit );
	
	require_action_quiet( mode == kAPAdvertiserMode_None, exit, err = kNoErr );
	
	port = CFNumberCreateInt64( _GetListenPort( inServer ) );
	require_action( port, exit, err = kNoMemoryErr );
	
	err = APAdvertiserSetProperty( inServer->advertiser, kAPAdvertiserProperty_ListeningPort, port );
	require_noerr( err, exit );
	
	err = APAdvertiserSetMode( inServer->advertiser, kAPAdvertiserMode_Discoverable );
	require_noerr( err, exit );
	
	aprs_ulog( kLogLevelNotice, "Started advertising.\n" );
	
exit:
	CFReleaseNullSafe( port );
	return( err );
}


//===========================================================================================================================
//	_EnsureStoppedAdvertising
//===========================================================================================================================

static OSStatus	_EnsureStoppedAdvertising( AirPlayReceiverServerRef inServer, const char *inReason )
{
	OSStatus			err = kNoErr;
	APAdvertiserMode	mode = kAPAdvertiserMode_None;
	
	err = APAdvertiserGetMode( inServer->advertiser, &mode );
	require_noerr( err, exit );
	
	require_action_quiet( mode != kAPAdvertiserMode_None, exit, err = kNoErr );
	
	err = APAdvertiserSetMode( inServer->advertiser, kAPAdvertiserMode_None );
	require_noerr( err, exit );
	
	aprs_ulog( kLogLevelNotice, "Stopped advertising due to %s\n", inReason );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_UpdateAdvertising
//===========================================================================================================================

static OSStatus	_UpdateAdvertising( AirPlayReceiverServerRef inServer )
{
	OSStatus				err;
	Boolean					activated = true;
	APAdvertiserInfoRef		info = NULL;
	
	if( !inServer->serversStarted || !activated )
	{
		if( !activated )
		{
			aprs_ulog( kLogLevelNotice, "Device has not been activated. Bonjour service registration disabled.\n" );
		}
		
		err = _EnsureStoppedAdvertising( inServer, "disable" );
		check_noerr( err );
		goto exit;
	}
	
	err = _CreateAdvertiserInfo( inServer, &info );
	require_noerr( err, exit );
	
	err = APAdvertiserSetProperty( inServer->advertiser, kAPAdvertiserProperty_AdvertiserInfo, info );
	require_noerr( err, exit );
	
	// The server is supposed to be advertising all the time, so start it if it hasn't started yet.
	
	err = _EnsureStartedAdvertising( inServer );
	require_noerr( err, exit );
	
exit:
	CFReleaseNullSafe( info );
	return( err );
}

//===========================================================================================================================
//	_AdvertiserInfoAddString
//===========================================================================================================================

static OSStatus _AdvertiserInfoAddString( APAdvertiserInfoRef inInfo, CFStringRef inKey, const char *cstr )
{
	OSStatus		err;
	CFStringRef		string = NULL;
	
	string = CFStringCreateWithCString( kCFAllocatorDefault, cstr, kCFStringEncodingUTF8 );
	require_action( string, exit, err = kNoMemoryErr );
	
	err = APAdvertiserInfoSetProperty( inInfo, inKey, string );
	require_noerr( err, exit );
	
exit:
	CFReleaseNullSafe( string );
	return( err );
}

//===========================================================================================================================
//	_AdvertiserInfoAddString
//===========================================================================================================================

static
	OSStatus _AdvertiserInfoAddNumber(
		APAdvertiserInfoRef inInfo,
		CFStringRef inKey,
		CFNumberType inType,
		const void *valuePtr )
{
	OSStatus		err;
	CFNumberRef		number = NULL;
	
	number = CFNumberCreate( kCFAllocatorDefault, inType, valuePtr );
	require_action( number, exit, err = kNoMemoryErr );
	
	err = APAdvertiserInfoSetProperty( inInfo, inKey, number );
	require_noerr( err, exit );
	
exit:
	CFReleaseNullSafe( number );
	return( err );
}

//===========================================================================================================================
//	_CreateAdvertiserInfo
//===========================================================================================================================

static OSStatus _CreateAdvertiserInfo( AirPlayReceiverServerRef inServer, APAdvertiserInfoRef *outAdvertiserInfo )
{
	OSStatus				err;
	char					cstr[ 256 ];
	const char *			ptr;
	APAdvertiserInfoRef		info = NULL;
	AirPlayFeatures			features;
	uint32_t				u32;
	
	err = APAdvertiserInfoCreate( kCFAllocatorDefault, &info );
	require_noerr( err, exit );
	
	// Device Name
	
	*cstr = '\0';
	err = AirPlayGetDeviceName( cstr, sizeof( cstr ) );
	require_noerr( err, exit );
	
	err = _AdvertiserInfoAddString( info, kAPAdvertiserInfoProperty_DeviceName, cstr );
	require_noerr( err, exit );
	
	// DeviceID
	
	AirPlayGetDeviceID( inServer->deviceID );
	MACAddressToCString( inServer->deviceID, cstr );
	
	err = _AdvertiserInfoAddString( info, kAPAdvertiserInfoProperty_DeviceID, cstr );
	require_noerr( err, exit );
	
	// Features
	
	features = AirPlayGetFeatures();
	
	err = _AdvertiserInfoAddNumber( info, kAPAdvertiserInfoProperty_Features, kCFNumberSInt64Type, &features );
	require_noerr( err, exit );
	
	// FirmwareRevision
	
	*cstr = '\0';
	AirPlayGetConfigCString( CFSTR( kAirPlayKey_FirmwareRevision ), cstr, sizeof( cstr ), NULL );
	if( *cstr == '\0' )
	{
		AirPlayReceiverServerPlatformGetCString( inServer, CFSTR( kAirPlayKey_FirmwareRevision ), NULL,
			cstr, sizeof( cstr ), NULL );
	}
	
	if( *cstr != '\0' )
	{
		err = _AdvertiserInfoAddString( info, kAPAdvertiserInfoProperty_FirmwareVersion, cstr );
		require_noerr( err, exit );
	}
	
	// Flags
	
	u32 = AirPlayGetStatusFlags();
	if( u32 != 0 )
	{
		err = _AdvertiserInfoAddNumber( info, kAPAdvertiserInfoProperty_SystemFlags, kCFNumberSInt32Type, &u32 );
		require_noerr( err, exit );
	}
	
	// Model
	
	*cstr = '\0';
	AirPlayGetDeviceModel( cstr, sizeof( cstr ) );
	if( *cstr != '\0' )
	{
		err = _AdvertiserInfoAddString( info, kAPAdvertiserInfoProperty_DeviceModel, cstr );
		require_noerr( err, exit );
	}
	
	// Protocol version
	
	ptr = kAirPlayProtocolVersionStr;
	if( strcmp( ptr, "1.0" ) != 0 )
	{
		err = _AdvertiserInfoAddString( info, kAPAdvertiserInfoProperty_ProtocolVersion, ptr );
		require_noerr( err, exit );
	}
	
{
	char *		cptr;
	
	// Public HomeKit pairing identity
	
	err = AirPlayCopyHomeKitPairingIdentity( &cptr, NULL );
	if( !err )	
	{
		err = _AdvertiserInfoAddString( info, kAPAdvertiserInfoProperty_PublicHomeKitPairingIdentity, cptr );
		free( cptr );
		require_noerr( err, exit );
	}
	else
	{
		aprs_ulog( kLogLevelNotice, "### Add pairing HomeKit peer identifier to advertiser info failed: %#m\n", err );
	}
}
	
	// Source Version
	
	ptr = kAirPlaySourceVersionStr;
	err = _AdvertiserInfoAddString( info, kAPAdvertiserInfoProperty_AirPlayVersion, ptr );
	require_noerr( err, exit );
	
	*outAdvertiserInfo = info;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

static HTTPServerRef _CreateHTTPServerForPort( AirPlayReceiverServerRef inServer, int inListenPort )
{
	OSStatus			err;
	HTTPServerRef		httpServer;
	HTTPServerDelegate	delegate;
	
	httpServer = NULL;
	
	HTTPServerDelegateInit( &delegate );
	delegate.context				= inServer;
	delegate.connectionExtraLen		= sizeof( struct AirPlayReceiverConnectionPrivate );
	delegate.connectionCreated_f	= _HandleHTTPConnectionCreated;
	
	err = HTTPServerCreate( &httpServer, &delegate );
	require_noerr( err, exit );
	
	httpServer->listenPort = -( inListenPort );

	HTTPServerSetDispatchQueue( httpServer, inServer->httpQueue );
	//HTTPServerSetLogging( inServer->httpServer, aprs_ucat() );
exit:
	return( httpServer );
}

//===========================================================================================================================
//	_StartServers
//===========================================================================================================================

static void	_StartServers( AirPlayReceiverServerRef inServer )
{
	OSStatus		err;
	
	DEBUG_USE_ONLY( err );
	
	if( inServer->serversStarted ) return;
	
	// Create the servers first.
	
	check( inServer->httpServer == NULL );
	inServer->httpServer = _CreateHTTPServerForPort( inServer, kAirPlayFixedPort_MediaControl );
//	inServer->httpServer = _CreateHTTPServerForPort( inServer, kAirPlayFixedPort_RTSPControl );
#if( AIRPLAY_HTTP_SERVER_LEGACY )
	check( inServer->httpServerLegacy == NULL );
	inServer->httpServerLegacy = _CreateHTTPServerForPort( inServer, kAirPlayFixedPort_RTSPControl );
#endif
	
	// After all the servers are created, apply settings then start them (synchronously).
	
	_UpdateServers( inServer );

	err = HTTPServerStartSync( inServer->httpServer );
	check_noerr( err );
	
#if( AIRPLAY_HTTP_SERVER_LEGACY )
	err = HTTPServerStartSync( inServer->httpServerLegacy );
	check_noerr( err );
#endif
	
	inServer->serversStarted = true;
	err = _UpdateAdvertising( inServer );
	check_noerr( err );
	
	aprs_ulog( kLogLevelNotice, "AirPlay servers started\n" );
}

//===========================================================================================================================
//	_StopServers
//===========================================================================================================================

static void	_StopServers( AirPlayReceiverServerRef inServer )
{
	OSStatus		err = kNoErr;
	
	err = _EnsureStoppedAdvertising( inServer, "stop" );
	check_noerr( err );
	
	HTTPServerForget( &inServer->httpServer );
#if( AIRPLAY_HTTP_SERVER_LEGACY )
	HTTPServerForget( &inServer->httpServerLegacy );
#endif
	
	if( inServer->serversStarted )
	{
		aprs_ulog( kLogLevelNotice, "AirPlay servers stopped\n" );
		inServer->serversStarted = false;
	}
}

//===========================================================================================================================
//	_UpdateServers
//===========================================================================================================================

static void	_UpdateServers( AirPlayReceiverServerRef inServer )
{
	(void) inServer;
}

#if 0
#pragma mark -
#pragma mark == http server control ==
#endif

//===========================================================================================================================
//	_IsConnectionActive
//===========================================================================================================================

static Boolean _IsConnectionActive( HTTPConnectionRef inConnection )
{
	Boolean isActive = false;
	AirPlayReceiverConnectionRef aprsCnx = (AirPlayReceiverConnectionRef) inConnection->delegate.context;
	if( aprsCnx && aprsCnx->didAnnounce )
		isActive = true;
	return isActive;
}

//===========================================================================================================================
//	_FindActiveConnection
//===========================================================================================================================

static HTTPConnectionRef _FindActiveConnection( AirPlayReceiverServerRef inServer )
{
	HTTPConnectionRef cnx;
	
	if( !inServer )					return NULL;
	if( inServer->httpServer )
	{
		for( cnx = inServer->httpServer->connections; cnx; cnx = cnx->next )
		{
			if( _IsConnectionActive( cnx ) )	return( cnx );
		}
	}
	
#if( AIRPLAY_HTTP_SERVER_LEGACY )
	if( inServer->httpServerLegacy )
	{
		for( cnx = inServer->httpServerLegacy->connections; cnx; cnx = cnx->next )
		{
			if( _IsConnectionActive( cnx ) )	return( cnx );
		}
	}
#endif
	
	return( NULL );
}

//===========================================================================================================================
//	_HijackHTTPServerConnections
//
//	This function should be called on inServer->httpQueue.
//===========================================================================================================================

static void
	_HijackHTTPServerConnections( HTTPServerRef	inHTTPServer, HTTPConnectionRef inHijacker )
{
	HTTPConnectionRef *	next;
	HTTPConnectionRef	conn;

	if( inHTTPServer )
	{
		next = &inHTTPServer->connections;
		while( ( conn = *next ) != NULL )
		{
			if( ( conn != inHijacker ) && _IsConnectionActive( conn ) )
			{
				aprs_ulog( kLogLevelNotice, "*** Hijacking connection %##a for %##a\n", &conn->peerAddr, &inHijacker->peerAddr );
				*next = conn->next;
				_DestroyConnection( conn );
			}
			else
			{
				next = &conn->next;
			}
		}
	}
}

//===========================================================================================================================
//	_HijackConnections
//
//	This function should be called on inServer->httpQueue.
//===========================================================================================================================

static void _HijackConnections( AirPlayReceiverServerRef inServer, HTTPConnectionRef inHijacker )
{	
	if( !inServer )				return;
	if( !inServer->httpServer )	return;
	
	// Close any connections that have started audio (should be 1 connection at most).
	// This leaves connections that haven't started audio because they may just be doing OPTIONS requests, etc.
	
	_HijackHTTPServerConnections( inServer->httpServer, inHijacker );
	
#if( AIRPLAY_HTTP_SERVER_LEGACY )
	if( !inServer->httpServerLegacy )	return;
	_HijackHTTPServerConnections( inServer->httpServerLegacy, inHijacker );
#endif
}

//===========================================================================================================================
//	_IsConnectionOfType
//
//	This function should be called on inServer->httpQueue.
//==========================================================================================================================='

static Boolean _IsConnectionOfType( HTTPConnectionRef inConnection, AirPlayHTTPConnectionTypes inConnectionType )
{
	Boolean isConnectionOfRequestedType = false;
	
	(void) inConnection;
	
	if( inConnectionType == kAirPlayHTTPConnectionTypeAny ) 
	{
		isConnectionOfRequestedType = true;
	}
	else if( inConnectionType == kAirPlayHTTPConnectionTypeAudio )
	{
		{
			isConnectionOfRequestedType = true;
		}
	}
	
	return( isConnectionOfRequestedType );
}

//===========================================================================================================================
//	_RemoveHTTPServerConnections
//
//	This function should be called on inServer->httpQueue.
//==========================================================================================================================='

typedef struct
{
	AirPlayHTTPConnectionTypes	connectionTypeToRemove;
	HTTPServerRef				httpServer;
}	AirPlayReceiverServer_RemoveHTTPConnectionsParams;

static void _RemoveHTTPServerConnections( void *inArg )
{
	AirPlayReceiverServer_RemoveHTTPConnectionsParams * params = (AirPlayReceiverServer_RemoveHTTPConnectionsParams *) inArg;
	HTTPConnectionRef *									next;
	HTTPConnectionRef									conn;

	if( params->httpServer )
	{
		next = &params->httpServer->connections;
		while( ( conn = *next ) != NULL )
		{
			if( _IsConnectionOfType( conn, params->connectionTypeToRemove ) )
			{
				*next = conn->next;
				_DestroyConnection( conn );
			}
			else
			{
				next = &conn->next;
			}
		}
		
		check( params->httpServer->connections == NULL );	
	}
	CFRelease( params->httpServer );
	free( params );
}

//===========================================================================================================================
//	_RemoveHTTPServerConnectionsOfType
//
//  This function should NOT be called on inServer->httpQueue.
//===========================================================================================================================

static void _RemoveHTTPServerConnectionsOfType( AirPlayReceiverServerRef inServer, HTTPServerRef inHTTPServer, AirPlayHTTPConnectionTypes inConnectionTypeToRemove )
{
	AirPlayReceiverServer_RemoveHTTPConnectionsParams * params	= NULL;
	OSStatus											err		= kNoErr;
	
	if( !inHTTPServer )			return;
	
	params = (AirPlayReceiverServer_RemoveHTTPConnectionsParams *) calloc( 1, sizeof( *params ) );
	require_action( params, exit, err = kNoMemoryErr );
	
	CFRetain( inHTTPServer );			// it will be released in _RemoveHTTPServerConnections
	params->httpServer = inHTTPServer;
	params->connectionTypeToRemove = inConnectionTypeToRemove;
	
	dispatch_sync_f( inServer->httpQueue, params, _RemoveHTTPServerConnections );
	params = NULL;
	
exit:
	if( params ) free( params );
}

//===========================================================================================================================
//	_RemoveAllAudioConnections
//
//  This function should NOT be called on inServer->httpQueue.
//===========================================================================================================================

static void _RemoveAllAudioConnections( void *inArg )
{
	AirPlayReceiverServerRef airPlayReceiverServer = (AirPlayReceiverServerRef) inArg;
	if( !airPlayReceiverServer )				return;
	
	// Note that if we failed to tear down some MediaControl context for unified path, we won't tear down that connection here,
	// and the sender might continue to think it is still airplaying...
	
	_RemoveHTTPServerConnectionsOfType( airPlayReceiverServer, airPlayReceiverServer->httpServer, kAirPlayHTTPConnectionTypeAudio );
	
#if( AIRPLAY_HTTP_SERVER_LEGACY )
	_RemoveHTTPServerConnectionsOfType( airPlayReceiverServer, airPlayReceiverServer->httpServerLegacy, kAirPlayHTTPConnectionTypeAudio );
#endif
	CFRelease( airPlayReceiverServer );
	aprs_ulog( kLogLevelNotice, "Stopped all AirPlay Audio Connections\n" );
}

//===========================================================================================================================
//	_RemoveAllConnections
//
//  This function should NOT be called on inServer->httpQueue.
//===========================================================================================================================

static void _RemoveAllConnections( AirPlayReceiverServerRef inServer )
{
	if( !inServer )					return;
	
	_RemoveHTTPServerConnectionsOfType( inServer, inServer->httpServer, kAirPlayHTTPConnectionTypeAny );
	
#if( AIRPLAY_HTTP_SERVER_LEGACY )
	_RemoveHTTPServerConnectionsOfType( inServer, inServer->httpServerLegacy, kAirPlayHTTPConnectionTypeAny );
#endif
}

//===========================================================================================================================
//	_GetListenPort
//===========================================================================================================================

static int	_GetListenPort( AirPlayReceiverServerRef inServer )
{
	if( inServer->httpServer ) return( inServer->httpServer->listeningPort );
	return( 0 );
}

//===========================================================================================================================
//	AirPlayReceiverServerStopAllConnections
//===========================================================================================================================

void	AirPlayReceiverServerStopAllAudioConnections( AirPlayReceiverServerRef inServer )
{
	if( !inServer ) return;
	
	CFRetain( inServer );
	dispatch_async_f( inServer->queue, inServer, _RemoveAllAudioConnections );
}

#if 0
#pragma mark -
#pragma mark == AirPlayReceiverConnection ==
#endif

//===========================================================================================================================
//	_HandleHTTPConnectionCreated
//===========================================================================================================================

static void
	_HandleHTTPConnectionCreated( 
		HTTPServerRef		inServer, 
		HTTPConnectionRef	inCnx, 
		void *				inCnxExtraPtr, 
		void *				inContext )
{
	AirPlayReceiverServerRef const		server	= (AirPlayReceiverServerRef) inContext;
	AirPlayReceiverConnectionRef const	cnx		= (AirPlayReceiverConnectionRef) inCnxExtraPtr;
	HTTPConnectionDelegate		delegate;
	
	(void) inServer;
	
	cnx->server  = server;
	cnx->httpCnx = inCnx;
	
	HTTPConnectionDelegateInit( &delegate );
	delegate.context			= cnx;
	delegate.initialize_f		= _HandleHTTPConnectionInitialize;
	delegate.finalize_f			= _HandleHTTPConnectionFinalize;
	delegate.close_f			= _HandleHTTPConnectionClose;
	delegate.handleMessage_f	= _HandleHTTPConnectionMessage;
	HTTPConnectionSetDelegate( inCnx, &delegate );
}

//===========================================================================================================================
//	_HandleHTTPConnectionClose
//===========================================================================================================================

static void	_HandleHTTPConnectionClose( HTTPConnectionRef inCnx, void *inContext )
{
	AirPlayReceiverConnectionRef const		cnx		= (AirPlayReceiverConnectionRef) inContext;
	AirPlayReceiverSessionRef				session;
	Boolean									allStreamsTornDown = false;
	
	(void) inCnx;
	
	session	= cnx->session;
	cnx->session = NULL;
	if( session )
	{
		// Check to see if all streams have been torn down.
		
		if(	( session->mainAudioCtx.type == kAirPlayStreamType_Invalid )
			&& ( session->altAudioCtx.type  == kAirPlayStreamType_Invalid )
			&& !session->screenInitialized
			)
		{
			allStreamsTornDown = true;
		}
		
		AirPlayReceiverSessionTearDown( session, NULL, ( !cnx->didAnnounce || allStreamsTornDown ) ? kNoErr : kConnectionErr, NULL );
		CFRelease( session );
		AirPlayReceiverServerSetBoolean( gAirPlayReceiverServer, CFSTR( kAirPlayProperty_Playing ), NULL, false );
	}
	
}


//===========================================================================================================================
//	_HandleHTTPConnectionInitialize
//===========================================================================================================================

static OSStatus _HandleHTTPConnectionInitialize( HTTPConnectionRef inCnx, void *inContext )
{
	OSStatus err;
	AirPlayReceiverConnectionRef const		cnx		= (AirPlayReceiverConnectionRef) inContext;
	
	cnx->compressionType	= kAirPlayCompressionType_Undefined;
    SocketSetKeepAlive( inCnx->sock, kAirPlayDataTimeoutSecs / 5, 5 );
	err	= kNoErr;
	
	return err;
}

//===========================================================================================================================
//	_HandleHTTPConnectionFinalize
//===========================================================================================================================

static void _HandleHTTPConnectionFinalize( HTTPConnectionRef inCnx, void *inContext )
{
	AirPlayReceiverConnectionRef const		cnx		= (AirPlayReceiverConnectionRef) inContext;
	
	(void) inCnx;

	cnx->server = NULL;
	
#if( TARGET_OS_POSIX )
	if( cnx->networkChangeListener )
	{
		NetworkChangeListenerStop( cnx->networkChangeListener );
		ForgetCF( &cnx->networkChangeListener );
	}
#endif
	ForgetCF( &cnx->pairSetupSessionHomeKit );
	ForgetCF( &cnx->pairVerifySessionHomeKit );
	ForgetCF( &cnx->logs );
	if( cnx->MFiSAP )
	{
		MFiSAP_Delete( cnx->MFiSAP );
		cnx->MFiSAP = NULL;
		cnx->MFiSAPDone = false;
	}
}

//===========================================================================================================================
//	_DestroyConnection
//===========================================================================================================================

static void _DestroyConnection( HTTPConnectionRef inCnx )
{
	HTTPConnectionStop( inCnx );
	if( inCnx->selfAddr.sa.sa_family != AF_UNSPEC )
	{
		aprs_ulog( kLogLevelInfo, "Closing  connection from %##a to %##a\n", &inCnx->peerAddr, &inCnx->selfAddr );
	}
	CFRelease( inCnx );
}

//===========================================================================================================================
//	_HandleHTTPConnectionMessage
//===========================================================================================================================

#define GetHeaderValue( req, name, outVal, outValLen ) \
	HTTPGetHeaderField( (req)->header.buf, (req)->header.len, name, NULL, NULL, outVal, outValLen, NULL )

static OSStatus _HandleHTTPConnectionMessage( HTTPConnectionRef inCnx, HTTPMessageRef inRequest, void *inContext )
{
	OSStatus				err;
	HTTPMessageRef			response	= inCnx->responseMsg;
	const char * const		methodPtr	= inRequest->header.methodPtr;
	size_t const			methodLen	= inRequest->header.methodLen;
	const char * const		pathPtr		= inRequest->header.url.pathPtr;
	size_t const			pathLen		= inRequest->header.url.pathLen;
	Boolean					logHTTP		= true;
	const char *			httpProtocol;
	HTTPStatus				status;
	const char *			cSeqPtr		= NULL;
	size_t					cSeqLen		= 0;
	AirPlayReceiverConnectionRef	cnx	= (AirPlayReceiverConnectionRef) inContext;
	
	require_action( cnx, exit, err = kParamErr );
	
	httpProtocol = ( strnicmp_prefix(inRequest->header.protocolPtr, inRequest->header.protocolLen, "HTTP" ) == 0 )
		? "HTTP/1.1" : kAirTunesHTTPVersionStr;
	
	inCnx->delegate.httpProtocol = httpProtocol;
	
	// OPTIONS and /feedback requests are too chatty so don't log them by default.
	if( ( ( strnicmpx( methodPtr, methodLen, "OPTIONS" ) == 0 ) ||
		( ( strnicmpx( methodPtr, methodLen, "POST" ) == 0 ) && ( strnicmp_suffix( pathPtr, pathLen, "/feedback" ) == 0 ) ) ) &&
		!log_category_enabled( aprs_http_ucat(), kLogLevelChatty ) )
	{
		logHTTP = false;
	}
	if( logHTTP ) LogHTTP( aprs_http_ucat(), aprs_http_ucat(), inRequest->header.buf, inRequest->header.len,
		inRequest->bodyPtr, inRequest->bodyLen );
	
	if( cnx->session ) ++cnx->session->source.activityCount;
	
	GetHeaderValue( inRequest, kHTTPHeader_CSeq, &cSeqPtr, &cSeqLen );
	
	// Parse the client device's ID. If not provided (e.g. older device) then fabricate one from the IP address.
	
	HTTPScanFHeaderValue( inRequest->header.buf, inRequest->header.len, kAirPlayHTTPHeader_DeviceID, "%llx", &cnx->clientDeviceID );
	if( cnx->clientDeviceID == 0 ) cnx->clientDeviceID = SockAddrToDeviceID( &inCnx->peerAddr );
	
	if( *cnx->clientName == '\0' )
	{
		const char *		namePtr	= NULL;
		size_t				nameLen	= 0;
		
		GetHeaderValue( inRequest, kAirPlayHTTPHeader_ClientName, &namePtr, &nameLen );
		if( nameLen > 0 ) TruncateUTF8( namePtr, nameLen, cnx->clientName, sizeof( cnx->clientName ), true );
	}
	
	if( strnicmp_suffix( pathPtr, pathLen, "/logs" ) == 0 )
	{
		status = _requestProcessGetLogs( cnx, inRequest );
		goto SendResponse;
	}

	// Reject requests except PIN-based pair-setup or pair-verify until paired when using pairAll or P2P connections.
	if( !cnx->pairingVerified && PairingRequired( inCnx, cnx ) &&
		( ( strnicmpx( methodPtr, methodLen, "POST" ) != 0 ) ||
		  ( ( strnicmp_suffix( pathPtr, pathLen, "/pair-setup" )		!= 0 ) &&
		    ( strnicmp_suffix( pathPtr, pathLen, "/pair-verify" )		!= 0 ) ) ) )
	{
		aprs_ulog( kLogLevelNotice, "### Unverified RTSP request denied: %.*s %.*s\n",
			(int) methodLen, methodPtr, (int) pathLen, pathPtr );
		_requestReportIfIncompatibleSender( cnx, inRequest );
		status = kHTTPStatus_Forbidden;
		goto SendResponse;
	}
	
	// Reject requests except PIN pairing URLs until paired when using PIN pairing.
	
	if( !cnx->pairingVerified && gAirPlayReceiverServer->pairPIN &&
		!( ( strnicmpx( methodPtr, methodLen, "GET" ) == 0 ) &&
		   ( strnicmp_suffix( pathPtr, pathLen, "/info" ) == 0 ) ) &&
		( ( strnicmpx( methodPtr, methodLen, "POST" ) != 0 ) ||
		  ( ( strnicmp_suffix( pathPtr, pathLen, "/pair-setup" )		!= 0 ) &&
		    ( strnicmp_suffix( pathPtr, pathLen, "/pair-verify" )		!= 0 ) ) ) )
	{
		aprs_ulog( kLogLevelNotice, "### Unverified RTSP request in PIN mode denied: %.*s %.*s\n",
			(int) methodLen, methodPtr, (int) pathLen, pathPtr );
		_requestReportIfIncompatibleSender( cnx, inRequest );
		status = kHTTPStatus_Forbidden;
		goto SendResponse;
	}
	
	// Process the request. Assume success initially, but we'll change it if there is an error.
	// Note: methods are ordered below roughly to how often they are used (most used earlier).
	
	err = HTTPHeader_InitResponse( &response->header, httpProtocol, kHTTPStatus_OK, NULL );
	require_noerr( err, exit );
	response->bodyLen = 0;
	
	if(      strnicmpx( methodPtr, methodLen, "OPTIONS" )			== 0 ) status = _requestProcessOptions( cnx );
	else if( strnicmpx( methodPtr, methodLen, "SET_PARAMETER" )		== 0 ) status = _requestProcessSetParameter( cnx, inRequest );
	else if( strnicmpx( methodPtr, methodLen, "FLUSH" )				== 0 ) status = _requestProcessFlush( cnx, inRequest );
	else if( strnicmpx( methodPtr, methodLen, "GET_PARAMETER" ) 	== 0 ) status = _requestProcessGetParameter( cnx, inRequest );
	else if( strnicmpx( methodPtr, methodLen, "RECORD" )			== 0 ) status = _requestProcessRecord( cnx, inRequest );
	else if( strnicmpx( methodPtr, methodLen, "SETUP" )				== 0 ) status = _requestProcessSetup( cnx, inRequest );
	else if( strnicmpx( methodPtr, methodLen, "TEARDOWN" )			== 0 ) status = _requestProcessTearDown( cnx, inRequest );
	else if( strnicmpx( methodPtr, methodLen, "GET" )				== 0 )
	{
		if( 0 ) {}
		else if( strnicmp_suffix( pathPtr, pathLen, "/log" )		== 0 ) status = _requestProcessGetLog( cnx );
		else if( strnicmp_suffix( pathPtr, pathLen, "/info" )		== 0 ) status = _requestProcessInfo( cnx, inRequest );
		else { dlog( kLogLevelNotice, "### Unsupported GET: '%.*s'\n", (int) pathLen, pathPtr ); status = kHTTPStatus_NotFound; }
	}
	else if( strnicmpx( methodPtr, methodLen, "POST" )				== 0 )
	{
		if( 0 ) {}
		else if( strnicmp_suffix( pathPtr, pathLen, "/auth-setup" )	== 0 ) status = _requestProcessAuthSetup( cnx, inRequest );
		else if( strnicmp_suffix( pathPtr, pathLen, "/command" )	== 0 ) status = _requestProcessCommand( cnx, inRequest );
		else if( strnicmp_suffix( pathPtr, pathLen, "/diag-info" )	== 0 ) status = kHTTPStatus_OK;
		else if( strnicmp_suffix( pathPtr, pathLen, "/feedback" )	== 0 ) status = _requestProcessFeedback( cnx, inRequest );
		else if( strnicmp_suffix( pathPtr, pathLen, "/info" )		== 0 ) status = _requestProcessInfo( cnx, inRequest );
		else if( strnicmp_suffix( pathPtr, pathLen, "/pair-setup" )		== 0 ) status = _requestProcessPairSetup( cnx, inRequest );
		else if( strnicmp_suffix( pathPtr, pathLen, "/pair-verify" )	== 0 ) status = _requestProcessPairVerify( cnx, inRequest );
		else { dlogassert( "Bad POST: '%.*s'", (int) pathLen, pathPtr ); status = kHTTPStatus_NotFound; }
	}
	else { dlogassert( "Bad method: %.*s", (int) methodLen, methodPtr ); status = kHTTPStatus_NotImplemented; }
	goto SendResponse;
	
SendResponse:
	
	// If an error occurred, reset the response message with a new status.
	
	if( status != kHTTPStatus_OK )
	{
		err = HTTPHeader_InitResponse( &response->header, httpProtocol, status, NULL );
		require_noerr( err, exit );
		response->bodyLen = 0;
		
		err = HTTPHeader_SetField( &response->header, kHTTPHeader_ContentLength, "0" );
		require_noerr( err, exit );
	}
	
	// Server
	
	err = HTTPHeader_SetField( &response->header, kHTTPHeader_Server, "AirTunes/%s", kAirPlaySourceVersionStr );
	require_noerr( err, exit );
	
	// WWW-Authenticate
	
	if( status == kHTTPStatus_Unauthorized )
	{

		char		nonce[ 64 ];
		
		err = HTTPMakeTimedNonce( kHTTPTimedNonceETagPtr, kHTTPTimedNonceETagLen,
			cnx->server->httpTimedNonceKey, sizeof( cnx->server->httpTimedNonceKey ),
			nonce, sizeof( nonce ), NULL );
		require_noerr( err, exit );
		
		err = HTTPHeader_SetField( &response->header, kHTTPHeader_WWWAuthenticate, "Digest realm=\"airplay\", nonce=\"%s\"", nonce );
		require_noerr( err, exit );
	}
	
	// CSeq
	
	if( cSeqPtr )
	{
		err = HTTPHeader_SetField( &response->header, kHTTPHeader_CSeq, "%.*s", (int) cSeqLen, cSeqPtr );
		require_noerr( err, exit );
	}
	
	// Apple-Challenge
	
	if( logHTTP ) LogHTTP( aprs_http_ucat(), aprs_http_ucat(), response->header.buf, response->header.len,
		response->bodyPtr, response->bodyLen );
	
	err = HTTPConnectionSendResponse( inCnx );
	require_noerr( err, exit );
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Request Processing ==
#endif

//===========================================================================================================================
//	_requestProcessAuthSetup
//===========================================================================================================================

static HTTPStatus _requestProcessAuthSetup( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus				status;
	OSStatus				err;
	uint8_t *				outputPtr;
	size_t					outputLen;
	HTTPMessageRef			response	= inCnx->httpCnx->responseMsg;
	
	aprs_ulog( kAirPlayPhaseLogLevel, "MFi\n" );
	outputPtr = NULL;
	require_action( inRequest->bodyOffset > 0, exit, status = kHTTPStatus_BadRequest );
	
	// Let MFi-SAP process the input data and generate output data.
	
	if( inCnx->MFiSAPDone && inCnx->MFiSAP )
	{
		MFiSAP_Delete( inCnx->MFiSAP );
		inCnx->MFiSAP = NULL;
		inCnx->MFiSAPDone = false;
	}
	if( !inCnx->MFiSAP )
	{
		err = MFiSAP_Create( &inCnx->MFiSAP, kMFiSAPVersion1 );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	}
	
	err = MFiSAP_Exchange( inCnx->MFiSAP, inRequest->bodyPtr, inRequest->bodyOffset, &outputPtr, &outputLen, &inCnx->MFiSAPDone );
	require_noerr_action( err, exit, status = kHTTPStatus_Forbidden );
	
	// Send the MFi-SAP output data in the response.
	
	err = HTTPMessageSetBody( response, kMIMEType_Binary, outputPtr, outputLen );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	
	status = kHTTPStatus_OK;
	
exit:
	if( outputPtr ) free( outputPtr );
	return( status );
}

//===========================================================================================================================
//	_requestProcessCommand
//===========================================================================================================================

static HTTPStatus _requestProcessCommand( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus				status;
	OSStatus				err;
	CFDictionaryRef			requestDict;
	CFStringRef				command = NULL;
	CFDictionaryRef			params;
	CFDictionaryRef			responseDict;
	
	requestDict = CFDictionaryCreateWithBytes( inRequest->bodyPtr, inRequest->bodyOffset, &err );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );
	
	command = CFDictionaryGetCFString( requestDict, CFSTR( kAirPlayKey_Type ), NULL );
	require_action( command, exit, err = kParamErr; status = kHTTPStatus_ParameterNotUnderstood );
	
	params = CFDictionaryGetCFDictionary( requestDict, CFSTR( kAirPlayKey_Params ), NULL );
	
	// Perform the command and send its response.
	
	require_action( inCnx->session, exit, err = kNotPreparedErr; status = kHTTPStatus_SessionNotFound );
	responseDict = NULL;
	err = AirPlayReceiverSessionControl( inCnx->session, kCFObjectFlagDirect, command, NULL, params, &responseDict );
	require_noerr_action_quiet( err, exit, status = kHTTPStatus_UnprocessableEntity );
	if( !responseDict )
	{
		responseDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( responseDict, exit, err = kNoMemoryErr; status = kHTTPStatus_InternalServerError );
	}
	
	status = _requestSendPlistResponse( inCnx->httpCnx, inRequest, responseDict, &err );
	CFRelease( responseDict );
	
exit:
	if( err ) aprs_ulog( kLogLevelNotice, "### Command '%@' failed: %#m, %#m\n", command, status, err );
	CFReleaseNullSafe( requestDict );
	return( status );
}

//===========================================================================================================================
//	_requestProcessFeedback
//===========================================================================================================================

static HTTPStatus _requestProcessFeedback( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus					status;
	OSStatus					err;
	CFDictionaryRef				input	= NULL;
	CFMutableDictionaryRef		output	= NULL;
	CFDictionaryRef				dict;
	
	input = CFDictionaryCreateWithBytes( inRequest->bodyPtr, inRequest->bodyOffset, &err );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );
	
	output = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( output, exit, err = kNoMemoryErr; status = kHTTPStatus_InternalServerError );
	
	if( inCnx->session )
	{
		dict = NULL;
		AirPlayReceiverSessionControl( inCnx->session, kCFObjectFlagDirect, CFSTR( kAirPlayCommand_UpdateFeedback ), NULL,
			input, &dict );
		if( dict )
		{
			CFDictionaryMergeDictionary( output, dict );
			CFRelease( dict );
		}
	}
	
	status = _requestSendPlistResponse( inCnx->httpCnx, inRequest, ( CFDictionaryGetCount( output ) > 0 ) ? output : NULL, &err );
	
exit:
	if( err ) aprs_ulog( kLogLevelNotice, "### Feedback failed: %#m, %#m\n", status, err );
	CFReleaseNullSafe( input );
	CFReleaseNullSafe( output );
	return( status );
}

//===========================================================================================================================
//	_requestProcessFlush
//===========================================================================================================================

static HTTPStatus _requestProcessFlush( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	OSStatus				err;
	HTTPStatus				status;
	uint16_t				flushSeq;
	uint32_t				flushTS;
	uint32_t				lastPlayedTS;
	HTTPMessageRef			response	= inCnx->httpCnx->responseMsg;
	
	aprs_ulog( kLogLevelInfo, "Flush\n" );
	
	if( !inCnx->didRecord )
	{
		dlogassert( "FLUSH not allowed when not playing" );
		status = kHTTPStatus_MethodNotValidInThisState;
		goto exit;
	}
	
	// Flush everything before the specified seq/ts.
	
	err = HTTPParseRTPInfo( inRequest->header.buf, inRequest->header.len, &flushSeq, &flushTS );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );
	
	err = AirPlayReceiverSessionFlushAudio( inCnx->session, flushTS, flushSeq, &lastPlayedTS );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	
	err = HTTPHeader_SetField( &response->header, kHTTPHeader_RTPInfo, "rtptime=%u", lastPlayedTS );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	
	status = kHTTPStatus_OK;
	
exit:
	return( status );
}

//===========================================================================================================================
//	_requestProcessGetLog
//===========================================================================================================================

static HTTPStatus _requestProcessGetLog( AirPlayReceiverConnectionRef inCnx )
{
	HTTPStatus		status;
	OSStatus		err;
	DataBuffer		dataBuf;
	uint8_t *		ptr;
	size_t			len;
	HTTPMessageRef	response = inCnx->httpCnx->responseMsg;
	
	DataBuffer_Init( &dataBuf, NULL, 0, 10 * kBytesPerMegaByte );
	
	{
	}
	
	DataBuffer_AppendF( &dataBuf, "AirPlay Diagnostics\n" );
	DataBuffer_AppendF( &dataBuf, "===================\n" );
	AirTunesDebugAppendShowData( "globals", &dataBuf );
	AirTunesDebugAppendShowData( "stats", &dataBuf );
	AirTunesDebugAppendShowData( "rtt", &dataBuf );
	AirTunesDebugAppendShowData( "retrans", &dataBuf );
	AirTunesDebugAppendShowData( "retransDone", &dataBuf );
	
#if( TARGET_OS_POSIX )
	DataBuffer_AppendF( &dataBuf, "+-+ syslog +--\n" );
	DataBuffer_RunProcessAndAppendOutput( &dataBuf, "syslog" );
	DataBuffer_AppendFile( &dataBuf, kAirPlayPrimaryLogPath );
#endif
	
	err = DataBuffer_Commit( &dataBuf, &ptr, &len );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	
	err = HTTPMessageSetBody( response, "text/plain", ptr, len );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	
	status = kHTTPStatus_OK;
	
exit:
	DataBuffer_Free( &dataBuf );
	return( status );
}

//===========================================================================================================================
//	_createLogsDataBuffer
//===========================================================================================================================

static void _createLogsDataBuffer( void *inArg )
{
	AirPlayReceiverLogsRef		inLogs = (AirPlayReceiverLogsRef) inArg;
	OSStatus					err;
	DataBuffer *				dataBuffer = NULL;
	CFDictionaryRef				dict;
	char						path[ PATH_MAX ];

	require_action( !inLogs->dataBuffer, exit, err = kInternalErr );

	dataBuffer = (DataBuffer *) malloc( sizeof( DataBuffer ) );
	require_action( dataBuffer, exit, err = kNoMemoryErr );

	DataBuffer_Init( dataBuffer, NULL, 0, 10 * kBytesPerMegaByte );

	dict = NULL;

	// Call delegate to get logs
	err = AirPlayReceiverServerControl( inLogs->server, kCFObjectFlagDirect, CFSTR( kAirPlayCommand_GetLogs ), NULL, NULL, &dict );
	require_noerr( err, exit );

	require_action( dict, exit, err = kUnknownErr );
	CFDictionaryGetCString( dict, CFSTR( kAirPlayKey_Path), path, PATH_MAX, &err );

	err = DataBuffer_AppendFile( dataBuffer, path );
	remove( path );
	require_noerr( err, exit );

	inLogs->dataBuffer = dataBuffer;
	dataBuffer = NULL;
	
exit:
	inLogs->status = err ? kHTTPStatus_InternalServerError : kHTTPStatus_OK;
	
	// Must set last for synchronization purposes
	inLogs->pending	= false;
	
	if( dataBuffer ) DataBuffer_Free( dataBuffer );
	check_noerr( err );
	
	CFRelease( inLogs );
}

//===========================================================================================================================
//	_requestProcessRetrieveLogs
//===========================================================================================================================

static HTTPStatus _requestProcessLogsRetrieve( AirPlayReceiverConnectionRef inCnx )
{
	HTTPStatus			status;
	OSStatus			err;
	uint8_t *			ptr;
	size_t				len;
	HTTPMessageRef		response = inCnx->httpCnx->responseMsg;
	
	require_action( inCnx->logs, exit, status = kHTTPStatus_NotFound );
	
	if( inCnx->logs->pending )
	{
		// Send 202 (Accepted) with Retry-After and Location
		err = HTTPHeader_InitResponse( &response->header, inCnx->httpCnx->delegate.httpProtocol, kHTTPStatus_Accepted, NULL );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
		
		HTTPHeader_SetField( &response->header, "Retry-After", "5" ); // Retry in 5 sec
		HTTPHeader_SetField( &response->header, "Location", "/Logs?id=%u", inCnx->logs->requestID );
		response->bodyLen = 0;
		
		// Return "OK" here to indicate that we prepared the response message ourselves
		status = kHTTPStatus_OK;
	}
	else
	{
		if( inCnx->logs->dataBuffer )
		{
			err = HTTPHeader_InitResponse( &response->header, inCnx->httpCnx->delegate.httpProtocol, kHTTPStatus_OK, NULL );
			require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
			response->bodyLen = 0;
			
			err = DataBuffer_Commit( inCnx->logs->dataBuffer, &ptr, &len );
			require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
			
			err = HTTPMessageSetBody( response, "application/x-gzip", ptr, len );
			require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
			
			DataBuffer_Free( inCnx->logs->dataBuffer );
			inCnx->logs->dataBuffer = NULL;
			inCnx->logs->requestID = 0;
			
			// Return "OK" here to indicate that we prepared the response message ourselves
			status = kHTTPStatus_OK;
		}
		else
		{
			check( inCnx->logs->status != kHTTPStatus_OK );
			status = inCnx->logs->status;
		}
		
		// Clean up d
		ForgetCF( &inCnx->logs );
	}

exit:
	return( status );
}

static HTTPStatus _requestProcessLogsInitiate( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus			status;
	OSStatus			err;
	const char *		headerPtr;
	size_t				headerLen;
	Boolean				replyAsync;
	HTTPMessageRef		response;
	
	// We only support one request for logs at a time
	require_action_quiet( !inCnx->logs || !inCnx->logs->pending, exit, status = kHTTPStatus_TooManyRequests );
	
	// Dispose of stale, unretrieved logs
	ForgetCF( &inCnx->logs );
	
	// Set up new logs
	err = AirPlayReceiverLogsCreate( inCnx->server, &inCnx->logs );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError);
	
	err = HTTPGetHeaderField( inRequest->header.buf, inRequest->header.len, "Prefer", NULL, NULL, &headerPtr, &headerLen, NULL );
	replyAsync = ( ( err == kNoErr ) && ( NULL != strnstr( headerPtr, "respond-async", headerLen ) ) );
	
	if( !replyAsync )
	{
		CFRetain( inCnx->logs ); // Will be released in _createLogsDataBuffer
		_createLogsDataBuffer( inCnx->logs );
		require_action( inCnx->logs->dataBuffer, exit, status = inCnx->logs->status );
		
		// Retrieve the results immediately since we were called synchronously
		status = _requestProcessLogsRetrieve( inCnx );
	}
	else
	{
		RandomBytes( &inCnx->logs->requestID, sizeof( inCnx->logs->requestID ) );
		if( inCnx->logs->requestID == 0 ) ++inCnx->logs->requestID; // Make sure it isn't zero

		CFRetain( inCnx->logs ); // Will be released in _createLogsDataBuffer
		dispatch_async_f( dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_DEFAULT, 0 ), inCnx->logs, _createLogsDataBuffer );
		
		// Send 202 (Accepted) with Retry-After and Location
		
		response = inCnx->httpCnx->responseMsg;
		
		err = HTTPHeader_InitResponse( &response->header, inCnx->httpCnx->delegate.httpProtocol, kHTTPStatus_Accepted, NULL );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
		
		HTTPHeader_SetField( &response->header, "Retry-After", "5" ); // Retry in 5 sec
		HTTPHeader_SetField( &response->header, "Location", "/Logs?id=%u", inCnx->logs->requestID );
		response->bodyLen = 0;
		
		// Return "OK" here to indicate that we prepared the response message ourselves
		status = kHTTPStatus_OK;
	}
	
exit:
	return( status );
}

//===========================================================================================================================
//	_requestProcessGetLogs
//===========================================================================================================================

static HTTPStatus _requestProcessGetLogs( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus			status;
	OSStatus			err;
	int					matchCount;
	const char *		idValue;
	size_t				idValueLen;
	unsigned int		requestID;
	
	err = HTTPMessageGetOrCopyFormVariable( inRequest, "id", &idValue, &idValueLen, NULL );
	if( err == kNoErr )
	{
		matchCount = SNScanF( idValue, idValueLen, "%u", &requestID );
		require_action( matchCount > 0, exit, status = kHTTPStatus_BadRequest );
		
		require_action( requestID == inCnx->logs->requestID, exit, status = kHTTPStatus_NotFound );
		status = _requestProcessLogsRetrieve( inCnx );
		
		ForgetCF( &inCnx->logs );
	}
	else
	{
		status = _requestProcessLogsInitiate( inCnx, inRequest );
	}
	
exit:
	return( status );
}

//===========================================================================================================================
//	_requestProcessInfo
//===========================================================================================================================

static HTTPStatus _requestProcessInfo( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus					status;
	OSStatus					err;
	CFMutableDictionaryRef		requestDict;
	CFMutableArrayRef			qualifier = NULL;
	CFDictionaryRef				responseDict;
	const char *				src;
	const char *				end;
	const char *				namePtr;
	size_t						nameLen;
	char *						nameBuf;
	uint32_t					userVersion = 0;
	
	requestDict = CFDictionaryCreateMutableWithBytes( inRequest->bodyPtr, inRequest->bodyOffset, &err );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );
	
	qualifier = (CFMutableArrayRef) CFDictionaryGetCFArray( requestDict, CFSTR( kAirPlayKey_Qualifier ), NULL );
	if( qualifier ) CFRetain( qualifier );
	
	src = inRequest->header.url.queryPtr;
	end = src + inRequest->header.url.queryLen;
	while( ( err = URLGetOrCopyNextVariable( src, end, &namePtr, &nameLen, &nameBuf, NULL, NULL, NULL, &src ) ) == kNoErr )
	{
		err = CFArrayEnsureCreatedAndAppendCString( &qualifier, namePtr, nameLen );
		if( nameBuf ) free( nameBuf );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	}
	
	HTTPScanFHeaderValue( inRequest->header.buf, inRequest->header.len, kAirPlayHTTPHeader_ProtocolVersion, "%u", &userVersion );
	if( inCnx->session ) AirPlayReceiverSessionSetUserVersion( inCnx->session, userVersion );
	
	responseDict = AirPlayCopyServerInfo( inCnx->session, qualifier, inCnx->httpCnx->ifMACAddress , &err );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	
	status = _requestSendPlistResponse( inCnx->httpCnx, inRequest, responseDict, &err );
	CFReleaseNullSafe( responseDict );
	
exit:
	CFReleaseNullSafe( qualifier );
	CFReleaseNullSafe( requestDict );
	if( err ) aprs_ulog( kLogLevelNotice, "### Get info failed: %#m, %#m\n", status, err );
	return( status );
}

//===========================================================================================================================
//	_requestProcessOptions
//===========================================================================================================================

static HTTPStatus _requestProcessOptions( AirPlayReceiverConnectionRef inCnx )
{
	HTTPMessageRef response = inCnx->httpCnx->responseMsg;
	
	HTTPHeader_SetField( &response->header, kHTTPHeader_Public,
		"ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER, POST, GET, PUT" );
	
	return( kHTTPStatus_OK );
}

//===========================================================================================================================
//	_requestProcessPairSetupHomeKit
//===========================================================================================================================

static HTTPStatus _requestProcessPairSetupHomeKit( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest, int inHomeKitPairingType )
{
	HTTPStatus				status;
	OSStatus				err;
	const char *			cptr;
	uint8_t					deviceID[ 6 ];
	char					cstr[ 32 ];
	char					fixedPin[8];
	uint8_t *				outputPtr	= NULL;
	size_t					outputLen;
	Boolean					done;
	HTTPMessageRef			response	= inCnx->httpCnx->responseMsg;
	
	aprs_ulog( kLogLevelNotice, "Control pair-setup HK, type %d\n", inHomeKitPairingType );
	
	{
		strcpy( fixedPin, "3939" );
		cptr = fixedPin;
	}
	
	// Perform the next stage of the PIN process.
	
	if( !inCnx->pairSetupSessionHomeKit )
	{
		err = PairingSessionCreate( &inCnx->pairSetupSessionHomeKit, NULL, kPairingSessionType_SetupServer );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
		PairingSessionSetKeychainInfo_AirPlay( inCnx->pairSetupSessionHomeKit );
		PairingSessionSetLogging( inCnx->pairSetupSessionHomeKit, aprs_ucat() );
		
		AirPlayGetDeviceID( deviceID );
		MACAddressToCString( deviceID, cstr );
		err = PairingSessionSetIdentifier( inCnx->pairSetupSessionHomeKit, cstr, kSizeCString );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	}
	
	err = PairingSessionSetSetupCode( inCnx->pairSetupSessionHomeKit, cptr, kSizeCString );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	
	err = PairingSessionExchange( inCnx->pairSetupSessionHomeKit, inRequest->bodyPtr, inRequest->bodyOffset,
		&outputPtr, &outputLen, &done );
	if( err == kBackoffErr ) { status = kHTTPStatus_NotEnoughBandwidth; goto exit; }
	require_noerr_action_quiet( err, exit, status = kHTTPStatus_ConnectionAuthorizationRequired );
	if( done )
	{
		*gAirPlayReceiverServer->pairPINStr = '\0';
		ForgetCF( &inCnx->pairSetupSessionHomeKit );
	}
	
	err = HTTPMessageSetBody( response, kMIMEType_AppleBinaryPlist, outputPtr, outputLen );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	
	status = kHTTPStatus_OK;
	
exit:
	if( outputPtr ) free( outputPtr );
	if( err ) aprs_ulog( kLogLevelNotice, "### Control pair-setup HK failed: %#m\n", err );
	return( status );
}

//===========================================================================================================================
//	_requestProcessPairSetup
//===========================================================================================================================

static HTTPStatus _requestProcessPairSetup( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus				status;
	int						homeKitPairingType = 0;
	Boolean					useHomeKitPairing = false;
	
	useHomeKitPairing = ( HTTPScanFHeaderValue( inRequest->header.buf, inRequest->header.len, kAirPlayHTTPHeader_HomeKitPairing, "%d", &homeKitPairingType ) == 1 );

	if( !useHomeKitPairing )
	{
		_requestReportIfIncompatibleSender( inCnx, inRequest );
		status = kHTTPStatus_Forbidden;
	}
	else
		status = _requestProcessPairSetupHomeKit( inCnx, inRequest, homeKitPairingType );

	return( status );
}

//===========================================================================================================================
//	_HandlePairVerifyHomeKitCompletion
//===========================================================================================================================

static void	_HandlePairVerifyHomeKitCompletion( HTTPMessageRef inMsg )
{
	HTTPConnectionRef				httpCnx	= (HTTPConnectionRef) inMsg->userContext1;
	AirPlayReceiverConnectionRef	cnx		= httpCnx->delegate.context;
	OSStatus						err;
	NetTransportDelegate			delegate;
	uint8_t							readKey[ 32 ], writeKey[ 32 ];
	
	// Set up security for the connection. All future requests and responses will be encrypted.
	
	require_action( cnx->pairVerifySessionHomeKit, exit, err = kNotPreparedErr );
	
	err = PairingSessionDeriveKey( cnx->pairVerifySessionHomeKit, kAirPlayPairingControlKeySaltPtr, kAirPlayPairingControlKeySaltLen, 
		kAirPlayPairingControlKeyReadInfoPtr, kAirPlayPairingControlKeyReadInfoLen, sizeof( readKey ), readKey );
	require_noerr( err, exit );
	
	err = PairingSessionDeriveKey( cnx->pairVerifySessionHomeKit, kAirPlayPairingControlKeySaltPtr, kAirPlayPairingControlKeySaltLen, 
		kAirPlayPairingControlKeyWriteInfoPtr, kAirPlayPairingControlKeyWriteInfoLen, sizeof( writeKey ), writeKey );
	require_noerr( err, exit );
	
	err = NetTransportChaCha20Poly1305Configure( &delegate, aprs_http_ucat(), writeKey, NULL, readKey, NULL );
	require_noerr( err, exit );
	MemZeroSecure( readKey, sizeof( readKey ) );
	MemZeroSecure( writeKey, sizeof( writeKey ) );
	HTTPConnectionSetTransportDelegate( cnx->httpCnx, &delegate );
	cnx->pairingVerified = true;
	aprs_ulog( kLogLevelTrace, "Pair-verify HK succeeded\n" );
	
exit:
	if( err ) 
	{
		ForgetCF( &cnx->pairVerifySessionHomeKit );
		aprs_ulog( kLogLevelWarning, "### Pair-verify HK completion failed: %#m\n", err );
	}
	CFRelease( httpCnx );
}

//===========================================================================================================================
//	_requestProcessPairVerifyHomeKit
//===========================================================================================================================

static HTTPStatus _requestProcessPairVerifyHomeKit( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus				status;
	OSStatus				err;
	Boolean					done;
	uint8_t *				outputPtr	= NULL;
	size_t					outputLen	= 0;
	uint8_t					deviceID[ 6 ];
	char					cstr[ 32 ];
	HTTPMessageRef			response = inCnx->httpCnx->responseMsg;
	
	HTTPScanFHeaderValue( inRequest->header.buf, inRequest->header.len, kAirPlayHTTPHeader_PairDerive, "%d", &inCnx->pairDerive );
	
	if( !inCnx->pairVerifySessionHomeKit )
	{
		err = PairingSessionCreate( &inCnx->pairVerifySessionHomeKit, NULL, kPairingSessionType_VerifyServer );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
		PairingSessionSetKeychainInfo_AirPlay( inCnx->pairVerifySessionHomeKit );
		PairingSessionSetLogging( inCnx->pairVerifySessionHomeKit, aprs_ucat() );
		
		AirPlayGetDeviceID( deviceID );
		MACAddressToCString( deviceID, cstr );
		err = PairingSessionSetIdentifier( inCnx->pairVerifySessionHomeKit, cstr, kSizeCString );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	}
	
	err = PairingSessionExchange( inCnx->pairVerifySessionHomeKit, inRequest->bodyPtr, inRequest->bodyOffset, &outputPtr, &outputLen, &done );
	if( err == kNotPreparedErr ) { status = kHTTPStatus_NetworkAuthenticationRequired; goto exit; }
	if( err == kSignatureErr )   { status = kHTTPStatus_Forbidden; goto exit; }
	require_noerr_action_quiet( err, exit, status = kHTTPStatus_BadRequest );
	if( done ) 
	{
		// Prepare to configure the connection for encryption/decryption, but send the response before setting up encryption. 
	
		CFRetain( inCnx->httpCnx );
		response->userContext1 = inCnx->httpCnx;
		response->completion   = _HandlePairVerifyHomeKitCompletion;
	}
	
	err = HTTPMessageSetBody( response, kMIMEType_Binary, outputPtr, outputLen );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	
	status = kHTTPStatus_OK;
	
exit:
	FreeNullSafe( outputPtr );
	if( err ) aprs_ulog( kLogLevelNotice, "### Control pair-verify HK failed: %d, %#m\n", status, err );
	return( status );
}

//===========================================================================================================================
//	_requestProcessPairVerify
//===========================================================================================================================

static HTTPStatus _requestProcessPairVerify( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus				status;
	int						homeKitPairingType = 0;
	Boolean					useHomeKitPairing;
	
	aprs_ulog( kLogLevelNotice, "Control pair-verify %d\n", inCnx->pairingCount + 1 );
	++inCnx->pairingCount;
	
	HTTPScanFHeaderValue( inRequest->header.buf, inRequest->header.len, kAirPlayHTTPHeader_PairDerive, "%d", &inCnx->pairDerive );
	
	useHomeKitPairing = ( HTTPScanFHeaderValue( inRequest->header.buf, inRequest->header.len, kAirPlayHTTPHeader_HomeKitPairing, "%d", &homeKitPairingType ) == 1 );

	if( !useHomeKitPairing )
	{
		_requestReportIfIncompatibleSender( inCnx, inRequest );
		status = kHTTPStatus_Forbidden;
	}
	else
		status = _requestProcessPairVerifyHomeKit( inCnx, inRequest );
	
	return( status );
}

//===========================================================================================================================
//	_requestProcessGetParameter
//===========================================================================================================================

static HTTPStatus _requestProcessGetParameter( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus				status;
	OSStatus				err;
	const char *			src;
	const char *			end;
	size_t					len;
	char					tempStr[ 256 ];
	char					responseBuf[ 256 ];
	int						n;
	HTTPMessageRef			response	= inCnx->httpCnx->responseMsg;
	
	if( !AirPlayReceiverConnectionDidSetup( inCnx ) )
	{
		dlogassert( "GET_PARAMETER not allowed before SETUP" );
		status = kHTTPStatus_MethodNotValidInThisState;
		goto exit;
	}
	
	// Check content type.
	
	err = GetHeaderValue( inRequest, kHTTPHeader_ContentType, &src, &len );
	if( err )
	{
		dlogassert( "No Content-Type header" );
		status = kHTTPStatus_BadRequest;
		goto exit;
	}
	if( strnicmpx( src, len, "text/parameters" ) != 0 )
	{
		dlogassert( "Bad Content-Type: '%.*s'", (int) len, src );
		status = kHTTPStatus_HeaderFieldNotValid;
		goto exit;
	}
	
	// Parse parameters. Each parameter is formatted as <name>\r\n
	
	src = (const char *) inRequest->bodyPtr;
	end = src + inRequest->bodyOffset;
	while( src < end )
	{
		char				c;
		const char *		namePtr;
		size_t				nameLen;
		
		namePtr = src;
		while( ( src < end ) && ( ( c = *src ) != '\r' ) && ( c != '\n' ) ) ++src;
		nameLen = (size_t)( src - namePtr );
		if( ( nameLen == 0 ) || ( src >= end ) )
		{
			dlogassert( "Bad parameter: '%.*s'", (int) inRequest->bodyOffset, inRequest->bodyPtr );
			status = kHTTPStatus_ParameterNotUnderstood;
			goto exit;
		}
		
		// Process the parameter.
		
		if( 0 ) {}
		
		// Name
		
		else if( strnicmpx( namePtr, nameLen, "name" ) == 0 )
		{
			err = AirPlayGetDeviceName( tempStr, sizeof( tempStr ) );
			require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
			
			n = snprintf( responseBuf, sizeof( responseBuf ), "name: %s\r\n", tempStr );
			err = HTTPMessageSetBody( response, "text/parameters", responseBuf, (size_t) n );
			require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
		}
		
		// Other
		
		else
		{
			dlogassert( "Unknown parameter: '%.*s'", (int) nameLen, namePtr );
			status = kHTTPStatus_ParameterNotUnderstood;
			goto exit;
		}
		
		while( ( src < end ) && ( ( ( c = *src ) == '\r' ) || ( c == '\n' ) ) ) ++src;
	}
	
	status = kHTTPStatus_OK;
	
exit:
	return( status );
}

//===========================================================================================================================
//	_requestProcessSetParameter
//===========================================================================================================================

static HTTPStatus _requestProcessSetParameter( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus				status;
	const char *			src;
	size_t					len;
	
	if( !AirPlayReceiverConnectionDidSetup( inCnx ) )
	{
		dlogassert( "SET_PARAMETER not allowed before SETUP" );
		status = kHTTPStatus_MethodNotValidInThisState;
		goto exit;
	}
	
	src = NULL;
	len = 0;
	GetHeaderValue( inRequest, kHTTPHeader_ContentType, &src, &len );
	if( ( len == 0 ) && ( inRequest->bodyOffset == 0 ) )			status = kHTTPStatus_OK;
	else if( strnicmpx( src, len, "text/parameters" ) == 0 )	status = _requestProcessSetParameterText( inCnx, inRequest );
	else if( MIMETypeIsPlist( src, len ) )						status = _requestProcessSetProperty( inCnx, inRequest );
	else { dlogassert( "Bad Content-Type: '%.*s'", (int) len, src ); status = kHTTPStatus_HeaderFieldNotValid; goto exit; }
	
exit:
	return( status );
}

//===========================================================================================================================
//	_requestProcessSetParameterText
//===========================================================================================================================

static HTTPStatus _requestProcessSetParameterText( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus			status;
	OSStatus			err;
	const char *		src;
	const char *		end;
	(void) inCnx;
	
	// Parse parameters. Each parameter is formatted as <name>: <value>\r\n
	
	src = (const char *) inRequest->bodyPtr;
	end = src + inRequest->bodyOffset;
	while( src < end )
	{
		char				c;
		const char *		namePtr;
		size_t				nameLen;
		const char *		valuePtr;
		size_t				valueLen;
		
		// Parse the name.
		
		namePtr = src;
		while( ( src < end ) && ( *src != ':' ) ) ++src;
		nameLen = (size_t)( src - namePtr );
		if( ( nameLen == 0 ) || ( src >= end ) )
		{
			dlogassert( "Bad parameter: '%.*s'", (int) inRequest->bodyOffset, inRequest->bodyPtr );
			status = kHTTPStatus_ParameterNotUnderstood;
			goto exit;
		}
		++src;
		while( ( src < end ) && ( ( ( c = *src ) == ' ' ) || ( c == '\t' ) ) ) ++src;
		
		// Parse the value.
		
		valuePtr = src;
		while( ( src < end ) && ( ( c = *src ) != '\r' ) && ( c != '\n' ) ) ++src;
		valueLen = (size_t)( src - valuePtr );
		
		// Process the parameter.
		
		if( 0 ) {}
		
		// Other
		
		else
		{
			(void) err;
			(void) valueLen;
			
			dlogassert( "Unknown parameter: '%.*s'", (int) nameLen, namePtr );
			status = kHTTPStatus_ParameterNotUnderstood;
			goto exit;
		}
		
		while( ( src < end ) && ( ( ( c = *src ) == '\r' ) || ( c == '\n' ) ) ) ++src;
	}
	
	status = kHTTPStatus_OK;
	
exit:
	return( status );
}

//===========================================================================================================================
//	_requestProcessSetProperty
//===========================================================================================================================

static HTTPStatus _requestProcessSetProperty( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus				status;
	OSStatus				err;
	CFDictionaryRef			requestDict;
	CFStringRef				property = NULL;
	CFTypeRef				qualifier;
	CFTypeRef				value;
	
	requestDict = CFDictionaryCreateWithBytes( inRequest->bodyPtr, inRequest->bodyOffset, &err );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );
	
	property = CFDictionaryGetCFString( requestDict, CFSTR( kAirPlayKey_Property ), NULL );
	require_action( property, exit, err = kParamErr; status = kHTTPStatus_BadRequest );
	
	qualifier	= CFDictionaryGetValue( requestDict, CFSTR( kAirPlayKey_Qualifier ) );
	value		= CFDictionaryGetValue( requestDict, CFSTR( kAirPlayKey_Value ) );
	
	// Set the property on the session.
	
	require_action( inCnx->session, exit, err = kNotPreparedErr; status = kHTTPStatus_SessionNotFound );
	err = AirPlayReceiverSessionSetProperty( inCnx->session, kCFObjectFlagDirect, property, qualifier, value );
	require_noerr_action_quiet( err, exit, status = kHTTPStatus_UnprocessableEntity );
	
	status = kHTTPStatus_OK;
	
exit:
	if( err ) aprs_ulog( kLogLevelNotice, "### Set property '%@' failed: %#m, %#m\n", property, status, err );
	CFReleaseNullSafe( requestDict );
	return( status );
}

//===========================================================================================================================
//	_requestProcessRecord
//===========================================================================================================================

static HTTPStatus _requestProcessRecord( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus							status;
	OSStatus							err;
	const char *						src;
	const char *						end;
	size_t								len;
	const char *						namePtr;
	size_t								nameLen;
	const char *						valuePtr;
	size_t								valueLen;
	AirPlayReceiverSessionStartInfo		startInfo;
	
	aprs_ulog( kAirPlayPhaseLogLevel, "Record\n" );
	
	if( !AirPlayReceiverConnectionDidSetup( inCnx ) )
	{
		dlogassert( "RECORD not allowed before SETUP" );
		status = kHTTPStatus_MethodNotValidInThisState;
		goto exit;
	}
	
	memset( &startInfo, 0, sizeof( startInfo ) );
	startInfo.clientName	= inCnx->clientName;
	startInfo.transportType	= inCnx->httpCnx->transportType;
	
	// Parse session duration info.
	
	src = NULL;
	len = 0;
	GetHeaderValue( inRequest, kAirPlayHTTPHeader_Durations, &src, &len );
	end = src + len;
	while( HTTPParseParameter( src, end, &namePtr, &nameLen, &valuePtr, &valueLen, NULL, &src ) == kNoErr )
	{
		if(      strnicmpx( namePtr, nameLen, "b" )  == 0 ) startInfo.bonjourMs		= TextToInt32( valuePtr, valueLen, 10 );
		else if( strnicmpx( namePtr, nameLen, "c" )  == 0 ) startInfo.connectMs		= TextToInt32( valuePtr, valueLen, 10 );
		else if( strnicmpx( namePtr, nameLen, "au" ) == 0 ) startInfo.authMs		= TextToInt32( valuePtr, valueLen, 10 );
		else if( strnicmpx( namePtr, nameLen, "an" ) == 0 ) startInfo.announceMs	= TextToInt32( valuePtr, valueLen, 10 );
		else if( strnicmpx( namePtr, nameLen, "sa" ) == 0 ) startInfo.setupAudioMs	= TextToInt32( valuePtr, valueLen, 10 );
		else if( strnicmpx( namePtr, nameLen, "ss" ) == 0 ) startInfo.setupScreenMs	= TextToInt32( valuePtr, valueLen, 10 );
	}
	
	// Start the session.
	
	err = AirPlayReceiverSessionStart( 
		inCnx->session, 
		&startInfo );
	if( AirPlayIsBusyError( err ) ) { status = kHTTPStatus_NotEnoughBandwidth; goto exit; }
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	
#if( TARGET_OS_POSIX )
	err = NetworkChangeListenerCreate( &inCnx->networkChangeListener );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	NetworkChangeListenerSetDispatchQueue( inCnx->networkChangeListener, AirPlayReceiverServerGetDispatchQueue( gAirPlayReceiverServer) );
	NetworkChangeListenerSetHandler( inCnx->networkChangeListener, _requestNetworkChangeListenerHandleEvent, inCnx );
	NetworkChangeListenerStart( inCnx->networkChangeListener );
#endif
	
	inCnx->didRecord = true;
	status = kHTTPStatus_OK;
	
exit:
	return( status );
}

//===========================================================================================================================
//	_requestProcessSetup
//===========================================================================================================================

static HTTPStatus _requestProcessSetup( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus				status;
	const char *			ptr;
	size_t					len;
	
	ptr = NULL;
	len = 0;
	GetHeaderValue( inRequest, kHTTPHeader_ContentType, &ptr, &len );
	if( MIMETypeIsPlist( ptr, len ) )
	{
		status = _requestProcessSetupPlist( inCnx, inRequest );
		goto exit;
	}
	
	{
		dlogassert( "Bad setup URL '%.*s'", (int) inRequest->header.urlLen, inRequest->header.urlPtr );
		status = kHTTPStatus_BadRequest;
		goto exit;
	}
	
exit:
	return( status );
}

//===========================================================================================================================
//	_requestProcessSetupPlist
//===========================================================================================================================

static HTTPStatus _requestProcessSetupPlist( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	HTTPStatus					status;
	OSStatus					err;
	CFMutableDictionaryRef		requestParams  = NULL;
	CFDictionaryRef				responseParams = NULL;
	uint8_t						sessionUUID[ 16 ];
	char						cstr[ 64 ];
	size_t						len;
	uint64_t					u64;
	AirPlayEncryptionType		et;
	
	aprs_ulog( kAirPlayPhaseLogLevel, "Setup\n" );
	
	// If we're denying interrupts then reject if there's already an active session.
	// Otherwise, hijack any active session to start the new one (last-in-wins).
	
	if( inCnx->server->denyInterruptions )
	{
		HTTPConnectionRef activeCnx;
		
		activeCnx = _FindActiveConnection( inCnx->server );
		if( activeCnx && ( activeCnx != inCnx->httpCnx ) )
		{
			aprs_ulog( kLogLevelNotice, "Denying interruption from %##a due to %##a\n", &inCnx->httpCnx->peerAddr, &activeCnx->peerAddr );
			status = kHTTPStatus_NotEnoughBandwidth;
			err = kNoErr;
			goto exit;
		}
	}
	_HijackConnections( inCnx->server, inCnx->httpCnx );
	
	requestParams = CFDictionaryCreateMutableWithBytes( inRequest->bodyPtr, inRequest->bodyOffset, &err );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );
	
	u64 = CFDictionaryGetMACAddress( requestParams, CFSTR( kAirPlayKey_DeviceID ), NULL, &err );
	if( !err ) inCnx->clientDeviceID = u64;
	
	snprintf( inCnx->ifName, sizeof( inCnx->ifName ), "%s", inCnx->httpCnx->ifName );

	CFDictionaryGetMACAddress( requestParams, CFSTR( kAirPlayKey_MACAddress ), inCnx->clientInterfaceMACAddress, &err );
	
	CFDictionaryGetCString( requestParams, CFSTR( kAirPlayKey_Name ), inCnx->clientName, sizeof( inCnx->clientName ), NULL );
	
	CFDictionaryGetData( requestParams, CFSTR( kAirPlayKey_SessionUUID ), sessionUUID, sizeof( sessionUUID ), &len, &err );
	if( !err )
	{
		require_action( len == sizeof( sessionUUID ), exit, err = kSizeErr; status = kHTTPStatus_BadRequest );
		inCnx->clientSessionID = ReadBig64( sessionUUID );
	}
	
	*cstr = '\0';
	CFDictionaryGetCString( requestParams, CFSTR( kAirPlayKey_SourceVersion ), cstr, sizeof( cstr ), NULL );
	if( *cstr != '\0' ) inCnx->clientVersion = TextToSourceVersion( cstr, kSizeCString );
	
	// Set up the session.
	
	if( !inCnx->session )
	{
		err = _requestCreateSession( inCnx, true );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
		strlcpy( gAirPlayAudioStats.ifname, inCnx->ifName, sizeof( gAirPlayAudioStats.ifname ) );
	}
	
	if( inCnx->pairVerifySessionHomeKit )
	{
		AirPlayReceiverSessionSetHomeKitSecurityContext( inCnx->session, inCnx->pairVerifySessionHomeKit );
	}
	else
	{
		et = (AirPlayEncryptionType) CFDictionaryGetInt64( requestParams, CFSTR( kAirPlayKey_EncryptionType ), &err );
		if( !err && ( et != kAirPlayEncryptionType_None ) )
		{
			uint8_t key[ 16 ], iv[ 16 ];
			
			err = _requestDecryptKey( inCnx, requestParams, et, key, iv );
			require_noerr_action( err, exit, status = kHTTPStatus_KeyManagementError );
			
			err = AirPlayReceiverSessionSetSecurityInfo( inCnx->session, key, iv );
			MemZeroSecure( key, sizeof( key ) );
			require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
		}
		CFDictionaryRemoveValue( requestParams, CFSTR( kAirPlayKey_EncryptionKey ) );
		CFDictionaryRemoveValue( requestParams, CFSTR( kAirPlayKey_EncryptionIV ) );
	}
	
	err = AirPlayReceiverSessionSetup( inCnx->session, requestParams, &responseParams );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );
	
	inCnx->didAnnounce		= true;
	inCnx->didAudioSetup	= true;
	inCnx->didScreenSetup	= true;
	AirPlayReceiverServerSetBoolean( gAirPlayReceiverServer, CFSTR( kAirPlayProperty_Playing ), NULL, true );
	
	status = _requestSendPlistResponse( inCnx->httpCnx, inRequest, responseParams, &err );
	require_noerr( err, exit );
	
exit:
	CFReleaseNullSafe( requestParams );
	CFReleaseNullSafe( responseParams );
	if( err ) aprs_ulog( kLogLevelNotice, "### Setup session failed: %#m\n", err );
	return( status );
}

//===========================================================================================================================
//	_requestProcessTearDown
//===========================================================================================================================

static HTTPStatus _requestProcessTearDown( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	CFDictionaryRef			requestDict;
	Boolean					done = true;
	
	requestDict = CFDictionaryCreateWithBytes( inRequest->bodyPtr, inRequest->bodyOffset, NULL );
	aprs_ulog( kLogLevelNotice, "Teardown %?@\n", log_category_enabled( aprs_ucat(), kLogLevelVerbose ), requestDict );
	
	if( inCnx->session )
	{
		AirPlayReceiverSessionTearDown( inCnx->session, requestDict, kNoErr, &done );
	}
	if( done )
	{
#if( TARGET_OS_POSIX )
		if( inCnx->networkChangeListener )
		{
			NetworkChangeListenerStop( inCnx->networkChangeListener );
			ForgetCF( &inCnx->networkChangeListener );
		}
#endif
		ForgetCF( &inCnx->session );
		AirPlayReceiverServerSetBoolean( gAirPlayReceiverServer, CFSTR( kAirPlayProperty_Playing ), NULL, false );
		gAirPlayAudioCompressionType = kAirPlayCompressionType_Undefined;
		inCnx->didAnnounce = false;
		inCnx->didAudioSetup = false;
		inCnx->didScreenSetup	= false;
		inCnx->didRecord = false;
	}
	CFReleaseNullSafe( requestDict );
	return( kHTTPStatus_OK );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_requestCreateSession
//===========================================================================================================================

static OSStatus _requestCreateSession( AirPlayReceiverConnectionRef inCnx, Boolean inUseEvents )
{
	OSStatus								err;
	AirPlayReceiverSessionCreateParams		createParams;
	
	require_action_quiet( !inCnx->session, exit, err = kNoErr );
	
	memset( &createParams, 0, sizeof( createParams ) );
	createParams.server				= gAirPlayReceiverServer;
	createParams.transportType		= inCnx->httpCnx->transportType;
	createParams.peerAddr			= &inCnx->httpCnx->peerAddr;
	createParams.clientDeviceID		= inCnx->clientDeviceID;
	createParams.clientSessionID	= inCnx->clientSessionID;
	createParams.clientVersion		= inCnx->clientVersion;
	createParams.useEvents			= inUseEvents;
	createParams.connection			= inCnx;
	
	memcpy( createParams.clientIfMACAddr, inCnx->clientInterfaceMACAddress, sizeof( createParams.clientIfMACAddr ) );
	snprintf( inCnx->ifName, sizeof( inCnx->ifName ), "%s", inCnx->httpCnx->ifName );
	
	err = AirPlayReceiverSessionCreate( &inCnx->session, &createParams );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_requestDecryptKey
//===========================================================================================================================

static OSStatus
	_requestDecryptKey(
		AirPlayReceiverConnectionRef	inCnx,
		CFDictionaryRef					inRequestParams,
		AirPlayEncryptionType			inType,
		uint8_t							inKeyBuf[ 16 ],
		uint8_t							inIVBuf[ 16 ] )
{
	OSStatus				err;
	const uint8_t *			keyPtr;
	size_t					len;
	
	keyPtr = CFDictionaryGetData( inRequestParams, CFSTR( kAirPlayKey_EncryptionKey ), NULL, 0, &len, &err );
	require_noerr( err, exit );
	
	if( 0 ) {}
	else if( inType == kAirPlayEncryptionType_MFi_SAPv1 )
	{
		require_action( inCnx->MFiSAP, exit, err = kAuthenticationErr );
		err = MFiSAP_Decrypt( inCnx->MFiSAP, keyPtr, len, inKeyBuf );
		require_noerr( err, exit );
	}
	else
	{
		(void) inCnx;
		aprs_ulog( kLogLevelWarning, "### Bad ET: %d\n", inType );
		err = kParamErr;
		goto exit;
	}
	
	CFDictionaryGetData( inRequestParams, CFSTR( kAirPlayKey_EncryptionIV ), inIVBuf, 16, &len, &err );
	require_noerr( err, exit );
	require_action( len == 16, exit, err = kSizeErr );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_requestReportIfIncompatibleSender
//===========================================================================================================================

static void _requestReportIfIncompatibleSender( AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest )
{
	const char *		userAgentPtr = NULL;
	size_t				userAgentLen = 0;
	const char *		ptr;
	uint32_t			sourceVersion = 0;
	
	(void) inCnx;  // unused
	
	GetHeaderValue( inRequest, kHTTPHeader_UserAgent, &userAgentPtr, &userAgentLen );
	ptr = ( userAgentLen > 0 ) ? ( (const char *) memchr( userAgentPtr, '/', userAgentLen ) ) : NULL;
	if( ptr )
	{
		++ptr;
		sourceVersion = TextToSourceVersion( ptr, (size_t)( ( userAgentPtr + userAgentLen ) - ptr ) );
	}
	if( sourceVersion < SourceVersionToInteger( 200, 30, 0 ) )
	{
		aprs_ulog( kLogLevelNotice, "### Reporting incompatible sender: '%.*s'\n", (int) userAgentLen, userAgentPtr );
	}
}

//===========================================================================================================================
//	_requestSendPlistResponse
//===========================================================================================================================

static HTTPStatus _requestSendPlistResponse( HTTPConnectionRef inCnx, HTTPMessageRef inRequest, CFPropertyListRef inPlist, OSStatus *outErr )
{
	HTTPStatus			status;
	OSStatus			err;
	const char *		httpProtocol;
	CFDataRef			data = NULL;
	const uint8_t *		ptr;
	size_t				len;
	HTTPMessageRef		response = inCnx->responseMsg;
	
	if( response->header.len == 0 )
	{
		httpProtocol = ( strnicmp_prefix( inRequest->header.protocolPtr, inRequest->header.protocolLen, "HTTP" ) == 0 )
			? "HTTP/1.1" : kAirTunesHTTPVersionStr;
		err = HTTPHeader_InitResponse( &response->header, httpProtocol, kHTTPStatus_OK, NULL );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
		response->bodyLen = 0;
	}
	
	if( inPlist )
	{
		data = CFPropertyListCreateData( NULL, inPlist, kCFPropertyListBinaryFormat_v1_0, 0, NULL );
		require_action( data, exit, err = kUnknownErr; status = kHTTPStatus_InternalServerError );
		ptr = CFDataGetBytePtr( data );
		len = (size_t) CFDataGetLength( data );
		err = HTTPMessageSetBody( response, kMIMEType_AppleBinaryPlist, ptr, len );
	}
	else
	{
		err = HTTPMessageSetBody( response, NULL, NULL, 0 );
	}
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	
	status = kHTTPStatus_OK;
	
exit:
	CFReleaseNullSafe( data );
	if( outErr ) *outErr = err;
	return( status );
}

#if( TARGET_OS_POSIX )
//===========================================================================================================================
//	_requestNetworkChangeListenerHandleEvent
//===========================================================================================================================

static void	_requestNetworkChangeListenerHandleEvent( uint32_t inEvent, void *inContext )
{
	OSStatus							err;
	AirPlayReceiverConnectionRef		cnx = (AirPlayReceiverConnectionRef) inContext;
	uint32_t							flags;
	Boolean								sessionDied = false;
	uint64_t							otherFlags	= 0;
	
	if( inEvent == kNetworkEvent_Changed )
	{
		err = SocketGetInterfaceInfo( cnx->httpCnx->sock, cnx->ifName, NULL, NULL, NULL, NULL, &flags, NULL, &otherFlags, NULL );
		if( err )
		{
			aprs_ulog( kLogLevelInfo, "### Can't get interface's %s info: err = %d; killing session.\n", cnx->ifName, err );
			sessionDied = true;
			goto exit;
		}
		if( !( flags & IFF_RUNNING ) )
		{
			aprs_ulog( kLogLevelInfo, "### Interface %s is not running; killing session.\n", cnx->ifName );
			sessionDied = true;
			goto exit;
		}
		if( otherFlags & kNetInterfaceFlag_Inactive )
		{
			aprs_ulog( kLogLevelInfo, "### Interface %s is inactive; killing session.\n", cnx->ifName );
			sessionDied = true;
			goto exit;
		}
	}
	
exit:
	if( sessionDied )
	{
#if( TARGET_OS_POSIX )
		if( cnx->networkChangeListener )
		{
			NetworkChangeListenerStop( cnx->networkChangeListener );
			ForgetCF( &cnx->networkChangeListener );
		}
#endif
		AirPlayReceiverServerControl( gAirPlayReceiverServer, kCFObjectFlagDirect, CFSTR( kAirPlayCommand_SessionDied ), cnx->session, NULL, NULL );
	}
}
#endif

