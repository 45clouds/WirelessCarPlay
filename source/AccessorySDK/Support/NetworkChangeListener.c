/*
	File:    	NetworkChangeListener.c
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
	
	Copyright (C) 2012-2015 Apple Inc. All Rights Reserved.
*/

#include "NetworkChangeListener.h"

#include <errno.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include "NetUtils.h"

#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

#if( TARGET_OS_LINUX )
	#include <linux/rtnetlink.h>
#endif
#if( TARGET_OS_POSIX )
	#include <net/route.h>
#endif

//===========================================================================================================================
//	Internals
//===========================================================================================================================

#define kNetworkChangeCoalesceNanos		( UINT64_C( 1 ) * kNanosecondsPerSecond )

typedef union
{
#if( TARGET_OS_LINUX )
	struct nlmsghdr			hdr;
#elif( TARGET_OS_POSIX )
	struct rt_msghdr		hdr;
#endif
	char					bytes[ 1024 ];
	
}	RoutingMessage;

struct NetworkChangeListenerPrivate
{
	CFRuntimeBase					base;			// CF type info. Must be first.
	dispatch_queue_t				queue;			// Queue to serialize operations.
	SocketRef						routingSock;	// Weak reference to a socket for reading routing messages.
	dispatch_source_t				routingSource;	// Dispatch source for being notified of routing messages.
	dispatch_source_t				coalesceTimer;	// Timer to coalesce changes.
	NetworkChangeHandlerFunc		handlerFunc;	// Function to call when a change occurs.
	void *							handlerArg;		// User-specified argument to pass to change handler.
};

static void _NetworkChangeListenerGetTypeID( void *inArg );
static void	_NetworkChangeListenerFinalize( CFTypeRef inCF );
static void	_NetworkChangeListenerStart( void *inArg );
static void	_NetworkChangeListenerStop( void *inArg );
static void	_NetworkChangeListenerStop2( NetworkChangeListenerRef me );

#if( TARGET_OS_POSIX )
	static void	_NetworkChangeListenerReadHandler( void *inArg );
	static void	_NetworkChangeListenerCancelHandler( void *inArg );
	static void	_NetworkChangeListenerCoalesceTimer( void *inArg );
	static void	_NetworkChangeListenerCoalesceCanceled( void *inArg );
#endif

static dispatch_once_t			gNetworkChangeListenerInitOnce = 0;
static CFTypeID					gNetworkChangeListenerTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kNetworkChangeListenerClass = 
{
	0,								// version
	"NetworkChangeListener",		// className
	NULL,							// init
	NULL,							// copy
	_NetworkChangeListenerFinalize,	// finalize
	NULL,							// equal -- NULL means pointer equality.
	NULL,							// hash  -- NULL means pointer hash.
	NULL,							// copyFormattingDesc
	NULL,							// copyDebugDesc
	NULL,							// reclaim
	NULL							// refcount
};

ulog_define( NetworkChanges, kLogLevelNotice, kLogFlags_Default, "NetworkChanges", NULL );
#define ncl_ucat()					&log_category_from_name( NetworkChanges )
#define ncl_ulog( LEVEL, ... )		ulog( ncl_ucat(), (LEVEL), __VA_ARGS__ )
#define ncl_dlog( LEVEL, ... )		dlogc( ncl_ucat(), (LEVEL), __VA_ARGS__ )

//===========================================================================================================================
//	NetworkChangeListenerGetTypeID
//===========================================================================================================================

CFTypeID	NetworkChangeListenerGetTypeID( void )
{
	dispatch_once_f( &gNetworkChangeListenerInitOnce, NULL, _NetworkChangeListenerGetTypeID );
	return( gNetworkChangeListenerTypeID );
}

static void _NetworkChangeListenerGetTypeID( void *inArg )
{
	(void) inArg;
	
	gNetworkChangeListenerTypeID = _CFRuntimeRegisterClass( &kNetworkChangeListenerClass );
	check( gNetworkChangeListenerTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	NetworkChangeListenerCreate
//===========================================================================================================================

OSStatus	NetworkChangeListenerCreate( NetworkChangeListenerRef *outListener )
{
	OSStatus						err;
	NetworkChangeListenerRef		me;
	size_t							extraLen;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (NetworkChangeListenerRef) _CFRuntimeCreateInstance( NULL, NetworkChangeListenerGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->routingSock = kInvalidSocketRef;
	ReplaceDispatchQueue( &me->queue, NULL ); // Default to the main queue.
	
	*outListener = me;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_NetworkChangeListenerFinalize
//===========================================================================================================================

static void	_NetworkChangeListenerFinalize( CFTypeRef inCF )
{
	NetworkChangeListenerRef const		me = (NetworkChangeListenerRef) inCF;
	
	check( !IsValidSocket( me->routingSock ) );
	check( me->routingSource == NULL );
	check( me->coalesceTimer == NULL );
	dispatch_forget( &me->queue );
}

//===========================================================================================================================
//	NetworkChangeListenerSetDispatchQueue
//===========================================================================================================================

void	NetworkChangeListenerSetDispatchQueue( NetworkChangeListenerRef me, dispatch_queue_t inQueue )
{
	ReplaceDispatchQueue( &me->queue, inQueue );
}

//===========================================================================================================================
//	NetworkChangeListenerSetHandler
//===========================================================================================================================

void
	NetworkChangeListenerSetHandler( 
		NetworkChangeListenerRef	me, 
		NetworkChangeHandlerFunc	inHandler, 
		void *						inArg )
{
	me->handlerFunc = inHandler;
	me->handlerArg  = inArg;
}

//===========================================================================================================================
//	NetworkChangeListenerStart
//===========================================================================================================================

void	NetworkChangeListenerStart( NetworkChangeListenerRef me )
{
	ncl_dlog( kLogLevelTrace, "Starting\n" );
	CFRetain( me );
	dispatch_async_f( me->queue, me, _NetworkChangeListenerStart );
}

static void	_NetworkChangeListenerStart( void *inArg )
{
	NetworkChangeListenerRef const		me = (NetworkChangeListenerRef) inArg;
	OSStatus							err;
#if( TARGET_OS_POSIX )
	SocketRef							sock;
#endif
	
#if( TARGET_OS_LINUX )
	struct sockaddr_nl					sockAddr;
	
	memset( &sockAddr, 0, sizeof( sockAddr ) );
	sockAddr.nl_family = AF_NETLINK;
	sockAddr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;
	
	sock = socket( AF_NETLINK, SOCK_RAW, NETLINK_ROUTE );
	err = map_socket_creation_errno( sock );
	require_noerr( err, exit );
	
	err = bind( sock, (struct sockaddr *) &sockAddr, sizeof( sockAddr ) );
	err = map_socket_noerr_errno( sock, err );
	require_noerr( err, exit );
#elif( TARGET_OS_POSIX )
	sock = socket( AF_ROUTE, SOCK_RAW, 0 );
	err = map_socket_creation_errno( sock );
	require_noerr( err, exit );
#endif
#if( TARGET_OS_POSIX )
	SocketMakeNonBlocking( sock );
	me->routingSock = sock;
	
	me->routingSource = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, (uintptr_t) sock, 0, me->queue );
	require_action( me->routingSource, exit, err = kUnknownErr );
	dispatch_set_context( me->routingSource, me );
	dispatch_source_set_event_handler_f(  me->routingSource, _NetworkChangeListenerReadHandler );
	dispatch_source_set_cancel_handler_f( me->routingSource, _NetworkChangeListenerCancelHandler );
	dispatch_resume( me->routingSource );
	CFRetain( me );
	
	ncl_dlog( kLogLevelTrace, "Started\n" );
	sock = kInvalidSocketRef;
#endif
	err = kNoErr;
	
#if( TARGET_OS_POSIX )
exit:
	ForgetSocket( &sock );
#endif
	if( err ) _NetworkChangeListenerStop2( me );
	CFRelease( me );
}

//===========================================================================================================================
//	NetworkChangeListenerStop
//===========================================================================================================================

void	NetworkChangeListenerStop( NetworkChangeListenerRef me )
{
	ncl_dlog( kLogLevelTrace, "Stopping\n" );
	CFRetain( me );
	dispatch_async_f( me->queue, me, _NetworkChangeListenerStop );
}

static void	_NetworkChangeListenerStop( void *inArg )
{
	NetworkChangeListenerRef const		me = (NetworkChangeListenerRef) inArg;
	
	_NetworkChangeListenerStop2( me );
	CFRelease( me );
}

static void	_NetworkChangeListenerStop2( NetworkChangeListenerRef me )
{
	dispatch_source_forget( &me->coalesceTimer );
	dispatch_source_forget( &me->routingSource );
	if( me->handlerFunc ) me->handlerFunc( kNetworkEvent_Stopped, me->handlerArg );
	ncl_dlog( kLogLevelTrace, "Stopped\n" );
}

#if( TARGET_OS_POSIX )
//===========================================================================================================================
//	_NetworkChangeListenerReadHandler
//===========================================================================================================================

static void	_NetworkChangeListenerReadHandler( void *inArg )
{
	NetworkChangeListenerRef const		me = (NetworkChangeListenerRef) inArg;
	OSStatus							err;
	ssize_t								n;
	RoutingMessage						msg;
	size_t								len;
	int									changes = 0;
	dispatch_source_t					source;
#if( TARGET_OS_LINUX )
	struct nlmsghdr *					hdr;
#endif
	
	for( ;; )
	{
		n = read( me->routingSock, msg.bytes, sizeof( msg.bytes ) );
		err = map_socket_value_errno( me->routingSock, n >= 0, n );
		if( err == EINTR ) continue;
		if( err == EWOULDBLOCK ) break;
		require_noerr( err, exit );
		len = (size_t) n;
		#if( TARGET_OS_LINUX )
		{
			for( hdr = &msg.hdr; NLMSG_OK( hdr, len ); hdr = NLMSG_NEXT( hdr, len ) )
			{
				switch( hdr->nlmsg_type )
				{
					case RTM_NEWLINK:
					case RTM_DELLINK:
					case RTM_NEWADDR:
					case RTM_DELADDR:
						++changes;
						break;
				
					default:
						break;
				}
				ncl_dlog( kLogLevelChatty, "Change: 0x%X\n", hdr->nlmsg_type );
			}
		}
		#else
			if( len < ( offsetof( struct rt_msghdr, rtm_msglen ) + sizeof( msg.hdr.rtm_msglen ) ) )
			{
				dlogassert( "Read %zu too small for header", len );
				continue;
			}
			if( len < msg.hdr.rtm_msglen )
			{
				dlogassert( "Read %zu too small for msg (%d)", len, msg.hdr.rtm_msglen );
				continue;
			}
			switch( msg.hdr.rtm_type )
			{
				#if( defined( RTM_NEWLINK ) )
				case RTM_NEWLINK:
				#endif
				#if( defined( RTM_DELLINK ) )
				case RTM_DELLINK:
				#endif
				case RTM_NEWADDR:
				case RTM_DELADDR:
				#if( defined( RTM_IFINFO ) )
				case RTM_IFINFO:
				#endif
				#if( defined( RTM_CHANGE ) )
				case RTM_CHANGE:
				#endif
					++changes;
					break;
				
				default:
					break;
			}
			ncl_dlog( kLogLevelChatty, "Change: 0x%X\n", msg.hdr.rtm_type );
		#endif
	}
	if( changes )
	{
		if( !me->coalesceTimer )
		{
			ncl_dlog( kLogLevelVerbose, "Starting coalesce timer\n" );
			
			me->coalesceTimer = source = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, me->queue );
			require_action( source, exit, err = kUnknownErr );
			dispatch_set_context( source, me );
			dispatch_source_set_event_handler_f( source, _NetworkChangeListenerCoalesceTimer );
			dispatch_source_set_cancel_handler_f( source, _NetworkChangeListenerCoalesceCanceled );
			dispatch_source_set_timer( source, dispatch_time( DISPATCH_TIME_NOW, kNetworkChangeCoalesceNanos ), 
				DISPATCH_TIME_FOREVER, kNanosecondsPerSecond );
			dispatch_resume( source );
			CFRetain( me );
		}
		else
		{
			ncl_dlog( kLogLevelVerbose, "Coalescing change\n" );
		}
	}
	err = kNoErr;
	
exit:
	if( err )
	{
		ncl_ulog( kLogLevelWarning, "### Network change handling failed: %#m\n", err );
		_NetworkChangeListenerStop2( me );
	}
}

//===========================================================================================================================
//	_NetworkChangeListenerCancelHandler
//===========================================================================================================================

static void	_NetworkChangeListenerCancelHandler( void *inArg )
{
	NetworkChangeListenerRef const		me = (NetworkChangeListenerRef) inArg;
	
	ncl_dlog( kLogLevelVerbose, "Canceled\n" );
	ForgetSocket( &me->routingSock );
	CFRelease( me );
}

//===========================================================================================================================
//	_NetworkChangeListenerCoalesceTimer
//===========================================================================================================================

static void	_NetworkChangeListenerCoalesceTimer( void *inArg )
{
	NetworkChangeListenerRef const		me = (NetworkChangeListenerRef) inArg;
	
	ncl_dlog( kLogLevelVerbose, "Delivering change\n" );
	dispatch_source_forget( &me->coalesceTimer );
	if( me->handlerFunc ) me->handlerFunc( kNetworkEvent_Changed, me->handlerArg );
}

//===========================================================================================================================
//	_NetworkChangeListenerCoalesceCanceled
//===========================================================================================================================

static void	_NetworkChangeListenerCoalesceCanceled( void *inArg )
{
	NetworkChangeListenerRef const		me = (NetworkChangeListenerRef) inArg;
	
	ncl_dlog( kLogLevelVerbose, "Coalesce canceled\n" );
	CFRelease( me );
}
#endif // TARGET_OS_POSIX
