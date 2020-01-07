/*
	File:    	CFLiteRunLoopSelect.c
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
	
	Copyright (C) 2006-2015 Apple Inc. All Rights Reserved.
*/

#include "CommonServices.h"	// Include early so we can conditionalize on TARGET_* flags.
#include "DebugServices.h"	// Include early so we can conditionalize on DEBUG flags.

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if( TARGET_OS_POSIX )
	#include <unistd.h>
#endif

#include "CFLite.h"
#include "NetUtils.h"
#include "ThreadUtils.h"
#include "TickUtils.h"

#include CF_HEADER

#if( CFLITE_ENABLED )

#if 0
#pragma mark == Constants ==
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

CFStringRef		kCFRunLoopDefaultMode	= CFSTR( "default" );
CFStringRef		kCFRunLoopCommonModes	= CFSTR( "common" );

#if 0
#pragma mark == Types ==
#endif

//===========================================================================================================================
//	Types
//===========================================================================================================================

// CFRunLoop

struct CFRunLoop
{
	CFRuntimeBase				base;
	Boolean						stop;
	SocketRef					commandSock;
	CFRunLoopTimerRef			timerList;
	CFRunLoopSourceRef			sourceList;
	CFRunLoopSourceRef			signaledSourceList;
};

// CFRunLoopSource

typedef enum
{
	kCFRunLoopSourceType_Generic	= 1, 
	kCFRunLoopSourceType_Socket		= 2

}	CFRunLoopSourceType;

struct CFRunLoopSource
{
	CFRuntimeBase				base;
	CFRunLoopSourceRef			next;
	CFRunLoopSourceRef			nextSignaled;
	CFRunLoopRef				rl;
	CFRunLoopSourceContext		context;
	CFRunLoopSourceType			type;
};

// CFRunLoopTimer

struct CFRunLoopTimer
{
	CFRuntimeBase				base;
	CFRunLoopTimerRef			next;
	CFRunLoopRef				rl;
	uint64_t					expireTicks;
	uint64_t					intervalTicks;
	CFRunLoopTimerCallBack		callback;
	void *						context;
};

// CFSocket

struct CFSocket
{
	CFRuntimeBase				base;
	CFRunLoopRef				rl;
	CFSocketNativeHandle		nativeSock;
	Boolean						connecting;
	CFOptionFlags				flags;
	CFOptionFlags				callbackTypes;
	CFSocketCallBack			callback;
	CFSocketContext				context;
	CFRunLoopSourceRef			source;
};

#if 0
#pragma mark -
#pragma mark == CFRunLoop ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

#define	CFRunLoopLock()				pthread_mutex_lock( &gCFRunLoopLock );
#define	CFRunLoopUnlock()			pthread_mutex_unlock( &gCFRunLoopLock );

DEBUG_STATIC OSStatus	__CFRunLoopCreate( CFRunLoopRef *outRL );
DEBUG_STATIC void		__CFRunLoopFree( CFTypeRef inObj );
DEBUG_STATIC OSStatus	__CFRunLoopRunOne( CFRunLoopRef inRL, struct timeval *inTimeout, Boolean inReturnAfterSourceHandled );
DEBUG_STATIC OSStatus	__CFRunLoopSelect( int inMaxFD, fd_set *inReadSet, fd_set *inWriteSet, uint64_t inTimeoutTicks );
DEBUG_STATIC void		__CFRunLoopCommandSocketHandler( CFRunLoopRef inRL );
DEBUG_STATIC void		__CFRunLoopSourceFree( CFTypeRef inObj );
DEBUG_STATIC void		__CFRunLoopTimerFree( CFTypeRef inObj );
DEBUG_STATIC void		__CFRunLoopTimerRequeue( CFRunLoopTimerRef inTimer );
DEBUG_STATIC void		__CFSocketHandler( CFSocketRef inSock, CFOptionFlags inCallBackType );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static const CFRuntimeClass		kCFRunLoopClass =
{
	0, 						// version
	"CFRunLoop",			// className
	NULL,					// init
	NULL, 					// copy
	__CFRunLoopFree,		// finalize
	NULL,					// equal
	NULL,					// hash
	NULL, 					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

static const CFRuntimeClass		kCFRunLoopSourceClass =
{
	0, 						// version
	"CFRunLoopSource",		// className
	NULL,					// init
	NULL, 					// copy
	__CFRunLoopSourceFree,	// finalize
	NULL,					// equal
	NULL,					// hash
	NULL, 					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

static const CFRuntimeClass		kCFRunLoopTimerClass =
{
	0, 						// version
	"CFRunLoopTimer",		// className
	NULL,					// init
	NULL, 					// copy
	__CFRunLoopTimerFree,	// finalize
	NULL,					// equal
	NULL,					// hash
	NULL, 					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

static pthread_mutex_t		gCFRunLoopLock;
static pthread_mutex_t *	gCFRunLoopLockPtr			= NULL;

static CFTypeID				gCFRunLoopTypeID			= _kCFRuntimeNotATypeID;
static CFTypeID				gCFRunLoopSourceTypeID		= _kCFRuntimeNotATypeID;
static CFTypeID				gCFRunLoopTimerTypeID		= _kCFRuntimeNotATypeID;

static CFRunLoopRef			gCFRunLoopCurrent			= NULL;

//===========================================================================================================================
//	CFRunLoopEnsureInitialized
//===========================================================================================================================

OSStatus	CFRunLoopEnsureInitialized( void )
{
	OSStatus		err;
	
	if( gCFRunLoopCurrent ) { err = kNoErr; goto exit; }
	
	err = pthread_mutex_init( &gCFRunLoopLock, NULL );
	require_noerr( err, exit );
	gCFRunLoopLockPtr = &gCFRunLoopLock;
	
	gCFRunLoopTypeID = _CFRuntimeRegisterClass( &kCFRunLoopClass );
	require_action( gCFRunLoopTypeID != _kCFRuntimeNotATypeID, exit, err = kUnknownErr );
	
	gCFRunLoopSourceTypeID = _CFRuntimeRegisterClass( &kCFRunLoopSourceClass );
	require_action( gCFRunLoopSourceTypeID != _kCFRuntimeNotATypeID, exit, err = kUnknownErr );
	
	gCFRunLoopTimerTypeID = _CFRuntimeRegisterClass( &kCFRunLoopTimerClass );
	require_action( gCFRunLoopTimerTypeID != _kCFRuntimeNotATypeID, exit, err = kUnknownErr );
	
	err = __CFRunLoopCreate( &gCFRunLoopCurrent );
	require_noerr( err, exit );
	
exit:
	if( err ) CFRunLoopFinalize();
	return( err );
}

//===========================================================================================================================
//	CFRunLoopFinalize
//===========================================================================================================================

OSStatus	CFRunLoopFinalize( void )
{
	// NOTE: CF doesn't support unregistering CFTypes so we just have to leak them.
	
	ForgetCF( &gCFRunLoopCurrent );
	pthread_mutex_forget( &gCFRunLoopLockPtr );
	return( kNoErr );
}

//===========================================================================================================================
//	__CFRunLoopCreate
//===========================================================================================================================

DEBUG_STATIC OSStatus	__CFRunLoopCreate( CFRunLoopRef *outRL )
{
	OSStatus			err;
	CFRunLoopRef		rl;
	size_t				size;
	SocketRef			sock;
	sockaddr_ip			sip;
	socklen_t			len;
	
	sock = kInvalidSocketRef;
	
	size = sizeof( struct CFRunLoop ) - sizeof( CFRuntimeBase );
	rl = (CFRunLoopRef) _CFRuntimeCreateInstance( kCFAllocatorDefault, gCFRunLoopTypeID, (CFIndex) size, NULL );
	require_action( rl, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) rl ) + sizeof( CFRuntimeBase ), 0, size );
	
	rl->commandSock = kInvalidSocketRef;
	
	// Set up a loopback socket that's connected to itself so we can send/receive to the command thread.
	
	sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	err = map_socket_creation_errno( sock );
	require_noerr( err, exit );
	
	memset( &sip.v4, 0, sizeof( sip.v4 ) );
	sip.v4.sin_family		= AF_INET;
	sip.v4.sin_port			= 0;
	sip.v4.sin_addr.s_addr	= htonl( INADDR_LOOPBACK );
	err = bind( sock, &sip.sa, sizeof( sip.v4 ) );
	err = map_socket_noerr_errno( sock, err );
	require_noerr( err, exit );
	
	len = (socklen_t) sizeof( sip );
	err = getsockname( sock, &sip.sa, &len );
	err = map_socket_noerr_errno( sock, err );
	require_noerr( err, exit );
	
	err = connect( sock, &sip.sa, len );
	err = map_socket_noerr_errno( sock, err );
	require_noerr( err, exit );
	
	rl->commandSock = sock;
	sock = kInvalidSocketRef;
	
	*outRL = rl;
	rl = NULL;
	
exit:
	ForgetSocket( &sock );
	if( rl ) CFRelease( rl );
	return( err );
}

//===========================================================================================================================
//	__CFRunLoopFree
//===========================================================================================================================

DEBUG_STATIC void	__CFRunLoopFree( CFTypeRef inObj )
{
	CFRunLoopRef const		rl = (CFRunLoopRef) inObj;
	CFRunLoopTimerRef		timer;
	CFRunLoopSourceRef		source;
	
	while( ( timer = rl->timerList ) != NULL )
	{
		rl->timerList = timer->next;
		CFRunLoopTimerInvalidate( timer );
		CFRelease( timer );
	}
	while( ( source = rl->sourceList ) != NULL )
	{
		rl->sourceList = source->next;
		CFRunLoopSourceInvalidate( source );
		CFRelease( source );
	}
	ForgetSocket( &rl->commandSock );
}

//===========================================================================================================================
//	CFRunLoopGetTypeID
//===========================================================================================================================

CFTypeID	CFRunLoopGetTypeID( void )
{
	CFRunLoopEnsureInitialized();
	return( gCFRunLoopTypeID );
}

//===========================================================================================================================
//	CFRunLoopGetCurrent
//===========================================================================================================================

CFRunLoopRef	CFRunLoopGetCurrent( void )
{
	CFRunLoopEnsureInitialized();
	return( gCFRunLoopCurrent );
}

//===========================================================================================================================
//	CFRunLoopGetMain
//===========================================================================================================================

CFRunLoopRef	CFRunLoopGetMain( void )
{
	// CFLite doesn't do per-thread runloops yet so the current is the main.
	
	return( CFRunLoopGetCurrent() );
}

//===========================================================================================================================
//	CFRunLoopRun
//===========================================================================================================================

void	CFRunLoopRun( void )
{
	OSStatus			err;
	CFRunLoopRef		rl;
	
	err = CFRunLoopEnsureInitialized();
	require_noerr( err, exit );
	
	rl = CFRunLoopGetCurrent();
	rl->stop = false;
	while( !rl->stop )
	{
		err = __CFRunLoopRunOne( rl, NULL, false );
		if( err ) break;
	}
	
exit:
	return;
}

//===========================================================================================================================
//	CFRunLoopRunInMode
//===========================================================================================================================

int32_t	CFRunLoopRunInMode( CFStringRef inMode, CFTimeInterval inSeconds, Boolean inReturnAfterSourceHandled )
{
	int32_t				result;
	uint64_t			microseconds;
	struct timeval		timeout;
	OSStatus			err;
	CFRunLoopRef		rl;
	
	(void) inMode;
	
	if( inSeconds > 0 )
	{
		microseconds	= (uint64_t)( inSeconds    * 1000000.0 );
		timeout.tv_sec	= (int32_t)(  microseconds / 1000000 );
		timeout.tv_usec	= (int32_t)(  microseconds % 1000000 );
	}
	else
	{
		timeout.tv_sec	= 0;
		timeout.tv_usec	= 0;
	}
	rl = CFRunLoopGetCurrent();
	rl->stop = false;
	for( ;; )
	{
		err = __CFRunLoopRunOne( rl, &timeout, inReturnAfterSourceHandled );
		if( ( err == kNoErr ) && inReturnAfterSourceHandled )
		{
			result = kCFRunLoopRunHandledSource;
			break;
		}
		else if( err == ETIMEDOUT )
		{
			result = kCFRunLoopRunTimedOut;
			break;
		}
		else if( rl->stop )
		{
			result = kCFRunLoopRunStopped;
			break;
		}
	}
	return( result );
}

//===========================================================================================================================
//	CFRunLoopRunOne
//===========================================================================================================================

DEBUG_STATIC OSStatus	__CFRunLoopRunOne( CFRunLoopRef inRL, struct timeval *inTimeout, Boolean inReturnAfterSourceHandled )
{
	OSStatus				err;
	int						maxFD;
	fd_set					readSet;
	fd_set					writeSet;
	CFSocketRef				sock;
	uint64_t				timeoutTicks;
	uint64_t				nearestTicks;
	uint64_t				nowTicks;
	uint64_t				ticksPerSec;
	Boolean					sourceHandled;
	CFRunLoopSourceRef		rls;
	CFRunLoopTimerRef		timer;
	
	nowTicks = UpTicks();
	ticksPerSec = UpTicksPerSecond();
	if( inTimeout )
	{
		timeoutTicks  = nowTicks;
		timeoutTicks +=   ( ( (uint64_t) inTimeout->tv_sec )  * ticksPerSec );
		timeoutTicks += ( ( ( (uint32_t) inTimeout->tv_usec ) * kMicrosecondsPerSecond ) / ticksPerSec );
	}
	else
	{
		timeoutTicks = kUpTicksForever;
	}
	for( ;; )
	{
		// Set up the descriptors to wait for.
		
		FD_ZERO( &readSet );
		FD_ZERO( &writeSet );
		FD_SET( inRL->commandSock, &readSet );
		maxFD = (int) inRL->commandSock;
		for( rls = inRL->sourceList; rls; rls = rls->next )
		{
			if( rls->type == kCFRunLoopSourceType_Socket )
			{
				sock = (CFSocketRef) rls->context.info;
				if(   ( sock->callbackTypes & kCFSocketReadCallBack ) ||
					( ( sock->callbackTypes & kCFSocketConnectCallBack ) && sock->connecting ) )
				{
					FD_SET( sock->nativeSock, &readSet );
					if( sock->nativeSock > maxFD ) maxFD = sock->nativeSock;
				}
				if(   ( sock->callbackTypes & kCFSocketWriteCallBack ) ||
					( ( sock->callbackTypes & kCFSocketConnectCallBack ) && sock->connecting ) )
				{
					FD_SET( sock->nativeSock, &writeSet );
					if( sock->nativeSock > maxFD ) maxFD = sock->nativeSock;
				}
			}
		}
		
		// Set up the timeout for the nearest timer (sorted by expiration time).
		
		timer = inRL->timerList;
		nearestTicks = ( timer && ( timer->expireTicks < timeoutTicks ) ) ? timer->expireTicks : timeoutTicks;
		
		// Wait for an event.
		
		sourceHandled = false;
		err = __CFRunLoopSelect( maxFD + 1, &readSet, &writeSet, nearestTicks );
		if( err == kNoErr )
		{
			if( FD_ISSET( inRL->commandSock, &readSet ) )
			{
				__CFRunLoopCommandSocketHandler( inRL );
			}
			else
			{
				for( rls = inRL->sourceList; rls; rls = rls->next )
				{
					if( rls->type == kCFRunLoopSourceType_Socket )
					{
						sock = (CFSocketRef) rls->context.info;
						if( FD_ISSET( sock->nativeSock, &readSet ) )
						{
							__CFSocketHandler( sock, kCFSocketReadCallBack );
							sourceHandled = true;
							break;
						}
						if( FD_ISSET( sock->nativeSock, &writeSet ) )
						{
							__CFSocketHandler( sock, kCFSocketWriteCallBack );
							sourceHandled = true;
							break;
						}
					}
				}
				check( sourceHandled );
			}
		}
		else if( err != ETIMEDOUT )
		{
			dlogassert( "select() error: %#m", err );
			goto exit;
		}
		
		// Process signaled sources.
		
		CFRunLoopLock();
		while( !inRL->stop && ( !sourceHandled || !inReturnAfterSourceHandled ) )
		{
			rls = inRL->signaledSourceList;
			if( !rls ) break;
			inRL->signaledSourceList = rls->nextSignaled;
				
			CFRunLoopUnlock();
			check( rls->context.perform );
			rls->context.perform( rls->context.info );
			CFRunLoopLock();
			
			sourceHandled = true;
		}
		
		// Process timers.
		
		nowTicks = UpTicks();
		while( !inRL->stop && ( !sourceHandled || !inReturnAfterSourceHandled ) )
		{
			Boolean		invalidate;
			
			for( timer = inRL->timerList; timer; timer = timer->next )
			{
				if( nowTicks >= timer->expireTicks )
				{
					break;
				}
			}
			if( !timer ) break;
			
			if( timer->intervalTicks > 0 )
			{
				timer->expireTicks += timer->intervalTicks;
				__CFRunLoopTimerRequeue( timer );
				invalidate = false;
			}
			else
			{
				invalidate = true;
			}
			CFRunLoopUnlock();
			
			if( invalidate )
			{
				CFRetain( timer ); // Retain so it's not released until after the callback returns.
				CFRunLoopTimerInvalidate( timer );
			}
			timer->callback( timer, timer->context );
			if( invalidate ) CFRelease( timer );
			CFRunLoopLock();
			
			sourceHandled = true;
		}
		CFRunLoopUnlock();
		
		if( ( sourceHandled && inReturnAfterSourceHandled ) || inRL->stop )
		{
			err = kNoErr;
			break;
		}
		if( nowTicks >= timeoutTicks )
		{
			err = ETIMEDOUT;
			break;
		}
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	__CFRunLoopSelect
//===========================================================================================================================

DEBUG_STATIC OSStatus	__CFRunLoopSelect( int inMaxFD, fd_set *inReadSet, fd_set *inWriteSet, uint64_t inTimeoutTicks )
{
	OSStatus				err;
	int						n;
	struct timeval			timeout;
	struct timeval *		timeoutPtr;
	uint64_t				nowTicks;
	uint64_t				ticksPerSec;
	uint64_t				deltaTicks;
	
	ticksPerSec = UpTicksPerSecond();
	do
	{
		nowTicks = UpTicks();
		if( inTimeoutTicks == kUpTicksForever )
		{
			timeoutPtr = NULL;
		}
		else if( nowTicks >= inTimeoutTicks )
		{
			timeout.tv_sec	= 0;
			timeout.tv_usec	= 0;
			timeoutPtr		= &timeout;
		}
		else
		{
			deltaTicks		= inTimeoutTicks - nowTicks;
			timeout.tv_sec	= (int32_t)(     deltaTicks / ticksPerSec );
			timeout.tv_usec	= (int32_t)( ( ( deltaTicks % ticksPerSec ) * kMicrosecondsPerSecond ) / ticksPerSec );
			timeoutPtr		= &timeout;
		}
		
		n = select( inMaxFD, inReadSet, inWriteSet, NULL, timeoutPtr );
		if( n  > 0 ) { err = kNoErr;	break; }
		if( n == 0 ) { err = ETIMEDOUT;	break; }
		err = global_value_errno( n );
		
	}	while( err == EINTR );
	
	return( err );
}

//===========================================================================================================================
//	CFRunLoopStop
//===========================================================================================================================

void	CFRunLoopStop( CFRunLoopRef inRL )
{
	inRL->stop = true;
	CFRunLoopWakeUp( inRL );
}

//===========================================================================================================================
//	CFRunLoopWakeUp
//===========================================================================================================================

void	CFRunLoopWakeUp( CFRunLoopRef inRL )
{
	OSStatus		err;
	ssize_t			n;
	
	DEBUG_USE_ONLY( err );
	
	n = send( inRL->commandSock, "w", 1, 0 );
	err = map_socket_value_errno( inRL->commandSock, n == 1, n );
	check_noerr( err );
}

//===========================================================================================================================
//	__CFRunLoopCommandSocketHandler
//===========================================================================================================================

DEBUG_STATIC void	__CFRunLoopCommandSocketHandler( CFRunLoopRef inRL )
{
	OSStatus		err;
	ssize_t			n;
	char			cmd;
	
	n = recv( inRL->commandSock, &cmd, 1, 0 );
	err = map_socket_value_errno( inRL->commandSock, n == 1, n );
	require_noerr( err, exit );
	
	if( cmd == 'w' ) // Wakeup
	{
		// Wakeups just break us out of kevent so we can process signaled sources so there's nothing to here.
	}
	else
	{
		dlog( kLogLevelError, "%s: ### unknown control cmd '%c'\n", __ROUTINE__, cmd );
		goto exit;
	}
	
exit:
	return;
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	CFRunLoopAddSource
//===========================================================================================================================

void	CFRunLoopAddSource( CFRunLoopRef inRL, CFRunLoopSourceRef inSource, CFStringRef inMode )
{
	CFRetain( inSource );
	CFRunLoopLock();
		inSource->rl		= inRL;
		inSource->next		= inRL->sourceList;
		inRL->sourceList	= inSource;
		if( inSource->context.schedule ) inSource->context.schedule( inSource->context.info, inRL, inMode );
	CFRunLoopUnlock();
}

//===========================================================================================================================
//	CFRunLoopRemoveSource
//===========================================================================================================================

void	CFRunLoopRemoveSource( CFRunLoopRef inRL, CFRunLoopSourceRef inSource, CFStringRef inMode )
{
	CFRunLoopSourceRef *		next;
	CFRunLoopSourceRef			curr;
	
	curr = NULL;
	CFRunLoopLock();
	if( !inRL ) inRL = inSource->rl;
	if( inRL )
	{
		for( next = &inRL->signaledSourceList; ( curr = *next ) != NULL; next = &curr->nextSignaled )
		{
			if( curr == inSource )
			{
				*next = curr->nextSignaled;
				break;
			}
		}
		
		for( next = &inRL->sourceList; ( curr = *next ) != NULL; next = &curr->next )
		{
			if( curr == inSource )
			{
				*next = curr->next;
				if( curr->context.cancel ) curr->context.cancel( curr->context.info, inRL, inMode );
				break;
			}
		}
	}
	CFRunLoopUnlock();
	if( curr ) CFRelease( curr );
}

//===========================================================================================================================
//	CFRunLoopAddTimer
//===========================================================================================================================

void	CFRunLoopAddTimer( CFRunLoopRef inRL, CFRunLoopTimerRef inTimer, CFStringRef inMode )
{
	CFRunLoopTimerRef *		next;
	CFRunLoopTimerRef		curr;
	
	(void) inMode;
	
	// Insert the timer sorted by expiration time.
	
	CFRetain( inTimer );
	CFRunLoopLock();
		inTimer->rl = inRL;
		for( next = &inRL->timerList; ( curr = *next ) != NULL; next = &curr->next )
		{
			if( inTimer->expireTicks < curr->expireTicks )
			{
				break;
			}
		}
		inTimer->next = *next;
		*next = inTimer;
	CFRunLoopUnlock();
}

//===========================================================================================================================
//	CFRunLoopRemoveTimer
//===========================================================================================================================

void	CFRunLoopRemoveTimer( CFRunLoopRef inRL, CFRunLoopTimerRef inTimer, CFStringRef inMode )
{
	CFRunLoopTimerRef *		next;
	CFRunLoopTimerRef		curr;
	
	(void) inMode;
	
	curr = NULL;
	CFRunLoopLock();
	if( !inRL ) inRL = inTimer->rl;
	if( inRL )
	{
		for( next = &inRL->timerList; ( curr = *next ) != NULL; next = &curr->next )
		{
			if( curr == inTimer )
			{
				*next = curr->next;
				curr->rl = NULL;
				break;
			}
		}
	}
	CFRunLoopUnlock();
	if( curr ) CFRelease( curr );
}

#if 0
#pragma mark -
#pragma mark == CFRunLoopSource ==
#endif

//===========================================================================================================================
//	CFRunLoopSourceGetTypeID
//===========================================================================================================================

CFTypeID	CFRunLoopSourceGetTypeID( void )
{
	CFRunLoopEnsureInitialized();
	return( gCFRunLoopSourceTypeID );
}

//===========================================================================================================================
//	CFRunLoopSourceCreate
//===========================================================================================================================

CFRunLoopSourceRef	CFRunLoopSourceCreate( CFAllocatorRef inAllocator, CFIndex inOrder, CFRunLoopSourceContext *inContext )
{
	CFRunLoopSourceRef		obj;
	size_t					size;
	
	(void) inOrder; // Unused
	
	size = sizeof( struct CFRunLoopSource ) - sizeof( CFRuntimeBase );
	obj = (CFRunLoopSourceRef) _CFRuntimeCreateInstance( inAllocator, CFRunLoopSourceGetTypeID(), (CFIndex) size, NULL );
	require( obj, exit );
	memset( ( (uint8_t *) obj ) + sizeof( CFRuntimeBase ), 0, size );
	
	obj->context	= *inContext;
	obj->type		= kCFRunLoopSourceType_Generic;
	
exit:
	return( obj );
}

//===========================================================================================================================
//	__CFRunLoopSourceFree
//===========================================================================================================================

DEBUG_STATIC void	__CFRunLoopSourceFree( CFTypeRef inObj )
{
	CFRunLoopRemoveSource( NULL, (CFRunLoopSourceRef) inObj, kCFRunLoopCommonModes );
}

//===========================================================================================================================
//	CFRunLoopSourceInvalidate
//===========================================================================================================================

void	CFRunLoopSourceInvalidate( CFRunLoopSourceRef inSource )
{
	CFRunLoopRemoveSource( NULL, inSource, kCFRunLoopCommonModes );
}

//===========================================================================================================================
//	CFRunLoopSourceSignal
//===========================================================================================================================

void	CFRunLoopSourceSignal( CFRunLoopSourceRef inSource )
{
	Boolean						wakeup;
	CFRunLoopRef				rl;
	CFRunLoopSourceRef *		next;
	CFRunLoopSourceRef			curr;
	
	wakeup = false;
	rl = NULL;
	CFRunLoopLock();
	if( inSource->context.perform )
	{
		rl = inSource->rl;
		for( next = &rl->signaledSourceList; ( curr = *next ) != NULL; next = &curr->nextSignaled )
		{
			if( curr == inSource )
			{
				break;
			}
		}
		if( !curr )
		{
			inSource->nextSignaled = NULL;
			*next = inSource;
			
			wakeup = true;
		}
	}
	CFRunLoopUnlock();
	if( wakeup ) CFRunLoopWakeUp( rl );
}

#if 0
#pragma mark -
#pragma mark == CFRunLoopTimer ==
#endif

//===========================================================================================================================
//	CFAbsoluteTimeGetCurrent
//===========================================================================================================================

CFAbsoluteTime	CFAbsoluteTimeGetCurrent( void )
{
	static Boolean		sInitialized = false;
	static double		sUpTicksToSecondsMultiplier;
	
	if( !sInitialized )
	{
		sUpTicksToSecondsMultiplier = 1.0 / UpTicksPerSecond();
		sInitialized = true;
	}
	return( ( (double) UpTicks() ) * sUpTicksToSecondsMultiplier );
}

//===========================================================================================================================
//	CFRunLoopTimerGetTypeID
//===========================================================================================================================

CFTypeID	CFRunLoopTimerGetTypeID( void )
{
	CFRunLoopEnsureInitialized();
	return( gCFRunLoopTimerTypeID );
}

//===========================================================================================================================
//	CFRunLoopTimerCreate
//===========================================================================================================================

CFRunLoopTimerRef
	CFRunLoopTimerCreate( 
		CFAllocatorRef			inAllocator,
		CFAbsoluteTime			inFireDate,
		CFTimeInterval			inInterval,
		CFOptionFlags			inFlags,
		CFIndex					inOrder,
		CFRunLoopTimerCallBack	inCallBack,
		CFRunLoopTimerContext *	inContext )
{
	CFRunLoopTimerRef		obj;
	size_t					size;
	
	(void) inFlags;
	(void) inOrder;
	
	obj = NULL;
	require( inCallBack, exit );
	require( inInterval <= ( 0x7FFFFFFF / 1000 ), exit );
	
	size = sizeof( struct CFRunLoopTimer ) - sizeof( CFRuntimeBase );
	obj = (CFRunLoopTimerRef) _CFRuntimeCreateInstance( inAllocator, CFRunLoopTimerGetTypeID(), (CFIndex) size, NULL );
	require( obj, exit );
	memset( ( (uint8_t *) obj ) + sizeof( CFRuntimeBase ), 0, size );
	
	obj->expireTicks	= (uint64_t)( inFireDate * UpTicksPerSecond() );
	obj->intervalTicks	= ( inInterval > 0 ) ? (uint64_t)( inInterval * UpTicksPerSecond() ) : 0;
	obj->callback		= inCallBack;
	obj->context		= inContext->info;
	
exit:
	return( obj );
}

//===========================================================================================================================
//	__CFRunLoopTimerFree
//===========================================================================================================================

DEBUG_STATIC void	__CFRunLoopTimerFree( CFTypeRef inObj )
{
	CFRunLoopRemoveTimer( NULL, (CFRunLoopTimerRef) inObj, kCFRunLoopCommonModes );
}

//===========================================================================================================================
//	CFRunLoopTimerInvalidate
//===========================================================================================================================

void	CFRunLoopTimerInvalidate( CFRunLoopTimerRef inTimer )
{
	CFRunLoopRemoveTimer( NULL, inTimer, kCFRunLoopCommonModes );
}

//===========================================================================================================================
//	CFRunLoopTimerSetNextFireDate
//===========================================================================================================================

void	CFRunLoopTimerSetNextFireDate( CFRunLoopTimerRef inTimer, CFAbsoluteTime inFireDate )
{
	CFRunLoopLock();
		inTimer->expireTicks = (uint64_t)( inFireDate * UpTicksPerSecond() );
		__CFRunLoopTimerRequeue( inTimer );
	CFRunLoopUnlock();
}

//===========================================================================================================================
//	__CFRunLoopTimerRequeue
//===========================================================================================================================

DEBUG_STATIC void	__CFRunLoopTimerRequeue( CFRunLoopTimerRef inTimer )
{
	CFRunLoopRef			rl;
	CFRunLoopTimerRef *		next;
	CFRunLoopTimerRef		curr;
	
	rl = inTimer->rl;
	if( rl )
	{
		// Remove and re-insert, sorted by the new expiration time.
		
		for( next = &rl->timerList; ( curr = *next ) != NULL; next = &curr->next )
		{
			if( curr == inTimer )
			{
				*next = curr->next;
				break;
			}
		}
		if( curr )
		{
			for( next = &rl->timerList; ( curr = *next ) != NULL; next = &curr->next )
			{
				if( inTimer->expireTicks < curr->expireTicks )
				{
					break;
				}
			}
			inTimer->next = *next;
			*next = inTimer;
		}
	}
}

#if 0
#pragma mark -
#pragma mark == CFSocket ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

DEBUG_STATIC OSStatus	__CFSocketEnsureInitialized( void );
DEBUG_STATIC void		__CFSocketFree( CFTypeRef inObj );
DEBUG_STATIC void		__CFSocketSourceSchedule( void *inInfo, CFRunLoopRef inRL, CFStringRef inMode );
DEBUG_STATIC void		__CFSocketSourceCancel( void *inInfo, CFRunLoopRef inRL, CFStringRef inMode );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static const CFRuntimeClass		kCFSocketClass =
{
	0, 					// version
	"CFSocket",			// className
	NULL,				// init
	NULL, 				// copy
	__CFSocketFree,		// finalize
	NULL,				// equal
	NULL,				// hash
	NULL, 				// copyFormattingDesc
	NULL,				// copyDebugDesc
	NULL,				// reclaim
	NULL				// refcount
};

static CFTypeID		gCFSocketTypeID = _kCFRuntimeNotATypeID;

//===========================================================================================================================
//	__CFSocketEnsureInitialized
//===========================================================================================================================

DEBUG_STATIC OSStatus	__CFSocketEnsureInitialized( void )
{
	OSStatus		err;
	
	CFRunLoopEnsureInitialized();
	CFRunLoopLock();
	
	if( gCFSocketTypeID == _kCFRuntimeNotATypeID )
	{
		gCFSocketTypeID = _CFRuntimeRegisterClass( &kCFSocketClass );
		require_action( gCFSocketTypeID != _kCFRuntimeNotATypeID, exit, err = kUnknownErr );
	}
	err = kNoErr;
	
exit:
	CFRunLoopUnlock();
	return( err );
}

//===========================================================================================================================
//	CFSocketGetTypeID
//===========================================================================================================================

CFTypeID	CFSocketGetTypeID( void )
{
	__CFSocketEnsureInitialized();
	return( gCFSocketTypeID );
}

//===========================================================================================================================
//	CFSocketCreateWithNative
//===========================================================================================================================

CFSocketRef
	CFSocketCreateWithNative( 
		CFAllocatorRef			inAllocator, 
		CFSocketNativeHandle	inSock, 
		CFOptionFlags			inCallBackTypes, 
		CFSocketCallBack		inCallBack, 
		const CFSocketContext *	inContext )
{
	CFSocketRef		obj;
	size_t			size;
	
	require_action( IsValidSocket( inSock ), exit, obj = NULL );
	
	size = sizeof( struct CFSocket ) - sizeof( CFRuntimeBase );
	obj = (CFSocketRef) _CFRuntimeCreateInstance( inAllocator, CFSocketGetTypeID(), (CFIndex) size, NULL );
	require( obj, exit );
	memset( ( (uint8_t *) obj ) + sizeof( CFRuntimeBase ), 0, size );
	
	obj->nativeSock		= inSock;
	obj->flags			= kCFSocketAutomaticallyReenableReadCallBack	|
						  kCFSocketAutomaticallyReenableAcceptCallBack	|
						  kCFSocketAutomaticallyReenableDataCallBack	|
						  kCFSocketCloseOnInvalidate;
	obj->callbackTypes	= inCallBackTypes;
	obj->callback		= inCallBack;
	obj->context		= *inContext;
	
exit:
	return( obj );
}

//===========================================================================================================================
//	__CFSocketFree
//===========================================================================================================================

DEBUG_STATIC void	__CFSocketFree( CFTypeRef inObj )
{
	CFSocketInvalidate( (CFSocketRef) inObj );
}

//===========================================================================================================================
//	CFSocketInvalidate
//===========================================================================================================================

void	CFSocketInvalidate( CFSocketRef inSock )
{
	CFRunLoopSourceRef		source;
	
	CFRunLoopLock();
		if( inSock->flags & kCFSocketCloseOnInvalidate )
		{
			ForgetSocket( &inSock->nativeSock );
		}
		source = inSock->source;
		inSock->source = NULL;
	CFRunLoopUnlock();
	
	if( source )
	{
		CFRunLoopSourceInvalidate( source );
		CFRelease( source );
	}
}

//===========================================================================================================================
//	CFSocketConnectToAddress
//===========================================================================================================================

CFSocketError	CFSocketConnectToAddress( CFSocketRef inSock, CFDataRef inAddr, CFTimeInterval inTimeout )
{
	CFSocketError				err;
	const struct sockaddr *		sa;
	socklen_t					realLen;
	socklen_t					len;
	int							connectErr;
	
	require_action( !inSock->connecting, exit, err = kCFSocketError );
	require_action( inSock->callbackTypes & kCFSocketConnectCallBack, exit, err = kCFSocketError );
	require_action( inTimeout < 0, exit, err = kCFSocketError ); // We don't suport synchronous behavior.
	
	// Make sure the address is valid.
	
	sa = (const struct sockaddr *) CFDataGetBytePtr( inAddr );
	realLen = (socklen_t) CFDataGetLength( inAddr );
	require_action( realLen >= sizeof( *sa ), exit, err = kCFSocketError );
	len = SockAddrGetSize( sa );
	require_action( realLen >= len, exit, err = kCFSocketError );
	
	// Start the connection process. We'll handle the connection from the readable or writable callback.
	
	inSock->connecting = true;
	connectErr = connect( inSock->nativeSock, sa, len );
	connectErr = map_socket_noerr_errno( inSock->nativeSock, connectErr );
	require_action_quiet( ( connectErr == 0 ) || ( connectErr == EINPROGRESS ), exit, err = kCFSocketError );
	
	err = kCFSocketSuccess;
	
exit:
	return( err );
}

//===========================================================================================================================
//	CFSocketGetNative
//===========================================================================================================================

CFSocketNativeHandle	CFSocketGetNative( CFSocketRef inSock )
{
	return( inSock->nativeSock );
}

//===========================================================================================================================
//	CFSocketGetSocketFlags
//===========================================================================================================================

CFOptionFlags	CFSocketGetSocketFlags( CFSocketRef inSock )
{
	return( inSock->flags );
}

//===========================================================================================================================
//	CFSocketSetSocketFlags
//===========================================================================================================================

void	CFSocketSetSocketFlags( CFSocketRef inSock, CFOptionFlags inFlags )
{
	inSock->flags = inFlags;
}

//===========================================================================================================================
//	CFSocketEnableCallBacks
//===========================================================================================================================

void	CFSocketEnableCallBacks( CFSocketRef inSock, CFOptionFlags inCallBackTypes )
{
	CFRunLoopLock();
		inSock->callbackTypes |= inCallBackTypes;
	CFRunLoopUnlock();
}

//===========================================================================================================================
//	CFSocketDisableCallBacks
//===========================================================================================================================

void	CFSocketDisableCallBacks( CFSocketRef inSock, CFOptionFlags inCallBackTypes )
{
	CFRunLoopLock();
		inSock->callbackTypes &= ~inCallBackTypes;
	CFRunLoopUnlock();
}

//===========================================================================================================================
//	__CFSocketHandler
//===========================================================================================================================

DEBUG_STATIC void	__CFSocketHandler( CFSocketRef inSock, CFOptionFlags inCallBackType )
{
	Boolean			call;
	int32_t			connectErr;
	int				socketErr;
	socklen_t		socketErrSize;
	const void *	callbackData;
	
	CFRunLoopLock();
	
	// If we're connecting then simulate a connect event on reads or writes.
	
	callbackData = NULL;
	if( inSock->connecting )
	{
		socketErr = 0;
		socketErrSize = (socklen_t) sizeof( socketErr );
		connectErr = getsockopt( inSock->nativeSock, SOL_SOCKET, SO_ERROR, (char *) &socketErr, &socketErrSize );
		if( connectErr == 0 )	connectErr   = socketErr;
		if( connectErr )		callbackData = &connectErr;
		
		check( inSock->callbackTypes & kCFSocketConnectCallBack );
		inCallBackType = kCFSocketConnectCallBack;
		inSock->connecting = false;
		call = true;
	}
	else
	{
		call = ( ( inSock->callbackTypes & inCallBackType ) != 0 );
	}
	if( call )
	{
		if( ( inCallBackType & kCFSocketReadCallBack ) && !( inSock->flags & kCFSocketAutomaticallyReenableReadCallBack ) )
		{
			inSock->callbackTypes &= ~( (CFOptionFlags) kCFSocketReadCallBack );
		}
		if( ( inCallBackType & kCFSocketWriteCallBack ) && !( inSock->flags & kCFSocketAutomaticallyReenableWriteCallBack ) )
		{
			inSock->callbackTypes &= ~( (CFOptionFlags) kCFSocketWriteCallBack );
		}
	}
	CFRunLoopUnlock();
	if( call ) inSock->callback( inSock, inCallBackType, NULL, callbackData, inSock->context.info );
}

//===========================================================================================================================
//	CFSocketCreateRunLoopSource
//===========================================================================================================================

CFRunLoopSourceRef	CFSocketCreateRunLoopSource( CFAllocatorRef inAllocator, CFSocketRef inSock, CFIndex inOrder )
{
	CFRunLoopSourceRef			source;
	CFRunLoopSourceContext		context;
	
	CFRunLoopLock();
	
	source = inSock->source;
	if( !source )
	{
		memset( &context, 0, sizeof( context ) );
		context.info		= inSock;
		context.schedule	= __CFSocketSourceSchedule;
		context.cancel		= __CFSocketSourceCancel;
		
		source = CFRunLoopSourceCreate( inAllocator, inOrder, &context );
		require( source, exit );
		source->type = kCFRunLoopSourceType_Socket;
		
		inSock->source = source;
	}
	CFRetain( source ); // Always retain so there is 1 extra reference for ourself.
	
exit:
	CFRunLoopUnlock();
	return( source );
}

//===========================================================================================================================
//	__CFSocketSourceSchedule
//
//	Warning: Assumes CFRunLoopLock is held.
//===========================================================================================================================

DEBUG_STATIC void	__CFSocketSourceSchedule( void *inInfo, CFRunLoopRef inRL, CFStringRef inMode )
{
	CFSocketRef const		sock = (CFSocketRef) inInfo;
	
	(void) inMode;
	
	sock->rl = inRL;
}

//===========================================================================================================================
//	__CFSocketSourceCancel
//
//	Warning: Assumes CFRunLoopLock is held.
//===========================================================================================================================

DEBUG_STATIC void	__CFSocketSourceCancel( void *inInfo, CFRunLoopRef inRL, CFStringRef inMode )
{
	CFSocketRef const		sock = (CFSocketRef) inInfo;
	
	(void) inRL;
	(void) inMode;
	
	sock->rl = NULL;
}

#if 0
#pragma mark -
#pragma mark == CFFileDescriptor ==
#endif

#if( TARGET_OS_POSIX )

//===========================================================================================================================
//	Types
//===========================================================================================================================

struct	CFFileDescriptor
{
	CFRuntimeBase					base;
	CFSocketRef						sock;
	CFFileDescriptorCallBack		callback;
	CFFileDescriptorContext			context;
	Boolean							invalidated;
};

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

DEBUG_STATIC void		__CFFileDescriptorFree( CFTypeRef inObj );
DEBUG_STATIC void
	__CFFileDescriptorSocketCallBack( 
		CFSocketRef				inSock, 
		CFSocketCallBackType	inType, 
		CFDataRef				inAddr,
		const void *			inData,
		void *					inContext );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static const CFRuntimeClass		kCFFileDescriptorClass =
{
	0, 						// version
	"CFFileDescriptor",		// className
	NULL,					// init
	NULL, 					// copy
	__CFFileDescriptorFree,	// finalize
	NULL,					// equal
	NULL,					// hash
	NULL, 					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

static CFTypeID		gCFFileDescriptorTypeID = _kCFRuntimeNotATypeID;

//===========================================================================================================================
//	CFFileDescriptorGetTypeID
//===========================================================================================================================

CFTypeID	CFFileDescriptorGetTypeID( void )
{
	CFRunLoopEnsureInitialized();
	CFRunLoopLock();
	
	if( gCFFileDescriptorTypeID == _kCFRuntimeNotATypeID )
	{
		gCFFileDescriptorTypeID = _CFRuntimeRegisterClass( &kCFFileDescriptorClass );
		require( gCFFileDescriptorTypeID != _kCFRuntimeNotATypeID, exit );
	}
	
exit:
	CFRunLoopUnlock();
	return( gCFFileDescriptorTypeID );
}

//===========================================================================================================================
//	CFFileDescriptorCreate
//===========================================================================================================================

CFFileDescriptorRef
	CFFileDescriptorCreate( 
		CFAllocatorRef						inAllocator, 
		CFFileDescriptorNativeDescriptor	inFD, 
		Boolean								inCloseOnInvalidate, 
		CFFileDescriptorCallBack			inCallBack, 
		const CFFileDescriptorContext *		inContext )
{
	CFFileDescriptorRef		result;
	CFFileDescriptorRef		obj;
	size_t					len;
	CFSocketContext			socketContext = { 0, NULL, NULL, NULL, NULL };
	CFOptionFlags			flags;
	
	result = NULL;
	
	len = sizeof( struct CFFileDescriptor ) - sizeof( CFRuntimeBase );
	obj = (CFFileDescriptorRef) _CFRuntimeCreateInstance( inAllocator, CFFileDescriptorGetTypeID(), (CFIndex) len, NULL );
	require( obj, exit );
	memset( ( (uint8_t *) obj ) + sizeof( CFRuntimeBase ), 0, len );
	
	obj->callback = inCallBack;
	obj->context  = *inContext;
	
	socketContext.info = obj;
	obj->sock = CFSocketCreateWithNative( inAllocator, inFD, kCFSocketReadCallBack | kCFSocketWriteCallBack, 
		__CFFileDescriptorSocketCallBack, &socketContext );
	require( obj->sock, exit );
	
	flags = CFSocketGetSocketFlags( obj->sock );
	if( inCloseOnInvalidate )	flags |=  kCFSocketCloseOnInvalidate;
	else						flags &= ~( (CFOptionFlags) kCFSocketCloseOnInvalidate );
	CFSocketSetSocketFlags( obj->sock, flags );
	CFSocketDisableCallBacks( obj->sock, kCFSocketReadCallBack | kCFSocketWriteCallBack );
	
	result = obj;
	obj = NULL;
	
exit:
	if( obj ) CFRelease( obj );
	return( result );
}

//===========================================================================================================================
//	__CFFileDescriptorFree
//===========================================================================================================================

DEBUG_STATIC void	__CFFileDescriptorFree( CFTypeRef inObj )
{
	CFSocketInvalidate( ( (CFFileDescriptorRef) inObj )->sock );
}

//===========================================================================================================================
//	__CFFileDescriptorSocketCallBack
//===========================================================================================================================

DEBUG_STATIC void
	__CFFileDescriptorSocketCallBack( 
		CFSocketRef				inSock, 
		CFSocketCallBackType	inType, 
		CFDataRef				inAddr,
		const void *			inData,
		void *					inContext )
{
	CFFileDescriptorRef const		fileDesc = (CFFileDescriptorRef) inContext;
	CFOptionFlags					callbackTypes;
	
	(void) inSock; // Unused
	(void) inAddr; // Unused
	(void) inData; // Unused
	
	callbackTypes = 0;
	if( inType & kCFSocketReadCallBack )	callbackTypes |= kCFFileDescriptorReadCallBack;
	if( inType & kCFSocketWriteCallBack )	callbackTypes |= kCFFileDescriptorWriteCallBack;
	fileDesc->callback( fileDesc, callbackTypes, fileDesc->context.info );
}

//===========================================================================================================================
//	CFFileDescriptorGetNativeDescriptor
//===========================================================================================================================

CFFileDescriptorNativeDescriptor	CFFileDescriptorGetNativeDescriptor( CFFileDescriptorRef inDesc )
{
	return( CFSocketGetNative( inDesc->sock ) );
}

//===========================================================================================================================
//	CFFileDescriptorGetContext
//===========================================================================================================================

void	CFFileDescriptorGetContext( CFFileDescriptorRef inDesc, CFFileDescriptorContext *inContext )
{
	*inContext = inDesc->context;
}

//===========================================================================================================================
//	CFFileDescriptorEnableCallBacks
//===========================================================================================================================

void	CFFileDescriptorEnableCallBacks( CFFileDescriptorRef inDesc, CFOptionFlags inCallBackTypes )
{
	CFOptionFlags		callbackTypes;
	
	callbackTypes = 0;
	if( inCallBackTypes & kCFFileDescriptorReadCallBack )	callbackTypes |= kCFSocketReadCallBack;
	if( inCallBackTypes & kCFFileDescriptorWriteCallBack )	callbackTypes |= kCFSocketWriteCallBack;
	CFSocketEnableCallBacks( inDesc->sock, callbackTypes );
}

//===========================================================================================================================
//	CFFileDescriptorDisableCallBacks
//===========================================================================================================================

void	CFFileDescriptorDisableCallBacks( CFFileDescriptorRef inDesc, CFOptionFlags inCallBackTypes )
{
	CFOptionFlags		callbackTypes;
	
	callbackTypes = 0;
	if( inCallBackTypes & kCFFileDescriptorReadCallBack )	callbackTypes |= kCFSocketReadCallBack;
	if( inCallBackTypes & kCFFileDescriptorWriteCallBack )	callbackTypes |= kCFSocketWriteCallBack;
	CFSocketDisableCallBacks( inDesc->sock, callbackTypes );
}

//===========================================================================================================================
//	CFFileDescriptorInvalidate
//===========================================================================================================================

void	CFFileDescriptorInvalidate( CFFileDescriptorRef inDesc )
{
	inDesc->invalidated = true;
	CFSocketInvalidate( inDesc->sock );
}

//===========================================================================================================================
//	CFFileDescriptorIsValid
//===========================================================================================================================

Boolean	CFFileDescriptorIsValid( CFFileDescriptorRef inDesc )
{
	return( !inDesc->invalidated );
}

//===========================================================================================================================
//	CFFileDescriptorCreateRunLoopSource
//===========================================================================================================================

CFRunLoopSourceRef	CFFileDescriptorCreateRunLoopSource( CFAllocatorRef inAllocator, CFFileDescriptorRef inDesc, CFIndex inOrder )
{
	return( CFSocketCreateRunLoopSource( inAllocator, inDesc->sock, inOrder ) );
}
#endif // TARGET_OS_POSIX

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )

static CFRunLoopSourceRef		gCFLiteRunLoopTestSource	= NULL;
#if( TARGET_OS_POSIX )
	static ssize_t				gCFLiteRunLoopTestReadBytes	= 0;
#endif
static int						gCFLiteRunLoopValue			= 0;
static OSStatus					gCFLiteRunLoopSocketError	= 0;

DEBUG_STATIC void	CFLiteRunLoopTestTimer( CFRunLoopTimerRef inTimer, void *inContext );
DEBUG_STATIC void	CFLiteRunLoopTestSourcePerform( void *inContext );
DEBUG_STATIC void
	CFLiteRunLoopSocketConnectCallBack( 
		CFSocketRef				inSock, 
		CFSocketCallBackType	inType, 
		CFDataRef				inAddr,
		const void *			inData,
		void *					inContext );
#if( TARGET_OS_POSIX )
	DEBUG_STATIC void
		CFLiteRunLoopFileDescriptorCallBack( 
			CFFileDescriptorRef	inDesc, 
			CFOptionFlags		inCallBackTypes, 
			void *				inContext );
#endif

//===========================================================================================================================
//	CFLiteRunLoopTestTimer
//===========================================================================================================================

DEBUG_STATIC void	CFLiteRunLoopTestTimer( CFRunLoopTimerRef inTimer, void *inContext )
{
	DEBUG_USE_ONLY( inContext );
	
	check( inContext == ( (void *)(uintptr_t) 0x12345678 ) );
	
	dlog( kLogLevelMax, "%N: %s: %p, %p\n", __ROUTINE__, (void *) inTimer, inContext );
	
	++gCFLiteRunLoopValue;
	if( gCFLiteRunLoopValue == 3 )
	{
		CFRunLoopTimerSetNextFireDate( inTimer, CFAbsoluteTimeGetCurrent() + 10.0 );
	}
	if( gCFLiteRunLoopValue == 10 )
	{
		CFRunLoopRemoveTimer( CFRunLoopGetCurrent(), inTimer, kCFRunLoopCommonModes );
		CFRunLoopStop( CFRunLoopGetCurrent() );
	}
}

//===========================================================================================================================
//	CFLiteRunLoopTestSourcePerform
//===========================================================================================================================

DEBUG_STATIC void	CFLiteRunLoopTestSourcePerform( void *inContext )
{
	DEBUG_USE_ONLY( inContext );
	
	dlog( kLogLevelMax, "%s: %p\n", __ROUTINE__, inContext );
	
	CFRunLoopRemoveSource( CFRunLoopGetCurrent(), gCFLiteRunLoopTestSource, kCFRunLoopCommonModes );
}

//===========================================================================================================================
//	CFLiteRunLoopTestSocket
//===========================================================================================================================

DEBUG_STATIC void
	CFLiteRunLoopSocketConnectCallBack( 
		CFSocketRef				inSock, 
		CFSocketCallBackType	inType, 
		CFDataRef				inAddr,
		const void *			inData,
		void *					inContext )
{
	int32_t *		socketErrPtr;
	
	(void) inSock;		// Unused
	(void) inType;		// Unused
	(void) inAddr;		// Unused
	(void) inContext;	// Unused
	
	socketErrPtr = (int32_t *) inData;
	if( socketErrPtr )
	{
		gCFLiteRunLoopSocketError = *socketErrPtr;
	}
	else
	{
		gCFLiteRunLoopSocketError = kNoErr;
	}
	dlog( kLogLevelNotice, "test connect callback: %#m\n", gCFLiteRunLoopSocketError );
	CFRunLoopStop( CFRunLoopGetCurrent() );
}

//===========================================================================================================================
//	CFLiteRunLoopFileDescriptorCallBack
//===========================================================================================================================

#if( TARGET_OS_POSIX )
DEBUG_STATIC void
	CFLiteRunLoopFileDescriptorCallBack( 
		CFFileDescriptorRef	inDesc, 
		CFOptionFlags		inCallBackTypes, 
		void *				inContext )
{
	CFFileDescriptorNativeDescriptor		nativeFD;
	ssize_t									n;
	char									buf[ 8 ];
	OSStatus								err;
	
	(void) inCallBackTypes;
	(void) inContext;
	DEBUG_USE_ONLY( err );
	
	dlog( kLogLevelMax, "%s: %p\n", __ROUTINE__, inContext );
	
	nativeFD = CFFileDescriptorGetNativeDescriptor( inDesc );
	n = read( nativeFD, buf, sizeof( buf ) );
	err = map_global_value_errno( n > 0, n );
	check_noerr( err );
	
	if( n > 0 )
	{
		gCFLiteRunLoopTestReadBytes += n;
		if( gCFLiteRunLoopTestReadBytes == 8 )
		{
			CFRunLoopStop( CFRunLoopGetCurrent() );
		}
	}
}
#endif

//===========================================================================================================================
//	CFiteRunLoopTest
//===========================================================================================================================

OSStatus	CFLiteRunLoopTest( void )
{
	OSStatus					err;
	CFRunLoopRef				rl;
	CFAbsoluteTime				t;
	CFRunLoopTimerRef			timer;
	CFRunLoopTimerContext		timerContext = { 0, NULL, NULL, NULL, NULL };
	CFRunLoopSourceRef			source;
	CFRunLoopSourceContext		sourceContext;
	SocketRef					sock;
	CFSocketRef					sockCF;
	CFSocketContext				sockCFContext;
	struct sockaddr_in			sa4;
	CFDataRef					addrCF;
	
	// Timers and Sources
	
	t = CFAbsoluteTimeGetCurrent();
	sleep( 1 );
	t = CFAbsoluteTimeGetCurrent() - t;
	require_action( ( t >= 0.8 ) && ( t <= 1.2 ), exit, err = kRangeErr );
	
	rl = CFRunLoopGetCurrent();
	
	dlog( kLogLevelMax, "%N: %s: Starting timer for 10 seconds\n", __ROUTINE__ );
	
	gCFLiteRunLoopValue = 0;
	timerContext.info = (void *) 0x12345678;
	timer = CFRunLoopTimerCreate( NULL, CFAbsoluteTimeGetCurrent() + 10.0, 1.0, 0, 0, CFLiteRunLoopTestTimer, &timerContext );
	require_action( timer, exit, err = kResponseErr );
	
	CFRunLoopAddTimer( rl, timer, kCFRunLoopCommonModes );
	
	sleep( 5 );
	dlog( kLogLevelMax, "%N: %s: Setting  timer for 10 seconds\n", __ROUTINE__ );
	CFRunLoopTimerSetNextFireDate( timer, CFAbsoluteTimeGetCurrent() + 10.0 );
	
	CFRelease( timer );
	
	memset( &sourceContext, 0, sizeof( sourceContext ) );
	sourceContext.info		= (void *) 0x12345679;
	sourceContext.perform	= CFLiteRunLoopTestSourcePerform;
	source = CFRunLoopSourceCreate( kCFAllocatorDefault, 0, &sourceContext );
	require_action( source, exit, err = kNoMemoryErr );
	gCFLiteRunLoopTestSource = source;
	
	CFRunLoopAddSource( rl, source, kCFRunLoopCommonModes );
	CFRelease( source );
	CFRunLoopSourceSignal( source );
	
	t = CFAbsoluteTimeGetCurrent();
	CFRunLoopRun();
	t = CFAbsoluteTimeGetCurrent() - t;
	require_action( gCFLiteRunLoopValue == 10, exit, err = kResponseErr );
	dlog( kLogLevelMax, "%N: %s: runloop stopped after (%f secs)\n", __ROUTINE__, t );
	require_action( ( t >= 27 ) && ( t <= 29 ), exit, err = kRangeErr );
	
	// Sockets: successful connect.
	
	dlog( kLogLevelNotice, "test successful connect\n" );
	
	sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	err = map_socket_creation_errno( sock );
	require_noerr( err, exit );
	
	err = SocketMakeNonBlocking( sock );
	require_noerr( err, exit );
	
	sockCF = CFSocketCreateWithNative( kCFAllocatorDefault, sock, kCFSocketConnectCallBack, 
		CFLiteRunLoopSocketConnectCallBack, &sockCFContext );
	require_action( sockCF, exit, err = kNoMemoryErr );
	
	source = CFSocketCreateRunLoopSource( kCFAllocatorDefault, sockCF, 0 );
	require_action( source, exit, err = kNoMemoryErr );
	
	CFRunLoopAddSource( rl, source, kCFRunLoopCommonModes );
	CFRelease( source );
	
	err = StringToSockAddr( "17.251.200.32:80", &sa4, sizeof( sa4 ), NULL ); // www.apple.com
	require_noerr( err, exit );
	addrCF = CFDataCreate( NULL, (const uint8_t *) &sa4, (CFIndex) sizeof( sa4 ) );
	require_action( addrCF, exit, err = kNoMemoryErr );
	
	gCFLiteRunLoopSocketError = kNotInitializedErr;
	err = CFSocketConnectToAddress( sockCF, addrCF, -1 );
	CFRelease( addrCF );
	require_noerr( err, exit );
	
	CFRunLoopRun();
	err = gCFLiteRunLoopSocketError;
	require_noerr( err, exit );
	
	CFRelease( sockCF );
	
	dlog( kLogLevelNotice, "test successful connect: DONE\n" );
	
	// Sockets: failed connect.
	
	dlog( kLogLevelNotice, "test failed connect\n" );
	
	sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	err = map_socket_creation_errno( sock );
	require_noerr( err, exit );
	
	err = SocketMakeNonBlocking( sock );
	require_noerr( err, exit );
	
	sockCF = CFSocketCreateWithNative( kCFAllocatorDefault, sock, kCFSocketConnectCallBack, 
		CFLiteRunLoopSocketConnectCallBack, &sockCFContext );
	require_action( sockCF, exit, err = kNoMemoryErr );

	source = CFSocketCreateRunLoopSource( kCFAllocatorDefault, sockCF, 0 );
	require_action( source, exit, err = kNoMemoryErr );
	
	CFRunLoopAddSource( rl, source, kCFRunLoopCommonModes );
	CFRelease( source );
	
	err = StringToSockAddr( "10.0.1.15:81", &sa4, sizeof( sa4 ), NULL ); // bj.apple.com
	require_noerr( err, exit );
	addrCF = CFDataCreate( NULL, (const uint8_t *) &sa4, (CFIndex) sizeof( sa4 ) );
	require_action( addrCF, exit, err = kNoMemoryErr );
	
	gCFLiteRunLoopSocketError = kNotInitializedErr;
	err = CFSocketConnectToAddress( sockCF, addrCF, -1 );
	CFRelease( addrCF );
	if( !err )
	{
		CFRunLoopRun();
		err = gCFLiteRunLoopSocketError;
		require_action( err != kNoErr, exit, err = kResponseErr );
	}
	CFRelease( sockCF );
	
	dlog( kLogLevelNotice, "test failed connect: DONE\n" );
	
	// FileDescriptors: pipes
	
#if( TARGET_OS_POSIX )
{
	int							pipeFDs[ 2 ];
	int							readPipe;
	int							writePipe;
	CFFileDescriptorContext		fileDescContext;
	CFFileDescriptorRef			fileDesc;
	ssize_t						n;
	
	err = pipe( pipeFDs );
	err = map_noerr_errno( err );
	require_noerr( err, exit );
	readPipe  = pipeFDs[ 0 ];
	writePipe = pipeFDs[ 1 ];
	
	memset( &fileDescContext, 0, sizeof( fileDescContext ) );
	fileDesc = CFFileDescriptorCreate( kCFAllocatorDefault, readPipe, false, CFLiteRunLoopFileDescriptorCallBack, 
		&fileDescContext );
	require_action( fileDesc, exit, err = kNoMemoryErr );
	
	CFFileDescriptorEnableCallBacks( fileDesc, kCFFileDescriptorReadCallBack );
	
	source = CFFileDescriptorCreateRunLoopSource( kCFAllocatorDefault, fileDesc, 0 );
	require_action( source, exit, err = kNoMemoryErr );
	CFRunLoopAddSource( rl, source, kCFRunLoopCommonModes );
	CFRelease( source );
	
	n = write( writePipe, "12345678", 8 );
	err = map_global_value_errno( n == 8, n );
	require_noerr( err, exit );
	
	CFRunLoopRun();
	
	close( writePipe );
	CFRelease( fileDesc );
}
#endif
	
	CFRunLoopFinalize();
	err = kNoErr;
	
exit:
	printf( "CFLiteRunLoopTest: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}
#endif // !EXCLUDE_UNIT_TESTS

#endif // CFLITE_ENABLED
