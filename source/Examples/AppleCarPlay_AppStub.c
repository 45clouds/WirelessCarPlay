/*
	File:    	AppleCarPlay_AppStub.c
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	n/a 
	
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
	
	Unless youn explicitly state otherwise, if you provide any ideas, suggestions, recommendations, bug 
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
	
	Copyright (C) 2007-2016 Apple Inc. All Rights Reserved.
*/

//===========================================================================================================================
//	This file contains sample code snippets that demonstrate how to use the plug-in APIs.  
//===========================================================================================================================

#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <CoreUtils/LogUtils.h>
#include <CoreUtils/StringUtils.h>
#include <AirPlayReceiverServer.h>
#include <AirPlayReceiverSession.h>
#include <AirPlayVersion.h>
#include <AirPlayUtils.h>
#include <CoreUtils/DebugServices.h>
#include <CoreUtils/HIDUtils.h>
#include "HIDKnob.h"
#include "HIDTouchScreen.h"
#include <CoreUtils/ScreenUtils.h>
#include <CoreUtils/MathUtils.h>
#include <CarPlayControlClient.h>
#include <CoreUtils/CFCompat.h>
#include <CoreUtils/CFUtils.h>

//===========================================================================================================================
//	Internals
//===========================================================================================================================

#define kDefaultUUID			CFSTR( "e5f7a68d-7b0f-4305-984b-974f677a150b" )
#define kScreenNoTimeout		( (uint64_t) INT64_C( -1 ) )

#define kUSBCountryCodeUnused               0
#define kUSBCountryCodeUS                   33
#define kUSBVendorTouchScreen               0
#define kUSBProductTouchScreen              0
#define kUSBVendorKnobButtons               0
#define kUSBProductKnobButtons              0

// Prototypes

static OSStatus	_SetupUI( void );
static void		_TearDownUI( void );
static void		_RunUI( void );

static void *	_AirPlayThread( void *inArg );

void CarPlayControlClientEventCallback( CarPlayControlClientRef client, CarPlayControlClientEvent event, void *eventInfo, void *context );
static OSStatus	_InitKnobDevice( void );

static CFTypeRef
	_AirPlayHandleServerCopyProperty( 
		AirPlayReceiverServerRef	inServer, 
		CFStringRef					inProperty, 
		CFTypeRef					inQualifier, 
		OSStatus *					outErr, 
		void *						inContext );
	
static void
	_AirPlayHandleSessionCreated( 
		AirPlayReceiverServerRef	inServer, 
		AirPlayReceiverSessionRef	inSession, 
		void *						inContext );
		
static void	
	_AirPlayHandleSessionFinalized( 
		AirPlayReceiverSessionRef inSession, 
		void *inContext );

static void
	_AirPlayHandleSessionStarted( 
		AirPlayReceiverSessionRef inSession, 
		void *inContext );

static void
	_AirPlayHandleModesChanged( 
		AirPlayReceiverSessionRef 	inSession, 
		const AirPlayModeState *	inState, 
		void *						inContext );

static void	
	_AirPlayHandleRequestUI( 
		AirPlayReceiverSessionRef inSession, 
		CFStringRef inURL,
		void *inContext );
	
static CFTypeRef
	_AirPlayHandleSessionCopyProperty( 
		AirPlayReceiverSessionRef	inSession, 
		CFStringRef					inProperty, 
		CFTypeRef					inQualifier, 
		OSStatus *					outErr, 
		void *						inContext );

static OSStatus 
	_AirPlayHandleSessionControl( 	
		AirPlayReceiverSessionRef 	inSession, 
 		CFStringRef         		inCommand,
        CFTypeRef           		inQualifier,
        CFDictionaryRef     		inParams,
        CFDictionaryRef *   		outParams,
		void *						inContext );

static OSStatus	_ParseModes( const char *inArg );
static OSStatus	_ParseModeResource( AirPlayResourceChange *inResource, const char *inArg );

static void	
	_sendGenericChangeModeRequest(
		const char					inStr[],
		AirPlayTransferType 		inScreenType,
		AirPlayTransferPriority		inScreenPriority,
		AirPlayConstraint			inScreenTake,
		AirPlayConstraint			inScreenBorrow,
		AirPlayTransferType 		inAudioType,
		AirPlayTransferPriority		inAudioPriority,
		AirPlayConstraint			inAudioTake,
		AirPlayConstraint			inAudioBorrow,
		AirPlayTriState				inPhone,
		AirPlaySpeechMode			inSpeech,
		AirPlayTriState				inTurnByTurn );
		
static void	
	_setChangeModesStruct(
		AirPlayModeChanges *		outModeChanges,
		AirPlayTransferType 		inScreenType,
		AirPlayTransferPriority		inScreenPriority,
		AirPlayConstraint			inScreenTake,
		AirPlayConstraint			inScreenBorrow,
		AirPlayTransferType 		inAudioType,
		AirPlayTransferPriority		inAudioPriority,
		AirPlayConstraint			inAudioTake,
		AirPlayConstraint			inAudioBorrow,
		AirPlayTriState				inPhone,
		AirPlaySpeechMode			inSpeech,
		AirPlayTriState				inTurnByTurn );		

ulog_define( CarPlayDemoApp, kLogLevelTrace, kLogFlags_Default, "CarPlayDemoApp", NULL );
#define app_ucat()					&log_category_from_name( CarPlayDemoApp )
#define app_ulog( LEVEL, ... )		ulog( app_ucat(), (LEVEL), __VA_ARGS__ )
#define app_dlog( LEVEL, ... )		dlogc( app_ucat(), (LEVEL), __VA_ARGS__ )

// Globals

static bool							gHIDConf		= false;
static bool							gKnob			= true;
static bool							gHiFiTouch		= true;
static bool							gLoFiTouch		= true;
static bool							gTouchpad		= true;
static bool							gScreenConf		= false;
static int							gVideoWidth		= 960;
static int							gVideoHeight	= 540;
static int							gPrimaryInputDevice			= kScreenPrimaryInputDevice_Undeclared;
static bool							gEnhancedRequestCarUI		= false;
static bool							gETCEnabled					= false;
static ScreenRef					gMainScreen 	= NULL;
static HIDDeviceRef					gTouchHID		= NULL;
static AirPlayReceiverServerRef		gAirPlayServer	= NULL;
static AirPlayReceiverSessionRef	gAirPlaySession	= NULL;
static CarPlayControlClientRef		gCarPlayControlClient = NULL;
static CFMutableArrayRef			gCarPlayControllers = NULL;
static pthread_t					gAirPlayThread;
static AirPlayModeChanges			gInitialModesRaw;
static CFDictionaryRef				gInitialModes		= NULL;
static bool							gHasInitialModes	= false;
static CFMutableArrayRef			gBluetoothIDs				= NULL;
static CFMutableArrayRef			gRightHandDrive				= NULL;
static CFMutableArrayRef			gNightMode				= NULL;

// Virtual knob device
static HIDDeviceRef		gKnobHID;
// Current state of thMODULE_AIRPLAY_SUPPORT_NAMEe virtual knob
Boolean gSelectButtonPressed = false;
Boolean gHomeButtonPressed = false;
Boolean gBackButtonPressed = false;
Boolean gSiriButtonPressed = false;
double	gXPosition = 0;
double	gYPosition = 0;
int8_t	gWheelPositionRelative = 0;
// Maximum and minimum X values reported by actual knob
#define kMaxXPosition	(1.0)
#define kMinXPosition	(-1.0)
// Maximum and minimum X values reported by actual knob
#define kMaxYPosition	(1.0)
#define kMinYPosition	(-1.0)
OSStatus	KnobUpdate( void );

void    setupTouchScreen();


//===========================================================================================================================
//	main
//===========================================================================================================================

int	main( int argc, char **argv )
{
	int					i;
	const char *		arg;
	OSStatus			err;
	uint32_t			u32;
	
	app_ulog( kLogLevelNotice, "AirPlay starting version %s\n", kAirPlaySourceVersionStr );
	signal( SIGPIPE, SIG_IGN ); // Ignore SIGPIPE signals so we get EPIPE errors from APIs instead of a signal.
	
	AirPlayModeChangesInit( &gInitialModesRaw );

	// Parse command line arguments.
	
	for( i = 1; i < argc; )
	{
		arg = argv[ i++ ];
		if( 0 ) {}
		else if( strcmp( arg, "--hid-conf" ) == 0 )
		{
			gHIDConf = true; // Use HID devices from the airplay.conf file instead of creating programmatically.
		}
		else if( strcmp( arg, "--no-knob" ) == 0 )
		{
			gKnob = false;
		}
		else if( strcmp( arg, "--no-hi-fi-touch" ) == 0 )
		{
			gHiFiTouch = false;
		}
		else if( strcmp( arg, "--no-lo-fi-touch" ) == 0 )
		{
			gLoFiTouch = false;
		}
		else if( strcmp( arg, "--no-touchpad" ) == 0 )
		{
			gTouchpad = false;
		}
		else if( strcmp( arg, "--width" ) == 0 )
		{
			if( i >= argc ) { fprintf( stderr, "error: %s requires a value\n", arg ); exit( 1 ); }
			arg = argv[ i++ ];
			gVideoWidth = atoi( arg );
		}
		else if( strcmp( arg, "--height" ) == 0 )
		{
			if( i >= argc ) { fprintf( stderr, "error: %s requires a value\n", arg ); exit( 1 ); }
			arg = argv[ i++ ];
			gVideoHeight = atoi( arg );
		}
		else if( strcmp( arg, "--screen-conf" ) == 0 )
		{
			gScreenConf = true; // Use screen devices from the airplay.conf file instead of creating programmatically.
		}
		else if( strcmp( arg, "--btid" ) == 0 )
		{
			if( i >= argc ) { fprintf( stderr, "error: %s requires a value\n", arg ); exit( 1 ); }
			err = CFArrayEnsureCreatedAndAppendCString( &gBluetoothIDs, argv[ i++ ], kSizeCString );
			check_noerr( err );
		}
		else if( strcmp( arg, "--modes" ) == 0 )
		{
			if( i >= argc )
			{
				fprintf( stderr, "error: %s requires a value. For example:\n", arg );
				fprintf( stderr, "    --modes screen=userInitiated,anytime\n" );
				fprintf( stderr, "    --modes mainAudio=anytime,anytime\n" );
				fprintf( stderr, "    --modes phoneCall\n" );
				fprintf( stderr, "    --modes speech=speaking\n" );
				fprintf( stderr, "    --modes turnByTurn\n" );
				exit( 1 );
			}
			arg = argv[ i++ ];
			err = _ParseModes( arg );
			if( err ) exit( 1 );
			gHasInitialModes = true;
		}
		else if( strcmp( arg, "--btid" ) == 0 )
		{
			if( i >= argc ) { fprintf( stderr, "error: %s requires a value\n", arg ); exit( 1 ); }
			err = CFArrayEnsureCreatedAndAppendCString( &gBluetoothIDs, argv[ i++ ], kSizeCString );
			check_noerr( err );
		}
		else if( strcmp( arg, "--right-hand-drive" ) == 0 )
		{
			if( i >= argc ) { fprintf( stderr, "error: %s requires a value\n", arg ); exit( 1 ); }
			arg = argv[ i++ ];
			if( !strcmp ( arg, "true" ) ) 			err = CFArrayEnsureCreatedAndAppendCString( &gRightHandDrive, "1", kSizeCString );
			else if( !strcmp ( arg, "false" ) ) 	err = CFArrayEnsureCreatedAndAppendCString( &gRightHandDrive, "0", kSizeCString );
			check_noerr( err );
		}
		else if( strcmp( arg, "--enable-etc" ) == 0 )
		{
			gETCEnabled = true;
		}
		else if( strcmp( arg, "--primary-input-device" ) == 0 )
		{
			if( i >= argc ) { fprintf( stderr, "error: %s requires a value\n", arg ); exit( 1 ); }
			arg = argv[ i++ ];
			gPrimaryInputDevice = atoi( arg );
		}
		else if( strcmp( arg, "--enhanced-requestcarui" ) == 0 )
		{
			gEnhancedRequestCarUI = true;
		}
	}
	
	if( gHasInitialModes )
	{
		gInitialModes = AirPlayCreateModesDictionary( &gInitialModesRaw, NULL, &err );
		check_noerr( err );
	}
	
	// Set up interaction with the platform UI framework.
	
	err = _SetupUI();
	require_noerr( err, exit );
	
	// Set up a screen object to represent the portion of the screen where AirPlay will render into.
	
	if( !gScreenConf )
	{
		err = ScreenCreate( &gMainScreen, NULL );
		require_noerr( err, exit );
		
		u32 = 0;
		if( gKnob )			u32 |= kScreenFeature_Knobs;
		if( gHiFiTouch )	u32 |= kScreenFeature_HighFidelityTouch;
		if( gLoFiTouch )	u32 |= kScreenFeature_LowFidelityTouch;
		if( gTouchpad )		u32 |= kScreenFeature_Touchpad;
		ScreenSetPropertyInt64( gMainScreen, kScreenProperty_Features, NULL, u32 );
		ScreenSetPropertyInt64( gMainScreen, kScreenProperty_WidthPixels, NULL, gVideoWidth );
		ScreenSetPropertyInt64( gMainScreen, kScreenProperty_HeightPixels, NULL, gVideoHeight );
		if( gPrimaryInputDevice != kScreenPrimaryInputDevice_Undeclared ) {
			ScreenSetPropertyInt64( gMainScreen, kScreenProperty_PrimaryInputDevice, NULL, gPrimaryInputDevice );
		}
		ScreenSetProperty( gMainScreen, kScreenProperty_UUID, NULL, kDefaultUUID );
		
		err = ScreenRegister( gMainScreen );
		require_noerr( err, exit );
	}
	
	// Set up a virtual HID device to post HID reports for the touch screen.
	// Note: if you're specifying your HID devices in the airplay.conf file then the code below is excluded.
	// This code shows how to programmatically create HID devices. If you are creating HID objects programmatically, 
	// like below, then use HIDDevicePostReport with the created HIDDeviceRef to post HID reports. Otherwise, you'll
	// need to use HIDPostReport API with HID UUID that matches the one(s) you put into the airplay.conf file.
	// See HIDTouchScreen.h on registering a sample HID descriptor for a touchscreen.
	
	if( ( gHiFiTouch || gLoFiTouch ) && !gHIDConf )
	{
		setupTouchScreen();
		
		err = HIDRegisterDevice( gTouchHID );
		require_noerr( err, exit );
	}
	
	// Register a virtual HID device for a knob controller.
	// See HIDKnob.h on registering a sample HID descriptor for a knob
	err = _InitKnobDevice();
	require_noerr( err, exit );
	
	// Start AirPlay in a separate thread since the demo app needs to own the main thread.
	
	err = pthread_create( &gAirPlayThread, NULL, _AirPlayThread, NULL );
	require_noerr( err, exit );
	
	// Run the main loop for the app to receive UI events. This doesn't return until the app quits.
	
	_RunUI();
	
exit:
	_TearDownUI();
	return( err ? 1 : 0 );
}

//===========================================================================================================================
//	_SetupUI
//===========================================================================================================================

static OSStatus	_SetupUI( void )
{
	OSStatus			err = kNoErr;

	// $$$ TODO: Set up the UI framework application and window for drawing and receiving user events.
	// See HIDUtils.h on registering HID user input (touch events, knobs, etc.).

	require_noerr( err, exit );

exit:
	if( err ) _TearDownUI();
	return( err );
}

//===========================================================================================================================
//	_TearDownUI
//===========================================================================================================================

static void	_TearDownUI( void )
{
	// $$$ TODO: Destroy up the UI framework application and window for drawing and receiving user events.
}

//===========================================================================================================================
//	_RunUI
//===========================================================================================================================

static void	_RunUI( void )
{

	// $$$ TODO: Get user input and send HID reports to AirPlay. 
	// For an example of creating a virtual HID knob report see KnobUpdate (void).
	// See HIDUtils.h & Support/HID*for details.
	
}

//===========================================================================================================================
//	_AirPlayThread
//===========================================================================================================================

static void *	_AirPlayThread( void *inArg )
{
	OSStatus							err;
	AirPlayReceiverServerDelegate		delegate;
	
	(void) inArg;

	// Create the AirPlay server. This advertise via Bonjour and starts listening for connections.
	
	err = AirPlayReceiverServerCreate( &gAirPlayServer );
	require_noerr( err, exit );
	
	// Register ourself as a delegate to receive server-level events, such as when a session is created.
	
	AirPlayReceiverServerDelegateInit( &delegate );
	delegate.copyProperty_f		= _AirPlayHandleServerCopyProperty;
	delegate.sessionCreated_f = _AirPlayHandleSessionCreated;
	AirPlayReceiverServerSetDelegate( gAirPlayServer, &delegate );
	
	err = CarPlayControlClientCreateWithServer( &gCarPlayControlClient, gAirPlayServer, CarPlayControlClientEventCallback, NULL );
	require_noerr( err, exit );

	gCarPlayControllers = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( gCarPlayControllers, exit, err = kNoMemoryErr );
	
	// Start the server and run until the app quits.
	
	AirPlayReceiverServerStart( gAirPlayServer );
	CarPlayControlClientStart( gCarPlayControlClient );
	CFRunLoopRun();
	CarPlayControlClientStop( gCarPlayControlClient );
	AirPlayReceiverServerStop( gAirPlayServer );
	
exit:
	CFReleaseNullSafe( gCarPlayControllers );
	CFReleaseNullSafe( gCarPlayControlClient );
	CFReleaseNullSafe( gAirPlayServer );
	return( NULL );
}

//===========================================================================================================================
//	_AirPlayHandleServerCopyProperty
//===========================================================================================================================

static CFTypeRef
	_AirPlayHandleServerCopyProperty( 
		AirPlayReceiverServerRef	inServer, 
		CFStringRef					inProperty, 
		CFTypeRef					inQualifier, 
		OSStatus *					outErr, 
		void *						inContext )
{
	CFTypeRef		value = NULL;
	OSStatus		err;
	
	(void) inServer;
	(void) inQualifier;
	(void) inContext;
	
	// BluetoothIDs
	
	if( CFEqual( inProperty, CFSTR( kAirPlayProperty_BluetoothIDs ) ) )
	{
		value = gBluetoothIDs;
		require_action_quiet( value, exit, err = kNotHandledErr );
		CFRetain( value );
	}
	else if( CFEqual( inProperty, CFSTR( kAirPlayKey_RightHandDrive ) ) )
    {
        value = gRightHandDrive;
        require_action_quiet( value, exit, err = kNotHandledErr );
        CFRetain( value );
    }
    else if( CFEqual( inProperty, CFSTR( kAirPlayKey_NightMode ) ) )
    {
        value = gNightMode;
        require_action_quiet( value, exit, err = kNotHandledErr );
        CFRetain( value );
    }
	else if( CFEqual( inProperty, CFSTR( kAirPlayKey_VehicleInformation ) ) )
	{
		CFMutableArrayRef vehicleDict;

		vehicleDict = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		require_action_quiet( vehicleDict, exit, err = kNotHandledErr );

		if( gETCEnabled ) {
			CFArrayAppendValue( vehicleDict, CFSTR( kAirPlayVehicleInformation_ETC ) );
		}
		if( CFArrayGetCount( vehicleDict ) == 0 ) {
			ForgetCF( &vehicleDict );
		} else {
			value = vehicleDict;
		}
	}
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_ExtendedFeatures ) ) )
	{
		CFMutableArrayRef extendedFeatures;

		extendedFeatures = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		require_action_quiet( extendedFeatures, exit, err = kNotHandledErr );

		CFArrayAppendValue( extendedFeatures, CFSTR( kAirPlayExtendedFeature_VocoderInfo ) );

		if( gEnhancedRequestCarUI ) {
			CFArrayAppendValue( extendedFeatures, CFSTR( kAirPlayExtendedFeature_EnhancedRequestCarUI ) );
		}
		if( CFArrayGetCount( extendedFeatures ) == 0 ) {
			ForgetCF( &extendedFeatures );
		} else {
			value = extendedFeatures;
		}
	}
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_OEMIcons ) ) )
	{
		CFTypeRef obj;
		CFMutableDictionaryRef iconDict;
		CFMutableArrayRef iconsArray;
		
		iconsArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		require_action_quiet( iconsArray, exit, err = kNotHandledErr );
		
		// Add icons for each required size
		obj = CFDataCreateWithFilePath( "/AirPlay/icon_120x120.png", NULL );
		if( obj ) {
			iconDict = CFDictionaryCreateMutable( NULL, 0,  &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			if( iconDict ) {
				CFDictionarySetInt64( iconDict, CFSTR( kAirPlayOEMIconKey_WidthPixels ), 120 );
				CFDictionarySetInt64( iconDict, CFSTR( kAirPlayOEMIconKey_HeightPixels ), 120 );
				CFDictionarySetBoolean( iconDict, CFSTR( kAirPlayOEMIconKey_Prerendered ), true );
				CFDictionarySetValue( iconDict, CFSTR( kAirPlayOEMIconKey_ImageData ), obj );
				CFArrayAppendValue( iconsArray, iconDict );
				CFRelease( iconDict );
				iconDict = NULL;
			}
			CFRelease( obj );
		}
		if( CFArrayGetCount( iconsArray ) > 0) {
			value = iconsArray;
		} else {
			CFRelease( iconsArray );
			err = kNotHandledErr;
			goto exit;
		}
	}
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}


//===========================================================================================================================
//	_AirPlayHandleSessionCreated
//===========================================================================================================================

static void
	_AirPlayHandleSessionCreated( 
		AirPlayReceiverServerRef	inServer, 
		AirPlayReceiverSessionRef	inSession, 
		void *						inContext )
{
	AirPlayReceiverSessionDelegate		delegate;
	
	(void) inServer;
	(void) inContext;
	
	app_ulog( kLogLevelNotice, "AirPlay session started\n" );
	gAirPlaySession = inSession;
	
	// Register ourself as a delegate to receive session-level events, such as modes changes.
	
	AirPlayReceiverSessionDelegateInit( &delegate );
	delegate.finalize_f		= _AirPlayHandleSessionFinalized;
	delegate.started_f		= _AirPlayHandleSessionStarted;
	delegate.copyProperty_f	= _AirPlayHandleSessionCopyProperty;
	delegate.modesChanged_f	= _AirPlayHandleModesChanged;
	delegate.requestUI_f	= _AirPlayHandleRequestUI;
	delegate.control_f		= _AirPlayHandleSessionControl;
	AirPlayReceiverSessionSetDelegate( inSession, &delegate );
}

//===========================================================================================================================
//	_AirPlayHandleSessionFinalized
//===========================================================================================================================

static void	_AirPlayHandleSessionFinalized( AirPlayReceiverSessionRef inSession, void *inContext )
{
	(void) inSession;
	(void) inContext;
	
	gAirPlaySession = NULL;
	app_ulog( kLogLevelNotice, "AirPlay session ended\n" );
}

//===========================================================================================================================
//	_AirPlayHandleSessionStarted
//===========================================================================================================================

static void _AirPlayHandleSessionStarted( AirPlayReceiverSessionRef inSession, void *inContext )
{
	OSStatus error;
	CFNumberRef value;

	(void) inContext;

	value = (CFNumberRef) AirPlayReceiverSessionCopyProperty( inSession, 0, CFSTR( kAirPlayProperty_TransportType ), NULL, &error );
	if( error == kNoErr && value ) {
		uint32_t transportType;

		CFNumberGetValue( (CFNumberRef) value, kCFNumberSInt32Type, &transportType ); 
		if( NetTransportTypeIsWiFi( transportType ) ) {
			// Start iAP
		}
	}
}

//===========================================================================================================================
//	_AirPlayHandleSessionCopyProperty
//===========================================================================================================================

static CFTypeRef
	_AirPlayHandleSessionCopyProperty( 
		AirPlayReceiverSessionRef	inSession, 
		CFStringRef					inProperty, 
		CFTypeRef					inQualifier, 
		OSStatus *					outErr, 
		void *						inContext )
{
	CFTypeRef		value = NULL;
	OSStatus		err;
	
	(void) inSession;
	(void) inQualifier;
	(void) inContext;
	
	// Modes
	
	if( CFEqual( inProperty, CFSTR( kAirPlayProperty_Modes ) ) )
	{
		value = gInitialModes;
		require_action_quiet( value, exit, err = kNotHandledErr );
		CFRetain( value );
	}
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	_AirPlayHandleModesChanged
//===========================================================================================================================

static void
	_AirPlayHandleModesChanged( 
		AirPlayReceiverSessionRef 	inSession, 
		const AirPlayModeState *	inState, 
		void *						inContext )
{
	(void) inSession;
	(void) inContext;
	
	app_ulog( kLogLevelNotice, "Modes changed: screen %s, mainAudio %s, speech %s (%s), phone %s, turns %s\n", 
		AirPlayEntityToString( inState->screen ), AirPlayEntityToString( inState->mainAudio ), 
		AirPlayEntityToString( inState->speech.entity ), AirPlaySpeechModeToString( inState->speech.mode ), 
		AirPlayEntityToString( inState->phoneCall ), AirPlayEntityToString( inState->turnByTurn ) );
}

//===========================================================================================================================
//	_AirPlayHandleRequestUI
//===========================================================================================================================

static void	_AirPlayHandleRequestUI( AirPlayReceiverSessionRef inSession, CFStringRef inURL, void *inContext )
{
	const char *str;
	size_t len;

	(void) inSession;
	(void) inContext;

	if( inURL ) {
		CFLStringGetCStringPtr( inURL, &str, &len);
	} else {
		str = NULL;
	}

	app_ulog( kLogLevelNotice, "Request accessory UI: \"%s\"\n", str ? str : "null" );
}

//===========================================================================================================================
//	_AirPlayHandleSessionControl
//===========================================================================================================================

static OSStatus 
	_AirPlayHandleSessionControl( 	
		AirPlayReceiverSessionRef 	inSession, 
 		CFStringRef         		inCommand,
        CFTypeRef           		inQualifier,
        CFDictionaryRef     		inParams,
        CFDictionaryRef *   		outParams,
		void *						inContext )
{
	OSStatus err;

	(void) inSession;
    (void) inQualifier;
    (void) inParams;
    (void) outParams;
	(void) inContext;

	if( CFEqual( inCommand, CFSTR( kAirPlayCommand_DisableBluetooth ) ) )
    {
		app_ulog( kLogLevelNotice, "Disable Bluetooth session control request\n" );
    	err = kNoErr;
    }
    else
    {
		app_ulog( kLogLevelNotice, "Unsupported session control request\n" );
        err = kNotHandledErr;
    }
	
	return( err );
}

//===========================================================================================================================
//	_InitKnobDevice
//===========================================================================================================================

static OSStatus	_InitKnobDevice( void )
{
	OSStatus			err;
	CFNumberRef         countryCode;
	CFNumberRef         productID;
	CFNumberRef         vendorID;
	uint8_t *			descPtr;
	size_t				descLen;
	CFDataRef			descData;
	
	// Create knob device.
	err = HIDDeviceCreateVirtual( &gKnobHID, NULL );
	require_noerr( err, exit );
	HIDDeviceSetProperty( gKnobHID, kHIDDeviceProperty_DisplayUUID, NULL, kDefaultUUID );
	HIDDeviceSetProperty( gKnobHID, kHIDDeviceProperty_Name, NULL, CFSTR( "My Knob" ) );
	
	countryCode = CFNumberCreateInt64( kUSBCountryCodeUS );
	if ( countryCode ) 
	{
		HIDDeviceSetProperty( gKnobHID, kHIDDeviceProperty_CountryCode, NULL, countryCode );
        CFRelease( countryCode );
	}
    productID = CFNumberCreateInt64( kUSBProductKnobButtons );
    if ( productID ) 
    {
    	HIDDeviceSetProperty( gKnobHID, kHIDDeviceProperty_ProductID, NULL, productID );
    	CFRelease( productID );
	}
	
	vendorID = CFNumberCreateInt64( kUSBVendorKnobButtons );
	if ( vendorID ) 
	{
		HIDDeviceSetProperty( gKnobHID, kHIDDeviceProperty_VendorID, NULL, vendorID );
		CFRelease( vendorID );
	}
	
	err = HIDKnobCreateDescriptor( &descPtr, &descLen );
	require_noerr( err, exit );
	descData = CFDataCreate( NULL, descPtr, (CFIndex) descLen );
	free( descPtr );
	require_action( descData, exit, err = kNoMemoryErr );
	HIDDeviceSetProperty( gKnobHID, kHIDDeviceProperty_ReportDescriptor, NULL, descData );
	CFRelease( descData );
	
	err = HIDRegisterDevice( gKnobHID );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	setupTouchScreen
//===========================================================================================================================

void	setupTouchScreen()
{
    OSStatus		err;
    CFNumberRef		countryCode;
    CFNumberRef		productID;
    CFNumberRef		vendorID;
	uint8_t *		descPtr;
	size_t			descLen;
	CFDataRef		descData;

	err = HIDDeviceCreateVirtual( &gTouchHID, NULL );
	require_noerr( err, exit );
	HIDDeviceSetProperty( gTouchHID, kHIDDeviceProperty_DisplayUUID, NULL, kDefaultUUID );
	HIDDeviceSetProperty( gTouchHID, kHIDDeviceProperty_Name, NULL, CFSTR( "Touch Screen" ) );
	countryCode = CFNumberCreateInt64( kUSBCountryCodeUnused );
	if ( countryCode ) 
	{
		HIDDeviceSetProperty( gTouchHID, kHIDDeviceProperty_CountryCode, NULL, countryCode );
		CFRelease( countryCode );
	}
	productID = CFNumberCreateInt64( kUSBProductTouchScreen );
	if ( productID ) 
	{
		HIDDeviceSetProperty( gTouchHID, kHIDDeviceProperty_ProductID, NULL, productID );
		CFRelease( productID );
	}
	vendorID = CFNumberCreateInt64( kUSBVendorTouchScreen );
	if ( vendorID ) 
	{
		HIDDeviceSetProperty( gTouchHID, kHIDDeviceProperty_VendorID, NULL, vendorID );
		CFRelease( vendorID );
	}
	
	err = HIDTouchScreenSingleCreateDescriptor( &descPtr, &descLen, gVideoWidth, gVideoHeight );
	require_noerr( err, exit );
	descData = CFDataCreate( NULL, descPtr, (CFIndex) descLen );
	free( descPtr );
	require_action( descData, exit, err = kNoMemoryErr );
	HIDDeviceSetProperty( gTouchHID, kHIDDeviceProperty_ReportDescriptor, NULL, descData );
	CFRelease( descData );
exit:;
}


//===========================================================================================================================
//	KnobUpdate
//===========================================================================================================================

OSStatus	KnobUpdate( void )
{
	OSStatus		err;
	uint8_t			report[ 4 ];
	int8_t			x;
	int8_t			y;
	
	// Normalize X and Y values to integers between -127 and 127
	x = (int8_t) TranslateValue( gXPosition, kMinXPosition, kMaxXPosition, -127, 127 );
	y = (int8_t) TranslateValue( gYPosition, kMinYPosition, kMinYPosition, -127, 127 );
	
	// A HIDKnobFillReport must be sent on both button pressed and button released events
	HIDKnobFillReport( report, gSelectButtonPressed, gHomeButtonPressed, gBackButtonPressed, x, y, gWheelPositionRelative );
	
	err = HIDDevicePostReport( gKnobHID, report, sizeof( report ) );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_ParseModes
//===========================================================================================================================

static OSStatus	_ParseModes( const char *inArg )
{
	OSStatus			err;
	const char *		token;
	size_t				len;
	
	token = inArg;
	while( ( *inArg != '\0' ) && ( *inArg != '=' ) ) ++inArg;
	len = (size_t)( inArg - token );
	if( *inArg != '\0' ) ++inArg;
	if( ( strnicmpx( token, len, kAirPlayResourceIDString_MainScreen ) == 0 ) ||
		( strnicmpx( token, len, "screen" ) == 0 ) )
	{
		err = _ParseModeResource( &gInitialModesRaw.screen, inArg );
		require_noerr_quiet( err, exit );
	}
	else if( strnicmpx( token, len, kAirPlayResourceIDString_MainAudio ) == 0 )
	{
		err = _ParseModeResource( &gInitialModesRaw.mainAudio, inArg );
		require_noerr_quiet( err, exit );
	}
	else if( strnicmpx( token, len, kAirPlayAppStateIDString_PhoneCall ) == 0 )
	{
		gInitialModesRaw.phoneCall = kAirPlayTriState_True;
	}
	else if( strnicmpx( token, len, kAirPlayAppStateIDString_Speech ) == 0 )
	{
		if( stricmp( inArg, kAirPlaySpeechModeString_None ) == 0 )
		{
			gInitialModesRaw.speech = kAirPlaySpeechMode_None;
		}
		else if( stricmp( inArg, kAirPlaySpeechModeString_Speaking ) == 0 )
		{
			gInitialModesRaw.speech = kAirPlaySpeechMode_Speaking;
		}
		else if( stricmp( inArg, kAirPlaySpeechModeString_Recognizing ) == 0 )
		{
			gInitialModesRaw.speech = kAirPlaySpeechMode_Recognizing;
		}
		else
		{
			err = kParamErr;
			goto exit;
		}
	}
	else if( strnicmpx( token, len, kAirPlayAppStateIDString_TurnByTurn ) == 0 )
	{
		gInitialModesRaw.turnByTurn = kAirPlayTriState_True;
	}
	else
	{
		err = kParamErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( err ) fprintf( stderr, "error: bad mode '%s'\n", inArg );
	return( err );
}

//===========================================================================================================================
//	_ParseModeResource
//===========================================================================================================================

static OSStatus	_ParseModeResource( AirPlayResourceChange *inResource, const char *inArg )
{
	OSStatus			err;
	const char *		token;
	size_t				len;
	
	inResource->type		= kAirPlayTransferType_Take;
	inResource->priority	= kAirPlayTransferPriority_UserInitiated;
	
	// TakeConstraint
	
	token = inArg;
	while( ( *inArg != '\0' ) && ( *inArg != ',' ) ) ++inArg;
	len = (size_t)( inArg - token );
	if( *inArg != '\0' ) ++inArg;
	if(      strnicmpx( token, len, kAirPlayConstraintString_Anytime ) == 0 )
	{
		inResource->takeConstraint = kAirPlayConstraint_Anytime;
	}
	else if( strnicmpx( token, len, kAirPlayConstraintString_UserInitiated ) == 0 )
	{
		inResource->takeConstraint = kAirPlayConstraint_UserInitiated;
	}
	else if( strnicmpx( token, len, kAirPlayConstraintString_Never ) == 0 )
	{
		inResource->takeConstraint = kAirPlayConstraint_Never;
	}
	else
	{
		err = kParamErr;
		goto exit;
	}
	
	// BorrowConstraint
	
	token = inArg;
	while( ( *inArg != '\0' ) && ( *inArg != ',' ) ) ++inArg;
	len = (size_t)( inArg - token );
	if( *inArg != '\0' ) ++inArg;
	if(      strnicmpx( token, len, kAirPlayConstraintString_Anytime ) == 0 )
	{
		inResource->borrowOrUnborrowConstraint = kAirPlayConstraint_Anytime;
	}
	else if( strnicmpx( token, len, kAirPlayConstraintString_UserInitiated ) == 0 )
	{
		inResource->borrowOrUnborrowConstraint = kAirPlayConstraint_UserInitiated;
	}
	else if( strnicmpx( token, len, kAirPlayConstraintString_Never ) == 0 )
	{
		inResource->borrowOrUnborrowConstraint = kAirPlayConstraint_Never;
	}
	else
	{
		err = kParamErr;
		goto exit;
	}
	
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_sendGenericChangeModeRequest
//===========================================================================================================================

void	_sendGenericChangeModeRequest(
	const char					inStr[],
	AirPlayTransferType 		inScreenType,
	AirPlayTransferPriority		inScreenPriority,
	AirPlayConstraint			inScreenTake,
	AirPlayConstraint			inScreenBorrow,
	AirPlayTransferType 		inAudioType,
	AirPlayTransferPriority		inAudioPriority,
	AirPlayConstraint			inAudioTake,
	AirPlayConstraint			inAudioBorrow,
	AirPlayTriState				inPhone,
	AirPlaySpeechMode			inSpeech,
	AirPlayTriState				inTurnByTurn )
{
	OSStatus				err;
	AirPlayModeChanges		changes;
	
	if( !inStr ) return;
	
	//when the Airplay session is active
	if( gAirPlaySession)
	{
		//changeModes (car -> iOS)
		_setChangeModesStruct( &changes, inScreenType, inScreenPriority, inScreenTake, inScreenBorrow,
			inAudioType, inAudioPriority, inAudioTake, inAudioBorrow, inPhone, inSpeech, inTurnByTurn );
		AirPlayReceiverSessionChangeModes( gAirPlaySession, &changes, NULL, NULL, NULL );	
	}
	//before the session is active, set initial modes.
	else
	{
		//Initial Mode (car -> iOS)
		_setChangeModesStruct( &gInitialModesRaw, inScreenType, inScreenPriority, inScreenTake, inScreenBorrow,
			inAudioType, inAudioPriority, inAudioTake, inAudioBorrow, inPhone, inSpeech, inTurnByTurn );
		gInitialModes = AirPlayCreateModesDictionary( &gInitialModesRaw, NULL, &err );
	}
}

//===========================================================================================================================
//	_setChangeModesStruct
//===========================================================================================================================

static void	_setChangeModesStruct(
	AirPlayModeChanges	* 		outModeChanges,
	AirPlayTransferType 		inScreenType,
	AirPlayTransferPriority		inScreenPriority,
	AirPlayConstraint			inScreenTake,
	AirPlayConstraint			inScreenBorrow,
	AirPlayTransferType 		inAudioType,
	AirPlayTransferPriority		inAudioPriority,
	AirPlayConstraint			inAudioTake,
	AirPlayConstraint			inAudioBorrow,
	AirPlayTriState				inPhone,
	AirPlaySpeechMode			inSpeech,
	AirPlayTriState				inTurnByTurn )
{
	if( !outModeChanges ) return;

	outModeChanges->screen.type								= inScreenType;
	outModeChanges->screen.priority							= inScreenPriority;
	outModeChanges->screen.takeConstraint 					= inScreenTake;
	outModeChanges->screen.borrowOrUnborrowConstraint 		= inScreenBorrow;
	outModeChanges->mainAudio.type							= inAudioType;
	outModeChanges->mainAudio.priority						= inAudioPriority;
	outModeChanges->mainAudio.takeConstraint 				= inAudioTake;
	outModeChanges->mainAudio.borrowOrUnborrowConstraint 	= inAudioBorrow;
	outModeChanges->phoneCall 								= inPhone;
	outModeChanges->speech 									= inSpeech;
	outModeChanges->turnByTurn 								= inTurnByTurn;
}




//===========================================================================================================================
//	CarPlayControlClient
//===========================================================================================================================
void CarPlayControlClientEventCallback( CarPlayControlClientRef client, CarPlayControlClientEvent event, void *eventInfo, void *context )
{
    (void) client;
	(void) context;
	CarPlayControllerRef controller = (CarPlayControllerRef)eventInfo;
	OSStatus err;

	app_ulog( kLogLevelNotice, "CarPlayControlClientEvent event received\n" );

	if (event == kCarPlayControlClientEvent_AddOrUpdateController) {
		const char *cStr = NULL;
		char *storage = NULL;
		CFStringRef name = NULL;
		CFIndex count;

		app_ulog( kLogLevelNotice, "CarPlayControlClientEvent Add/Update event received\n" );

		CarPlayControllerCopyName( controller, &name);
		CFStringGetOrCopyCStringUTF8(name, &cStr, &storage, NULL);
		app_ulog( kLogLevelNotice, "Adding CarPlayController '%s'\n", cStr );

		// Add the new client to the array 
		CFArrayAppendValue( gCarPlayControllers, controller );
		count = CFArrayGetCount( gCarPlayControllers );

		if( count == 1 ) {
			// Try to connect if this is the only client
			int i;

			for( i = 0; i < 5; i++ ) {
				err = CarPlayControlClientConnect(gCarPlayControlClient, controller);
				app_ulog( kLogLevelNotice, "CarPlayControlClientConnect %s: %#m\n", err ? "failed" : "succeeded", err );

				if( err != kNoErr ) {
					sleep( 1 );
					continue;
				} else {
					break;
				}
			}
		}
		free( storage );
		CFRelease( name );

	} else if (event == kCarPlayControlClientEvent_RemoveController) {
		CFIndex ndx, count;
		CFStringRef name = NULL;
		const char *cStr = NULL;
		char *storage = NULL;

		app_ulog( kLogLevelNotice, "CarPlayControlClientEvent Remove event received\n" );

		CarPlayControllerCopyName( controller, &name);
		CFStringGetOrCopyCStringUTF8(name, &cStr, &storage, NULL);
		app_ulog( kLogLevelNotice, "Removing CarPlayController '%s'\n", cStr );
		
		count = CFArrayGetCount( gCarPlayControllers );
		ndx = CFArrayGetFirstIndexOfValue( gCarPlayControllers, CFRangeMake(0, count), controller );
		
		CFArrayRemoveValueAtIndex( gCarPlayControllers, ndx );
		free( storage );
		CFRelease( name );
	} else {
		app_ulog( kLogLevelNotice, "CarPlayControlClientEvent event type %d received\n", (int)event );
	}
}

