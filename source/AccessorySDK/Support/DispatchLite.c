/*
	File:    	DispatchLite.c
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
	
	Copyright (C) 2008-2015 Apple Inc. All Rights Reserved.
*/

#include "DispatchLite.h"	// Include early for DISPATCH_LITE_*, etc. definitions.

#include "CommonServices.h"	// Include early for TARGET_*, etc. definitions.
#include "DebugServices.h"	// Include early for DEBUG_*, etc. definitions.

#if( DISPATCH_LITE_ENABLED )

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if( DISPATCH_LITE_USE_KQUEUE )
	#include <sys/event.h>
#endif

#if( TARGET_OS_POSIX )
	#include <fcntl.h>
	#include <pthread.h>
	#include <sys/stat.h>
	#include <sys/time.h>
#endif

#include "AtomicUtils.h"
#include "StringUtils.h"
#include "ThreadUtils.h"
#include "TickUtils.h"

#if( DISPATCH_LITE_CF_ENABLED )
	#include CF_HEADER
#endif

#if 0
#pragma mark == Constants and Types ==
#endif
#include <glib.h>
//===========================================================================================================================
//	Constants and Types
//===========================================================================================================================

#if( !defined( TARGET_NO_PTHREAD_BROADCAST ) )
	#define TARGET_NO_PTHREAD_BROADCAST				0
#endif

#if( TARGET_OS_WINDOWS )
	#define kLibDispatchWindowEvent_Socket			( WM_USER + 0x100 )
	#define kLibDispatchWindowEvent_WorkPending		( WM_USER + 0x101 )
#endif

// dispatch_base

struct dispatch_base_s
{
	uint32_t				magic;
	int32_t					refCount;
	void *					context;
	dispatch_function_t		doFree;
	dispatch_function_t		doFinalize;
};

// dispatch_item

typedef struct dispatch_item_s *	dispatch_item_t;
struct dispatch_item_s
{
	dispatch_item_t			next;
	dispatch_function_t		function;
	void *					context;
};

// dispatch_simple_item

typedef struct
{
	dispatch_function_t		function;
	void *					context;
	
}	dispatch_simple_item;

// dispatch_queue

#define kDispatchQueue_MagicGood		0x64717565  // 'dque'
#define kDispatchQueue_MagicBad			0x44515545  // 'DQUE'
#define	DispatchQueueValid( OBJ )	\
	( ( OBJ ) && ( ( OBJ )->base.magic == kDispatchQueue_MagicGood ) && ( ( OBJ )->base.refCount > 0 ) )

struct dispatch_queue_s
{
	struct dispatch_base_s		base;
	pthread_mutex_t				lock;
	pthread_mutex_t *			lockPtr;
	dispatch_item_t				itemsHead;
	dispatch_item_t *			itemsNext;	// Address of last element's next field. Never NULL.
	Boolean						busy;		// true if currently draining the queue.
	Boolean						pending;	// true if a drain request is pending.
	Boolean						concurrent;
	int32_t						suspendCount;
	char						label[ 64 ];
	int							priority;
};

// dispatch_source

#define kDispatchSource_MagicGood				0x64737263  // 'dsrc'
#define kDispatchSource_MagicBad				0x44535243  // 'DSRC'
#define	DispatchSourceValidOrFreeing( OBJ )		( ( OBJ ) && ( ( OBJ )->base.magic == kDispatchSource_MagicGood ) )

#define	DispatchSourceValid( OBJ )		\
	( ( OBJ ) && ( ( OBJ )->base.magic == kDispatchSource_MagicGood ) && ( ( OBJ )->base.refCount > 0 ) )

struct dispatch_source_s
{
	struct dispatch_base_s		base;
	int32_t						suspendCount;
	dispatch_queue_t			queue;
	Boolean						canceled;
	Boolean						started;			// Registered with kqueue at least once before.
	int32_t						pending;			// 1 if a callback is currently pending.
	dispatch_function_t			handlerFunction;
	dispatch_function_t			cancelFunction;
	dispatch_source_type_t		type;
#if( DISPATCH_LITE_USE_SELECT )
	dispatch_source_t			armedNext;
#endif
	union
	{
		// Read/Write
		
		struct
		{
			SocketRef			fd;
			#if( TARGET_OS_WINDOWS )
				Boolean			fdTracked;
			#endif
			size_t				avail;
			
		}	rw;
		
		// Signals
		
		struct
		{
			unsigned long		sig;
			
		}	sig;
		
		// Timers
		
		struct
		{
			dispatch_time_t		start;
			uint64_t			intervalMs;
			unsigned long		count;
			#if( DISPATCH_LITE_USE_SELECT )
				uint64_t		expireTicks;
			#elif( TARGET_OS_WINDOWS )
				HANDLE			timer;
			#endif
			
		}	timer;
		
	}	u;
};

// dispatch_group

#define kDispatchGroup_MagicGood		0x64677270  // 'dgrp'
#define kDispatchGroup_MagicBad			0x44475250  // 'DGRP'
#define	DispatchGroupValid( OBJ )		\
	( ( OBJ ) && ( ( OBJ )->base.magic == kDispatchGroup_MagicGood ) && ( ( OBJ )->base.refCount > 0 ) )

struct dispatch_group_s
{
	struct dispatch_base_s		base;
	int32_t						outstanding;
	dispatch_semaphore_t		sem;
	dispatch_queue_t			notifyQueue;
	void *						notifyContext;
	dispatch_function_t			notifyFunction;
};

// dispatch_select_packet

#if( DISPATCH_LITE_USE_SELECT )

#define kDispatchCommandArmSource			1
#define kDispatchCommandDisarmSource		2
#define kDispatchCommandFreeSource			3
#define kDispatchCommandQuit				4

typedef struct
{
	uint8_t					cmd;
	dispatch_source_t		source;
	
}	dispatch_select_packet;

#endif

// dispatch_semaphore

struct dispatch_semaphore_s
{
	struct dispatch_base_s		base;
	pthread_mutex_t				mutex;
	pthread_mutex_t *			mutexPtr;
	pthread_cond_t				condition;
	pthread_cond_t *			conditionPtr;
	long						value;
};

// DispatchSocketHashEntry

#if( TARGET_OS_WINDOWS )
typedef struct DispatchSocketHashEntry *		DispatchSocketHashRef;
struct DispatchSocketHashEntry
{
	DispatchSocketHashRef		next;
	SOCKET						sock;
	dispatch_source_t			source;
};
#endif

// dispatch_union

typedef union
{
	struct dispatch_base_s			base;
	struct dispatch_queue_s			queue;
	struct dispatch_source_s		source;
	struct dispatch_group_s			group;
	struct dispatch_semaphore_s		sem;
	
}	dispatch_union;

#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

DEBUG_STATIC OSStatus		__LibDispatch_ScheduleWork( dispatch_function_t inFunction, void *inContext );
#if( !TARGET_OS_WINDOWS )
	DEBUG_STATIC void *		__LibDispatch_WorkThread( void *inArg );
#endif
DEBUG_STATIC void			__dispatch_empty_callback( void *inContext );

// Objects

DEBUG_STATIC void	__dispatch_free_object( dispatch_object_t inObj );

// Queues

DEBUG_STATIC dispatch_queue_t	__dispatch_queue_create_internal( const char *inLabel );
DEBUG_STATIC void				__dispatch_queue_free( void *inArg );
DEBUG_STATIC void				__dispatch_queue_suspend( dispatch_queue_t inQueue );
DEBUG_STATIC void				__dispatch_queue_resume( dispatch_queue_t inQueue );
DEBUG_STATIC void				__dispatch_queue_concurrent_drain( void *inContext );
DEBUG_STATIC void				__dispatch_queue_serial_drain( void *inContext );
DEBUG_STATIC void				__dispatch_queue_serial_drain_locked( dispatch_queue_t inQueue );

// Sources

DEBUG_STATIC void	__dispatch_source_suspend( dispatch_source_t inSource );
DEBUG_STATIC void	__dispatch_source_resume( dispatch_source_t inSource );
DEBUG_STATIC void	__dispatch_source_free( void *inContext );

// Time

DEBUG_STATIC uint64_t	__dispatch_milliseconds( void );
DEBUG_STATIC uint64_t	__dispatch_wall_milliseconds( void );

// CF Support

#if( DISPATCH_LITE_CF_ENABLED )
	DEBUG_STATIC OSStatus		__LibDispatchCF_EnsureInitialized( void );
	DEBUG_STATIC void			__LibDispatchCF_Finalize( void );
	DEBUG_STATIC OSStatus		__LibDispatchCF_Schedule( void );
	DEBUG_STATIC void			__LibDispatchCF_SourcePerform( void *inContext );
#endif

// Platform Support

DEBUG_STATIC void	__LibDispatch_PlatformFreeSource( void *inArg );
DEBUG_STATIC void	__LibDispatch_PlatformArmSourceAndUnlock( dispatch_source_t inSource );
DEBUG_STATIC void	__LibDispatch_PlatformDisarmSourceAndUnlock( dispatch_source_t inSource );

// KQueue Support

#if( DISPATCH_LITE_USE_KQUEUE )
	DEBUG_STATIC int		__LibDispatch_GetKQueue( void );
	DEBUG_STATIC OSStatus	__LibDispatch_KQueueDispatchAsync( dispatch_function_t inFunction, void *inContext );
	DEBUG_STATIC void		__LibDispatch_KQueueDrain( void *inContext );
	DEBUG_STATIC void		__LibDispatch_KQueueFDEvent( void *inContext );
	DEBUG_STATIC void		__LibDispatch_KQueueSignalEvent( void *inContext );
	DEBUG_STATIC void		__LibDispatch_KQueueTimerEvent( void *inContext );
#endif

// Select Support

#if( DISPATCH_LITE_USE_SELECT )
	DEBUG_STATIC OSStatus	__LibDispatch_SelectEnsureInitialized( void );
	DEBUG_STATIC void		__LibDispatch_SelectDrain( void *inArg );
	DEBUG_STATIC OSStatus	__LibDispatch_SelectHandleCommand( const dispatch_select_packet *inPkt );
	DEBUG_STATIC void		__LibDispatch_SelectHandleReadWriteEvent( void *inContext );
	DEBUG_STATIC void		__LibDispatch_SelectHandleTimerEvent( void *inContext );
	DEBUG_STATIC void		__LibDispatch_PlatformArmOrDisarmSourceAndUnlock( dispatch_source_t inSource, uint8_t inCmd );
#endif

// Windows Support

#if( TARGET_OS_WINDOWS )
	DEBUG_STATIC LRESULT CALLBACK	__LibDispatch_MainQueueWinProc( HWND inWindow, UINT inMsg, WPARAM inWParam, LPARAM inLParam );
	DEBUG_STATIC OSStatus			__LibDispatch_IOThreadStart( void );
	DEBUG_STATIC void				__LibDispatch_IOThreadStop( void );
	DEBUG_STATIC void *				__LibDispatch_IOThread( void *inArg );
	DEBUG_STATIC LRESULT CALLBACK	__LibDispatch_IOThreadWinProc( HWND inWindow, UINT inMsg, WPARAM inWParam, LPARAM inLParam );
	#define							__LibDispatch_AddSocket( SOCK, SOURCE )		__LibDispatch_LookupSocket( ( SOCK ), ( SOURCE ), NULL )
	#define							__LibDispatch_RemoveSocket( SOCK )			__LibDispatch_LookupSocket( ( SOCK ), NULL, NULL )
	DEBUG_STATIC OSStatus			__LibDispatch_LookupSocket( SOCKET inSock, dispatch_source_t inSource, dispatch_source_t *outSource );
	
	DEBUG_STATIC void				__LibDispatch_WindowsSocketEvent( void *inContext );
	DEBUG_STATIC void CALLBACK		__LibDispatch_WindowsTimerCallBack( void *inArg, BOOLEAN inFired );
	DEBUG_STATIC void				__LibDispatch_WindowsTimerEvent( void *inContext );
#endif

#if 0
#pragma mark == Globals ==
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static Boolean						gDispatchInitialized				= false;
static pthread_mutex_t				gDispatchInitializeMutex			= PTHREAD_MUTEX_INITIALIZER;

// Thread Pool

#if( !TARGET_OS_WINDOWS )
	static pthread_mutex_t			gDispatchThreadPool_Mutex;
	static pthread_mutex_t *		gDispatchThreadPool_MutexPtr		= NULL;
	static pthread_cond_t			gDispatchThreadPool_Condition;
	static pthread_cond_t *			gDispatchThreadPool_ConditionPtr	= NULL;
	static pthread_attr_t			gDispatchThreadPool_Attr;
	static pthread_attr_t *			gDispatchThreadPool_AttrPtr			= NULL;
	static int						gDispatchThreadPool_CurrentThreads	= 0;
	static int						gDispatchThreadPool_IdleThreads		= 0;
	static int						gDispatchThreadPool_MaxThreads		= 128;
	static dispatch_item_t			gDispatchThreadPool_ItemsHead		= NULL;
	static dispatch_item_t *		gDispatchThreadPool_ItemsNext		= NULL;
	static int						gDispatchThreadPool_PendingItems	= 0;
	static Boolean					gDispatchThreadPool_Quit			= false;
#endif

// Globals Queues

static pthread_key_t				gDispatchKey_CurrentQueue;
static pthread_key_t *				gDispatchKey_CurrentQueuePtr		= NULL;
static dispatch_queue_t				gDispatchConcurrentQueues[ 3 ]		= { NULL, NULL, NULL };

static dispatch_queue_t				gDispatchMainQueue					= NULL;
dispatch_function_t					gDispatchMainQueueScheduleHookFunc	= NULL;

#if( !TARGET_OS_WINDOWS )
	static pthread_cond_t			gDispatchMainQueue_Condition;
	static pthread_cond_t *			gDispatchMainQueue_ConditionPtr		= NULL;
	static Boolean					gDispatchMainQueue_WorkPending		= false;
#endif

// CF Support

#if( DISPATCH_LITE_CF_ENABLED )
	static Boolean					gDispatchCFInitialized				= false;
	static CFRunLoopRef				gDispatchCF_MainRL					= NULL;
	static CFRunLoopSourceRef		gDispatchCF_Source					= NULL;
#endif

// KQueue Support

#if( DISPATCH_LITE_USE_KQUEUE )
	static pthread_mutex_t			gDispatchKQueue_Mutex;
	static pthread_mutex_t *		gDispatchKQueue_MutexPtr			= NULL;
	static int32_t					gDispatchKQueue_FD					= -1;
	static int32_t					gDispatchKQueue_PipeSend			= -1;
	static int32_t					gDispatchKQueue_PipeRecv			= -1;
	static dispatch_queue_t			gDispatchKQueue_DrainQueue			= NULL;
#endif

// Select Support

#if( DISPATCH_LITE_USE_SELECT )
	static dispatch_queue_t			gDispatchSelect_CommandQueue		= NULL;
	static SocketRef				gDispatchSelect_CommandSock			= kInvalidSocketRef;
	static dispatch_source_t		gDispatchSelect_ReadWriteList		= NULL;
	static fd_set					gDispatchSelect_ReadSet;
	static fd_set					gDispatchSelect_WriteSet;
	static dispatch_source_t		gDispatchSelect_TimerList			= NULL;
	static dispatch_semaphore_t		gDispatchSelect_QuitSem				= NULL;
#endif

// Windows Support

#if( TARGET_OS_WINDOWS )
	static LONG						gDispatchConcurrentCount			= 0;
	static LONG						gDispatchSerialCount				= 0;
	
	static HWND						gDispatchMainQueueWindow			= NULL;
	
	static pthread_t				gDispatchIOThread;
	static pthread_t *				gDispatchIOThreadPtr				= NULL;
	static HWND						gDispatchIOThreadWindow				= NULL;
	static DispatchSocketHashRef	gDispatchSocketHashTable[ 61 ];
	static pthread_mutex_t			gDispatchSocketHashTableMutex;
	static pthread_mutex_t *		gDispatchSocketHashTableMutexPtr	= NULL;
	
	static Boolean					gDispatchDidWSAStartup				= false;
#endif

dlog_define( gcd, kLogLevelNotice, kLogFlags_Default, "gcd", NULL );
#if( TARGET_HAS_C99_VA_ARGS )
	#define gcd_ulog( LEVEL, ... )		dlogc( &log_category_from_name( gcd ), (LEVEL), __VA_ARGS__ )
#else
	#define gcd_ulog( LEVEL, ARGS... )	dlogc( &log_category_from_name( gcd ), (LEVEL), ## ARGS )
#endif

#if 0
#pragma mark == General ==
#endif

//===========================================================================================================================
//	LibDispatch_EnsureInitialized
//
//	Note: Must be called from the main thread.
//===========================================================================================================================

OSStatus	LibDispatch_EnsureInitialized( void )
{
	OSStatus		err;
	
	pthread_mutex_lock( &gDispatchInitializeMutex );
	require_action_quiet( !gDispatchInitialized, exit, err = kNoErr );
	
	// Set up thread pool.

#if( !TARGET_OS_WINDOWS )
	gDispatchThreadPool_Quit = false;
	
	err = pthread_mutex_init( &gDispatchThreadPool_Mutex, NULL );
	require_noerr( err, exit );
	gDispatchThreadPool_MutexPtr = &gDispatchThreadPool_Mutex;
	
	err = pthread_cond_init( &gDispatchThreadPool_Condition, NULL );
	require_noerr( err, exit );
	gDispatchThreadPool_ConditionPtr = &gDispatchThreadPool_Condition;
	
	err = pthread_attr_init( &gDispatchThreadPool_Attr );
	require_noerr( err, exit );
	gDispatchThreadPool_AttrPtr = &gDispatchThreadPool_Attr;
	
	err = pthread_attr_setdetachstate( gDispatchThreadPool_AttrPtr, PTHREAD_CREATE_DETACHED );
	require_noerr( err, exit );
	
	gDispatchThreadPool_ItemsHead = NULL;
	gDispatchThreadPool_ItemsNext = &gDispatchThreadPool_ItemsHead;
#endif
	
	// Set up global queues.
	
	err = pthread_key_create( &gDispatchKey_CurrentQueue, NULL ); 
	require_noerr( err, exit );
	gDispatchKey_CurrentQueuePtr = &gDispatchKey_CurrentQueue;
	
	gDispatchConcurrentQueues[ 0 ] = __dispatch_queue_create_internal( "com.apple.low-priority-root-queue" );
	require_action( gDispatchConcurrentQueues[ 0 ], exit, err = ENOMEM );
	gDispatchConcurrentQueues[ 0 ]->concurrent = true;
	
	gDispatchConcurrentQueues[ 1 ] = __dispatch_queue_create_internal( "com.apple.root-queue" );
	require_action( gDispatchConcurrentQueues[ 1 ], exit, err = ENOMEM );
	gDispatchConcurrentQueues[ 1 ]->concurrent = true;
	
	gDispatchConcurrentQueues[ 2 ] = __dispatch_queue_create_internal( "com.apple.high-priority-root-queue" );
	require_action( gDispatchConcurrentQueues[ 2 ], exit, err = ENOMEM );
	gDispatchConcurrentQueues[ 2 ]->concurrent = true;
	
	// Set up the main queue.
	
#if( !TARGET_OS_WINDOWS )
	gDispatchMainQueue_WorkPending = false;
#endif
	
	gDispatchMainQueue = __dispatch_queue_create_internal( "com.apple.main-queue" );
	require_action( gDispatchMainQueue, exit, err = ENOMEM );
	
#if( !TARGET_OS_WINDOWS )
	err = pthread_cond_init( &gDispatchMainQueue_Condition, NULL );
	require_noerr( err, exit );
	gDispatchMainQueue_ConditionPtr = &gDispatchMainQueue_Condition;
#endif
	
	pthread_setspecific( gDispatchKey_CurrentQueue, gDispatchMainQueue );
	
	// Set up kqueue support.
	
#if( DISPATCH_LITE_USE_KQUEUE )
	err = pthread_mutex_init( &gDispatchKQueue_Mutex, NULL );
	require_noerr( err, exit );
	gDispatchKQueue_MutexPtr = &gDispatchKQueue_Mutex;
	
	gDispatchKQueue_DrainQueue = __dispatch_queue_create_internal( "com.apple.kqueue-drain" );
	require_action( gDispatchKQueue_DrainQueue, exit, err = ENOMEM );
#endif
	
	// Set up CF support.
	
#if( DISPATCH_LITE_CF_ENABLED )
	err = __LibDispatchCF_EnsureInitialized();
	require_noerr( err, exit );
#endif
	
	// Set up select() support.
	
#if( DISPATCH_LITE_USE_SELECT )
	err = __LibDispatch_SelectEnsureInitialized();
	require_noerr( err, exit );
#endif
	
	// Set up Windows support.
	
#if( TARGET_OS_WINDOWS )
{
	HINSTANCE		instance;
	WNDCLASSEX		wcex;
	HWND			wind;
	WSADATA			wsaData;
	
	gDispatchConcurrentCount = 0;
	gDispatchSerialCount     = 0;
	
	instance = GetModuleHandle( NULL );
	err = map_global_value_errno( instance, instance );
	require_noerr( err, exit );
	
	wcex.cbSize			= sizeof( wcex );
	wcex.style			= 0;
	wcex.lpfnWndProc	= __LibDispatch_MainQueueWinProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= instance;
	wcex.hIcon			= NULL;
	wcex.hCursor		= NULL;
	wcex.hbrBackground	= NULL;
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= TEXT( "libdispatch_main" );
	wcex.hIconSm		= NULL;
	RegisterClassEx( &wcex );
	
	wind = CreateWindow( wcex.lpszClassName, wcex.lpszClassName, 0, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, 
		instance, NULL );
	err = map_global_value_errno( wind, wind );
	require_noerr( err, exit );
	gDispatchMainQueueWindow = wind;
	
	// Set up handling of IO.
	
	err = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
	check_noerr( err );
	gDispatchDidWSAStartup = true;
	
	err = pthread_mutex_init( &gDispatchSocketHashTableMutex, NULL );
	require_noerr( err, exit );
	gDispatchSocketHashTableMutexPtr = &gDispatchSocketHashTableMutex;
	
	err = __LibDispatch_IOThreadStart();
	require_noerr( err, exit );
}
#endif // TARGET_OS_WINDOWS
	
	gDispatchInitialized = true;
	
exit:
	pthread_mutex_unlock( &gDispatchInitializeMutex );
	if( err ) LibDispatch_Finalize();
	return( err );
}

//===========================================================================================================================
//	LibDispatch_Finalize
//===========================================================================================================================

void	LibDispatch_Finalize( void )
{
	OSStatus		err;
	size_t			i;
	
	DEBUG_USE_ONLY( err );
	
	// Tear down CF support.
	
#if( DISPATCH_LITE_CF_ENABLED )
	__LibDispatchCF_Finalize();
#endif
	
	// Tear down kqueue support.
	
#if( DISPATCH_LITE_USE_KQUEUE )
	if( gDispatchKQueue_DrainQueue )
	{
		// Tells the kqueue thread to quit then force a synchronous empty callback to wait for it to finish.
		
		if( gDispatchKQueue_PipeSend >= 0 )
		{
			__LibDispatch_KQueueDispatchAsync( NULL, NULL );
		}
		dispatch_sync_f( gDispatchKQueue_DrainQueue, NULL, __dispatch_empty_callback );
		dispatch_release( gDispatchKQueue_DrainQueue );
		gDispatchKQueue_DrainQueue = NULL;
	}
	ForgetFD( &gDispatchKQueue_FD );
	ForgetFD( &gDispatchKQueue_PipeSend );
	ForgetFD( &gDispatchKQueue_PipeRecv );
	pthread_mutex_forget( &gDispatchKQueue_MutexPtr );
#endif
	
	// Tear down select() support.
	
#if( DISPATCH_LITE_USE_SELECT )
	if( IsValidSocket( gDispatchSelect_CommandSock ) )
	{
		dispatch_select_packet		pkt;
		ssize_t						n;
		
		check( gDispatchSelect_QuitSem == NULL );
		gDispatchSelect_QuitSem = dispatch_semaphore_create( 0 );
		check( gDispatchSelect_QuitSem );
		
		pkt.cmd		= kDispatchCommandQuit;
		pkt.source	= NULL;
		n = send( gDispatchSelect_CommandSock, (char *) &pkt, sizeof( pkt ), 0 );
		err = map_socket_value_errno( gDispatchSelect_CommandSock, n == (ssize_t) sizeof( pkt ), n );
		check_noerr( err );
		
		if( gDispatchSelect_QuitSem )
		{
			dispatch_semaphore_wait( gDispatchSelect_QuitSem, DISPATCH_TIME_FOREVER );
			dispatch_release( gDispatchSelect_QuitSem );
			gDispatchSelect_QuitSem = NULL;
		}
	}
	dispatch_forget( &gDispatchSelect_CommandQueue );
	ForgetSocket( &gDispatchSelect_CommandSock );
	check( gDispatchSelect_ReadWriteList == NULL );
	check( gDispatchSelect_TimerList == NULL );
	check( gDispatchSelect_QuitSem == NULL );
#endif
	
	// Tear down Windows support.
	
#if( TARGET_OS_WINDOWS )
	__LibDispatch_IOThreadStop();
	pthread_mutex_forget( &gDispatchSocketHashTableMutexPtr );
	if( gDispatchMainQueueWindow )
	{
		BOOL		good;
		
		good = DestroyWindow( gDispatchMainQueueWindow );
		err = map_global_value_errno( good, good );
		check_noerr( err );
		gDispatchMainQueueWindow = NULL;
	}
	if( gDispatchDidWSAStartup )
	{
		gDispatchDidWSAStartup = false;
		WSACleanup();
	}
#endif
	
	// Tear down the main queue.
	
#if( TARGET_OS_WINDOWS )
	while( InterlockedCompareExchange( &gDispatchSerialCount, 0, 0 ) != 0 )
	{
		// Cheesy to use a sleep loop like this, but the only alternative I could think of is to use a pthread
		// mutex and conditional variable to signal each time the thread is done because we're using the Windows
		// thread pool API and don't have direct access to the thread. The only time this sleep loop is used is
		// when finalizing the entire libdispatch library and since that usually only happens once at app quit, 
		// if it even happens at all, I opt'd for the lower overhead, but somewhat cheesy sleep loop.
		
		Sleep( 1 );
	}
#endif
	dispatch_forget( &gDispatchMainQueue );
#if( !TARGET_OS_WINDOWS )
	pthread_cond_forget( &gDispatchMainQueue_ConditionPtr );
#endif
	
	// Tear down global queues.
	
#if( TARGET_OS_WINDOWS )
	while( InterlockedCompareExchange( &gDispatchConcurrentCount, 0, 0 ) != 0 )
	{
		// Cheesy to use a sleep loop like this, but the only alternative I could think of is to use a pthread
		// mutex and conditional variable to signal each time the thread is done because we're using the Windows
		// thread pool API and don't have direct access to the thread. The only time this sleep loop is used is
		// when finalizing the entire libdispatch library and since that usually only happens once at app quit, 
		// if it even happens at all, I opt'd for the lower overhead, but somewhat cheesy sleep loop.
		
		Sleep( 1 );
	}
#else
	for( i = 0; i < countof( gDispatchConcurrentQueues ); ++i )
	{
		if( gDispatchConcurrentQueues[ i ] )
		{
			dispatch_sync_f( gDispatchConcurrentQueues[ i ], NULL, __dispatch_empty_callback );
		}
	}
#endif
	for( i = 0; i < countof( gDispatchConcurrentQueues ); ++i )
	{
		dispatch_forget( &gDispatchConcurrentQueues[ i ] );
	}
	if( gDispatchKey_CurrentQueuePtr )
	{
		err = pthread_key_delete( gDispatchKey_CurrentQueue );
		check_noerr( err );
		gDispatchKey_CurrentQueuePtr = NULL;
	}
	
	// Tear down thread pool.
	
#if( !TARGET_OS_WINDOWS )
	if( gDispatchThreadPool_MutexPtr )
	{
		pthread_mutex_lock( gDispatchThreadPool_MutexPtr );
		gDispatchThreadPool_Quit = true;
		#if( TARGET_NO_PTHREAD_BROADCAST )
			while( gDispatchThreadPool_CurrentThreads > 0 )
			{
				err = pthread_cond_signal( gDispatchThreadPool_ConditionPtr );
				check_noerr( err );
				
				pthread_mutex_unlock( gDispatchThreadPool_MutexPtr );
				usleep( 50000 ); // Cheesy, but probably not worth a quit condition.
				pthread_mutex_lock( gDispatchThreadPool_MutexPtr );
			}
		#else
			if( gDispatchThreadPool_CurrentThreads > 0 )
			{
				if( gDispatchThreadPool_IdleThreads > 0 )
				{
					err = pthread_cond_broadcast( gDispatchThreadPool_ConditionPtr );
					check_noerr( err );
				}
				while( gDispatchThreadPool_CurrentThreads > 0 )
				{
					err = pthread_cond_wait( gDispatchThreadPool_ConditionPtr, gDispatchThreadPool_MutexPtr );
					check_noerr( err );
				}
			}
		#endif
		pthread_mutex_unlock( gDispatchThreadPool_MutexPtr );
	}
	check( gDispatchThreadPool_ItemsHead == NULL );
	check( gDispatchThreadPool_PendingItems == 0 );
	
	if( gDispatchThreadPool_AttrPtr )
	{
		pthread_attr_destroy( gDispatchThreadPool_AttrPtr );
		gDispatchThreadPool_AttrPtr = NULL;
	}
	pthread_cond_forget( &gDispatchThreadPool_ConditionPtr );
	pthread_mutex_forget( &gDispatchThreadPool_MutexPtr );
#endif
	
	gDispatchInitialized = false;
}

//===========================================================================================================================
//	__LibDispatch_ScheduleWork
//===========================================================================================================================

#if( TARGET_OS_WINDOWS )
DEBUG_STATIC DWORD WINAPI	__LibDispatch_ScheduleWorkWrapper( void *inArg );

DEBUG_STATIC OSStatus	__LibDispatch_ScheduleWork( dispatch_function_t inFunction, void *inContext )
{
	OSStatus			err;
	dispatch_item_t		item;
	BOOL				good;
	
	item = (dispatch_item_t) malloc( sizeof( *item ) );
	require_action( item, exit, err = kNoMemoryErr );
	item->function = inFunction;
	item->context  = inContext;
	
	good = QueueUserWorkItem( __LibDispatch_ScheduleWorkWrapper, item, WT_EXECUTEDEFAULT );
	err = map_global_value_errno( good, good );
	require_noerr( err, exit );
	item = NULL;
	
exit:
	if( item ) free( item );
	return( err );
}

DEBUG_STATIC DWORD WINAPI	__LibDispatch_ScheduleWorkWrapper( void *inArg )
{
	dispatch_item_t const		item = (dispatch_item_t) inArg;
	
	item->function( item->context );
	free( item );
	return( 0 );
}
#else // !TARGET_OS_WINDOWS
DEBUG_STATIC OSStatus	__LibDispatch_ScheduleWork( dispatch_function_t inFunction, void *inContext )
{
	OSStatus			err;
	dispatch_item_t		item;
	pthread_t			tid;
	
	item = (dispatch_item_t) calloc( 1, sizeof( *item ) );
	require_action( item, exit2, err = kNoMemoryErr );
	item->function = inFunction;
	item->context  = inContext;
	
	pthread_mutex_lock( gDispatchThreadPool_MutexPtr );
	
	*gDispatchThreadPool_ItemsNext = item;
	 gDispatchThreadPool_ItemsNext = &item->next;
	++gDispatchThreadPool_PendingItems;
	
	// If there's already an idle thread, signal one that more work is available.
	
	if( gDispatchThreadPool_IdleThreads > 0 )
	{
		err = pthread_cond_signal( gDispatchThreadPool_ConditionPtr );
		check_noerr( err );
	}
	
	// If we have more work than threads and we haven't hit our max threads, create a new thread.
	
	if( gDispatchThreadPool_IdleThreads < gDispatchThreadPool_PendingItems )
	{
		if( gDispatchThreadPool_CurrentThreads < gDispatchThreadPool_MaxThreads )
		{
			err = pthread_create( &tid, gDispatchThreadPool_AttrPtr, __LibDispatch_WorkThread, NULL );
			require_noerr( err, exit );
			
			++gDispatchThreadPool_CurrentThreads;
			gcd_ulog( kLogLevelChatty, "+++ created work thread: %d now (%d max)\n", 
				gDispatchThreadPool_CurrentThreads, gDispatchThreadPool_MaxThreads );
		}
		else
		{
			gcd_ulog( kLogLevelNotice, "*** more work, but not allowed to create another thread (%d of %d)\n", 
				gDispatchThreadPool_CurrentThreads, gDispatchThreadPool_MaxThreads );
		}
	}
	err = kNoErr;
	
exit:
	pthread_mutex_unlock( gDispatchThreadPool_MutexPtr );
	
exit2:
	return( err );
}
#endif // TARGET_OS_WINDOWS

//===========================================================================================================================
//	__LibDispatch_WorkThread
//===========================================================================================================================

#if( !TARGET_OS_WINDOWS )
DEBUG_STATIC void *	__LibDispatch_WorkThread( void *inArg )
{
	OSStatus			err;
	Boolean				timedOut;
	dispatch_item_t		item;
	struct timespec		timeout;
	
	(void) inArg; // Unused
	
	pthread_setname_np_compat( "gcd-work" );
	pthread_mutex_lock( gDispatchThreadPool_MutexPtr );
	for( ;; )
	{
		// Wait for work or timeout if we're idle for too long.
		
		#if( TARGET_OS_THREADX )
			timeout.tv_sec  = 2;
			timeout.tv_nsec = 0;
			timedOut = false;
			while( ( gDispatchThreadPool_ItemsHead == NULL ) && !gDispatchThreadPool_Quit )
			{
				++gDispatchThreadPool_IdleThreads;
				err = pthread_cond_timedwait_relative_np( gDispatchThreadPool_ConditionPtr, gDispatchThreadPool_MutexPtr, 
					&timeout );
				--gDispatchThreadPool_IdleThreads;
				if( err == ETIMEDOUT )
				{
					timedOut = true;
					break;
				}
				check_noerr( err );
			}
		#else
			struct timeval		now;
			
			// Warning: there's a bug here in that pthread_cond_timedwait is broken without support for CLOCK_MONOTONIC,  
			// which is rarely supported. This needs to be revisited to come up with a better replacement for waiting.
			
			gettimeofday( &now, NULL );
			timeout.tv_sec  = now.tv_sec + 2;
			timeout.tv_nsec = now.tv_usec * 1000;
			timedOut = false;
			while( ( gDispatchThreadPool_ItemsHead == NULL ) && !gDispatchThreadPool_Quit )
			{
				++gDispatchThreadPool_IdleThreads;
				err = pthread_cond_timedwait( gDispatchThreadPool_ConditionPtr, gDispatchThreadPool_MutexPtr, &timeout );
				--gDispatchThreadPool_IdleThreads;
				if( err == ETIMEDOUT )
				{
					timedOut = true;
					break;
				}
				check_noerr( err );
			}
		#endif
		
		// Process the next item (if any).
		
		item = gDispatchThreadPool_ItemsHead;
		if( item )
		{
			timedOut = false;
			
			if( ( gDispatchThreadPool_ItemsHead = item->next ) == NULL )
			{
				gDispatchThreadPool_ItemsNext = &gDispatchThreadPool_ItemsHead;
			}
			--gDispatchThreadPool_PendingItems;
			
			pthread_mutex_unlock( gDispatchThreadPool_MutexPtr );
			item->function( item->context );
			free( item );
			pthread_mutex_lock( gDispatchThreadPool_MutexPtr );
		}
		
		// Exit if there's no more work and we're quitting.
		
		item = gDispatchThreadPool_ItemsHead;
		if( ( item == NULL ) && gDispatchThreadPool_Quit )
		{
			// If we're last thread to go away, tell the delete function.
			
			if( --gDispatchThreadPool_CurrentThreads == 0 )
			{
				#if( !TARGET_NO_PTHREAD_BROADCAST )
					err = pthread_cond_broadcast( gDispatchThreadPool_ConditionPtr );
					check_noerr( err );
				#endif
			}
			gcd_ulog( kLogLevelNotice, "--- quitting thread: %d now (%d max)\n", 
				gDispatchThreadPool_CurrentThreads, gDispatchThreadPool_MaxThreads );
			break;
		}
		
		// Exit if there's no more work and we timed out.
		
		if( ( item == NULL ) && timedOut )
		{
			--gDispatchThreadPool_CurrentThreads;
			gcd_ulog( kLogLevelChatty, "--- aging idle thread: %d now (%d max)\n", 
				gDispatchThreadPool_CurrentThreads, gDispatchThreadPool_MaxThreads );
			break;
		}
	}
	pthread_mutex_unlock( gDispatchThreadPool_MutexPtr );	
	return( NULL );
}
#endif // !TARGET_OS_WINDOWS

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	__dispatch_free_object
//===========================================================================================================================

DEBUG_STATIC void	__dispatch_free_object( dispatch_object_t inObj )
{
	dispatch_union * const			obj		= (dispatch_union *) inObj;
	dispatch_function_t const		func	= obj->base.doFinalize;
	void * const					context	= obj->base.context;
	
	free( inObj );
	if( func ) func( context );
}

//===========================================================================================================================
//	dispatch_retain
//===========================================================================================================================

void	dispatch_retain( dispatch_object_t inObj )
{
	dispatch_union * const		obj = (dispatch_union *) inObj;
	
	require( obj, exit );
	
	atomic_add_32( &obj->base.refCount, 1 );
	
exit:
	return;
}

//===========================================================================================================================
//	dispatch_release
//===========================================================================================================================

void	dispatch_release( dispatch_object_t inObj )
{
	dispatch_union * const		obj = (dispatch_union *) inObj;
	
	require( obj, exit );
	
	if( atomic_add_and_fetch_32( &obj->base.refCount, -1 ) == 0 )
	{
		check( obj->base.doFree );
		obj->base.doFree( inObj );
	}
	
exit:
	return;
}

//===========================================================================================================================
//	dispatch_get_context
//===========================================================================================================================

void *	dispatch_get_context( dispatch_object_t inObj )
{
	return( ( (const dispatch_union *) inObj )->base.context );
}

//===========================================================================================================================
//	dispatch_set_context
//===========================================================================================================================

void	dispatch_set_context( dispatch_object_t inObj, void *inContext )
{
	( (dispatch_union *) inObj )->base.context = inContext;
}

//===========================================================================================================================
//	dispatch_set_finalizer_f
//===========================================================================================================================

void	dispatch_set_finalizer_f( dispatch_object_t inObj, dispatch_function_t inFinalizer )
{
	( (dispatch_union *) inObj )->base.doFinalize = inFinalizer;
}

//===========================================================================================================================
//	dispatch_suspend
//===========================================================================================================================

void	dispatch_suspend( dispatch_object_t inObj )
{
	uint32_t		magic;
	
	require( inObj, exit );
	magic = *( (const uint32_t *) inObj );
	if(      magic == kDispatchQueue_MagicGood )	__dispatch_queue_suspend( (dispatch_queue_t) inObj );
	else if( magic == kDispatchSource_MagicGood )	__dispatch_source_suspend( (dispatch_source_t) inObj );
	else { dlogassert( "bad object '%C' %p", magic, inObj ); goto exit; }
	
exit:
	return;
}

//===========================================================================================================================
//	dispatch_resume
//===========================================================================================================================

void	dispatch_resume( dispatch_object_t inObj )
{
	uint32_t		magic;
	
	require( inObj, exit );
	magic = *( (const uint32_t *) inObj );
	if(      magic == kDispatchQueue_MagicGood )	__dispatch_queue_resume( (dispatch_queue_t) inObj );
	else if( magic == kDispatchSource_MagicGood )	__dispatch_source_resume( (dispatch_source_t) inObj );
	else { dlogassert( "bad object '%C' %p", magic, inObj ); goto exit; }
	
exit:
	return;
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	dispatch_after_f
//===========================================================================================================================

typedef struct
{
	dispatch_source_t		timer;
	dispatch_function_t		user_f;
	void *					user_ctx;
	
}	dispatch_after_context;

static void	dispatch_after_handler( void *inContext );

void	dispatch_after_f( dispatch_time_t inWhen, dispatch_queue_t inQueue, void *inContext, dispatch_function_t inFunction )
{
	dispatch_time_t					nowDT;
	uint64_t						nanos;
	dispatch_after_context *		ctx = NULL;
	
	if( inWhen == DISPATCH_TIME_FOREVER )
	{
		dlogassert( "### dispatch_after_f with DISPATCH_TIME_FOREVER will never run" );
		goto exit;
	}
	if( inWhen == 0 )
	{
		dispatch_async_f( inQueue, inContext, inFunction );
		goto exit;
	}
	
	// Calculate the nanoseconds until the deadline.
	
	if( ( (int64_t) inWhen ) < 0 ) // Negative if wall clock time.
	{
		inWhen	= (dispatch_time_t)( -( (int64_t) inWhen ) );
		nowDT	= (dispatch_time_t)( -( (int64_t) dispatch_walltime( NULL, 0 ) ) );
	}
	else
	{
		nowDT = dispatch_time( DISPATCH_TIME_NOW, 0 );
	}
	nanos = (uint64_t)( ( nowDT < inWhen ) ? ( kNanosecondsPerMillisecond * ( inWhen - nowDT ) ) : 0 );
	
	// Start a timer to call the function at the deadline.
	
	ctx = (dispatch_after_context *) calloc( 1, sizeof( *ctx ) );
	require( ctx, exit );
	ctx->user_f		= inFunction;
	ctx->user_ctx	= inContext;
	
	ctx->timer = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, inQueue );
	require( ctx->timer, exit );
	dispatch_set_context( ctx->timer, ctx );
	dispatch_source_set_event_handler_f( ctx->timer, dispatch_after_handler );
	dispatch_source_set_timer( ctx->timer, dispatch_time( DISPATCH_TIME_NOW, (int64_t) nanos ), DISPATCH_TIME_FOREVER, nanos / 10 );
	dispatch_resume( ctx->timer );
	ctx = NULL;
	
exit:
	FreeNullSafe( ctx );
}

//===========================================================================================================================
//	dispatch_after_handler
//===========================================================================================================================

static void	dispatch_after_handler( void *inContext )
{
	dispatch_after_context * const		ctx = (dispatch_after_context *) inContext;
	
	dispatch_source_forget( &ctx->timer );
	ctx->user_f( ctx->user_ctx );
	free( ctx );
}

//===========================================================================================================================
//	dispatch_async
//===========================================================================================================================

void	dispatch_async_f( dispatch_queue_t inQueue, void *inContext, dispatch_function_t inFunction )
{
	OSStatus			err;
	dispatch_item_t		item;
	Boolean				wasEmpty;
	
	DEBUG_USE_ONLY( err );
	
	require_action( DispatchQueueValid( inQueue ), exit, err = EINVAL );
	
	item = (dispatch_item_t) calloc( 1, sizeof( *item ) );
	require_action( item, exit, err = ENOMEM );
	
	item->function	= inFunction;
	item->context	= inContext;
	
	pthread_mutex_lock( inQueue->lockPtr );
	
	wasEmpty = (Boolean)( inQueue->itemsHead == NULL );
	if( wasEmpty ) dispatch_retain( inQueue );
	
	*inQueue->itemsNext = item;
	 inQueue->itemsNext = &item->next;
	
	if( inQueue == gDispatchMainQueue )
	{
		#if( TARGET_OS_POSIX || TARGET_OS_THREADX )
			gDispatchMainQueue_WorkPending = true;
			pthread_cond_signal( gDispatchMainQueue_ConditionPtr );
			
			#if( DISPATCH_LITE_CF_ENABLED )
				__LibDispatchCF_Schedule();
			#endif
		#elif( TARGET_OS_WINDOWS )
			if( !inQueue->pending )
			{
				BOOL		good;
				
				inQueue->pending = true;
				good = PostMessage( gDispatchMainQueueWindow, kLibDispatchWindowEvent_WorkPending, 0, 0 );
				err = map_global_value_errno( good, good );
				check_noerr( err );
			}
		#else
			#error "update to support your platform"
		#endif
		
		if( gDispatchMainQueueScheduleHookFunc )
		{
			gDispatchMainQueueScheduleHookFunc( NULL );
		}
	}
	else if( inQueue->concurrent )
	{
		#if( TARGET_OS_WINDOWS )
			LONG		prev;
			
			prev = InterlockedIncrement( &gDispatchConcurrentCount );
			check( prev >= 0 );
		#endif
		
		err = __LibDispatch_ScheduleWork( __dispatch_queue_concurrent_drain, inQueue );
		check_noerr( err );
	}
	else if( wasEmpty && !inQueue->busy && ( inQueue->suspendCount == 0 ) )
	{
		#if( TARGET_OS_WINDOWS )
			LONG		prev;
			
			prev = InterlockedIncrement( &gDispatchSerialCount );
			check( prev >= 0 );
		#endif
		
		err = __LibDispatch_ScheduleWork( __dispatch_queue_serial_drain, inQueue );
		check_noerr( err );
	}
	
	pthread_mutex_unlock( inQueue->lockPtr );
	
exit:
	return;
}

//===========================================================================================================================
//	dispatch_sync
//===========================================================================================================================

typedef struct
{
	dispatch_function_t		function;
	void *					context;
	pthread_mutex_t			mutex;
	pthread_mutex_t *		mutexPtr;
	pthread_cond_t			condition;
	pthread_cond_t *		conditionPtr;
	Boolean					done;

}	DispatchSyncItemData;

DEBUG_STATIC void	__dispatch_sync_callback( void *inContext );

void	dispatch_sync_f( dispatch_queue_t inQueue, void *inContext, dispatch_function_t inFunction )
{
	int							err;
	DispatchSyncItemData		itemData;
	
	itemData.function		= inFunction;
	itemData.context		= inContext;
	itemData.mutexPtr		= NULL;
	itemData.conditionPtr	= NULL;
	itemData.done			= false;
	
	require( DispatchQueueValid( inQueue ), exit );
	check_string( inQueue != dispatch_get_current_queue(), "DEADLOCK: illegal from current queue" );
	
	err = pthread_mutex_init( &itemData.mutex, NULL );
	require_noerr( err, exit );
	itemData.mutexPtr = &itemData.mutex;
	
	err = pthread_cond_init( &itemData.condition, NULL );
	require_noerr( err, exit );
	itemData.conditionPtr = &itemData.condition;
	
	dispatch_async_f( inQueue, &itemData, __dispatch_sync_callback );
	
	pthread_mutex_lock( itemData.mutexPtr );
	while( !itemData.done )
	{
		pthread_cond_wait( itemData.conditionPtr, itemData.mutexPtr );
	}
	pthread_mutex_unlock( itemData.mutexPtr );
	
exit:
	pthread_cond_forget( &itemData.conditionPtr );
	pthread_mutex_forget( &itemData.mutexPtr );
}

DEBUG_STATIC void	__dispatch_sync_callback( void *inContext )
{
	DispatchSyncItemData * const		itemData = (DispatchSyncItemData *) inContext;
	
	itemData->function( itemData->context );
	
	pthread_mutex_lock( itemData->mutexPtr );
		itemData->done = true;
		pthread_cond_signal( itemData->conditionPtr );
	pthread_mutex_unlock( itemData->mutexPtr );
}

void	__dispatch_empty_callback( void *inContext )
{
	(void) inContext; // Unused
	
	// Do nothing. This is only used to serialize via dispatch_sync_f().
}

//===========================================================================================================================
//	dispatch_get_global_queue
//===========================================================================================================================

dispatch_queue_t	dispatch_get_global_queue( long inPriority, unsigned long inFlags )
{
	dispatch_queue_t		queue;
	OSStatus				err;
	
	(void) inFlags; // Unused
	
	queue = NULL;
	err = LibDispatch_EnsureInitialized();
	require_noerr( err, exit );
	
	if(      inPriority == DISPATCH_QUEUE_PRIORITY_DEFAULT ) queue = gDispatchConcurrentQueues[ 1 ];
	else if( inPriority >  DISPATCH_QUEUE_PRIORITY_DEFAULT ) queue = gDispatchConcurrentQueues[ 2 ];
	else													 queue = gDispatchConcurrentQueues[ 0 ];
	require( queue, exit );
	
exit:
	return( queue );
}

//===========================================================================================================================
//	dispatch_get_current_queue
//===========================================================================================================================

dispatch_queue_t	dispatch_get_current_queue( void )
{
	dispatch_queue_t		queue;
	OSStatus				err;
	
	queue = NULL;
	err = LibDispatch_EnsureInitialized();
	require_noerr( err, exit );
	
	queue = (dispatch_queue_t) pthread_getspecific( gDispatchKey_CurrentQueue );
	
exit:
	return( queue );
}

//===========================================================================================================================
//	dispatch_get_main_queue
//===========================================================================================================================

dispatch_queue_t	dispatch_get_main_queue( void )
{
	dispatch_queue_t		queue;
	OSStatus				err;
	
	queue = NULL;
	err = LibDispatch_EnsureInitialized();
	require_noerr( err, exit );
	
	queue = gDispatchMainQueue;
	require( queue, exit );
	
exit:
	return( queue );
}

//===========================================================================================================================
//	dispatch_main
//===========================================================================================================================

#if( TARGET_OS_WINDOWS )
void	dispatch_main( void )
{
	OSStatus				err;
	dispatch_queue_t		queue;
	BOOL					result;
	MSG						msg;
	
	err = LibDispatch_EnsureInitialized();
	require_noerr( err, exit );
	
	queue = gDispatchMainQueue;
	pthread_mutex_lock( queue->lockPtr );
	while( queue->base.refCount > 0 )
	{
		queue->pending = false;
		__dispatch_queue_serial_drain_locked( queue );
		pthread_mutex_unlock( queue->lockPtr );
		
		result = GetMessage( &msg, gDispatchMainQueueWindow, 0, 0 );
		check_string( result != 0, "should never get WM_QUIT here" );
		err = map_global_value_errno( result != -1, result );
		require_noerr( err, exit );
		
		TranslateMessage( &msg );
		DispatchMessage( &msg );
		
		pthread_mutex_lock( queue->lockPtr );
	}
	
	gDispatchMainQueue = NULL;
	pthread_mutex_unlock( queue->lockPtr );
	__dispatch_queue_free( queue );
	
exit:
	return;
}
#else // !TARGET_OS_WINDOWS
void	dispatch_main( void )
{
	OSStatus				err;
	dispatch_queue_t		queue;
	
	err = LibDispatch_EnsureInitialized();
	require_noerr( err, exit );
	
	queue = gDispatchMainQueue;
	pthread_mutex_lock( queue->lockPtr );
	while( queue->base.refCount > 0 )
	{
		__dispatch_queue_serial_drain_locked( queue );
		
		while( !gDispatchMainQueue_WorkPending )
		{
			pthread_cond_wait( gDispatchMainQueue_ConditionPtr, queue->lockPtr );
		}
		gDispatchMainQueue_WorkPending = false;
	}
	
	gDispatchMainQueue = NULL;
	pthread_mutex_unlock( queue->lockPtr );
	__dispatch_queue_free( queue );
	
exit:
	return;
}
#endif // TARGET_OS_WINDOWS

//===========================================================================================================================
//	dispatch_main_drain_np
//===========================================================================================================================

void	dispatch_main_drain_np( void )
{
	require( gDispatchMainQueue, exit );
	require( dispatch_get_current_queue() == gDispatchMainQueue, exit );
	
	__dispatch_queue_serial_drain( gDispatchMainQueue );
	
exit:
	return;
}

#if 0
#pragma mark -
#pragma mark == Queues ==
#endif

//===========================================================================================================================
//	dispatch_queue_create
//===========================================================================================================================

dispatch_queue_t	dispatch_queue_create( const char *inLabel, dispatch_queue_attr_t inAttr )
{
	dispatch_queue_t		queue;
	OSStatus				err;
	
	(void) inAttr; // Unused
	
	queue = NULL;
	err = LibDispatch_EnsureInitialized();
	require_noerr( err, exit );
	
	queue = __dispatch_queue_create_internal( inLabel );
	
exit:
	return( queue );
}

//===========================================================================================================================
//	__dispatch_queue_create_internal
//===========================================================================================================================

DEBUG_STATIC dispatch_queue_t	__dispatch_queue_create_internal( const char *inLabel )
{
	dispatch_queue_t		queue;
	OSStatus				err;
	dispatch_queue_t		obj;
	
	queue = NULL;
	obj = (dispatch_queue_t) calloc( 1, sizeof( *obj ) );
	require( obj, exit );
	
	obj->base.magic		= kDispatchQueue_MagicGood;
	obj->base.refCount	= 1;
	obj->base.doFree	= __dispatch_queue_free;
	obj->priority		= DISPATCH_QUEUE_PRIORITY_DEFAULT;
	obj->itemsNext		= &obj->itemsHead;
	
	if( !inLabel ) inLabel = "";
	strlcpy( obj->label, inLabel, sizeof( obj->label ) );
	
	err = pthread_mutex_init( &obj->lock, NULL );
	require_noerr( err, exit );
	obj->lockPtr = &obj->lock;
	
	queue = obj;
	obj = NULL;
	
exit:
	if( obj ) free( obj );
	return( queue );
}

//===========================================================================================================================
//	__dispatch_queue_free
//===========================================================================================================================

DEBUG_STATIC void	__dispatch_queue_free( void *inArg )
{
	dispatch_queue_t const		queue = (dispatch_queue_t) inArg;
	
	check( queue && ( queue->base.magic == kDispatchQueue_MagicGood ) );
	check( queue->base.refCount == 0 );
	check( queue->itemsHead == NULL );
	
	pthread_mutex_lock( queue->lockPtr );
	if( queue == gDispatchMainQueue )
	{
		#if( TARGET_OS_WINDOWS )
			OSStatus		err;
			BOOL			good;
			
			good = PostMessage( gDispatchMainQueueWindow, kLibDispatchWindowEvent_WorkPending, 0, 0 );
			err = map_global_value_errno( good, good );
			check_noerr( err );
		#else
			gDispatchMainQueue_WorkPending = true;
			pthread_cond_signal( gDispatchMainQueue_ConditionPtr );
		#endif
		
		pthread_mutex_unlock( queue->lockPtr );
	}
	else
	{
		pthread_mutex_unlock( queue->lockPtr );
		
		pthread_mutex_forget( &queue->lockPtr );
		queue->base.magic = kDispatchQueue_MagicBad;
		__dispatch_free_object( queue );
	}
}

//===========================================================================================================================
//	dispatch_queue_get_label
//===========================================================================================================================

const char *	dispatch_queue_get_label( dispatch_queue_t inQueue )
{
	require( DispatchQueueValid( inQueue ), exit );
	return( inQueue->label );
	
exit:
	return( "<<INVALID>>" );
}

//===========================================================================================================================
//	__dispatch_queue_suspend
//===========================================================================================================================

DEBUG_STATIC void	__dispatch_queue_suspend( dispatch_queue_t inQueue )
{
	require( DispatchQueueValid( inQueue ), exit );
	require( inQueue != gDispatchMainQueue, exit );
	require( !inQueue->concurrent, exit );
	
	pthread_mutex_lock( inQueue->lockPtr );
		++inQueue->suspendCount;
		check( inQueue->suspendCount >= 1 );
	pthread_mutex_unlock( inQueue->lockPtr );
	
exit:
	return;
}

//===========================================================================================================================
//	__dispatch_queue_resume
//===========================================================================================================================

DEBUG_STATIC void	__dispatch_queue_resume( dispatch_queue_t inQueue )
{
	OSStatus		err;
	
	DEBUG_USE_ONLY( err );
	
	require( DispatchQueueValid( inQueue ), exit );
	require( inQueue != gDispatchMainQueue, exit );
	require( !inQueue->concurrent, exit );
	
	pthread_mutex_lock( inQueue->lockPtr );
	check( inQueue->suspendCount > 0 );
	if( ( --inQueue->suspendCount == 0 ) && inQueue->itemsHead )
	{
		#if( TARGET_OS_WINDOWS )
			LONG		prev;
			
			prev = InterlockedIncrement( &gDispatchSerialCount );
			check( prev >= 0 );
		#endif
		
		err = __LibDispatch_ScheduleWork( __dispatch_queue_serial_drain, inQueue );
		check_noerr( err );
	}
	pthread_mutex_unlock( inQueue->lockPtr );
	
exit:
	return;
}

//===========================================================================================================================
//	__dispatch_queue_concurrent_drain
//===========================================================================================================================

DEBUG_STATIC void	__dispatch_queue_concurrent_drain( void *inContext )
{
	dispatch_queue_t		lastQueue;
	dispatch_queue_t		queue;
	size_t					i, n;
	Boolean					releaseQueue;
	dispatch_item_t			item;
	
	(void) inContext; // Unused
	
	lastQueue = NULL;
	n = countof( gDispatchConcurrentQueues );
	for( i = n; i > 0; )
	{
		queue = gDispatchConcurrentQueues[ i - 1 ];
		check( DispatchQueueValid( queue ) );
		
		releaseQueue = false;
		pthread_mutex_lock( queue->lockPtr );
		if( ( ( item = queue->itemsHead ) != NULL ) && ( ( queue->itemsHead = item->next ) == NULL ) )
		{
			queue->itemsNext = &queue->itemsHead;
			releaseQueue = true;
		}
		pthread_mutex_unlock( queue->lockPtr );
		
		if( item )
		{
			if( queue != lastQueue )
			{
				pthread_setspecific( gDispatchKey_CurrentQueue, queue );
				lastQueue = queue;
			}
			
			item->function( item->context );
			free( item );
			i = n; // Reset back to the highest priority queue so we always prefer it.
		}
		else
		{
			--i;
		}
		if( releaseQueue )
		{
			dispatch_release( queue );
		}
	}
	
#if( TARGET_OS_WINDOWS )
	InterlockedDecrement( &gDispatchConcurrentCount );
#endif
}

//===========================================================================================================================
//	__dispatch_queue_serial_drain
//===========================================================================================================================

DEBUG_STATIC void	__dispatch_queue_serial_drain( void *inContext )
{
	dispatch_queue_t const		queue = (dispatch_queue_t) inContext;
	
	require( DispatchQueueValid( queue ), exit2 );
	
	pthread_mutex_lock( queue->lockPtr );
	dispatch_retain( queue ); // Retain so refCount doesn't drop to 0 with lock held.
	
	if( queue->busy ) goto exit; // Exit if another thread is already draining the queue.
	queue->busy = true;
	
	pthread_setspecific( gDispatchKey_CurrentQueue, queue );
	__dispatch_queue_serial_drain_locked( queue );
	
	queue->busy = false;
	
exit:
	pthread_mutex_unlock( queue->lockPtr );
	dispatch_release( queue ); // After lock is released so refCount doesn't drop to 0 with lock held.
	
exit2:
#if( TARGET_OS_WINDOWS )
	InterlockedDecrement( &gDispatchSerialCount );
#endif
	return;
}

//===========================================================================================================================
//	__dispatch_queue_serial_drain_locked
//
//	Note: Queue must be locked on entry, but this function may unlock and re-lock it before exiting.
//===========================================================================================================================

DEBUG_STATIC void	__dispatch_queue_serial_drain_locked( dispatch_queue_t inQueue )
{
	dispatch_item_t		item;
	Boolean				releaseQueue;
	
	check_string( ( inQueue == gDispatchMainQueue ) || ( inQueue->base.refCount > 1 ), "caller didn't retain queue" );
	check_string( dispatch_get_current_queue() == inQueue, "caller didn't set current queue" );
	
	while( ( item = inQueue->itemsHead ) != NULL )
	{
		if( inQueue->suspendCount > 0 )
		{
			break;
		}
		
		releaseQueue = false;
		if( ( inQueue->itemsHead = item->next ) == NULL )
		{
			inQueue->itemsNext = &inQueue->itemsHead;
			releaseQueue = true;
		}
		
		pthread_mutex_unlock( inQueue->lockPtr );
			item->function( item->context );
			free( item );
			if( releaseQueue )
			{
				dispatch_release( inQueue );
			}
		pthread_mutex_lock( inQueue->lockPtr );
	}
}

#if 0
#pragma mark -
#pragma mark == Sources ==
#endif

//===========================================================================================================================
//	dispatch_source_create
//===========================================================================================================================

dispatch_source_t
	dispatch_source_create( 
		dispatch_source_type_t	inType, 
		uintptr_t				inHandle, 
		unsigned long			inMask, 
		dispatch_queue_t		inQueue )
{
	dispatch_source_t		source;
	dispatch_source_t		obj;
	
	(void) inMask; // Unused
	
	source = NULL;
	obj = NULL;
	require( DispatchQueueValid( inQueue ), exit );
	
	obj = (dispatch_source_t) calloc( 1, sizeof( *obj ) );
	require( obj, exit );
	
	obj->base.magic			= kDispatchSource_MagicGood;
	obj->base.refCount		= 1;
	obj->base.doFree		= __LibDispatch_PlatformFreeSource;
	obj->queue				= inQueue;
	obj->suspendCount		= 1;
	obj->type				= inType;
	dispatch_retain( inQueue );
	
	if( ( inType == DISPATCH_SOURCE_TYPE_READ ) || ( inType == DISPATCH_SOURCE_TYPE_WRITE ) )
	{
		obj->u.rw.fd = (SocketRef) inHandle;
		
		#if( TARGET_OS_WINDOWS )
		{
			OSStatus		err;
			
			err = __LibDispatch_AddSocket( obj->u.rw.fd, obj );
			require_noerr( err, exit );
			
			obj->u.rw.fdTracked = true;
		}
		#endif
	}
	else if( inType == DISPATCH_SOURCE_TYPE_SIGNAL )
	{
		obj->u.sig.sig = (unsigned long) inHandle;
	}
	else if( inType == DISPATCH_SOURCE_TYPE_TIMER )
	{
		// "handle" and "mask" are unused for now.
	}
	else
	{
		dlogassert( "bad type: %d", inType );
		goto exit;
	}
	
	source = obj;
	obj = NULL;
	
exit:
	if( obj ) __dispatch_source_free( obj );
	return( source );
}

//===========================================================================================================================
//	__dispatch_source_free
//===========================================================================================================================

DEBUG_STATIC void	__dispatch_source_free( void *inContext )
{
	dispatch_source_t const		source = (dispatch_source_t) inContext;
	
	check( source && ( source->base.magic == kDispatchSource_MagicGood ) );
	check( source->base.refCount == 0 );
	
#if( TARGET_OS_WINDOWS )
	if( ( source->type == DISPATCH_SOURCE_TYPE_READ ) || ( source->type == DISPATCH_SOURCE_TYPE_WRITE ) )
	{
		if( source->u.rw.fdTracked )
		{
			__LibDispatch_RemoveSocket( source->u.rw.fd );
			source->u.rw.fdTracked = false;
		}
	}
#endif
	
	if( !source->canceled && source->cancelFunction )
	{
		source->cancelFunction( source->base.context );
	}
	
	dispatch_release( source->queue );
	source->base.magic = kDispatchQueue_MagicBad;
	__dispatch_free_object( source );
}

//===========================================================================================================================
//	dispatch_source_set_event_handler
//===========================================================================================================================

void	dispatch_source_set_event_handler_f( dispatch_source_t inSource, dispatch_function_t inHandler )
{
	inSource->handlerFunction = inHandler;
}

//===========================================================================================================================
//	dispatch_source_set_cancel_handler
//===========================================================================================================================

void	dispatch_source_set_cancel_handler_f( dispatch_source_t inSource, dispatch_function_t inHandler )
{
	inSource->cancelFunction = inHandler;
}

//===========================================================================================================================
//	dispatch_source_cancel
//===========================================================================================================================

void	dispatch_source_cancel( dispatch_source_t inSource )
{
	require( DispatchSourceValid( inSource ), exit );
	
	pthread_mutex_lock( inSource->queue->lockPtr );
	
	if( inSource->canceled )
	{
		pthread_mutex_unlock( inSource->queue->lockPtr );
		return;
	}
	
	inSource->canceled = true;
	__LibDispatch_PlatformDisarmSourceAndUnlock( inSource );
	
	if( inSource->cancelFunction )
	{
		dispatch_async_f( inSource->queue, inSource->base.context, inSource->cancelFunction );
	}
	
exit:
	return;
}

//===========================================================================================================================
//	dispatch_source_set_timer
//===========================================================================================================================

void
	dispatch_source_set_timer( 
		dispatch_source_t	inSource, 
		dispatch_time_t		inStart, 
		uint64_t			inInterval, 
		uint64_t			inLeeway )
{
	(void) inLeeway; // Unused
	
	require( DispatchSourceValid( inSource ), exit );
	
	pthread_mutex_lock( inSource->queue->lockPtr );
	
	inSource->u.timer.start			= inStart;
	inSource->u.timer.intervalMs	= inInterval / kNanosecondsPerMillisecond;
	if( inSource->u.timer.intervalMs == 0 ) inSource->u.timer.intervalMs = 1; // Round up to at least 1 millisecond.
	
	if( ( inSource->suspendCount == 0 ) && !inSource->canceled )
	{
		__LibDispatch_PlatformArmSourceAndUnlock( inSource );
	}
	else
	{
		pthread_mutex_unlock( inSource->queue->lockPtr );
	}
	
exit:
	return;
}

//===========================================================================================================================
//	__dispatch_source_suspend
//===========================================================================================================================

DEBUG_STATIC void	__dispatch_source_suspend( dispatch_source_t inSource )
{
	require( DispatchSourceValid( inSource ), exit );
	
	pthread_mutex_lock( inSource->queue->lockPtr );
	check( inSource->suspendCount >= 0 );
	if( inSource->suspendCount++ == 0 )
	{
		__LibDispatch_PlatformDisarmSourceAndUnlock( inSource );
	}
	else
	{
		pthread_mutex_unlock( inSource->queue->lockPtr );
	}
	
exit:
	return;
}

//===========================================================================================================================
//	__dispatch_source_resume
//===========================================================================================================================

DEBUG_STATIC void	__dispatch_source_resume( dispatch_source_t inSource )
{
	require( DispatchSourceValid( inSource ), exit );
	
	pthread_mutex_lock( inSource->queue->lockPtr );
	check( inSource->suspendCount > 0 );
	if( ( --inSource->suspendCount == 0 ) && !inSource->canceled )
	{
		__LibDispatch_PlatformArmSourceAndUnlock( inSource );
	}
	else
	{
		pthread_mutex_unlock( inSource->queue->lockPtr );
	}
	
exit:
	return;
}

//===========================================================================================================================
//	dispatch_source_get_data
//===========================================================================================================================

unsigned long	dispatch_source_get_data( dispatch_source_t inSource )
{
	require( DispatchSourceValidOrFreeing( inSource ), exit );
	
	if(      inSource->type == DISPATCH_SOURCE_TYPE_READ )	return( (unsigned long) inSource->u.rw.avail );
	else if( inSource->type == DISPATCH_SOURCE_TYPE_TIMER )	return( 0 ); // $$$ TO DO: fire count since last callback.
	else dlogassert( "'%s' doesn't make sense for source type %d", __ROUTINE__, inSource->type );
	
exit:
	return( 0 );
}

//===========================================================================================================================
//	dispatch_source_get_handle
//===========================================================================================================================

uintptr_t	dispatch_source_get_handle( dispatch_source_t inSource )
{
	require( DispatchSourceValidOrFreeing( inSource ), exit );
	
	if(      inSource->type == DISPATCH_SOURCE_TYPE_READ )		return( (uintptr_t) inSource->u.rw.fd );
	else if( inSource->type == DISPATCH_SOURCE_TYPE_WRITE )		return( (uintptr_t) inSource->u.rw.fd );
	else if( inSource->type == DISPATCH_SOURCE_TYPE_SIGNAL )	return( (uintptr_t) inSource->u.sig.sig );
	else dlogassert( "'%s' doesn't make sense for source %d", __ROUTINE__, inSource->type );
	
exit:
	return( 0 );
}

#if 0
#pragma mark -
#pragma mark == Groups ==
#endif

DEBUG_STATIC void	__dispatch_group_free( void *inObj );
DEBUG_STATIC void	__dispatch_group_notify_callback( void *inContext );

//===========================================================================================================================
//	dispatch_group_create
//===========================================================================================================================

dispatch_group_t	dispatch_group_create( void )
{
	dispatch_group_t		group;
	dispatch_group_t		obj;
	
	group = NULL;
	
	obj = (dispatch_group_t) calloc( 1, sizeof( *obj ) );
	require( obj, exit );
	
	obj->base.magic		= kDispatchGroup_MagicGood;
	obj->base.refCount	= 1;
	obj->base.doFree	= __dispatch_group_free;
	obj->outstanding	= 1;
	
	obj->sem = dispatch_semaphore_create( 0 );
	require( obj->sem, exit );
	
	group = obj;
	obj = NULL;
	
exit:
	if( obj ) dispatch_release( obj );
	return( group );
}

//===========================================================================================================================
//	__dispatch_group_free
//===========================================================================================================================

DEBUG_STATIC void	__dispatch_group_free( void *inObj )
{
	dispatch_group_t const		obj = (dispatch_group_t) inObj;
	
	dispatch_forget( &obj->sem );
	__dispatch_free_object( obj );
}

//===========================================================================================================================
//	dispatch_group_async_f
//===========================================================================================================================

typedef struct
{
	dispatch_group_t		group;
	void *					context;
	dispatch_function_t		function;

}	dispatch_group_async_params;

DEBUG_STATIC void	__dispatch_group_async_callback( void *inContext );

void
	dispatch_group_async_f( 
		dispatch_group_t	inGroup, 
		dispatch_queue_t	inQueue, 
		void *				inContext, 
		dispatch_function_t	inFunction )
{
	dispatch_group_async_params *		params;
	
	params = NULL;
	require( DispatchGroupValid( inGroup ), exit );
	
	params = (dispatch_group_async_params *) malloc( sizeof( *params ) );
	require( params, exit );
	params->group		= inGroup;
	params->context		= inContext;
	params->function	= inFunction;
	
	atomic_add_32( &inGroup->outstanding, 1 );
	dispatch_async_f( inQueue, params, __dispatch_group_async_callback );
	params = NULL;
	
exit:
	if( params ) free( params );
}

DEBUG_STATIC void	__dispatch_group_async_callback( void *inContext )
{
	dispatch_group_async_params * const		params	= (dispatch_group_async_params *) inContext;
	dispatch_group_t const					group	= params->group;
	
	params->function( params->context );
	free( params );
	
	if( atomic_add_and_fetch_32( &group->outstanding, -1 ) == 0 )
	{
		if( group->notifyQueue )
		{
			dispatch_async_f( group->notifyQueue, group, __dispatch_group_notify_callback );
		}
		else
		{
			dispatch_semaphore_signal( group->sem );
		}
	}
}

//===========================================================================================================================
//	dispatch_group_wait
//===========================================================================================================================

void	dispatch_group_wait( dispatch_group_t inGroup, uint64_t inTimeout )
{
	require( DispatchGroupValid( inGroup ), exit );
	
	if( atomic_add_and_fetch_32( &inGroup->outstanding, -1 ) != 0 )
	{
		dispatch_semaphore_wait( inGroup->sem, inTimeout );
	}
	
	inGroup->outstanding = 1;
	
exit:
	return;
}

//===========================================================================================================================
//	dispatch_group_notify_f
//===========================================================================================================================

void
	dispatch_group_notify_f( 
		dispatch_group_t	inGroup, 
		dispatch_queue_t	inQueue, 
		void *				inContext, 
		dispatch_function_t	inFunction )
{
	require( DispatchGroupValid( inGroup ), exit );
	require( inGroup->notifyQueue == NULL, exit );
	
	dispatch_retain( inGroup ); // Retain because it's legal to release the group immediately after notify.
	
	inGroup->notifyQueue	= inQueue;
	inGroup->notifyContext	= inContext;
	inGroup->notifyFunction	= inFunction;
	dispatch_retain( inQueue );
	
	if( atomic_add_and_fetch_32( &inGroup->outstanding, -1 ) == 0 )
	{
		dispatch_async_f( inQueue, inGroup, __dispatch_group_notify_callback );
	}
	
exit:
	return;
}

DEBUG_STATIC void	__dispatch_group_notify_callback( void *inContext )
{
	dispatch_group_t const			group		= (dispatch_group_t) inContext;
	dispatch_function_t const		function	= group->notifyFunction;
	void * const					context		= group->notifyContext;
	
	dispatch_release( group->notifyQueue );
	group->notifyQueue		= NULL;
	group->notifyFunction	= NULL;
	group->outstanding		= 1;
	
	function( context );
	
	dispatch_release( group );
}

//===========================================================================================================================
//	dispatch_group_enter
//===========================================================================================================================

void	dispatch_group_enter( dispatch_group_t inGroup )
{
	atomic_add_and_fetch_32( &inGroup->outstanding, 1 );
}

//===========================================================================================================================
//	dispatch_group_leave
//===========================================================================================================================

void	dispatch_group_leave( dispatch_group_t inGroup )
{
	if( atomic_add_and_fetch_32( &inGroup->outstanding, -1 ) == 0 )
	{
		if( inGroup->notifyQueue )	dispatch_async_f( inGroup->notifyQueue, inGroup, __dispatch_group_notify_callback );
		else						dispatch_semaphore_signal( inGroup->sem );
	}
}

#if 0
#pragma mark -
#pragma mark == Once ==
#endif

//===========================================================================================================================
//	dispatch_once_f_slow
//===========================================================================================================================

void	dispatch_once_f_slow( dispatch_once_t *inOnce, void *inContext, dispatch_function_t inFunction )
{
	dispatch_once_t		prev;
	
	prev = atomic_val_compare_and_swap_32( inOnce, 0, 1 );
	if( __builtin_expect( prev, 2 ) == 2 )
	{
		// Already initialized. Nothing to do.
	}
	else if( prev == 0 )
	{
		// Got lock. Initialize it and release the lock
		
		inFunction( inContext );
		atomic_read_write_barrier();
		*inOnce = 2;
	}
	else
	{
		// Another thread is initializing it. Yield while it completes.
		
		volatile atomic_once_t * const		volatileOnce = inOnce;
		
		do
		{
			atomic_yield();
			
		}	while( *volatileOnce != 2 );
		
		atomic_read_write_barrier();
	}
}

#if 0
#pragma mark -
#pragma mark == Semaphore ==
#endif

DEBUG_STATIC void	__dispatch_semaphore_free( void *inObj );

//===========================================================================================================================
//	dispatch_semaphore_create
//===========================================================================================================================

dispatch_semaphore_t	dispatch_semaphore_create( long inValue )
{
	dispatch_semaphore_t		sem;
	dispatch_semaphore_t		obj;
	OSStatus					err;
	
	sem = NULL;
	
	obj = (dispatch_semaphore_t) calloc( 1, sizeof( *obj ) );
	require( obj, exit );
	obj->base.refCount	= 1;
	obj->base.doFree	= __dispatch_semaphore_free;
	obj->value			= inValue;
	
	err = pthread_mutex_init( &obj->mutex, NULL );
	require_noerr( err, exit );
	obj->mutexPtr = &obj->mutex;
	
	err = pthread_cond_init( &obj->condition, NULL );
	require_noerr( err, exit );
	obj->conditionPtr = &obj->condition;
	
	sem = obj;
	obj = NULL;
	
exit:
	if( obj ) dispatch_release( obj );
	return( sem );
}

//===========================================================================================================================
//	__dispatch_semaphore_free
//===========================================================================================================================

DEBUG_STATIC void	__dispatch_semaphore_free( void *inObj )
{
	dispatch_semaphore_t const		obj = (dispatch_semaphore_t) inObj;
	
	pthread_mutex_forget( &obj->mutexPtr );
	pthread_cond_forget( &obj->conditionPtr );
	__dispatch_free_object( obj );
}

//===========================================================================================================================
//	dispatch_semaphore_wait
//===========================================================================================================================

long	dispatch_semaphore_wait( dispatch_semaphore_t inSem, dispatch_time_t inTimeout )
{
	OSStatus		err;
	
	if( inTimeout == DISPATCH_TIME_FOREVER )
	{
		pthread_mutex_lock( inSem->mutexPtr );
		while( inSem->value <= 0 )
		{
			err = pthread_cond_wait( inSem->conditionPtr, inSem->mutexPtr );
			check_noerr( err );
		}
		inSem->value -= 1;
		pthread_mutex_unlock( inSem->mutexPtr );
		err = kNoErr;
	}
	else
	{
		#if( TARGET_OS_POSIX )
			dispatch_time_t		nowDT;
			struct timeval		nowTV;
			struct timespec		deadline;
			
			if( ( (int64_t) inTimeout ) < 0 ) // Negative if wall clock time.
			{
				inTimeout	= (dispatch_time_t)( -( (int64_t) inTimeout ) );
				nowDT		= (dispatch_time_t)( -( (int64_t) dispatch_walltime( NULL, 0 ) ) );
			}
			else
			{
				nowDT = dispatch_time( DISPATCH_TIME_NOW, 0 );
			}
			inTimeout = ( nowDT < inTimeout ) ? ( inTimeout - nowDT ) : 0;
			gettimeofday( &nowTV, NULL );
			deadline.tv_sec  =   nowTV.tv_sec			+   ( inTimeout / 1000 );
			deadline.tv_nsec = ( nowTV.tv_usec * 1000 )	+ ( ( inTimeout % 1000 ) * kNanosecondsPerMillisecond );
			if( deadline.tv_nsec >= kNanosecondsPerSecond )
			{
				deadline.tv_sec  += 1;
				deadline.tv_nsec -= kNanosecondsPerSecond;
			}
			
			err = kNoErr;
			pthread_mutex_lock( inSem->mutexPtr );
			while( inSem->value <= 0 )
			{
				err = pthread_cond_timedwait( inSem->conditionPtr, inSem->mutexPtr, &deadline );
				if( err ) break;
			}
			if( !err ) inSem->value -= 1;
			pthread_mutex_unlock( inSem->mutexPtr );
		#else
			dlogassert( "Timeouts not supported on this platform" );
			err = kUnsupportedErr;
		#endif
	}
	return( err );
}

//===========================================================================================================================
//	dispatch_semaphore_signal
//===========================================================================================================================

long	dispatch_semaphore_signal( dispatch_semaphore_t inSem )
{
	OSStatus		err;
	
	DEBUG_USE_ONLY( err );
	
	pthread_mutex_lock( inSem->mutexPtr );
		inSem->value += 1;
		err = pthread_cond_signal( inSem->conditionPtr );
		check_noerr( err );
	pthread_mutex_unlock( inSem->mutexPtr );
	return( 0 );
}

#if 0
#pragma mark -
#pragma mark == Time ==
#endif

//===========================================================================================================================
//	dispatch_time
//===========================================================================================================================

dispatch_time_t	dispatch_time( dispatch_time_t inWhen, int64_t inDelta )
{
	if( inWhen == DISPATCH_TIME_FOREVER )
	{
		return( DISPATCH_TIME_FOREVER );
	}
	
	// Wall clock (negative).
	
	inDelta /= kNanosecondsPerMillisecond;
	if( ( (int64_t) inWhen ) < 0 )
	{
		if( inDelta >= 0 )
		{
			if( (int64_t)( inWhen -= inDelta ) >= 0 )
			{
				return( DISPATCH_TIME_FOREVER ); // overflow
			}
			return( inWhen );
		}
		if( (int64_t)( inWhen -= inDelta ) >= -1 )
		{
			return( (dispatch_time_t) -2 ); // underflow (use -2 because -1 == DISPATCH_TIME_FOREVER == forever)
		}
		return( inWhen );
	}
	
	// Default clock (milliseconds).
	
   	if( inWhen == DISPATCH_TIME_NOW )
	{
		inWhen = __dispatch_milliseconds();
	}
	if( inDelta >= 0 )
	{
		if( (int64_t)( inWhen += inDelta ) <= 0 )
		{
			return( DISPATCH_TIME_FOREVER ); // overflow
		}
		return( inWhen );
	}
	if( (int64_t)( inWhen += inDelta ) < 1 )
	{
		return( 1 ); // underflow
	}
	return( inWhen );
}

//===========================================================================================================================
//	dispatch_walltime
//===========================================================================================================================

dispatch_time_t	dispatch_walltime( const struct timespec *ioWhen, int64_t inDelta )
{
	int64_t		ms;
	
	if( ioWhen )
	{
		ms = ( ioWhen->tv_sec * INT64_C_safe( kMillisecondsPerSecond ) ) + ( ioWhen->tv_nsec / kNanosecondsPerMillisecond );
	}
	else
	{
		ms = __dispatch_wall_milliseconds();
	}
	
	inDelta /= kNanosecondsPerMillisecond;
	ms += inDelta;
	if( ms <= 1 )
	{
		// Use -2 because -1 == DISPATCH_TIME_FOREVER == forever.
		
		return( ( inDelta >= 0 ) ? DISPATCH_TIME_FOREVER : (uint64_t) INT64_C( -2 ) );
	}
	return( -ms );
}

//===========================================================================================================================
//	__dispatch_milliseconds
//===========================================================================================================================

DEBUG_STATIC uint64_t	__dispatch_milliseconds( void )
{
	return( ( UpTicks() * kMillisecondsPerSecond ) / UpTicksPerSecond() );
}

//===========================================================================================================================
//	__dispatch_wall_milliseconds
//===========================================================================================================================

DEBUG_STATIC uint64_t	__dispatch_wall_milliseconds( void )
{
#if( TARGET_OS_THREADX )
	return( time( NULL ) * UINT64_C_safe( kMillisecondsPerSecond ) ); // $$$ TO DO: use something with more resolution.
#else
	struct timeval		tv;
	
	gettimeofday( &tv, NULL );
	return( ( tv.tv_sec * UINT64_C_safe( kMillisecondsPerSecond ) ) + ( tv.tv_usec / kMicrosecondsPerMillisecond ) );
#endif
}

#if 0
#pragma mark -
#pragma mark == CF Support ==
#endif

#if( DISPATCH_LITE_CF_ENABLED )

//===========================================================================================================================
//	__LibDispatchCF_EnsureInitialized
//
//	Note: Must be called with the initialize mutex locked.
//===========================================================================================================================

DEBUG_STATIC OSStatus	__LibDispatchCF_EnsureInitialized( void )
{
	OSStatus					err;
	CFRunLoopSourceContext		sourceContext = { 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	
	require_action_quiet( !gDispatchCFInitialized, exit, err = kNoErr );
	
	sourceContext.perform = __LibDispatchCF_SourcePerform;
	gDispatchCF_Source = CFRunLoopSourceCreate( NULL, 0, &sourceContext );
	require_action( gDispatchCF_Source, exit, err = kNoMemoryErr );
	
	gDispatchCF_MainRL = CFRunLoopGetMain();
	CFRunLoopAddSource( gDispatchCF_MainRL, gDispatchCF_Source, kCFRunLoopCommonModes );
	
	gDispatchCFInitialized = true;
	err = kNoErr;
	
exit:
	if( err ) __LibDispatchCF_Finalize();
	return( err );
}

//===========================================================================================================================
//	__LibDispatchCF_Finalize
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatchCF_Finalize( void )
{	
	if( gDispatchCF_Source )
	{
		CFRunLoopSourceInvalidate( gDispatchCF_Source );
		CFRelease( gDispatchCF_Source );
		gDispatchCF_Source = NULL;
	}
	gDispatchCF_MainRL = NULL;
	gDispatchCFInitialized = false;
}

//===========================================================================================================================
//	__LibDispatchCF_Schedule
//===========================================================================================================================

DEBUG_STATIC OSStatus	__LibDispatchCF_Schedule( void )
{
	CFRunLoopSourceSignal( gDispatchCF_Source );
	CFRunLoopWakeUp( gDispatchCF_MainRL );
	return( kNoErr );
}

//===========================================================================================================================
//	__LibDispatchCF_SourcePerform
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatchCF_SourcePerform( void *inContext )
{
	(void) inContext; // Unused
	
	__dispatch_queue_serial_drain( gDispatchMainQueue );
}

#endif // DISPATCH_LITE_CF_ENABLED

#if 0
#pragma mark -
#pragma mark == KQueue Support ==
#endif

#if( DISPATCH_LITE_USE_KQUEUE )
//===========================================================================================================================
//	__LibDispatch_GetKQueue
//===========================================================================================================================

DEBUG_STATIC int	__LibDispatch_GetKQueue( void )
{
	OSStatus			err;
	int					pipeFDs[ 2 ];
	struct kevent		ke;
	int					n;
	
	pthread_mutex_lock( gDispatchKQueue_MutexPtr );
	if( gDispatchKQueue_PipeSend < 0 )
	{
		err = pipe( pipeFDs );
		err = map_global_noerr_errno( err );
		check_noerr( err );
		
		gDispatchKQueue_PipeSend = pipeFDs[ 1 ];
		gDispatchKQueue_PipeRecv = pipeFDs[ 0 ];
	}
	if( gDispatchKQueue_FD < 0 )
	{
		gDispatchKQueue_FD = kqueue();
		err = map_fd_creation_errno( gDispatchKQueue_FD );
		check_noerr( err );
		
		EV_SET( &ke, gDispatchKQueue_PipeRecv, EVFILT_READ, EV_ADD, 0, 0, 0 );
		n = kevent( gDispatchKQueue_FD, &ke, 1, NULL, 0, NULL );
		err = map_global_value_errno( n == 0, n );
		check_noerr( err );
		
		dispatch_async_f( gDispatchKQueue_DrainQueue, NULL, __LibDispatch_KQueueDrain );
	}
	pthread_mutex_unlock( gDispatchKQueue_MutexPtr );
	return( gDispatchKQueue_FD );
}

//===========================================================================================================================
//	__LibDispatch_PlatformFreeSource
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_PlatformFreeSourceCallBack( void *inContext );

DEBUG_STATIC void	__LibDispatch_PlatformFreeSource( void *inArg )
{
	dispatch_source_t const		source = (dispatch_source_t) inArg;
	
	require( DispatchSourceValidOrFreeing( source ), exit );
	
	// Mark it as canceled to prevent scheduling new work then schedule a callback on the kqueue thread to 
	// serialize the delete behind any queued work.
	
	pthread_mutex_lock( source->queue->lockPtr );
		source->canceled = true;
		__LibDispatch_KQueueDispatchAsync( __LibDispatch_PlatformFreeSourceCallBack, source );
	pthread_mutex_unlock( source->queue->lockPtr );
	
exit:
	return;
}

DEBUG_STATIC void	__LibDispatch_PlatformFreeSourceCallBack( void *inContext )
{
	dispatch_source_t const		source = (dispatch_source_t) inContext;
	
	check( source->base.refCount == 0 );
	check( source->canceled );
	
	// Make sure the kqueue entry is disarmed so it won't fire again.
	
	pthread_mutex_lock( source->queue->lockPtr );
	__LibDispatch_PlatformDisarmSourceAndUnlock( source );
	
	// Serialize the final free on the source's queue to serialize it with any previous events that may be queued.
	
	dispatch_async_f( source->queue, source, __dispatch_source_free );
}

//===========================================================================================================================
//	__LibDispatch_PlatformArmSourceAndUnlock
//
//	Note: Owning queue must be locked on entry and it will be unlocked on exit.
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_PlatformArmSourceAndUnlock( dispatch_source_t inSource )
{
	struct kevent		ke;
	int					n;
	OSStatus			err;
	unsigned short		keFlags;
	uint64_t			milliseconds;
	
	if( inSource->type == DISPATCH_SOURCE_TYPE_READ )
	{
		EV_SET_compat( &ke, inSource->u.rw.fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, inSource );
	}
	else if( inSource->type == DISPATCH_SOURCE_TYPE_WRITE )
	{
		EV_SET_compat( &ke, inSource->u.rw.fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, inSource );
	}
	else if( inSource->type == DISPATCH_SOURCE_TYPE_SIGNAL )
	{
		keFlags = inSource->started ? EV_ENABLE : EV_ADD;
		EV_SET_compat( &ke, inSource->u.sig.sig, EVFILT_SIGNAL, keFlags, 0, 0, inSource );
	}
	else if( inSource->type == DISPATCH_SOURCE_TYPE_TIMER )
	{
		keFlags = inSource->started ? EV_ENABLE : EV_ADD;
		if( ( (int64_t) inSource->u.timer.start ) < 0 ) // Wall clock time.
		{
			uint64_t		tempStart;
			
			tempStart = -inSource->u.timer.start;
			milliseconds = tempStart - __dispatch_wall_milliseconds();
			if( milliseconds > tempStart ) milliseconds = 0; // Start is in the past...force to now.
		}
		else
		{
			milliseconds = inSource->u.timer.start - __dispatch_milliseconds();
			if( milliseconds > inSource->u.timer.start ) milliseconds = 0; // Start is in the past...force to now.
		}
		if( milliseconds <= 0 ) milliseconds = 1; // 0 confuses some kqueue implementations.
		EV_SET_compat( &ke, (uintptr_t) inSource, EVFILT_TIMER, keFlags, 0, (intptr_t) milliseconds, inSource );
	}
	else
	{
		dlogassert( "unknown source type: %d", inSource->type );
		goto exit;
	}
	inSource->started = true;
	
	n = kevent( __LibDispatch_GetKQueue(), &ke, 1, NULL, 0, NULL );
	err = map_global_value_errno( n == 0, n );
	require_noerr( err, exit );
	
exit:
	pthread_mutex_unlock( inSource->queue->lockPtr );
}

//===========================================================================================================================
//	__LibDispatch_PlatformDisarmSourceAndUnlock
//
//	Note: Owning queue must be locked on entry and it will be unlocked on exit.
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_PlatformDisarmSourceAndUnlock( dispatch_source_t inSource )
{
	struct kevent		ke;
	int					n;
	OSStatus			err;
	Boolean				deleting;
	
	if( !inSource->started ) goto exit;
	
	if( inSource->type == DISPATCH_SOURCE_TYPE_READ )
	{
		EV_SET_compat( &ke, inSource->u.rw.fd, EVFILT_READ, EV_DELETE, 0, 0, inSource );
		deleting = true;
	}
	else if( inSource->type == DISPATCH_SOURCE_TYPE_WRITE )
	{
		EV_SET_compat( &ke, inSource->u.rw.fd, EVFILT_WRITE, EV_DELETE, 0, 0, inSource );
		deleting = true;
	}
	else if( inSource->type == DISPATCH_SOURCE_TYPE_SIGNAL )
	{
		deleting = inSource->canceled;
		EV_SET_compat( &ke, inSource->u.sig.sig, EVFILT_SIGNAL, deleting ? EV_DELETE : EV_DISABLE, 0, 0, inSource );
	}
	else if( inSource->type == DISPATCH_SOURCE_TYPE_TIMER )
	{	
		deleting = inSource->canceled;
		EV_SET_compat( &ke, (uintptr_t) inSource, EVFILT_TIMER, deleting ? EV_DELETE : EV_DISABLE, 0, 0, inSource );
	}
	else
	{
		dlogassert( "unknown source type: %d", inSource->type );
		goto exit;
	}
	
	n = kevent( __LibDispatch_GetKQueue(), &ke, 1, NULL, 0, NULL );
	err = map_global_value_errno( n == 0, n );
	if( err == ENOENT ) err = 0;
	if( deleting ) inSource->started = false;
	require_noerr( err, exit );
	
exit:
	pthread_mutex_unlock( inSource->queue->lockPtr );
}

//===========================================================================================================================
//	__LibDispatch_KQueueDispatchAsync
//===========================================================================================================================

DEBUG_STATIC OSStatus	__LibDispatch_KQueueDispatchAsync( dispatch_function_t inFunction, void *inContext )
{
	OSStatus					err;
	dispatch_simple_item		itemData;
	ssize_t						n;
	
	itemData.function = inFunction;
	itemData.context  = inContext;
	
	// Note: pipe reads/writes <= PIPE_BUF are atomic.
	
	check( gDispatchKQueue_PipeSend >= 0 );
	n = write( gDispatchKQueue_PipeSend, &itemData, sizeof( itemData ) );
	err = map_global_value_errno( n == (ssize_t) sizeof( itemData ), n );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	__LibDispatch_KQueueDrain
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_KQueueDrain( void *inContext )
{
	struct kevent				ke;
	int							kq;
	OSStatus					err;
	dispatch_source_t			source;
	dispatch_simple_item		dispatchItemData;
	ssize_t						n;
	
	(void) inContext; // Unused
	
	kq = __LibDispatch_GetKQueue();
	for( ;; )
	{
		n = kevent( kq, NULL, 0, &ke, 1, NULL );
		err = map_global_value_errno( n > 0, n );
		if( err == EINTR ) continue;
		require_noerr( err, exit );
		
		source = (dispatch_source_t) ke.udata;
		switch( ke.filter )
		{
			case EVFILT_READ:
				if( ke.ident == (uintptr_t) gDispatchKQueue_PipeRecv )
				{
					// Note: pipe reads/writes <= PIPE_BUF are atomic.
					
					n = read( gDispatchKQueue_PipeRecv, &dispatchItemData, sizeof( dispatchItemData ) );
					err = map_global_value_errno( n == (ssize_t) sizeof( dispatchItemData ), n );
					check_noerr( err );
					if( err ) break;
					
					if( dispatchItemData.function )
					{
						dispatchItemData.function( dispatchItemData.context );
						break;
					}
					else
					{
						gcd_ulog( kLogLevelNotice, "*** quitting kqueue thread\n" );
						goto exit;
					}
				}
				
				check( DispatchSourceValidOrFreeing( source ) );
				check( source->type == DISPATCH_SOURCE_TYPE_READ );
				
				source->u.rw.avail = ( ke.data < 0 ) ? 0 : (size_t)( ke.data );
				dispatch_async_f( source->queue, source, __LibDispatch_KQueueFDEvent );
				break;
			
			case EVFILT_WRITE:
				check( DispatchSourceValidOrFreeing( source ) );
				check( source->type == DISPATCH_SOURCE_TYPE_WRITE );
				
				source->u.rw.avail = ( ke.data < 0 ) ? 0 : (size_t)( ke.data );
				dispatch_async_f( source->queue, source, __LibDispatch_KQueueFDEvent );
				break;
			
			case EVFILT_SIGNAL:
				check( DispatchSourceValidOrFreeing( source ) );
				check( source->type == DISPATCH_SOURCE_TYPE_SIGNAL );
				
				dispatch_async_f( source->queue, source, __LibDispatch_KQueueSignalEvent );
				break;
			
			case EVFILT_TIMER:
				check( DispatchSourceValidOrFreeing( source ) );
				check( source->type == DISPATCH_SOURCE_TYPE_TIMER );
				
				if( ( atomic_fetch_and_or_32( &source->pending, 1 ) & 1 ) == 0 )
				{
					dispatch_async_f( source->queue, source, __LibDispatch_KQueueTimerEvent );
				}
				break;
			
			default:
				dlogassert( "unknown kevent filter: %d", ke.filter );
				break;
		}
	}
	
exit:
	return;
}

//===========================================================================================================================
//	__LibDispatch_KQueueFDEvent
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_KQueueFDEvent( void *inContext )
{
	dispatch_source_t const		source = (dispatch_source_t) inContext;
	
	if( source->canceled || ( source->suspendCount > 0 ) )
		return;
	
	source->handlerFunction( source->base.context );
	
	pthread_mutex_lock( source->queue->lockPtr );
	if( !source->canceled && ( source->suspendCount == 0 ) )
	{
		__LibDispatch_PlatformArmSourceAndUnlock( source );
	}
	else
	{
		pthread_mutex_unlock( source->queue->lockPtr );
	}
}

//===========================================================================================================================
//	__LibDispatch_KQueueSignalEvent
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_KQueueSignalEvent( void *inContext )
{
	dispatch_source_t const		source = (dispatch_source_t) inContext;
	
	if( source->canceled || ( source->suspendCount > 0 ) )
		return;
	
	source->handlerFunction( source->base.context );
}

//===========================================================================================================================
//	__LibDispatch_KQueueTimerEvent
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_KQueueTimerEvent( void *inContext )
{
	dispatch_source_t const		source = (dispatch_source_t) inContext;
	
	atomic_and_32( &source->pending, ~( (int32_t) 1 ) );
	if( source->canceled || ( source->suspendCount > 0 ) )
		return;
	
	source->handlerFunction( source->base.context );
}
#endif // DISPATCH_LITE_USE_KQUEUE

#if 0
#pragma mark -
#pragma mark == Select Support ==
#endif

#if( DISPATCH_LITE_USE_SELECT )

//===========================================================================================================================
//	__LibDispatch_SelectEnsureInitialized
//===========================================================================================================================

DEBUG_STATIC OSStatus	__LibDispatch_SelectEnsureInitialized( void )
{
	OSStatus		err;
	SocketRef		sock;
	sockaddr_ip		sip;
	socklen_t		len;
	
	sock = kInvalidSocketRef;
	
	gDispatchSelect_CommandQueue = __dispatch_queue_create_internal( "com.apple.select-commands" );
	require_action( gDispatchSelect_CommandQueue, exit, err = ENOMEM );
	
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
	
	gDispatchSelect_CommandSock = sock;
	sock = kInvalidSocketRef;
	
	dispatch_async_f( gDispatchSelect_CommandQueue, NULL, __LibDispatch_SelectDrain ); // $$$ TO DO: do this lazily.
	
exit:
	ForgetSocket( &sock );
	return( err );
}

//===========================================================================================================================
//	__LibDispatch_SelectDrain
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_SelectDrain( void *inArg )
{
	OSStatus				err;
	int						n;
	dispatch_source_t		curr;
	dispatch_source_t *		next;
	uint64_t				ticksPerSec;
	uint64_t				nowTicks;
	uint64_t				deltaTicks;
	struct timeval			timeout;
	struct timeval *		timeoutPtr;
	fd_set *				writeSetPtr;
	int						maxFD;
	dispatch_source_t		expiredList;
	dispatch_source_t		expiredCurr;
	
	(void) inArg;
	
	pthread_setname_np_compat( "gcd-select-drain" );
	ticksPerSec = UpTicksPerSecond();
	for( ;; )
	{
		// Set up the timeout for the nearest timer (sorted by expiration time).
		
		curr = gDispatchSelect_TimerList;
		if( curr )
		{
			nowTicks = UpTicks();
			if( nowTicks < curr->u.timer.expireTicks )
			{
				deltaTicks		= curr->u.timer.expireTicks - nowTicks;
				timeout.tv_sec  = (int32_t)(     deltaTicks / ticksPerSec );
				timeout.tv_usec = (int32_t)( ( ( deltaTicks % ticksPerSec ) * kMicrosecondsPerSecond ) / ticksPerSec );
			}
			else
			{
				timeout.tv_sec  = 0;
				timeout.tv_usec = 0;
			}
			timeoutPtr = &timeout;
		}
		else
		{
			timeoutPtr = NULL;
		}
		
		// Wait for an event.
		
		FD_ZERO( &gDispatchSelect_ReadSet );
		FD_ZERO( &gDispatchSelect_WriteSet );
		maxFD = gDispatchSelect_CommandSock;
		FD_SET( gDispatchSelect_CommandSock, &gDispatchSelect_ReadSet );
		writeSetPtr = NULL;
		for( curr = gDispatchSelect_ReadWriteList; curr; curr = curr->armedNext )
		{
			if( curr->type == DISPATCH_SOURCE_TYPE_READ )
			{
				if( !curr->canceled )
				{
					check( IsValidFD( curr->u.rw.fd ) );
					FD_SET( curr->u.rw.fd, &gDispatchSelect_ReadSet );
					if( curr->u.rw.fd > maxFD ) maxFD = curr->u.rw.fd;
				}
			}
			else if( curr->type == DISPATCH_SOURCE_TYPE_WRITE )
			{
				if( !curr->canceled )
				{
					check( IsValidFD( curr->u.rw.fd ) );
					FD_SET( curr->u.rw.fd, &gDispatchSelect_WriteSet );
					if( curr->u.rw.fd > maxFD ) maxFD = curr->u.rw.fd;
					writeSetPtr = &gDispatchSelect_WriteSet;
				}
			}
		}
		n = select( maxFD + 1, &gDispatchSelect_ReadSet, writeSetPtr, NULL, timeoutPtr );
		err = select_errno( n );
		if( !err )
		{
			// Process ready file descriptors.
			
			if( FD_ISSET( gDispatchSelect_CommandSock, &gDispatchSelect_ReadSet ) )
			{
				err = __LibDispatch_SelectHandleCommand( NULL );
				if( err ) break;
			}
			else
			{
				for( next = &gDispatchSelect_ReadWriteList; ( curr = *next ) != NULL; )
				{
					check( DispatchSourceValidOrFreeing( curr ) );
					
					if( ( curr->type == DISPATCH_SOURCE_TYPE_READ ) && 
						FD_ISSET( curr->u.rw.fd, &gDispatchSelect_ReadSet ) )
					{
						*next = curr->armedNext;
						dispatch_async_f( curr->queue, curr, __LibDispatch_SelectHandleReadWriteEvent );
					}
					else if( ( curr->type == DISPATCH_SOURCE_TYPE_WRITE ) && 
							 FD_ISSET( curr->u.rw.fd, &gDispatchSelect_WriteSet ) )
					{
						*next = curr->armedNext;
						dispatch_async_f( curr->queue, curr, __LibDispatch_SelectHandleReadWriteEvent );
					}
					else
					{
						next = &curr->armedNext;
					}
				}
			}
		}
		else if( err != kTimeoutErr )
		{
			dlogassert( "select() error: %#m", err );
			sleep( 1 );
		}
		
		// Process expired timers.
		
		expiredList = NULL;
		nowTicks = UpTicks();
		while( ( curr = gDispatchSelect_TimerList ) != NULL )
		{
			if( nowTicks < curr->u.timer.expireTicks ) break;
			gDispatchSelect_TimerList = curr->armedNext;
			curr->armedNext = expiredList;
			expiredList = curr;
			
			check( DispatchSourceValidOrFreeing( curr ) );
			check( curr->type == DISPATCH_SOURCE_TYPE_TIMER );
			
			if( ( atomic_fetch_and_or_32( &curr->pending, 1 ) & 1 ) == 0 )
			{
				curr->u.timer.count = 0;
				dispatch_async_f( curr->queue, curr, __LibDispatch_SelectHandleTimerEvent );
			}
			else
			{
				++curr->u.timer.count;
			}
		}
		
		// Re-insert expired timers based on their new expiration.
		
		while( ( expiredCurr = expiredList ) != NULL )
		{
			expiredList = expiredCurr->armedNext;
			
			deltaTicks = MillisecondsToUpTicks( expiredCurr->u.timer.intervalMs );
			add_saturate( expiredCurr->u.timer.expireTicks, deltaTicks, UINT64_MAX );
			for( next = &gDispatchSelect_TimerList; ( curr = *next ) != NULL; next = &curr->armedNext )
			{
				if( expiredCurr->u.timer.expireTicks < curr->u.timer.expireTicks )
					break;
			}
			expiredCurr->armedNext = *next;
			*next = expiredCurr;
		}
	}
	check( gDispatchSelect_ReadWriteList == NULL );
	check( gDispatchSelect_TimerList == NULL );
}

//===========================================================================================================================
//	__LibDispatch_SelectHandleCommand
//===========================================================================================================================

DEBUG_STATIC OSStatus	__LibDispatch_SelectHandleCommand( const dispatch_select_packet *inPkt )
{
	OSStatus					err;
	dispatch_select_packet		pkt;
	ssize_t						n;
	dispatch_source_t *			next;
	dispatch_source_t			curr;
	
	if( inPkt == NULL )
	{
		n = recv( gDispatchSelect_CommandSock, (char *) &pkt, sizeof( pkt ), 0 );
		err = map_socket_value_errno( gDispatchSelect_CommandSock, n >= 0, n );
		require_noerr( err, exit );
		
		inPkt = &pkt;
	}
	switch( inPkt->cmd )
	{
		case kDispatchCommandArmSource:
			if( inPkt->source->type == DISPATCH_SOURCE_TYPE_TIMER )
			{
				uint64_t		milliseconds;
				
				if( ( (int64_t) inPkt->source->u.timer.start ) < 0 ) // Wall clock time.
				{
					uint64_t		tempStart;
					
					tempStart = -inPkt->source->u.timer.start;
					milliseconds = tempStart - __dispatch_wall_milliseconds();
					if( milliseconds > tempStart ) milliseconds = 0; // Start is in the past...force to now.
				}
				else
				{
					milliseconds = inPkt->source->u.timer.start - __dispatch_milliseconds();
					if( milliseconds > inPkt->source->u.timer.start ) milliseconds = 0; // Start is in the past...force to now.
				}
				inPkt->source->u.timer.expireTicks = UpTicks() + MillisecondsToUpTicks( milliseconds );
				
				// Remove the timer if it already exists and re-insert it based on the new expiration.
				
				for( next = &gDispatchSelect_TimerList; ( curr = *next ) != NULL; next = &curr->armedNext )
				{
					if( curr == inPkt->source )
					{
						*next = curr->armedNext;
						break;
					}
				}
				for( next = &gDispatchSelect_TimerList; ( curr = *next ) != NULL; next = &curr->armedNext )
				{
					if( inPkt->source->u.timer.expireTicks < curr->u.timer.expireTicks )
						break;
				}
				inPkt->source->armedNext = *next;
				*next = inPkt->source;
			}
			else if( ( inPkt->source->type == DISPATCH_SOURCE_TYPE_READ ) ||
					 ( inPkt->source->type == DISPATCH_SOURCE_TYPE_WRITE ) )
			{
				for( next = &gDispatchSelect_ReadWriteList; ( curr = *next ) != NULL; next = &curr->armedNext )
				{
					if( curr == inPkt->source ) break;
				}
				if( !curr )
				{
					inPkt->source->armedNext = NULL;
					*next = inPkt->source;
				}
			}
			break;
		
		case kDispatchCommandDisarmSource:
			if( inPkt->source->type == DISPATCH_SOURCE_TYPE_TIMER )
			{
				for( next = &gDispatchSelect_TimerList; ( curr = *next ) != NULL; next = &curr->armedNext )
				{
					if( curr == inPkt->source )
					{
						*next = curr->armedNext;
						break;
					}
				}
			}
			else if( ( inPkt->source->type == DISPATCH_SOURCE_TYPE_READ ) ||
					 ( inPkt->source->type == DISPATCH_SOURCE_TYPE_WRITE ) )
			{
				for( next = &gDispatchSelect_ReadWriteList; ( curr = *next ) != NULL; next = &curr->armedNext )
				{
					if( curr == inPkt->source )
					{
						*next = curr->armedNext;
						break;
					}
				}
			}
			break;
		
		case kDispatchCommandFreeSource:
			check( inPkt->source->base.refCount == 0 );
			check( inPkt->source->canceled );
			
			// Make sure the kqueue entry is disarmed so it won't fire again.
			
			pthread_mutex_lock( inPkt->source->queue->lockPtr );
			__LibDispatch_PlatformDisarmSourceAndUnlock( inPkt->source );
			
			// Serialize the final free on the source's queue to serialize it with any previous events that may be queued.
			
			dispatch_async_f( inPkt->source->queue, inPkt->source, __dispatch_source_free );
			break;
		
		case kDispatchCommandQuit:
			gcd_ulog( kLogLevelInfo, "quit command received...exiting\n" );
			check( gDispatchSelect_QuitSem );
			if( gDispatchSelect_QuitSem ) dispatch_semaphore_signal( gDispatchSelect_QuitSem );
			err = kEndingErr;
			goto exit;
		
		default:
			dlogassert( "unknown command: %p", inPkt->cmd );
			err = kInternalErr;
			goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	__LibDispatch_SelectHandleReadWriteEvent
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_SelectHandleReadWriteEvent( void *inContext )
{
	dispatch_source_t const		source = (dispatch_source_t) inContext;
	
	if( source->canceled || ( source->suspendCount > 0 ) )
		return;
	
	source->handlerFunction( source->base.context );
	
	pthread_mutex_lock( source->queue->lockPtr );
	if( !source->canceled && ( source->suspendCount == 0 ) )
	{
		__LibDispatch_PlatformArmSourceAndUnlock( source );
	}
	else
	{
		pthread_mutex_unlock( source->queue->lockPtr );
	}
}

//===========================================================================================================================
//	__LibDispatch_SelectHandleTimerEvent
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_SelectHandleTimerEvent( void *inContext )
{
	dispatch_source_t const		source = (dispatch_source_t) inContext;
	
	atomic_fetch_and_and_32( &source->pending, ~( (int32_t) 1 ) );
	if( source->canceled || ( source->suspendCount > 0 ) )
		return;
	
	source->handlerFunction( source->base.context );
}

//===========================================================================================================================
//	__LibDispatch_PlatformFreeSource
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_PlatformFreeSource( void *inArg )
{
	dispatch_source_t const		source = (dispatch_source_t) inArg;
	dispatch_select_packet		pkt;
	ssize_t						n;
	OSStatus					err;
	
	DEBUG_USE_ONLY( err );
	
	require( DispatchSourceValidOrFreeing( source ), exit );
	
	// Mark it as canceled to prevent scheduling new work then schedule a callback on the kqueue thread to 
	// serialize the delete behind any queued work.
	
	pthread_mutex_lock( source->queue->lockPtr );
		source->canceled = true;
		
		pkt.cmd		= kDispatchCommandFreeSource;
		pkt.source	= source;
		n = send( gDispatchSelect_CommandSock, (char *) &pkt, sizeof( pkt ), 0 );
		err = map_socket_value_errno( gDispatchSelect_CommandSock, n == (ssize_t) sizeof( pkt ), n );
		check_noerr( err );
	pthread_mutex_unlock( source->queue->lockPtr );
	
exit:
	return;
}

//===========================================================================================================================
//	__LibDispatch_PlatformArmSourceAndUnlock
//
//	Note: Owning queue must be locked on entry and it will be unlocked on exit.
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_PlatformArmSourceAndUnlock( dispatch_source_t inSource )
{
	__LibDispatch_PlatformArmOrDisarmSourceAndUnlock( inSource, kDispatchCommandArmSource );
}

//===========================================================================================================================
//	__LibDispatch_PlatformArmOrDisarmSourceAndUnlock
//
//	Note: Owning queue must be locked on entry and it will be unlocked on exit.
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_PlatformDisarmSourceAndUnlock( dispatch_source_t inSource )
{
	__LibDispatch_PlatformArmOrDisarmSourceAndUnlock( inSource, kDispatchCommandDisarmSource );
}

//===========================================================================================================================
//	__LibDispatch_PlatformDisarmSourceAndUnlock
//
//	Note: Owning queue must be locked on entry and it will be unlocked on exit.
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_PlatformArmOrDisarmSourceAndUnlock( dispatch_source_t inSource, uint8_t inCmd )
{
	dispatch_select_packet		pkt;
	ssize_t						n;
	OSStatus					err;
	
	DEBUG_USE_ONLY( err );
	
	pkt.cmd		= inCmd;
	pkt.source	= inSource;
	
	if( dispatch_get_current_queue() == gDispatchSelect_CommandQueue )
	{
		pthread_mutex_unlock( inSource->queue->lockPtr );
		__LibDispatch_SelectHandleCommand( &pkt );
	}
	else
	{
		n = send( gDispatchSelect_CommandSock, (char *) &pkt, sizeof( pkt ), 0 );
		err = map_socket_value_errno( gDispatchSelect_CommandSock, n == (ssize_t) sizeof( pkt ), n );
		check_noerr( err );
		
		pthread_mutex_unlock( inSource->queue->lockPtr );
	}
}

#endif // DISPATCH_LITE_USE_SELECT

#if( TARGET_OS_WINDOWS )

#if 0
#pragma mark -
#pragma mark == Windows Support ==
#endif

//===========================================================================================================================
//	__LibDispatch_MainQueueWinProc
//===========================================================================================================================

DEBUG_STATIC LRESULT CALLBACK __LibDispatch_MainQueueWinProc( HWND inWindow, UINT inMsg, WPARAM inWParam, LPARAM inLParam )
{
	LRESULT		result;
	
	switch( inMsg )
	{
		case kLibDispatchWindowEvent_WorkPending:
			pthread_mutex_lock( gDispatchMainQueue->lockPtr );
				gDispatchMainQueue->pending = false;
				__dispatch_queue_serial_drain_locked( gDispatchMainQueue );
			pthread_mutex_unlock( gDispatchMainQueue->lockPtr );
			result = 0;
			break;
		
		default:
			result = DefWindowProc( inWindow, inMsg, inWParam, inLParam );
			break;
	}
	return( result );
}

//===========================================================================================================================
//	__LibDispatch_IOThreadStart
//===========================================================================================================================

typedef struct
{
	pthread_mutex_t			mutex;
	pthread_mutex_t *		mutexPtr;
	pthread_cond_t			cond;
	pthread_cond_t *		condPtr;
	OSStatus				error;
	Boolean					done;
	
}	LibDispatch_IOThreadStartData;

DEBUG_STATIC OSStatus	__LibDispatch_IOThreadStart( void )
{
	OSStatus							err;
	LibDispatch_IOThreadStartData		startData;
	
	// To send messages to a window on a specific thread, we have to create the window on that thread.
	// So we have to start the thread and make sure it is fully initialized before returning from here.
	
	startData.mutexPtr	= NULL;
	startData.condPtr	= NULL;
	startData.error		= kNoErr;
	startData.done		= false;
	
	err = pthread_mutex_init( &startData.mutex, NULL );
	require_noerr( err, exit );
	startData.mutexPtr = &startData.mutex;
	
	err = pthread_cond_init( &startData.cond, NULL );
	require_noerr( err, exit );
	startData.condPtr = &startData.cond;
	
	err = pthread_create( &gDispatchIOThread, NULL, __LibDispatch_IOThread, &startData );
	require_noerr( err, exit );
	gDispatchIOThreadPtr = &gDispatchIOThread;
	
	pthread_mutex_lock( startData.mutexPtr );
	while( !startData.done )
	{
		pthread_cond_wait( startData.condPtr, startData.mutexPtr );
	}
	err = startData.error;
	pthread_mutex_unlock( startData.mutexPtr );
	require_noerr( err, exit );
	
exit:
	pthread_mutex_forget( &startData.mutexPtr );
	pthread_cond_forget( &startData.condPtr );
	if( err ) __LibDispatch_IOThreadStop();
	return( err );
}

//===========================================================================================================================
//	__LibDispatch_IOThreadStop
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_IOThreadStop( void )
{
	BOOL			good;
	OSStatus		err;
	
	if( gDispatchIOThreadWindow )
	{
		good = PostMessage( gDispatchIOThreadWindow, WM_QUIT, 0, 0 );
		err = map_global_value_errno( good, good );
		check_noerr( err );
	}
	if( gDispatchIOThreadPtr )
	{
		check( gDispatchIOThreadPtr );
		err = pthread_join( gDispatchIOThread, NULL );
		check_noerr( err );
		gDispatchIOThreadPtr = NULL;
	}
	check( gDispatchIOThreadWindow == NULL );
}

//===========================================================================================================================
//	__LibDispatch_IOThread
//===========================================================================================================================

DEBUG_STATIC void *	__LibDispatch_IOThread( void *inArg )
{
	LibDispatch_IOThreadStartData * const		startData = (LibDispatch_IOThreadStartData *) inArg;
	OSStatus									err;
	Boolean										initialized;
	HINSTANCE									instance;
	WNDCLASSEX									wcex;
	HWND										wind;
	MSG											msg;
	BOOL										result;
	
	initialized = false;
	pthread_setname_np_compat( "libdispatch_io" );
	
	// Set up a window to process messages on this thread.
	
	instance = GetModuleHandle( NULL );
	err = map_global_value_errno( instance, instance );
	require_noerr( err, exit );
	
	wcex.cbSize			= sizeof( wcex );
	wcex.style			= 0;
	wcex.lpfnWndProc	= __LibDispatch_IOThreadWinProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= instance;
	wcex.hIcon			= NULL;
	wcex.hCursor		= NULL;
	wcex.hbrBackground	= NULL;
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= TEXT( "libdispatch_io" );
	wcex.hIconSm		= NULL;
	RegisterClassEx( &wcex );
	
	wind = CreateWindow( wcex.lpszClassName, wcex.lpszClassName, 0, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, 
		instance, NULL );
	err = map_global_value_errno( wind, wind );
	require_noerr( err, exit );
	gDispatchIOThreadWindow = wind;
	
	PeekMessage( &msg, NULL, WM_USER, WM_USER, PM_NOREMOVE ); // Force a message queue to be created.
	
	// Signal the start code that we've fully initialized.
	
	pthread_mutex_lock( startData->mutexPtr );
		startData->error = kNoErr;
		startData->done  = true;
		pthread_cond_signal( startData->condPtr );
	pthread_mutex_unlock( startData->mutexPtr );
	initialized = true;
	
	// Process messages until we receive WM_QUIT (i.e. GetMessage returns 0).
	
	for( ;; )
	{
		result = GetMessage( &msg, gDispatchIOThreadWindow, 0, 0 );
		if( result == 0 ) break;
		err = map_global_value_errno( result != -1, result );
		require_noerr( err, exit );
		
		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}
	
	result = DestroyWindow( gDispatchIOThreadWindow );
	err = map_global_value_errno( result, result );
	check_noerr( err );
	gDispatchIOThreadWindow = NULL;
	
exit:
	if( err && !initialized )
	{
		pthread_mutex_lock( startData->mutexPtr );
			startData->error = err;
			startData->done  = true;
			pthread_cond_signal( startData->condPtr );
		pthread_mutex_unlock( startData->mutexPtr );
	}
	return( NULL );
}

//===========================================================================================================================
//	__LibDispatch_IOThreadWinProc
//===========================================================================================================================

DEBUG_STATIC LRESULT CALLBACK __LibDispatch_IOThreadWinProc( HWND inWindow, UINT inMsg, WPARAM inWParam, LPARAM inLParam )
{
	LRESULT					result;
	OSStatus				err;
	SOCKET					sock;
	dispatch_source_t		source;
	DWORD					event;
	
	switch( inMsg )
	{
		case kLibDispatchWindowEvent_Socket:
			sock  = (SOCKET) inWParam;
			err   = (OSStatus) WSAGETSELECTERROR( inLParam );
			event = WSAGETSELECTEVENT( inLParam );
			if( err ) gcd_ulog( kLogLevelNotice, "### socket %d error %#m, event 0x%X\n", sock, err, event );
			
			err = __LibDispatch_LookupSocket( sock, NULL, &source );
			if( err == kNoErr )
			{
				dispatch_async_f( source->queue, source, __LibDispatch_WindowsSocketEvent );
			}
			result = 0;
			break;
		
		default:
			result = DefWindowProc( inWindow, inMsg, inWParam, inLParam );
			break;
	}
	return( result );
}

//===========================================================================================================================
//	__LibDispatch_LookupSocket
//===========================================================================================================================

DEBUG_STATIC OSStatus	__LibDispatch_LookupSocket( SOCKET inSock, dispatch_source_t inSource, dispatch_source_t *outSource )
{
	OSStatus					err;
	DWORD						hash;
	DispatchSocketHashRef *		next;
	DispatchSocketHashRef		curr;
	
	pthread_mutex_lock( gDispatchSocketHashTableMutexPtr );
	
	hash = (DWORD)( inSock % countof( gDispatchSocketHashTable ) );
	for( next = &gDispatchSocketHashTable[ hash ]; ( curr = *next ) != NULL; next = &curr->next )
	{
		if( curr->sock == inSock )
		{
			break;
		}
	}
	if( curr && outSource )			// Lookup (found)
	{
		*outSource = curr->source;
	}
	else if( curr && !inSource )	// Delete (found)
	{
		gcd_ulog( kLogLevelChatty, "removing socket %d for source %p @ %N\n", inSock, curr->source );
		
		*next = curr->next;
		free( curr );
	}
	else if( curr )					// Duplicate?
	{
		dlogassert( "socket %d already there", inSock );
		err = kDuplicateErr;
		goto exit;
	}
	else if( inSource )				// Add
	{
		gcd_ulog( kLogLevelChatty, "adding socket %d for source %p @ %N\n", inSock, inSource );
		
		curr = (DispatchSocketHashRef) malloc( sizeof( *curr ) );
		require_action( curr, exit, err = ENOMEM );
		curr->next   = NULL;
		curr->sock   = inSock;
		curr->source = inSource;
		*next = curr;
	}
	else
	{
		gcd_ulog( kLogLevelNotice, "### socket %d msg when not in list (stale, queued msg?) @ %N\n", inSock );
		err = kNotFoundErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	pthread_mutex_unlock( gDispatchSocketHashTableMutexPtr );
	return( err );
}

//===========================================================================================================================
//	__LibDispatch_PlatformFreeSource
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_PlatformFreeSource( void *inArg )
{
	dispatch_source_t const		source = (dispatch_source_t) inArg;
	
	require( DispatchSourceValidOrFreeing( source ), exit );
	
	pthread_mutex_lock( source->queue->lockPtr );
	source->canceled = true;
	__LibDispatch_PlatformDisarmSourceAndUnlock( source );
	
	// Serialize the final free on the source's queue to serialize it with any previous events that may be queued.
	
	dispatch_async_f( source->queue, source, __dispatch_source_free );
	
exit:
	return;
}

//===========================================================================================================================
//	__LibDispatch_PlatformArmSourceAndUnlock
//
//	Note: Owning queue must be locked on entry and it will be unlocked on exit.
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_PlatformArmSourceAndUnlock( dispatch_source_t inSource )
{
	OSStatus		err;
	
	if( inSource->type == DISPATCH_SOURCE_TYPE_READ )
	{
		err = WSAAsyncSelect( inSource->u.rw.fd, gDispatchIOThreadWindow, kLibDispatchWindowEvent_Socket, 
			FD_READ | FD_ACCEPT | FD_CONNECT | FD_CLOSE );
		err = map_global_value_errno( err != SOCKET_ERROR, err );
		require_noerr( err, exit );
	}
	else if( inSource->type == DISPATCH_SOURCE_TYPE_WRITE )
	{
		err = WSAAsyncSelect( inSource->u.rw.fd, gDispatchIOThreadWindow, kLibDispatchWindowEvent_Socket, FD_WRITE );
		err = map_global_value_errno( err != SOCKET_ERROR, err );
		require_noerr( err, exit );
	}
	else if( inSource->type == DISPATCH_SOURCE_TYPE_TIMER )
	{
		int64_t			signedStart;
		uint64_t		milliseconds;
		BOOL			good;
		
		signedStart = (int64_t) inSource->u.timer.start;
		if( signedStart < 0 ) // Wall clock time.
		{
			uint64_t		tempStart;
			
			tempStart = (uint64_t) -signedStart;
			milliseconds = tempStart - __dispatch_wall_milliseconds();
			if( milliseconds > tempStart ) milliseconds = 0; // Start is in the past...force to now.
		}
		else
		{
			milliseconds = inSource->u.timer.start - __dispatch_milliseconds();
			if( milliseconds > inSource->u.timer.start ) milliseconds = 0; // Start is in the past...force to now.
		}
		if( milliseconds <= 0 ) milliseconds = 1;
		
		check( inSource->u.timer.timer == NULL );
		good = CreateTimerQueueTimer( &inSource->u.timer.timer, NULL, __LibDispatch_WindowsTimerCallBack, 
			inSource, (DWORD) milliseconds, (DWORD) inSource->u.timer.intervalMs, WT_EXECUTEDEFAULT );
		err = map_global_value_errno( good, good );
		require_noerr( err, exit );
	}
	else
	{
		dlogassert( "unknown source type: %d", inSource->type );
		err = EINVAL;
		goto exit;
	}
	inSource->started = true;
	
exit:
	pthread_mutex_unlock( inSource->queue->lockPtr );
}

//===========================================================================================================================
//	__LibDispatch_PlatformDisarmSourceAndUnlock
//
//	Note: Owning queue must be locked on entry and it will be unlocked on exit.
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_PlatformDisarmSourceAndUnlock( dispatch_source_t inSource )
{
	OSStatus		err;
	BOOL			good;
	Boolean			locked;
	HANDLE			completionEvent;
	DWORD			waitResult;
	
	locked = true;
	completionEvent = NULL;
	if( !inSource->started ) goto exit2;
	
	if( ( inSource->type == DISPATCH_SOURCE_TYPE_READ ) || ( inSource->type == DISPATCH_SOURCE_TYPE_WRITE ) )
	{
		err = WSAAsyncSelect( inSource->u.rw.fd, gDispatchIOThreadWindow, kLibDispatchWindowEvent_Socket, 0 );
		err = map_global_value_errno( err != SOCKET_ERROR, err );
		require_noerr( err, exit );
		
		#if( TARGET_OS_WINDOWS )
			if( inSource->canceled && inSource->u.rw.fdTracked )
			{
				__LibDispatch_RemoveSocket( inSource->u.rw.fd );
				inSource->u.rw.fdTracked = false;
			}
		#endif
	}
	else if( inSource->type == DISPATCH_SOURCE_TYPE_TIMER )
	{
		if( inSource->u.timer.timer )
		{
			completionEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
			err = map_global_value_errno( completionEvent, completionEvent );
			require_noerr( err, exit );
			
			good = DeleteTimerQueueTimer( NULL, inSource->u.timer.timer, completionEvent );
			err = map_global_value_errno( good, good );
			require_noerr( err, exit );
			inSource->u.timer.timer = NULL;
			
			pthread_mutex_unlock( inSource->queue->lockPtr );
			locked = false;
			
			waitResult = WaitForSingleObject( completionEvent, INFINITE );
			err = map_global_value_errno( waitResult == WAIT_OBJECT_0, waitResult );
			require_noerr( err, exit );
		}
	}
	else
	{
		dlogassert( "unknown source type: %d", inSource->type );
		err = EINVAL;
		goto exit;
	}
	
exit:
	inSource->started = false;
exit2:
	if( locked )			pthread_mutex_unlock( inSource->queue->lockPtr );
	if( completionEvent ) 	CloseHandle( completionEvent );
}

//===========================================================================================================================
//	__LibDispatch_WindowsSocketEvent
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_WindowsSocketEvent( void *inContext )
{
	dispatch_source_t const		source = (dispatch_source_t) inContext;
	
	if( source->canceled || ( source->suspendCount > 0 ) )
		return;
	
	source->handlerFunction( source->base.context );
	
	pthread_mutex_lock( source->queue->lockPtr );
	if( !source->canceled && ( source->suspendCount == 0 ) )
	{
		__LibDispatch_PlatformArmSourceAndUnlock( source );
	}
	else
	{
		pthread_mutex_unlock( source->queue->lockPtr );
	}
}

//===========================================================================================================================
//	__LibDispatch_WindowsTimerCallBack
//===========================================================================================================================

DEBUG_STATIC void CALLBACK	__LibDispatch_WindowsTimerCallBack( void *inArg, BOOLEAN inFired )
{
	dispatch_source_t const		source = (dispatch_source_t) inArg;
	
	(void) inFired; // Unused

	if( ( atomic_fetch_and_or_32( &source->pending, 1 ) & 1 ) == 0 )
	{
		dispatch_async_f( source->queue, source, __LibDispatch_WindowsTimerEvent );
	}
}

//===========================================================================================================================
//	__LibDispatch_WindowsTimerEvent
//===========================================================================================================================

DEBUG_STATIC void	__LibDispatch_WindowsTimerEvent( void *inContext )
{
	dispatch_source_t const		source = (dispatch_source_t) inContext;
	
	atomic_fetch_and_and_32( &source->pending, ~( (int32_t) 1 ) );
	if( source->canceled || ( source->suspendCount > 0 ) )
		return;
	
	source->handlerFunction( source->base.context );
}
#endif // TARGET_OS_WINDOWS

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	DispatchLite_Test
//===========================================================================================================================

void	DispatchLite_Test1( void *inContext );
void	DispatchLite_Test2( void *inContext );
void	DispatchLiteTest_GroupWork( void *inContext );
void	DispatchLiteTest_GroupNotify( void *inContext );
void	DispatchLite_OnceTest( void *inContext );
void	DispatchLite_TestQueueFinalizerCallBack( void *inContext );
void	DispatchLite_CancelCallBack( void *inContext );
#if( TARGET_OS_POSIX )
	void	DispatchLite_ReadTestCallBack( void *inContext );
	void	DispatchLite_WriteTestCallBack( void *inContext );
#endif
void	DispatchLite_TimerTestCallBack( void *inContext );
void	DispatchLite_TimerTest2CallBack( void *inContext );
void	DispatchLite_AfterCallBack( void *inContext );
#if( DISPATCH_LITE_CF_ENABLED )
	void	DispatchLite_TestCF( void *inContext );
#endif
#if( TARGET_OS_WINDOWS )
	void	DispatchLite_TestWindowsMessageLoop( void *inContext );
#endif

void	DispatchLiteTest_WaitUntilDone( void );
void	DispatchLiteTest_WaitUntilSourceDone( void );

dispatch_queue_t		gDispatchLiteTestQueue	= NULL;
volatile int32_t		gDispatchLiteTest1		= 0;
volatile int32_t		gDispatchLiteTest2		= 0;
volatile Boolean		gDispatchLiteTestDone	= false;
volatile OSStatus		gDispatchLiteTestErr	= kNoErr;
volatile Boolean		gDispatchLiteSourceDone	= false;

typedef struct
{
	dispatch_source_t		source;
	void *					context;

}	SourceTuple;

OSStatus	DispatchLite_Test( void )
{
	OSStatus				err;
	dispatch_queue_t		dq;
	dispatch_group_t		group;
	dispatch_semaphore_t	sem;
#if( TARGET_OS_POSIX )
	int						fd;
#endif
	dispatch_source_t		source;
	SourceTuple				sourceTuple;
	int						n;
	dispatch_once_t			once;
	struct timespec			ts;
	dispatch_time_t			dt1, dt2;
	int64_t					delta;
	uint64_t				u64;
	
	gDispatchLiteTestQueue	= NULL;
	gDispatchLiteTest1		= 0;
	gDispatchLiteTest2		= 0;
	gDispatchLiteTestDone	= false;
	gDispatchLiteTestErr	= kNoErr;
	gDispatchLiteSourceDone	= false;
	
	// Serial Test
	
	gcd_ulog( kLogLevelMax, "\n" );
	gcd_ulog( kLogLevelMax, "== %s: serial test\n", __ROUTINE__ );
	
	dq = dispatch_queue_create( "SerialTest", NULL );
	require_action( dq, exit, err = -1 );
	dispatch_set_finalizer_f( dq, DispatchLite_TestQueueFinalizerCallBack );
	gDispatchLiteTestQueue = dq;
	
	dispatch_async_f( dq, (void *)(intptr_t)   5, DispatchLite_Test1 );
	dispatch_async_f( dq, (void *)(intptr_t)  50, DispatchLite_Test1 );
	dispatch_async_f( dq, (void *)(intptr_t) 500, DispatchLite_Test1 );
	
	dispatch_sync_f( dq, (void *)(intptr_t) 5000, DispatchLite_Test1 );
	require_action( gDispatchLiteTest1 == 5555, exit, err = kResponseErr );
	require_action( !gDispatchLiteTestDone, exit, err = -1 );
	
	dispatch_release( dq );
	DispatchLiteTest_WaitUntilDone();
	
	// Concurrent Test
	
	gcd_ulog( kLogLevelMax, "\n" );
	gcd_ulog( kLogLevelMax, "== %s: concurrent test\n", __ROUTINE__ );
	
	dq = dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_LOW, 0 );
	dispatch_async_f( dq, (void *)(intptr_t) 5, DispatchLite_Test2 );
	
	dq = dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_DEFAULT, 0 );
	dispatch_async_f( dq, (void *)(intptr_t) 50, DispatchLite_Test2 );
	
	dq = dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_HIGH, 0 );
	dispatch_async_f( dq, (void *)(intptr_t)  500, DispatchLite_Test2 );
	dispatch_async_f( dq, (void *)(intptr_t) 5000, DispatchLite_Test2 );
	
	DispatchLiteTest_WaitUntilDone();
	require_action( gDispatchLiteTest2 == 5555, exit, err = kResponseErr );
	
	// Group Test -- sync
	
	gcd_ulog( kLogLevelMax, "\n" );
	gcd_ulog( kLogLevelMax, "== %s: group test sync\n", __ROUTINE__ );
	
	gDispatchLiteTest1 = 0;
	
	group = dispatch_group_create();
	require_action( group, exit, err = -1 );
	
	dq = dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_LOW, 0 );
	dispatch_group_async_f( group, dq, (void *)(intptr_t) 3, DispatchLiteTest_GroupWork );
	
	dq = dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_DEFAULT, 0 );
	dispatch_group_async_f( group, dq, (void *)(intptr_t) 30, DispatchLiteTest_GroupWork );
	
	dq = dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_HIGH, 0 );
	dispatch_group_async_f( group, dq, (void *)(intptr_t) 300, DispatchLiteTest_GroupWork );
	
	dispatch_group_wait( group, DISPATCH_TIME_FOREVER );
	require_action( gDispatchLiteTest1 == 333, exit, err = kResponseErr );
	
	dispatch_release( group );
	
	// Group Test -- async
	
	gcd_ulog( kLogLevelMax, "\n" );
	gcd_ulog( kLogLevelMax, "== %s: serial test async\n", __ROUTINE__ );
	
	gDispatchLiteTest1 = 0;
	
	group = dispatch_group_create();
	require_action( group, exit, err = -1 );
	
	dq = dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_HIGH, 0 );
	dispatch_group_async_f( group, dq, (void *)(intptr_t) 3, DispatchLiteTest_GroupWork );
	
	dq = dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_DEFAULT, 0 );
	dispatch_group_async_f( group, dq, (void *)(intptr_t) 30, DispatchLiteTest_GroupWork );
	
	dq = dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_LOW, 0 );
	dispatch_group_async_f( group, dq, (void *)(intptr_t) 300, DispatchLiteTest_GroupWork );
	
	dq = dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_HIGH, 0 );
	dispatch_group_notify_f( group, dq, NULL, DispatchLiteTest_GroupNotify );
	
	DispatchLiteTest_WaitUntilDone();
	require_action( gDispatchLiteTest1 == 333, exit, err = -1 );
	
	dispatch_release( group );
	
	// Once Test
	
	gcd_ulog( kLogLevelMax, "\n" );
	gcd_ulog( kLogLevelMax, "== %s: once test\n", __ROUTINE__ );
	
	n = 0;
	once = 0;
	dispatch_once_f( &once, &n, DispatchLite_OnceTest );
	require_action( n == 1, exit, err = -1 );
	dispatch_once_f( &once, &n, DispatchLite_OnceTest );
	require_action( n == 1, exit, err = -1 );
	dispatch_once_f( &once, &n, DispatchLite_OnceTest );
	require_action( n == 1, exit, err = -1 );
	
	// Semaphore Test
	
	gcd_ulog( kLogLevelMax, "\n" );
	gcd_ulog( kLogLevelMax, "== %s: semaphore test\n", __ROUTINE__ );
	
	sem = dispatch_semaphore_create( 0 );
	require_action( sem, exit, err = -1 );
	
	dispatch_semaphore_signal( sem );
	dispatch_semaphore_signal( sem );
	err = (OSStatus) dispatch_semaphore_wait( sem, DISPATCH_TIME_FOREVER );
	require_noerr( err, exit );
	err = (OSStatus) dispatch_semaphore_wait( sem, DISPATCH_TIME_FOREVER );
	require_noerr( err, exit );
	
	dispatch_release( sem );
	
	// Time Test
	
#if( TARGET_OS_WINDOWS )
	ts.tv_sec = (long) time( NULL );
#else
	ts.tv_sec = time( NULL );
#endif
	ts.tv_nsec = 0;
	dt1 = dispatch_walltime( &ts, 0 );
	dt2 = dispatch_walltime( NULL, 0 );
	delta = (int64_t)( dt1 - dt2 );
	if( delta < 0 ) delta = -delta;
	require_action( delta <= 2000000000, exit, err = kResponseErr );
	
	// Timer Test
	
	gcd_ulog( kLogLevelMax, "\n" );
	gcd_ulog( kLogLevelMax, "== %s: timer test\n", __ROUTINE__ );
	
	dq = dispatch_queue_create( "TimerTest", NULL );
	require_action( dq, exit, err = -1 );
	gDispatchLiteTestQueue = dq;
	
	n = 0;
	source = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dq );
	require_action( source, exit, err = -1 );
	sourceTuple.source = source;
	sourceTuple.context = &n;
	dispatch_set_context( source, &sourceTuple );
	dispatch_source_set_event_handler_f( source, DispatchLite_TimerTestCallBack );
	dispatch_source_set_cancel_handler_f( source, DispatchLite_CancelCallBack );
	dispatch_source_set_timer( source, dispatch_time( DISPATCH_TIME_NOW, 200 * kNanosecondsPerMillisecond ), 
		200 * kNanosecondsPerMillisecond, kNanosecondsPerMillisecond );
	dispatch_resume( source );
	
	DispatchLiteTest_WaitUntilDone();
	require_action( n == 3, exit, err = -1 );
	
	dispatch_release( source );
	DispatchLiteTest_WaitUntilSourceDone();
	
	// Timer Test 2
	
	gcd_ulog( kLogLevelMax, "\n" );
	gcd_ulog( kLogLevelMax, "== %s: timer test 2\n", __ROUTINE__ );
	
	n = 0;
	source = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dq );
	require_action( source, exit, err = -1 );
	sourceTuple.source = source;
	sourceTuple.context = &n;
	dispatch_set_context( source, &sourceTuple );
	dispatch_source_set_event_handler_f( source, DispatchLite_TimerTest2CallBack );
	dispatch_source_set_cancel_handler_f( source, DispatchLite_CancelCallBack );
	dispatch_source_set_timer( source, dispatch_time( DISPATCH_TIME_NOW, 1000 * kNanosecondsPerMillisecond ), 
		DISPATCH_TIME_FOREVER, 0 );
	dispatch_resume( source );
	
	u64 = UpTicks();
	DispatchLiteTest_WaitUntilDone();
	u64 = UpTicksToMilliseconds( UpTicks() - u64 );
	require_action( n == 3, exit, err = kResponseErr );
	require_action( ( u64 >= 2950 ) && ( u64 <= 3200 ), exit, err = kRangeErr );
	
	dispatch_release( source );
	DispatchLiteTest_WaitUntilSourceDone();
	
	// dispatch_after
	
	gcd_ulog( kLogLevelMax, "\n" );
	gcd_ulog( kLogLevelMax, "== %s: dispatch_after test\n", __ROUTINE__ );
	
	n = 0;
	sourceTuple.source = NULL;
	sourceTuple.context = &n;
	u64 = UpTicks();
	dispatch_after_f( dispatch_time( DISPATCH_TIME_NOW, 100 * kNanosecondsPerMillisecond ), dq, &sourceTuple, 
		DispatchLite_AfterCallBack );
	DispatchLiteTest_WaitUntilDone();
	u64 = UpTicksToMilliseconds( UpTicks() - u64 );
	require_action( n == 1, exit, err = -1 );
	require_action( ( u64 >= 90 ) && ( u64 <= 200 ), exit, err = kRangeErr );
	
	// Write Test
	
#if( TARGET_OS_POSIX )
	gcd_ulog( kLogLevelMax, "\n" );
	gcd_ulog( kLogLevelMax, "== %s: write test\n", __ROUTINE__ );
	
	dq = dispatch_queue_create( "WriteTest", NULL );
	require_action( dq, exit, err = -1 );
	gDispatchLiteTestQueue = dq;
	
	fd = open( "/tmp/DispatchLiteReadWriteTest", O_CREAT | O_RDWR, S_IRWXU ); 
	err = map_fd_creation_errno( fd );
	require_noerr( err, exit );
	
	source = dispatch_source_create( DISPATCH_SOURCE_TYPE_WRITE, fd, 0, dq );
	require_action( source, exit, err = -1 );
	dispatch_set_context( source, source );
	dispatch_source_set_event_handler_f( source, DispatchLite_WriteTestCallBack );
	dispatch_source_set_cancel_handler_f( source, DispatchLite_CancelCallBack );
	dispatch_resume( source );
	
	DispatchLiteTest_WaitUntilDone();
	require_noerr( gDispatchLiteTestErr, exit );
	
	dispatch_release( source );
	DispatchLiteTest_WaitUntilSourceDone();
	err = close( fd );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
	
	// Read Test
	
	gcd_ulog( kLogLevelMax, "\n" );
	gcd_ulog( kLogLevelMax, "== %s: read test\n", __ROUTINE__ );
	
	fd = open( "/tmp/DispatchLiteReadWriteTest", O_RDONLY, 0 ); 
	err = map_fd_creation_errno( fd );
	require_noerr( err, exit );
	
	source = dispatch_source_create( DISPATCH_SOURCE_TYPE_READ, fd, 0, dq );
	require_action( source, exit, err = -1 );
	dispatch_set_context( source, source );
	dispatch_source_set_event_handler_f( source, DispatchLite_ReadTestCallBack );
	dispatch_source_set_cancel_handler_f( source, DispatchLite_CancelCallBack );
	dispatch_resume( source );
	
	DispatchLiteTest_WaitUntilDone();
	require_noerr( gDispatchLiteTestErr, exit );
	
	dispatch_release( source );
	DispatchLiteTest_WaitUntilSourceDone();
	err = close( fd );
	err = map_global_noerr_errno( err );
	require_noerr( err, exit );
	unlink( "/tmp/DispatchLiteReadWriteTest" );
	
	dispatch_release( dq );
#else
	gcd_ulog( kLogLevelMax, "### writes not implemented so read/write test skipped\n" );
#endif
	
	// Main Queue Test
	
	usleep( 100000 ); // 100ms delay so logging doesn't intersperse.
	gcd_ulog( kLogLevelMax, "\n" );
	gcd_ulog( kLogLevelMax, "== %s: main queue test\n", __ROUTINE__ );
	
	dq = dispatch_get_main_queue();
	gDispatchLiteTestQueue = dq;
	
	n = 0;
	source = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dq );
	require_action( source, exit, err = -1 );
	sourceTuple.source = source;
	sourceTuple.context = &n;
	dispatch_set_context( source, &sourceTuple );
	dispatch_source_set_event_handler_f( source, DispatchLite_TimerTestCallBack );
	dispatch_source_set_cancel_handler_f( source, DispatchLite_CancelCallBack );
	dispatch_source_set_timer( source, dispatch_time( DISPATCH_TIME_NOW, 200 * kNanosecondsPerMillisecond ), 
		200 * kNanosecondsPerMillisecond, kNanosecondsPerMillisecond );
	dispatch_resume( source );
	
	dispatch_main();
	require_action( n == 3, exit, err = -1 );
	
	LibDispatch_Finalize();
	
	// CF Test
	
#if( DISPATCH_LITE_CF_ENABLED )
	gcd_ulog( kLogLevelMax, "\n" );
	gcd_ulog( kLogLevelMax, "== %s: CF test\n", __ROUTINE__ );
	
	dq = dispatch_get_main_queue();
	gDispatchLiteTestQueue = dq;
	
	gDispatchLiteTest1 = 0;
	dispatch_async_f( dq, (void *)(intptr_t) 1234, DispatchLite_TestCF );
	dispatch_async_f( dq, (void *)(intptr_t) 2345, DispatchLite_TestCF );
	
	CFRunLoopRun();
	require_action( gDispatchLiteTest1 == 3579, exit, err = -1 );
#endif
	
	// Windows Message Loop Test
	
#if( TARGET_OS_WINDOWS )
	gcd_ulog( kLogLevelMax, "\n" );
	gcd_ulog( kLogLevelMax, "== %s: Windows message loop test\n", __ROUTINE__ );
	
	gDispatchLiteTestDone = false;

	dq = dispatch_get_main_queue();
	gDispatchLiteTestQueue = dq;
	
	gDispatchLiteTest1 = 0;
	dispatch_async_f( dq, (void *)(intptr_t) 1234, DispatchLite_TestWindowsMessageLoop );
	dispatch_async_f( dq, (void *)(intptr_t) 2345, DispatchLite_TestWindowsMessageLoop );
	
	while( !gDispatchLiteTestDone )
	{
		BOOL		result;
		MSG			msg;
		
		result = GetMessage( &msg, NULL, 0, 0 );
		check_string( result != 0, "should never get WM_QUIT here" );
		err = map_global_value_errno( result != -1, result );
		require_noerr( err, exit );
		
		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}
	require_action( gDispatchLiteTest1 == 3579, exit, err = -1 );
#endif
	
exit:
	printf( "DispatchLite_Test: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

void	DispatchLite_Test1( void *inContext )
{
	int		x;
	
	require( dispatch_get_current_queue() == gDispatchLiteTestQueue, exit );
	
	x = (int)(intptr_t) inContext;
	gcd_ulog( kLogLevelMax, "\t%s: pthread = %p, x = %5d, gDispatchLiteTest1 = %5d\n", 
		__ROUTINE__, (uintptr_t) pthread_self(), x, gDispatchLiteTest1 );
	if(      ( x ==    5 ) && ( gDispatchLiteTest1 ==   0 ) ) gDispatchLiteTest1 += x;
	else if( ( x ==   50 ) && ( gDispatchLiteTest1 ==   5 ) ) gDispatchLiteTest1 += x;
	else if( ( x ==  500 ) && ( gDispatchLiteTest1 ==  55 ) ) gDispatchLiteTest1 += x;
	else if( ( x == 5000 ) && ( gDispatchLiteTest1 == 555 ) ) gDispatchLiteTest1 += x;
	else
	{
		dlogassert( "bad dispatch: x = %d, gDispatchLiteTest1 = %d", x, gDispatchLiteTest1 );
	}
	
exit:
	return;
}

void	DispatchLite_Test2( void *inContext )
{
	int32_t		x;
	
	x = (int32_t)(intptr_t) inContext;
	if(      x ==   5 ) require( dispatch_get_current_queue() == dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_LOW, 0 ), exit );
	else if( x ==  50 ) require( dispatch_get_current_queue() == dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_DEFAULT, 0 ), exit );
	else if( x == 500 ) require( dispatch_get_current_queue() == dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_HIGH, 0 ), exit );
	else				require( dispatch_get_current_queue() == dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_HIGH, 0 ), exit );
	
	usleep( 10000 );
	
	gcd_ulog( kLogLevelMax, "\t%s: pthread = %p, x = %5d\n", __ROUTINE__, (uintptr_t) pthread_self(), x );
	x = atomic_add_and_fetch_32( (int32_t *) &gDispatchLiteTest2, x );
	if( x == 5555 )
	{
		gDispatchLiteTestDone = true;
	}
	
exit:
	return;
}

void	DispatchLiteTest_GroupWork( void *inContext )
{
	int32_t		x;
	
	x = (int32_t)(intptr_t) inContext;
	if( x == 3 ) usleep( 10000 );
	atomic_add_and_fetch_32( (int32_t *) &gDispatchLiteTest1, x );
	
	gcd_ulog( kLogLevelMax, "\t%s: pthread = %p, x = %5d\n", __ROUTINE__, (uintptr_t) pthread_self(), x );
}

void	DispatchLiteTest_GroupNotify( void *inContext )
{
	int32_t		x;
	
	(void) inContext; // Unused
	
	x = gDispatchLiteTest1;
	if( x == 333 )
	{
		gDispatchLiteTestDone = true;
	}
	else
	{
		dlogassert( "group notify when not complete: %d", x );
	}
	gcd_ulog( kLogLevelMax, "\t%s: pthread = %p, x = %5d\n", __ROUTINE__, (uintptr_t) pthread_self(), x );
}

void	DispatchLite_OnceTest( void *inContext )
{
	*( (int *) inContext ) += 1;
}

void	DispatchLite_TestQueueFinalizerCallBack( void *inContext )
{
	(void) inContext;
	
	gDispatchLiteTestDone = true;
}

void	DispatchLite_CancelCallBack( void *inContext )
{
	(void) inContext; // Unused
	
	gDispatchLiteSourceDone = true;
}

#if( TARGET_OS_POSIX )
void	DispatchLite_ReadTestCallBack( void *inContext )
{
	dispatch_source_t const		source = (dispatch_source_t) inContext;
	OSStatus					err;
	int							fd;
	char						buf[ 4 ];
	ssize_t						n;
	
	require_action( dispatch_get_current_queue() == gDispatchLiteTestQueue, exit, err = -1 );
#if( !DISPATCH_LITE_USE_SELECT )
	require_action( dispatch_source_get_data( source ) == 4, exit, err = -1 );
#endif
	
	fd = (int) dispatch_source_get_handle( source );
	n = read( fd, buf, 4 );
	err = map_global_value_errno( n == 4, n );
	require_noerr( err, exit );
	
	gcd_ulog( kLogLevelNotice, "\t%s: err = %#m\n", __ROUTINE__, err );
	dispatch_source_cancel( source );
	gDispatchLiteTestDone = true;
	
exit:
	gDispatchLiteTestErr = err;
}

void	DispatchLite_WriteTestCallBack( void *inContext )
{
	dispatch_source_t const		source = (dispatch_source_t) inContext;
	OSStatus					err;
	int							fd;
	ssize_t						n;
	
	require_action( dispatch_get_current_queue() == gDispatchLiteTestQueue, exit, err = -1 );
	
	fd = (int) dispatch_source_get_handle( source );
	n = write( fd, "test", 4 );
	err = map_global_value_errno( n == 4, n );
	require_noerr( err, exit );
	
	gcd_ulog( kLogLevelNotice, "\t%s: err = %#m\n", __ROUTINE__, err );
	dispatch_source_cancel( source );
	gDispatchLiteTestDone = true;
	
exit:
	gDispatchLiteTestErr = err;
}
#endif // TARGET_OS_POSIX

void	DispatchLite_TimerTestCallBack( void *inContext )
{
	SourceTuple * const			context = (SourceTuple *) inContext;
	int * const					n = (int *)( context->context );
	
	require_action( dispatch_get_current_queue() == gDispatchLiteTestQueue, exit, gDispatchLiteTestErr = -1 );
	
	*n += 1;
	gcd_ulog( kLogLevelNotice, "\t%s: n = %d\n", __ROUTINE__, *n );
	if( *n == 3 )
	{
		dispatch_source_cancel( context->source );
		if( dispatch_get_current_queue() == dispatch_get_main_queue() )
		{
			dispatch_release( context->source );
			dispatch_release( dispatch_get_main_queue() );
		}
		gDispatchLiteTestDone = true;
	}
	
exit:
	return;
}

void	DispatchLite_TimerTest2CallBack( void *inContext )
{
	SourceTuple * const			context = (SourceTuple *) inContext;
	int * const					n = (int *)( context->context );
	
	require_action( dispatch_get_current_queue() == gDispatchLiteTestQueue, exit, gDispatchLiteTestErr = -1 );
	
	*n += 1;
	gcd_ulog( kLogLevelNotice, "\t%s: n = %d\n", __ROUTINE__, *n );
	if( *n == 3 )
	{
		dispatch_source_cancel( context->source );
		if( dispatch_get_current_queue() == dispatch_get_main_queue() )
		{
			dispatch_release( context->source );
			dispatch_release( dispatch_get_main_queue() );
		}
		gDispatchLiteTestDone = true;
	}
	else
	{
		dispatch_source_set_timer( context->source, dispatch_time( DISPATCH_TIME_NOW, 1000 * kNanosecondsPerMillisecond ), 
			DISPATCH_TIME_FOREVER, 0 );
	}
	
exit:
	return;
}

void	DispatchLite_AfterCallBack( void *inContext )
{
	SourceTuple * const			context = (SourceTuple *) inContext;
	int * const					n = (int *)( context->context );
	
	require_action( dispatch_get_current_queue() == gDispatchLiteTestQueue, exit, gDispatchLiteTestErr = -1 );
	
	*n += 1;
	gcd_ulog( kLogLevelNotice, "\t%s: n = %d\n", __ROUTINE__, *n );
	if( *n == 1 )
	{
		gDispatchLiteTestDone = true;
	}
	else
	{
		gcd_ulog( kLogLevelNotice, "### Duplicate dispatch_after callback\n" );
		gDispatchLiteTestErr = kDuplicateErr;
	}
	
exit:
	return;
}

#if( DISPATCH_LITE_CF_ENABLED )
void	DispatchLite_TestCF( void *inContext )
{
	int		x;
	
	check( dispatch_get_current_queue() == gDispatchLiteTestQueue );
	
	x = (int)(intptr_t) inContext;
	gcd_ulog( kLogLevelNotice, "\t%s: x = %d\n", __ROUTINE__, x );
	
	gDispatchLiteTest1 += x;
	if( x == 2345 )
	{
		CFRunLoopStop( CFRunLoopGetCurrent() );
	}
}
#endif

#if( TARGET_OS_WINDOWS )
void	DispatchLite_TestWindowsMessageLoop( void *inContext )
{
	int		x;
	
	check( dispatch_get_current_queue() == gDispatchLiteTestQueue );
	
	x = (int)(intptr_t) inContext;
	gcd_ulog( kLogLevelNotice, "\t%s: x = %d\n", __ROUTINE__, x );
	
	gDispatchLiteTest1 += x;
	if( x == 2345 )
	{
		gDispatchLiteTestDone = true;
	}
}
#endif

void	DispatchLiteTest_WaitUntilDone( void )
{
	int		timeout;
	
	timeout = 500;
	while( !gDispatchLiteTestDone && ( timeout > 0 ) )
	{
		usleep( 10000 );
		--timeout;
	}
	check( gDispatchLiteTestDone );
	gDispatchLiteTestDone = false;
}

void	DispatchLiteTest_WaitUntilSourceDone( void )
{
	int		timeout;
	
	timeout = 500;
	while( !gDispatchLiteSourceDone && ( timeout > 0 ) )
	{
		usleep( 10000 );
		--timeout;
	}
	check( gDispatchLiteSourceDone );
	gDispatchLiteSourceDone = false;
}

#endif // !EXCLUDE_UNIT_TESTS
#endif // DISPATCH_LITE_ENABLED
