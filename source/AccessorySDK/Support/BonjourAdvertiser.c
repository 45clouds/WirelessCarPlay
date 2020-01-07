/*
	File:    	BonjourAdvertiser.c
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
	
	Copyright (C) 2013-2014 Apple Inc. All Rights Reserved.
*/

#include "BonjourAdvertiser.h"

#include "CFUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "dns_sd.h"
#include "NetUtils.h"
#include "StringUtils.h"
#include "TickUtils.h"
#include <glib.h>

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

//===========================================================================================================================
//	Internals
//===========================================================================================================================

#if( TARGET_OS_DARWIN )
	#define BONJOUR_USE_GCD		1
#else
	#define BONJOUR_USE_GCD		0
#endif

#if( !BONJOUR_USE_GCD )
typedef struct
{
	BonjourAdvertiserRef	advertiser;
	int						dnsFD;
	DNSServiceRef			dnsService;
	dispatch_source_t		dnsSource;
	
}	DNSServiceContext;
#endif

struct BonjourAdvertiserPrivate
{
	CFRuntimeBase			base;			// CF type info. Must be first.
	LogCategory *			ucat;			// Log category to use for logging.
	dispatch_queue_t		queue;			// Queue to serialize operations.
#if( BONJOUR_USE_GCD )
	DNSServiceRef			dnsService;		// DNS-SD service for the advertiser.
#else
	DNSServiceContext *		dnsContext;		// Context for managing the DNSService and file descriptor.
#endif
	dispatch_source_t		retryTimer;		// Timer to retry Bonjour failures (e.g. mDNSResponder crashing).
	uint64_t				startTicks;		// Time we last tried to start Bonjour .
	Boolean					started;		// True if start has been called.
	
	// Service advertising
	
	char *					domain;			// Domain to register the service on.
	uint64_t				flags;			// Flags to control the advertiser.
	uint32_t				ifindex;		// Interface index to register the service on (or 0 for all).
	char					ifname[ IF_NAMESIZE + 1 ]; // Name of the interface to register the service on (empty for all).
	char *					name;			// Name to register the service as (or NULL to use the system name).
	Boolean					p2pAllow;		// True if P2P connections should be allowed.
	int						port;			// TCP/UDP port number the service is listening on.
	char *					serviceType;	// Type of service to register.
	uint8_t *				txtPtr;			// TXT record to advertise (or NULL if there's no TXT record).
	uint16_t				txtLen;			// Number of bytes in the TXT record.
};

static void		_BonjourAdvertiserGetTypeID( void *inContext );
static void		_BonjourAdvertiserFinalize( CFTypeRef inCF );
static void		_BonjourAdvertiserStart( void *inArg );
static void		_BonjourAdvertiserStop( void *inArg );
static void		_BonjourAdvertiserUpdate( void *inArg );
static void		_BonjourAdvertiserUpdateService( BonjourAdvertiserRef me );
static void		_BonjourAdvertiserHandleError( BonjourAdvertiserRef me, OSStatus inError );
static void DNSSD_API
	_BonjourAdvertiserRegistrationHandler( 
		DNSServiceRef		inRef, 
		DNSServiceFlags		inFlags, 
		DNSServiceErrorType	inError, 
		const char *		inName,
		const char *		inType,
		const char *		inDomain,
		void *				inContext );
#if( !BONJOUR_USE_GCD )
	static void	_BonjourAdvertiserReadHandler( void *inArg );
	static void	_BonjourAdvertiserCancelHandler( void *inArg );
#endif
static void	_BonjourAdvertiserRetryTimer( void *inArg );

static const CFRuntimeClass		kBonjourAdvertiserClass = 
{
	0,							// version
	"BonjourAdvertiser",		// className
	NULL,						// init
	NULL,						// copy
	_BonjourAdvertiserFinalize,	// finalize
	NULL,						// equal -- NULL means pointer equality.
	NULL,						// hash  -- NULL means pointer hash.
	NULL,						// copyFormattingDesc
	NULL,						// copyDebugDesc
	NULL,						// reclaim
	NULL						// refcount
};

static dispatch_once_t		gBonjourAdvertiserInitOnce	= 0;
static CFTypeID				gBonjourAdvertiserTypeID		= _kCFRuntimeNotATypeID;

//===========================================================================================================================
//	Logging
//===========================================================================================================================

ulog_define( BonjourAdvertiser, kLogLevelTrace, kLogFlags_Default, "BonjourAdvertiser", NULL );
#define ba_ucat()								&log_category_from_name( BonjourAdvertiser )
#define ba_dlog( ADVERTISER, LEVEL, ... )		dlogc( (ADVERTISER)->ucat, (LEVEL), __VA_ARGS__ )
#define ba_ulog( ADVERTISER, LEVEL, ... )		ulog( (ADVERTISER)->ucat, (LEVEL), __VA_ARGS__ )

//===========================================================================================================================
//	BonjourAdvertiserGetTypeID
//===========================================================================================================================

CFTypeID	BonjourAdvertiserGetTypeID( void )
{
	dispatch_once_f( &gBonjourAdvertiserInitOnce, NULL, _BonjourAdvertiserGetTypeID );
	return( gBonjourAdvertiserTypeID );
}

static void _BonjourAdvertiserGetTypeID( void *inContext )
{
	(void) inContext;
	
	gBonjourAdvertiserTypeID = _CFRuntimeRegisterClass( &kBonjourAdvertiserClass );
	check( gBonjourAdvertiserTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	BonjourAdvertiserCreate
//===========================================================================================================================

OSStatus	BonjourAdvertiserCreate( BonjourAdvertiserRef *outAdvertiser )
{
	OSStatus					err;
	BonjourAdvertiserRef		me;
	size_t						extraLen;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (BonjourAdvertiserRef) _CFRuntimeCreateInstance( NULL, BonjourAdvertiserGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->queue = dispatch_queue_create( "BonjourAdvertiser", NULL );
	require_action( me->queue, exit, err = kUnknownErr );
	
	me->ucat = ba_ucat();
	
	*outAdvertiser = me;
	me = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( me );
	return( err );
}

//===========================================================================================================================
//	_BonjourAdvertiserFinalize
//===========================================================================================================================

static void	_BonjourAdvertiserFinalize( CFTypeRef inCF )
{
	BonjourAdvertiserRef const		me = (BonjourAdvertiserRef) inCF;
	
	dispatch_forget( &me->queue );
#if( BONJOUR_USE_GCD )
	check( !me->dnsService );
#else
	check( !me->dnsContext );
#endif
	check( !me->retryTimer );
	check( !me->started );
	
	ForgetMem( &me->domain );
	ForgetMem( &me->name );
	ForgetMem( &me->serviceType );
	ForgetMem( &me->txtPtr );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	BonjourAdvertiserSetDispatchQueue
//===========================================================================================================================

void	BonjourAdvertiserSetDispatchQueue( BonjourAdvertiserRef me, dispatch_queue_t inQueue )
{
	ReplaceDispatchQueue( &me->queue, inQueue );
}

//===========================================================================================================================
//	BonjourAdvertiserSetFlags
//===========================================================================================================================

void	BonjourAdvertiserSetFlags( BonjourAdvertiserRef me, uint64_t inFlags )
{
	me->flags = inFlags;
}

//===========================================================================================================================
//	BonjourAdvertiserSetInterfaceIndex
//===========================================================================================================================

void	BonjourAdvertiserSetInterfaceIndex( BonjourAdvertiserRef me, uint32_t inIfIndex )
{
	me->ifindex = inIfIndex;
}

//===========================================================================================================================
//	BonjourAdvertiserSetInterfaceName
//===========================================================================================================================

void	BonjourAdvertiserSetInterfaceName( BonjourAdvertiserRef me, const char *inIfName )
{
	strlcpy( me->ifname, inIfName, sizeof( me->ifname ) );
}

//===========================================================================================================================
//	BonjourAdvertiserSetLogging
//===========================================================================================================================

void	BonjourAdvertiserSetLogging( BonjourAdvertiserRef me, LogCategory *inCategory )
{
	me->ucat = inCategory;
}

//===========================================================================================================================
//	BonjourAdvertiserSetName
//===========================================================================================================================

OSStatus	BonjourAdvertiserSetName( BonjourAdvertiserRef me, const char *inName )
{
	return( ReplaceDifferentString( &me->name, inName ) );
}

//===========================================================================================================================
//	BonjourAdvertiserSetServiceType
//===========================================================================================================================

OSStatus	BonjourAdvertiserSetServiceType( BonjourAdvertiserRef me, const char *inType )
{
	return( ReplaceDifferentString( &me->serviceType, inType ) );
}

//===========================================================================================================================
//	BonjourAdvertiserSetDomain
//===========================================================================================================================

OSStatus	BonjourAdvertiserSetDomain( BonjourAdvertiserRef me, const char *inDomain )
{
	return( ReplaceDifferentString( &me->domain, inDomain ) );
}

//===========================================================================================================================
//	BonjourAdvertiserSetP2P
//===========================================================================================================================

void	BonjourAdvertiserSetP2P( BonjourAdvertiserRef me, Boolean inP2P )
{
	me->p2pAllow = inP2P;
}

//===========================================================================================================================
//	BonjourAdvertiserSetPort
//===========================================================================================================================

void	BonjourAdvertiserSetPort( BonjourAdvertiserRef me, int inPort )
{
	me->port = inPort;
}

//===========================================================================================================================
//	BonjourAdvertiserSetTXTRecord
//===========================================================================================================================

OSStatus	BonjourAdvertiserSetTXTRecord( BonjourAdvertiserRef me, const void *inTXT, size_t inLen )
{
	OSStatus		err;
	uint8_t *		ptr = NULL;
	
	if( inLen > 0 )
	{
		ptr = (uint8_t *) malloc( inLen );
		require_action( ptr, exit, err = kNoMemoryErr );
		memcpy( ptr, inTXT, inLen );
	}
	FreeNullSafe( me->txtPtr );
	me->txtPtr = ptr;
	me->txtLen = (uint16_t) inLen;
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	BonjourAdvertiserStart
//===========================================================================================================================

OSStatus	BonjourAdvertiserStart( BonjourAdvertiserRef me )
{
	CFRetain( me );
	dispatch_async_f( me->queue, me, _BonjourAdvertiserStart );
	return( kNoErr );
}

static void	_BonjourAdvertiserStart( void *inArg )
{
	BonjourAdvertiserRef const		me = (BonjourAdvertiserRef) inArg;
	
	if( !me->started )
	{
		me->started = true;
		CFRetain( me );
	}
	me->startTicks = UpTicks();
	_BonjourAdvertiserUpdateService( me );
	CFRelease( me );
}

//===========================================================================================================================
//	BonjourAdvertiserStop
//===========================================================================================================================

void	BonjourAdvertiserStop( BonjourAdvertiserRef me )
{
	CFRetain( me );
	dispatch_async_f( me->queue, me, _BonjourAdvertiserStop );
}

static void	_BonjourAdvertiserStop( void *inArg )
{
	BonjourAdvertiserRef const		me = (BonjourAdvertiserRef) inArg;
	
	dispatch_source_forget( &me->retryTimer );
	
#if( BONJOUR_USE_GCD )
	DNSServiceForget( &me->dnsService );
#else
	if( me->dnsContext )
	{
		dispatch_source_forget( &me->dnsContext->dnsSource );
		me->dnsContext = NULL;
	}
#endif
	
	if( me->started )
	{
		ba_ulog( me, kLogLevelNotice, "Deregistered Bonjour %s\n", me->serviceType );
		CFRelease( me );
	}
	me->started = false;
	CFRelease( me );
}

//===========================================================================================================================
//	BonjourAdvertiserUpdate
//===========================================================================================================================

OSStatus	BonjourAdvertiserUpdate( BonjourAdvertiserRef me )
{
	CFRetain( me );
	dispatch_async_f( me->queue, me, _BonjourAdvertiserUpdate );
	return( kNoErr );
}

static void	_BonjourAdvertiserUpdate( void *inArg )
{
	BonjourAdvertiserRef const		me = (BonjourAdvertiserRef) inArg;
	
	_BonjourAdvertiserUpdateService( me );
	CFRelease( me );
}

static void	_BonjourAdvertiserUpdateService( BonjourAdvertiserRef me )
{
	OSStatus				err;
#if( !BONJOUR_USE_GCD )
	DNSServiceContext *		ctx = NULL;
#endif
	DNSServiceRef *			dnsServicePtr;
	DNSServiceFlags			flags;
	uint32_t				ifindex;
	
	require_action( me->started, exit2, err = kNotPreparedErr );
	
#if( BONJOUR_USE_GCD )
	dnsServicePtr = &me->dnsService;
#else
	ctx = me->dnsContext; 
	if( !ctx )
	{
		ctx = (DNSServiceContext *) calloc( 1, sizeof( *ctx ) );
		require_action( ctx, exit, err = kNoMemoryErr );
		ctx->advertiser = me;
	}
	dnsServicePtr = &ctx->dnsService;
#endif
	
	if( *dnsServicePtr )
	{
		err = DNSServiceUpdateRecord( *dnsServicePtr, NULL, 0, me->txtLen, me->txtPtr, 0 );
		if( !err ) ba_ulog( me, kLogLevelNotice, "Updated Bonjour TXT for %s\n", me->serviceType );
		else
		{
			#if( BONJOUR_USE_GCD )
				DNSServiceForget( dnsServicePtr ); // Update not supported so deregister to force a re-register below.
			#else
				if( me->dnsContext )
				{
					dispatch_source_forget( &me->dnsContext->dnsSource );
					me->dnsContext = NULL;
				}
			#endif
		}
	}
	if( !( *dnsServicePtr ) )
	{
		flags = (DNSServiceFlags)( me->flags & UINT32_C( 0xFFFFFFFF ) );
		
		ifindex = me->ifindex;
		if( *me->ifname != '\0' )
		{
			ifindex = if_nametoindex( me->ifname );
			require_action_quiet( ifindex != 0, exit, err = kNotFoundErr );
		}
		err = DNSServiceRegister( dnsServicePtr, flags, ifindex, me->name, me->serviceType, me->domain, NULL, 
			htons( (uint16_t) me->port ), me->txtLen, me->txtPtr, _BonjourAdvertiserRegistrationHandler, me );
		require_noerr_quiet( err, exit );
		
		#if( BONJOUR_USE_GCD )
			DNSServiceSetDispatchQueue( *dnsServicePtr, me->queue );
		#else
			ctx->dnsFD = DNSServiceRefSockFD( *dnsServicePtr );
			require_action( IsValidSocket( ctx->dnsFD ), exit, err = kUnknownErr );
			
			ctx->dnsSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, ctx->dnsFD, 0, me->queue );
			require_action( ctx->dnsSource, exit, err = kUnknownErr );
			CFRetain( ctx->advertiser );
			me->dnsContext = ctx;
			dispatch_set_context( ctx->dnsSource, ctx );
			dispatch_source_set_event_handler_f(  ctx->dnsSource, _BonjourAdvertiserReadHandler );
			dispatch_source_set_cancel_handler_f( ctx->dnsSource, _BonjourAdvertiserCancelHandler );
			dispatch_resume( ctx->dnsSource );
		#endif
		
		ba_ulog( me, kLogLevelNotice, "Registering Bonjour %s port %d\n", me->serviceType, me->port );
	}
	
#if( !BONJOUR_USE_GCD )
	ctx = NULL;
#endif
	err = kNoErr;
	
exit:
	_BonjourAdvertiserHandleError( me, err );
	
exit2:
#if( !BONJOUR_USE_GCD )
	if( ctx )
	{
		DNSServiceForget( &ctx->dnsService );
		free( ctx );
	}
#endif
	return;
}

//===========================================================================================================================
//	_BonjourAdvertiserHandleError
//===========================================================================================================================

static void	_BonjourAdvertiserHandleError( BonjourAdvertiserRef me, OSStatus inError )
{	
	uint64_t		ms;
	
	if( !inError )
	{
		dispatch_source_forget( &me->retryTimer );
	}
	else if( !me->retryTimer )
	{
		ms = UpTicksToMilliseconds( UpTicks() - me->startTicks );
		ms = ( ms < 11113 ) ? ( 11113 - ms ) : 1; // Use 11113 to avoid being syntonic with 10 second re-launching.
		ba_ulog( me, kLogLevelNotice, "### Bonjour register for %s failed, retrying in %llu ms: %#m\n", 
			me->serviceType, ms, inError );
		
		me->retryTimer = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, me->queue );
		check( me->retryTimer );
		if( me->retryTimer )
		{
			dispatch_set_context( me->retryTimer, me );
			dispatch_source_set_event_handler_f( me->retryTimer, _BonjourAdvertiserRetryTimer );
			dispatch_source_set_timer( me->retryTimer, dispatch_time_milliseconds( ms ), DISPATCH_TIME_FOREVER, 
				kNanosecondsPerSecond );
			dispatch_resume( me->retryTimer );
		}
	}
}

//===========================================================================================================================
//	_BonjourAdvertiserRegistrationHandler
//===========================================================================================================================

static void DNSSD_API
	_BonjourAdvertiserRegistrationHandler( 
		DNSServiceRef		inRef, 
		DNSServiceFlags		inFlags, 
		DNSServiceErrorType	inError, 
		const char *		inName,
		const char *		inType,
		const char *		inDomain,
		void *				inContext )
{
	BonjourAdvertiserRef const		me = (BonjourAdvertiserRef) inContext;
	
	(void) inRef;
	(void) inFlags;
		
	if( inError == kDNSServiceErr_ServiceNotRunning )
	{
		ba_ulog( me, kLogLevelNotice, "### Bonjour server crashed for %s\n", me->serviceType );
		#if( BONJOUR_USE_GCD )
			DNSServiceForget( &me->dnsService );
		#else
			if( me->dnsContext )
			{
				dispatch_source_forget( &me->dnsContext->dnsSource );
				me->dnsContext = NULL;
			}
		#endif
		BonjourAdvertiserUpdate( me );
	}
	else if( inError )
	{
		ba_ulog( me, kLogLevelNotice, "### Bonjour registration error for %s: %#m\n", me->serviceType, inError );
	}
	else
	{
		ba_ulog( me, kLogLevelNotice, "Registered Bonjour %s.%s%s\n", inName, inType, inDomain );
	}
}

#if( !BONJOUR_USE_GCD )
//===========================================================================================================================
//	_BonjourAdvertiserReadHandler
//===========================================================================================================================

static void	_BonjourAdvertiserReadHandler( void *inArg )
{
	DNSServiceContext * const		ctx = (DNSServiceContext *) inArg;
	OSStatus						err;
	
	err = DNSServiceProcessResult( ctx->dnsService );
	if( err )
	{
		ba_ulog( ctx->advertiser, kLogLevelNotice, "### Bonjour server crashed for %s: %#m\n", 
			ctx->advertiser->serviceType, err );
		ctx->advertiser->dnsContext = NULL;
		dispatch_source_forget( &ctx->dnsSource );
		_BonjourAdvertiserHandleError( ctx->advertiser, err );
	}
}

//===========================================================================================================================
//	_BonjourAdvertiserCancelHandler
//===========================================================================================================================

static void	_BonjourAdvertiserCancelHandler( void *inArg )
{
	DNSServiceContext * const		ctx = (DNSServiceContext *) inArg;
	
	DNSServiceForget( &ctx->dnsService );
	CFRelease( ctx->advertiser );
	free( ctx );
}
#endif

//===========================================================================================================================
//	_BonjourAdvertiserRetryTimer
//===========================================================================================================================

static void	_BonjourAdvertiserRetryTimer( void *inArg )
{
	BonjourAdvertiserRef const		me = (BonjourAdvertiserRef) inArg;
	
	ba_ulog( me, kLogLevelNotice, "Retrying Bonjour register for %s after failure\n", me->serviceType );
	dispatch_source_forget( &me->retryTimer );
	me->startTicks = UpTicks();
	_BonjourAdvertiserUpdateService( me );
}

#if 0
#pragma mark -
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	BonjourAdvertiserTest
//===========================================================================================================================

OSStatus	BonjourAdvertiserTest( void );
OSStatus	BonjourAdvertiserTest( void )
{
	OSStatus					err;
	BonjourAdvertiserRef		advertiser = NULL;
	TXTRecordRef				txtRec;
	uint8_t						txtBuf[ 256 ];
	
	TXTRecordCreate( &txtRec, (uint16_t) sizeof( txtBuf ), txtBuf );
	
	err = TXTRecordSetValue( &txtRec, "key1", 6, "value1" );
	require_noerr( err, exit );
	
	err = TXTRecordSetValue( &txtRec, "key2", 6, "value2" );
	require_noerr( err, exit );
	
	err = BonjourAdvertiserCreate( &advertiser );
	require_noerr( err, exit );
	
	BonjourAdvertiserSetPort( advertiser, 12345 );
	
	err = BonjourAdvertiserSetServiceType( advertiser, "_http._tcp" );
	require_noerr( err, exit );
	
	err = BonjourAdvertiserSetTXTRecord( advertiser, TXTRecordGetBytesPtr( &txtRec ), TXTRecordGetLength( &txtRec ) );
	require_noerr( err, exit );
	
	err = BonjourAdvertiserStart( advertiser );
	require_noerr( err, exit );
	
	while( CFRunLoopRunInMode( kCFRunLoopDefaultMode, 10.0, true ) != kCFRunLoopRunTimedOut ) {}
	
	BonjourAdvertiserStop( advertiser );
	
exit:
	BonjourAdvertiserForget( &advertiser );
	while( CFRunLoopRunInMode( kCFRunLoopDefaultMode, 2.0, true ) != kCFRunLoopRunTimedOut ) {}
	printf( "BonjourAdvertiserTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
	
}
#endif // !EXCLUDE_UNIT_TESTS
