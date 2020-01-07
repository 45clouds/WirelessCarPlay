/*
	File:    	HIDUtilsQNX.c
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

#include <pthread.h>

#include "CFUtils.h"
#include "CommonServices.h"
#include "StringUtils.h"
#include "ThreadUtils.h"
#include "UUIDUtils.h"

#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

#if( HIDUTILS_HID_RAW )

#include <sys/hiddi.h>
#include <sys/hidut.h>

//===========================================================================================================================
//	External
//===========================================================================================================================

// QNX's headers don't have these prototypes, but it's in their nm output so guess at the prototypes here.

extern int	hidd_get_device_instance( struct hidd_connection *inCnx, uint16_t inIndex, hidd_device_instance_t *outInstance );
extern int	hidd_get_report_desc( struct hidd_connection *inCnx, hidd_device_instance_t *inDevice, uint8_t **outPtr, uint16_t *outLen );

//===========================================================================================================================
//	Structures
//===========================================================================================================================

#define kHIDMaxReports		256

typedef struct HIDConnectionDevicePrivate *		HIDConnectionDeviceRef;
typedef struct HIDReportListenerPrivate *		HIDReportListenerRef;

// HIDConnectionDevicePrivate

struct HIDConnectionDevicePrivate
{
	HIDConnectionDeviceRef			next;				// Next device in the list.
	struct hidd_connection *		hidCnx;				// Connection to the hidd library.
	hidd_device_instance_t *		deviceInstance;		// Device instance from the hidd library.
	uint32_t						vendorID;			// USB vendor ID of device.
	uint32_t						productID;			// USB product ID of device.
	uint8_t							uuid[ 16 ];			// UUID for device. May not be persistent.
	CFDataRef						reportDescriptor;	// HID descriptor to describe reports for this device.
	CFStringRef						name;				// Name of the device (i.e. product string).
	int								reportsAttached;	// Total number of reports attached.
};

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static OSStatus	_HIDRegisterBrowser( HIDBrowserRef inBrowser );
static void		_HIDDeregisterBrowser( HIDBrowserRef inBrowser );
static OSStatus	_HIDRegisterReportListener( HIDDeviceRef inDevice );
static void		_HIDDeregisterReportListener( HIDDeviceRef inDevice );
static void		_HIDListenForReports( HIDConnectionDeviceRef inDevice, struct hidd_collection *inCollection, int inDepth );

static void	_HIDConnectionHandleDeviceInsertion( struct hidd_connection *inCnx, hidd_device_instance_t *inDevice );
static void	_HIDConnectionHandleDeviceRemoval( struct hidd_connection *inCnx, hidd_device_instance_t *inDevice );
static void
	_HIDConnectionHandleReport( 
		struct hidd_connection *	inCnx, 
		struct hidd_report *		inReport, 
		void *						inReportPtr, 
		_Uint32t					inReportLen, 
		_Uint32t					inFlags, 
		void *						inUser );

static void	_HIDBrowserDeviceAttached( HIDBrowserRef inBrowser, HIDConnectionDeviceRef inDevice );
static void	_HIDBrowserDeviceDetached( HIDBrowserRef inBrowser, HIDConnectionDeviceRef inDevice );

#endif // HIDUTILS_HID_RAW

static void	_HIDDeviceHandleReport( HIDDeviceRef inDevice, const void *inReportPtr, size_t inReportLen );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

#if( HIDUTILS_HID_RAW )
static hidd_funcs_t				gHIDConnectFuncs = 
{
	_HIDDI_NFUNCS, 
	_HIDConnectionHandleDeviceInsertion, 
	_HIDConnectionHandleDeviceRemoval, 
	_HIDConnectionHandleReport, 
	NULL
};

static hidd_connect_parm_t		gHIDConnectParams = 
{
	NULL,				// path
	HID_VERSION,		// vhid
	HIDD_VERSION,		// vhidd
	0,					// flags
	0,					// evtbufsz
	NULL,				// device_ident
	&gHIDConnectFuncs,	// funcs
	0,					// connect_wait
	{ 0, 0, 0, 0, 0 }	// _reserved
};
#endif

static pthread_mutex_t					gHIDLock				= PTHREAD_MUTEX_INITIALIZER;
#if( HIDUTILS_HID_RAW )
	static struct hidd_connection *		gHIDCnx					= NULL;
	static HIDBrowserRef				gHIDBrowsers			= NULL;
	static HIDConnectionDeviceRef		gHIDDevices				= NULL;
#endif
static HIDDeviceRef						gHIDReportListeners		= NULL;
static CFMutableArrayRef				gVirtualHIDDevices		= NULL;

ulog_define( HIDUtils, kLogLevelVerbose, kLogFlags_Default, "HID", NULL );
#define hid_dlog( LEVEL, ... )		dlogc( &log_category_from_name( HIDUtils ), (LEVEL), __VA_ARGS__ )
#define hid_ulog( LEVEL, ... )		ulog( &log_category_from_name( HIDUtils ), (LEVEL), __VA_ARGS__ )

//===========================================================================================================================
//	HIDBrowser
//===========================================================================================================================

struct HIDBrowserPrivate
{
	CFRuntimeBase					base;			// CF type info. Must be first.
	HIDBrowserRef					next;			// Next browser in the global list.
	dispatch_queue_t				queue;			// Queue to run all operations and deliver callbacks on.
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
	HIDDeviceRef				nextReportListener;	// Next device in the report listener list.
	HIDBrowserRef				browser;			// Browser this device is associated with.
	dispatch_queue_t			queue;				// Queue to run all operations and deliver callbacks on.
	CFNumberRef					countryCode;		// Country code.
	CFStringRef					displayUUID;		// UUID of the display this HID device is associated with (or NULL if none).
	CFStringRef					name;				// Name of the device (e.g. "ShuttleXpress").
	uint32_t					productID;			// USB product ID of device.
	CFNumberRef					productIDObj;		// USB product ID of device as an object.
	CFDataRef					reportDescriptor;	// HID descriptor to describe reports for this device.
	CFNumberRef					sampleRate;			// Sample rate of the device.
	uint8_t						uuid[ 16 ];			// UUID for device. May not be persistent.
	uint32_t					vendorID;			// USB vendor ID of device.
	CFNumberRef					vendorIDObj;		// USB vendor ID of device as an object.
	Boolean						isVirtual;			// True if it's a virtual device.
	Boolean						started;			// True if we've started listening for reports.
	HIDDeviceEventHandler_f		eventHandler;		// Function to call when a event is received from the device.
	void *						eventContext;		// Context to pass to event handler.
};

static void	_HIDDeviceGetTypeID( void *inContext );
static void	_HIDDeviceFinalize( CFTypeRef inCF );

#if( HIDUTILS_HID_RAW )
	static OSStatus	_HIDDeviceCreate( HIDDeviceRef *outDevice, HIDConnectionDeviceRef inDevice );
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

#if( HIDUTILS_HID_RAW )
//===========================================================================================================================
//	_HIDRegisterBrowser
//===========================================================================================================================

static OSStatus	_HIDRegisterBrowser( HIDBrowserRef inBrowser )
{
	OSStatus		err;
	
	pthread_mutex_lock( &gHIDLock );
	
	CFRetain( inBrowser );
	inBrowser->next = gHIDBrowsers;
	gHIDBrowsers = inBrowser;
	hid_dlog( kLogLevelTrace, "HID browser registered: %p\n", inBrowser );
	
	if( !gHIDCnx )
	{
		err = hidd_connect( &gHIDConnectParams, &gHIDCnx );
		require_noerr( err, exit );
	}
	err = kNoErr;
	
exit:
	pthread_mutex_unlock( &gHIDLock );
	if( err ) _HIDDeregisterBrowser( inBrowser );
	return( err );
}

//===========================================================================================================================
//	_HIDDeregisterBrowser
//===========================================================================================================================

static void	_HIDDeregisterBrowser( HIDBrowserRef inBrowser )
{
	HIDBrowserRef *		next;
	HIDBrowserRef		curr;
	
	pthread_mutex_lock( &gHIDLock );
	for( next = &gHIDBrowsers; ( curr = *next ) != NULL; next = &curr->next )
	{
		if( curr == inBrowser )
		{
			*next = curr->next;
			hid_dlog( kLogLevelTrace, "HID browser deregistered: %p\n", curr );
			CFRelease( curr );
			break;
		}
	}
	if( !gHIDBrowsers && gHIDCnx )
	{
		hidd_disconnect( gHIDCnx );
		gHIDCnx = NULL;
	}
	pthread_mutex_unlock( &gHIDLock );
}
#endif // HIDUTILS_HID_RAW

//===========================================================================================================================
//	_HIDRegisterReportListener
//===========================================================================================================================

static OSStatus	_HIDRegisterReportListener( HIDDeviceRef inDevice )
{
	pthread_mutex_lock( &gHIDLock );
	CFRetain( inDevice );
	inDevice->nextReportListener = gHIDReportListeners;
	gHIDReportListeners = inDevice;
	hid_dlog( kLogLevelTrace, "HID report listener registered: %#U (%@)\n", inDevice->uuid, inDevice->name );
	pthread_mutex_unlock( &gHIDLock );
	return( kNoErr );
}

//===========================================================================================================================
//	_HIDDeregisterReportListener
//===========================================================================================================================

static void	_HIDDeregisterReportListener( HIDDeviceRef inDevice )
{
	HIDDeviceRef *		next;
	HIDDeviceRef		curr;
	
	pthread_mutex_lock( &gHIDLock );
	for( next = &gHIDReportListeners; ( curr = *next ) != NULL; next = &curr->nextReportListener )
	{
		if( curr == inDevice )
		{
			*next = curr->nextReportListener;
			hid_dlog( kLogLevelTrace, "HID report listener deregistered: %#U (%@)\n", inDevice->uuid, inDevice->name );
			CFRelease( curr );
			break;
		}
	}
	pthread_mutex_unlock( &gHIDLock );
}

#if( HIDUTILS_HID_RAW )
//===========================================================================================================================
//	_HIDListenForReports
//===========================================================================================================================

static void	_HIDListenForReports( HIDConnectionDeviceRef inDevice, struct hidd_collection *inCollection, int inDepth )
{
	OSStatus							err;
	uint16_t							reportIndex, collectionIndex, collectionCount;
	struct hidd_report_instance *		reportInstance;
	struct hidd_report *				report;
	struct hidd_collection **			collections;
	
	for( reportIndex = 0; reportIndex < kHIDMaxReports; ++reportIndex )
	{
		err = hidd_get_report_instance( inCollection, reportIndex, HID_INPUT_REPORT, &reportInstance );
		if( err ) continue;
		
		err = hidd_report_attach( inDevice->hidCnx, inDevice->deviceInstance, reportInstance, 0, sizeof( inDevice ), &report );
		if( err ) continue;
		*( (HIDConnectionDeviceRef *) hidd_report_extra( report ) ) = inDevice;
		++inDevice->reportsAttached;
		
		hid_dlog( kLogLevelTrace, "HID device %#U (%@) listening on report %d at level %d (%d total)\n", 
			inDevice->uuid, inDevice->name, reportIndex, inDepth, inDevice->reportsAttached );
	}
	if( inDepth >= kHIDMaxReports )
	{
		hid_ulog( kLogLevelWarning, "HID device %#U (%@) stopping at max level %d\n", inDevice->uuid, inDevice->name, inDepth );
		goto exit;
	}
	
	// Recursively process all the sub-collections.
	
	++inDepth;
	err = hidd_get_collections( NULL, inCollection, &collections, &collectionCount );
	if( err ) collectionCount = 0;
	
	hid_dlog( kLogLevelTrace, "HID device %#U (%@) processing %d collection(s) at depth %d: %#m\n", 
		inDevice->uuid, inDevice->name, collectionCount, inDepth, err );
	
	for( collectionIndex = 0; collectionIndex < collectionCount; ++collectionIndex )
	{
		_HIDListenForReports( inDevice, collections[ collectionIndex ], inDepth );
	}
	
exit:
	return;
}

//===========================================================================================================================
//	_HIDConnectionHandleDeviceInsertion
//===========================================================================================================================

static void	_HIDConnectionHandleDeviceInsertion( struct hidd_connection *inCnx, hidd_device_instance_t *inDevice )
{
	OSStatus						err;
	HIDConnectionDeviceRef			device;
	char							cstr[ 256 ];
	HIDInfo							hidInfo;
	uint8_t *						descPtr = NULL;
	size_t							descLen;
	uint16_t						u16;
	struct hidd_collection **		collections;
	uint16_t						i, n;
	HIDBrowserRef					browser;
	
	device = (HIDConnectionDeviceRef) calloc( 1, sizeof( *device ) );
	require_action( device, exit, err = kNoMemoryErr );
	device->hidCnx			= inCnx;
	device->deviceInstance	= inDevice;
	device->vendorID		= inDevice->device_ident.vendor_id;
	device->productID		= inDevice->device_ident.product_id;
	UUIDGet( device->uuid );
	
	HIDInfoInit( &hidInfo );
	hidInfo.vendorID  = inDevice->device_ident.vendor_id;
	hidInfo.productID = inDevice->device_ident.product_id;
	err = HIDCopyOverrideDescriptor( &hidInfo, &descPtr, &descLen );
	if( err )
	{
		err = hidd_get_report_desc( inCnx, inDevice, &descPtr, &u16 );
		require_noerr( err, exit );
		descLen = u16;
	}
	device->reportDescriptor = CFDataCreate( NULL, descPtr, (CFIndex) descLen );
	require( device->reportDescriptor, exit );
	
	*cstr = '\0';
	err = hidd_get_product_string( inCnx, inDevice, cstr, (_Uint16t) sizeof( cstr ) );
	check_noerr( err );
	if( err ) *cstr = '\0';
	if( *cstr != '\0' )
	{
		device->name = CFStringCreateWithCString( NULL, cstr, kCFStringEncodingUTF8 );
		check( device->name );
	}
	if( !device->name )
	{
		device->name = CFSTR( "?" );
		CFRetain( device->name );
	}
	
	pthread_mutex_lock( &gHIDLock );
	device->next = gHIDDevices;
	gHIDDevices = device;
	pthread_mutex_unlock( &gHIDLock );
	
	hid_ulog( kLogLevelInfo, "HID device attached: vendor 0x%04X, product 0x%04X, %#U (%@)\n", 
		device->vendorID, device->productID, device->uuid, device->name );
	
	// Start listening for reports.
	
	err = hidd_get_collections( inDevice, NULL, &collections, &n );
	if( err ) n = 0;
	hid_dlog( kLogLevelTrace, "HID device %#U (%@) processing %d collection(s) at depth 1: %#m\n", 
		device->uuid, device->name, n, err );
	for( i = 0; i < n; ++i )
	{
		_HIDListenForReports( device, collections[ i ], 1 );
	}
	
	// Notify all the browsers.
	
	pthread_mutex_lock( &gHIDLock );
	for( browser = gHIDBrowsers; browser; browser = browser->next )
	{
		_HIDBrowserDeviceAttached( browser, device );
	}
	pthread_mutex_unlock( &gHIDLock );
	device = NULL;
	
exit:
	FreeNullSafe( descPtr );
	FreeNullSafe( device );
}

//===========================================================================================================================
//	_HIDConnectionHandleDeviceRemoval
//===========================================================================================================================

static void	_HIDConnectionHandleDeviceRemoval( struct hidd_connection *inCnx, hidd_device_instance_t *inDevice )
{
	HIDConnectionDeviceRef *		next;
	HIDConnectionDeviceRef			device;
	HIDBrowserRef					browser;
	
	(void) inCnx;
	
	pthread_mutex_lock( &gHIDLock );
	for( next = &gHIDDevices; ( device = *next ) != NULL; next = &device->next )
	{
		if( device->deviceInstance == inDevice )
		{
			*next = device->next;
			break;
		}
	}
	require_action( device, exit, 
		hid_ulog( kLogLevelNotice, "### Untracked HID device detached: vendor 0x%04X, product 0x%04X\n", 
			inDevice->device_ident.vendor_id, inDevice->device_ident.product_id ) );
	
	hid_ulog( kLogLevelInfo, "HID device detached: vendor 0x%04X, product 0x%04X, %#U (%@)\n", 
		device->vendorID, device->productID, device->uuid, device->name );
	
	if( device->reportsAttached > 0 )
	{
		device->reportsAttached = 0;
		hidd_reports_detach( device->hidCnx, device->deviceInstance );
	}
	
	// Notify all the listeners.
	
	for( browser = gHIDBrowsers; browser; browser = browser->next )
	{
		_HIDBrowserDeviceDetached( browser, device );
	}
	
	ForgetCF( &device->name );
	ForgetCF( &device->reportDescriptor );
	free( device );
	
exit:
	pthread_mutex_unlock( &gHIDLock );
}

//===========================================================================================================================
//	_HIDConnectionHandleReport
//===========================================================================================================================

static void
	_HIDConnectionHandleReport( 
		struct hidd_connection *	inCnx, 
		struct hidd_report *		inReport, 
		void *						inReportPtr, 
		_Uint32t					inReportLen, 
		_Uint32t					inFlags, 
		void *						inUser )
{
	HIDConnectionDeviceRef const		connectionDevice = *( (HIDConnectionDeviceRef *) hidd_report_extra( inReport ) );
	HIDDeviceRef						device;
	
	(void) inCnx;
	(void) inReport;
	(void) inFlags;
	(void) inUser;
	
	hid_ulog( kLogLevelChatty, "HID device %#U (%@) report %.3H\n", connectionDevice->uuid, connectionDevice->name, 
		inReportPtr, (int) inReportLen, 128 ); 
	
	pthread_mutex_lock( &gHIDLock );
	for( device = gHIDReportListeners; device; device = device->nextReportListener )
	{
		if( UUIDCompare( connectionDevice->uuid, device->uuid ) == 0 )
		{
			_HIDDeviceHandleReport( device, inReportPtr, inReportLen );
		}
	}
	pthread_mutex_unlock( &gHIDLock );
}

#endif // HIDUTILS_HID_RAW

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
	CFIndex					i, n;
	HIDDeviceRef			hidDevice;
	
	pthread_mutex_lock( browser->devicesLockPtr );
	ForgetCF( &browser->devices );
	browser->devices = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	pthread_mutex_unlock( browser->devicesLockPtr );
	require_action( browser->devices, exit, err = kNoMemoryErr );
	
	hid_dlog( kLogLevelTrace, "HID browser started\n" );
	
#if( HIDUTILS_HID_RAW )
	err = _HIDRegisterBrowser( browser );
	require_noerr( err, exit );
#endif
	
	// Add virtual devices and fake attach events for them.
	
	pthread_mutex_lock( &gHIDLock );
	n = gVirtualHIDDevices ? CFArrayGetCount( gVirtualHIDDevices ) : 0;
	for( i = 0; i < n; ++i )
	{
		hidDevice = (HIDDeviceRef) CFArrayGetValueAtIndex( gVirtualHIDDevices, i );
		pthread_mutex_lock( browser->devicesLockPtr );
		CFArrayAppendValue( browser->devices, hidDevice );
		pthread_mutex_unlock( browser->devicesLockPtr );
	}
	pthread_mutex_unlock( &gHIDLock );
	
	if( browser->eventHandler )
	{
		n = CFArrayGetCount( browser->devices );
		for( i = 0; i < n; ++i )
		{
			hidDevice = (HIDDeviceRef) CFArrayGetValueAtIndex( browser->devices, i );
			if( hidDevice->isVirtual ) browser->eventHandler( kHIDBrowserEventAttached, hidDevice, browser->eventContext );
		}
		browser->eventHandler( kHIDBrowserEventStarted, NULL, browser->eventContext );
	}
	err = kNoErr;
	
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
	
#if( HIDUTILS_HID_RAW )
	_HIDDeregisterBrowser( browser );
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
//	_HIDBrowserDeviceAttached
//===========================================================================================================================

static void	_HIDBrowserDeviceAttached2( void *inContext );

static void	_HIDBrowserDeviceAttached( HIDBrowserRef inBrowser, HIDConnectionDeviceRef inDevice )
{
	OSStatus			err;
	HIDDeviceRef		device;
	
	err = _HIDDeviceCreate( &device, inDevice );
	require_noerr( err, exit );
	
	CFRetain( inBrowser );
	device->browser = inBrowser;
	
	dispatch_async_f( inBrowser->queue, device, _HIDBrowserDeviceAttached2 );
	
exit:
	return;
}

static void	_HIDBrowserDeviceAttached2( void *inContext )
{
	HIDDeviceRef const		device  = (HIDDeviceRef) inContext;
	HIDBrowserRef const		browser = device->browser;
	
	require_quiet( browser->devices, exit );
	
	pthread_mutex_lock( browser->devicesLockPtr );
	CFArrayAppendValue( browser->devices, device );
	pthread_mutex_unlock( browser->devicesLockPtr );
	if( browser->eventHandler ) browser->eventHandler( kHIDBrowserEventAttached, device, browser->eventContext );
	
exit:
	CFRelease( device );
}

//===========================================================================================================================
//	_HIDBrowserDeviceDetached
//===========================================================================================================================

typedef struct
{
	HIDBrowserRef		browser;
	uint8_t				uuid[ 16 ];
	
}	HIDBrowserDeviceDetachedParams;

static void	_HIDBrowserDeviceDetached2( void *inContext );

static void	_HIDBrowserDeviceDetached( HIDBrowserRef inBrowser, HIDConnectionDeviceRef inDevice )
{
	HIDBrowserDeviceDetachedParams *		params;
	
	params = (HIDBrowserDeviceDetachedParams *) malloc( sizeof( *params ) );
	require( params, exit );
	CFRetain( inBrowser );
	params->browser = inBrowser;
	UUIDCopy( params->uuid, inDevice->uuid );
	
	dispatch_async_f( inBrowser->queue, params, _HIDBrowserDeviceDetached2 );
	
exit:
	return;
}

static void	_HIDBrowserDeviceDetached2( void *inContext )
{
	HIDBrowserDeviceDetachedParams * const		params  = (HIDBrowserDeviceDetachedParams *) inContext;
	HIDBrowserRef const							browser = params->browser;
	CFIndex										i, n;
	HIDDeviceRef								device;
	
	require_quiet( browser->devices, exit );
	
	n = CFArrayGetCount( browser->devices );
	for( i = n - 1; i >= 0; --i )
	{
		device = (HIDDeviceRef) CFArrayGetValueAtIndex( browser->devices, i );
		if( UUIDCompare( device->uuid, params->uuid ) == 0 )
		{
			if( browser->eventHandler ) browser->eventHandler( kHIDBrowserEventDetached, device, browser->eventContext );
			pthread_mutex_lock( browser->devicesLockPtr );
			CFArrayRemoveValueAtIndex( browser->devices, i );
			pthread_mutex_unlock( browser->devicesLockPtr );
			break;
		}
	}
	
exit:
	CFRelease( browser );
	free( params );
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
	me->isVirtual = true;
	
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
		if( !err ) me->productIDObj = CFNumberCreateInt64( s64 );
		
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
		if( !err ) me->vendorIDObj = CFNumberCreateInt64( s64 );
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
//	_HIDDeviceCreate
//===========================================================================================================================

static OSStatus	_HIDDeviceCreate( HIDDeviceRef *outDevice, HIDConnectionDeviceRef inDevice )
{
	OSStatus			err;
	HIDDeviceRef		me;
	size_t				extraLen;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (HIDDeviceRef) _CFRuntimeCreateInstance( NULL, HIDDeviceGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->queue = dispatch_get_main_queue();
	arc_safe_dispatch_retain( me->queue );
	
	me->productID = inDevice->productID;
	me->vendorID = inDevice->vendorID;
	UUIDCopy( me->uuid, inDevice->uuid );
	
	CFRetain( inDevice->reportDescriptor );
	me->reportDescriptor = inDevice->reportDescriptor;
	
	CFRetain( inDevice->name );
	me->name = inDevice->name;
	
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
	
	ForgetCF( &me->browser );
	ForgetCF( &me->countryCode );
	ForgetCF( &me->displayUUID );
	ForgetCF( &me->name );
	ForgetCF( &me->productIDObj );
	ForgetCF( &me->reportDescriptor );
	ForgetCF( &me->sampleRate );
	ForgetCF( &me->vendorIDObj );
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
		value = inDevice->productIDObj;
		if( value ) CFRetain( value );
	}
	
	// ReportDescriptor
	
	else if( CFEqual( inProperty, kHIDDeviceProperty_ReportDescriptor ) )
	{
		value = inDevice->reportDescriptor;
		require_action_quiet( value, exit, err = kNotFoundErr );
		CFRetain( value );
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
		value = inDevice->vendorIDObj;
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
		
		CFReleaseNullSafe( inDevice->productIDObj );
		inDevice->productIDObj = num;
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
		
		CFReleaseNullSafe( inDevice->vendorIDObj );
		inDevice->vendorIDObj = num;
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

OSStatus	HIDDevicePostReport( HIDDeviceRef inDevice, const void *inReportPtr, size_t inReportLen )
{
	HIDDeviceRef		device;
	
	pthread_mutex_lock( &gHIDLock );
	for( device = gHIDReportListeners; device; device = device->nextReportListener )
	{
		if( UUIDCompare( inDevice->uuid, device->uuid ) == 0 )
		{
			_HIDDeviceHandleReport( device, inReportPtr, inReportLen );
		}
	}
	pthread_mutex_unlock( &gHIDLock );
	return( kNoErr );
}

//===========================================================================================================================
//	HIDDeviceStart
//===========================================================================================================================

static void	_HIDDeviceStart( void *inContext );

OSStatus	HIDDeviceStart( HIDDeviceRef inDevice )
{
	hid_dlog( kLogLevelTrace, "HID device starting...\n" );
	CFRetain( inDevice );
	dispatch_async_f( inDevice->queue, inDevice, _HIDDeviceStart );
	return( kNoErr );
}

static void	_HIDDeviceStart( void *inContext )
{
	HIDDeviceRef const		device = (HIDDeviceRef) inContext;
	OSStatus				err;
	
	DEBUG_USE_ONLY( err );
	
	err = _HIDRegisterReportListener( device );
	check_noerr( err );
	device->started = true;
	
	hid_ulog( kLogLevelTrace, "HID device %#U (%@) started\n", device->uuid, device->name );
	CFRelease( device );
}

//===========================================================================================================================
//	HIDDeviceStop
//===========================================================================================================================

static void	_HIDDeviceStop( void *inContext );

void	HIDDeviceStop( HIDDeviceRef inDevice )
{
	hid_dlog( kLogLevelTrace, "HID device stopping...\n" );
	CFRetain( inDevice );
	dispatch_async_f( inDevice->queue, inDevice, _HIDDeviceStop );
}

static void	_HIDDeviceStop( void *inContext )
{
	HIDDeviceRef const		device = (HIDDeviceRef) inContext;
	
	if( device->started )
	{
		device->started = false;
		_HIDDeregisterReportListener( device );
		if( device->eventHandler ) device->eventHandler( NULL, kHIDDeviceEventStopped, kNoErr, NULL, 0, device->eventContext );
		hid_dlog( kLogLevelTrace, "HID device stopped\n" );
	}
	CFRelease( device );
}

//===========================================================================================================================
//	_HIDDeviceHandleReport
//===========================================================================================================================

typedef struct
{
	HIDDeviceRef		device;
	size_t				reportLen;
	uint8_t				reportData[ 1 ]; // Variable length report.
	
}	HIDDeviceHandleReportParams;

static void	_HIDDeviceHandleReport2( void *inContext );

static void	_HIDDeviceHandleReport( HIDDeviceRef inDevice, const void *inReportPtr, size_t inReportLen )
{
	HIDDeviceHandleReportParams *		params;
	
	params = (HIDDeviceHandleReportParams *) malloc( offsetof( HIDDeviceHandleReportParams, reportData ) + inReportLen );
	require( params, exit );
	
	CFRetain( inDevice );
	params->device = inDevice;
	params->reportLen = inReportLen;
	memcpy( params->reportData, inReportPtr, inReportLen );
	
	dispatch_async_f( inDevice->queue, params, _HIDDeviceHandleReport2 );
	
exit:
	return;
}

static void	_HIDDeviceHandleReport2( void *inContext )
{
	HIDDeviceHandleReportParams * const		params = (HIDDeviceHandleReportParams *) inContext;
	HIDDeviceRef const						device = params->device;
	
	require_quiet( device->started, exit );
	if( device->eventHandler ) device->eventHandler( device, kHIDDeviceEventReport, kNoErr, 
		params->reportData, params->reportLen, device->eventContext );
	
exit:
	CFRelease( device );
	free( params );
}

#if 0
#pragma mark -
#pragma mark == Utils ==
#endif

//===========================================================================================================================
//	HIDCopyDevices
//===========================================================================================================================

CFArrayRef	HIDCopyDevices( OSStatus *outErr )
{
	CFArrayRef					result = NULL;
	CFMutableArrayRef			array  = NULL;
	OSStatus					err;
	CFIndex						i, n;
	HIDDeviceRef				device;
#if( HIDUTILS_HID_RAW )
	HIDConnectionDeviceRef		connectionDevice;
#endif
	
	array = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( array, exit, err = kNoMemoryErr );
	
	pthread_mutex_lock( &gHIDLock );
	n = gVirtualHIDDevices ? CFArrayGetCount( gVirtualHIDDevices ) : 0;
	for( i = 0; i < n; ++i )
	{
		device = (HIDDeviceRef) CFArrayGetValueAtIndex( gVirtualHIDDevices, i );
		CFArrayAppendValue( array, device );
	}
#if( HIDUTILS_HID_RAW )
	for( connectionDevice = gHIDDevices; connectionDevice; connectionDevice = connectionDevice->next )
	{
		err = _HIDDeviceCreate( &device, connectionDevice );
		check_noerr( err );
		if( !err )
		{
			CFArrayAppendValue( array, device );
			CFRelease( device );
		}
	}
#endif
	pthread_mutex_unlock( &gHIDLock );
	
	result = array;
	array = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( array );
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	HIDRegisterDevice
//===========================================================================================================================

OSStatus	HIDRegisterDevice( HIDDeviceRef inDevice )
{
	OSStatus		err;
	
	pthread_mutex_lock( &gHIDLock );
	
	if( !gVirtualHIDDevices )
	{
		gVirtualHIDDevices = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		require_action( gVirtualHIDDevices, exit, err = kNoMemoryErr );
	}
	CFArrayAppendValue( gVirtualHIDDevices, inDevice );
	hid_ulog( kLogLevelNotice, "Registered HID %''@, %#U\n", inDevice->name, inDevice->uuid );
	err = kNoErr;
	
exit:
	pthread_mutex_unlock( &gHIDLock );
	return( err );
}

//===========================================================================================================================
//	HIDDeregisterDevice
//===========================================================================================================================

OSStatus	HIDDeregisterDevice( HIDDeviceRef inDevice )
{
	CFIndex				i, n;
	HIDDeviceRef		device;
	
	pthread_mutex_lock( &gHIDLock );
	
	n = gVirtualHIDDevices ? CFArrayGetCount( gVirtualHIDDevices ) : 0;
	for( i = n - 1; i >= 0; --i )
	{
		device = (HIDDeviceRef) CFArrayGetValueAtIndex( gVirtualHIDDevices, i );
		if( device == inDevice )
		{
			hid_ulog( kLogLevelNotice, "Deregistered HID %''@, %#U\n", inDevice->name, inDevice->uuid );
			CFArrayRemoveValueAtIndex( gVirtualHIDDevices, i );
			--n;
		}
	}
	if( n == 0 ) ForgetCF( &gVirtualHIDDevices );
	
	pthread_mutex_unlock( &gHIDLock );
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
	
	pthread_mutex_lock( &gHIDLock );
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
	pthread_mutex_unlock( &gHIDLock );
	require_action_quiet( i < n, exit, err = kNotFoundErr; 
		hid_ulog( kLogLevelNotice, "### Post HID report for %@ not found\n", inUUID ) );
	
exit:
	return( err );
}
