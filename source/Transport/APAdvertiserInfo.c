/*
	File:    	APAdvertiserInfo.c
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

	#include "APSDispatchUtils.h"
#include <CoreUtils/DebugServices.h>
#include <CoreUtils/CommonServices.h>
#include <CoreUtils/CFUtils.h>
#include <CoreUtils/StringUtils.h>
#include CF_RUNTIME_HEADER
#include "dns_sd.h"


#include "APFeatures.h"
#include "APStatusFlags.h"
#include "APAdvertiserInfo.h"
#include "APDiscoveryBonjourCommon.h"

#if 0
#pragma mark - Constants
#endif
#include<glib.h>
//===========================================================================================================================
//	Constants
//===========================================================================================================================

// Required Property Keys
const CFStringRef kAPAdvertiserInfoProperty_DeviceID =						CFSTR( "deviceID" );
const CFStringRef kAPAdvertiserInfoProperty_DeviceName =					CFSTR( "deviceName" );

// Optional Property Keys
const CFStringRef kAPAdvertiserInfoProperty_AirPlayVersion =				CFSTR( "airPlayVersion" );
const CFStringRef kAPAdvertiserInfoProperty_DeviceModel =					CFSTR( "deviceModel" );
const CFStringRef kAPAdvertiserInfoProperty_Features =						CFSTR( "features" );
const CFStringRef kAPAdvertiserInfoProperty_FirmwareVersion =				CFSTR( "firmwareVersion" );
const CFStringRef kAPAdvertiserInfoProperty_SystemFlags =					CFSTR( "systemFlags" );
const CFStringRef kAPAdvertiserInfoProperty_PasswordRequired =				CFSTR( "password" );
const CFStringRef kAPAdvertiserInfoProperty_PINRequired =					CFSTR( "PIN" );
const CFStringRef kAPAdvertiserInfoProperty_ProtocolVersion =				CFSTR( "protocolVersion" );
const CFStringRef kAPAdvertiserInfoProperty_PublicHomeKitPairingIdentity =	CFSTR( "publicHomeKitPairingIdentity" );
const CFStringRef kAPAdvertiserInfoProperty_PublicKey =						CFSTR( "publicKey" );

#if 0
#pragma mark - Structures
#endif

//===========================================================================================================================
//	Structures
//===========================================================================================================================

// APAdvertiserInfo

struct OpaqueAPAdvertiserInfo
{
	CFRuntimeBase					base;						// CF type info. Must be first.
	dispatch_queue_t				queue;						// Queue for serializing operations for the advertiser.
	
	CFStringRef						deviceID;
	CFStringRef						deviceName;
	APFeatures						features;
	CFStringRef						firmwareVersion;
	APStatusFlags					flags;
	CFStringRef						model;
	Boolean							password;
	Boolean							pinEnabled;
	CFStringRef						protocolVersion;
	CFStringRef						publicHKPIdentity;
	CFStringRef						publicKey;
	uint32_t						seed;
	CFStringRef						sourceVersion;
};

#if 0
#pragma mark - Prototypes
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static void		APAdvertiserInfoFinalize( CFTypeRef inCF );
static Boolean	_APAdvertiserInfoCompare( CFTypeRef inCF1, CFTypeRef inCF2 );
static CFStringRef APAdvertiserInfoCopyDescription( CFTypeRef inCF );
static void		_APAdvertiserInfoCopyProperty( void *inContext );
static void		_APAdvertiserInfoSetProperty( void *inContext );
static void		_APAdvertiserInfoCopyDescription( void *inContext );
static OSStatus
	_APAdvertiserInfoCopyInternal(
		CFAllocatorRef inAllocator,
		APAdvertiserInfoRef inAdvertiserInfo,
		APAdvertiserInfoRef *outAdvertiserInfoCopy );
static void _APAdvertiserInfoCopy( void *inContext );
static OSStatus
	_APAdvertiserInfoReplaceStringProperty(
		CFAllocatorRef inAllocator,
		CFStringRef inValue,
		CFStringRef *inDestination );
static void	_APAdvertiserInfoCopyAirPlayData( void *inContext );
static void _APAdvertiserInfoCreateAirPlayServiceName( void *inContext );

#if 0
#pragma mark - Globals
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static const CFRuntimeClass		kAPAdvertiserInfoClass =
{
	0,										// version
	"APAdvertiserInfo",						// className
	NULL,									// init
	NULL,									// copy
	APAdvertiserInfoFinalize,				// finalize
	_APAdvertiserInfoCompare,				// equal -- NULL means pointer equality.
	NULL,									// hash  -- NULL means pointer hash.
	NULL,									// copyFormattingDesc
	APAdvertiserInfoCopyDescription,		// copyDebugDesc
	NULL,									// reclaim
	NULL									// refcount
};

ulog_define( APAdvertiserInfo, kLogLevelNotice, kLogFlags_Default, "APAdvertiserInfo",  NULL );
#define apai_ucat()					&log_category_from_name( APAdvertiserInfo )
#define apai_ulog( LEVEL, ... )		ulog( apai_ucat(), (LEVEL), __VA_ARGS__ )
#define apai_dlog( LEVEL, ... )		dlogc( apai_ucat(), (LEVEL), __VA_ARGS__ )

#if 0
#pragma mark - API
#endif

//===========================================================================================================================
//	APAdvertiserInfoGetTypeID
//===========================================================================================================================

static void _APAdvertiserInfoClassRegister( void *inCtx )
{
	CFTypeID *typeID = (CFTypeID*) inCtx;
	*typeID = _CFRuntimeRegisterClass( &kAPAdvertiserInfoClass );
	check( *typeID != _kCFRuntimeNotATypeID );
}

EXPORT_GLOBAL
CFTypeID APAdvertiserInfoGetTypeID( void )
{
	static dispatch_once_t		initOnce = 0;
	static CFTypeID				typeID = _kCFRuntimeNotATypeID;
	dispatch_once_f( &initOnce, &typeID, _APAdvertiserInfoClassRegister );
	return( typeID );
}

//===========================================================================================================================
//	APAdvertiserInfoCreate
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus APAdvertiserInfoCreate( CFAllocatorRef inAllocator, APAdvertiserInfoRef *outAdvertiserInfo )
{
	OSStatus error = kNoErr;
	size_t	extraLength;
	APAdvertiserInfoRef obj = NULL;
	
	extraLength = sizeof( *obj ) - sizeof ( obj->base );
	obj = (APAdvertiserInfoRef) _CFRuntimeCreateInstance( inAllocator, APAdvertiserInfoGetTypeID(),
		(CFIndex) extraLength, NULL );
	require_action( obj, bail, error = kNoMemoryErr );
	memset( ( (uint8_t *) obj ) + sizeof( obj->base ), 0, extraLength );
	
	obj->queue = dispatch_queue_create( "APAdvertiserInfo", 0 );
	
	*outAdvertiserInfo = obj;
	obj = NULL;
	
bail:
	CFReleaseNullSafe( obj );
	return( error );
}

//===========================================================================================================================
//	APAdvertiserInfoCopyProperty
//===========================================================================================================================

typedef struct
{
	APAdvertiserInfoRef		advertiserInfo;
	CFStringRef				property;
	CFAllocatorRef			allocator;
	void *					value;
	OSStatus				error;
	
}	APAdvertiserInfoCopyPropertyParams;

EXPORT_GLOBAL
OSStatus
	APAdvertiserInfoCopyProperty(
		APAdvertiserInfoRef	inAdvertiserInfo,
		CFStringRef inProperty,
		CFAllocatorRef inAllocator,
		void *outValue )
{
	APAdvertiserInfoCopyPropertyParams		params = { inAdvertiserInfo, inProperty, inAllocator, outValue, kNoErr };
	
	dispatch_sync_f( inAdvertiserInfo->queue, &params, _APAdvertiserInfoCopyProperty );
	
	return( params.error );
}

//===========================================================================================================================
//	APAdvertiserInfoSetProperty
//===========================================================================================================================

typedef struct
{
	APAdvertiserInfoRef		advertiserInfo;
	CFStringRef				property;
	CFTypeRef				value;
	OSStatus				error;
	
}	APAdvertiserInfoSetPropertyParams;

EXPORT_GLOBAL
OSStatus
	APAdvertiserInfoSetProperty(
		APAdvertiserInfoRef inAdvertiserInfo,
		CFStringRef inProperty,
		CFTypeRef inValue )
{
	APAdvertiserInfoSetPropertyParams		params = { inAdvertiserInfo, inProperty, inValue, kNoErr };
	
	dispatch_sync_f( inAdvertiserInfo->queue, &params, _APAdvertiserInfoSetProperty );
	
	return( params.error );
}

//===========================================================================================================================
//	APAdvertiserInfoCopyDescription
//===========================================================================================================================

typedef struct
{
	APAdvertiserInfoRef		info;
	CFStringRef *			descriptionCopy;
	OSStatus				err;
}	APAdvertiserInfoCopyDescriptionParams;

static CFStringRef APAdvertiserInfoCopyDescription( CFTypeRef inCF )
{
	APAdvertiserInfoRef							info			= (APAdvertiserInfoRef) inCF;
	CFStringRef									descriptionCopy = NULL;
	APAdvertiserInfoCopyDescriptionParams		params = { info, &descriptionCopy, kNoErr };
	
	dispatch_sync_f( info->queue, &params, _APAdvertiserInfoCopyDescription );
	require_noerr( params.err, bail );
	
bail:
	return( descriptionCopy );
}

#if 0
#pragma mark - SPI
#endif

//===========================================================================================================================
//	APAdvertiserInfoCopy
//===========================================================================================================================

typedef struct
{
	CFAllocatorRef			allocator;
	APAdvertiserInfoRef		info;
	APAdvertiserInfoRef		*copiedInfo;
	OSStatus				error;
	
}	APAdvertiserInfoCopyParams;

EXPORT_GLOBAL
OSStatus
	APAdvertiserInfoCopy(
		CFAllocatorRef inAllocator,
		APAdvertiserInfoRef inAdvertiserInfo,
		APAdvertiserInfoRef *outAdvertiserInfoCopy )
{
	APAdvertiserInfoCopyParams		params = { inAllocator, inAdvertiserInfo, outAdvertiserInfoCopy, kNoErr };
	
	dispatch_sync_f( inAdvertiserInfo->queue, &params, _APAdvertiserInfoCopy );
	
	return( params.error );
}
//===========================================================================================================================
//	APAdvertiserInfoCopyAirPlayData
//===========================================================================================================================

typedef struct
{
	APAdvertiserInfoRef		info;
	CFDataRef *				outData;
	OSStatus				error;
}	APAdvertiserInfoCopyDataParams;

EXPORT_GLOBAL
OSStatus APAdvertiserInfoCopyAirPlayData( APAdvertiserInfoRef inAdvertiserInfo, CFDataRef *outTXTRecord )
{
	APAdvertiserInfoCopyDataParams		params = { inAdvertiserInfo, outTXTRecord, kNoErr };
	
	dispatch_sync_f( inAdvertiserInfo->queue, &params, _APAdvertiserInfoCopyAirPlayData );
	
	return( params.error );
}

//===========================================================================================================================
//	APAdvertiserInfoCreateAirPlayServiceName
//===========================================================================================================================

typedef struct
{
	APAdvertiserInfoRef		info;
	CFStringRef *			outName;
	OSStatus				error;
}	APAdvertiserInfoCopyNameParams;

EXPORT_GLOBAL
OSStatus APAdvertiserInfoCreateAirPlayServiceName( APAdvertiserInfoRef inAdvertiserInfo, CFStringRef *outServiceName )
{
	APAdvertiserInfoCopyNameParams		params = { inAdvertiserInfo, outServiceName, kNoErr };
	
	dispatch_sync_f( inAdvertiserInfo->queue, &params, _APAdvertiserInfoCreateAirPlayServiceName );
	
	return( params.error );
}

#if 0
#pragma mark - Internal
#endif

//===========================================================================================================================
//	APAdvertiserInfoFinalize
//===========================================================================================================================

static void APAdvertiserInfoFinalize( CFTypeRef inCF )
{
	APAdvertiserInfoRef const	info = (APAdvertiserInfoRef) inCF;
	
	ForgetCF( &info->deviceID );
	ForgetCF( &info->deviceName );
	ForgetCF( &info->firmwareVersion );
	ForgetCF( &info->model );
	ForgetCF( &info->protocolVersion );
	ForgetCF( &info->publicHKPIdentity );
	ForgetCF( &info->publicKey );
	ForgetCF( &info->sourceVersion );
	
	dispatch_forget( &info->queue );
}

//===========================================================================================================================
//	_APAdvertiserInfoCompare
//===========================================================================================================================

static Boolean _APAdvertiserInfoCompare( CFTypeRef inCF1, CFTypeRef inCF2 )
{
	Boolean					equal = false;
	APAdvertiserInfoRef		info1 = (APAdvertiserInfoRef) inCF1;
	APAdvertiserInfoRef		info2 = (APAdvertiserInfoRef) inCF2;
	
	if( !inCF1 && !inCF2 )
	{
		return( true );
	}
	
	// TODO: dispatch on internal queue
	//APSCheckIsCurrentDispatchQueue( inAdvertiserInfo->queue );
	
	require_quiet( info1 && info2, bail );
	require_quiet( CFIsType( info1, APAdvertiserInfo ), bail );
	require_quiet( CFIsType( info2, APAdvertiserInfo ), bail );

	require_quiet( CFEqualNullSafe( info1->deviceID, info2->deviceID ), bail );
	require_quiet( CFEqualNullSafe( info1->deviceName, info2->deviceName ), bail );
	require_quiet( info1->features == info2->features, bail );
	require_quiet( CFEqualNullSafe( info1->firmwareVersion, info2->firmwareVersion ), bail );
	require_quiet( info1->flags == info2->flags, bail );
	require_quiet( CFEqualNullSafe( info1->model, info2->model ), bail );
	require_quiet( info1->password == info2->password, bail );
	require_quiet( info1->pinEnabled == info2->pinEnabled, bail );
	require_quiet( CFEqualNullSafe( info1->protocolVersion, info2->protocolVersion ), bail );
	require_quiet( CFEqualNullSafe( info1->publicHKPIdentity, info2->publicHKPIdentity ), bail );
	require_quiet( CFEqualNullSafe( info1->publicKey, info2->publicKey ), bail );
	require_quiet( info1->seed == info2->seed, bail );
	require_quiet( CFEqualNullSafe( info1->sourceVersion, info2->sourceVersion ), bail );
	
	equal = true;
		
bail:
	return( equal );
}

//===========================================================================================================================
//	_APAdvertiserInfoCopyDescription
//===========================================================================================================================

static void _APAdvertiserInfoCopyDescription( void *inContext )
{
	OSStatus									err				= kNoErr;
	APAdvertiserInfoCopyDescriptionParams *		params			= (APAdvertiserInfoCopyDescriptionParams *) inContext;
	APAdvertiserInfoRef							inInfo			= params->info;
	CFStringRef *								outDescription	= params->descriptionCopy;
	CFStringRef									description		= NULL;
	CFStringRef									tempDescription	= NULL;
	
	APSCheckIsCurrentDispatchQueue( inInfo->queue );
	
	tempDescription = CFStringCreateWithFormat( NULL, NULL,
		CFSTR(
			"<APAdvertiserInfo %p"
			"\n\tdeviceID=%@"
			"\n\tdeviceName=%@"
			"\n\tfeatures=0x%010llX"
			"\n\tfirmwareVersion=%@"
			"\n\tsystemFlags=0x%04X"
			"\n\tdeviceModel=%@"
			"\n\tpassword=%d"
			"\n\tPIN=%d"
			"\n\tprotocolVersion=%@"
			"\n\tpublicHomeKitPairingIdentity=%@"
			"\n\tpublicKey=%@" ),
		inInfo, inInfo->deviceID, inInfo->deviceName, inInfo->features, inInfo->firmwareVersion, inInfo->flags,
		inInfo->model, inInfo->password, inInfo->pinEnabled, inInfo->protocolVersion, inInfo->publicHKPIdentity,
		inInfo->publicKey );
	CFReleaseNullSafe( description );
	description = tempDescription;
	tempDescription = NULL;
	require_action( description, bail, err = kNoMemoryErr );
	tempDescription = CFStringCreateWithFormat( NULL, NULL,
		CFSTR( "%@\n\tsourceVersion=%@" ), description, inInfo->sourceVersion );
	CFReleaseNullSafe( description );
	description = tempDescription;
	tempDescription = NULL;
	require_action( description, bail, err = kNoMemoryErr );
	tempDescription = CFStringCreateWithFormat( NULL, NULL,
		CFSTR( "%@>" ), description );
	CFReleaseNullSafe( description );
	description = tempDescription;
	tempDescription = NULL;
	require_action( description, bail, err = kNoMemoryErr );

	*outDescription = description;
	description = NULL;
	
bail:
	CFReleaseNullSafe( description );
	params->err = err;
	return;
}

//===========================================================================================================================
//	_APAdvertiserInfoInternalCopy
//===========================================================================================================================

static OSStatus
	_APAdvertiserInfoCopyInternal(
		CFAllocatorRef inAllocator,
		APAdvertiserInfoRef inAdvertiserInfo,
		APAdvertiserInfoRef *outAdvertiserInfoCopy )
{
	OSStatus					err = kNoErr;
	APAdvertiserInfoRef			in = inAdvertiserInfo;
	APAdvertiserInfoRef			out = NULL;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiserInfo->queue );
	
	require_action( outAdvertiserInfoCopy, bail, err = kParamErr );
	
	if( !inAdvertiserInfo )
	{
		*outAdvertiserInfoCopy = NULL;
		goto bail;
	}
	
	err = APAdvertiserInfoCreate( inAllocator, &out );
	require_noerr( err, bail );
	
	// Device ID
	
	if( in->deviceID)
	{
		out->deviceID = CFStringCreateCopy( inAllocator, in->deviceID );
	}
	
	// Device Name
	
	if( in->deviceName)
	{
		out->deviceName = CFStringCreateCopy( inAllocator, in->deviceName );
	}
	
	// Features
	
	out->features = in->features;
	
	// Firmware Version
	
	if( in->firmwareVersion)
	{
		out->firmwareVersion = CFStringCreateCopy( inAllocator, in->firmwareVersion );
	}
	
	// Flags
	
	out->flags = in->flags;
	
	// Model
	
	if( in->model)
	{
		out->model = CFStringCreateCopy( inAllocator, in->model );
	}
	
	// Password
	
	out->password = in->password;
	
	// PIN
	
	out->pinEnabled = in->pinEnabled;
	
	// Protocol Version
	
	if( in->protocolVersion)
	{
		out->protocolVersion = CFStringCreateCopy( inAllocator, in->protocolVersion );
	}
	
	// Public HomeKit Pairing Identity
	
	if( in->publicHKPIdentity)
	{
		out->publicHKPIdentity = CFStringCreateCopy( inAllocator, in->publicHKPIdentity );
	}
	
	// Public Key
	
	if( in->publicKey)
	{
		out->publicKey = CFStringCreateCopy( inAllocator, in->publicKey );
	}
	
	// Seed
	
	out->seed = in->seed;
	
	// Source Version
	
	if( in->sourceVersion)
	{
		out->sourceVersion = CFStringCreateCopy( inAllocator, in->sourceVersion );
	}
	
	*outAdvertiserInfoCopy = out;
	
bail:
	return( err );
}

//===========================================================================================================================
//	_APAdvertiserInfoCopy
//===========================================================================================================================

static void _APAdvertiserInfoCopy( void *inContext )
{
	OSStatus						err			= kNoErr;
	APAdvertiserInfoCopyParams *	params		= (APAdvertiserInfoCopyParams *) inContext;
	CFAllocatorRef					allocator	= params->allocator;
	APAdvertiserInfoRef				info		= params->info;
	APAdvertiserInfoRef *			copiedInfo	= params->copiedInfo;
	
	APSCheckIsCurrentDispatchQueue( info->queue );
	
	err = _APAdvertiserInfoCopyInternal( allocator, info, copiedInfo );
	require_noerr( err, bail );
	
bail:
	params->error = err;
}


//===========================================================================================================================
//	_APAdvertiserInfoCopyPropertyInternal
//===========================================================================================================================

static OSStatus
	_APAdvertiserInfoCopyPropertyInternal(
		APAdvertiserInfoRef	inAdvertiserInfo,
		CFStringRef inProperty,
		CFAllocatorRef inAllocator,
		void *outValue )
{
	CFTypeRef		value = NULL;
	OSStatus		error = kNoErr;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiserInfo->queue );
	
	(void) inAllocator;
	
	require_action( outValue, bail, error = kParamErr );
	require_action( inProperty, bail, error = kParamErr );
	
	if( 0 ) {}
	
	// DeviceID
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_DeviceID ) )
	{
		value = inAdvertiserInfo->deviceID ? CFStringCreateCopy( inAllocator, inAdvertiserInfo->deviceID ) : NULL;
	}
	
	// Device Name
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_DeviceName ) )
	{
		value = inAdvertiserInfo->deviceName ?
			CFStringCreateCopy( inAllocator, inAdvertiserInfo->deviceName ) : NULL;
	}
	
	// Features
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_Features ) )
	{
		value = inAdvertiserInfo->features ?
			CFNumberCreate( inAllocator, kCFNumberSInt64Type, &inAdvertiserInfo->features ) : NULL;
	}
	
	// Firmware Version
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_FirmwareVersion ) )
	{
		value = inAdvertiserInfo->firmwareVersion ?
			CFStringCreateCopy( inAllocator, inAdvertiserInfo->firmwareVersion ) : NULL;
	}
	
	// Flags
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_SystemFlags ) )
	{
		value = CFNumberCreate( inAllocator, kCFNumberSInt32Type, &inAdvertiserInfo->flags );
	}
	
	// Model
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_DeviceModel ) )
	{
		value = inAdvertiserInfo->model ? CFStringCreateCopy( inAllocator, inAdvertiserInfo->model ) : NULL;
	}
	
	// Password
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_PasswordRequired ) )
	{
		value = CFRetain( inAdvertiserInfo->password ? kCFBooleanTrue : kCFBooleanFalse );
	}
	
	// PIN
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_PINRequired ) )
	{
		value = CFRetain( inAdvertiserInfo->pinEnabled ? kCFBooleanTrue : kCFBooleanFalse );
	}
	
	// Protocol version
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_ProtocolVersion ) )
	{
		value = inAdvertiserInfo->protocolVersion ?
			CFStringCreateCopy( inAllocator, inAdvertiserInfo->protocolVersion ) : NULL;
	}
	
	// Public Home Kit Pairing Identity
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_PublicHomeKitPairingIdentity ) )
	{
		value = inAdvertiserInfo->publicHKPIdentity ?
		CFStringCreateCopy( inAllocator, inAdvertiserInfo->publicHKPIdentity ) : NULL;
	}
	
	// Public key
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_PublicKey ) )
	{
		value = inAdvertiserInfo->publicKey ?
			CFStringCreateCopy( inAllocator, inAdvertiserInfo->publicKey ) : NULL;
	}
	
	// Source version
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_AirPlayVersion ) )
	{
		value = inAdvertiserInfo->sourceVersion ?
			CFStringCreateCopy( inAllocator, inAdvertiserInfo->sourceVersion ) : NULL;
	}
	
	// Unknown (TODO: need to handle arbitrary key-value pairs)
	
	else
	{
		error = kNotFoundErr;
		goto bail;
	}
	
	*( (CFTypeRef *) outValue ) = value;
	
bail:
	return( error );
}

//===========================================================================================================================
//	_APAdvertiserInfoCopyProperty
//===========================================================================================================================

static void		_APAdvertiserInfoCopyProperty( void *inContext )
{
	APAdvertiserInfoCopyPropertyParams * const	params = (APAdvertiserInfoCopyPropertyParams *) inContext;
	APAdvertiserInfoRef							inAdvertiserInfo = params->advertiserInfo;
	CFStringRef									inProperty = params->property;
	CFAllocatorRef								inAllocator = params->allocator;
	void *										outValue = params->value;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiserInfo->queue );
	
	params->error = _APAdvertiserInfoCopyPropertyInternal( inAdvertiserInfo, inProperty, inAllocator, outValue );
}

//===========================================================================================================================
//	_APAdvertiserInfoReplaceStringProperty
//===========================================================================================================================

static OSStatus
	_APAdvertiserInfoReplaceStringProperty(
		CFAllocatorRef inAllocator,
		CFStringRef inValue,
		CFStringRef *inDestination )
{
	OSStatus		err			= kNoErr;
	CFStringRef		stringCopy	= NULL;
	
	require_action( inDestination, bail, err = kParamErr );
	
	if( !inValue )
	{
		ForgetCF( inDestination );
		goto bail;
	}
	
	require_action( CFIsType( inValue, CFString ), bail, err = kParamErr );
	require_action( CFStringGetLength( inValue ), bail, err = kParamErr );
	
	if( !CFEqualNullSafe( inValue, *inDestination ) )
	{
		stringCopy = CFStringCreateCopy( inAllocator, (CFStringRef) inValue );
		require_action( stringCopy, bail, err = kNoMemoryErr );
		
		CFReleaseNullSafe( *inDestination );
		*inDestination = stringCopy;
		
		stringCopy = NULL;
	}
	
bail:
	CFReleaseNullSafe( stringCopy );
	return( err );
}

//===========================================================================================================================
//	_APAdvertiserInfoSetProperty
//===========================================================================================================================

static void _APAdvertiserInfoSetProperty( void *inContext )
{
	OSStatus									err = kNoErr;
	APAdvertiserInfoSetPropertyParams * const	params = (APAdvertiserInfoSetPropertyParams *) inContext;
	APAdvertiserInfoRef							inAdvertiserInfo = params->advertiserInfo;
	CFStringRef									inProperty = params->property;
	CFTypeRef									inValue = params->value;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiserInfo->queue );
	
	require_action( inProperty, bail, err = kParamErr );
	
	if( 0 ) {}
	
	// DeviceID
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_DeviceID ) )
	{
		err = _APAdvertiserInfoReplaceStringProperty( NULL, (CFStringRef) inValue,
			&inAdvertiserInfo->deviceID );
		require_noerr( err, bail );
	}
	
	// Device Name
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_DeviceName ) )
	{
		err = _APAdvertiserInfoReplaceStringProperty( NULL, (CFStringRef) inValue,
			&inAdvertiserInfo->deviceName );
		require_noerr( err, bail );
	}
	
	// Features
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_Features ) )
	{
		inAdvertiserInfo->features = CFGetInt64( inValue, &err );
		require_noerr( err, bail );
	}
	
	// Firmware Version
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_FirmwareVersion ) )
	{
		err = _APAdvertiserInfoReplaceStringProperty( NULL, (CFStringRef) inValue,
			&inAdvertiserInfo->firmwareVersion );
		require_noerr( err, bail );
	}
	
	// Flags
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_SystemFlags ) )
	{
		inAdvertiserInfo->flags = (uint32_t) CFGetInt64( inValue, &err );
		require_noerr( err, bail );
	}
	
	// Model
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_DeviceModel ) )
	{
		err = _APAdvertiserInfoReplaceStringProperty( NULL, (CFStringRef) inValue,
			&inAdvertiserInfo->model );
		require_noerr( err, bail );
	}
	
	// Password
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_PasswordRequired ) )
	{
		inAdvertiserInfo->password = CFGetBoolean( inValue, &err );
		require_noerr( err, bail );
	}
	
	// PIN
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_PINRequired ) )
	{
		inAdvertiserInfo->pinEnabled = CFGetBoolean( inValue, &err );
		require_noerr( err, bail );

	}
	
	// Protocol version
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_ProtocolVersion ) )
	{
		err = _APAdvertiserInfoReplaceStringProperty( NULL, (CFStringRef) inValue,
			&inAdvertiserInfo->protocolVersion );
		require_noerr( err, bail );
	}
	
	// Public HomeKit pairing identity
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_PublicHomeKitPairingIdentity ) )
	{
		err = _APAdvertiserInfoReplaceStringProperty( NULL, (CFStringRef) inValue,
			&inAdvertiserInfo->publicHKPIdentity );
		require_noerr( err, bail );
	}
	
	// Public key
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_PublicKey ) )
	{
		err = _APAdvertiserInfoReplaceStringProperty( NULL, (CFStringRef) inValue,
			&inAdvertiserInfo->publicKey );
		require_noerr( err, bail );
	}
	
	// Source version
	
	else if( CFEqual( inProperty, kAPAdvertiserInfoProperty_AirPlayVersion ) )
	{
		err = _APAdvertiserInfoReplaceStringProperty( NULL, (CFStringRef) inValue,
			&inAdvertiserInfo->sourceVersion );
		require_noerr( err, bail );
	}
	
	// Unknown (TODO: need to handle arbitrary key-value pairs)
	
	else
	{
		err = kNotFoundErr;
	}

bail:
	params->error = err;
}

//===========================================================================================================================
//	_APAdvertiserInfoAddStringToTXTRecord
//===========================================================================================================================

static OSStatus
	_APAdvertiserInfoAddStringToTXTRecord(
		CFStringRef inString,
		TXTRecordRef *inTXTRecord,
		const char *inTXTRecordKey )
{
	OSStatus	err = kNoErr;
	Boolean		success;
	char		cstr[ 256 ];
	
	require_action( inString, bail, err = kParamErr );
	require_action( CFStringGetLength( inString ), bail, err = kParamErr );
	
	*cstr = '\0';
	success = CFStringGetCString( inString, cstr, (CFIndex) sizeof( cstr ), kCFStringEncodingUTF8 );
	require_action( success, bail, err = kValueErr );
	err = TXTRecordSetValue( inTXTRecord, inTXTRecordKey, (uint8_t) strlen( cstr ), cstr );
	require_noerr( err, bail );

bail:
	return( err );
}

//===========================================================================================================================
//	_APAdvertiserInfoCopyAirPlayData
//===========================================================================================================================

static void _APAdvertiserInfoCopyAirPlayData( void *inContext )
{
	OSStatus			err = kNoErr;
	APAdvertiserInfoCopyDataParams *	params = (APAdvertiserInfoCopyDataParams *) inContext;
	APAdvertiserInfoRef					inAdvertiserInfo = params->info;
	CFDataRef *							outData = params->outData;
	TXTRecordRef						txtRec;
	uint8_t								txtBuf[ 256 ];
	const uint8_t *						txtPtr = NULL;
	uint16_t							txtLen = 0;
	char								cstr[ 256 ];
	int									n = 0;
	uint32_t							u32 = 0;
	APFeatures							features = 0;
	CFDataRef							data = NULL;
	
	APSCheckIsCurrentDispatchQueue( inAdvertiserInfo->queue );
	
	TXTRecordCreate( &txtRec, (uint16_t) sizeof( txtBuf ), txtBuf );

	require_action( inAdvertiserInfo, bail, err = kParamErr );
	require_action( outData, bail, err = kParamErr );
	
	// DeviceID (required)
	
	err = _APAdvertiserInfoAddStringToTXTRecord( inAdvertiserInfo->deviceID, &txtRec, kAirPlayTXTKey_DeviceID );
	require_noerr_action( err, bail,
	{
		apai_ulog( kLogLevelInfo, "Failed to add DeviceID, which is required. Can't create AirPlay data.\n" );
	} );
		
	// Features (optional)

	if( inAdvertiserInfo->features )
	{
		features = inAdvertiserInfo->features;
		u32 = (uint32_t)( ( features >> 32 ) & UINT32_C( 0xFFFFFFFF ) );
		if( u32 != 0 )
		{
			n = snprintf( cstr, sizeof( cstr ), "0x%X,0x%X", (uint32_t)( features & UINT32_C( 0xFFFFFFFF ) ), u32 );
		}
		else
		{
			n = snprintf( cstr, sizeof( cstr ), "0x%X", (uint32_t)( features & UINT32_C( 0xFFFFFFFF ) ) );
		}
		err = TXTRecordSetValue( &txtRec, kAirPlayTXTKey_Features, (uint8_t) n, cstr );
		require_noerr( err, bail );
	}
	
	// FirmwareRevision (optional)
	
	if( inAdvertiserInfo->firmwareVersion && CFStringGetLength( inAdvertiserInfo->firmwareVersion ) )
	{
		err = _APAdvertiserInfoAddStringToTXTRecord( inAdvertiserInfo->firmwareVersion, &txtRec,
			kAirPlayTXTKey_FirmwareVersion );
		require_noerr( err, bail );
	}
	
	// Flags (optional)
	
	u32 = inAdvertiserInfo->flags;
	if( u32 != 0 )
	{
		n = snprintf( cstr, sizeof( cstr ), "0x%x", u32 );
		err = TXTRecordSetValue( &txtRec, kAirPlayTXTKey_Flags, (uint8_t) n, cstr );
		require_noerr( err, bail );
	}
	
	// Model (optional)
	
	if( inAdvertiserInfo->model && CFStringGetLength( inAdvertiserInfo->model ) )
	{
		err = _APAdvertiserInfoAddStringToTXTRecord( inAdvertiserInfo->model, &txtRec, kAirPlayTXTKey_Model );
		require_noerr( err, bail );
	}
	
	// Protocol version (optional)
	
	if( inAdvertiserInfo->protocolVersion && CFStringGetLength( inAdvertiserInfo->protocolVersion ) )
	{
		err = _APAdvertiserInfoAddStringToTXTRecord( inAdvertiserInfo->protocolVersion, &txtRec,
			kAirPlayTXTKey_ProtocolVersion );
		require_noerr( err, bail );
	}
	
	// Public HomeKit pairing identity (optional)
	
	if( inAdvertiserInfo->publicHKPIdentity && CFStringGetLength( inAdvertiserInfo->publicHKPIdentity ) )
	{
		err = _APAdvertiserInfoAddStringToTXTRecord( inAdvertiserInfo->publicHKPIdentity, &txtRec,
			kAirPlayTXTKey_PublicHKID );
		require_noerr( err, bail );
	}
	
	// Public key (optional)
	
	if( inAdvertiserInfo->publicKey && CFStringGetLength( inAdvertiserInfo->publicKey ) )
	{
		err = _APAdvertiserInfoAddStringToTXTRecord( inAdvertiserInfo->publicKey, &txtRec, kAirPlayTXTKey_PublicKey );
		require_noerr( err, bail );
	}
	
	// Source Version (optional)
	
	if( inAdvertiserInfo->sourceVersion && CFStringGetLength( inAdvertiserInfo->sourceVersion ) )
	{
		err = _APAdvertiserInfoAddStringToTXTRecord( inAdvertiserInfo->sourceVersion, &txtRec,
			kAirPlayTXTKey_SourceVersion );
		require_noerr( err, bail );
	}
	
	txtPtr = TXTRecordGetBytesPtr( &txtRec );
	txtLen = TXTRecordGetLength( &txtRec );
	data = CFDataCreate( NULL, txtPtr, txtLen );
	require_action( data, bail, err = kNoMemoryErr );
	
	*outData = data;
	
bail:
	TXTRecordDeallocate( &txtRec );
	params->error = err;
}

//===========================================================================================================================
//	_APAdvertiserInfoCreateAirPlayServiceName
//===========================================================================================================================

static void _APAdvertiserInfoCreateAirPlayServiceName( void *inContext )
{
	APAdvertiserInfoCopyNameParams *	params = (APAdvertiserInfoCopyNameParams *) inContext;
	APAdvertiserInfoRef					info = params->info;
	CFStringRef	*						outName = params->outName;
	
	APSCheckIsCurrentDispatchQueue( info->queue );
	
	params->error = _APAdvertiserInfoCopyPropertyInternal( info, kAPAdvertiserInfoProperty_DeviceName,
		NULL, outName );
}

#if 0
#pragma mark - Debug
#endif

//===========================================================================================================================
//	_APAdvertiserInfoDebugShow
//===========================================================================================================================

typedef struct
{
	APAdvertiserInfoRef		info;
	int						verbose;
	DataBuffer *			dataBuffer;
	OSStatus				error;
	
}	APAdvertiserInfoDebugShowParams;

static void _APAdvertiserInfoDebugShow( void *inContext )
{
	APAdvertiserInfoDebugShowParams *	params = (APAdvertiserInfoDebugShowParams *) inContext;
	APAdvertiserInfoRef					inInfo = params->info;
	DataBuffer *						inDataBuf = params->dataBuffer;
	
	APSCheckIsCurrentDispatchQueue( inInfo->queue );
	
	DataBuffer_AppendF( inDataBuf, "Advertiser Info:\n" );
	DataBuffer_AppendF( inDataBuf, "\t%20@: %@\n", kAPAdvertiserInfoProperty_DeviceID, inInfo->deviceID );
	DataBuffer_AppendF( inDataBuf, "\t%20@: %@\n", kAPAdvertiserInfoProperty_DeviceName, inInfo->deviceName );
	DataBuffer_AppendF( inDataBuf, "\t%20@: %d\n", kAPAdvertiserInfoProperty_Features, (int) inInfo->features );
	DataBuffer_AppendF( inDataBuf, "\t%20@: %@\n", kAPAdvertiserInfoProperty_DeviceModel, inInfo->model );
	DataBuffer_AppendF( inDataBuf, "\t%20@: %@\n", kAPAdvertiserInfoProperty_AirPlayVersion, inInfo->sourceVersion );
	DataBuffer_AppendF( inDataBuf, "\t%20@: %@\n", kAPAdvertiserInfoProperty_FirmwareVersion, inInfo->firmwareVersion );
	DataBuffer_AppendF( inDataBuf, "\t%20@: %d\n", kAPAdvertiserInfoProperty_SystemFlags, (long) inInfo->flags );
	DataBuffer_AppendF( inDataBuf, "\t%20@: %d\n", kAPAdvertiserInfoProperty_PasswordRequired, (int) inInfo->password );
	DataBuffer_AppendF( inDataBuf, "\t%20@: %d\n", kAPAdvertiserInfoProperty_PINRequired, (int) inInfo->pinEnabled );
	DataBuffer_AppendF( inDataBuf, "\t%20@: %@\n", kAPAdvertiserInfoProperty_ProtocolVersion, inInfo->protocolVersion );
	DataBuffer_AppendF( inDataBuf, "\t%20@: %@\n", kAPAdvertiserInfoProperty_PublicHomeKitPairingIdentity, inInfo->publicHKPIdentity );
	DataBuffer_AppendF( inDataBuf, "\t%20@: %@\n", kAPAdvertiserInfoProperty_PublicKey, inInfo->publicKey );
}

//===========================================================================================================================
//	APAdvertiserInfoDebugShow
//===========================================================================================================================

OSStatus APAdvertiserInfoDebugShow( APAdvertiserInfoRef inInfo, int inVerbose, DataBuffer *inDataBuf )
{
	APAdvertiserInfoDebugShowParams		params = { inInfo, inVerbose, inDataBuf, kNoErr };
	
	dispatch_sync_f( inInfo->queue, &params, _APAdvertiserInfoDebugShow );
	
	return( params.error );
}
