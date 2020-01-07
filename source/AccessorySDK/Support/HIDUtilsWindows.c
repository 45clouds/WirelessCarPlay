/*
	File:    	HIDUtilsWindows.c
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

#include "HIDUtils.h"

#include "CFUtils.h"
#include "CommonServices.h"
#include "StringUtils.h"
#include "ThreadUtils.h"
#include "UUIDUtils.h"

#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

//===========================================================================================================================
//	Internals
//===========================================================================================================================

static pthread_mutex_t			gVirtualHIDLock		= PTHREAD_MUTEX_INITIALIZER;
static CFMutableArrayRef		gVirtualHIDDevices	= NULL;

ulog_define( HIDUtils, kLogLevelVerbose, kLogFlags_Default, "HID", NULL );
#define hid_dlog( LEVEL, ... )		dlogc( &log_category_from_name( HIDUtils ), (LEVEL), __VA_ARGS__ )
#define hid_ulog( LEVEL, ... )		ulog( &log_category_from_name( HIDUtils ), (LEVEL), __VA_ARGS__ )

#if 0
#pragma mark == HIDBrowser ==
#endif

//===========================================================================================================================
//	HIDBrowser
//===========================================================================================================================

struct HIDBrowserPrivate
{
	CFRuntimeBase					base;			// CF type info. Must be first.
	dispatch_queue_t				queue;			// Queue to run all operations and deliver callbacks on.
	dispatch_source_t				source;			// Source to call us when events are pending.
	CFMutableArrayRef				devices;		// Device objects we've detected.
	pthread_mutex_t					devicesLock;	// Lock for modifications to "devices".
	pthread_mutex_t *				devicesLockPtr;	// Ptr for devicesLock for NULL testing.
	HIDBrowserEventHandler_f		eventHandler;	// Function to call when an event occurs.
	void *							eventContext;	// Context to pass to function when an event occurs.
};

static void	_HIDBrowserGetTypeID( void *inContext );
static void	_HIDBrowserFinalize( CFTypeRef inCF );

static dispatch_once_t			gHIDBrowserInitOnce = 0;
static CFTypeID					gHIDBrowserTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kHIDBrowserClass = 
{
	0,						// version
	"HIDBrowser",			// className
	NULL,					// init
	NULL,					// copy
	_HIDBrowserFinalize,	// finalize
	NULL,					// equal -- NULL means pointer equality.
	NULL,					// hash  -- NULL means pointer hash.
	NULL,					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

//===========================================================================================================================
//	HIDDevice
//===========================================================================================================================

struct HIDDevicePrivate
{
	CFRuntimeBase				base;				// CF type info. Must be first.
	dispatch_queue_t			queue;				// Queue to run all operations and deliver callbacks on.
	CFNumberRef					countryCode;		// Country code.
	CFStringRef					displayUUID;		// Display this HID device is associated with (or NULL if none).
	CFStringRef					name;				// Name of the device (e.g. "ShuttleXpress").
	CFNumberRef					productID;			// Product ID of the device.
	CFDataRef					reportDescriptor;	// HID descriptor to describe reports for this device.
	CFNumberRef					sampleRate;			// Sample rate of the device.
	uint8_t						uuid[ 16 ];			// UUID for device. May not be persistent.
	CFNumberRef					vendorID;			// Vendor ID of the device.
	Boolean						started;			// True if the device has been started.
	HIDDeviceEventHandler_f		eventHandler;		// Function to call when a event is received from the device.
	void *						eventContext;		// Context to pass to event handler.
};

static void 		_HIDDeviceGetTypeID( void *inContext );
static void			_HIDDeviceFinalize( CFTypeRef inCF );

static dispatch_once_t			gHIDDeviceInitOnce = 0;
static CFTypeID					gHIDDeviceTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kHIDDeviceClass = 
{
	0,						// version
	"HIDDevice",			// className
	NULL,					// init
	NULL,					// copy
	_HIDDeviceFinalize,		// finalize
	NULL,					// equal -- NULL means pointer equality.
	NULL,					// hash  -- NULL means pointer hash.
	NULL,					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

//===========================================================================================================================
//	HIDBrowserGetTypeID
//===========================================================================================================================

CFTypeID	HIDBrowserGetTypeID( void )
{
	dispatch_once_f( &gHIDBrowserInitOnce, NULL, _HIDBrowserGetTypeID );
	return( gHIDBrowserTypeID );
}

static void _HIDBrowserGetTypeID( void *inContext )
{
	(void) inContext;
	
	gHIDBrowserTypeID = _CFRuntimeRegisterClass( &kHIDBrowserClass );
	check( gHIDBrowserTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	HIDBrowserCreate
//===========================================================================================================================

OSStatus	HIDBrowserCreate( HIDBrowserRef *outBrowser )
{
	OSStatus			err;
	HIDBrowserRef		me;
	size_t				extraLen;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (HIDBrowserRef) _CFRuntimeCreateInstance( NULL, HIDBrowserGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->queue = dispatch_get_main_queue();
	arc_safe_dispatch_retain( me->queue );
	
	err = pthread_mutex_init( &me->devicesLock, NULL );
	require_noerr( err, exit );
	me->devicesLockPtr = &me->devicesLock;
	
	*outBrowser = me;
	me = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( me );
	return( err );
}

//===========================================================================================================================
//	_HIDBrowserFinalize
//===========================================================================================================================

static void	_HIDBrowserFinalize( CFTypeRef inCF )
{
	HIDBrowserRef const		me = (HIDBrowserRef) inCF;
	
	check( !me->source );
	check( !me->devices );
	pthread_mutex_forget( &me->devicesLockPtr );
	dispatch_forget( &me->queue );
}

//===========================================================================================================================
//	HIDBrowserCopyProperty
//===========================================================================================================================

CFTypeRef	HIDBrowserCopyProperty( HIDBrowserRef inBrowser, CFStringRef inProperty, CFTypeRef inQualifier, OSStatus *outErr )
{
	OSStatus			err;
	CFTypeRef			value = NULL;
	
	(void) inQualifier;
	
	if( 0 ) {}
	
	// Devices
	
	else if( CFEqual( inProperty, kHIDBrowserProperty_Devices ) )
	{
		pthread_mutex_lock( inBrowser->devicesLockPtr );
		if( inBrowser->devices )
		{
			value = CFArrayCreateCopy( NULL, inBrowser->devices );
			pthread_mutex_unlock( inBrowser->devicesLockPtr );
			require_action( value, exit, err = kNoMemoryErr );
		}
		else
		{
			pthread_mutex_unlock( inBrowser->devicesLockPtr );
		}
		if( !value )
		{
			value = CFArrayCreate( NULL, NULL, 0, &kCFTypeArrayCallBacks );
			require_action( value, exit, err = kNoMemoryErr );
		}
	}
	
	// Unknown...
	
	else
	{
		err = kNotFoundErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	HIDBrowserSetProperty
//===========================================================================================================================

OSStatus	HIDBrowserSetProperty( HIDBrowserRef inBrowser, CFStringRef inProperty, CFTypeRef inQualifier, CFTypeRef inValue )
{
	OSStatus		err;

	(void) inBrowser;
	(void) inProperty;
	(void) inValue;
	(void) inQualifier;
	
	if( 0 ) {}
		
	// Unknown...
	
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HIDBrowserSetDispatchQueue
//===========================================================================================================================

void	HIDBrowserSetDispatchQueue( HIDBrowserRef inBrowser, dispatch_queue_t inQueue )
{
	ReplaceDispatchQueue( &inBrowser->queue, inQueue );
}

//===========================================================================================================================
//	HIDBrowserSetEventHandler
//===========================================================================================================================

void	HIDBrowserSetEventHandler( HIDBrowserRef inBrowser, HIDBrowserEventHandler_f inHandler, void *inContext )
{
	inBrowser->eventHandler = inHandler;
	inBrowser->eventContext	= inContext;
}

//===========================================================================================================================
//	HIDBrowserStart
//===========================================================================================================================

static void	_HIDBrowserStart( void *inContext );

OSStatus	HIDBrowserStart( HIDBrowserRef inBrowser )
{
	hid_dlog( kLogLevelTrace, "HID browser starting...\n" );
	CFRetain( inBrowser );
	dispatch_async_f( inBrowser->queue, inBrowser, _HIDBrowserStart );
	return( kNoErr );
}

static void	_HIDBrowserStart( void *inContext )
{
	HIDBrowserRef const		browser = (HIDBrowserRef) inContext;
	OSStatus				err;
	HIDDeviceRef			hidDevice;
	CFIndex					i, n;

	// Get the already-attached devices.
	
	pthread_mutex_lock( browser->devicesLockPtr );
	ForgetCF( &browser->devices );
	browser->devices = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	pthread_mutex_unlock( browser->devicesLockPtr );
	require_action( browser->devices, exit, err = kNoMemoryErr );
	
	pthread_mutex_lock( &gVirtualHIDLock );
	n = gVirtualHIDDevices ? CFArrayGetCount( gVirtualHIDDevices ) : 0;
	for( i = 0; i < n; ++i )
	{
		hidDevice = (HIDDeviceRef) CFArrayGetValueAtIndex( gVirtualHIDDevices, i );
		pthread_mutex_lock( browser->devicesLockPtr );
		CFArrayAppendValue( browser->devices, hidDevice );
		pthread_mutex_unlock( browser->devicesLockPtr );
	}
	pthread_mutex_unlock( &gVirtualHIDLock );
	
	// Fake attach events for each device that was already attached.
	
	if( browser->eventHandler )
	{
		n = CFArrayGetCount( browser->devices );
		for( i = 0; i < n; ++i )
		{
			hidDevice = (HIDDeviceRef) CFArrayGetValueAtIndex( browser->devices, i );
			browser->eventHandler( kHIDBrowserEventAttached, hidDevice, browser->eventContext );
		}
		browser->eventHandler( kHIDBrowserEventStarted, NULL, browser->eventContext );
	}
	
	err = kNoErr;
	hid_dlog( kLogLevelTrace, "HID browser started\n" );
	
exit:
	if( err )
	{
		hid_ulog( kLogLevelNotice, "### HID browser start failed: %#m\n", err );
		if( browser->eventHandler ) browser->eventHandler( kHIDBrowserEventStopped, NULL, browser->eventContext );
		pthread_mutex_lock( browser->devicesLockPtr );
		ForgetCF( &browser->devices );
		pthread_mutex_unlock( browser->devicesLockPtr );
	}
	CFRelease( browser );
}

//===========================================================================================================================
//	HIDBrowserStop
//===========================================================================================================================

static void	_HIDBrowserStop( void *inContext );

void	HIDBrowserStop( HIDBrowserRef inBrowser )
{
	hid_dlog( kLogLevelTrace, "HID browser stopping...\n" );
	CFRetain( inBrowser );
	dispatch_async_f( inBrowser->queue, inBrowser, _HIDBrowserStop );
}

static void	_HIDBrowserStop( void *inContext )
{
	HIDBrowserRef const		browser	= (HIDBrowserRef) inContext;
	
	dispatch_source_forget( &browser->source );
	if( browser->eventHandler ) browser->eventHandler( kHIDBrowserEventStopped, NULL, browser->eventContext );
	pthread_mutex_lock( browser->devicesLockPtr );
	ForgetCF( &browser->devices );
	pthread_mutex_unlock( browser->devicesLockPtr );
	CFRelease( browser );
	hid_dlog( kLogLevelTrace, "HID browser stopped\n" );
}

//===========================================================================================================================
//	HIDBrowserStopDevices
//===========================================================================================================================

static void	_HIDBrowserStopDevices( void *inContext );

void	HIDBrowserStopDevices( HIDBrowserRef inBrowser )
{
	hid_dlog( kLogLevelTrace, "HID browser stopping devices...\n" );
	CFRetain( inBrowser );
	dispatch_async_f( inBrowser->queue, inBrowser, _HIDBrowserStopDevices );
}

static void	_HIDBrowserStopDevices( void *inContext )
{
	HIDBrowserRef const		browser	= (HIDBrowserRef) inContext;
	CFIndex					i, n;
	HIDDeviceRef			hidDevice;
	
	n = browser->devices ? CFArrayGetCount( browser->devices ) : 0;
	for( i = 0; i < n; ++i )
	{
		hidDevice = (HIDDeviceRef) CFArrayGetValueAtIndex( browser->devices, i );
		HIDDeviceStop( hidDevice );
	}
	
	CFRelease( browser );
	hid_dlog( kLogLevelTrace, "HID browser stopped devices\n" );
}

#if 0
#pragma mark -
#pragma mark == HIDDevice ==
#endif

//===========================================================================================================================
//	HIDDeviceGetTypeID
//===========================================================================================================================

CFTypeID	HIDDeviceGetTypeID( void )
{
	dispatch_once_f( &gHIDDeviceInitOnce, NULL, _HIDDeviceGetTypeID );
	return( gHIDDeviceTypeID );
}

static void _HIDDeviceGetTypeID( void *inContext )
{
	(void) inContext;
	
	gHIDDeviceTypeID = _CFRuntimeRegisterClass( &kHIDDeviceClass );
	check( gHIDDeviceTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	HIDDeviceCreateVirtual
//===========================================================================================================================

OSStatus	HIDDeviceCreateVirtual( HIDDeviceRef *outDevice, CFDictionaryRef inProperties )
{
	OSStatus			err;
	HIDDeviceRef		me;
	size_t				extraLen;
	CFTypeRef			property;
	uint8_t				uuid[ 16 ];
	char				cstr[ 128 ];
	int64_t				s64;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (HIDDeviceRef) _CFRuntimeCreateInstance( NULL, HIDDeviceGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->queue = dispatch_get_main_queue();
	arc_safe_dispatch_retain( me->queue );
	
	UUIDGet( me->uuid );
	
	if( inProperties )
	{
		s64 = CFDictionaryGetInt64( inProperties, kHIDDeviceProperty_CountryCode, &err );
		if( !err ) me->countryCode = CFNumberCreateInt64( s64 );
		
		err = CFDictionaryGetUUID( inProperties, kHIDDeviceProperty_DisplayUUID, uuid );
		if( !err )
		{
			UUIDtoCString( uuid, false, cstr );
			me->displayUUID = CFStringCreateWithCString( NULL, cstr, kCFStringEncodingUTF8 );
			require_action( me->displayUUID, exit, err = kUnknownErr );
		}
		
		property = CFDictionaryGetCFString( inProperties, kHIDDeviceProperty_Name, NULL );
		if( property )
		{
			CFRetain( property );
			me->name = (CFStringRef) property;
		}
		
		s64 = CFDictionaryGetInt64( inProperties, kHIDDeviceProperty_ProductID, &err );
		if( !err ) me->productID = CFNumberCreateInt64( s64 );
		
		property = CFDictionaryCopyCFData( inProperties, kHIDDeviceProperty_ReportDescriptor, NULL, NULL );
		if( property ) me->reportDescriptor = (CFDataRef) property;
		
		s64 = CFDictionaryGetInt64( inProperties, kHIDDeviceProperty_SampleRate, &err );
		if( !err )
		{
			me->sampleRate = CFNumberCreateInt64( s64 );
			require_action( me->sampleRate, exit, err = kUnknownErr );
		}
		
		CFDictionaryGetUUID( inProperties, kHIDDeviceProperty_UUID, me->uuid );
		
		s64 = CFDictionaryGetInt64( inProperties, kHIDDeviceProperty_VendorID, &err );
		if( !err ) me->vendorID = CFNumberCreateInt64( s64 );
	}
	
	*outDevice = me;
	me = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( me );
	return( err );
}

//===========================================================================================================================
//	_HIDDeviceFinalize
//===========================================================================================================================

static void	_HIDDeviceFinalize( CFTypeRef inCF )
{
	HIDDeviceRef const		me = (HIDDeviceRef) inCF;
	
	ForgetCF( &me->countryCode );
	ForgetCF( &me->displayUUID );
	ForgetCF( &me->name );
	ForgetCF( &me->productID );
	ForgetCF( &me->reportDescriptor );
	ForgetCF( &me->sampleRate );
	ForgetCF( &me->vendorID );
	dispatch_forget( &me->queue );
	hid_dlog( kLogLevelVerbose, "HID device finalized\n" );
}

//===========================================================================================================================
//	HIDDeviceSetDispatchQueue
//===========================================================================================================================

void	HIDDeviceSetDispatchQueue( HIDDeviceRef inDevice, dispatch_queue_t inQueue )
{
	ReplaceDispatchQueue( &inDevice->queue, inQueue );
}

//===========================================================================================================================
//	HIDDeviceSetEventHandler
//===========================================================================================================================

void	HIDDeviceSetEventHandler( HIDDeviceRef inDevice, HIDDeviceEventHandler_f inHandler, void *inContext )
{
	inDevice->eventHandler = inHandler;
	inDevice->eventContext = inContext;
}

//===========================================================================================================================
//	HIDDeviceCopyProperty
//===========================================================================================================================

CFTypeRef	HIDDeviceCopyProperty( HIDDeviceRef inDevice, CFStringRef inProperty, CFTypeRef inQualifier, OSStatus *outErr )
{
	OSStatus			err;
	CFTypeRef			value = NULL;
	char				cstr[ 128 ];
	
	(void) inQualifier;
	
	if( 0 ) {}
	
	// CountryCode
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_CountryCode ) )
	{
		value = inDevice->countryCode;
		if( value ) CFRetain( value );
	}
	
	// DisplayUUID
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_DisplayUUID ) )
	{
		value = inDevice->displayUUID;
		require_action_quiet( value, exit, err = kNotFoundErr );
		CFRetain( value );
	}
	
	// Name
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_Name ) )
	{
		value = inDevice->name;
		CFRetain( value );
	}
	
	// ProductID
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_ProductID ) )
	{
		value = inDevice->productID;
		if( value ) CFRetain( value );
	}
	
	// ReportDescriptor
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_ReportDescriptor ) )
	{
		value = inDevice->reportDescriptor;
		if( value )
		{
			CFRetain( value );
		}
	}
	
	// SampleRate
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_SampleRate ) )
	{
		value = inDevice->sampleRate;
		if( value ) CFRetain( value );
	}
	
	// UUID
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_UUID ) )
	{
		value = CFStringCreateWithCString( NULL, UUIDtoCString( inDevice->uuid, false, cstr ), kCFStringEncodingUTF8 );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// VendorID
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_VendorID ) )
	{
		value = inDevice->vendorID;
		if( value ) CFRetain( value );
	}
	
	// Unknown...
	
	else
	{
		err = kNotFoundErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	HIDDeviceSetProperty
//===========================================================================================================================

OSStatus	HIDDeviceSetProperty( HIDDeviceRef inDevice, CFStringRef inProperty, CFTypeRef inQualifier, CFTypeRef inValue )
{
	OSStatus		err;
	int64_t			s64;
	CFNumberRef		num;
	
	(void) inQualifier;
	
	if( 0 ) {}
	
	// CountryCode
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_CountryCode ) )
	{
		s64 = CFGetInt64( inValue, &err );
		require_noerr( err, exit );
		
		num = CFNumberCreateInt64( s64 );
		require_action( num, exit, err = kUnknownErr );
		
		CFReleaseNullSafe( inDevice->countryCode );
		inDevice->countryCode = num;
	}
	
	// DisplayUUID
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_DisplayUUID ) )
	{
		require_action( !inValue || CFIsType( inValue, CFString ), exit, err = kTypeErr );
		CFRetainNullSafe( inValue );
		CFReleaseNullSafe( inDevice->displayUUID );
		inDevice->displayUUID = (CFStringRef) inValue;
	}
	
	// Name
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_Name ) )
	{
		require_action( !inValue || CFIsType( inValue, CFString ), exit, err = kTypeErr );
		CFRetainNullSafe( inValue );
		CFReleaseNullSafe( inDevice->name );
		inDevice->name = (CFStringRef) inValue;
	}
	
	// ProductID
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_ProductID ) )
	{
		s64 = CFGetInt64( inValue, &err );
		require_noerr( err, exit );
		
		num = CFNumberCreateInt64( s64 );
		require_action( num, exit, err = kUnknownErr );
		
		CFReleaseNullSafe( inDevice->productID );
		inDevice->productID = num;
	}
	
	// ReportDescriptor
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_ReportDescriptor ) )
	{
		require_action( !inValue || CFIsType( inValue, CFData ), exit, err = kTypeErr );
		CFRetainNullSafe( inValue );
		CFReleaseNullSafe( inDevice->reportDescriptor );
		inDevice->reportDescriptor = (CFDataRef) inValue;
	}
	
	// SampleRate
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_SampleRate ) )
	{
		s64 = CFGetInt64( inValue, &err );
		require_noerr( err, exit );
		
		num = CFNumberCreateInt64( s64 );
		require_action( num, exit, err = kUnknownErr );
		
		CFReleaseNullSafe( inDevice->sampleRate );
		inDevice->sampleRate = num;
	}
	
	// VendorID
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_VendorID ) )
	{
		s64 = CFGetInt64( inValue, &err );
		require_noerr( err, exit );
		
		num = CFNumberCreateInt64( s64 );
		require_action( num, exit, err = kUnknownErr );
		
		CFReleaseNullSafe( inDevice->vendorID );
		inDevice->vendorID = num;
	}
	
	// Unknown...
	
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HIDDevicePostReport
//===========================================================================================================================

typedef struct
{
	HIDDeviceRef		device;
	size_t				reportLen;
	uint8_t				reportData[ 1 ]; // Variable length.
	
}	HIDDevicePostReportParams;

static void	_HIDDevicePostReport( void *inContext );

OSStatus	HIDDevicePostReport( HIDDeviceRef inDevice, const void *inReportPtr, size_t inReportLen )
{
	OSStatus						err;
	HIDDevicePostReportParams *		params;
	
	params = (HIDDevicePostReportParams *) malloc( offsetof( HIDDevicePostReportParams, reportData ) + inReportLen );
	require_action( params, exit, err = kNoMemoryErr );
	
	CFRetain( inDevice );
	params->device = inDevice;
	params->reportLen = inReportLen;
	memcpy( params->reportData, inReportPtr, inReportLen );
	
	dispatch_async_f( inDevice->queue, params, _HIDDevicePostReport );
	err = kNoErr;
	
exit:
	return( err );
}

static void	_HIDDevicePostReport( void *inContext )
{
	HIDDevicePostReportParams * const		params = (HIDDevicePostReportParams *) inContext;
	HIDDeviceRef const						device = params->device;
	
	if( device->eventHandler )
	{
		device->eventHandler( device, kHIDDeviceEventReport, kNoErr, params->reportData, params->reportLen, device->eventContext );
	}
	CFRelease( device );
	free( params );
}

//===========================================================================================================================
//	HIDDeviceStart
//===========================================================================================================================

static void	_HIDDeviceStart( void *inContext );

OSStatus	HIDDeviceStart( HIDDeviceRef inDevice )
{
	hid_dlog( kLogLevelVerbose, "HID device starting...\n" );
	CFRetain( inDevice );
	dispatch_async_f( inDevice->queue, inDevice, _HIDDeviceStart );
	return( kNoErr );
}

static void	_HIDDeviceStart( void *inContext )
{
	HIDDeviceRef const		device = (HIDDeviceRef) inContext;
	
	device->started = true;
	hid_dlog( kLogLevelVerbose, "HID device started\n" );
	CFRelease( device );
}

//===========================================================================================================================
//	HIDDeviceStop
//===========================================================================================================================

static void	_HIDDeviceStop( void *inContext );

void	HIDDeviceStop( HIDDeviceRef inDevice )
{
	hid_dlog( kLogLevelVerbose, "HID device stopping...\n" );
	CFRetain( inDevice );
	dispatch_async_f( inDevice->queue, inDevice, _HIDDeviceStop );
}

static void	_HIDDeviceStop( void *inContext )
{
	HIDDeviceRef const		device	= (HIDDeviceRef) inContext;
	Boolean					wasStarted;
	
	wasStarted = device->started;
	device->started = false;
	if( wasStarted )
	{
		if( device->eventHandler ) device->eventHandler( NULL, kHIDDeviceEventStopped, kNoErr, NULL, 0, device->eventContext );
		hid_dlog( kLogLevelVerbose, "HID device stopped\n" );
	}
	CFRelease( device );
}

#if 0
#pragma mark -
#pragma mark == Utils ==
#endif

//===========================================================================================================================
//	HIDRegisterDevice
//===========================================================================================================================

OSStatus	HIDRegisterDevice( HIDDeviceRef inDevice )
{
	OSStatus		err;
	
	pthread_mutex_lock( &gVirtualHIDLock );
	
	if( !gVirtualHIDDevices )
	{
		gVirtualHIDDevices = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		require_action( gVirtualHIDDevices, exit, err = kNoMemoryErr );
	}
	CFArrayAppendValue( gVirtualHIDDevices, inDevice );
	hid_ulog( kLogLevelNotice, "Registered HID %''@, %#U\n", inDevice->name, inDevice->uuid );
	err = kNoErr;
	
exit:
	pthread_mutex_unlock( &gVirtualHIDLock );
	return( err );
}

//===========================================================================================================================
//	HIDDeregisterDevice
//===========================================================================================================================

OSStatus	HIDDeregisterDevice( HIDDeviceRef inDevice )
{
	CFIndex				i, n;
	HIDDeviceRef		device;
	
	pthread_mutex_lock( &gVirtualHIDLock );
	
	n = gVirtualHIDDevices ? CFArrayGetCount( gVirtualHIDDevices ) : 0;
	for( i = 0; i < n; ++i )
	{
		device = (HIDDeviceRef) CFArrayGetValueAtIndex( gVirtualHIDDevices, i );
		if( device == inDevice )
		{
			hid_ulog( kLogLevelNotice, "Deregistered HID %''@, %#U\n", inDevice->name, inDevice->uuid );
			CFArrayRemoveValueAtIndex( gVirtualHIDDevices, i );
			--i;
			--n;
		}
	}
	if( n == 0 )
	{
		ForgetCF( &gVirtualHIDDevices );
	}
	
	pthread_mutex_unlock( &gVirtualHIDLock );
	return( kNoErr );
}

//===========================================================================================================================
//	HIDPostReport
//===========================================================================================================================

OSStatus	HIDPostReport( CFStringRef inUUID, const void *inReportPtr, size_t inReportLen )
{
	OSStatus			err;
	uint8_t				uuid[ 16 ];
	CFIndex				i, n;
	HIDDeviceRef		device;
	
	err = CFGetUUID( inUUID, uuid );
	require_noerr( err, exit );
	
	pthread_mutex_lock( &gVirtualHIDLock );
	n = gVirtualHIDDevices ? CFArrayGetCount( gVirtualHIDDevices ) : 0;
	for( i = 0; i < n; ++i )
	{
		device = (HIDDeviceRef) CFArrayGetValueAtIndex( gVirtualHIDDevices, i );
		if( memcmp( uuid, device->uuid, 16 ) == 0 )
		{
			HIDDevicePostReport( device, inReportPtr, inReportLen );
			break;
		}
	}
	pthread_mutex_unlock( &gVirtualHIDLock );
	require_action_quiet( i < n, exit, err = kNotFoundErr; 
		hid_ulog( kLogLevelNotice, "### Post HID report for %@ not found\n", inUUID ) );
	
exit:
	return( err );
}
