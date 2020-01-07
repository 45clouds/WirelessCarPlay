/*
	File:    	ScreenUtils.c
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
	
	Copyright (C) 2013-2015 Apple Inc. All Rights Reserved.
*/

#include "ScreenUtils.h"

#include "CFUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "StringUtils.h"
#include "ThreadUtils.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

//===========================================================================================================================
//	Screen
//===========================================================================================================================

#if( !defined( SCREEN_DEFAULT_FEATURES ) )
	#define SCREEN_DEFAULT_FEATURES		( kScreenFeature_Knobs | kScreenFeature_LowFidelityTouch | kScreenFeature_HighFidelityTouch )
#endif
#if( !defined( SCREEN_DEFAULT_MAX_FPS ) )
	#define SCREEN_DEFAULT_MAX_FPS		60
#endif
#if( !defined( SCREEN_DEFAULT_WIDTH ) )
	#define SCREEN_DEFAULT_WIDTH		960
#endif
#if( !defined( SCREEN_DEFAULT_HEIGHT ) )
	#define SCREEN_DEFAULT_HEIGHT		540
#endif
#if( !defined( SCREEN_DEFAULT_UUID ) )
	#define SCREEN_DEFAULT_UUID			"e5f7a68d-7b0f-4305-984b-974f677a150b"
#endif

struct ScreenPrivate
{
	CFRuntimeBase		base;				// CF type info. Must be first.
	CFDataRef			edid;				// EDID describing the screen.
	uint32_t			features;			// Features of the screen. See kScreenFeature_*.
	uint32_t			maxFPS;				// Max frames per second the screen can handle.
	CFTypeRef			platformLayer;		// Platform-specific rendering layer for overrides.
	uint32_t			pixelWidth;			// Width in pixels.
	uint32_t			pixelHeight;		// Height in pixels.
	uint32_t			physicalWidth;		// Width in millimeters.
	uint32_t			physicalHeight;		// Height in millimeters.
	uint32_t			primaryInputDevice;	// Primary input device for the screen.
	CFStringRef			uuid;				// Unique ID.
};

static void	_ScreenGetTypeID( void *inContext );
static void	_ScreenFinalize( CFTypeRef inCF );

static dispatch_once_t			gScreenInitOnce = 0;
static CFTypeID					gScreenTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kScreenClass = 
{
	0,					// version
	"Screen",			// className
	NULL,				// init
	NULL,				// copy
	_ScreenFinalize,	// finalize
	NULL,				// equal -- NULL means pointer equality.
	NULL,				// hash  -- NULL means pointer hash.
	NULL,				// copyFormattingDesc
	NULL,				// copyDebugDesc
	NULL,				// reclaim
	NULL				// refcount
};

static pthread_mutex_t			gScreenLock  = PTHREAD_MUTEX_INITIALIZER;
static CFMutableArrayRef		gScreenArray = NULL;

ulog_define( Screen, kLogLevelVerbose, kLogFlags_Default, "Screen", NULL );
#define screen_dlog( LEVEL, ... )		dlogc( &log_category_from_name( Screen ), (LEVEL), __VA_ARGS__ )
#define screen_ulog( LEVEL, ... )		ulog( &log_category_from_name( Screen ), (LEVEL), __VA_ARGS__ )

//===========================================================================================================================
//	ScreenGetTypeID
//===========================================================================================================================

CFTypeID	ScreenGetTypeID( void )
{
	dispatch_once_f( &gScreenInitOnce, NULL, _ScreenGetTypeID );
	return( gScreenTypeID );
}

static void _ScreenGetTypeID( void *inContext )
{
	(void) inContext;
	
	gScreenTypeID = _CFRuntimeRegisterClass( &kScreenClass );
	check( gScreenTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	ScreenCreate
//===========================================================================================================================

OSStatus	ScreenCreate( ScreenRef *outScreen, CFDictionaryRef inProperties )
{
	OSStatus		err;
	ScreenRef		me;
	size_t			extraLen;
	uint32_t		u32;
	char			cstr[ 128 ];
	uint8_t			uuid[ 16 ];
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (ScreenRef) _CFRuntimeCreateInstance( NULL, ScreenGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	ScreenSetPropertyInt64( me, kScreenProperty_Features, NULL, SCREEN_DEFAULT_FEATURES );
	ScreenSetPropertyInt64( me, kScreenProperty_MaxFPS, NULL, SCREEN_DEFAULT_MAX_FPS );
	ScreenSetPropertyInt64( me, kScreenProperty_WidthPixels, NULL, SCREEN_DEFAULT_WIDTH );
	ScreenSetPropertyInt64( me, kScreenProperty_HeightPixels, NULL, SCREEN_DEFAULT_HEIGHT );
	ScreenSetProperty( me, kScreenProperty_UUID, NULL, CFSTR( SCREEN_DEFAULT_UUID ) );
	
	if( inProperties )
	{
		me->edid = CFDictionaryGetCFData( inProperties, kScreenProperty_EDID, NULL );
		CFRetainNullSafe( me->edid );
		
		u32 = (uint32_t) CFDictionaryGetInt64( inProperties, kScreenProperty_Features, &err );
		if( !err ) me->features = u32;
		
		u32 = (uint32_t) CFDictionaryGetInt64( inProperties, kScreenProperty_MaxFPS, &err );
		if( !err ) me->maxFPS = u32;
		
		u32 = (uint32_t) CFDictionaryGetInt64( inProperties, kScreenProperty_PrimaryInputDevice, &err );
		if( !err ) me->primaryInputDevice = u32;
		
		u32 = (uint32_t) CFDictionaryGetInt64( inProperties, kScreenProperty_WidthPixels, &err );
		if( !err ) me->pixelWidth = u32;
		
		u32 = (uint32_t) CFDictionaryGetInt64( inProperties, kScreenProperty_HeightPixels, &err );
		if( !err ) me->pixelHeight = u32;
		
		u32 = (uint32_t) CFDictionaryGetInt64( inProperties, kScreenProperty_WidthPhysical, &err );
		if( !err ) me->physicalWidth = u32;
		
		u32 = (uint32_t) CFDictionaryGetInt64( inProperties, kScreenProperty_HeightPhysical, &err );
		if( !err ) me->physicalHeight = u32;
		
		err = CFDictionaryGetUUID( inProperties, kScreenProperty_UUID, uuid );
		if( !err )
		{
			UUIDtoCString( uuid, false, cstr );
			me->uuid = CFStringCreateWithCString( NULL, cstr, kCFStringEncodingUTF8 );
			require_action( me->uuid, exit, err = kUnknownErr );
		}
	}
	
	*outScreen = me;
	me = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( me );
	return( err );
}

//===========================================================================================================================
//	_ScreenFinalize
//===========================================================================================================================

static void	_ScreenFinalize( CFTypeRef inCF )
{
	ScreenRef const		me = (ScreenRef) inCF;
	
	ForgetCF( &me->edid );
	ForgetCF( &me->platformLayer );
	ForgetCF( &me->uuid );
}

//===========================================================================================================================
//	_ScreenCopyProperty
//===========================================================================================================================

CFTypeRef
	_ScreenCopyProperty( 
		CFTypeRef		inObject, 
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		OSStatus *		outErr )
{
	ScreenRef const		me = (ScreenRef) inObject;
	CFTypeRef			value = NULL;
	OSStatus			err;
	
	(void) inFlags;
	(void) inQualifier;
	
	if( 0 ) {}
	
	// EDID
	
	else if( CFEqual( inProperty, kScreenProperty_EDID ) )
	{
		value = me->edid;
		require_action_quiet( value, exit, err = kNotHandledErr );
		CFRetain( value );
	}
	
	// Features
	
	else if( CFEqual( inProperty, kScreenProperty_Features ) )
	{
		value = CFNumberCreate( NULL, kCFNumberSInt32Type, &me->features );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// MaxFPS
	
	else if( CFEqual( inProperty, kScreenProperty_MaxFPS ) )
	{
		value = CFNumberCreate( NULL, kCFNumberSInt32Type, &me->maxFPS );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// PlatformLayer
	
	else if( me->platformLayer && CFEqual( inProperty, kScreenProperty_PlatformLayer ) )
	{
		value = me->platformLayer;
		CFRetain( value );
	}
	
	// Physical dimensions
	
	else if( CFEqual( inProperty, kScreenProperty_WidthPhysical ) )
	{
		value = CFNumberCreate( NULL, kCFNumberSInt32Type, &me->physicalWidth );
		require_action( value, exit, err = kNoMemoryErr );
	}
	else if( CFEqual( inProperty, kScreenProperty_HeightPhysical ) )
	{
		value = CFNumberCreate( NULL, kCFNumberSInt32Type, &me->physicalHeight );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// Pixel dimensions
	
	else if( CFEqual( inProperty, kScreenProperty_WidthPixels ) )
	{
		value = CFNumberCreate( NULL, kCFNumberSInt32Type, &me->pixelWidth );
		require_action( value, exit, err = kNoMemoryErr );
	}
	else if( CFEqual( inProperty, kScreenProperty_HeightPixels ) )
	{
		value = CFNumberCreate( NULL, kCFNumberSInt32Type, &me->pixelHeight );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// PrimaryInputDevice
	
	else if( CFEqual( inProperty, kScreenProperty_PrimaryInputDevice ) )
	{
		value = CFNumberCreate( NULL, kCFNumberSInt32Type, &me->primaryInputDevice );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// UUID
	
	else if( CFEqual( inProperty, kScreenProperty_UUID ) )
	{
		value = me->uuid;
		require_action_quiet( value, exit, err = kNotPreparedErr );
		CFRetain( value );
	}
	
	// Unknown...
	
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
//	_ScreenSetProperty
//===========================================================================================================================

OSStatus
	_ScreenSetProperty( 
		CFTypeRef		inObject, 
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		CFTypeRef		inValue )
{
	ScreenRef const		me = (ScreenRef) inObject;
	OSStatus			err;
	
	(void) inFlags;
	(void) inQualifier;
	
	if( 0 ) {}
	
	// EDID
	
	else if( CFEqual( inProperty, kScreenProperty_EDID ) )
	{
		require_action( !inValue || CFIsType( inValue, CFData ), exit, err = kTypeErr );
		ReplaceCF( &me->edid, inValue );
	}
	
	// Features
	
	else if( CFEqual( inProperty, kScreenProperty_Features ) )
	{
		me->features = (uint32_t) CFGetInt64( inValue, NULL );
	}
	
	// MaxFPS
	
	else if( CFEqual( inProperty, kScreenProperty_MaxFPS ) )
	{
		me->maxFPS = (uint32_t) CFGetInt64( inValue, NULL );
	}
	
	// PlatformLayer
	
	else if( CFEqual( inProperty, kScreenProperty_PlatformLayer ) )
	{
		ReplaceCF( &me->platformLayer, inValue );
	}
	
	// Physical dimensions
	
	else if( CFEqual( inProperty, kScreenProperty_WidthPhysical ) )
	{
		me->physicalWidth = (uint32_t) CFGetInt64( inValue, NULL );
	}
	else if( CFEqual( inProperty, kScreenProperty_HeightPhysical ) )
	{
		me->physicalHeight = (uint32_t) CFGetInt64( inValue, NULL );
	}
	
	// Pixel dimensions
	
	else if( CFEqual( inProperty, kScreenProperty_WidthPixels ) )
	{
		me->pixelWidth = (uint32_t) CFGetInt64( inValue, NULL );
	}
	else if( CFEqual( inProperty, kScreenProperty_HeightPixels ) )
	{
		me->pixelHeight = (uint32_t) CFGetInt64( inValue, NULL );
	}
	
	// PrimaryInputDevice
	
	else if( CFEqual( inProperty, kScreenProperty_PrimaryInputDevice ) )
	{
		me->primaryInputDevice = (uint32_t) CFGetInt64( inValue, NULL );
	}
	
	// UUID
	
	else if( CFEqual( inProperty, kScreenProperty_UUID ) )
	{
		require_action( !inValue || CFIsType( inValue, CFString ), exit, err = kTypeErr );
		ReplaceCF( &me->uuid, inValue );
	}
	
	// Unknown
	
	else
	{
		err = kNotHandledErr;
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
//	ScreenCopyMain
//===========================================================================================================================

ScreenRef	ScreenCopyMain( OSStatus *outErr )
{
	ScreenRef		screen = NULL;
	OSStatus		err;
	
	pthread_mutex_lock( &gScreenLock );
	if( gScreenArray && ( CFArrayGetCount( gScreenArray ) > 0 ) )
	{
		screen = (ScreenRef) CFArrayGetValueAtIndex( gScreenArray, 0 );
		CFRetain( screen );
	}
	
	// If there are no registered screens, create a default one.
	
	if( !screen )
	{
		if( !gScreenArray )
		{
			gScreenArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			require_action( gScreenArray, exit, err = kNoMemoryErr );
		}
		
		err = ScreenCreate( &screen, NULL );
		require_noerr( err, exit );
		
		CFArrayAppendValue( gScreenArray, screen );
	}
	err = kNoErr;
	
exit:
	pthread_mutex_unlock( &gScreenLock );
	if( outErr ) *outErr = err;
	return( screen );
}
	
//===========================================================================================================================
//	ScreenRegister
//===========================================================================================================================

OSStatus	ScreenRegister( ScreenRef inScreen )
{
	OSStatus		err;
	
	pthread_mutex_lock( &gScreenLock );
	
	if( !gScreenArray )
	{
		gScreenArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		require_action( gScreenArray, exit, err = kNoMemoryErr );
	}
	CFArrayAppendValue( gScreenArray, inScreen );
	screen_ulog( kLogLevelNotice, "Registered screen %@ %u x %u, %u FPS\n", 
		inScreen->uuid, inScreen->pixelWidth, inScreen->pixelHeight, inScreen->maxFPS );
	err = kNoErr;
	
exit:
	pthread_mutex_unlock( &gScreenLock );
	return( err );
}

//===========================================================================================================================
//	ScreenDeregister
//===========================================================================================================================

OSStatus	ScreenDeregister( ScreenRef inScreen )
{
	CFIndex			i, n;
	ScreenRef		screen;
	
	pthread_mutex_lock( &gScreenLock );
	
	n = gScreenArray ? CFArrayGetCount( gScreenArray ) : 0;
	for( i = n - 1; i >= 0; --i )
	{
		screen = (ScreenRef) CFArrayGetValueAtIndex( gScreenArray, i );
		if( screen == inScreen )
		{
			screen_ulog( kLogLevelNotice, "Deregistered screen %@ %u x %u\n", 
				inScreen->uuid, inScreen->pixelWidth, inScreen->pixelHeight );
			CFArrayRemoveValueAtIndex( gScreenArray, i );
			--n;
		}
	}
	if( n == 0 ) ForgetCF( &gScreenArray );
	
	pthread_mutex_unlock( &gScreenLock );
	return( kNoErr );
}
