/*
	File:    	CarPlayControlClient.c
	Package: 	CarPlay Communications Plug-in.
	Abstract: 	n/a 
	Version: 	280.33.8
	
	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
	Apple software is governed by and subject to the terms and conditions of your MFi License,
	including, but not limited to, the restrictions specified in the provision entitled ‚ÄùPublic 
	Software‚Ä? and is further subject to your agreement to the following additional terms, and your 
	agreement that the use, installation, modification or redistribution of this Apple software
	constitutes acceptance of these additional terms. If you do not agree with these additional terms,
	please do not use, install, modify or redistribute this Apple software.
	
	Subject to all of these terms and in¬†consideration of your agreement to abide by them, Apple grants
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
	fixes or enhancements to Apple in connection with this software (‚ÄúFeedback‚Ä?, you hereby grant to
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
	
	Copyright (C) 2014-2015 Apple Inc. All Rights Reserved.
*/

#include "CarPlayControlClient.h"
	#include "CarPlayControlCommon.h"

#include <CoreUtils/BonjourBrowser.h>
#include <CoreUtils/HTTPUtils.h>
#include <CoreUtils/HTTPClient.h>
#include <CoreUtils/PrintFUtils.h>
#include <CoreUtils/StringUtils.h>
#include <CoreUtils/TickUtils.h>

#include <stdio.h>
#include <string.h>

#include "AirPlayVersion.h"
#include "AirPlayReceiverServerPriv.h"
#include "dns_sd.h"
#include <glib.h>
#include CF_HEADER

//===========================================================================================================================
//	Constants
//===========================================================================================================================

#define kCarPlayControlClient_ConnectionTimeoutSeconds		5

//===========================================================================================================================
//	Configuration
//===========================================================================================================================

ulog_define( CarPlayControlClient, kLogLevelNotice, kLogFlags_Default, "CarPlayControl", NULL );
#define cpcc_ulog( LEVEL, ... )		ulog( &log_category_from_name( CarPlayControlClient ), ( LEVEL ), __VA_ARGS__ )

//===========================================================================================================================
// Prototypes
//===========================================================================================================================

static void _CarPlayControlClientRemoveController( CarPlayControlClientRef inClient, CarPlayControllerRef inController );

//===========================================================================================================================
// DNServiceContext
//===========================================================================================================================

typedef struct DNSServiceContext			DNSServiceContext;
typedef void ( *DNSServiceHandleError )( DNSServiceContext* inServiceCxt, OSStatus inErr, void *inCtx );
#define DNSServiceContextForget( X )		ForgetCustom( X, _DNSServiceDispose )

struct DNSServiceContext
{
	DNSServiceHandleError		errorHandler;
	void *						errorHandlerCtx;
	DNSServiceRef				service;
	dispatch_queue_t			queue;
	dispatch_source_t			source;
};

static void	_DNSServiceSocketEventCallback( void *inCtx )
{
	DNSServiceContext *		ctx = (DNSServiceContext*) inCtx;
	OSStatus				err;
	
	err = DNSServiceProcessResult( ctx->service );
	if( err && ctx->errorHandler )
	{
		ctx->errorHandler( ctx, err, ctx->errorHandlerCtx );
	}
}

static void	_DNSServiceSocketCancelCallback( void *inCtx )
{
	DNSServiceContext *		ctx = (DNSServiceContext*) inCtx;

	if( ctx->service )
	{
		DNSServiceRefDeallocate( ctx->service );
	}
	dispatch_release_null_safe( ctx->queue );
	free( ctx );
}

static OSStatus
	DNSServiceContextCreate(
		DNSServiceContext **outServiceCtx, DNSServiceRef inService,
		DNSServiceHandleError inErrorHandler, void *inErrorHandlerCtx )
{
	OSStatus				err;
	DNSServiceContext *		ctx;
	int						dnsSocket;
	
	ctx = calloc( 1, sizeof( *ctx ) );
	require_action( ctx, exit, err = kNoMemoryErr );
	
	dnsSocket = DNSServiceRefSockFD( inService );
	require_action( IsValidSocket( dnsSocket ), exit, err = kUnknownErr );
	
	ctx->queue = dispatch_queue_create( "CarPlayControl DNSServiceContext", NULL );
	require_action( ctx->queue, exit, err = kNoMemoryErr );
	
	ctx->source = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, dnsSocket, 0, ctx->queue );
	require_action( ctx->source, exit, err = kNoMemoryErr );
	
	// Success ( No more errors after this point, so assume ownership of "inService" )
	
	err = kNoErr;

	ctx->errorHandler = inErrorHandler;
	ctx->errorHandlerCtx = inErrorHandlerCtx;
	ctx->service = inService;
	dispatch_set_context( ctx->source, ctx );
	dispatch_source_set_event_handler_f( ctx->source, _DNSServiceSocketEventCallback );
	dispatch_source_set_cancel_handler_f( ctx->source, _DNSServiceSocketCancelCallback );
	dispatch_resume( ctx->source );

	*outServiceCtx = ctx;
	ctx = NULL;
	
exit:
	free( ctx );
	return( err );
}

static void _DNSServiceDispose( DNSServiceContext *inServiceCtx )
{
	dispatch_source_forget( &inServiceCtx->source );
}

//===========================================================================================================================
//	CarPlayBonjourService
//===========================================================================================================================

typedef struct CarPlayBonjourService			CarPlayBonjourService;
typedef const struct CarPlayBonjourService		*CarPlayBonjourServiceRef;

struct CarPlayBonjourService
{
	CFRuntimeBase				base;				// Must be first
	dispatch_queue_t			queue;				// Serialization queue
	CFDictionaryRef				bonjourservice;		// Bonjour service
	CFStringRef					serviceType;		// Bonjour service type
	char *						hostName;			// Service host name
	uint16_t					port;				// Service port number
	uint32_t					interfaceNdx;		// Service interface index
	OSStatus					resolutionError;	// Address resolution error
};

//===========================================================================================================================
//	_CarPlayBonjourServiceFinalize
//===========================================================================================================================

static void _CarPlayBonjourServiceFinalize( CFTypeRef inObj )
{
	CarPlayBonjourService *		service	= (CarPlayBonjourService*) inObj;
	
	dispatch_forget( &service->queue );
	ForgetCF( &service->bonjourservice );
	ForgetCF( &service->serviceType );
	ForgetMem( &service->hostName );
	
	cpcc_ulog( kLogLevelVerbose, "Finalized CarPlayBonjourService [%{ptr}]\n", inObj );
}

//===========================================================================================================================
//	CarPlayBonjourServiceGetTypeID
//===========================================================================================================================

static void _CarPlayBonjourServiceGetTypeID( void *inCtx )
{
	static const CFRuntimeClass		kCarPlayBonjourServiceClass =
	{
		0,											// version
		"CarPlayBonjourService",					// className
		NULL,										// init
		NULL,										// copy
		_CarPlayBonjourServiceFinalize,				// finalize
		NULL,										// equal -- NULL means pointer equality.
		NULL,										// hash  -- NULL means pointer hash.
		NULL,										// copyFormattingDesc
		NULL,										// copyDebugDesc
		NULL,										// reclaim
		NULL										// refcount
	};

	*( (CFTypeID*) inCtx ) = _CFRuntimeRegisterClass( &kCarPlayBonjourServiceClass );
}

static CFTypeID CarPlayBonjourServiceGetTypeID( void )
{
	static dispatch_once_t		serviceInitOnce = 0;
	static CFTypeID				serviceTypeID = _kCFRuntimeNotATypeID;
	
	dispatch_once_f( &serviceInitOnce, &serviceTypeID, _CarPlayBonjourServiceGetTypeID );
	return( serviceTypeID );
}

//===========================================================================================================================
//	_CarPlayBonjourServiceResolveReply
//===========================================================================================================================

typedef struct
{
	dispatch_semaphore_t	semaphore;
	Boolean					resolved;
	OSStatus				err;
	char *					hostname;
	uint16_t				port;
	uint32_t				interfaceNdx;
} _ResolveContext;

static void DNSSD_API
	_CarPlayBonjourServiceResolveReply(
		DNSServiceRef inServiceRef, DNSServiceFlags inFlags, uint32_t inIfNdx, DNSServiceErrorType inErr, const char* inFullName,
		const char* inTargetName, uint16_t inPort, uint16_t inTXTLen, const unsigned char* inTXTPtr, void* inCtx )
{
	_ResolveContext *		ctx = (_ResolveContext*) inCtx;
	
	(void) inServiceRef;
	(void) inFlags;
	(void) inIfNdx;
	(void) inFullName;
	(void) inTXTLen;
	(void) inTXTPtr;
	
	require_quiet( !ctx->resolved, exit );
	require_action( !inErr, exit, ctx->err = inErr );
	
	ctx->err = inErr;
	if( !ctx->err )
	{
		ctx->resolved = true;
		ctx->hostname = strdup( inTargetName );
		ctx->port = ntoh16( inPort );
		ctx->interfaceNdx = inIfNdx;
	}
	
exit:
	if( !( inFlags & kDNSServiceFlagsMoreComing ) )
		dispatch_semaphore_signal( ctx->semaphore );
}

//===========================================================================================================================
//	_CarPlayBonjourServiceResolveAddress
//===========================================================================================================================

static void _CarPlayBonjourServiceResolveAddress( void *inCtx )
{
	OSStatus					err;
	CarPlayBonjourService *		service = (CarPlayBonjourService*) inCtx;
	DNSServiceRef				dnsService = NULL;
	DNSServiceContext *			dnsServiceCtx = NULL;
	_ResolveContext				resolveCtx;
	dispatch_semaphore_t		dnsServiceSemaphore;
	long						timeout;
	char *						serviceType = NULL;
	uint32_t					ifNdx;
	char *						name = NULL;
	char *						domain = NULL;
	
	// Set up the context

	memset( &resolveCtx, 0, sizeof ( resolveCtx ) );

	dnsServiceSemaphore = dispatch_semaphore_create( 0 );
	require_action( dnsServiceSemaphore, exit, err = kNoMemoryErr );
	resolveCtx.semaphore = dnsServiceSemaphore;
	
	// Initiate the resolve

	serviceType = CFCopyCString( service->serviceType, NULL );
	ifNdx = (uint32_t) CFDictionaryGetInt64( service->bonjourservice, CFSTR( kBonjourDeviceKey_InterfaceIndex ), NULL );
	name = CFDictionaryCopyCString( service->bonjourservice, CFSTR( kBonjourDeviceKey_RawName ), NULL );
	domain = CFDictionaryCopyCString( service->bonjourservice, CFSTR( kBonjourDeviceKey_Domain ), NULL );
	require_action( name && serviceType && domain, exit, err = kNoMemoryErr );

	err = DNSServiceResolve( &dnsService, 0, ifNdx, name, serviceType, domain, _CarPlayBonjourServiceResolveReply, &resolveCtx );
	require_noerr( err, exit );

	err = DNSServiceContextCreate( &dnsServiceCtx, dnsService, NULL, NULL );
	require_noerr_action( err, exit, DNSServiceRefDeallocate( dnsService ) );
	
	// Wait for a result (or timeout)
	
	timeout = dispatch_semaphore_wait( dnsServiceSemaphore, dispatch_time( DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC ) );
	require_action( timeout == 0, exit, err = kTimeoutErr );

	// Handle the result
	
	err = resolveCtx.err;
	require_noerr( err, exit );

	ForgetMem( &service->hostName );
	service->hostName = resolveCtx.hostname;
	service->port = resolveCtx.port;
	service->interfaceNdx = resolveCtx.interfaceNdx;
	
exit:
	service->resolutionError = err;
	
	cpcc_ulog( kLogLevelNotice, "CarPlayBonjourService [%{ptr}] resolved to: %s port: %u interfaceNdx: %lu (%#m)\n",
		service, service->hostName ? service->hostName : "<null>", service->port, service->interfaceNdx, err );

	dispatch_release_null_safe( dnsServiceSemaphore );
	DNSServiceContextForget( &dnsServiceCtx );
	ForgetMem( &serviceType );
	ForgetMem( &name );
	ForgetMem( &domain );
	
	CFRelease( service );
}

static OSStatus CarPlayBonjourServiceResolveAddress( CarPlayBonjourServiceRef inService )
{
	CFRetain( inService );
	dispatch_async_f( inService->queue, (void*) inService, _CarPlayBonjourServiceResolveAddress );

	return( kNoErr );
}

//===========================================================================================================================
//	CarPlayBonjourServiceIsSameBonjourService
//===========================================================================================================================

static Boolean CarPlayBonjourServiceIsSameBonjourService( CarPlayBonjourServiceRef inService, CFDictionaryRef inBonjourService )
{
	return CFEqual( inService->bonjourservice, inBonjourService );
}

//===========================================================================================================================
//	CarPlayBonjourServiceIsSameBonjourService
//===========================================================================================================================

static Boolean CarPlayBonjourServiceIsWiFi( CarPlayBonjourServiceRef inService )
{
	return CFDictionaryGetBoolean( inService->bonjourservice, CFSTR( kBonjourDeviceKey_WiFi ), NULL );
}

//===========================================================================================================================
//	CarPlayBonjourServiceGetAddress
//===========================================================================================================================

typedef struct
{
	CarPlayBonjourServiceRef	service;
	const char *				hostName;
	uint16_t					port;
	uint32_t					interfaceNdx;
	OSStatus					err;
} _GetAddressContext;

static void _CarPlayBonjourServiceGetAddress( void *inCtx )
{
	OSStatus					err;
	_GetAddressContext *		context	= (_GetAddressContext*) inCtx;
	
	require_action( context->service->hostName, exit, err = kNotFoundErr );
	
	context->hostName = context->service->hostName;
	context->port = context->service->port;
	context->interfaceNdx = context->service->interfaceNdx;
	
	err = kNoErr;
	
exit:
	context->err = err;
}

static OSStatus
	CarPlayBonjourServiceGetAddress(
		CarPlayBonjourServiceRef inService, const char **outHostname, uint16_t *outPort, uint32_t *outInterfaceNdx )
{
	_GetAddressContext		context	= { inService, NULL, 0, 0, kNoErr };

	dispatch_sync_f( inService->queue, &context, _CarPlayBonjourServiceGetAddress );
	require_noerr( context.err, exit );
	
	*outHostname = context.hostName;
	*outPort = context.port;
	*outInterfaceNdx = context.interfaceNdx;
	
exit:
	return context.err;
}

//===========================================================================================================================
//	CarPlayBonjourServiceCreate
//===========================================================================================================================

static OSStatus
	CarPlayBonjourServiceCreate(
		CarPlayBonjourServiceRef *outController, CFDictionaryRef inService, CFStringRef inServiceType )
{
	OSStatus					err;
	CarPlayBonjourService *		obj;
	size_t						extraLen;
	
	extraLen = sizeof( *obj ) - sizeof( obj->base );
	obj = (CarPlayBonjourService*) _CFRuntimeCreateInstance( NULL, CarPlayBonjourServiceGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( obj, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) obj ) + sizeof( obj->base ), 0, extraLen );
	
	obj->queue = dispatch_queue_create( "CarPlayBonjourService", NULL );
	require_action( obj->queue, exit, err = kNoMemoryErr );
	
	obj->bonjourservice = (CFDictionaryRef) CFRetain( inService );
	obj->serviceType = (CFStringRef) CFRetain( inServiceType );
	
	err = kNoErr;
	
	cpcc_ulog( kLogLevelTrace, "Created CarPlayBonjourService [%{ptr}] for:\n%1@\n", obj, inServiceType, inService );

	*outController = obj;
	obj = NULL;
	
exit:
	CFReleaseNullSafe( obj );
	return( err );
}

//===========================================================================================================================
//	CarPlayController
//===========================================================================================================================

typedef struct CarPlayController
{
	CFRuntimeBase				base;					// Must be first
	CarPlayControlClientRef		client;					// Weak reference to creator (Not retained)
	dispatch_queue_t			internalQueue;			// Internal synchronization queue
	CFDictionaryRef				device;					// Bonjour device
	CFMutableDictionaryRef		activeServices;			// Active Bonjour services
	CFMutableDictionaryRef		inactiveWiFiServices;	// Inactive WiFi Bonjour services
} CarPlayController;

static CFDictionaryRef _CarPlayControllerCopyCurrentDevice( CarPlayControllerRef inController );

//===========================================================================================================================
//	_CarPlayControllerFinalize
//===========================================================================================================================

static void	_CarPlayControllerFinalize( CFTypeRef inObj )
{
	CarPlayController *		controller = (CarPlayController*) inObj;
	
	dispatch_forget( &controller->internalQueue );
	ForgetCF( &controller->activeServices );
	ForgetCF( &controller->inactiveWiFiServices );
	controller->client = NULL;
	
	cpcc_ulog( kLogLevelVerbose, "Finalized CarPlayController [%{ptr}]\n", inObj );
}

//===========================================================================================================================
//	CarPlayControllerGetTypeID
//===========================================================================================================================

static void _CarPlayControllerGetTypeID( void *inCtx )
{
	static const CFRuntimeClass		kCarPlayControllerClass =
	{
		0,								// version
		"CarPlayController",			// className
		NULL,							// init
		NULL,							// copy
		_CarPlayControllerFinalize,		// finalize
		NULL,							// equal -- NULL means pointer equality.
		NULL,							// hash  -- NULL means pointer hash.
		NULL,							// copyFormattingDesc
		NULL,							// copyDebugDesc
		NULL,							// reclaim
		NULL							// refcount
	};

	*( (CFTypeID*) inCtx ) = _CFRuntimeRegisterClass( &kCarPlayControllerClass );
}

CFTypeID CarPlayControllerGetTypeID( void )
{
	static dispatch_once_t		carPlayControllerInitOnce = 0;
	static CFTypeID				carPlayControllerTypeID = _kCFRuntimeNotATypeID;
	
	dispatch_once_f( &carPlayControllerInitOnce, &carPlayControllerTypeID, _CarPlayControllerGetTypeID );
	return( carPlayControllerTypeID );
}

//===========================================================================================================================
//	CarPlayControllerCreate
//===========================================================================================================================

static OSStatus _CarPlayControllerCreate( CarPlayController **outController, CarPlayControlClientRef inClient )
{
	OSStatus				err;
	CarPlayController *		obj;
	size_t					extraLen;
	
	extraLen = sizeof( *obj ) - sizeof( obj->base );
	obj = (CarPlayController*) _CFRuntimeCreateInstance( NULL, CarPlayControllerGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( obj, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) obj ) + sizeof( obj->base ), 0, extraLen );
	
	require_action( inClient, exit, err = kParamErr );
	obj->client = inClient;
	
	obj->internalQueue = dispatch_queue_create( "CarPlayController", NULL );
	require_action( obj->internalQueue, exit, err = kNoMemoryErr );
	
	obj->activeServices = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( obj->activeServices, exit, err = kNoMemoryErr );
	
	obj->inactiveWiFiServices = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( obj->inactiveWiFiServices, exit, err = kNoMemoryErr );
	
	err = kNoErr;
	
	cpcc_ulog( kLogLevelTrace, "Created controller [%{ptr}]\n", obj );

	*outController = obj;
	obj = NULL;
	
exit:
	CFReleaseNullSafe( obj );
	return( err );
}

//===========================================================================================================================
//	_CarPlayControllerDeactivateService
//===========================================================================================================================

static void _CarPlayControllerDeactivateService( const void *inService, void *inCtx )
{
	CarPlayControllerRef			controller = (CarPlayControllerRef) inCtx;
	CFDictionaryRef					bonjourService = (CFDictionaryRef) inService;
	CFTypeRef						serviceInterfaceNdx;
	CarPlayBonjourServiceRef		service;
	Boolean							wifi;
	
	serviceInterfaceNdx = CFDictionaryGetValue( bonjourService, CFSTR( kBonjourDeviceKey_InterfaceIndex ) );
	service = CFDictionaryGetValue( controller->activeServices, serviceInterfaceNdx );
	check( service && CarPlayBonjourServiceIsSameBonjourService( service, bonjourService ) );
	
	wifi = CFDictionaryGetBoolean( bonjourService, CFSTR( kBonjourDeviceKey_WiFi ), NULL );
	
	// Remove the service from the active list and (if it is a WiFi service) add it to the inactive service list
	
	CFRetain( service );

	CFDictionaryRemoveValue( controller->activeServices, serviceInterfaceNdx );
	
	if( wifi )
		CFDictionarySetValue( controller->inactiveWiFiServices, serviceInterfaceNdx, service );
	
	cpcc_ulog( kLogLevelNotice, "Controller [%{ptr}] %s Service [%{ptr}]\n",
		 controller, wifi ? "Deactivated" : "Removed", service );
	
	CFRelease( service );
}

//===========================================================================================================================
//	_CarPlayControllerActivateService
//===========================================================================================================================

static void _CarPlayControllerActivateService( const void *inService, void *inCtx )
{
	OSStatus						err;
	CarPlayControllerRef			controller = (CarPlayControllerRef) inCtx;
	CFDictionaryRef					bonjourService = (CFDictionaryRef) inService;
	CFTypeRef						serviceInterfaceNdx;
	CarPlayBonjourServiceRef		service;
	
	serviceInterfaceNdx = CFDictionaryGetValue( bonjourService, CFSTR( kBonjourDeviceKey_InterfaceIndex ) );
	require_quiet( !CFDictionaryContainsKey( controller->activeServices, serviceInterfaceNdx ), exit );

	// If we have an inactive service on the same interface, move it to the active list (we'll validate it below)
	
	service = CFDictionaryGetValue( controller->inactiveWiFiServices, serviceInterfaceNdx );
	if( service )
	{
		CFRetain( service );
		CFDictionaryRemoveValue( controller->inactiveWiFiServices, serviceInterfaceNdx );
		CFDictionarySetValue( controller->activeServices, serviceInterfaceNdx, service );
		CFRelease( service );
	}

	// Verify if we have a existing and valid active service
	
	service = CFDictionaryGetValue( controller->activeServices, serviceInterfaceNdx );
	if( !service || !CarPlayBonjourServiceIsSameBonjourService( service, bonjourService ) )
	{
		err = CarPlayBonjourServiceCreate( &service, bonjourService,
					CFDictionaryGetCFString( controller->device, CFSTR( kBonjourDeviceKey_ServiceType ), NULL ) );
		if( err ) service = NULL;
		
		CFDictionarySetValue( controller->activeServices, serviceInterfaceNdx, service );
	}
	
	// Resolve the service (if we are resurrecting from the inactive list we still do this in case the address / port number changed)
	
	if( service )
	{
		CarPlayBonjourServiceResolveAddress( service );
	}
	
	cpcc_ulog( kLogLevelNotice, "Controller [%{ptr}] Activated Service [%{ptr}]\n", controller, service );
	
exit:
	return;
}

//===========================================================================================================================
//	_CarPlayControllerUpdateDevice
//===========================================================================================================================

typedef struct
{
	CarPlayController *		controller;
	CFDictionaryRef			device;
	CFIndex					newServiceCount;
}
_UpdateDeviceContext;

static void _CarPlayControllerUpdateInternal( void *inCtx )
{
	_UpdateDeviceContext *		ctx = (_UpdateDeviceContext*) inCtx;
	CFDictionaryRef				oldDevice;
	CFArrayRef					removedServices;
	CFArrayRef					deviceServices;
	
	check( ctx->device );
	removedServices = CFDictionaryGetCFArray( ctx->device, CFSTR( kBonjourDeviceKey_RemovedServices ), NULL );
	deviceServices = CFDictionaryGetCFArray( ctx->device, CFSTR( kBonjourDeviceKey_Services ), NULL );

	oldDevice = ctx->controller->device;
	CFRetainNullSafe( oldDevice );
	
	if( ctx->device )	BonjourDevice_MergeInfo( &ctx->controller->device, ctx->device );
	else				BonjourDevice_RemoveInterfaceInfo( &ctx->controller->device, "", true );
	
	// Deactivate removed services first

	if( removedServices )
	{
		CFArrayApplyFunction( removedServices, CFRangeMake( 0, CFArrayGetCount( removedServices ) ),
			_CarPlayControllerDeactivateService, (void*) ctx->controller );
	}
	
	// Activate new / existing services

	if( deviceServices )
	{
		CFArrayApplyFunction( deviceServices, CFRangeMake( 0, CFArrayGetCount( deviceServices ) ),
			_CarPlayControllerActivateService, (void*) ctx->controller );
	}
	
	// Count the services
	
	ctx->newServiceCount = CFDictionaryGetCount( ctx->controller->activeServices ) +
							CFDictionaryGetCount( ctx->controller->inactiveWiFiServices );
	
	// Clean up
	
	CFReleaseNullSafe( oldDevice );
}

static void	_CarPlayControllerUpdate( CarPlayController *inController, CFDictionaryRef inDevice, CFIndex *outServiceCount )
{
	_UpdateDeviceContext		ctx = { inController, inDevice, 0 };
	
	require( inController, exit );
	
	dispatch_sync_f( inController->internalQueue, &ctx, _CarPlayControllerUpdateInternal );
	
	if( outServiceCount )
		*outServiceCount = ctx.newServiceCount;

exit:
	return;
}

//===========================================================================================================================
//	_CarPlayControllerCopyBestService
//===========================================================================================================================

typedef struct
{
	CarPlayControllerRef		controller;
	CarPlayBonjourServiceRef	bestService;
	Boolean						bestServiceIsActive;
	OSStatus					err;
} _CopyBestServiceContext;

static void _CarPlayControllerCopyBestService( void *inCtx )
{
	_CopyBestServiceContext *		ctx = (_CopyBestServiceContext*) inCtx;
	CFArrayRef						serviceKeys;
	CFIndex							serviceNdx, serviceCount;
	CarPlayBonjourServiceRef		carPlayService;
	Boolean							active = false;
	
	serviceKeys = CFDictionaryCopyKeys( ctx->controller->activeServices, NULL );
	require_action( serviceKeys, exit, ctx->err = kNoMemoryErr );
	
	// Pick the best active service (wired beats wireless)
	
	for( serviceNdx = 0, serviceCount = CFArrayGetCount( serviceKeys ); serviceNdx < serviceCount; ++serviceNdx )
	{
		carPlayService = CFDictionaryGetValue( ctx->controller->activeServices,
								CFArrayGetValueAtIndex( serviceKeys, serviceNdx ) );
		if( !CarPlayBonjourServiceIsWiFi( carPlayService ) )
		{
			ReplaceCF( &ctx->bestService, carPlayService );
			active = true;
			break;
		}
		else if( !ctx->bestService )
		{
			ctx->bestService = CFRetain( carPlayService );
			active = true;
		}
	}
	ForgetCF( &serviceKeys );
	
	// If there were no active services, then choose any inactive wireless service
	
	if( !ctx->bestService )
	{
		serviceKeys = CFDictionaryCopyKeys( ctx->controller->inactiveWiFiServices, NULL );
		require_action( serviceKeys, exit, ctx->err = kNoMemoryErr );
		
		if( CFArrayGetCount( serviceKeys ) > 0 )
		{
			carPlayService = CFDictionaryGetValue( ctx->controller->inactiveWiFiServices,
									CFArrayGetValueAtIndex( serviceKeys, serviceNdx ) );
			ctx->bestService = CFRetain( carPlayService );
		}
		ForgetCF( &serviceKeys );
	}

	cpcc_ulog( kLogLevelTrace, "Controller [%{ptr}] found best Service [%{ptr}] (%s) from:\nActiveServices:%@\nInactiveServices:%@\n",
		ctx->controller, ctx->bestService, ctx->bestService ? ( active ? "Active" : "Inactive" ) : "N/A",
		ctx->controller->activeServices, ctx->controller->inactiveWiFiServices );
	require_action_quiet( ctx->bestService, exit, ctx->err = kNotFoundErr );
	
	ctx->bestServiceIsActive = active;
	
exit:
	return;
}

static OSStatus
	CarPlayControllerCopyBestService(
		CarPlayControllerRef inController, CarPlayBonjourServiceRef *outBestService, Boolean *outBestServiceIsActive )
{
	_CopyBestServiceContext		ctx = { inController, NULL, false, kNoErr };
	
	dispatch_sync_f( inController->internalQueue, &ctx, _CarPlayControllerCopyBestService );
	require_noerr( ctx.err, exit );
	
	*outBestService = ctx.bestService;
	*outBestServiceIsActive = ctx.bestServiceIsActive;
	
exit:
	return ctx.err;
}

//===========================================================================================================================
//	_CarPlayControllerCopyCurrentDevice
//===========================================================================================================================

typedef struct
{
	CarPlayControllerRef	controller;
	CFDictionaryRef *		device;
} _CopyCurrentDeviceContext;

static void _CarPlayControllerCopyCurrentDeviceInternal( void *inCtx )
{
	_CopyCurrentDeviceContext		*ctx = (_CopyCurrentDeviceContext*) inCtx;
	*ctx->device = (CFDictionaryRef) CFRetain( ctx->controller->device );
}

static CFDictionaryRef _CarPlayControllerCopyCurrentDevice( CarPlayControllerRef inController )
{
	_CopyCurrentDeviceContext		ctx;
	CFDictionaryRef					device;
	
	ctx.controller = inController;
	ctx.device = &device;
	
	dispatch_sync_f( inController->internalQueue, &ctx, _CarPlayControllerCopyCurrentDeviceInternal );
	
	return( device );
}

//===========================================================================================================================
//	CarPlayControllerCopyName
//===========================================================================================================================

OSStatus CarPlayControllerCopyName( CarPlayControllerRef inController, CFStringRef *outName )
{
	OSStatus			err;
	CFDictionaryRef		device = NULL;
	CFStringRef			name;
	
	require_action( inController, exit, err = kParamErr );
	require_action( outName, exit, err = kParamErr );
	
	device = _CarPlayControllerCopyCurrentDevice( inController );
	name = CFDictionaryGetCFString( device, CFSTR( kBonjourDeviceKey_Name ), NULL );
	require_action( name, exit, err = kUnknownErr );
	
	*outName = (CFStringRef) CFRetain( name );
	err = kNoErr;
	
exit:
	CFRelease( device );
	return( err );
}

//===========================================================================================================================
//	CarPlayControllerCopyDNSName
//===========================================================================================================================

OSStatus CarPlayControllerCopyDNSName( CarPlayControllerRef inController, CFStringRef *outIfName )
{
	OSStatus			err;
	CFDictionaryRef		device = NULL;
	CFStringRef			name;
	
	require_action( inController, exit, err = kParamErr );
	require_action( outIfName, exit, err = kParamErr );
	
	device = _CarPlayControllerCopyCurrentDevice( inController );
	name = CFDictionaryGetCFString( device, CFSTR( kBonjourDeviceKey_DNSName ), NULL );
	require_action( name, exit, err = kUnknownErr );
	
	*outIfName = (CFStringRef) CFRetain( name );
	err = kNoErr;
	
exit:
	CFRelease( device );
	return( err );
}

//===========================================================================================================================
//	CarPlayControllerGetBluetoothMacAddress
//===========================================================================================================================

OSStatus CarPlayControllerGetBluetoothMacAddress( CarPlayControllerRef inController, uint8_t outAddress[ 6 ] )
{
	OSStatus			err;
	CFDictionaryRef		device = NULL;
	
	require_action( inController, exit, err = kParamErr );
	
	device = _CarPlayControllerCopyCurrentDevice( inController );
	BonjourDevice_GetDeviceID( device, outAddress, &err );
	require_noerr( err, exit );
	
exit:
	CFRelease( device );
	return( err );
}

//===========================================================================================================================
//	CarPlayControllerCopySourceVersion
//===========================================================================================================================

OSStatus	CarPlayControllerCopySourceVersion( CarPlayControllerRef inController, CFStringRef *outSourceVersion )
{
	OSStatus			err;
	CFDictionaryRef		device = NULL;
	CFStringRef			sourceVersion = NULL;
	
	require_action( inController, exit, err = kParamErr );
	require_action( outSourceVersion, exit, err = kParamErr );
	
	device = _CarPlayControllerCopyCurrentDevice( inController );
	sourceVersion = BonjourDevice_CopyCFString( device, kAPSCarPlayControlTxtRecordKey_SourceVersion, &err );
	require_noerr( err, exit );
	
	*outSourceVersion = sourceVersion;
	sourceVersion = NULL;
	
exit:
	CFRelease( device );
	CFReleaseNullSafe( sourceVersion );

	return( err );
}


//===========================================================================================================================
//	_CarPlayControllerForgetInactiveServices
//===========================================================================================================================

typedef struct
{
	CarPlayControllerRef	controller;
	CFIndex					serviceCount;
} _ForgetInactiveServicesContext;

static void _CarPlayControllerForgetInactiveServicesInternal( void *inCtx )
{
	_ForgetInactiveServicesContext *		context =	(_ForgetInactiveServicesContext*) inCtx;
	
	CFDictionaryRemoveAllValues( context->controller->inactiveWiFiServices );
	context->serviceCount = CFDictionaryGetCount( context->controller->activeServices ) +
								CFDictionaryGetCount( context->controller->inactiveWiFiServices );
}

static void _CarPlayControllerForgetInactiveServices( CarPlayControllerRef inController, CFIndex *outServiceCount )
{
	_ForgetInactiveServicesContext		context	= { inController, 0 };
	
	dispatch_sync_f( inController->internalQueue, &context, _CarPlayControllerForgetInactiveServicesInternal );
	
	if( outServiceCount )
		*outServiceCount = context.serviceCount;
}

//===========================================================================================================================
//	CarPlayControlClient
//===========================================================================================================================

typedef struct CarPlayControlClient
{
	CFRuntimeBase					base;				// CF type info. Must be first.
	dispatch_queue_t				internalQueue;
	dispatch_queue_t				httpQueue;

	dispatch_queue_t				eventQueue;
	CarPlayControlClientCallback	eventCallback;
	void *							eventCtx;
	
	Boolean							started;
	BonjourBrowserRef				browser;
	CFMutableArrayRef				controllers;
	
	AirPlayReceiverServerRef		server;
	uint64_t						deviceID;
} CarPlayControlClient;

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static void _CarPlayControlClientEnsureStarted( void *inCtx );
static void _CarPlayControlClientEnsureStopped( void *inCtx );
static void _CarPlayControlClientPostEvent( CarPlayControlClientRef inClient, CarPlayControlClientEvent inEvent, const void *inEventInfo );
static void _CarPlayControlClientBrowseCallback( BonjourBrowserEventType inEventType, CFDictionaryRef inEventInfo, void *inCtx );

//===========================================================================================================================
//	_CarPlayControlClientFinalize
//===========================================================================================================================

static void	_CarPlayControlClientFinalize( CFTypeRef inCF )
{
	CarPlayControlClient *		client = (CarPlayControlClient*) inCF;
	
	cpcc_ulog( kLogLevelTrace, "CarPlayControlClient %p Finalized\n", inCF );

	dispatch_forget( &client->internalQueue );
	dispatch_forget( &client->httpQueue );
	dispatch_forget( &client->eventQueue );
	check( !client->browser );
	ForgetCF( &client->controllers );
	ForgetCF( &client->server );
}

//===========================================================================================================================
//	_CarPlayControlClientGetTypeID
//===========================================================================================================================

static void _CarPlayControlClientGetTypeID( void *inCtx )
{
	static const CFRuntimeClass		kCarPlayControlClientClass =
	{
		0,								// version
		"CarPlayControlClient",			// className
		NULL,							// init
		NULL,							// copy
		_CarPlayControlClientFinalize,	// finalize
		NULL,							// equal -- NULL means pointer equality.
		NULL,							// hash  -- NULL means pointer hash.
		NULL,							// copyFormattingDesc
		NULL,							// copyDebugDesc
		NULL,							// reclaim
		NULL							// refcount
	};

	*( (CFTypeID*) inCtx ) = _CFRuntimeRegisterClass( &kCarPlayControlClientClass );
}

CFTypeID CarPlayControlClientGetTypeID( void )
{
	static dispatch_once_t		carPlayControlClientInitOnce = 0;
	static CFTypeID				carPlayControlClientTypeID = _kCFRuntimeNotATypeID;
	
	dispatch_once_f( &carPlayControlClientInitOnce, &carPlayControlClientTypeID, _CarPlayControlClientGetTypeID );
	return( carPlayControlClientTypeID );
}

//===========================================================================================================================
//	CarPlayControlClientCreate
//===========================================================================================================================

OSStatus CarPlayControlClientCreateWithServer(
			CarPlayControlClientRef * outClient, AirPlayReceiverServerRef inServer,
			CarPlayControlClientCallback inCallback, void * inCtx )
{
	OSStatus					err;
	CarPlayControlClient *		obj = NULL;
	size_t						extraLen;
	
	require_action( outClient, exit, err = kParamErr );

	extraLen = sizeof( *obj ) - sizeof( obj->base );	
	obj = (CarPlayControlClient*) _CFRuntimeCreateInstance( NULL, CarPlayControlClientGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( obj, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) obj ) + sizeof( obj->base ), 0, extraLen );
	
	obj->internalQueue = dispatch_queue_create( "CarPlayControlClient Internal", 0 );
	require_action( obj->internalQueue, exit, err = kNoMemoryErr );
	
	obj->httpQueue = dispatch_queue_create( "CarPlayControlClient http", 0 );
	require_action( obj->httpQueue, exit, err = kNoMemoryErr );

	require_action( inCallback, exit, err = kParamErr );
	obj->eventCallback = inCallback;
	obj->eventCtx = inCtx;
	obj->eventQueue = dispatch_queue_create( "CarPlayControlClient Event", 0 );
	require_action( obj->eventQueue, exit, err = kNoMemoryErr );
	
	obj->server = inServer;
	CFRetainNullSafe( obj->server );
	require_action( obj->server, exit, err = kParamErr );
	
	obj->controllers = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( obj->controllers, exit, err = kNoMemoryErr );
	
	*outClient = obj;
	obj = NULL;
	
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( obj );
	return( err );
}

//===========================================================================================================================
//	CarPlayControlClientStart
//===========================================================================================================================

OSStatus CarPlayControlClientStart( CarPlayControlClientRef inClient )
{
	OSStatus		err;
	
	require_action( inClient, exit, err = kParamErr );
	
	CFRetain( inClient );
	dispatch_async_f( inClient->internalQueue, (void*) inClient, _CarPlayControlClientEnsureStarted );
	err = kNoErr;
	
	cpcc_ulog( kLogLevelNotice, "CarPlayControlClientStart: %#m\n", err );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_CarPlayControlClientEnsureStarted
//===========================================================================================================================

static void _CarPlayControlClientEnsureStarted( void *inCtx )
{
	OSStatus					err;
	CarPlayControlClient *		client = (CarPlayControlClient*) inCtx;
	
	require_action_quiet( !client->started, exit, err = kNoErr );
	
	if( client->deviceID == 0 )
		client->deviceID = AirPlayGetDeviceID( NULL );

	check( !client->browser );
	err = BonjourBrowser_Create( &client->browser, "CarPlayControlClient" );
	require_noerr( err, exit );
		
	BonjourBrowser_SetDispatchQueue( client->browser, client->internalQueue );
	BonjourBrowser_SetEventHandler( client->browser, _CarPlayControlClientBrowseCallback, client );
	err = BonjourBrowser_Start( client->browser,
			kAPSCarPlayControlBonjourServiceType, kAPSCarPlayControlBonjourServiceDomain, NULL, kBonjourBrowserFlag_StandardID );
	require_noerr( err, exit );
	
	CFRetain( client ); // Released on stop event
	client->started = true;
	
exit:
	cpcc_ulog( kLogLevelTrace, "_CarPlayControlClientEnsureStarted: %#m\n", err );
	if( err )
	{
		BonjourBrowser_Forget( &client->browser );
		_CarPlayControlClientPostEvent( client, kCarPlayControlClientEvent_Stopped, &err );
	}
	CFRelease( client );
}

//===========================================================================================================================
//	CarPlayControlClientStop
//===========================================================================================================================

OSStatus CarPlayControlClientStop( CarPlayControlClientRef inClient )
{
	OSStatus		err;
	
	require_action( inClient, exit, err = kParamErr );
	
	CFRetain( inClient );
	dispatch_async_f( inClient->internalQueue, (void*) inClient, _CarPlayControlClientEnsureStopped );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_CarPlayControlClientEnsureStopped
//===========================================================================================================================

static void	_CarPlayControlClientEnsureStopped( void *inCtx )
{
	CarPlayControlClient *		client = (CarPlayControlClient*) inCtx;

	require_quiet( client->started, exit );
	BonjourBrowser_Stop( client->browser );

exit:
	CFRelease( client );
}

//===========================================================================================================================
//	_CarPlayControlClientPostEvent
//===========================================================================================================================

typedef struct
{
	CarPlayControlClientRef		client;
	CarPlayControlClientEvent	event;
	CarPlayControllerRef		controller;
	OSStatus					err;
	const void *				eventInfo;
} CallbackContext;

static void _CarPlayControlClientCallEventCallback( void *inCtx )
{
	CallbackContext *		ctx = (CallbackContext*) inCtx;
	
	cpcc_ulog( kLogLevelTrace, "Posting event: %s\n", CarPlayControlEventToString( ctx->event ) );
	
	ctx->client->eventCallback( ctx->client, ctx->event, (void*) ctx->eventInfo, ctx->client->eventCtx );
	CFReleaseNullSafe( ctx->controller );
	CFReleaseNullSafe( ctx->client );
	free( ctx );
}

static void _CarPlayControlClientPostEvent( CarPlayControlClientRef inClient, CarPlayControlClientEvent inEvent, const void *inEventInfo )
{
	CallbackContext *		callbackCtx;
	
	callbackCtx = calloc( 1, sizeof( CallbackContext ) );
	require( callbackCtx, exit );
	callbackCtx->client = (CarPlayControlClientRef) CFRetain( inClient );
	callbackCtx->event = inEvent;
	
	switch( inEvent )
	{
		case kCarPlayControlClientEvent_AddOrUpdateController:
		case kCarPlayControlClientEvent_RemoveController:
			callbackCtx->controller = (CarPlayControllerRef) CFRetain( (CarPlayControllerRef) inEventInfo );
			callbackCtx->eventInfo = callbackCtx->controller;
			break;
		case kCarPlayControlClientEvent_Stopped:
			callbackCtx->err = inEventInfo ? *( (OSStatus*) inEventInfo ) : kNoErr;
			callbackCtx->eventInfo = &callbackCtx->err;
			break;
		default:
			check( "Unhandled CarPlayControlClientEvent" == 0 );
			break;
	}

	dispatch_async_f( inClient->eventQueue, callbackCtx, _CarPlayControlClientCallEventCallback );
	
exit:
	return;
}

//===========================================================================================================================
//	_CarPlayControlClientHandleBonjourAddOrUpdate
//===========================================================================================================================

static void _CarPlayControlClientHandleBonjourAddOrUpdate( CarPlayControlClientRef inClient, CFDictionaryRef inEventInfo )
{
	uint64_t				deviceID;
	CFIndex					ndx, count;
	CarPlayController *		controller;
	
	// See if we have an existing entry to update
	
	controller = NULL;
	deviceID = BonjourDevice_GetDeviceID( inEventInfo, NULL, NULL );
	for( ndx = 0, count = CFArrayGetCount( inClient->controllers ); ndx < count; ++ndx )
	{
		controller = (CarPlayController*) CFArrayGetValueAtIndex( inClient->controllers, ndx );
		if( BonjourDevice_GetDeviceID( controller->device, NULL, NULL ) == deviceID )
		{
			CFRetain( controller );
			break;
		}
		else
		{
			controller = NULL;
		}
	}

	// Otherwise create new
	
	if( !controller )
	{
		_CarPlayControllerCreate( &controller, inClient );
		if( controller )
			CFArrayAppendValue( inClient->controllers, controller );
	}
	
	// Update the controller and notify our client
	
	if( controller )
	{
		_CarPlayControllerUpdate( controller, inEventInfo, NULL );
		_CarPlayControlClientPostEvent( inClient, kCarPlayControlClientEvent_AddOrUpdateController, controller );
		CFRelease( controller );
	}
}

//===========================================================================================================================
//	_CarPlayControlClientRemoveController
//===========================================================================================================================

static void _CarPlayControlClientRemoveController( CarPlayControlClientRef inClient, CarPlayControllerRef inController )
{
	CFIndex			ndx;
	
	// Find the controller and remove it if there are no more services

	CFRetain( inController );
	
	ndx = CFArrayGetFirstIndexOfValue( inClient->controllers, CFRangeMake( 0, CFArrayGetCount( inClient->controllers ) ), inController );
	check( ndx != kCFNotFound );
	
	if( ndx != kCFNotFound )
	{
		// Remove and notify our client
		CFArrayRemoveValueAtIndex( inClient->controllers, ndx );
		_CarPlayControlClientPostEvent( inClient, kCarPlayControlClientEvent_RemoveController, inController );
	}
	
	CFRelease( inController );
}

//===========================================================================================================================
//	_CarPlayControlClientHandleBonjourRemove
//===========================================================================================================================

static void _CarPlayControlClientHandleBonjourRemove( CarPlayControlClientRef inClient, CFDictionaryRef inEventInfo )
{

	uint64_t				deviceID;
	CFIndex					ndx, count, serviceCount;
	CarPlayController *		controller;
	
	// Find the controller and updated it
	controller = NULL;
	deviceID = BonjourDevice_GetDeviceID( inEventInfo, NULL, NULL );
	for( ndx = 0, count = CFArrayGetCount( inClient->controllers ); ndx < count; ++ndx )
	{
		controller = (CarPlayController*) CFArrayGetValueAtIndex( inClient->controllers, ndx );
		if( BonjourDevice_GetDeviceID( controller->device, NULL, NULL ) == deviceID )
		{
			CFRetain( controller );
			_CarPlayControllerUpdate( controller, inEventInfo, &serviceCount );

			// Remove the device if there are no more services
			if( serviceCount == 0 )
				_CarPlayControlClientRemoveController( inClient, controller );

			break;
		}
	}
}

//===========================================================================================================================
//	_CarPlayControlClientHandleBonjourStopOrRestart
//===========================================================================================================================

static void _CarPlayControlClientHandleBonjourStopOrRestart( CarPlayControlClient * inClient, BonjourBrowserEventType inEventType )
{

	CFIndex					ndx, count;
	CarPlayController *		controller;
	
	for( ndx = 0, count = CFArrayGetCount( inClient->controllers ); ndx < count; ++ndx )
	{
		controller = (CarPlayController*) CFArrayGetValueAtIndex( inClient->controllers, ndx );
		_CarPlayControllerUpdate( controller, NULL, NULL );
		
		// Notify client of removal if restarting
		if( inEventType == kBonjourBrowserEventType_Restarted )
			_CarPlayControlClientPostEvent( inClient, kCarPlayControlClientEvent_RemoveController, controller );
	}
	CFArrayRemoveAllValues( inClient->controllers );
	
	// Notify client of stop
	if( inEventType == kBonjourBrowserEventType_Stop )
	{
		_CarPlayControlClientPostEvent( inClient, kCarPlayControlClientEvent_Stopped, NULL );
		inClient->started = false;
		BonjourBrowser_Forget( &inClient->browser );
		CFRelease( inClient ); // Balance retain in _CarPlayControlClientEnsureStarted
	}
}

//===========================================================================================================================
//	_CarPlayControlClientBrowseCallback
//===========================================================================================================================

static void _CarPlayControlClientBrowseCallback( BonjourBrowserEventType inEventType, CFDictionaryRef inEventInfo, void *inCtx )
{

	CarPlayControlClient *		client = (CarPlayControlClient*) inCtx;
	
	cpcc_ulog( kLogLevelTrace, "BrowseCallback: event=%s, info=%@\n", BonjourBrowserEventTypeToString(inEventType), inEventInfo );
	
	if( inEventType == kBonjourBrowserEventType_AddOrUpdateDevice )
	{
		_CarPlayControlClientHandleBonjourAddOrUpdate( client, inEventInfo );
	}
	else if( inEventType == kBonjourBrowserEventType_RemoveDevice )
	{
		_CarPlayControlClientHandleBonjourRemove( client, inEventInfo );
	}
	else if( inEventType == kBonjourBrowserEventType_Restarted || inEventType == kBonjourBrowserEventType_Stop )
	{
		_CarPlayControlClientHandleBonjourStopOrRestart( client, inEventType );
	}
}

//===========================================================================================================================
//	_CarPlayControlClientSendCommand
//===========================================================================================================================

typedef struct
{
	CarPlayControllerRef	controller;
	const char *			command;
	OSStatus				err;
} _SendCommandContext;

static OSStatus
	_CarPlayControlClientSendCommand(
		CarPlayControlClientRef inClient, CarPlayBonjourServiceRef inService, const char *inCommand )
{
	OSStatus			err;
	const char *		hostName = NULL;
	char *				hostNameWithInterfaceNdx = NULL;
	uint16_t			port = 0;
	uint32_t			interfaceNdx = 0;
	HTTPClientRef		httpClient = NULL;
	HTTPClientFlags		httpFlags;
	HTTPMessageRef		httpMessage = NULL;
	
	// Get the service address
	
	err = CarPlayBonjourServiceGetAddress( inService, &hostName, &port, &interfaceNdx );
	require_noerr( err, exit );
	
	ASPrintF( &hostNameWithInterfaceNdx, "%s%%%lu", hostName, interfaceNdx );

	// Connect to the controller
	
	cpcc_ulog( kLogLevelNotice, "CarPlayControl connecting to %s on port %d\n", hostNameWithInterfaceNdx, (int) port );
	err = HTTPClientCreate( &httpClient );
	require_noerr( err, exit );
	
	HTTPClientSetDispatchQueue( httpClient, inClient->httpQueue );
	HTTPClientSetTimeout( httpClient, kCarPlayControlClient_ConnectionTimeoutSeconds );
	
	HTTPClientSetLogging( httpClient, &log_category_from_name( CarPlayControlClient ) );
	HTTPClientSetConnectionLogging( httpClient, &log_category_from_name( CarPlayControlClient ) );
	
	err = HTTPClientSetDestination( httpClient, hostNameWithInterfaceNdx, port );
	require_noerr( err, exit );
	
	httpFlags = kHTTPClientFlag_Reachability;
	if( CarPlayBonjourServiceIsWiFi( inService ) ) httpFlags |= kHTTPClientFlag_NonLinkLocal;
	HTTPClientSetFlags( httpClient, httpFlags, httpFlags );
	
	// Build the request
	
	err = HTTPMessageCreate( &httpMessage );
	require_noerr( err, exit );
	
	err = HTTPMessageInitRequest( httpMessage, kHTTPVersionString_1pt1, kHTTPMethodString_GET, "/ctrl-int/1/%s", inCommand );
	require_noerr( err, exit );
	err = HTTPMessageSetHeaderField( httpMessage, kHTTPHeader_Host, "%s", hostName );
	require_noerr( err, exit );
	err = HTTPMessageSetHeaderField( httpMessage, kHTTPHeader_UserAgent, "%s", kAirPlayUserAgentStr );
	require_noerr( err, exit );
	err = HTTPMessageSetHeaderField( httpMessage, "AirPlay-Receiver-Device-ID", "%llu", inClient->deviceID );
	require_noerr( err, exit );
	
	cpcc_ulog( kLogLevelTrace, "********** CarPlayControl Request **********\n" );
	cpcc_ulog( kLogLevelTrace, "%@\n", httpMessage );
	
	// send the request
	
	err = HTTPClientSendMessageSync( httpClient, httpMessage );
	require_noerr( err, exit );

	// Read the response header
	
	require_action( httpMessage->header.statusCode != 0, exit, err = kResponseErr );
	require_action( IsHTTPStatusCode_Success( httpMessage->header.statusCode ), exit, err = HTTPStatusToOSStatus( httpMessage->header.statusCode ) );
		
	cpcc_ulog( kLogLevelTrace, "********** CarPlayControl Response **********\n" );
	cpcc_ulog( kLogLevelTrace, "%@\n", httpMessage );
	
exit:
	if( err )	cpcc_ulog( kLogLevelVerbose, "CarPlayControl command \"%s\" failed    @ %N: %#m\n", inCommand, err );
	else		cpcc_ulog( kLogLevelVerbose, "CarPlayControl command \"%s\" completed @ %N\n", inCommand);
	
	ForgetMem( &hostNameWithInterfaceNdx );
	HTTPClientForget( &httpClient );
	CFReleaseNullSafe( httpMessage );
	
	return( err );
}

//===========================================================================================================================
//	CarPlayControlClientConnect
//===========================================================================================================================

static void _CarPlayControlClientConnect( void *inCtx )
{
	_SendCommandContext *			ctx = (_SendCommandContext*) inCtx;
	CarPlayControlClientRef			client = ctx->controller->client;
	CarPlayBonjourServiceRef		carPlayService = NULL;
	Boolean							carPlayServiceIsActive = false;
	CFIndex							serviceCount;
	
	require_action( client->started, exit, ctx->err = kStateErr );
	
	ctx->err = CarPlayControllerCopyBestService( ctx->controller, &carPlayService, &carPlayServiceIsActive );
	require_noerr( ctx->err, exit );
	
	ctx->err = _CarPlayControlClientSendCommand( client, carPlayService, ctx->command );
	require_noerr( ctx->err, exit );
	
exit:
	if( ctx->err != kNoErr )
	{
		if( carPlayServiceIsActive )
		{
			// Kick of a new resolve, in case the address or port number changed
			
			CarPlayBonjourServiceResolveAddress( carPlayService );
			
			// Also try reconfirming the device

			CFDictionaryRef device = _CarPlayControllerCopyCurrentDevice( ctx->controller );
			BonjourBrowser_ReconfirmDevice( client->browser, device );
			CFRelease( device );
			
		}
		else
		{
			// Probably gone for good, so remove the inactive services
			
			_CarPlayControllerForgetInactiveServices( ctx->controller, &serviceCount );
			
			if( serviceCount == 0 )
				_CarPlayControlClientRemoveController( client, ctx->controller );

		}
	}
}

OSStatus CarPlayControlClientConnect( CarPlayControlClientRef inClient, CarPlayControllerRef inController )
{
	_SendCommandContext		commandCtx;
	
	memset( &commandCtx, 0, sizeof( commandCtx ) );	
	require_action( inClient && inController && inController->client == inClient, exit, commandCtx.err = kParamErr );

	commandCtx.controller = inController;
	commandCtx.command = kAPSCarPlayControlCommandStr_Connect;
	
	dispatch_sync_f( inClient->internalQueue, &commandCtx, _CarPlayControlClientConnect );
	
exit:
	return( commandCtx.err );
}

//===========================================================================================================================
//	CarPlayControlClientDisconnect
//===========================================================================================================================

static void _CarPlayControlClientDisconnect( void *inCtx )
{
	_SendCommandContext *		ctx = (_SendCommandContext*) inCtx;
	CarPlayControlClientRef		client = ctx->controller->client;
	
	require_action( client->started, exit, ctx->err = kStateErr );
	require_action( client->server, exit, ctx->err = kInternalErr );
	
	ctx->err = AirPlayReceiverServerControlAsync( client->server, CFSTR( kAirPlayCommand_SessionDied ),
					NULL, NULL, NULL, NULL, NULL );
	require_noerr( ctx->err, exit );
	
exit:
	;
}

OSStatus CarPlayControlClientDisconnect( CarPlayControlClientRef inClient, CarPlayControllerRef inController )
{
	_SendCommandContext		commandCtx;
	
	memset( &commandCtx, 0, sizeof( commandCtx ) );
	require_action( inClient && inController && inController->client == inClient, exit, commandCtx.err = kParamErr );
	
	commandCtx.controller = inController;
	commandCtx.command = NULL;
	
	dispatch_sync_f( inClient->internalQueue, &commandCtx, _CarPlayControlClientDisconnect );
	
exit:
	return( commandCtx.err );
}

