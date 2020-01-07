/*
	File:    	APAdvertiser.c
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
	
	Copyright (C) 2015 Apple Inc. All Rights Reserved.
*/

#include "APConfig.h"

#include <CoreUtils/BonjourAdvertiser.h>
#include <CoreUtils/CFUtils.h>
#include <CoreUtils/CommonServices.h>
#include <CoreUtils/DebugServices.h>
#include <CoreUtils/LogUtils.h>

#include CF_RUNTIME_HEADER

#include "APAdvertiser.h"
#include "APAdvertiser.h"
#include "APAdvertiserInfo.h"
#include "AirPlayUtils.h"
#include "AirPlaySettings.h"

#include "dns_sd.h"

#define APSCheckIsCurrentDispatchQueue( __q__ )	{ }

#if 0
#pragma mark - Constants
#endif
#include <glib.h>
//===========================================================================================================================
//	Constants
//===========================================================================================================================

EXPORT_GLOBAL const CFStringRef kAPAdvertiserProperty_AdvertiserInfo =	CFSTR( "advertiserInfo" );
EXPORT_GLOBAL const CFStringRef kAPAdvertiserProperty_EnforceSolo =		CFSTR( "enforceSolo" );
EXPORT_GLOBAL const CFStringRef kAPAdvertiserProperty_InterfaceIndex =	CFSTR( "interfaceIndex" );
EXPORT_GLOBAL const CFStringRef kAPAdvertiserProperty_ListeningPort =	CFSTR( "listeningPort" );

#if 0
#pragma mark - Structures
#endif

//===========================================================================================================================
//	Structures
//===========================================================================================================================

struct OpaqueAPAdvertiser
{
	CFRuntimeBase					base;						// CF type info. Must be first.
	dispatch_queue_t				advertiserQueue;			// Queue for serializing operations for the advertiser.
	int								listeningPort;				// Listening port set by the client
	
	APAdvertiserInfoRef				advertiserInfo;				// Advertiser info object.
	APAdvertiserMode				mode;						// Advertising mode.
	
	Boolean							bonjourDisabled;			// True if Bonjour is disabled.
	Boolean							bonjourStarted;				// True if Bonjour advertising started.
	int								bonjourInterfaceIndex;		// Interface index to be advertised on (advertiser over all if 0)
	
	BonjourAdvertiserRef			bonjourAirPlay;				// airplay Bonjour advertiser object
};

#if 0
#pragma mark - Prototypes
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static void			_APAdvertiserFinalize( CFTypeRef inCF );
static void			_APAdvertiserInvalidate( void *inContext );
static void			_APAdvertiserCopyProperty( void *inContext );
static void			_APAdvertiserSetProperty( void *inContext );
static void			_APAdvertiserGetMode( void *inContext );
static void			_APAdvertiserSetMode( void *inContext );
static OSStatus		_APAdvertiserStartAdvertising( APAdvertiserRef inAdvertiser );
static OSStatus		_APAdvertiserStopAdvertising( APAdvertiserRef inAdvertiser );
static OSStatus		_APAdvertiserUpdateAdvertising( APAdvertiserRef inAdvertiser );
static OSStatus		_APAdvertiserStartBonjourAdvertising( APAdvertiserRef inAdvertiser );
static OSStatus		_APAdvertiserStopBonjourAdvertising( APAdvertiserRef inAdvertiser );
static OSStatus		_APAdvertiserUpdateBonjourAdvertising( APAdvertiserRef inAdvertiser );
static OSStatus		_APAdvertiserSetupBonjourAirPlay( APAdvertiserRef inAdvertiser );
static OSStatus		_APAdvertiserSetModeInternal( APAdvertiserRef inAdvertiser, APAdvertiserMode inMode );
static void			_APAdvertiserUpdatePreferences( void *inContext );
//static OSStatus		_APAdvertiserHandleDebug( CFDictionaryRef inRequest, CFDictionaryRef *outResponse, void *inContext );

#if 0
#pragma mark - Globals
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static const CFRuntimeClass		kAPAdvertiserClass =
{
	0,						// version
	"APAdvertiser",			// className
	NULL,					// init
	NULL,					// copy
	_APAdvertiserFinalize,	// finalize
	NULL,					// equal -- NULL means pointer equality.
	NULL,					// hash  -- NULL means pointer hash.
	NULL,					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

ulog_define( APAdvertiser, kLogLevelNotice, kLogFlags_Default, "APAdvertiser",  NULL );
#define apa_ucat()					&log_category_from_name( APAdvertiser )
#define apa_ulog( LEVEL, ... )		ulog( apa_ucat(), (LEVEL), __VA_ARGS__ )
#define apa_dlog( LEVEL, ... )		dlogc( apa_ucat(), (LEVEL), __VA_ARGS__ )

#if 0
#pragma mark - API
#endif

//===========================================================================================================================
//	APAdvertiserGetTypeID
//===========================================================================================================================

static void _APAdvertiserClassRegister( void *inCtx )
{
	CFTypeID *typeID = (CFTypeID*) inCtx;
	*typeID = _CFRuntimeRegisterClass( &kAPAdvertiserClass );
	check( *typeID != _kCFRuntimeNotATypeID );
}

EXPORT_GLOBAL
CFTypeID APAdvertiserGetTypeID( void )
{
	static dispatch_once_t		initOnce = 0;
	static CFTypeID				typeID = _kCFRuntimeNotATypeID;
	dispatch_once_f( &initOnce, &typeID, _APAdvertiserClassRegister );
	return( typeID );
}

//===========================================================================================================================
//	APAdvertiserCreate
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus APAdvertiserCreate( APAdvertiserRef *outAdvertiser )
{
	OSStatus			err = kNoErr;
	size_t				extraLength;
	APAdvertiserRef		obj;
	
	extraLength = sizeof( *obj ) - sizeof ( obj->base );
	obj = (APAdvertiserRef) _CFRuntimeCreateInstance( NULL, APAdvertiserGetTypeID(), (CFIndex) extraLength, NULL );
	require_action( obj, bail, err = kNoMemoryErr );
	memset( ( (uint8_t *) obj ) + sizeof( obj->base ), 0, extraLength );
	
	obj->advertiserQueue = dispatch_queue_create( "APAdvertiser", 0 );
	
	err = BonjourAdvertiserCreate( &obj->bonjourAirPlay );
	require_noerr( err, bail );
	BonjourAdvertiserSetDispatchQueue( obj->bonjourAirPlay, obj->advertiserQueue );
	
	// Temporary debug code
// #if( 0 )
// 	if( IsAppleInternalBuild() )
// 	{
// 		DebugIPC_EnsureInitialized( _APAdvertiserHandleDebug, obj );
// 	}
// #endif
	
	*outAdvertiser = obj;
	obj = NULL;
	
bail:
	return( err );
}

//===========================================================================================================================
//	APAdvertiserInvalidate
//===========================================================================================================================

typedef struct
{
	APAdvertiserRef			advertiser;
	OSStatus				error;
	
}	APAdvertiserInvalidateParams;

EXPORT_GLOBAL
OSStatus APAdvertiserInvalidate( APAdvertiserRef inAdvertiser )
{
	APAdvertiserInvalidateParams		params = { inAdvertiser, kNoErr };
	
	dispatch_sync_f( inAdvertiser->advertiserQueue, &params, _APAdvertiserInvalidate );
	check_noerr( params.error );

	return( params.error );
}

//===========================================================================================================================
//	APAdvertiserGetMode
//===========================================================================================================================

typedef struct
{
	APAdvertiserRef			advertiser;
	APAdvertiserMode		mode;
	OSStatus				error;
	
}	APAdvertiserModeParams;

EXPORT_GLOBAL
OSStatus APAdvertiserGetMode( APAdvertiserRef inAdvertiser, APAdvertiserMode *outAdvertisingMode )
{
	APAdvertiserModeParams		params = { inAdvertiser, kAPAdvertiserMode_None, kNoErr };
	
	dispatch_sync_f( inAdvertiser->advertiserQueue, &params, _APAdvertiserGetMode );
	
	*outAdvertisingMode = params.mode;
	
	return( params.error );
}

//===========================================================================================================================
//	APAdvertiserSetMode
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus APAdvertiserSetMode( APAdvertiserRef inAdvertiser, APAdvertiserMode inMode )
{
	APAdvertiserModeParams		params = { inAdvertiser, inMode, kNoErr };
	
	dispatch_sync_f( inAdvertiser->advertiserQueue, &params, _APAdvertiserSetMode );
	
	return( params.error );
}

//===========================================================================================================================
//	APAdvertiserCopyProperty
//===========================================================================================================================

typedef struct
{
	APAdvertiserRef			advertiser;
	CFStringRef				property;
	CFAllocatorRef			allocator;
	void *					value;
	OSStatus				error;
	
}	APAdvertiserCopyPropertyParams;

EXPORT_GLOBAL
OSStatus
	APAdvertiserCopyProperty(
		APAdvertiserRef	inAdvertiser,
		CFStringRef inProperty,
		CFAllocatorRef inAllocator,
		void *outValue )
{
	APAdvertiserCopyPropertyParams		params = { inAdvertiser, inProperty, inAllocator, outValue, kNoErr };
	
	dispatch_sync_f( inAdvertiser->advertiserQueue, &params, _APAdvertiserCopyProperty );
		
	return( params.error );
}

//===========================================================================================================================
//	APAdvertiserSetProperty
//===========================================================================================================================

typedef struct
{
	APAdvertiserRef			advertiser;
	CFStringRef				property;
	CFTypeRef				value;
	OSStatus				error;
	
}	APAdvertiserSetPropertyParams;

EXPORT_GLOBAL
OSStatus APAdvertiserSetProperty( APAdvertiserRef inAdvertiser, CFStringRef inProperty, CFTypeRef inValue )
{
	APAdvertiserSetPropertyParams		params = { inAdvertiser, inProperty, inValue, kNoErr };
	
	dispatch_sync_f( inAdvertiser->advertiserQueue, &params, _APAdvertiserSetProperty );
	
	return( params.error );
}

//===========================================================================================================================
//	APAdvertiserUpdatePreferences
//===========================================================================================================================

typedef struct
{
	APAdvertiserRef			advertiser;
	Boolean					shouldSynchronize;
	OSStatus				error;
	
}	APAdvertiserUpdatePreferencesParams;

EXPORT_GLOBAL
OSStatus APAdvertiserUpdatePreferences( APAdvertiserRef inAdvertiser, Boolean inShouldSynchronize )
{
	APAdvertiserUpdatePreferencesParams		params = { inAdvertiser, inShouldSynchronize, kNoErr };
	
	dispatch_sync_f( inAdvertiser->advertiserQueue, &params, _APAdvertiserUpdatePreferences );
	
	return( params.error );
}

#if 0
#pragma mark - Internal
#endif

//===========================================================================================================================
//	_APAdvertiserFinalize
//===========================================================================================================================

static void _APAdvertiserFinalize( CFTypeRef inCF )
{
	APAdvertiserRef const	advertiser = (APAdvertiserRef) inCF;
	
	BonjourAdvertiserForget( &advertiser->bonjourAirPlay );
	
	ForgetCF( &advertiser->advertiserInfo );

	dispatch_forget( &advertiser->advertiserQueue );
}

//===========================================================================================================================
//	_APAdvertiserInvalidate
//===========================================================================================================================

static void _APAdvertiserInvalidate( void *inContext )
{
	OSStatus								err = kNoErr;
	APAdvertiserInvalidateParams * const	params = (APAdvertiserInvalidateParams *) inContext;
	APAdvertiserRef							inAdvertiser = params->advertiser;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	err = _APAdvertiserSetModeInternal( inAdvertiser, kAPAdvertiserMode_None );
	require_noerr( err, bail );
	
bail:
	params->error = err;
}

//===========================================================================================================================
//	_APAdvertiserGetMode
//===========================================================================================================================

static void _APAdvertiserGetMode( void *inContext )
{
	OSStatus								err = kNoErr;
	APAdvertiserModeParams * const			params = (APAdvertiserModeParams *) inContext;
	APAdvertiserRef							inAdvertiser = params->advertiser;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	params->mode = inAdvertiser->mode;
	params->error = err;
}

//===========================================================================================================================
//	_APAdvertiserSetModeInternal
//===========================================================================================================================

static OSStatus _APAdvertiserSetModeInternal( APAdvertiserRef inAdvertiser, APAdvertiserMode inMode )
{
	OSStatus								err = kNoErr;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	require_action_quiet( inMode != inAdvertiser->mode, bail, err = kNoErr );
	
	if( inMode == kAPAdvertiserMode_None )
	{
		err = _APAdvertiserStopAdvertising( inAdvertiser );
		require_noerr( err, bail );
		
		inAdvertiser->mode = inMode;
	}
	else if( inMode == kAPAdvertiserMode_Discoverable )
	{
		err = _APAdvertiserStartAdvertising( inAdvertiser );
		require_noerr( err, bail );
		
		inAdvertiser->mode = inMode;
	}
	else
	{
		apa_ulog( kLogLevelError, "Unrecognized advertiser mode %d.\n", inMode );
		err = kNotFoundErr;
		goto bail;
	}

bail:
	return( err );
}

//===========================================================================================================================
//	_APAdvertiserSetMode
//===========================================================================================================================

static void _APAdvertiserSetMode( void *inContext )
{
	OSStatus								err = kNoErr;
	APAdvertiserModeParams * const			params = (APAdvertiserModeParams *) inContext;
	APAdvertiserRef							inAdvertiser = params->advertiser;
	APAdvertiserMode						inMode = params->mode;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	err = _APAdvertiserSetModeInternal( inAdvertiser, inMode );
	require_noerr( err, bail );
	
bail:
	params->error = err;
}

//===========================================================================================================================
//	_APAdvertiserSetAdvertiserInfo
//===========================================================================================================================

static OSStatus _APAdvertiserSetAdvertiserInfo( APAdvertiserRef inAdvertiser, APAdvertiserInfoRef inAdvertiserInfo )
{
	OSStatus				err			= kNoErr;
	APAdvertiserInfoRef		copyInfo	= NULL;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	if( !CFEqualNullSafe( inAdvertiser->advertiserInfo, inAdvertiserInfo ) )
	{
		err = APAdvertiserInfoCopy( kCFAllocatorDefault, inAdvertiserInfo, &copyInfo );
		require_noerr( err, bail );
		
		CFReleaseNullSafe( inAdvertiser->advertiserInfo );
		inAdvertiser->advertiserInfo = copyInfo;
		
		err = _APAdvertiserUpdateAdvertising( inAdvertiser );
		check_noerr( err );
	}

bail:
	return( err );
}

//===========================================================================================================================
//	_APAdvertiserCopyAdvertiserInfo
//===========================================================================================================================

static OSStatus _APAdvertiserCopyAdvertiserInfo( APAdvertiserRef inAdvertiser, CFTypeRef *outAdvertiserInfo )
{
	OSStatus				err			= kNoErr;
	APAdvertiserInfoRef		infoCopy	= NULL;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	if( inAdvertiser->advertiserInfo )
	{
		err = APAdvertiserInfoCopy( kCFAllocatorDefault, inAdvertiser->advertiserInfo, &infoCopy );
		require_noerr( err, bail );
	}
	
	*outAdvertiserInfo = (CFTypeRef) infoCopy;

bail:
	return( err );
}

//===========================================================================================================================
//	_APAdvertiserCopyProperty
//===========================================================================================================================

static void _APAdvertiserCopyProperty( void *inContext )
{
	OSStatus									err = kNoErr;
	APAdvertiserCopyPropertyParams * const		params = (APAdvertiserCopyPropertyParams *) inContext;
	APAdvertiserRef								inAdvertiser = params->advertiser;
	CFStringRef									inProperty = params->property;
	void *										outValue = params->value;
	CFTypeRef									value = NULL;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	require_action( outValue, bail, err = kParamErr );
	require_action( inProperty, bail, err = kParamErr );
	
	if( 0 ) {}
	
	// Advertiser Info
	
	else if( CFEqual( inProperty, kAPAdvertiserProperty_AdvertiserInfo ) )
	{
		err = _APAdvertiserCopyAdvertiserInfo( inAdvertiser, &value );
		require_noerr( err, bail );
	}
	
	// Interface Index
	
	else if( CFEqual( inProperty, kAPAdvertiserProperty_InterfaceIndex ) )
	{
		value = CFNumberCreateInt64( inAdvertiser->bonjourInterfaceIndex );
	}
	
	// Listening Port
	
	else if( CFEqual( inProperty, kAPAdvertiserProperty_ListeningPort ) )
	{
		value = CFNumberCreateInt64( inAdvertiser->listeningPort );
	}

	// Unknown
	
	else
	{
		err = kNotFoundErr;
	}
	
	*( (CFTypeRef *) outValue ) = value;
	
bail:
	params->error = err;
}

//===========================================================================================================================
//	_APAdvertiserSetProperty
//===========================================================================================================================

static void _APAdvertiserSetProperty( void *inContext )
{
	OSStatus									err = kNoErr;
	APAdvertiserSetPropertyParams * const		params = (APAdvertiserSetPropertyParams *) inContext;
	APAdvertiserRef								inAdvertiser = params->advertiser;
	CFStringRef									inProperty = params->property;
	CFTypeRef									inValue = params->value;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	require_action( inProperty, bail, err = kParamErr );
	
	if( 0 ) {}
	
	// Advertiser Info
	
	else if( CFEqual( inProperty, kAPAdvertiserProperty_AdvertiserInfo ) )
	{
		err = _APAdvertiserSetAdvertiserInfo( inAdvertiser, (APAdvertiserInfoRef) inValue );
		require_noerr( err, bail );
	}
	
	// Interface Index
	
	else if( CFEqual( inProperty, kAPAdvertiserProperty_InterfaceIndex ) )
	{
		uint32_t interfaceIndex = (uint32_t) CFGetInt64( inValue, &err );
		require_noerr( err, bail );
		
		inAdvertiser->bonjourInterfaceIndex = (int) interfaceIndex;
	}
	
	// Listening Port
	
	else if( CFEqual( inProperty, kAPAdvertiserProperty_ListeningPort ) )
	{
		uint32_t listeningPort = (uint32_t) CFGetInt64( inValue, &err );
		require_noerr( err, bail );
		require_action( listeningPort > 0, bail, err = kParamErr );
		
		inAdvertiser->listeningPort = (int) listeningPort;
	}
	
	// Unknown
	
	else
	{
		err = kNotFoundErr;
	}

bail:
	params->error = err;
}


//===========================================================================================================================
//	_APAdvertiserUpdateBonjourAdvertising
//===========================================================================================================================

static OSStatus _APAdvertiserUpdateBonjourAdvertising( APAdvertiserRef inAdvertiser )
{
	OSStatus		err = kNoErr;
	CFDataRef		airPlayTxtRecord = NULL;
	CFDataRef		raopTxtRecord = NULL;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	require_action_quiet( inAdvertiser->bonjourStarted, bail, err = kNotPreparedErr );
	
	err = APAdvertiserInfoCopyAirPlayData( inAdvertiser->advertiserInfo, &airPlayTxtRecord );
	// HANDLE ERRORS (do we report this back to "SetInfo" or do we update advertiser via a separate API?)
	
	err = BonjourAdvertiserSetTXTRecord( inAdvertiser->bonjourAirPlay, CFDataGetBytePtr( airPlayTxtRecord ),
										(size_t) CFDataGetLength( airPlayTxtRecord ) );
	// HANDLE ERROR
	
	err = BonjourAdvertiserUpdate( inAdvertiser->bonjourAirPlay );
	// HANDLE ERROR
	
bail:
	CFReleaseNullSafe( airPlayTxtRecord );
	CFReleaseNullSafe( raopTxtRecord );
	return( err );
}

//===========================================================================================================================
//	_APAdvertiserUpdateAdvertising
//===========================================================================================================================

static OSStatus _APAdvertiserUpdateAdvertising( APAdvertiserRef inAdvertiser )
{
	OSStatus		err = kNoErr;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	if( inAdvertiser->mode == kAPAdvertiserMode_Discoverable )
	{
		if( inAdvertiser->bonjourDisabled && inAdvertiser->bonjourStarted )
		{
			err = _APAdvertiserStopBonjourAdvertising( inAdvertiser );
			require_noerr( err, bail );
		}
		else if( !inAdvertiser->bonjourDisabled && !inAdvertiser->bonjourStarted )
		{
			err = _APAdvertiserStartBonjourAdvertising( inAdvertiser );
			require_noerr( err, bail );
		}
		else if ( !inAdvertiser->bonjourDisabled && inAdvertiser->bonjourStarted )
		{
			err = _APAdvertiserUpdateBonjourAdvertising( inAdvertiser );
			require_noerr( err, bail );
		}
		
	}
	
bail:
	return( err );
}

//===========================================================================================================================
//	_APAdvertiserStartBonjourAdvertising
//===========================================================================================================================

static OSStatus _APAdvertiserStartBonjourAdvertising( APAdvertiserRef inAdvertiser )
{
	OSStatus			err = kNoErr;
	CFDataRef			airPlayTxtRecord = NULL;
	CFDataRef			raopTxtRecord = NULL;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	require_action( inAdvertiser->advertiserInfo, bail,
	{
		apa_ulog( kLogLevelError, "Advertiser info is required to start Bonjour advertising.\n" );
		err = kNotPreparedErr;
	} );
	
	// _airplay
	
	err = _APAdvertiserSetupBonjourAirPlay( inAdvertiser );
	require_noerr( err, bail );
	
	err = APAdvertiserInfoCopyAirPlayData( inAdvertiser->advertiserInfo, &airPlayTxtRecord );
	require_noerr_action( err, bail,
	{
		apa_ulog( kLogLevelError, "Unable to create a valid Bonjour TXT record for _airplay from advertiser info.\n" );
	} );
	
	err = BonjourAdvertiserSetTXTRecord( inAdvertiser->bonjourAirPlay, CFDataGetBytePtr( airPlayTxtRecord ),
										(size_t) CFDataGetLength( airPlayTxtRecord ) );
	require_noerr( err, bail );
	
	err = BonjourAdvertiserStart( inAdvertiser->bonjourAirPlay );
	require_noerr( err, bail );
	
	apa_ulog( kLogLevelInfo, "Started advertising _airplay service.\n" );
	
	inAdvertiser->bonjourStarted = true;
	
bail:
	CFReleaseNullSafe( airPlayTxtRecord );
	CFReleaseNullSafe( raopTxtRecord );

	return( err );
}

//===========================================================================================================================
//	_APAdvertiserStartAdvertising
//===========================================================================================================================

static OSStatus _APAdvertiserStartAdvertising( APAdvertiserRef inAdvertiser )
{
	OSStatus			err = kNoErr;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	require_action( inAdvertiser->advertiserInfo, bail, err = kNotPreparedErr );
	require_action( inAdvertiser->listeningPort > 0, bail,err = kNotPreparedErr );
	
	if( !inAdvertiser->bonjourDisabled && !inAdvertiser->bonjourStarted )
	{
		err = _APAdvertiserStartBonjourAdvertising( inAdvertiser );
		require_noerr( err, bail );
	}
	
bail:
	return( err );
}

//===========================================================================================================================
//	_APAdvertiserStopBonjourAdvertising
//===========================================================================================================================

static OSStatus _APAdvertiserStopBonjourAdvertising( APAdvertiserRef inAdvertiser )
{
	OSStatus		err = kNoErr;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	BonjourAdvertiserStop( inAdvertiser->bonjourAirPlay );
	inAdvertiser->bonjourStarted = false;
	
	return( err );
}

//===========================================================================================================================
//	_APAdvertiserStopAdvertising
//===========================================================================================================================

static OSStatus _APAdvertiserStopAdvertising( APAdvertiserRef inAdvertiser )
{
	OSStatus		err = kNoErr;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	err = _APAdvertiserStopBonjourAdvertising( inAdvertiser );
	require_noerr( err, bail );
	
bail:
	return( err );
}

//===========================================================================================================================
//	_APAdvertiserSetupBonjourAirPlay
//===========================================================================================================================

static OSStatus _APAdvertiserSetupBonjourAirPlay( APAdvertiserRef inAdvertiser )
{
	OSStatus			err = kNoErr;
	const char *		domain;
	CFStringRef			airPlayServiceName = NULL;
	char				name[256];
	Boolean				success;
	uint32_t			ifIndex;
	CFNumberRef			features;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	domain = kAirPlayBonjourServiceDomain;
	ifIndex = inAdvertiser->bonjourInterfaceIndex ? inAdvertiser->bonjourInterfaceIndex : kDNSServiceInterfaceIndexAny;
	
	err = APAdvertiserInfoCreateAirPlayServiceName( inAdvertiser->advertiserInfo, &airPlayServiceName );
	require_noerr( err, bail );
	
	success = CFStringGetCString( airPlayServiceName, name, sizeof( name ), kCFStringEncodingUTF8 );
	require_action( success, bail, err = kInternalErr );
	
	err = BonjourAdvertiserSetDomain( inAdvertiser->bonjourAirPlay, domain );
	require_noerr( err, bail );
	err = BonjourAdvertiserSetName( inAdvertiser->bonjourAirPlay, name );
	require_noerr( err, bail );
	err = BonjourAdvertiserSetServiceType( inAdvertiser->bonjourAirPlay, kAirPlayBonjourServiceType );
	require_noerr( err, bail );
	APAdvertiserInfoCopyProperty( inAdvertiser->advertiserInfo, kAPAdvertiserInfoProperty_Features, kCFAllocatorDefault, &features );
	if( features && ( CFGetInt64( features, NULL ) & kAirPlayFeature_Car ) )
		BonjourAdvertiserSetFlags( inAdvertiser->bonjourAirPlay, kDNSServiceFlagsKnownUnique );

	BonjourAdvertiserSetInterfaceIndex( inAdvertiser->bonjourAirPlay, ifIndex );
	BonjourAdvertiserSetPort( inAdvertiser->bonjourAirPlay, inAdvertiser->listeningPort );
	
bail:
	CFReleaseNullSafe( airPlayServiceName );
	return( err );
}

//===========================================================================================================================
//	_APAdvertiserUpdatePreferences
//===========================================================================================================================

static void _APAdvertiserUpdatePreferences( void *inContext )
{
	OSStatus										err = kNoErr;
	APAdvertiserUpdatePreferencesParams * const		params = (APAdvertiserUpdatePreferencesParams *) inContext;
	APAdvertiserRef									inAdvertiser = params->advertiser;
	Boolean											inShouldSynchronize = params->shouldSynchronize;
	Boolean											b;
	Boolean											updateAdvertising = false;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	if( inShouldSynchronize )
	{
		AirPlaySettings_Synchronize();
	}
	
	// BonjourDisabled
	
	b = AirPlaySettings_GetBoolean( NULL, CFSTR( kAirPlayPrefKey_BonjourDisabled ), NULL );
	if( b != inAdvertiser->bonjourDisabled )
	{
		apa_ulog( kLogLevelInfo, "Bonjour Disabled: %s -> %s\n", !b ? "yes" : "no", b ? "yes" : "no" );
		inAdvertiser->bonjourDisabled = b;
		updateAdvertising = true;
	}
	
	// Act on any settings changes.
	
	if( updateAdvertising )
	{
		err = _APAdvertiserUpdateAdvertising( inAdvertiser );
		require_noerr( err, bail );
	}
	
bail:
	params->error = err;
}

#if( 0 )
//===========================================================================================================================
//	_APAdvertiserHandleDebug
//===========================================================================================================================

static OSStatus	_APAdvertiserHandleDebug( CFDictionaryRef inRequest, CFDictionaryRef *outResponse, void *inContext )
{
	OSStatus					err;
	CFStringRef					opcode;
	DataBuffer					dataBuf;
	CFMutableDictionaryRef		response;
	APAdvertiserRef advertiser = (APAdvertiserRef) inContext;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiser->advertiserQueue );
	
	DataBuffer_Init( &dataBuf, NULL, 0, 10 * kBytesPerMegaByte );
	response = NULL;
	
	opcode = (CFStringRef) CFDictionaryGetValue( inRequest, kDebugIPCKey_Command );
	require_action_quiet( opcode, bail, err = kNotHandledErr );
	require_action( CFIsType( opcode, CFString ), bail, err = kTypeErr );
	
	if( 0 ) {} // Empty if to simplify else if's below.
	
	// Show
	
	else if( CFEqual( opcode, kDebugIPCOpCode_Show ) )
	{
		int		verbose;
		
		verbose = (int) CFDictionaryGetInt64( inRequest, CFSTR( "verbose" ), NULL );
		err = APAdvertiserDebugShow( advertiser, verbose, &dataBuf );
		require_noerr( err, bail );
		
		err = CFPropertyListCreateFormatted( NULL, &response, "{%kO=%.*s}",
											kDebugIPCKey_Value, (int) dataBuf.bufferLen, dataBuf.bufferPtr );
		require_noerr( err, bail );
	}
	
	// Other
	
	else
	{
		apa_dlog( kLogLevelWarning, "### Unsupported debug command: %@\n", opcode );
		err = kNotHandledErr;
		goto bail;
	}
	
	if( response )
	{
		CFDictionarySetValue( response, kDebugIPCKey_ResponseType, opcode );
	}
	*outResponse = response;
	response = NULL;
	err = kNoErr;
	
bail:
	DataBuffer_Free( &dataBuf );
	if( response )
	{
		CFRelease( response );
	}
	return( err );
}
#endif

//===========================================================================================================================
//	_APAdvertiserDebugShow
//===========================================================================================================================

typedef struct
{
	APAdvertiserRef			advertiser;
	int						verbose;
	DataBuffer *			dataBuffer;
	OSStatus				error;
	
}	APAdvertiserDebugShowParams;

static void _APAdvertiserDebugShow( void *inContext )
{
	OSStatus						err = kNoErr;
	APAdvertiserDebugShowParams *	params = (APAdvertiserDebugShowParams *) inContext;
	APAdvertiserRef					advertiser = params->advertiser;
	int								verbose = params->verbose;
	DataBuffer *					dataBuffer = params->dataBuffer;
	
	APSCheckIsCurrentDispatchQueue( advertiser->advertiserQueue );
	
	DataBuffer_AppendF( dataBuffer, "\n" );
	DataBuffer_AppendF( dataBuffer, "+-+ AirPlay Advertiser state +-+\n" );
	DataBuffer_AppendF( dataBuffer, "\n" );
	
	DataBuffer_AppendF( dataBuffer, "Mode=%s", ( advertiser->mode == kAPAdvertiserMode_None ) ? "none" : "presence" );
	DataBuffer_AppendF( dataBuffer, " listeningPort=%5d", advertiser->listeningPort );
	
	if( verbose && advertiser->advertiserInfo )
	{
		DataBuffer_AppendF( dataBuffer, "\n");
		err = APAdvertiserInfoDebugShow( advertiser->advertiserInfo, verbose, dataBuffer );
		check_noerr( err );
	}
	
	params->error = err;
	return;
}

//===========================================================================================================================
//	APAdvertiserDebugShow
//===========================================================================================================================

OSStatus APAdvertiserDebugShow( APAdvertiserRef inAdvertiser, int inVerbose, DataBuffer *inDataBuf )
{
	APAdvertiserDebugShowParams			params = { inAdvertiser, inVerbose, inDataBuf, kNoErr };

	dispatch_sync_f( inAdvertiser->advertiserQueue, &params, _APAdvertiserDebugShow );

	return( params.error );
}

