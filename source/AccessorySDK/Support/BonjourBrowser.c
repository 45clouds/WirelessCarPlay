/*
	File:    	BonjourBrowser.c
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	410.8
	
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
	
	Copyright (C) 2010-2015 Apple Inc. All Rights Reserved.
*/

#include "BonjourBrowser.h"

#include <dns_sd.h>

#include "CFUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "NetUtils.h"
#include "PrintFUtils.h"
#include "StringUtils.h"
#include "TickUtils.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

#if( TARGET_OS_POSIX )
	#include <net/if.h>
#endif
#include <glib.h>
//===========================================================================================================================
//	Types
//===========================================================================================================================

#if( TARGET_OS_DARWIN )
	#define BONJOUR_USE_GCD		1
#else
	#define BONJOUR_USE_GCD		0
#endif

typedef struct BonjourDevice *		BonjourDeviceRef;
typedef struct BonjourRData *		BonjourRDataRef;
typedef struct BonjourService *		BonjourServiceRef;

// DNSServiceContext

#if( !BONJOUR_USE_GCD )
typedef struct
{
	BonjourBrowserRef		browser;
	int						dnsFD;
	DNSServiceRef			dnsService;
	dispatch_source_t		dnsSource;
	
}	DNSServiceContext;
#endif

// BonjourBrowser

struct BonjourBrowser
{
	CFRuntimeBase						base;				// CF type info. Must be first.
	dispatch_queue_t					internalQueue;		// Dispatch queue to run internal operations.
#if( BONJOUR_USE_GCD )
	DNSServiceRef						connectionRef;		// DNS-SD service for the browser connection.
#else
	DNSServiceContext *					connectionCtx;		// Context for managing the DNSServiceRef and file descriptor.
#endif
	DNSServiceRef						browseRef;			// Browser for PTRs. May be a threshold-only browser.
	DNSServiceRef						detailedBrowseRef;	// Ignored browser hack until <radar:13613078> is fixed.
	BonjourServiceRef					services;			// List of individual service's we've discovered.
	BonjourDeviceRef					devices;			// List of logical devices we've discovered.
	Boolean								started;			// True if user has started the browser.
	uint64_t							startTicks;			// Time when Bonjour was first started.
	dispatch_source_t					retryTimer;			// Timer to retry on Bonjour failures (e.g. discoveryd crashing).
	
	// Configuration
	
	char *								serviceType;		// Bonjour service we've been started to search for (e.g. "_http._tcp.").
	char *								domain;				// DNS domain we've been started to search in. Empty means all domains.
	char *								ifname;				// Network interface we've started to search on. NULL means all interfaces.
	uint64_t							flags;				// Flags to control browsing.
	dispatch_queue_t					eventQueue;			// Dispatch queue to invoke callbacks on.
	BonjourBrowserEventHandlerFunc		eventHandler_f;		// Function to call when events occur.
	void *								eventHandler_ctx;	// User-supplied context to pass to eventHandler_f.
#if( COMPILER_HAS_BLOCKS )
	BonjourBrowserEventHandlerBlock		eventHandler_b;		// Block to invoke when events occur.
#endif
};

// BonjourDevice

struct BonjourDevice
{
	BonjourDeviceRef			next;
	BonjourBrowserRef			browser;
	char						deviceID[ 64 ];
	CFDictionaryRef				deviceInfo;
	BonjourServiceRef			services;
	int							autoStopTXT; // Stop TXT queries after we get the first one. -1 means "don't know yet".
};

// BonjourRData

struct BonjourRData
{
	BonjourRDataRef			next;
	size_t					rdlen;
	uint8_t					rdata[ 1 ]; // Variable length.
};

// BonjourService

struct BonjourService
{
	BonjourServiceRef		nextForBrowser;
	BonjourServiceRef		nextForDevice;
	BonjourBrowserRef		browser;
	char *					name;
	char *					type;
	char *					domain;
	uint32_t				ifindex;
	char					ifname[ IF_NAMESIZE + 1 ];
	Boolean					isP2P;
	Boolean					isWiFi;
	NetTransportType		transportType;
	DNSServiceRef			txtQueryRef;
	BonjourRDataRef			txtRDataList;
	BonjourDeviceRef		device;
};

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static void _BonjourBrowserGetTypeID( void *inContext );
static void	_BonjourBrowser_Finalize( CFTypeRef inCF );
static void	_BonjourBrowser_Start( void *inArg );
static void	_BonjourBrowser_Stop( void *inArg );
static void	_BonjourBrowser_EnsureStarted( BonjourBrowserRef inBrowser );
static void	_BonjourBrowser_EnsureStopped( BonjourBrowserRef inBrowser );
static void	_BonjourBrowser_CopyDevices( void *inArg );
static void	_BonjourBrowser_ReconfirmDevice( void *inArg );
#if( !BONJOUR_USE_GCD )
	static void	_BonjourBrowserReadHandler( void *inArg );
	static void	_BonjourBrowserCancelHandler( void *inArg );
#endif
static void DNSSD_API
	_BonjourBrowser_BrowseHandler(
		DNSServiceRef			inRef,
		DNSServiceFlags			inFlags,
		uint32_t				inIfIndex,
		DNSServiceErrorType		inError,
		const char *			inName,  
		const char *			inType,    
		const char *			inDomain,  
		void *					inContext );
static void DNSSD_API
	_BonjourBrowser_IgnoredBrowseHandler(
		DNSServiceRef			inRef,
		DNSServiceFlags			inFlags,
		uint32_t				inIfIndex,
		DNSServiceErrorType		inError,
		const char *			inName,  
		const char *			inType,    
		const char *			inDomain,  
		void *					inContext );
static OSStatus
	_BonjourBrowser_PostEvent( 
		BonjourBrowserRef		inBrowser, 
		BonjourBrowserEventType	inEventType, 
		BonjourDeviceRef		inDevice, 
		BonjourServiceRef		inRemovedService );
static void	_BonjourBrowser_PostEventOnEventQueue( void *inArg );
static void	_BonjourBrowser_RemoveService( BonjourBrowserRef inBrowser, BonjourServiceRef inService, Boolean inUpdateTXT );
static void	_BonjourBrowser_HandleError( BonjourBrowserRef me, OSStatus inError );
static void	_BonjourBrowser_RetryTimerFired( void *inArg );
static void	_BonjourBrowser_RetryTimerCanceled( void *inArg );

// BonjourDevice

static CFDictionaryRef		_BonjourDevice_CreateDictionary( BonjourDeviceRef inDevice, OSStatus *outErr );
static void					_BonjourDevice_Free( BonjourDeviceRef inDevice );
static BonjourServiceRef	_BonjourDevice_GetBestService( BonjourDeviceRef inDevice );
static OSStatus				_BonjourDevice_UpdateTXTQueries( BonjourDeviceRef inDevice );

// BonjourService

static OSStatus
	_BonjourService_Create( 
		BonjourBrowserRef	inManager, 
		const char *		inName, 
		const char *		inType, 
		const char *		inDomain, 
		uint32_t			inIfIndex, 
		BonjourServiceRef *	outService );
static void		_BonjourService_Free( BonjourServiceRef inService );
static OSStatus	_BonjourService_StartTXTQuery( BonjourServiceRef inService );
static void DNSSD_API
	_BonjourService_TXTHandler(
		DNSServiceRef		inRef,
		DNSServiceFlags		inFlags,
		uint32_t			inIfIndex,
		DNSServiceErrorType	inError,
		const char *		inFullName,    
		uint16_t			inRRType,
		uint16_t			inRRClass,
		uint16_t			inRDataSize,
		const void *		inRData,
		uint32_t			inTTL,
		void *				inContext );
static OSStatus				_BonjourService_AddRData( BonjourServiceRef inService, const void *inRData, size_t inRDLen );
static void					_BonjourService_RemoveRData( BonjourServiceRef inService, const void *inRData, size_t inRDLen );
static CFDictionaryRef		_BonjourService_CreateDictionary( BonjourServiceRef inService, OSStatus *outErr );
static OSStatus				_BonjourService_GetDeviceID( BonjourServiceRef inService, char *inKeyBuf, size_t inKeyMaxLen );
static const char *			_BonjourService_GetName( BonjourServiceRef inService, size_t *outNameLen );
static Boolean				_BonjourService_IsAutoStopTXT( BonjourServiceRef inService );
static CFComparisonResult	_BonjourService_Comparator( const void *inA, const void *inB, void *inContext );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static const CFRuntimeClass		kBonjourBrowserClass = 
{
	0,							// version
	"BonjourBrowser",			// className
	NULL,						// init
	NULL,						// copy
	_BonjourBrowser_Finalize,	// finalize
	NULL,						// equal -- NULL means pointer equality.
	NULL,						// hash  -- NULL means pointer hash.
	NULL,						// copyFormattingDesc
	NULL,						// copyDebugDesc
	NULL,						// reclaim
	NULL						// refcount
};

static dispatch_once_t		gBonjourBrowserInitOnce	= 0;
static CFTypeID				gBonjourBrowserTypeID	= _kCFRuntimeNotATypeID;

ulog_define( BonjourBrowser, kLogLevelNotice, kLogFlags_Default, "BonjourBrowser", NULL );
#define bb_ulog( LEVEL, ... )		ulog( &log_category_from_name( BonjourBrowser ), (LEVEL), __VA_ARGS__ )

ulog_define( BonjourIssues, kLogLevelNotice, kLogFlags_Default, "BonjourIssues", NULL );
#define bb_issues_ucat()					&log_category_from_name( BonjourIssues )
#define bb_issues_ulog( LEVEL, ... )		ulog( &log_category_from_name( BonjourIssues ), (LEVEL), __VA_ARGS__ )

#if 0
#pragma mark -
#pragma mark == BonjourBrowser ==
#endif

GSList * carplayDevlist =  NULL;
//===========================================================================================================================
//	BonjourBrowserGetTypeID
//===========================================================================================================================

CFTypeID	BonjourBrowserGetTypeID( void )
{
	dispatch_once_f( &gBonjourBrowserInitOnce, NULL, _BonjourBrowserGetTypeID );
	return( gBonjourBrowserTypeID );
}

static void _BonjourBrowserGetTypeID( void *inContext )
{
	(void) inContext;
	
	gBonjourBrowserTypeID = _CFRuntimeRegisterClass( &kBonjourBrowserClass );
	check( gBonjourBrowserTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	BonjourBrowser_Create
//===========================================================================================================================

OSStatus	BonjourBrowser_Create( BonjourBrowserRef *outBrowser, const char *inLabel )
{
	OSStatus				err;
	BonjourBrowserRef		obj;
	size_t					extraLen;
	
	extraLen = sizeof( *obj ) - sizeof( obj->base );
	obj = (BonjourBrowserRef) _CFRuntimeCreateInstance( NULL, BonjourBrowserGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( obj, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) obj ) + sizeof( obj->base ), 0, extraLen );
	
	obj->internalQueue = dispatch_queue_create( inLabel ? inLabel : "BonjourBrowser", NULL );
	require_action( obj->internalQueue, exit, err = kUnknownErr );
	
	obj->eventQueue = dispatch_get_main_queue();
	arc_safe_dispatch_retain( obj->eventQueue );
	
	*outBrowser = obj;
	obj = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( obj );
	return( err );
}

static void	_BonjourBrowser_Finalize( CFTypeRef inCF )
{
	BonjourBrowserRef const		browser = (BonjourBrowserRef) inCF;
	
	dispatch_forget( &browser->internalQueue );
	dispatch_forget( &browser->eventQueue );
#if( BONJOUR_USE_GCD )
	check( !browser->connectionRef );
#else
	check( !browser->connectionCtx );
#endif
	check( !browser->browseRef );
	check( !browser->detailedBrowseRef );
	check( !browser->services );
	check( !browser->devices );
#if( COMPILER_HAS_BLOCKS )
	ForgetBlock( &browser->eventHandler_b );
#endif
	check( !browser->serviceType );
	check( !browser->domain );
	check( !browser->ifname );
	bb_ulog( kLogLevelVerbose, "Finalized browser\n" );
}

//===========================================================================================================================
//	BonjourBrowser_Delete
//===========================================================================================================================

// Just here for older CoreUtils clients that link to the old name. When all clients are gone, this can be removed.
void	BonjourBrowser_Delete( BonjourBrowserRef inBrowser );
void	BonjourBrowser_Delete( BonjourBrowserRef inBrowser )
{
	CFRelease( inBrowser );
}

//===========================================================================================================================
//	BonjourBrowser_SetDispatchQueue
//===========================================================================================================================

void	BonjourBrowser_SetDispatchQueue( BonjourBrowserRef inBrowser, dispatch_queue_t inQueue )
{
	ReplaceDispatchQueue( &inBrowser->eventQueue, inQueue );
}

//===========================================================================================================================
//	BonjourBrowser_SetEventHandler
//===========================================================================================================================

void	BonjourBrowser_SetEventHandler( BonjourBrowserRef inBrowser, BonjourBrowserEventHandlerFunc inFunc, void *inContext )
{
	inBrowser->eventHandler_f	= inFunc;
	inBrowser->eventHandler_ctx	= inContext;
}

//===========================================================================================================================
//	BonjourBrowser_SetEventHandlerBlock
//===========================================================================================================================

#if( COMPILER_HAS_BLOCKS )
void	BonjourBrowser_SetEventHandlerBlock( BonjourBrowserRef inBrowser, BonjourBrowserEventHandlerBlock inHandler )
{
	ReplaceBlock( &inBrowser->eventHandler_b, inHandler );
}
#endif

//===========================================================================================================================
//	BonjourBrowser_Start
//===========================================================================================================================

typedef struct
{
	BonjourBrowserRef		browser;
	char *					serviceType;
	char *					domain;
	char *					ifname;
	uint64_t				flags;
	
}	BonjourBrowser_StartParams;

OSStatus
	BonjourBrowser_Start( 
		BonjourBrowserRef	inBrowser, 
		const char *		inServiceType, 
		const char *		inDomain, 
		const char *		inIfName, 
		uint64_t			inFlags )
{
	OSStatus							err;
	BonjourBrowser_StartParams *		params;
	
	params = (BonjourBrowser_StartParams *) calloc( 1, sizeof( *params ) );
	require_action( params, exit, err = kNoMemoryErr );
	
	params->browser = inBrowser;
	params->flags = inFlags;
	
	params->serviceType = strdup( inServiceType );
	require_action( params->serviceType, exit, err = kNoMemoryErr );
	
	params->domain = strdup( inDomain ? inDomain : "" );
	require_action( params->domain, exit, err = kNoMemoryErr );
	
	if( inIfName && ( *inIfName != '\0' ) )
	{
		params->ifname = strdup( inIfName );
		require_action( params->ifname, exit, err = kNoMemoryErr );
	}
	
	CFRetain( inBrowser );
	dispatch_async_f( inBrowser->internalQueue, params, _BonjourBrowser_Start );
	params = NULL;
	err = kNoErr;
	
exit:
	if( params )
	{
		FreeNullSafe( params->serviceType );
		FreeNullSafe( params->domain );
		FreeNullSafe( params->ifname );
		free( params );
	}
	return( err );
}

//===========================================================================================================================
//	_BonjourBrowser_Start
//===========================================================================================================================

static void	_BonjourBrowser_Start( void *inArg )
{
	BonjourBrowser_StartParams * const		params	= (BonjourBrowser_StartParams *) inArg;
	BonjourBrowserRef const					browser	= params->browser;
	
	FreeNullSafe( browser->serviceType );
	browser->serviceType = params->serviceType;
	
	FreeNullSafe( browser->domain );
	browser->domain = params->domain;
	
	FreeNullSafe( browser->ifname );
	browser->ifname = params->ifname;
	
	browser->flags = params->flags;
	
	if( !browser->started )
	{
		CFRetain( browser );
		browser->started = true;
		browser->startTicks	= UpTicks();
	}
	_BonjourBrowser_EnsureStarted( browser );
	
	free( params );
	CFRelease( browser );
}

//===========================================================================================================================
//	BonjourBrowser_Stop
//===========================================================================================================================

void	BonjourBrowser_Stop( BonjourBrowserRef inBrowser )
{
	CFRetain( inBrowser );
	dispatch_async_f( inBrowser->internalQueue, inBrowser, _BonjourBrowser_Stop );
}

//===========================================================================================================================
//	_BonjourBrowser_Stop
//===========================================================================================================================

static void	_BonjourBrowser_Stop( void *inArg )
{
	BonjourBrowserRef const		browser = (BonjourBrowserRef) inArg;
	Boolean						wasStarted;
	
	wasStarted = browser->started;
	browser->started = false;
	if( wasStarted ) bb_ulog( kLogLevelTrace, "Stopping browse for %s\n", browser->serviceType );
	
	dispatch_source_forget( &browser->retryTimer );
	_BonjourBrowser_EnsureStopped( browser );
	if( wasStarted )
	{
		_BonjourBrowser_PostEvent( browser, kBonjourBrowserEventType_Stop, NULL, NULL );
		bb_ulog( kLogLevelTrace, "Stopped browse for %s\n", browser->serviceType );
		CFRelease( browser ); // Undo retain from _BonjourBrowser_Start on first start.
	}
	ForgetMem( &browser->serviceType );
	ForgetMem( &browser->domain );
	ForgetMem( &browser->ifname );
	CFRelease( browser ); // Undo retain from BonjourBrowser_Stop.
}

//===========================================================================================================================
//	_BonjourBrowser_EnsureStarted
//===========================================================================================================================

static void	_BonjourBrowser_EnsureStarted( BonjourBrowserRef inBrowser )
{
	uint64_t				flags64		= inBrowser->flags;
	uint32_t				flags32		= (uint32_t)( flags64 & 0xFFFFFFFFU );
	Boolean const			traffic		= ( flags64 & kBonjourBrowserFlag_Traffic )    ? true : false;
	Boolean const			background	= ( flags64 & kBonjourBrowserFlag_Background ) ? true : false;
#if( !BONJOUR_USE_GCD )
	DNSServiceContext *		ctx			= NULL;
#endif
	OSStatus				err;
	DNSServiceRef *			connectionRef;
	DNSServiceRef			service;
	uint32_t				ifindex;
	
	// Set up a shared connection for DNS-SD operations.
	
#if( BONJOUR_USE_GCD )
	connectionRef = &inBrowser->connectionRef;
#else
	ctx = inBrowser->connectionCtx; 
	if( !ctx )
	{
		ctx = (DNSServiceContext *) calloc( 1, sizeof( *ctx ) );
		require_action( ctx, exit, err = kNoMemoryErr );
		ctx->browser = inBrowser;
	}
	connectionRef = &ctx->dnsService;
#endif
	if( !*connectionRef )
	{
		bb_ulog( kLogLevelTrace, "Creating shared connection to browse for %s\n", inBrowser->serviceType );
		err = DNSServiceCreateConnection( connectionRef );
		require_noerr_quiet( err, exit );
		
		#if( BONJOUR_USE_GCD )
			DNSServiceSetDispatchQueue( *connectionRef, inBrowser->internalQueue );
		#else
			ctx->dnsFD = DNSServiceRefSockFD( *connectionRef );
			require_action( IsValidSocket( ctx->dnsFD ), exit, err = kUnknownErr );
			
			ctx->dnsSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, ctx->dnsFD, 0, inBrowser->internalQueue );
			require_action( ctx->dnsSource, exit, err = kUnknownErr );
			CFRetain( ctx->browser );
			inBrowser->connectionCtx = ctx;
			dispatch_set_context( ctx->dnsSource, ctx );
			dispatch_source_set_event_handler_f(  ctx->dnsSource, _BonjourBrowserReadHandler );
			dispatch_source_set_cancel_handler_f( ctx->dnsSource, _BonjourBrowserCancelHandler );
			dispatch_resume( ctx->dnsSource );
		#endif
	}
	
	// Start browsing.
	
	if( !inBrowser->browseRef )
	{
		uint32_t		flags32Alt;
		
		flags32Alt = flags32;
		if( traffic )
		{
			flags32Alt |= kDNSServiceFlagsThresholdOne_compat;
		}
		ifindex = inBrowser->ifname ? if_nametoindex( inBrowser->ifname ) : kDNSServiceInterfaceIndexAny;
		service = *connectionRef;
		bb_ulog( kLogLevelTrace, "Starting browse for %s on if %s, flags 0x%X\n", 
			inBrowser->serviceType, inBrowser->ifname ? inBrowser->ifname : "<any>", flags32Alt );
		err = DNSServiceBrowse( &service, kDNSServiceFlagsShareConnection | flags32Alt, ifindex, 
			inBrowser->serviceType, inBrowser->domain, _BonjourBrowser_BrowseHandler, inBrowser );		
		require_noerr( err, exit );
		inBrowser->browseRef = service;
	}
	
	// If we're using the traffic reduction scheme and not in background mode, start a separate full browse.
	// This hack causes two browses to be active. The one above is a present-only browse that is always active 
	// as long as we're started. The one below is a detailed browse where we ignore the results, but having
	// this browse is enough to cause the present-only browse to get all the results. This is needed to avoid
	// needing to fully tear down the browse when switching modes. Once <radar:13613078> is implemented in 
	// Bonjour, we can remove this second browse and just change the existing one as needed.
	
	if( background )
	{
		if( inBrowser->detailedBrowseRef )
		{
			bb_ulog( kLogLevelInfo, "Stopping detail browse for %s, if %s, flags 0x%X\n", 
				inBrowser->serviceType, inBrowser->ifname ? inBrowser->ifname : "<any>", flags32 );
			DNSServiceForget( &inBrowser->detailedBrowseRef );
			
		}
	}
	else if( traffic && !inBrowser->detailedBrowseRef )
	{
		flags32 &= ~kDNSServiceFlagsThresholdOne_compat;
		ifindex = inBrowser->ifname ? if_nametoindex( inBrowser->ifname ) : kDNSServiceInterfaceIndexAny;
		service = *connectionRef;
		bb_ulog( kLogLevelInfo, "Starting detail browse for %s, if %s, flags 0x%X\n", 
			inBrowser->serviceType, inBrowser->ifname ? inBrowser->ifname : "<any>", flags32 );
		err = DNSServiceBrowse( &service, kDNSServiceFlagsShareConnection | flags32, ifindex, 
			inBrowser->serviceType, inBrowser->domain, _BonjourBrowser_IgnoredBrowseHandler, inBrowser );
		check_noerr( err );
		if( !err ) inBrowser->detailedBrowseRef = service;
	}
	
#if( !BONJOUR_USE_GCD )
	ctx = NULL;
#endif
	err = kNoErr;
	
exit:
	if( err )
	{
		bb_ulog( kLogLevelWarning, "### Start browse for %s on if %s, flags 0x%llX failed: %#m\n", 
			inBrowser->serviceType, inBrowser->ifname ? inBrowser->ifname : "<any>", flags64, err );
		_BonjourBrowser_EnsureStopped( inBrowser );
	}
	_BonjourBrowser_HandleError( inBrowser, err );
#if( !BONJOUR_USE_GCD )
	if( ctx )
	{
		DNSServiceForget( &ctx->dnsService );
		free( ctx );
	}
#endif
}

//===========================================================================================================================
//	_BonjourBrowser_EnsureStopped
//===========================================================================================================================

static void	_BonjourBrowser_EnsureStopped( BonjourBrowserRef inBrowser )
{
	BonjourDeviceRef		device;
	BonjourServiceRef		service;
	
	// When GCD is supported directly by DNS-SD, just deallocate the service ref and we're done (it will cancel GCD).
	// Note: deallocating the parent connection DNSServiceRef also releases all child DNSServiceRef's so invalidate them.
	
#if( BONJOUR_USE_GCD )
	DNSServiceForget( &inBrowser->connectionRef );
#else
	if( inBrowser->connectionCtx )
	{
		dispatch_source_forget( &inBrowser->connectionCtx->dnsSource );
		inBrowser->connectionCtx = NULL;
	}
#endif
	inBrowser->browseRef = NULL;
	inBrowser->detailedBrowseRef = NULL;
	while( ( device = inBrowser->devices ) != NULL )
	{
		inBrowser->devices = device->next;
		_BonjourDevice_Free( device );
	}
	while( ( service = inBrowser->services ) != NULL )
	{
		inBrowser->services = service->nextForBrowser;
		service->txtQueryRef = NULL; // Invalidated by DNSServiceRefDeallocate of parent connection DNSServiceRef.
		_BonjourService_Free( service );
	}
}

//===========================================================================================================================
//	BonjourBrowser_CopyDevices
//===========================================================================================================================

typedef struct
{
	BonjourBrowserRef		browser;
	CFMutableArrayRef		array;
	OSStatus				error;
	
}	BonjourBrowser_CopyDevicesParams;

CFArrayRef	BonjourBrowser_CopyDevices( BonjourBrowserRef inBrowser, OSStatus *outErr )
{
	BonjourBrowser_CopyDevicesParams		params = { inBrowser, NULL, kInProgressErr };
	
	dispatch_sync_f( inBrowser->internalQueue, &params, _BonjourBrowser_CopyDevices );
	if( outErr ) *outErr = params.error;
	return( params.array );
}

static void	_BonjourBrowser_CopyDevices( void *inArg )
{
	BonjourBrowser_CopyDevicesParams * const		params = (BonjourBrowser_CopyDevicesParams *) inArg;
	OSStatus										err;
	CFMutableArrayRef								array;
	BonjourDeviceRef								device;
	CFDictionaryRef									deviceInfo;
	
	array = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( array, exit, err = kNoMemoryErr );
	
	for( device = params->browser->devices; device; device = device->next )
	{
		deviceInfo = _BonjourDevice_CreateDictionary( device, &err );
		if( deviceInfo )
		{
			CFArrayAppendValue( array, deviceInfo );
			CFRelease( deviceInfo );
		}
	}
	
	params->array = array;
	array = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( array );
	params->error = err;
}

//===========================================================================================================================
//	BonjourBrowser_ReconfirmDevice
//===========================================================================================================================

typedef struct
{
	BonjourBrowserRef		browser;
	CFDictionaryRef			deviceInfo;
	
}	BonjourBrowser_ReconfirmDeviceParams;

void	BonjourBrowser_ReconfirmDevice( BonjourBrowserRef inBrowser, CFDictionaryRef inDeviceInfo )
{
	BonjourBrowser_ReconfirmDeviceParams *		params;
	
	params = (BonjourBrowser_ReconfirmDeviceParams *) malloc( sizeof( *params ) );
	require( params, exit );
	params->browser = inBrowser;
	params->deviceInfo = inDeviceInfo;
	CFRetain( inDeviceInfo );
	
	CFRetain( inBrowser );
	dispatch_async_f( inBrowser->internalQueue, params, _BonjourBrowser_ReconfirmDevice );
	
exit:
	return;
}

static void	_BonjourBrowser_ReconfirmDevice( void *inArg )
{
	BonjourBrowser_ReconfirmDeviceParams * const		params  = (BonjourBrowser_ReconfirmDeviceParams *) inArg;
	BonjourBrowserRef const								browser = params->browser;
	OSStatus											err;
	char												deviceID[ 64 ];
	BonjourDeviceRef									device;
	BonjourServiceRef									service;
	uint8_t												rdata[ 256 ];
	uint8_t *											ptr;
	char												name[ kDNSServiceMaxDomainName ];
	
	// Find the device.
	
	*deviceID = '\0';
	CFDictionaryGetCString( params->deviceInfo, CFSTR( kBonjourDeviceKey_DeviceID ), deviceID, sizeof( deviceID ), NULL );
	require( *deviceID != '\0', exit );
	
	for( device = browser->devices; device; device = device->next )
	{
		if( stricmp( device->deviceID, deviceID ) == 0 )
		{
			break;
		}
	}
	require_quiet( device, exit );
	
	// Reconfirm each PTR record of the device.
	
	for( service = device->services; service; service = service->nextForDevice )
	{
		err = DNSServiceConstructFullName( name, service->name, service->type, service->domain );
		check_noerr( err );
		if( err ) continue;
		
		ptr = MakeDomainNameFromDNSNameString( rdata, name );
		check( ptr );
		if( !ptr ) continue;
		
		snprintf( name, sizeof( name ), "%s%s", service->type, service->domain );
		bb_ulog( kLogLevelNotice, "Reconfirming PTR for %s.%s%s on %s\n", service->name, service->type, service->domain, 
			service->ifname );
		err = DNSServiceReconfirmRecord( 0, service->ifindex, name, kDNSServiceType_PTR, kDNSServiceClass_IN, 
			(uint16_t)( ptr - rdata ), rdata );
		check_noerr( err );
	}
	
exit:
	CFRelease( params->deviceInfo );
	free( params );
	CFRelease( browser );
}

#if 0
#pragma mark -
#endif

#if( !BONJOUR_USE_GCD )
//===========================================================================================================================
//	_BonjourBrowserReadHandler
//===========================================================================================================================

static void	_BonjourBrowserReadHandler( void *inArg )
{
	DNSServiceContext * const		ctx = (DNSServiceContext *) inArg;
	OSStatus						err;
	
	err = DNSServiceProcessResult( ctx->dnsService );
	if( err )
	{
		bb_ulog( kLogLevelNotice, "### Bonjour server crashed for %s: %#m\n", ctx->browser->serviceType, err );
		ctx->browser->connectionCtx = NULL;
		dispatch_source_forget( &ctx->dnsSource );
		_BonjourBrowser_HandleError( ctx->browser, err );
	}
}

//===========================================================================================================================
//	_BonjourBrowserCancelHandler
//===========================================================================================================================

static void	_BonjourBrowserCancelHandler( void *inArg )
{
	DNSServiceContext * const		ctx = (DNSServiceContext *) inArg;
	
	DNSServiceForget( &ctx->dnsService );
	CFRelease( ctx->browser );
	free( ctx );
}
#endif

//===========================================================================================================================
//	_BonjourBrowser_BrowseHandler
//===========================================================================================================================

static void DNSSD_API
	_BonjourBrowser_BrowseHandler(
		DNSServiceRef			inRef,
		DNSServiceFlags			inFlags,
		uint32_t				inIfIndex,
		DNSServiceErrorType		inError,
		const char *			inName,  
		const char *			inType,    
		const char *			inDomain,  
		void *					inContext )
{
	BonjourBrowserRef const		browser = (BonjourBrowserRef) inContext;
	BonjourServiceRef *			nextService;
	BonjourServiceRef			service;
	OSStatus					err;
	Boolean						add;
	
	(void) inRef;

	if( inError == kDNSServiceErr_ServiceNotRunning )
	{
		bb_ulog( kLogLevelWarning, "### Browser for %s server crashed\n", browser->serviceType );
		_BonjourBrowser_HandleError( browser, inError );
		goto exit;
	}
	else if( inError )
	{
		bb_ulog( kLogLevelWarning, "### Browser for %s browse error: %#m\n", browser->serviceType, inError );
		goto exit;
	}
	require_action( browser->started, exit, bb_ulog( kLogLevelWarning, "### Browse response after stop\n" ) );
	
	add = ( inFlags & kDNSServiceFlagsAdd ) ? true : false;
	bb_ulog( kLogLevelNotice, "Bonjour PTR %s %s.%s%s on %u\n", add ? "Add" : "Rmv", inName, inType, inDomain, inIfIndex );
	
	for( nextService = &browser->services; ( service = *nextService ) != NULL; nextService = &service->nextForBrowser )
	{
		if( service->ifindex != inIfIndex )				continue; // Interface must match (zero/all is not allowed here).
		if( stricmp( service->name,   inName   ) != 0 )	continue; // Name must match.
		if( stricmp( service->type,   inType   ) != 0 )	continue; // Type must match.
		if( stricmp( service->domain, inDomain ) != 0 )	continue; // Domain must match.
		break;
	}
	if( add )
	{
		require_quiet( service == NULL, exit );
		err = _BonjourService_Create( browser, inName, inType, inDomain, inIfIndex, &service );
		require_noerr( err, exit );
		
		err = _BonjourService_StartTXTQuery( service );
		if( err ) _BonjourService_Free( service );
		require_noerr( err, exit );
		
		*nextService = service;

		carplayInfo	* _devInfo;
		_devInfo				=	( carplayInfo *)g_new( carplayInfo,1 );
		_devInfo->name			=	g_strdup( service->name );		
		_devInfo->ifname		=	g_strdup( service->ifname );	
		_devInfo->ifindex		=	service->ifindex;
		_devInfo->transportType	=	service->transportType;
		_devInfo->mac_addr		=	NULL;	

		carplayDevlist =	g_slist_prepend(carplayDevlist, _devInfo);
		printf( "\033[1;32;40m Bonjour PTR add device info :name = %s ifname = %s ifindex = %d transportType = %d\033[0m\n",
				_devInfo->name,_devInfo->ifname,_devInfo->ifindex,_devInfo->transportType);
	}
	else
	{
		require_quiet( service, exit );
		*nextService = service->nextForBrowser;
		_BonjourBrowser_RemoveService( browser, service, true );
		_BonjourService_Free( service );

		GSList *iterator = NULL;
		for (iterator = carplayDevlist; iterator; iterator = iterator->next) 
		{
			printf( "\033[1;32;40m Bonjour PTR remove device ifindex : ifindex = %d cur list ifindex = %d\033[0m\n",
					service->ifindex,((carplayInfo *)iterator->data)->ifindex);

			if ((service->ifindex == ((carplayInfo *)iterator->data)->ifindex)&&( g_strcmp0( service->name,   ((carplayInfo *)iterator->data)->name   ) == 0))
			{
				printf( "\033[1;32;40m Bonjour PTR remove device info : name = %s mac_addr = %s ifname = %s\033[0m\n",
					((carplayInfo *)iterator->data)->name,((carplayInfo *)iterator->data)->mac_addr,((carplayInfo *)iterator->data)->ifname);

				if( ((carplayInfo *)iterator->data)->name != NULL )
					g_free(((carplayInfo *)iterator->data)->name );
				if( ((carplayInfo *)iterator->data)->mac_addr != NULL )
					g_free(((carplayInfo *)iterator->data)->mac_addr );
				if( ((carplayInfo *)iterator->data)->ifname != NULL )
					g_free(((carplayInfo *)iterator->data)->ifname );

				carplayDevlist = g_slist_remove( carplayDevlist, iterator->data );
				break;
			}
		}
	}
	
exit:
	return;
}

//===========================================================================================================================
//	_BonjourBrowser_IgnoredBrowseHandler
//===========================================================================================================================

static void DNSSD_API
	_BonjourBrowser_IgnoredBrowseHandler(
		DNSServiceRef			inRef,
		DNSServiceFlags			inFlags,
		uint32_t				inIfIndex,
		DNSServiceErrorType		inError,
		const char *			inName,  
		const char *			inType,    
		const char *			inDomain,  
		void *					inContext )
{
	BonjourBrowserRef const		browser = (BonjourBrowserRef) inContext;
	
	require_quiet( browser->flags & kBonjourBrowserFlag_Traffic, exit );
	
	_BonjourBrowser_BrowseHandler( inRef, inFlags, inIfIndex, inError, inName, inType, inDomain, inContext );
	
exit:
	return;
}

//===========================================================================================================================
//	_BonjourBrowser_PostEvent
//===========================================================================================================================

typedef struct
{
	BonjourBrowserEventHandlerFunc		eventHandler_f;
	void *								eventHandler_ctx;
	BonjourBrowserEventType				eventType;
	CFDictionaryRef						deviceInfo;
	
}	BonjourBrowser_PostEventParams;

static OSStatus
	_BonjourBrowser_PostEvent( 
		BonjourBrowserRef		inBrowser, 
		BonjourBrowserEventType	inEventType,
		BonjourDeviceRef		inDevice, 
		BonjourServiceRef		inRemovedService )
{
	OSStatus					err;
	CFDictionaryRef				deviceInfo;
	CFMutableDictionaryRef		mutableDeviceInfo = NULL;
	
#if( COMPILER_HAS_BLOCKS )
	require_action_quiet( inBrowser->eventHandler_f || inBrowser->eventHandler_b, exit, err = kNoErr );
#else
	require_action_quiet( inBrowser->eventHandler_f, exit, err = kNoErr );
#endif
	
	if( inDevice )
	{
		deviceInfo = _BonjourDevice_CreateDictionary( inDevice, &err );
		require_noerr( err, exit );
		
		CFReleaseNullSafe( inDevice->deviceInfo );
		inDevice->deviceInfo = deviceInfo;
		
		if( inRemovedService )
		{
			CFDictionaryRef		serviceDict;
			const void *		objects[ 1 ];
			CFArrayRef			removedServices;
			
			mutableDeviceInfo = CFDictionaryCreateMutableCopy( NULL, 0, deviceInfo );
			require_action( mutableDeviceInfo, exit, err = kNoMemoryErr );
			
			serviceDict = _BonjourService_CreateDictionary( inRemovedService, &err );
			require_noerr( err, exit );
			
			objects[ 0 ] = serviceDict;
			removedServices = CFArrayCreate( NULL, objects, 1, &kCFTypeArrayCallBacks );
			CFRelease( serviceDict );
			require_action( removedServices, exit, err = kNoMemoryErr );
			
			CFDictionarySetValue( mutableDeviceInfo, CFSTR( kBonjourDeviceKey_RemovedServices ), removedServices );
			CFRelease( removedServices );
			deviceInfo = mutableDeviceInfo;
		}
	}
	else
	{
		deviceInfo = NULL;
	}
	
#if( COMPILER_HAS_BLOCKS )
	if( inBrowser->eventHandler_b )
	{
		BonjourBrowserEventHandlerBlock		eventHandler_b;
		
		eventHandler_b = Block_copy( inBrowser->eventHandler_b );
		require_action( eventHandler_b, exit, err = kUnknownErr );
		CFRetainNullSafe( deviceInfo );
		
		dispatch_async( inBrowser->eventQueue, 
		^{
			eventHandler_b( inEventType, deviceInfo );
			Block_release( eventHandler_b );
			CFReleaseNullSafe( deviceInfo );
		} );
	}
	else
#endif
	{
		BonjourBrowser_PostEventParams *		params;
		
		params = (BonjourBrowser_PostEventParams *) calloc( 1, sizeof( *params ) );
		require_action( params, exit, err = kNoMemoryErr );
		params->eventType			= inEventType;
		params->eventHandler_f		= inBrowser->eventHandler_f;
		params->eventHandler_ctx	= inBrowser->eventHandler_ctx;
		params->deviceInfo			= deviceInfo;
		CFRetainNullSafe( deviceInfo );
		
		dispatch_async_f( inBrowser->eventQueue, params, _BonjourBrowser_PostEventOnEventQueue );
	}
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( mutableDeviceInfo );
	return( err );
}

//===========================================================================================================================
//	_BonjourBrowser_PostEventOnEventQueue
//===========================================================================================================================

static void	_BonjourBrowser_PostEventOnEventQueue( void *inArg )
{
	BonjourBrowser_PostEventParams * const		params = (BonjourBrowser_PostEventParams *) inArg;
	params->eventHandler_f( params->eventType, params->deviceInfo, params->eventHandler_ctx );
	CFReleaseNullSafe( params->deviceInfo );
	free( params );
}

//===========================================================================================================================
//	_BonjourBrowser_RemoveService
//===========================================================================================================================

static void	_BonjourBrowser_RemoveService( BonjourBrowserRef inBrowser, BonjourServiceRef inService, Boolean inUpdateTXT )
{
	BonjourDeviceRef		device;
	BonjourDeviceRef		deviceCurr;
	BonjourDeviceRef *		deviceNext;
	BonjourServiceRef *		serviceNext;
	BonjourServiceRef		service;
	
	device = inService->device;
	if( !device ) goto exit;
	
	// Remove the service from the device.
	
	for( serviceNext = &device->services; ( service = *serviceNext ) != NULL; serviceNext = &service->nextForDevice )
	{
		if( service == inService )
		{
			*serviceNext = service->nextForDevice;
			break;
		}
	}
	check( service );
	
	// If there are no other services associated with this device, remove the device too.
	
	if( device->services == NULL )
	{
		for( deviceNext = &inBrowser->devices; ( deviceCurr = *deviceNext ) != NULL; deviceNext = &deviceCurr->next )
		{
			if( deviceCurr == device )
			{
				*deviceNext = device->next;
				break;
			}
		}
		check( deviceCurr );
		
		_BonjourBrowser_PostEvent( inBrowser, kBonjourBrowserEventType_RemoveDevice, device, inService );
		_BonjourDevice_Free( device );
	}
	else
	{
		_BonjourBrowser_PostEvent( inBrowser, kBonjourBrowserEventType_AddOrUpdateDevice, device, inService );
		
		// Re-evaluate TXT queries for this device since we may need to restart an inactive one if the service
		// that was just removed was the only active TXT query (e.g. local service goes away, leaving only BTMM).
		
		if( inUpdateTXT ) _BonjourDevice_UpdateTXTQueries( device );
	}
	
exit:
	return;
}

//===========================================================================================================================
//	_BonjourBrowser_HandleError
//===========================================================================================================================

static void	_BonjourBrowser_HandleError( BonjourBrowserRef me, OSStatus inError )
{
	BonjourServiceRef		service;
	uint64_t				ms;
	
	if( !inError )
	{
		dispatch_source_forget( &me->retryTimer );
		goto exit;
	}
	
	// Remove all the services and send remove events for each one.
	
	if( !me->retryTimer ) _BonjourBrowser_PostEvent( me, kBonjourBrowserEventType_Restarted, NULL, NULL );
	while( ( service = me->services ) != NULL )
	{
		me->services = service->nextForBrowser;
		_BonjourBrowser_RemoveService( me, service, false );
		_BonjourService_Free( service );
	}
	_BonjourBrowser_EnsureStopped( me );
	
	// Start a timer to retry later.
	
	require_quiet( !me->retryTimer, exit );
	
	ms = UpTicksToMilliseconds( UpTicks() - me->startTicks );
	ms = ( ms < 10513 ) ? ( 10513 - ms ) : 100; // Use 10513 to avoid being syntonic with 10 second launchd re-launching.
	bb_ulog( kLogLevelNotice, "### Browse for %s failed, retrying in %llu ms: %#m\n", me->serviceType, ms, inError );
	
	me->retryTimer = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, me->internalQueue );
	require_action( me->retryTimer, exit, bb_ulog( kLogLevelWarning, "### Bonjour retry timer failed\n" ) );
	CFRetain( me );
	dispatch_set_context( me->retryTimer, me );
	dispatch_source_set_event_handler_f( me->retryTimer, _BonjourBrowser_RetryTimerFired );
	dispatch_source_set_cancel_handler_f( me->retryTimer, _BonjourBrowser_RetryTimerCanceled );
	dispatch_source_set_timer( me->retryTimer, dispatch_time_milliseconds( ms ), DISPATCH_TIME_FOREVER, kNanosecondsPerSecond );
	dispatch_resume( me->retryTimer );
	
exit:
	return;
}


//===========================================================================================================================
//	_BonjourBrowser_RetryTimerFired
//===========================================================================================================================

static void	_BonjourBrowser_RetryTimerFired( void *inArg )
{
	BonjourBrowserRef const		me = (BonjourBrowserRef) inArg;
	
	bb_ulog( kLogLevelNotice, "Retrying Bonjour start for %s after failure\n", me->serviceType );
	dispatch_source_forget( &me->retryTimer );
	me->startTicks = UpTicks();
	_BonjourBrowser_EnsureStarted( me );
}

//===========================================================================================================================
//	_BonjourBrowser_RetryTimerCanceled
//===========================================================================================================================

static void	_BonjourBrowser_RetryTimerCanceled( void *inArg )
{
	BonjourBrowserRef const		me = (BonjourBrowserRef) inArg;
	
	CFRelease( me );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	BonjourDevice_Reconfirm
//===========================================================================================================================

void	BonjourDevice_Reconfirm( CFDictionaryRef inDeviceInfo )
{
	OSStatus			err;
	CFArrayRef			services;
	CFIndex				i, n;
	CFDictionaryRef		serviceInfo;
	char				rawName[ 128 ];
	char				type[ 128 ];
	char				domain[ kDNSServiceMaxDomainName ];
	char				name[ kDNSServiceMaxDomainName ];
	uint32_t			ifindex;
	uint8_t				rdata[ 256 ];
	uint8_t *			ptr;
	
	*type = '\0';
	CFDictionaryGetCString( inDeviceInfo, CFSTR( kBonjourDeviceKey_ServiceType ), type, sizeof( type ), NULL );
		
	services = CFDictionaryGetCFArray( inDeviceInfo, CFSTR( kBonjourDeviceKey_Services ), NULL );
	n = services ? CFArrayGetCount( services ) : 0;
	for( i = 0; i < n; ++i )
	{
		serviceInfo = CFArrayGetCFDictionaryAtIndex( services, i, NULL );
		check( serviceInfo );
		if( !serviceInfo ) continue;
		
		*rawName = '\0';
		CFDictionaryGetCString( serviceInfo, CFSTR( kBonjourDeviceKey_RawName ), rawName, sizeof( rawName ), &err );
		check_noerr( err );
		if( err ) continue;
		
		*domain = '\0';
		CFDictionaryGetCString( serviceInfo, CFSTR( kBonjourDeviceKey_Domain ), domain, sizeof( domain ), NULL );
		check_noerr( err );
		if( err ) continue;
		
		ifindex = (uint32_t) CFDictionaryGetInt64( serviceInfo, CFSTR( kBonjourDeviceKey_InterfaceIndex ), NULL );
		
		err = DNSServiceConstructFullName( name, rawName, type, domain );
		check_noerr( err );
		if( err ) continue;
		
		ptr = MakeDomainNameFromDNSNameString( rdata, name );
		check( ptr );
		if( !ptr ) continue;
		
		snprintf( name, sizeof( name ), "%s%s", type, domain );
		bb_ulog( kLogLevelNotice, "Reconfirming PTR for %s.%s%s on interface %u\n", rawName, type, domain, ifindex );
		err = DNSServiceReconfirmRecord( 0, ifindex, name, kDNSServiceType_PTR, kDNSServiceClass_IN, 
			(uint16_t)( ptr - rdata ), rdata );
		check_noerr( err );
	}
}

//===========================================================================================================================
//	_BonjourDevice_CreateDictionary
//===========================================================================================================================

static CFDictionaryRef	_BonjourDevice_CreateDictionary( BonjourDeviceRef inDevice, OSStatus *outErr )
{
	CFMutableDictionaryRef		result = NULL;
	OSStatus					err;
	BonjourServiceRef			bestService, service;
	const uint8_t *				txtPtr;
	size_t						txtLen;
	CFMutableDictionaryRef		newDeviceInfo	= NULL;
	CFMutableArrayRef			serviceArray	= NULL;
	CFDictionaryRef				serviceDict;
	const char *				namePtr;
	size_t						nameLen;
	char						fullName[ kDNSServiceMaxDomainName + 64 ];
	size_t						len;
	Boolean						p2pOnly;
	
	// If there's no TXT RData (e.g. TXT records removed), but there is a previous deviceInfo then use that.
	// This is so we can post a remove event in the case where the TXT remove comes before the PTR remove.
	
	bestService = _BonjourDevice_GetBestService( inDevice );
	if( !bestService || !bestService->txtRDataList )
	{
		require_action_quiet( inDevice->deviceInfo, exit, err = kNotPreparedErr );
		result = CFDictionaryCreateMutableCopy( NULL, 0, inDevice->deviceInfo );
		require_action( result, exit, err = kNoMemoryErr );
		CFDictionaryRemoveValue( result, CFSTR( kBonjourDeviceKey_Services ) );
		err = kNoErr;
		goto exit;
	}
	txtPtr = bestService->txtRDataList->rdata;
	txtLen = bestService->txtRDataList->rdlen;
	
	newDeviceInfo = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( newDeviceInfo, exit, err = kNoMemoryErr );
	
	// Name
	
	namePtr = _BonjourService_GetName( bestService, &nameLen );
	CFDictionarySetCString( newDeviceInfo, CFSTR( kBonjourDeviceKey_Name ), namePtr, nameLen );
	
	// DeviceID
	
	CFDictionarySetCString( newDeviceInfo, CFSTR( kBonjourDeviceKey_DeviceID ), inDevice->deviceID, kSizeCString );
	
	// DNSName
	
	err = DNSServiceConstructFullNameEx( fullName, bestService->name, bestService->type, bestService->domain );
	require_noerr( err, exit );
	len = strlen( fullName );
	snprintf( &fullName[ len ], sizeof( fullName ) - len, "%%%u", bestService->ifindex );
	CFDictionarySetCString( newDeviceInfo, CFSTR( kBonjourDeviceKey_DNSName ), fullName, kSizeCString );
	
	// ServiceType
	
	CFDictionarySetCString( newDeviceInfo, CFSTR( kBonjourDeviceKey_ServiceType ), bestService->type, kSizeCString );
	
	// TXT
	
	if( stricmp( bestService->type, "_airport._tcp." ) == 0 )
	{
		TXTRecordRef		txtRec;
		uint8_t				txtBuf[ 256 ];
		const char *		src;
		const char *		end;
		char				keyStr[ 8 ];
		size_t				keyLen;
		char				valueStr[ 256 ];
		size_t				valueLen;
		
		// Convert the AirPort comma-separated TXT record into a normal TXT record.
		
		require_action( txtLen > 0, exit, err = kSizeErr );
		TXTRecordCreate( &txtRec, (uint16_t) sizeof( txtBuf ), txtBuf );
		src = ( (const char *) txtPtr ) + 1;
		end = src + ( txtLen - 1 );
		while( ParseCommaSeparatedNameValuePair( src, end, keyStr, sizeof( keyStr ) - 1, &keyLen, NULL, 
			valueStr, sizeof( valueStr ), &valueLen, NULL, &src ) == kNoErr )
		{
			keyStr[ keyLen ] = '\0';
			TXTRecordSetValue( &txtRec, keyStr, (uint8_t) valueLen, valueStr );
		}
		
		CFDictionarySetData( newDeviceInfo, CFSTR( kBonjourDeviceKey_TXT ), 
			TXTRecordGetBytesPtr( &txtRec ), TXTRecordGetLength( &txtRec ) );
		TXTRecordDeallocate( &txtRec );
	}
	else
	{
		CFDictionarySetData( newDeviceInfo, CFSTR( kBonjourDeviceKey_TXT ), txtPtr, txtLen );
	}
	
	// Add per-service info.
	
	p2pOnly = true;
	for( service = inDevice->services; service; service = service->nextForDevice )
	{
		if( !service->isP2P ) p2pOnly = false;
		
		if( !serviceArray )
		{
			serviceArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			require_action( serviceArray, exit, err = kNoMemoryErr );
		}
		serviceDict = _BonjourService_CreateDictionary( service, &err );
		require_noerr( err, exit );
		CFArrayAppendValue( serviceArray, serviceDict );
		CFRelease( serviceDict );
	}
	if( serviceArray )
	{
		CFDictionarySetValue( newDeviceInfo, CFSTR( kBonjourDeviceKey_Services ), serviceArray );
		CFRelease( serviceArray );
		serviceArray = NULL;
	}
	if( p2pOnly ) CFDictionarySetValue( newDeviceInfo, CFSTR( kBonjourDeviceKey_P2POnly ), kCFBooleanTrue );
	
	result = newDeviceInfo;
	newDeviceInfo = NULL;
	
exit:
	CFReleaseNullSafe( serviceArray );
	CFReleaseNullSafe( newDeviceInfo );
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	_BonjourDevice_Free
//===========================================================================================================================

static void	_BonjourDevice_Free( BonjourDeviceRef inDevice )
{
	ForgetCF( &inDevice->deviceInfo );
	free( inDevice );
}

//===========================================================================================================================
//	_BonjourDevice_GetBestService
//===========================================================================================================================

static BonjourServiceRef	_BonjourDevice_GetBestService( BonjourDeviceRef inDevice )
{
	BonjourServiceRef		firstNonP2P, firstLocal, service;
	
	// Pick a service to use for TXT queries. All the services should have the same info so we don't need to keep 
	// redundant queries active. This prefers the local service because it should be faster and more reliable
	// than having to go across the Internet to talk to a Wide-Area server.
	
	firstNonP2P = NULL;
	firstLocal  = NULL;
	for( service = inDevice->services; service; service = service->nextForDevice )
	{
		if( strcmp( service->domain, "local." ) == 0 )
		{
			if( !service->isWiFi )					return( service ); // Prefer wired interfaces over all others.
			if( !firstNonP2P && !service->isP2P )	firstNonP2P = service;
			if( !firstLocal )						firstLocal = service;
		}
	}
	if( firstNonP2P ) return( firstNonP2P );
	if( firstLocal )  return( firstLocal );
	return( inDevice->services );
}

//===========================================================================================================================
//	_BonjourDevice_UpdateTXTQueries
//===========================================================================================================================

static OSStatus	_BonjourDevice_UpdateTXTQueries( BonjourDeviceRef inDevice )
{
	OSStatus				err;
	BonjourServiceRef		bestService;
	BonjourServiceRef		service;
	
	bestService = _BonjourDevice_GetBestService( inDevice );
	require_action( bestService, exit, err = kNotFoundErr );
	
	if( ( inDevice->autoStopTXT < 0 ) && bestService->txtRDataList )
	{
		inDevice->autoStopTXT = _BonjourService_IsAutoStopTXT( bestService );
	}
	
	// Stop TXT queries we no longer need.
	
	for( service = inDevice->services; service; service = service->nextForDevice )
	{
		if( ( inDevice->autoStopTXT > 0 ) || ( service != bestService ) )
		{
			DNSServiceForget( &service->txtQueryRef );
		}
	}
	
	// Make sure we have a TXT query active for the best service if needed.
	
	if( ( inDevice->autoStopTXT <= 0 ) && !bestService->txtQueryRef )
	{
		err = _BonjourService_StartTXTQuery( bestService );
		require_noerr( err, exit );
	}
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_BonjourService_Create
//===========================================================================================================================

static OSStatus
	_BonjourService_Create( 
		BonjourBrowserRef	inBrowser, 
		const char *		inName, 
		const char *		inType, 
		const char *		inDomain, 
		uint32_t			inIfIndex, 
		BonjourServiceRef *	outService )
{
	OSStatus				err;
	BonjourServiceRef		obj;
	
	obj = (BonjourServiceRef) calloc( 1, sizeof( *obj ) );
	require_action( obj, exit, err = kNoMemoryErr );
	
	obj->browser = inBrowser;
	
	obj->name = strdup( inName );
	require_action( obj->name, exit, err = kDNSServiceErr_NoMemory );
	
	obj->type = strdup( inType );
	require_action( obj->type, exit, err = kDNSServiceErr_NoMemory );
	
	obj->domain = strdup( inDomain );
	require_action( obj->domain, exit, err = kDNSServiceErr_NoMemory );
	
	obj->ifindex = inIfIndex;
	if_indextoname( inIfIndex, obj->ifname );
	SocketGetInterfaceInfo( kInvalidSocketRef, obj->ifname, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &obj->transportType );
	if( NetTransportTypeIsP2P( obj->transportType ) )		{ obj->isP2P  = true; obj->isWiFi = true; }
	else if( obj->transportType == kNetTransportType_WiFi )	  obj->isWiFi = true;
	
	*outService = obj;
	obj = NULL;
	err = kNoErr;
	
exit:
	if( obj ) _BonjourService_Free( obj );
	return( err );
}

//===========================================================================================================================
//	_BonjourService_Free
//===========================================================================================================================

static void	_BonjourService_Free( BonjourServiceRef inService )
{
	BonjourRDataRef		rdata;
	
	ForgetMem( &inService->name );
	ForgetMem( &inService->type );
	ForgetMem( &inService->domain );
	DNSServiceForget( &inService->txtQueryRef );
	while( ( rdata = inService->txtRDataList ) != NULL )
	{
		inService->txtRDataList = rdata->next;
		free( rdata );
	}
	free( inService );
}

//===========================================================================================================================
//	_BonjourService_StartTXTQuery
//===========================================================================================================================

static OSStatus	_BonjourService_StartTXTQuery( BonjourServiceRef inService )
{
	OSStatus			err;
	char				fullName[ kDNSServiceMaxDomainName ];
	DNSServiceRef		service;
	
	err = DNSServiceConstructFullName( fullName, inService->name, inService->type, inService->domain );
	require_noerr( err, exit );
	
#if( BONJOUR_USE_GCD )
	service = inService->browser->connectionRef;
#else
	require_action( inService->browser->connectionCtx, exit, err = kNotPreparedErr );
	service = inService->browser->connectionCtx->dnsService;
#endif
	err = DNSServiceQueryRecord( &service, kDNSServiceFlagsShareConnection | kDNSServiceFlagsUnicastResponse_compat, 
		inService->ifindex, fullName, kDNSServiceType_TXT, kDNSServiceClass_IN, _BonjourService_TXTHandler, inService );
	require_noerr( err, exit );
	inService->txtQueryRef = service;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_BonjourService_TXTHandler
//===========================================================================================================================

static void DNSSD_API
	_BonjourService_TXTHandler(
		DNSServiceRef		inRef,
		DNSServiceFlags		inFlags,
		uint32_t			inIfIndex,
		DNSServiceErrorType	inError,
		const char *		inFullName,    
		uint16_t			inRRType,
		uint16_t			inRRClass,
		uint16_t			inRDataSize,
		const void *		inRData,
		uint32_t			inTTL,
		void *				inContext )
{
	BonjourServiceRef const		service = (BonjourServiceRef) inContext;
	BonjourBrowserRef const		browser = service->browser;
	OSStatus					err;
	Boolean						add;
	char						deviceID[ 64 ];
	BonjourDeviceRef			device;
	
	(void) inRef;
	(void) inRRType;
	(void) inRRClass;
	(void) inTTL;
	
	require_noerr_action_quiet( inError, exit, // This probably means discoveryd crashed. Ignore here. PTR handler will retry.
		bb_ulog( kLogLevelWarning, "### Browser for %s TXT error: %#m\n", browser->serviceType, inError ) );
	require_action( browser->started, exit, bb_ulog( kLogLevelWarning, "### TXT response after stop\n" ) );
	
	add = ( inFlags & kDNSServiceFlagsAdd ) ? true : false;
	bb_ulog( kLogLevelVerbose, "Bonjour TXT %s %s on %u\n", add ? "Add" : "Rmv", inFullName, inIfIndex );
	
	// Due to <radar:7193656>, when there is a name collision, we may end up getting an add for the new, good data
	// then get adds/removes of the bad data. Eventually, the bad data(s) will be removed, but we won't get an another
	// add of the good data (since it already delivered an add) so we have to track all the records that have been
	// added/removed so we still have that one good record later when all the bad ones have finally been removed.
	
	if( add )	_BonjourService_AddRData(    service, inRData, inRDataSize );
	else		_BonjourService_RemoveRData( service, inRData, inRDataSize );
	if( service->txtRDataList )
	{
		// Due to <radar:10037073>, mDNSResponder may deliver a TXT record for a different device and then later 
		// update again with the correct info. To work around this without clients needing to deal with device
		// identities changing underneath them, if the unique deviceID changes, remove and then re-add the device.
		
		err = _BonjourService_GetDeviceID( service, deviceID, sizeof( deviceID ) );
		require_noerr( err, exit );
		
		device = service->device;
		if( device && ( stricmp( device->deviceID, deviceID ) != 0 ) )
		{
			bb_issues_ulog( kLogLevelInfo, "Removing and re-adding %s.%s%s for stale TXT record update (%s -> %s)\n", 
				service->name, service->type, service->domain, device->deviceID, deviceID );
			
			_BonjourBrowser_RemoveService( browser, service, true );
			service->device = NULL;
		}
		
		// If the service isn't associated with a device then find a matching one or create a new one.
		
		if( !service->device )
		{
			for( device = browser->devices; device; device = device->next )
			{
				if( stricmp( device->deviceID, deviceID ) == 0 )
				{
					break;
				}
			}
			if( !device )
			{
				device = (BonjourDeviceRef) calloc( 1, sizeof( *device ) );
				require_action( device, exit, err = kNoMemoryErr );
				device->autoStopTXT = -1; // -1 means we don't know yet.
				
				device->browser = browser;
				strlcpy( device->deviceID, deviceID, sizeof( device->deviceID ) );
				
				device->next = browser->devices;
				browser->devices = device;
			}
			
			service->device = device;
			service->nextForDevice = device->services;
			device->services = service;
		}
		
		_BonjourDevice_UpdateTXTQueries( device );
		_BonjourBrowser_PostEvent( browser, kBonjourBrowserEventType_AddOrUpdateDevice, device, NULL );
	}
	
exit:
	return;
}

//===========================================================================================================================
//	_BonjourService_AddRData
//===========================================================================================================================

static OSStatus	_BonjourService_AddRData( BonjourServiceRef inService, const void *inRData, size_t inRDLen )
{
	OSStatus			err;
	BonjourRDataRef		obj;
	
	if( inService->txtRDataList && log_category_enabled( bb_issues_ucat(), kLogLevelInfo ) )
	{
		int		i;
		
		bb_issues_ulog( kLogLevelInfo, "### Add without remove new:   %s.%s%s%%%u: %#{txt}\n", 
			inService->name, inService->type, inService->domain, inService->ifindex, inRData, inRDLen );
		i = 1;
		for( obj = inService->txtRDataList; obj; obj = obj->next )
		{
			bb_issues_ulog( kLogLevelInfo, "### Add without remove old %d: %s.%s%s%%%u: %#{txt}\n", 
				i, inService->name, inService->type, inService->domain, inService->ifindex, obj->rdata, obj->rdlen );
			++i;
		}
	}
	
	for( obj = inService->txtRDataList; obj; obj = obj->next )
	{
		if( MemEqual( inRData, inRDLen, obj->rdata, obj->rdlen ) )
		{
			err = kDuplicateErr;
			goto exit;
		}
	}
	
	obj = (BonjourRDataRef) calloc( 1, offsetof( struct BonjourRData, rdata ) + inRDLen );
	require_action( obj, exit, err = kNoMemoryErr );
	
	obj->next  = inService->txtRDataList;
	obj->rdlen = inRDLen;
	memcpy( obj->rdata, inRData, inRDLen );
	inService->txtRDataList = obj;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_BonjourService_RemoveRData
//===========================================================================================================================

static void	_BonjourService_RemoveRData( BonjourServiceRef inService, const void *inRData, size_t inRDLen )
{
	BonjourRDataRef *		next;
	BonjourRDataRef			curr;
	
	for( next = &inService->txtRDataList; ( curr = *next ) != NULL; next = &curr->next )
	{
		if( MemEqual( inRData, inRDLen, curr->rdata, curr->rdlen ) )
		{
			*next = curr->next;
			free( curr );
			break;
		}
	}
	if( !curr )
	{
		bb_ulog( kLogLevelInfo, "### Removed RData missing for %s.%s%s %%%u\n%1.1H\n", 
			inService->name, inService->type, inService->domain, inService->ifindex, 
			inRData, (int) inRDLen, (int) inRDLen );
	}
}

//===========================================================================================================================
//	_BonjourService_CreateDictionary
//===========================================================================================================================

static CFDictionaryRef	_BonjourService_CreateDictionary( BonjourServiceRef inService, OSStatus *outErr )
{
	CFDictionaryRef				result = NULL;
	OSStatus					err;
	CFMutableDictionaryRef		serviceDict = NULL;
	char						fullName[ kDNSServiceMaxDomainName + 64 ];
	size_t						len;
	
	serviceDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( serviceDict, exit, err = kNoMemoryErr );
		
	// DNSName
	
	err = DNSServiceConstructFullNameEx( fullName, inService->name, inService->type, inService->domain );
	require_noerr( err, exit );
	len = strlen( fullName );
	snprintf( &fullName[ len ], sizeof( fullName ) - len, "%%%u", inService->ifindex );
	CFDictionarySetCString( serviceDict, CFSTR( kBonjourDeviceKey_DNSName ), fullName, kSizeCString );
	
	// Domain
	
	CFDictionarySetCString( serviceDict, CFSTR( kBonjourDeviceKey_Domain ), inService->domain, kSizeCString );
	
	// InterfaceIndex
	
	CFDictionarySetInt64( serviceDict, CFSTR( kBonjourDeviceKey_InterfaceIndex ), inService->ifindex );
	
	// InterfaceName
	
	CFDictionarySetCString( serviceDict, CFSTR( kBonjourDeviceKey_InterfaceName ), inService->ifname, kSizeCString );
	
	// P2P
	
	if( inService->isP2P ) CFDictionarySetValue( serviceDict, CFSTR( kBonjourDeviceKey_P2P ), kCFBooleanTrue );
	
	// RawName
	
	CFDictionarySetCString( serviceDict, CFSTR( kBonjourDeviceKey_RawName ), inService->name, kSizeCString );
	
	// TransportType
	
	CFDictionarySetInt64( serviceDict, CFSTR( kBonjourDeviceKey_TransportType ), inService->transportType );
	
	// WiFi
	
	if( inService->isWiFi ) CFDictionarySetValue( serviceDict, CFSTR( kBonjourDeviceKey_WiFi ), kCFBooleanTrue );
	
	result = serviceDict;
	serviceDict = NULL;
	
exit:
	CFReleaseNullSafe( serviceDict );
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	_BonjourService_GetDeviceID
//===========================================================================================================================

static OSStatus	_BonjourService_GetDeviceID( BonjourServiceRef inService, char *inKeyBuf, size_t inKeyMaxLen )
{
	OSStatus			err;
	const uint8_t *		txtPtr;
	uint16_t			txtLen;
	const char *		keyPtr;
	uint8_t				keyLen;
	char				valueStr[ 256 ];
	uint8_t				deviceID[ 6 ];
	
	txtPtr = inService->txtRDataList->rdata;
	txtLen = (uint16_t) inService->txtRDataList->rdlen;
	
	if( ( stricmp( inService->type, "_airplay._tcp." )    == 0 ) ||
		( stricmp( inService->type, "_mfi-config._tcp." ) == 0 ) )
	{
		// This key should already be in AA:BB:CC:DD:EE:FF format, but validate and normalize to handle case issues, etc.
		
		keyPtr = (const char *) TXTRecordGetValuePtr( txtLen, txtPtr, "deviceid", &keyLen );
		require_action( keyPtr, exit, err = kNotFoundErr );
		
		err = TextToMACAddress( keyPtr, keyLen, deviceID );
		require_noerr( err, exit );
		
		keyPtr = MACAddressToCString( deviceID, valueStr );
		keyLen = (uint8_t) strlen( keyPtr );
	}
	else if( ( inService->browser->flags & kBonjourBrowserFlag_StandardID ) || 
			 ( stricmp( inService->type, "_hap._tcp." ) == 0 ) )
	{
		// This key should already be in AA:BB:CC:DD:EE:FF format, but validate and normalize to handle case issues, etc.
		
		keyPtr = (const char *) TXTRecordGetValuePtr( txtLen, txtPtr, "id", &keyLen );
		require_action( keyPtr, exit, err = kNotFoundErr );
		
		err = TextToMACAddress( keyPtr, keyLen, deviceID );
		require_noerr( err, exit );
		
		keyPtr = MACAddressToCString( deviceID, valueStr );
		keyLen = (uint8_t) strlen( keyPtr );
	}
	else if( stricmp( inService->type, "_airport._tcp." ) == 0 )
	{
		const char *		src;
		const char *		end;
		char				nameStr[ 8 ];
		size_t				nameLen;
		size_t				valueLen;
		
		// AirPort uses a comma-separated string for its TXT record with "waMA=XX-XX-XX-XX-XX-XX" as the device ID.
		
		require_action( txtLen > 0, exit, err = kSizeErr );
		keyPtr = NULL;
		keyLen = 0;
		src = ( (const char *) txtPtr ) + 1;
		end = src + ( txtLen - 1 );
		while( ParseCommaSeparatedNameValuePair( src, end, nameStr, sizeof( nameStr ), &nameLen, NULL, 
			valueStr, sizeof( valueStr ), &valueLen, NULL, &src ) == kNoErr )
		{
			if( strnicmpx( nameStr, nameLen, "waMA" ) == 0 )
			{
				keyPtr = valueStr;
				keyLen = (uint8_t) valueLen;
				break;
			}
		}
		require_action( keyPtr, exit, err = kNotFoundErr );
		
		// Normalize to AA:BB:CC:DD:EE:FF format.
		
		err = TextToMACAddress( keyPtr, keyLen, deviceID );
		require_noerr( err, exit );
		
		keyPtr = MACAddressToCString( deviceID, valueStr );
		keyLen = (uint8_t) strlen( keyPtr );
	}
	else if( stricmp( inService->type, "_raop._tcp." ) == 0 )
	{
		// These are formatted as AABBCCDDEEFF@<name> so parse the device ID from the prefix.
		
		keyPtr = strchr( inService->name, '@' );
		require_action( keyPtr, exit, err = kMalformedErr );
		keyLen = (uint8_t)( keyPtr - inService->name );
		keyPtr = inService->name;
		
		// Normalize to AA:BB:CC:DD:EE:FF format.
		
		err = TextToMACAddress( keyPtr, keyLen, deviceID );
		require_noerr( err, exit );
		
		keyPtr = MACAddressToCString( deviceID, valueStr );
		keyLen = (uint8_t) strlen( keyPtr );
	}
	else
	{
		dlogassert( "Unsupported service type '%s'", inService->type );
		err = kUnsupportedErr;
		goto exit;
	}
	
	require_action( keyLen < inKeyMaxLen, exit, err = kSizeErr );
	memcpy( inKeyBuf, keyPtr, keyLen );
	inKeyBuf[ keyLen ] = '\0';
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_BonjourService_GetName
//===========================================================================================================================

static const char *	_BonjourService_GetName( BonjourServiceRef inService, size_t *outNameLen )
{
	const char *		namePtr;
	size_t				nameLen;
	
	if( stricmp( inService->type, "_raop._tcp." ) == 0 )
	{
		// These services prefixes the name with the deviceID so strip off the prefix to get the name.
		
		namePtr = strchr( inService->name, '@' );
		namePtr = namePtr ? ( namePtr + 1 ) : inService->name;
		nameLen = strlen( namePtr );
	}
	else
	{
		namePtr = inService->name;
		nameLen = strlen( namePtr );
	}
	*outNameLen = nameLen;
	return( namePtr );
}

//===========================================================================================================================
//	_BonjourService_IsAutoStopTXT
//===========================================================================================================================

static Boolean	_BonjourService_IsAutoStopTXT( BonjourServiceRef inService )
{
	const uint8_t *		txtPtr;
	uint16_t			txtLen;
	const char *		valuePtr;
	uint8_t				valueLen;
	int					n;
	uint64_t			u64;
	
	if( stricmp( inService->type, "_raop._tcp." ) == 0 )
	{
		txtPtr = inService->txtRDataList->rdata;
		txtLen = (uint16_t) inService->txtRDataList->rdlen;
		
		// Check for a "Unified Bonjour" feature flag to indicate it's using both _airplay._tcp and _raop._tcp.
		// This means _airplay._tcp contains everything _raop._tcp has so we can stop TXT queries for _raop._tcp.
		
		valuePtr = (const char *) TXTRecordGetValuePtr( txtLen, txtPtr, "ft", &valueLen );
		if( valuePtr )
		{
			n = SNScanF( valuePtr, valueLen, "%llx", &u64 );
			if( ( n == 1 ) && ( u64 & 0x40000000 ) ) // kAirPlayFeature_UnifiedBonjour
			{
				return( true );
			}
		}
		
		// For devices built before the feature flag was introduced, fall back to checking for an AppleTV.
		// AppleTV has always used both _airplay._tcp and _raop._tcp so we can stop _raop._tcp queries to AppleTV.
		
		valuePtr = (const char *) TXTRecordGetValuePtr( txtLen, txtPtr, "am", &valueLen );
		if( valuePtr && ( strnicmp_prefix( valuePtr, valueLen, "AppleTV" ) == 0 ) )
		{
			return( true );
		}
	}
	return( false );
}

//===========================================================================================================================
//	_BonjourService_Comparator
//===========================================================================================================================

static CFComparisonResult	_BonjourService_Comparator( const void *inA, const void *inB, void *inContext )
{
	CFComparisonResult			cmp;
	uint64_t const				flags	= *( (uint64_t *) inContext );
	CFDictionaryRef const		aInfo	= (CFDictionaryRef) inA;
	CFDictionaryRef const		bInfo	= (CFDictionaryRef) inB;
	CFStringRef					cfstr;
	NetTransportType			aTT, bTT;
	int							a, b;
	
	// Prefer local over Wide Area Bonjour.
	
	cfstr = CFDictionaryGetCFString( aInfo, CFSTR( kBonjourDeviceKey_Domain ), NULL );
	a = cfstr && CFEqual( cfstr, CFSTR( "local." ) );
	cfstr = CFDictionaryGetCFString( bInfo, CFSTR( kBonjourDeviceKey_Domain ), NULL );
	b = cfstr && CFEqual( cfstr, CFSTR( "local." ) );
	cmp = b - a;
	require_quiet( cmp == 0, exit );
	
	// Prefer DirectLink.
	
	aTT = (NetTransportType) CFDictionaryGetInt64( aInfo, CFSTR( kBonjourDeviceKey_TransportType ), NULL );
	bTT = (NetTransportType) CFDictionaryGetInt64( bInfo, CFSTR( kBonjourDeviceKey_TransportType ), NULL );
	a = ( aTT & kNetTransportType_DirectLink ) ? 1 : 0;
	b = ( bTT & kNetTransportType_DirectLink ) ? 1 : 0;
	cmp = b - a;
	require_quiet( cmp == 0, exit );
	
	// Optionally prefer P2P.
	
	if( flags & kBonjourBrowserFlag_P2P )
	{
		cmp = NetTransportTypeIsP2P( bTT ) - NetTransportTypeIsP2P( aTT );
		require_quiet( cmp == 0, exit );
	}
	
	// Prefer wired.
	
	cmp = NetTransportTypeIsWired( bTT ) - NetTransportTypeIsWired( aTT );
	require_quiet( cmp == 0, exit );
	
	// Optionally prefer non-P2P.
	
	if( !( flags & kBonjourBrowserFlag_P2P ) )
	{
		cmp = !NetTransportTypeIsP2P( bTT ) - !NetTransportTypeIsP2P( aTT );
		require_quiet( cmp == 0, exit );
	}
	
exit:
	return( cmp );
}

#if 0
#pragma mark -
#pragma mark == Accessors ==
#endif

//===========================================================================================================================
//	BonjourDevice_CopyCFString
//===========================================================================================================================

CFStringRef	BonjourDevice_CopyCFString( CFDictionaryRef inDeviceInfo, const char *inTXTKey, OSStatus *outErr )
{
	CFStringRef			cfstr;
	CFDataRef			txt;
	const uint8_t *		txtPtr;
	uint16_t			txtLen;
	const uint8_t *		valuePtr;
	uint8_t				valueLen;
	OSStatus			err;
	
	cfstr = NULL;
	
	txt = (CFDataRef) CFDictionaryGetValue( inDeviceInfo, CFSTR( kBonjourDeviceKey_TXT ) );
	require_action( txt, exit, err = kInternalErr );
	txtPtr = CFDataGetBytePtr( txt );
	txtLen = (uint16_t) CFDataGetLength( txt );
	
	valuePtr = (const uint8_t *) TXTRecordGetValuePtr( txtLen, txtPtr, inTXTKey, &valueLen );
	require_action_quiet( valuePtr, exit, err = kNotFoundErr );
	valueLen = (uint8_t) strnlen( (const char *) valuePtr, valueLen );
	
	cfstr = CFStringCreateWithBytes( NULL, valuePtr, valueLen, kCFStringEncodingUTF8, false );
	require_action( cfstr, exit, err = kMalformedErr );
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( cfstr );
}

//===========================================================================================================================
//	BonjourDevice_GetBitListValue
//===========================================================================================================================

uint32_t	BonjourDevice_GetBitListValue( CFDictionaryRef inDeviceInfo, const char *inTXTKey, OSStatus *outErr )
{
	uint32_t			value;
	CFDataRef			txt;
	const uint8_t *		txtPtr;
	uint16_t			txtLen;
	const char *		valuePtr;
	uint8_t				valueLen;
	OSStatus			err;
	
	value = 0;
	
	txt = (CFDataRef) CFDictionaryGetValue( inDeviceInfo, CFSTR( kBonjourDeviceKey_TXT ) );
	require_action( txt, exit, err = kInternalErr );
	txtPtr = CFDataGetBytePtr( txt );
	txtLen = (uint16_t) CFDataGetLength( txt );
	
	valuePtr = (const char *) TXTRecordGetValuePtr( txtLen, txtPtr, inTXTKey, &valueLen );
	require_action_quiet( valuePtr, exit, err = kNotFoundErr );
	
	err = BitListString_Parse( valuePtr, valueLen, &value );
	require_noerr( err, exit );
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	BonjourDevice_GetDeviceID
//===========================================================================================================================

uint64_t	BonjourDevice_GetDeviceID( CFDictionaryRef inDeviceInfo, uint8_t outDeviceID[ 6 ], OSStatus *outErr )
{
	uint64_t			value;
	CFStringRef			tempCFStr;
	char				tempCStr[ 64 ];
	uint8_t				tempBuf[ 6 ];
	Boolean				good;
	OSStatus			err;
	
	value = 0;
	
	tempCFStr = (CFStringRef) CFDictionaryGetValue( inDeviceInfo, CFSTR( kBonjourDeviceKey_DeviceID ) );
	require_action( tempCFStr, exit, err = kInternalErr );
	
	good = CFStringGetCString( tempCFStr, tempCStr, (CFIndex) sizeof( tempCStr ), kCFStringEncodingASCII );
	require_action( good, exit, err = kMalformedErr );
	
	if( !outDeviceID ) outDeviceID = tempBuf;
	err = TextToMACAddress( tempCStr, kSizeCString, outDeviceID );
	require_noerr( err, exit );
	value = ReadBig48( outDeviceID );
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	BonjourDevice_CopyDNSNames
//===========================================================================================================================

char *	BonjourDevice_CopyDNSNames( CFDictionaryRef inDeviceInfo, uint64_t inFlags, OSStatus *outErr )
{
	char *					result		= NULL;
	char *					dnsNames	= NULL;
	OSStatus				err;
	CFArrayRef				services;
	CFMutableArrayRef		mutableServices;
	CFIndex					i, n;
	CFDictionaryRef			info;
	CFStringRef				cfstr;
	
	services = (CFArrayRef) CFDictionaryGetValue( inDeviceInfo, CFSTR( kBonjourDeviceKey_Services ) );
	if( services )	mutableServices = CFArrayCreateMutableCopy( NULL, 0, services );
	else			mutableServices = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( mutableServices, exit, err = kNoMemoryErr );
	
	CFArraySortValues( mutableServices, CFRangeMake( 0, CFArrayGetCount( mutableServices ) ), 
		_BonjourService_Comparator, &inFlags );
	n = CFArrayGetCount( mutableServices );
	for( i = 0; i < n; ++i )
	{
		info = (CFDictionaryRef) CFArrayGetValueAtIndex( mutableServices, i );
		cfstr = CFDictionaryGetCFString( info, CFSTR( kBonjourDeviceKey_DNSName ), NULL );
		if( !cfstr ) continue;
		
		err = AppendPrintF( &dnsNames, "%s%@", dnsNames ? kASCII_RecordSeparatorStr : "", cfstr );
		require_action( err > 0, exit, err = kUnknownErr );
	}
	require_action_quiet( dnsNames, exit, err = kNotFoundErr );
	
	result = dnsNames;
	dnsNames = NULL;
	err = kNoErr;
	
exit:
	FreeNullSafe( dnsNames );
	CFReleaseNullSafe( mutableServices );
	if( outErr ) *outErr = err;
	return( result );
}

//===========================================================================================================================
//	BonjourDevice_GetDNSName
//===========================================================================================================================

OSStatus	BonjourDevice_GetDNSName( CFDictionaryRef inDeviceInfo, uint64_t inFlags, char *inBuf, size_t inMaxLen )
{
	Boolean const		wantsP2P = (Boolean)( ( inFlags & kBonjourBrowserFlag_P2P ) != 0 );
	OSStatus			err;
	CFArrayRef			services;
	CFIndex				i, n;
	CFDictionaryRef		firstWiredInfo, firstLocalInfo, firstInfo, bestInfo, info;
	Boolean				b;
	char				tempStr[ 256 ];
	
	firstWiredInfo	= NULL;
	firstLocalInfo	= NULL;
	firstInfo		= NULL;
	services = (CFArrayRef) CFDictionaryGetValue( inDeviceInfo, CFSTR( kBonjourDeviceKey_Services ) );
	n = services ? CFArrayGetCount( services ) : 0;
	for( i = 0; i < n; ++i )
	{
		info = (CFDictionaryRef) CFArrayGetValueAtIndex( services, i );
		
		b = CFDictionaryGetBoolean( info, CFSTR( kBonjourDeviceKey_P2P ), NULL );
		if( b != wantsP2P ) continue;
		
		*tempStr = '\0';
		CFDictionaryGetCString( info, CFSTR( kBonjourDeviceKey_Domain ), tempStr, sizeof( tempStr ), NULL );
		if( strcmp( tempStr, "local." ) == 0 )
		{
			if( !firstWiredInfo && !CFDictionaryGetBoolean( info, CFSTR( kBonjourDeviceKey_WiFi ), NULL ) )
			{
				firstWiredInfo = info;
			}
			if( !firstLocalInfo ) firstLocalInfo = info;
		}
		if( !firstInfo ) firstInfo = info;
	}
	if(      firstWiredInfo )	bestInfo = firstWiredInfo;
	else if( firstLocalInfo )	bestInfo = firstLocalInfo;
	else						bestInfo = firstInfo;
	require_action_quiet( bestInfo, exit, err = kNotFoundErr );
	
	if( inBuf )
	{
		CFDictionaryGetCString( bestInfo, CFSTR( kBonjourDeviceKey_DNSName ), inBuf, inMaxLen, &err );
		require_noerr( err, exit );
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	BonjourDevice_GetInt64
//===========================================================================================================================

int64_t	BonjourDevice_GetInt64( CFDictionaryRef inDeviceInfo, const char *inTXTKey, int inBase, OSStatus *outErr )
{
	Value64				value, value2;
	CFDataRef			txt;
	const uint8_t *		txtPtr;
	uint16_t			txtLen;
	const char *		valuePtr;
	uint8_t				valueLen;
	char				tempStr[ 64 ];
	int					n;
	OSStatus			err;
	
	value.s64 = 0;
	
	txt = (CFDataRef) CFDictionaryGetValue( inDeviceInfo, CFSTR( kBonjourDeviceKey_TXT ) );
	require_action( txt, exit, err = kInternalErr );
	txtPtr = CFDataGetBytePtr( txt );
	txtLen = (uint16_t) CFDataGetLength( txt );
	
	valuePtr = (const char *) TXTRecordGetValuePtr( txtLen, txtPtr, inTXTKey, &valueLen );
	require_action_quiet( valuePtr, exit, err = kNotFoundErr );
	require_action( valueLen < sizeof( tempStr ), exit, err = kSizeErr );
	memcpy( tempStr, valuePtr, valueLen );
	tempStr[ valueLen ] = '\0';
	
	if(      IsTrueString( valuePtr,  valueLen ) ) { value.s64 = 1; n = 1; }
	else if( IsFalseString( valuePtr, valueLen ) ) { value.s64 = 0; n = 1; }
	else if( inBase ==  0 ) n = SNScanF( valuePtr, valueLen, "%lli", &value.s64 );
	else if( inBase == 16 ) n = SNScanF( valuePtr, valueLen, "%llx", &value.u64 );
	else if( inBase == 10 ) n = SNScanF( valuePtr, valueLen, "%lld", &value.s64 );
	else if( inBase ==  8 ) n = SNScanF( valuePtr, valueLen, "%llo", &value.u64 );
	else { dlogassert( "Unsupported base (%d)", inBase ); err = kParamErr; goto exit; }
	require_action( n == 1, exit, err = kMalformedErr );
	
	valuePtr = strchr( tempStr, ',' );
	if( valuePtr )
	{
		value2.s64 = 0;
		++valuePtr;
		if(      IsTrueString( valuePtr,  SIZE_MAX ) ) { value2.s64 = 1; n = 1; }
		else if( IsFalseString( valuePtr, SIZE_MAX ) ) { value2.s64 = 0; n = 1; }
		else if( inBase ==  0 ) n = SNScanF( valuePtr, SIZE_MAX, "%lli", &value2.s64 );
		else if( inBase == 16 ) n = SNScanF( valuePtr, SIZE_MAX, "%llx", &value2.u64 );
		else if( inBase == 10 ) n = SNScanF( valuePtr, SIZE_MAX, "%lld", &value2.s64 );
		else if( inBase ==  8 ) n = SNScanF( valuePtr, SIZE_MAX, "%llo", &value2.u64 );
		else { dlogassert( "Unsupported base (%d)", inBase ); err = kParamErr; goto exit; }
		require_action( n == 1, exit, err = kMalformedErr );
		
		value.u64 |= ( value2.u64 << 32 );
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( value.s64 );
}

//===========================================================================================================================
//	BonjourDevice_MergeInfo
//===========================================================================================================================

OSStatus	BonjourDevice_MergeInfo( CFDictionaryRef *ioDeviceInfo, CFDictionaryRef inNewDeviceInfo )
{
	CFDictionaryRef const		oldDeviceInfo	= *ioDeviceInfo;
	OSStatus					err;
	CFMutableDictionaryRef		mergedInfo		= NULL;
	CFMutableArrayRef			mergedServices	= NULL;
	CFArrayRef					oldServices		= NULL;
	CFArrayRef					newServices;
	CFIndex						oldIndex, oldCount, newIndex, newCount;
	CFDictionaryRef				oldServiceDict, newServiceDict;
	CFArrayRef					removedServices;
	CFIndex						removedIndex, removedCount;
	CFDictionaryRef				removedServiceDict;
	CFTypeRef					oldDomain, oldIfName, newDomain, newIfName, removedDomain, removedIfName;
	Boolean						p2pOnly;
	
	if( !oldDeviceInfo )
	{
		CFRetain( inNewDeviceInfo );
		*ioDeviceInfo = inNewDeviceInfo;
		err = kNoErr;
		goto exit;
	}
	
	// Create the new info, but save off the old services since the merge of the new info may replace the services array.
	
	oldServices = CFDictionaryGetCFArray( oldDeviceInfo, CFSTR( kBonjourDeviceKey_Services ), NULL );
	CFRetainNullSafe( oldServices );
	
	mergedInfo = CFDictionaryCreateMutableCopy( NULL, 0, oldDeviceInfo );
	require_action( mergedInfo, exit, err = kNoMemoryErr );
	
	CFDictionaryMergeDictionary( mergedInfo, inNewDeviceInfo );
	CFDictionaryRemoveValue( mergedInfo, CFSTR( kBonjourDeviceKey_RemovedServices ) );
	
	newServices = CFDictionaryGetCFArray( inNewDeviceInfo, CFSTR( kBonjourDeviceKey_Services ), NULL );
	if( newServices )	mergedServices = CFArrayCreateMutableCopy( NULL, 0, newServices );
	else				mergedServices = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( mergedServices, exit, err = kNoMemoryErr );
	
	// Add each old service that's not in the removed list and that's not already in the new list.
	
	removedServices	= CFDictionaryGetCFArray( inNewDeviceInfo, CFSTR( kBonjourDeviceKey_RemovedServices ), NULL );
	removedCount	= removedServices ? CFArrayGetCount( removedServices ) : 0;
	newCount		= newServices ? CFArrayGetCount( newServices ) : 0;
	oldCount		= oldServices ? CFArrayGetCount( oldServices ) : 0;
	for( oldIndex = 0; oldIndex < oldCount; ++oldIndex )
	{
		oldServiceDict = CFArrayGetCFDictionaryAtIndex( oldServices, oldIndex, NULL );
		if( !oldServiceDict ) continue;
		
		oldDomain = CFDictionaryGetValue( oldServiceDict, CFSTR( kBonjourDeviceKey_Domain ) );
		oldIfName = CFDictionaryGetValue( oldServiceDict, CFSTR( kBonjourDeviceKey_InterfaceName ) );
		
		// Skip services that are already there.
		
		for( newIndex = 0; newIndex < newCount; ++newIndex )
		{
			newServiceDict = CFArrayGetCFDictionaryAtIndex( newServices, newIndex, NULL );
			if( !newServiceDict ) continue;
			
			newDomain = CFDictionaryGetValue( newServiceDict, CFSTR( kBonjourDeviceKey_Domain ) );
			newIfName = CFDictionaryGetValue( newServiceDict, CFSTR( kBonjourDeviceKey_InterfaceName ) );
			if( CFEqualNullSafe( oldDomain, newDomain ) && CFEqualNullSafe( oldIfName, newIfName ) )
			{
				break;
			}
		}
		if( newIndex < newCount ) continue;
		
		// Skip removed services.
		
		for( removedIndex = 0; removedIndex < removedCount; ++removedIndex )
		{
			removedServiceDict = CFArrayGetCFDictionaryAtIndex( removedServices, removedIndex, NULL );
			if( !removedServiceDict ) continue;
			
			removedDomain = CFDictionaryGetValue( removedServiceDict, CFSTR( kBonjourDeviceKey_Domain ) );
			removedIfName = CFDictionaryGetValue( removedServiceDict, CFSTR( kBonjourDeviceKey_InterfaceName ) );
			if( CFEqualNullSafe( oldDomain, removedDomain ) && CFEqualNullSafe( oldIfName, removedIfName ) )
			{
				break;
			}
		}
		if( removedIndex < removedCount ) continue;
		
		CFArrayAppendValue( mergedServices, oldServiceDict );
	}
	CFDictionarySetValue( mergedInfo, CFSTR( kBonjourDeviceKey_Services ), mergedServices );
	
	// Update P2POnly. $$$ TO DO: This should really be removed and callers should manage this differently.
	
	newCount = CFArrayGetCount( mergedServices );
	p2pOnly = ( newCount > 0 ) ? true : false;
	for( newIndex = 0; newIndex < newCount; ++newIndex )
	{
		newServiceDict = CFArrayGetCFDictionaryAtIndex( mergedServices, newIndex, NULL );
		if( !newServiceDict ) continue;
		
		if( !CFDictionaryGetBoolean( newServiceDict, CFSTR( kBonjourDeviceKey_P2P ), NULL ) )
		{
			p2pOnly = false;
			break;
		}
	}
	if( p2pOnly )	CFDictionarySetValue( mergedInfo, CFSTR( kBonjourDeviceKey_P2POnly ), kCFBooleanTrue );
	else			CFDictionaryRemoveValue( mergedInfo, CFSTR( kBonjourDeviceKey_P2POnly ) );
	
	if( oldDeviceInfo ) CFRelease( oldDeviceInfo );
	*ioDeviceInfo = mergedInfo;
	mergedInfo = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( mergedServices );
	CFReleaseNullSafe( mergedInfo );
	CFReleaseNullSafe( oldServices );
	return( err );
}

//===========================================================================================================================
//	BonjourDevice_RemoveInterfaceInfo
//===========================================================================================================================

OSStatus	BonjourDevice_RemoveInterfaceInfo( CFDictionaryRef *ioDeviceInfo, const char *inIfName, Boolean inRemoveOthers )
{
	OSStatus					err;
	CFMutableDictionaryRef		newInfo		= NULL;
	CFMutableArrayRef			newServices = NULL;
	CFArrayRef					oldServices;
	CFIndex						n;
	CFDictionaryRef				serviceDict;
	char						ifname[ IF_NAMESIZE + 1 ];
	
	require_action_quiet( *ioDeviceInfo, exit, err = kNoErr );
	oldServices = CFDictionaryGetCFArray( *ioDeviceInfo, CFSTR( kBonjourDeviceKey_Services ), NULL );
	n = oldServices ? CFArrayGetCount( oldServices ) : 0;
	require_action_quiet( n > 0, exit, err = kNoErr );
	
	newInfo = CFDictionaryCreateMutableCopy( NULL, 0, *ioDeviceInfo );
	require_action( newInfo, exit, err = kNoMemoryErr );
	
	newServices = CFArrayCreateMutableCopy( NULL, 0, oldServices );
	require_action( newServices, exit, err = kNoMemoryErr );
	
	while( n-- > 0 )
	{
		serviceDict = CFArrayGetCFDictionaryAtIndex( newServices, n, NULL );
		check( serviceDict );
		if( !serviceDict ) continue;
		
		*ifname = '\0';
		CFDictionaryGetCString( serviceDict, CFSTR( kBonjourDeviceKey_InterfaceName ), ifname, sizeof( ifname ), NULL );
		if( ( strcmp( ifname, inIfName ) == 0 ) != !inRemoveOthers ) continue;
		
		CFArrayRemoveValueAtIndex( newServices, n );
	}
	n = CFArrayGetCount( newServices );
	if( n > 0 )	CFDictionarySetValue( newInfo, CFSTR( kBonjourDeviceKey_Services ), newServices );
	else		CFDictionaryRemoveValue( newInfo, CFSTR( kBonjourDeviceKey_Services ) );
	
	CFRelease( *ioDeviceInfo );
	*ioDeviceInfo = newInfo;
	newInfo = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( newServices );
	CFReleaseNullSafe( newInfo );
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )

typedef struct
{
	dispatch_semaphore_t		doneSem;
	
}	BonjourBrowser_TestContext;

void			BonjourBrowser_TestHandler( BonjourBrowserEventType inEventType, CFDictionaryRef inEventInfo, void *inContext );
OSStatus		BonjourBrowser_TestHelpers( void );
CFDictionaryRef	BonjourDevice_GetServiceByInterfaceName( CFDictionaryRef inDeviceInfo, const char *inIfName, OSStatus *outErr );

//===========================================================================================================================
//	BonjourBrowser_Test
//===========================================================================================================================

OSStatus	BonjourBrowser_Test( void )
{
	OSStatus						err;
	CFMutableDictionaryRef			info		= NULL;
	char *							dnsNames	= NULL;
	BonjourBrowser_TestContext		context		= { NULL };
	BonjourBrowser_TestContext *	contextPtr;
	dispatch_queue_t				queue		= NULL;
	BonjourBrowserRef				browser		= NULL;
	CFMutableDictionaryRef			dict		= NULL;
	int64_t							s64;
	
	// BonjourDevice_CopyDNSNames test.
	
	err = CFPropertyListCreateFormatted( NULL, &info, 
		"{"
			"%kO="
			"["
				"{"
					"%kO=%O"	// dnsName
					"%kO=%O"	// domain
					"%kO=%i"	// transportType
				"}"
				"{"
					"%kO=%O"	// dnsName
					"%kO=%O"	// domain
					"%kO=%i"	// transportType
				"}"
				"{"
					"%kO=%O"	// dnsName
					"%kO=%O"	// domain
					"%kO=%i"	// transportType
				"}"
				"{"
					"%kO=%O"	// dnsName
					"%kO=%O"	// domain
					"%kO=%i"	// transportType
				"}"
				"{"
					"%kO=%O"	// dnsName
					"%kO=%O"	// domain
					"%kO=%i"	// transportType
				"}"
			"]"
		"}", 
		CFSTR( kBonjourDeviceKey_Services ), 
			CFSTR( kBonjourDeviceKey_DNSName ),			CFSTR( "Ethernet" ), 
			CFSTR( kBonjourDeviceKey_Domain ),			CFSTR( "local." ), 
			CFSTR( kBonjourDeviceKey_TransportType ),	kNetTransportType_Ethernet, 
			
			CFSTR( kBonjourDeviceKey_DNSName ),			CFSTR( "WiFiDirect" ), 
			CFSTR( kBonjourDeviceKey_Domain ),			CFSTR( "local." ), 
			CFSTR( kBonjourDeviceKey_TransportType ),	kNetTransportType_WiFiDirect, 
			
			CFSTR( kBonjourDeviceKey_DNSName ),			CFSTR( "DirectLink-local" ), 
			CFSTR( kBonjourDeviceKey_Domain ),			CFSTR( "local." ), 
			CFSTR( kBonjourDeviceKey_TransportType ),	kNetTransportType_DirectLink, 
			
			CFSTR( kBonjourDeviceKey_DNSName ),			CFSTR( "DirectLink-wide" ), 
			CFSTR( kBonjourDeviceKey_Domain ),			CFSTR( "apple.com." ), 
			CFSTR( kBonjourDeviceKey_TransportType ),	kNetTransportType_DirectLink, 
			
			CFSTR( kBonjourDeviceKey_DNSName ),			CFSTR( "WiFi" ), 
			CFSTR( kBonjourDeviceKey_Domain ),			CFSTR( "local." ), 
			CFSTR( kBonjourDeviceKey_TransportType ),	kNetTransportType_WiFi
	);
	require_noerr( err, exit );
	
	dnsNames = BonjourDevice_CopyDNSNames( info, 0, &err );
	require_noerr( err, exit );
	err = ( strcmp( dnsNames, 
		"DirectLink-local"	kASCII_RecordSeparatorStr
		"Ethernet"			kASCII_RecordSeparatorStr
		"WiFi"				kASCII_RecordSeparatorStr
		"WiFiDirect"		kASCII_RecordSeparatorStr 
		"DirectLink-wide" ) == 0 ) ? kNoErr : -1;
	free( dnsNames );
	require_noerr( err, exit );
	
	dnsNames = BonjourDevice_CopyDNSNames( info, kBonjourBrowserFlag_P2P, &err );
	require_noerr( err, exit );
	err = ( strcmp( dnsNames, 
		"DirectLink-local"	kASCII_RecordSeparatorStr
		"WiFiDirect"		kASCII_RecordSeparatorStr 
		"Ethernet"			kASCII_RecordSeparatorStr
		"WiFi"				kASCII_RecordSeparatorStr
		"DirectLink-wide" ) == 0 ) ? kNoErr : -1;
	free( dnsNames );
	require_noerr( err, exit );
	
	// Bonjour Device Accessors.
	
	dict = (CFMutableDictionaryRef) CFCreateF( &err, 
		"{"
			"%kO=%D"
		"}", 
		CFSTR( kBonjourDeviceKey_TXT ), "\x07" "x=12345", 8 );
	require_noerr( err, exit );
	s64 = BonjourDevice_GetInt64( dict, "x", 0, &err );
	require_action( s64 == 12345, exit, err = kResponseErr );
	s64 = BonjourDevice_GetInt64( dict, "x", 10, &err );
	require_action( s64 == 12345, exit, err = kResponseErr );
	ForgetCF( &dict );
	
	dict = (CFMutableDictionaryRef) CFCreateF( &err, 
		"{"
			"%kO=%D"
		"}", 
		CFSTR( kBonjourDeviceKey_TXT ), "\x09" "x=0x12345", 10 );
	require_noerr( err, exit );
	s64 = BonjourDevice_GetInt64( dict, "x", 0, &err );
	require_action( s64 == 0x12345, exit, err = kResponseErr );
	s64 = BonjourDevice_GetInt64( dict, "x", 16, &err );
	require_action( s64 == 0x12345, exit, err = kResponseErr );
	ForgetCF( &dict );
	
	dict = (CFMutableDictionaryRef) CFCreateF( &err, 
		"{"
			"%kO=%D"
		"}", 
		CFSTR( kBonjourDeviceKey_TXT ), "\x0B" "x=12345,678", 12 );
	require_noerr( err, exit );
	s64 = BonjourDevice_GetInt64( dict, "x", 0, &err );
	require_action( s64 == ( ( INT64_C( 678 ) << 32 ) | 12345 ), exit, err = kResponseErr );
	s64 = BonjourDevice_GetInt64( dict, "x", 10, &err );
	require_action( s64 == ( ( INT64_C( 678 ) << 32 ) | 12345 ), exit, err = kResponseErr );
	ForgetCF( &dict );
	
	dict = (CFMutableDictionaryRef) CFCreateF( &err, 
		"{"
			"%kO=%D"
		"}", 
		CFSTR( kBonjourDeviceKey_TXT ), "\x0F" "x=0x12345,0x678", 16 );
	require_noerr( err, exit );
	s64 = BonjourDevice_GetInt64( dict, "x", 0, &err );
	require_action( s64 == ( ( INT64_C( 0x678 ) << 32 ) | 0x12345 ), exit, err = kResponseErr );
	s64 = BonjourDevice_GetInt64( dict, "x", 16, &err );
	require_action( s64 == ( ( INT64_C( 0x678 ) << 32 ) | 0x12345 ), exit, err = kResponseErr );
	ForgetCF( &dict );
	
	// Browse test.
	
	queue = dispatch_queue_create( __ROUTINE__, NULL );
	require_action( queue, exit, err = -1 );
	
	context.doneSem = dispatch_semaphore_create( 0 );
	require_action( context.doneSem, exit, err = -1 );
	
	err = BonjourBrowser_Create( &browser, "Test" );
	require_noerr( err, exit );
	BonjourBrowser_SetDispatchQueue( browser, queue );
	
	contextPtr = &context;
#if( COMPILER_HAS_BLOCKS )
	BonjourBrowser_SetEventHandlerBlock( browser,
	^( BonjourBrowserEventType inEventType, CFDictionaryRef inEventInfo )
	{
		BonjourBrowser_TestHandler( inEventType, inEventInfo, contextPtr );
	} );
#else
	BonjourBrowser_SetEventHandler( browser, BonjourBrowser_TestHandler, contextPtr );
#endif
	
	dlog( kLogLevelVerbose, "Starting...\n" );
	err = BonjourBrowser_Start( browser, "_raop._tcp", NULL, kDNSServiceInterfaceIndexAny, 0 );
	require_noerr( err, exit );
	
	sleep( 2 );
	
	dlog( kLogLevelVerbose, "Stopping...\n" );
	BonjourBrowser_Stop( browser );
	dispatch_semaphore_wait( context.doneSem, DISPATCH_TIME_FOREVER );
	dlog( kLogLevelVerbose, "Stopped\n" );
	
	dlog( kLogLevelVerbose, "Releasing...\n" );
	BonjourBrowser_Forget( &browser );
	browser = NULL;
	usleep( 200 * 1000 );
	
exit:
	ForgetCF( &info );
	ForgetCF( &dict );
	BonjourBrowser_Forget( &browser );
	if( context.doneSem )	arc_safe_dispatch_release( context.doneSem );
	if( queue )				arc_safe_dispatch_release( queue );
	printf( "BonjourBrowser_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

//===========================================================================================================================
//	BonjourBrowser_TestHandler
//===========================================================================================================================

void	BonjourBrowser_TestHandler( BonjourBrowserEventType inEventType, CFDictionaryRef inEventInfo, void *inContext )
{
	BonjourBrowser_TestContext * const		context = (BonjourBrowser_TestContext *) inContext;
	
	DEBUG_USE_ONLY( inEventInfo );
	
	switch( inEventType )
	{
		case kBonjourBrowserEventType_AddOrUpdateDevice:
			dlog( kLogLevelVerbose, "ADD/UPDATE:\n%1@\n", inEventInfo );
			break;
		
		case kBonjourBrowserEventType_RemoveDevice:
			dlog( kLogLevelVerbose, "REMOVE:\n%1@\n", inEventInfo );
			break;
		
		case kBonjourBrowserEventType_Stop:
			dlog( kLogLevelVerbose, "STOP\n" );
			dispatch_semaphore_signal( context->doneSem );
			break;
		
		default:
			break;
	}
}

//===========================================================================================================================
//	BonjourBrowser_TestHelpers
//===========================================================================================================================

OSStatus	BonjourBrowser_TestHelpers( void )
{
	OSStatus					err;
	CFMutableDictionaryRef		plist;
	CFDictionaryRef				deviceInfo = NULL;
	
	err = CFPropertyListCreateFormatted( kCFAllocatorDefault, &plist, 
		"{"
			"%kO=%O"	// name
			"%kO="		// services
			"["
				"{"
					"%kO=%O"	// domain
					"%kO=%i"	// ifindex
					"%kO=%O"	// ifname
				"}"
				"{"
					"%kO=%O"	// domain
					"%kO=%i"	// ifindex
					"%kO=%O"	// ifname
				"}"
				"{"
					"%kO=%O"	// domain
					"%kO=%i"	// ifindex
					"%kO=%O"	// ifname
				"}"
				"{"
					"%kO=%O"	// domain
					"%kO=%i"	// ifindex
					"%kO=%O"	// ifname
				"}"
			"]"
		"}", 
		CFSTR( kBonjourDeviceKey_Name ), CFSTR( "MyDevice" ), 
		CFSTR( kBonjourDeviceKey_Services ), 
			CFSTR( kBonjourDeviceKey_Domain ),			CFSTR( "local." ), 
			CFSTR( kBonjourDeviceKey_InterfaceIndex ),	4, 
			CFSTR( kBonjourDeviceKey_InterfaceName ),	CFSTR( "en0" ), 
			
			CFSTR( kBonjourDeviceKey_Domain ),			CFSTR( "local." ), 
			CFSTR( kBonjourDeviceKey_InterfaceIndex ),	5, 
			CFSTR( kBonjourDeviceKey_InterfaceName ),	CFSTR( "en1" ), 
			
			CFSTR( kBonjourDeviceKey_Domain ),			CFSTR( "local." ), 
			CFSTR( kBonjourDeviceKey_InterfaceIndex ),	6, 
			CFSTR( kBonjourDeviceKey_InterfaceName ),	CFSTR( "en2" ), 
			
			CFSTR( kBonjourDeviceKey_Domain ),			CFSTR( "local." ), 
			CFSTR( kBonjourDeviceKey_InterfaceIndex ),	7, 
			CFSTR( kBonjourDeviceKey_InterfaceName ),	CFSTR( "en3" )
	);
	require_noerr( err, exit );
	deviceInfo = plist;
	
	err = BonjourDevice_RemoveInterfaceInfo( &deviceInfo, "en1", false );
	require_noerr( err, exit );
	require_action( BonjourDevice_GetServiceByInterfaceName( deviceInfo, "en0", NULL ), exit, err = -1 );
	require_action( !BonjourDevice_GetServiceByInterfaceName( deviceInfo, "en1", NULL ), exit, err = -1 );
	require_action( BonjourDevice_GetServiceByInterfaceName( deviceInfo, "en2", NULL ), exit, err = -1 );
	require_action( BonjourDevice_GetServiceByInterfaceName( deviceInfo, "en3", NULL ), exit, err = -1 );
	
	err = BonjourDevice_RemoveInterfaceInfo( &deviceInfo, "en3", true );
	require_noerr( err, exit );
	require_action( !BonjourDevice_GetServiceByInterfaceName( deviceInfo, "en0", NULL ), exit, err = -1 );
	require_action( !BonjourDevice_GetServiceByInterfaceName( deviceInfo, "en1", NULL ), exit, err = -1 );
	require_action( !BonjourDevice_GetServiceByInterfaceName( deviceInfo, "en2", NULL ), exit, err = -1 );
	require_action( BonjourDevice_GetServiceByInterfaceName( deviceInfo, "en3", NULL ), exit, err = -1 );
	
	err = BonjourDevice_RemoveInterfaceInfo( &deviceInfo, "en3", false );
	require_noerr( err, exit );
	require_action( !CFDictionaryGetValue( deviceInfo, CFSTR( kBonjourDeviceKey_Services ) ), exit, err = -1 );
	
exit:
	CFReleaseNullSafe( deviceInfo );
	printf( "BonjourBrowser_TestHelpers: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

//===========================================================================================================================
//	BonjourDevice_GetServiceByInterfaceName
//===========================================================================================================================

CFDictionaryRef	BonjourDevice_GetServiceByInterfaceName( CFDictionaryRef inDeviceInfo, const char *inIfName, OSStatus *outErr )
{
	CFDictionaryRef		result = NULL;
	CFArrayRef			services;
	CFIndex				i, n;
	CFDictionaryRef		serviceDict;
	char				ifname[ IF_NAMESIZE + 1 ];
	
	services = CFDictionaryGetCFArray( inDeviceInfo, CFSTR( kBonjourDeviceKey_Services ), NULL );
	require_quiet( services, exit );
	
	n = CFArrayGetCount( services );
	for( i = 0; i < n; ++i )
	{
		serviceDict = CFArrayGetCFDictionaryAtIndex( services, i, NULL );
		if( !serviceDict ) continue;
		
		*ifname = '\0';
		CFDictionaryGetCString( serviceDict, CFSTR( kBonjourDeviceKey_InterfaceName ), ifname, sizeof( ifname ), NULL );
		if( strcmp( ifname, inIfName ) != 0 ) continue;
		
		result = serviceDict;
		break;
	}
	
exit:
	if( outErr ) *outErr = result ? kNoErr : kNotFoundErr;
	return( result );
}

#endif // !EXCLUDE_UNIT_TESTS
