/*
	File:    	HIDUtilsLinux.c
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

#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include "CFUtils.h"
#include "CommonServices.h"
#include "StringUtils.h"
#include "ThreadUtils.h"
#include "UUIDUtils.h"

#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

#if( HIDUTILS_HID_RAW )
	#include <libudev.h>
	#include <linux/hidraw.h>
#endif

//===========================================================================================================================
//	Internals
//===========================================================================================================================

#if( HIDUTILS_HID_RAW )
	static OSStatus		_HIDDeviceCreateWithHIDRawDevice( HIDDeviceRef *outDevice, struct udev_device *inDevice );
	static CFArrayRef	_HIDCopyDevicesWithUdev( struct udev *inUdev, OSStatus *outErr );
#endif

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

#if( HIDUTILS_HID_RAW )
typedef struct
{
	HIDBrowserRef					browser;		// Owning browser.
	struct udev *					udev;			// udev object backing the monitor.
	struct udev_monitor *			monitor;		// udev monitor object.
	
}	HIDMonitorContext;
#endif

struct HIDBrowserPrivate
{
	CFRuntimeBase					base;			// CF type info. Must be first.
	dispatch_queue_t				queue;			// Queue to run all operations and deliver callbacks on.
#if( HIDUTILS_HID_RAW )
	dispatch_source_t				source;			// Source to call us when events are pending.
#endif
	CFMutableArrayRef				devices;		// Device objects we've detected.
	pthread_mutex_t					devicesLock;	// Lock for modifications to "devices".
	pthread_mutex_t *				devicesLockPtr;	// Ptr for devicesLock for NULL testing.
	HIDBrowserEventHandler_f		eventHandler;	// Function to call when an event occurs.
	void *							eventContext;	// Context to pass to function when an event occurs.
};

static void	_HIDBrowserGetTypeID( void *inContext );
static void	_HIDBrowserFinalize( CFTypeRef inCF );
#if( HIDUTILS_HID_RAW )
	static void	_HIDBrowserReadHandler( void *inContext );
	static void	_HIDBrowserCancelHandler( void *inContext );
#endif

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

#if( HIDUTILS_HID_RAW )
typedef struct
{
	HIDDeviceRef		device;			// Owning browser.
	int					fd;				// File desriptor for the hidraw device.
	uint8_t				buf[ 4096 ];	// Buffer for reading reports from the device.
	
}	HIDDeviceContext;
#endif

struct HIDDevicePrivate
{
	CFRuntimeBase				base;				// CF type info. Must be first.
	dispatch_queue_t			queue;				// Queue to run all operations and deliver callbacks on.
#if( HIDUTILS_HID_RAW )
	struct udev_device *		hidrawDevice;		// Underlying device this object is tracking.
	struct udev_device *		hidDevice;			// Weak reference to the HID parent of the device.
	struct udev_device *		usbDevice;			// Weak reference to the USB parent of the device.
	dispatch_source_t			source;				// Source for reading HID reports from the hidraw dev node.
#endif
	CFNumberRef					countryCode;		// Country code.
	CFStringRef					displayUUID;		// Display this HID device is associated with (or NULL if none).
	CFStringRef					name;				// Name of the device (e.g. "ShuttleXpress").
	CFNumberRef					productID;			// USB product ID of device.
	CFDataRef					reportDescriptor;	// HID descriptor to describe reports for this device.
	CFNumberRef					sampleRate;			// Sample rate of the device.
	uint8_t						uuid[ 16 ];			// UUID for device. May not be persistent.
	CFNumberRef					vendorID;			// USB vendor ID of device.
	Boolean						started;			// True if the device has been started.
	HIDDeviceEventHandler_f		eventHandler;		// Function to call when a event is received from the device.
	void *						eventContext;		// Context to pass to event handler.
};

static void	_HIDDeviceGetTypeID( void *inContext );
static void	_HIDDeviceFinalize( CFTypeRef inCF );
#if( HIDUTILS_HID_RAW )
	static CFDataRef	_HIDDeviceCopyReportDescriptor( HIDDeviceRef inDevice, OSStatus *outErr );
	static void			_HIDDeviceReadHandler( void *inContext );
	static void			_HIDDeviceCancelHandler( void *inContext );
#endif

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

#if 0
#pragma mark -
#endif

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
	
#if( HIDUTILS_HID_RAW )
	check( !me->source );
#endif
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
	CFMutableArrayRef		array;
#if( HIDUTILS_HID_RAW )
	struct udev *			udev;
	HIDMonitorContext *		ctx;
	int						fd;
#endif
	CFIndex					i, n;
	HIDDeviceRef			hidDevice;
	
#if( HIDUTILS_HID_RAW )
	dispatch_source_forget( &browser->source );
	
	// Start listening for device attaches and detaches.
	
	ctx = (HIDMonitorContext *) calloc( 1, sizeof( *ctx ) );
	require_action( ctx, exit, err = kNoMemoryErr );
	ctx->browser = browser;
	
	ctx->udev = udev = udev_new();
	require_action( udev, exit, err = kUnknownErr );
	
	ctx->monitor = udev_monitor_new_from_netlink( udev, "udev" );
	require_action( ctx->monitor, exit, err = kUnknownErr );
	
	err = udev_monitor_filter_add_match_subsystem_devtype( ctx->monitor, "hidraw", NULL );
	require_noerr( err, exit );
	
	err = udev_monitor_enable_receiving( ctx->monitor );
	require_noerr( err, exit );
#endif
	
	// Get the already-attached devices.
	
#if( HIDUTILS_HID_RAW )
	array = _HIDCopyDevicesWithUdev( udev, NULL );
	if( !array )
#endif
	{
		array = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		require_action( array, exit, err = kNoMemoryErr );
	}
	pthread_mutex_lock( browser->devicesLockPtr );
	ReplaceCF( &browser->devices, array );
	pthread_mutex_unlock( browser->devicesLockPtr );
	CFRelease( array );
	
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
	
#if( HIDUTILS_HID_RAW )
	// Set up to be notified when a device is attached/detached.
	
	fd = udev_monitor_get_fd( ctx->monitor );
	require_action( fd >= 0, exit, err = kUnknownErr );
	
	browser->source = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, fd, 0, browser->queue );
	require_action( browser->source, exit, err = kUnknownErr );
	CFRetain( browser );
	dispatch_set_context( browser->source, ctx );
	dispatch_source_set_event_handler_f(  browser->source, _HIDBrowserReadHandler );
	dispatch_source_set_cancel_handler_f( browser->source, _HIDBrowserCancelHandler );
	dispatch_resume( browser->source );
	ctx = NULL;
#endif
	
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
#if( HIDUTILS_HID_RAW )
	if( ctx )
	{
		if( ctx->monitor )	udev_monitor_unref( ctx->monitor );
		if( ctx->udev )		udev_unref( ctx->udev );
		free( ctx );
	}
#endif
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
	
#if( HIDUTILS_HID_RAW )
	dispatch_source_forget( &browser->source );
#endif
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

#if( HIDUTILS_HID_RAW )
//===========================================================================================================================
//	_HIDBrowserReadHandler
//===========================================================================================================================

static void	_HIDBrowserReadHandler( void *inContext )
{
	HIDMonitorContext * const		ctx		= (HIDMonitorContext *) inContext;
	HIDBrowserRef const				browser	= ctx->browser;
	OSStatus						err;
	const char *					action = "?";
	struct udev_device *			device;
	const char *					path;
	const char *					path2;
	CFIndex							i, n;
	HIDDeviceRef					hidDevice;
	
	device = udev_monitor_receive_device( ctx->monitor );
	require_action( device, exit, err = kReadErr );
	
	path = udev_device_get_devnode( device );
	require_action( path, exit, err = kPathErr );
	
	// Match the device by its /dev path.
	
	hidDevice = NULL;
	n = CFArrayGetCount( browser->devices );
	for( i = 0; i < n; ++i )
	{
		hidDevice = (HIDDeviceRef) CFArrayGetValueAtIndex( browser->devices, i );
		if( !hidDevice->hidrawDevice ) continue;
		path2 = udev_device_get_devnode( hidDevice->hidrawDevice );
		check( path2 );
		if( !path2 ) continue;
		
		if( strcmp( path, path2 ) == 0 )
		{
			break;
		}
	}
	if( i >= n ) hidDevice = NULL;
	
	// Process event by action.
	
	action = udev_device_get_action( device );
	require_action( action, exit, err = kParamErr );
	if( strcmp( action, "add" ) == 0 )
	{
		if( !hidDevice )
		{
			err = _HIDDeviceCreateWithHIDRawDevice( &hidDevice, device );
			require_noerr( err, exit );
			pthread_mutex_lock( browser->devicesLockPtr );
			CFArrayAppendValue( browser->devices, hidDevice );
			pthread_mutex_unlock( browser->devicesLockPtr );
			
			hid_ulog( kLogLevelTrace, "HID device attached: %#U (%@)\n", hidDevice->uuid, hidDevice->name );
			if( browser->eventHandler ) browser->eventHandler( kHIDBrowserEventAttached, hidDevice, browser->eventContext );
			CFRelease( hidDevice );
		}
	}
	else if( strcmp( action, "remove" ) == 0 )
	{
		if( hidDevice )
		{
			hid_ulog( kLogLevelTrace, "HID device detached: %#U (%@)\n", hidDevice->uuid, hidDevice->name );
			if( browser->eventHandler ) browser->eventHandler( kHIDBrowserEventDetached, hidDevice, browser->eventContext );
			pthread_mutex_lock( browser->devicesLockPtr );
			CFArrayRemoveValueAtIndex( browser->devices, i );
			pthread_mutex_unlock( browser->devicesLockPtr );
		}
	}
	else
	{
		hid_dlog( kLogLevelNotice, "HID device action: '%s'\n" );
	}
	err = kNoErr;
	
exit:
	if( err )		hid_ulog( kLogLevelNotice, "### HID browser action '%s' failed: %#m\n", action, err );
	if( device )	udev_device_unref( device );
}

//===========================================================================================================================
//	_HIDBrowserCancelHandler
//===========================================================================================================================

static void	_HIDBrowserCancelHandler( void *inContext )
{
	HIDMonitorContext * const		ctx = (HIDMonitorContext *) inContext;
	
	hid_dlog( kLogLevelTrace, "HID browser canceling...\n" );
	udev_monitor_unref( ctx->monitor );
	udev_unref( ctx->udev );
	CFRelease( ctx->browser );
	free( ctx );
	hid_dlog( kLogLevelTrace, "HID browser canceled\n" );
}
#endif // HIDUTILS_HID_RAW

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
#if 1
		s64 = CFDictionaryGetInt64( inProperties, kHIDDeviceProperty_CountryCode, &err );
		if( !err ) me->countryCode = CFNumberCreateInt64( s64 );

		s64 = CFDictionaryGetInt64( inProperties, kHIDDeviceProperty_ProductID, &err );
		if( !err ) me->productID = CFNumberCreateInt64( s64 );

		s64 = CFDictionaryGetInt64( inProperties, kHIDDeviceProperty_VendorID, &err );
		if( !err ) me->vendorID = CFNumberCreateInt64( s64 );
#endif

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
		
		property = CFDictionaryCopyCFData( inProperties, kHIDDeviceProperty_ReportDescriptor, NULL, NULL );
		if( property ) me->reportDescriptor = (CFDataRef) property;
		
		s64 = CFDictionaryGetInt64( inProperties, kHIDDeviceProperty_SampleRate, &err );
		if( !err )
		{
			me->sampleRate = CFNumberCreateInt64( s64 );
			require_action( me->sampleRate, exit, err = kUnknownErr );
		}
		
		CFDictionaryGetUUID( inProperties, kHIDDeviceProperty_UUID, me->uuid );
	}
	
	*outDevice = me;
	me = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( me );
	return( err );
}

#if( HIDUTILS_HID_RAW )
//===========================================================================================================================
//	_HIDDeviceCreateWithHIDRawDevice
//===========================================================================================================================

static OSStatus	_HIDDeviceCreateWithHIDRawDevice( HIDDeviceRef *outDevice, struct udev_device *inDevice )
{
	OSStatus			err;
	HIDDeviceRef		me;
	size_t				extraLen;
	const char *		str;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (HIDDeviceRef) _CFRuntimeCreateInstance( NULL, HIDDeviceGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->queue = dispatch_get_main_queue();
	arc_safe_dispatch_retain( me->queue );
	
	me->hidDevice = udev_device_get_parent_with_subsystem_devtype( inDevice, "hid", NULL );
	require_action( me->hidDevice, exit, err = kIncompatibleErr );
	
	me->usbDevice = udev_device_get_parent_with_subsystem_devtype( inDevice, "usb", "usb_device" );
	require_action( me->usbDevice, exit, err = kIncompatibleErr );
	
	str = udev_device_get_sysattr_value( me->usbDevice, "product" );
	if( !str ) str = "?";
	me->name = CFStringCreateWithCString( NULL, str, kCFStringEncodingUTF8 );
	require_action( me->name, exit, err = kFormatErr );
	
	udev_device_ref( inDevice );
	me->hidrawDevice = inDevice;
	
	UUIDGet( me->uuid );
	
	*outDevice = me;
	me = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( me );
	return( err );
}
#endif

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
#if( HIDUTILS_HID_RAW )
	if( me->hidrawDevice ) udev_device_unref( me->hidrawDevice );
	check( !me->source );
#endif
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
	
	// ProductID
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_ProductID ) )
	{
		value = inDevice->productID;
		if( value ) CFRetain( value );
	}
	
	// Name
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_Name ) )
	{
		value = inDevice->name;
		CFRetain( value );
	}
	
	// ReportDescriptor
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_ReportDescriptor ) )
	{
		value = inDevice->reportDescriptor;
		if( value )
		{
			CFRetain( value );
		}
		else
		{
			#if( HIDUTILS_HID_RAW )
				value = _HIDDeviceCopyReportDescriptor( inDevice, &err );
				require_noerr( err, exit );
			#else
				err = kNotFoundErr;
				goto exit;
			#endif
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

#if( HIDUTILS_HID_RAW )
//===========================================================================================================================
//	_HIDDeviceCopyReportDescriptor
//===========================================================================================================================

static CFDataRef	_HIDDeviceCopyReportDescriptor( HIDDeviceRef inDevice, OSStatus *outErr )
{
	CFDataRef							data = NULL;
	OSStatus							err;
	const char *						path;
	int									fd = -1;
	int									size;
	struct hidraw_report_descriptor		desc;
	struct hidraw_devinfo				hidrawInfo;
	HIDInfo								hidInfo;
	const uint8_t *						descPtr;
	uint8_t *							descBuf = NULL;
	size_t								descLen;
	
	require_action( inDevice->hidrawDevice, exit, err = kUnsupportedErr );
	
	path = udev_device_get_devnode( inDevice->hidrawDevice );
	require_action( path, exit, err = kPathErr );
	
	fd = open( path, O_RDWR );
	err = map_fd_creation_errno( fd );
	require_noerr( err, exit );
	
	err = ioctl( fd, HIDIOCGRAWINFO, &hidrawInfo );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
	
	HIDInfoInit( &hidInfo );
	hidInfo.vendorID  = hidrawInfo.vendor;
	hidInfo.productID = hidrawInfo.product;
	err = HIDCopyOverrideDescriptor( &hidInfo, &descBuf, &descLen );
	if( err )
	{
		size = 0;
		err = ioctl( fd, HIDIOCGRDESCSIZE, &size );
		err = map_global_noerr_errno( err );
		require_noerr( err, exit );
		
		memset( &desc, 0, sizeof( desc ) );
		desc.size = size;
		err = ioctl( fd, HIDIOCGRDESC, &desc );
		err = map_global_noerr_errno( err );
		require_noerr( err, exit );
		descPtr = desc.value;
		descLen = desc.size;
	}
	else
	{
		descPtr = descBuf;
	}
	data = CFDataCreate( NULL, descPtr, (CFIndex) descLen );
	require_action( data, exit, err = kNoMemoryErr );
	
exit:
	FreeNullSafe( descBuf );
	ForgetFD( &fd );
	if( outErr ) *outErr = err;
	return( data );
}
#endif

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
#if( HIDUTILS_HID_RAW )
	OSStatus				err;
	HIDDeviceContext *		ctx = NULL;
	const char *			path;
	
	// If there's no HID raw device, it's a virtual device so start doesn't need to do anything.
	
	if( !device->hidrawDevice )
	{
		device->started = true;
		err = kNoErr;
		goto exit;
	}
	
	dispatch_source_forget( &device->source );
	
	ctx = (HIDDeviceContext *) calloc( 1, sizeof( *ctx ) );
	require_action( ctx, exit, err = kNoMemoryErr );
	ctx->device	= device;
	ctx->fd		= -1;
	
	path = udev_device_get_devnode( device->hidrawDevice );
	require_action( path, exit, err = kPathErr );
	
	ctx->fd = open( path, O_RDWR );
	err = map_fd_creation_errno( ctx->fd );
	require_noerr( err, exit );
	
	err = fcntl( ctx->fd, F_SETFL, fcntl( ctx->fd, F_GETFL, 0 ) | O_NONBLOCK );
	err = map_global_value_errno( err != -1, err );
	require_noerr( err, exit );
	
	device->source = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, ctx->fd, 0, device->queue );
	require_action( device->source, exit, err = kUnknownErr );
	CFRetain( device );
	dispatch_set_context( device->source, ctx );
	dispatch_source_set_event_handler_f(  device->source, _HIDDeviceReadHandler );
	dispatch_source_set_cancel_handler_f( device->source, _HIDDeviceCancelHandler );
	dispatch_resume( device->source );
	ctx = NULL;
#endif
	
	device->started = true;
	hid_dlog( kLogLevelVerbose, "HID device started\n" );
	
#if( HIDUTILS_HID_RAW )
exit:
	if( ctx )
	{
		ForgetFD( &ctx->fd );
		free( ctx );
	}
	if( err )
	{
		hid_ulog( kLogLevelNotice, "### HID device start failed: %#m\n", err );
		if( device->eventHandler ) device->eventHandler( NULL, kHIDDeviceEventStopped, err, NULL, 0, device->eventContext );
	}
#endif
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
#if( HIDUTILS_HID_RAW )
	dispatch_source_forget( &device->source );
#endif
	if( wasStarted )
	{
		if( device->eventHandler ) device->eventHandler( NULL, kHIDDeviceEventStopped, kNoErr, NULL, 0, device->eventContext );
		hid_dlog( kLogLevelVerbose, "HID device stopped\n" );
	}
	CFRelease( device );
}

#if( HIDUTILS_HID_RAW )
//===========================================================================================================================
//	_HIDDeviceReadHandler
//===========================================================================================================================

static void	_HIDDeviceReadHandler( void *inContext )
{
	HIDDeviceContext * const		ctx		= (HIDDeviceContext *) inContext;
	HIDDeviceRef const				device	= ctx->device;
	OSStatus						err;
	ssize_t							n;
	
	n = read( ctx->fd, ctx->buf, sizeof( ctx->buf ) );
	err = map_global_value_errno( n >= 0, n );
	if( !err )
	{
		if( device->eventHandler )
		{
			device->eventHandler( device, kHIDDeviceEventReport, err, ctx->buf, (size_t) n, device->eventContext );
		}
	}
	else
	{
		dispatch_source_cancel( device->source );
		if( device->eventHandler )
		{
			device->eventHandler( device, kHIDDeviceEventReport, kEndingErr, NULL, 0, device->eventContext );
		}
	}
}

//===========================================================================================================================
//	_HIDDeviceCancelHandler
//===========================================================================================================================

static void	_HIDDeviceCancelHandler( void *inContext )
{
	HIDDeviceContext * const		ctx = (HIDDeviceContext *) inContext;
	OSStatus						err;
	
	DEBUG_USE_ONLY( err );
	
	err = close( ctx->fd );
	err = map_global_noerr_errno( err );
	check_noerr( err );
	
	CFRelease( ctx->device );
	free( ctx );
	hid_dlog( kLogLevelVerbose, "HID device canceled\n" );
}
#endif

#if 0
#pragma mark -
#pragma mark == Utils ==
#endif

//===========================================================================================================================
//	HIDCopyDevices
//===========================================================================================================================

CFArrayRef	HIDCopyDevices( OSStatus *outErr )
{
	CFArrayRef			result = NULL;
	OSStatus			err;
#if( HIDUTILS_HID_RAW )
	struct udev *		udev;
	
	udev = udev_new();
	require_action( udev, exit, err = kUnknownErr );
	
	result = _HIDCopyDevicesWithUdev( udev, &err );
	udev_unref( udev );
#else
	result = CFArrayCreate( NULL, NULL, 0, &kCFTypeArrayCallBacks );
	require_action( result, exit, err = kNoMemoryErr );
	err = kNoErr;
#endif
	
exit:
	if( outErr ) *outErr = err;
	return( result );
}

#if( HIDUTILS_HID_RAW )
//===========================================================================================================================
//	_HIDCopyDevicesWithUdev
//===========================================================================================================================

static CFArrayRef	_HIDCopyDevicesWithUdev( struct udev *inUdev, OSStatus *outErr )
{
	CFArrayRef					result		= NULL;
	CFMutableArrayRef			array		= NULL;
	struct udev_enumerate *		enumerate	= NULL;
	OSStatus					err;
	struct udev_list_entry *	listEntry;
	const char *				path;
	struct udev_device *		hidrawDevice;
	HIDDeviceRef				hidDevice;
	CFTypeRef					obj;
	
	array = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( array, exit, err = kNoMemoryErr );
	
	enumerate = udev_enumerate_new( inUdev );
	require_action( enumerate, exit, err = kUnknownErr );
	
	err = udev_enumerate_add_match_subsystem( enumerate, "hidraw" );
	require_noerr( err, exit );
	
	err = udev_enumerate_scan_devices( enumerate );
	require_noerr( err, exit );
	
	udev_list_entry_foreach( listEntry, udev_enumerate_get_list_entry( enumerate ) )
	{
		path = udev_list_entry_get_name( listEntry );
		check( path );
		if( !path ) continue;
		
		hidrawDevice = udev_device_new_from_syspath( inUdev, path );
		check( hidrawDevice );
		if( !hidrawDevice ) continue;
		
		err = _HIDDeviceCreateWithHIDRawDevice( &hidDevice, hidrawDevice );
		udev_device_unref( hidrawDevice );
		check_noerr( err );
		if( err ) continue;
		
		obj =  HIDDeviceCopyProperty( hidDevice, kHIDDeviceProperty_ReportDescriptor, NULL, NULL );
		if( obj )
		{
			HIDDeviceSetProperty( hidDevice, kHIDDeviceProperty_ReportDescriptor, NULL, obj );
			CFRelease( obj );
		}
		
		CFArrayAppendValue( array, hidDevice );
		CFRelease( hidDevice );
	}
	
	result = array;
	array = NULL;
	
exit:
	CFReleaseNullSafe( array );
	if( enumerate )	udev_enumerate_unref( enumerate );
	if( outErr )	*outErr = err;
	return( result );
}
#endif // HIDUTILS_HID_RAW

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

OSStatus	HIDRemoveFileConfig( void )
{
	OSStatus			err = 0;
	CFIndex				n;

	pthread_mutex_lock( &gVirtualHIDLock );
	n = gVirtualHIDDevices ? CFArrayGetCount( gVirtualHIDDevices ) : 0;

	hid_ulog( kLogLevelNotice, "HID remain num[%d]\n",n);

	CFArrayRemoveAllValues(gVirtualHIDDevices);
	gVirtualHIDDevices = NULL;

	pthread_mutex_unlock( &gVirtualHIDLock );

	return( err );

}
