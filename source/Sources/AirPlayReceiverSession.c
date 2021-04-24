/*
	File:    	AirPlayReceiverSession.c
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	320.17
	
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
	
	Copyright (C) 2005-2016 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if( !defined( _CRT_SECURE_NO_DEPRECATE ) )
	#define _CRT_SECURE_NO_DEPRECATE		1
#endif

#if 0
#pragma mark == Includes ==
#endif

#include "AirPlayReceiverSession.h"
#include "AirPlayReceiverSessionPriv.h"

#include "AudioConverter.h"

#include "AESUtils.h"
#include "CFUtils.h"
#include "CFLiteBinaryPlist.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "HTTPClient.h"
#include "HTTPUtils.h"
#include "MathUtils.h"
#include "NetTransportChaCha20Poly1305.h"
#include "NetUtils.h"
#include "RandomNumberUtils.h"
#include "StringUtils.h"
#include "TickUtils.h"
#include "TimeUtils.h"
#include "UUIDUtils.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverServerPriv.h"
#include "AirPlayUtils.h"
#include "AirTunesClock.h"

#include CF_HEADER

#if( TARGET_OS_POSIX )
	#include <sys/types.h>
	
	#include <arpa/inet.h>
	#include <netinet/in.h>
		#include <netinet/in_systm.h>
		#include <netinet/ip.h>
	#include <netinet/tcp.h>
	#include <pthread.h>
	#include <sched.h>
	#include <sys/socket.h>
	#include <sys/sysctl.h>
#endif

#if 0
#pragma mark == Constants ==
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

	#define kAirPlayInputRingSize				65536	// 371 millieconds at 44100 Hz.
	#define	kAirTunesRTPSocketBufferSize		524288	// 512 KB / 44100 Hz * 2 byte samples * 2 channels + headers ~= 2.8 seconds.
	#define	kAirTunesRTPOffsetResetThreshold	4410	// 100 ms @ 44100 Hz. An absolute diff of more than this resets.
	#define kAirTunesRTPOffsetApplyThreshold	441000	// 10 seconds @ 44100 Hz. If delta > this, do an immediate reset.
	#define kAirTunesBufferNodeCountUDP			512		// 512 nodes * 352 samples per node = ~4 seconds.
	#define kAirTunesRetransmitMaxLoss			128		// Max contiguous loss to try to recover. ~2 second @ 44100 Hz
	#define kAirTunesRetransmitCount			512		// Max number of outstanding retransmits.

	check_compile_time( kAirTunesBufferNodeCountUDP	<= kAirTunesDupWindowSize );
	check_compile_time( kAirTunesRetransmitCount	<= kAirTunesDupWindowSize );

#define kAirPlayEventTimeoutNS	(10ll * kNanosecondsPerSecond) // Timeout in nanosecond for event message
#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

#define	AirTunesFreeBufferNode( SESSION, NODE )				\
	do														\
	{														\
		( NODE )->next->prev	= ( NODE )->prev;			\
		( NODE )->prev->next	= ( NODE )->next;			\
		( NODE )->next			= ( SESSION )->freeList;	\
		( SESSION )->freeList	= ( NODE );					\
		--( SESSION )->busyNodeCount;						\
															\
	}	while( 0 )

#define NanosecondsToMilliseconds32( NANOS )	\
	( (uint32_t)( ( (NANOS) == UINT64_MAX ) ? 0 : ( (NANOS) / kNanosecondsPerMillisecond ) ) )

// General

static void		_GetTypeID( void *inContext );
static void		_Finalize( CFTypeRef inCF );
static void		_EventReplyTimeoutCallback( void *inContext );
static void		_PerformPeriodTasks( void *inContext );
	static OSStatus	_SessionLock( AirPlayReceiverSessionRef inSession );
	static OSStatus	_SessionUnlock( AirPlayReceiverSessionRef inSession );
static OSStatus	_UpdateFeedback( AirPlayReceiverSessionRef inSession, CFDictionaryRef inInput, CFDictionaryRef *outOutput );

// Control/Events

static OSStatus
	_ControlSetup( 
		AirPlayReceiverSessionRef	inSession, 
		CFDictionaryRef				inRequestParams, 
		CFMutableDictionaryRef		inResponseParams );
static OSStatus _ControlIdleStateTransition( AirPlayReceiverSessionRef inSession, CFMutableDictionaryRef inResponseParams );
static void		_ControlTearDown( AirPlayReceiverSessionRef inSession );
static OSStatus	_ControlStart( AirPlayReceiverSessionRef inSession );

// GeneralAudio

static OSStatus
	_GeneralAudioSetup(
		AirPlayReceiverSessionRef	inSession,
		AirPlayStreamType			inStreamType,
		CFDictionaryRef				inStreamDesc,
		CFMutableDictionaryRef		inResponseParams );
static void *	_GeneralAudioThread( void *inArg );
static OSStatus	_GeneralAudioReceiveRTCP( AirPlayReceiverSessionRef inSession, SocketRef inSock, RTCPType inExpectedType );
static OSStatus	_GeneralAudioReceiveRTP( AirPlayReceiverSessionRef inSession, RTPPacket *inPkt, size_t inSize );
static OSStatus
	_GeneralAudioProcessPacket( 
		AirPlayReceiverSessionRef	inSession, 
		AirTunesBufferNode *		inNode, 
		size_t 						inSize, 
		Boolean						inIsRetransmit );
static OSStatus
	_GeneralAudioDecodePacket( 
		AirPlayReceiverSessionRef	inSession, 
		uint8_t *					inAAD, 
		size_t						inAADSize,
		uint8_t *					inSrc, 
		size_t						inSrcSize, 
		uint8_t *					inDst, 
		size_t						inDstSize, 
		size_t *					outSize );
static void		_GeneralAudioRender( AirPlayReceiverSessionRef inSession, uint32_t inRTPTime, void *inBuffer, size_t inSize );
static Boolean	_GeneralAudioTrackDups( AirPlayReceiverSessionRef inSession, uint16_t inSeq );
static void		_GeneralAudioTrackLosses( AirPlayReceiverSessionRef inSession, AirTunesBufferNode *inNode );
static void		_GeneralAudioUpdateLatency( AirPlayReceiverSessionRef inSession );

static OSStatus	_RetransmitsSendRequest( AirPlayReceiverSessionRef inSession, uint16_t inSeqStart, uint16_t inSeqCount );
static OSStatus	_RetransmitsProcessResponse( AirPlayReceiverSessionRef inSession, RTCPRetransmitResponsePacket *inPkt, size_t inSize );
static void		_RetransmitsSchedule( AirPlayReceiverSessionRef inSession, uint16_t inSeqStart, uint16_t inSeqCount );
static void		_RetransmitsUpdate( AirPlayReceiverSessionRef inSession, AirTunesBufferNode *inNode, Boolean inIsRetransmit );
static void		_RetransmitsAbortAll( AirPlayReceiverSessionRef inSession, const char *inReason );
static void		_RetransmitsAbortOne( AirPlayReceiverSessionRef inSession, uint16_t inSeq, const char *inReason );

// MainAudio

static OSStatus
	_MainAltAudioSetup(
		AirPlayReceiverSessionRef	inSession, 
		AirPlayStreamType			inType, 
		CFDictionaryRef				inRequestStreamDesc, 
		CFMutableDictionaryRef		inResponseParams );
static void *	_MainAltAudioThread( void *inArg );
static void		_MainAltAudioProcessPacket( AirPlayAudioStreamContext * const ctx );
static OSStatus	_MainAltAudioGetAADFromRTPHeader( AirPlayAudioStreamContext * const ctx, RTPHeader *inRTPHeaderPtr, uint8_t **outAAD, size_t *outAADLength );

// Timing

static OSStatus	_TimingInitialize( AirPlayReceiverSessionRef inSession );
static OSStatus	_TimingFinalize( AirPlayReceiverSessionRef inSession );
static OSStatus	_TimingNegotiate( AirPlayReceiverSessionRef inSession );
static void *	_TimingThread( void *inArg );
static OSStatus	_TimingSendRequest( AirPlayReceiverSessionRef inSession );
static OSStatus	_TimingReceiveResponse( AirPlayReceiverSessionRef inSession, SocketRef inSock );
static OSStatus	_TimingProcessResponse( AirPlayReceiverSessionRef inSession, RTCPTimeSyncPacket *inPkt, const AirTunesTime *inTime );

static OSStatus	_IdleStateKeepAliveInitialize( AirPlayReceiverSessionRef inSession );
static OSStatus	_IdleStateKeepAliveStart( AirPlayReceiverSessionRef inSession );
static OSStatus	_IdleStateKeepAliveStop( AirPlayReceiverSessionRef inSession );
static OSStatus	_IdleStateKeepAliveFinalize( AirPlayReceiverSessionRef inSession );
static void *	_IdleStateKeepAliveThread( void *inArg );
static OSStatus	_IdleStateKeepAliveReceiveBeacon( AirPlayReceiverSessionRef inSession, SocketRef inSock );
#define			_UsingIdleStateKeepAlive( ME )	( IsValidSocket( (ME)->keepAliveSock ) )

#define			_UsingScreenOrAudio( ME ) \
						( ( (ME)->mainAudioCtx.type != kAirPlayStreamType_Invalid ) || \
						  ( (ME)->altAudioCtx.type != kAirPlayStreamType_Invalid ) \
						  || ( (ME)->screenInitialized ) )

// Screen

static OSStatus
	_ScreenSetup(
		AirPlayReceiverSessionRef	inSession, 
		CFDictionaryRef				inStreamDesc,
		CFMutableDictionaryRef		inResponseParams );

static void		_ScreenTearDown( AirPlayReceiverSessionRef inSession );
static OSStatus	_ScreenStart( AirPlayReceiverSessionRef inSession );
static void *	_ScreenThread( void *inArg );
static uint64_t _ScreenGetSynchronizedNTPTime( void *inContext );
static uint64_t _ScreenGetUpTicksNearSynchronizedNTPTime( void *inContext, uint64_t inNTPTime );

// Utils

static OSStatus	_AddResponseStream( CFMutableDictionaryRef inResponseParams, CFDictionaryRef inStreamDesc );

static OSStatus	_AudioDecoderInitialize( AirPlayReceiverSessionRef inSession );
static OSStatus
	_AudioDecoderDecodeFrame( 
		AirPlayReceiverSessionRef	inSession, 
		const uint8_t *				inSrcPtr, 
		size_t						inSrcLen, 
		uint8_t *					inDstPtr, 
		size_t						inDstMaxLen, 
		size_t *					outDstLen );
static OSStatus
	_AudioDecoderDecodeCallback(
		AudioConverterRef				inAudioConverter,
		UInt32 *						ioNumberDataPackets,
		AudioBufferList *				ioData,
		AudioStreamPacketDescription **	outDataPacketDescription,
		void *							inUserData );
#define _AudioDecoderConcealLoss( SESSION, PTR, LEN )	memset( (PTR), 0, (LEN) )
static OSStatus
	_AudioEncoderEncodeCallback(
		AudioConverterRef				inAudioConverter,
		UInt32 *						ioNumberDataPackets,
		AudioBufferList *				ioData,
		AudioStreamPacketDescription **	outDataPacketDescription,
		void *							inUserData );

int _CompareOSBuildVersionStrings( const char *inVersion1, const char *inVersion2 );

static OSStatus
	_GetStreamSecurityKeys( 
		AirPlayReceiverSessionRef	inSession,
		uint64_t					streamConnectionID, 
		size_t						inInputKeyLen, 
		uint8_t *					outInputKey,
		size_t						inOutputKeyLen,
		uint8_t *					outOutputKey );

static void	_LogStarted( AirPlayReceiverSessionRef inSession, AirPlayReceiverSessionStartInfo *inInfo, OSStatus inStatus );
static void	_LogEnded( AirPlayReceiverSessionRef inSession, OSStatus inReason );
static void	_LogUpdate( AirPlayReceiverSessionRef inSession, uint64_t inTicks, Boolean inForce );
static void	_TearDownStream( AirPlayReceiverSessionRef inSession, AirPlayAudioStreamContext * const ctx, Boolean inIsFinalizing );
static void	_UpdateEstimatedRate( AirPlayAudioStreamContext *ctx, uint32_t inSampleTime, uint64_t inHostTime );

// Debugging

ulog_define( AirPlayReceiverCore,		kLogLevelNotice, kLogFlags_Default, "AirPlay",  NULL );
#define atr_ucat()						&log_category_from_name( AirPlayReceiverCore )
#define atr_ulog( LEVEL, ... )			ulog( atr_ucat(), (LEVEL), __VA_ARGS__ )

ulog_define( AirPlayReceiverEvents,	kLogLevelNotice, kLogFlags_Default, "AirPlay",  NULL );
#define atr_events_ucat()				&log_category_from_name( AirPlayReceiverEvents )

ulog_define( AirPlayReceiverStats,		kLogLevelNotice, kLogFlags_Default, "AirPlay", "AirPlayReceiverStats:rate=5;3000" );
#define atr_stats_ucat()				&log_category_from_name( AirPlayReceiverStats )
#define atr_stats_ulog( LEVEL, ... )	ulog( atr_stats_ucat(), (LEVEL), __VA_ARGS__ )

#if 0
#pragma mark == Globals ==
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static const CFRuntimeClass		kAirPlayReceiverSessionClass = 
{
	0,							// version
	"AirPlayReceiverSession",	// className
	NULL,						// init
	NULL,						// copy
	_Finalize,					// finalize
	NULL,						// equal -- NULL means pointer equality.
	NULL,						// hash  -- NULL means pointer hash.
	NULL,						// copyFormattingDesc
	NULL,						// copyDebugDesc
	NULL,						// reclaim
	NULL						// refcount
};

static dispatch_once_t		gAirPlayReceiverSessionInitOnce	= 0;
static CFTypeID				gAirPlayReceiverSessionTypeID	= _kCFRuntimeNotATypeID;
static int32_t				gAirTunesRelativeTimeOffset		= 0;		// Custom adjustment to the real offset for fine tuning.

AirPlayReceiverSessionRef	gAirTunes = NULL;
AirPlayAudioStats			gAirPlayAudioStats;

#if 0
#pragma mark == General ==
#endif

//===========================================================================================================================
//	AirPlayReceiverSessionGetTypeID
//===========================================================================================================================

CFTypeID	AirPlayReceiverSessionGetTypeID( void )
{
	dispatch_once_f( &gAirPlayReceiverSessionInitOnce, NULL, _GetTypeID );
	return( gAirPlayReceiverSessionTypeID );
}

//===========================================================================================================================
//	AirPlayReceiverSessionCreate
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionCreate( 
		AirPlayReceiverSessionRef *					outSession, 
		const AirPlayReceiverSessionCreateParams *	inParams )
{
	AirPlayReceiverServerRef const		server = inParams->server;
	OSStatus							err;
	AirPlayReceiverSessionRef			me;
	size_t								extraLen;
	uint64_t							ticksPerSec;
	uint64_t							ticks;
	AirTunesSource *					ats;
	
	ticksPerSec = UpTicksPerSecond();
	ticks = UpTicks();
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (AirPlayReceiverSessionRef) _CFRuntimeCreateInstance( NULL, AirPlayReceiverSessionGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->queue = AirPlayReceiverServerGetDispatchQueue( server );
	dispatch_retain( me->queue );
	
	CFRetain( server );
	me->server = server;
	
	me->connection = inParams->connection;

	// Initialize variables to a good state so we can safely clean up if something fails during init.
	
	me->transportType			= inParams->transportType;
	me->peerAddr				= *inParams->peerAddr;
	UUIDGet( me->sessionUUID );
	me->startStatus				= kNotInitializedErr;
	me->clientDeviceID			= inParams->clientDeviceID;
	me->clientSessionID			= inParams->clientSessionID;
	me->clientVersion			= inParams->clientVersion;
	
	memcpy( me->clientIfMACAddr, inParams->clientIfMACAddr, sizeof( inParams->clientIfMACAddr ) );
	
	me->sessionTicks			= ticks;
	me->sessionIdle				= true;
	me->sessionIdleValid		= false;
	me->useEvents				= inParams->useEvents;
	me->eventSock				= kInvalidSocketRef;
	me->mainAudioCtx.cmdSock	= kInvalidSocketRef;
	me->mainAudioCtx.dataSock	= kInvalidSocketRef;
	me->altAudioCtx.cmdSock		= kInvalidSocketRef;
	me->altAudioCtx.dataSock	= kInvalidSocketRef;
	me->keepAliveSock			= kInvalidSocketRef;
	me->keepAliveCmdSock		= kInvalidSocketRef;
	me->rtcpSock				= kInvalidSocketRef;
	me->timingSock				= kInvalidSocketRef;
	me->timingCmdSock			= kInvalidSocketRef;
	me->screenSock				= kInvalidSocketRef;
	
	me->eventQueue = dispatch_queue_create( "AirPlayReceiverSessionEventQueue", NULL );
	require_action( me->eventQueue, exit, err = kNoMemoryErr );
	
	me->eventReplyTimer = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, me->eventQueue );
	require_action( me->eventReplyTimer, exit, err = kNoMemoryErr );

	dispatch_set_context( me->eventReplyTimer, me );
	dispatch_source_set_event_handler_f( me->eventReplyTimer, _EventReplyTimeoutCallback );
	dispatch_source_set_timer( me->eventReplyTimer, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, kNanosecondsPerSecond );
	dispatch_resume( me->eventReplyTimer );

	err = AirPlayReceiverSessionScreen_Create( &me->screenSession );
	require_noerr( err, exit );
	
	// Finish initialization.
	
	err = pthread_mutex_init( &me->mutex, NULL );
	require_noerr( err, exit );
	me->mutexPtr = &me->mutex;
	
	me->glitchIntervalTicks			= 1 * kSecondsPerMinute * ticksPerSec;
	me->glitchNextTicks				= ticks + me->glitchIntervalTicks;
	
	ats								= &me->source;
	ats->lastActivityTicks			= ticks;
	ats->maxIdleTicks				= server->timeoutDataSecs * ticksPerSec;
	ats->perSecTicks				= 1 * ticksPerSec;
	ats->perSecLastTicks			= 0; // Explicit 0 to note that it's intentional to force an immmediate update.
	ats->lastIdleLogTicks			= ticks;
	ats->idleLogIntervalTicks		= 10 * ticksPerSec;
	
	ats->rtcpTIClockRTTMin			= +1000000.0;
	ats->rtcpTIClockRTTMax			= -1000000.0;
	ats->rtcpTIClockOffsetMin		= +1000000.0;
	ats->rtcpTIClockOffsetMax		= -1000000.0;
	
	if( server->delegate.sessionCreated_f ) server->delegate.sessionCreated_f( server, me, server->delegate.context );
	err = AirPlayReceiverSessionPlatformInitialize( me );
	require_noerr( err, exit );
	if( me->delegate.initialize_f )
	{
		err = me->delegate.initialize_f( me, me->delegate.context );
		require_noerr( err, exit );
	}
	
	*outSession = me;
	me = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( me );
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionGetDispatchQueue
//===========================================================================================================================

dispatch_queue_t	AirPlayReceiverSessionGetDispatchQueue( AirPlayReceiverSessionRef inSession )
{
	return( inSession->queue );
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetDelegate
//===========================================================================================================================

EXPORT_GLOBAL
void	AirPlayReceiverSessionSetDelegate( AirPlayReceiverSessionRef inSession, const AirPlayReceiverSessionDelegate *inDelegate )
{
	inSession->delegate = *inDelegate;
}

//===========================================================================================================================
//	AirPlayReceiverSessionControl
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
	AirPlayReceiverSessionControl( 
		CFTypeRef			inSession, 
		uint32_t			inFlags, 
		CFStringRef			inCommand, 
		CFTypeRef			inQualifier, 
		CFDictionaryRef		inParams, 
		CFDictionaryRef *	outParams )
{
	AirPlayReceiverSessionRef const		session = (AirPlayReceiverSessionRef) inSession;
	OSStatus							err;
	
	if( 0 ) {}
		
	// ModesChanged
	
	else if( session->delegate.modesChanged_f && CFEqual( inCommand, CFSTR( kAirPlayCommand_ModesChanged ) ) )
	{
		AirPlayModeState		modeState;
		
		require_action( inParams, exit, err = kParamErr );
		err = AirPlayReceiverSessionMakeModeStateFromDictionary( session, inParams, &modeState );
		require_noerr( err, exit );
		
		atr_ulog( kLogLevelNotice, "Modes changed: screen %s, mainAudio %s, speech %s (%s), phone %s, turns %s\n", 
			AirPlayEntityToString( modeState.screen ), AirPlayEntityToString( modeState.mainAudio ), 
			AirPlayEntityToString( modeState.speech.entity ), AirPlaySpeechModeToString( modeState.speech.mode ), 
			AirPlayEntityToString( modeState.phoneCall ), AirPlayEntityToString( modeState.turnByTurn ) );
		session->delegate.modesChanged_f( session, &modeState, session->delegate.context );
	}
	
	// RequestUI
	
	else if( session->delegate.requestUI_f && CFEqual( inCommand, CFSTR( kAirPlayCommand_RequestUI ) ) )
	{
		CFStringRef		url;
		
		if( inParams )	url = (CFStringRef) CFDictionaryGetValue( inParams, CFSTR( kAirPlayKey_URL ) );
		else			url = NULL;
		
		atr_ulog( kLogLevelNotice, "Request accessory UI\n" );
		session->delegate.requestUI_f( session, url, session->delegate.context );
	}
	
	// UpdateFeedback
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_UpdateFeedback ) ) )
	{
		err = _UpdateFeedback( session, inParams, outParams );
		require_noerr( err, exit );
	}
	
	// SetHIDInputMode
	
	else if( session->delegate.control_f && CFEqual( inCommand, CFSTR( kAirPlayCommand_HIDSetInputMode ) ) )
	{
		atr_ulog( kLogLevelNotice, "Set HIDInputMode\n" );
		session->delegate.control_f( session, inCommand, inQualifier, inParams, outParams, session->delegate.context );
	}
	
	// iAPSendMessage
	
	else if( session->delegate.control_f && CFEqual( inCommand, CFSTR( kAirPlayCommand_iAPSendMessage ) ) )
	{
		atr_ulog( kLogLevelTrace, "iAP Send Message\n" );
		session->delegate.control_f( session, inCommand, inQualifier, inParams, outParams, session->delegate.context );
	}
	
	// Other
	
	else
	{
		err = AirPlayReceiverSessionPlatformControl( session, inFlags, inCommand, inQualifier, inParams, outParams );
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionCopyProperty
//===========================================================================================================================

EXPORT_GLOBAL
CFTypeRef
	AirPlayReceiverSessionCopyProperty( 
		CFTypeRef	inSession, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		OSStatus *	outErr )
{
	AirPlayReceiverSessionRef const		session = (AirPlayReceiverSessionRef) inSession;
	OSStatus							err;
	CFTypeRef							value = NULL;
	
	(void) inFlags;
	(void) inQualifier;
	
	if( 0 ) {}
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_TransportType ) ) )
	{
		value = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, (int32_t*) &session->transportType );
		err = value ? kNoErr : kNoMemoryErr;
		goto exit;
	}
	
	// Unknown
	
	else
	{
		value = AirPlayReceiverSessionPlatformCopyProperty( session, inFlags, inProperty, inQualifier, &err );
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetProperty
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
	AirPlayReceiverSessionSetProperty( 
		CFTypeRef	inSession, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		CFTypeRef	inValue )
{
	AirPlayReceiverSessionRef const		session = (AirPlayReceiverSessionRef) inSession;
	OSStatus							err;
	
	if( 0 ) {}
	
	// TimelineOffset
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_TimelineOffset ) ) )
	{
		int32_t		offset;
		
		offset = (int32_t) CFGetInt64( inValue, NULL );
		require_action( ( offset >= -250 ) && ( offset <= 250 ), exit, err = kRangeErr );
		gAirTunesRelativeTimeOffset = offset;
	}
	
	// Other
	
	else
	{
		err = AirPlayReceiverSessionPlatformSetProperty( session, inFlags, inProperty, inQualifier, inValue );
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetHomeKitSecurityContext
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionSetHomeKitSecurityContext( 
		AirPlayReceiverSessionRef	inSession, 
		PairingSessionRef			inHKPairVerifySession )
{
	ReplaceCF( &inSession->pairVerifySession, inHKPairVerifySession );
	return( kNoErr );
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetSecurityInfo
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionSetSecurityInfo( 
		AirPlayReceiverSessionRef	inSession, 
		const uint8_t				inKey[ 16 ], 
		const uint8_t				inIV[ 16 ] )
{
	OSStatus		err;
	
	AES_CBCFrame_Final( &inSession->decryptorStorage );
	inSession->decryptor = NULL;
	
	err = AES_CBCFrame_Init( &inSession->decryptorStorage, inKey, inIV, false );
	require_noerr( err, exit );
	inSession->decryptor = &inSession->decryptorStorage;
	
	memcpy( inSession->aesSessionKey, inKey, sizeof( inSession->aesSessionKey ) );
	memcpy( inSession->aesSessionIV, inIV, sizeof( inSession->aesSessionIV ) );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_EventReplyTimeoutCallback
//===========================================================================================================================

static void	_EventReplyTimeoutCallback( void *inContext )
{
	AirPlayReceiverSessionRef const		me  = (AirPlayReceiverSessionRef) inContext;
	sockaddr_ip							sip;
	size_t								len;
	SocketRef							sock = kInvalidSocketRef;
	const int							connectTimeoutSec = 20;
	OSStatus							err;

	require_action_quiet( me->eventPendingMessageCount > 0, exit, err = kNoErr );

	// connect to the sender event socket
	err = HTTPClientGetPeerAddress( me->eventClient, &sip, sizeof( sip ), &len );
	require_noerr( err, exit );

	sock = socket( sip.sa.sa_family, SOCK_STREAM, IPPROTO_TCP );
	require_action( IsValidSocket( sock ), exit, err = kUnknownErr );

	atr_ulog( kLogLevelWarning, "### Waking up device\n" );
	err = SocketConnect( sock, &sip, connectTimeoutSec );
	require( ( err == ECONNREFUSED ), exit );
	err = kNoErr;

exit:
	check_noerr( err );
	ForgetSocket( &sock );
}

//===========================================================================================================================
//	AirPlayReceiverSessionSendCommand
//===========================================================================================================================

static void	_AirPlayReceiverSessionSendCommandCompletion( HTTPMessageRef inMsg );

typedef struct
{
	AirPlayReceiverSessionRef						session;
	CFDictionaryRef									request;
	AirPlayReceiverSessionCommandCompletionFunc		completion;
	void *											completionContext;
	OSStatus										err;
}
SendCommandContext;

static void _AirPlayReceiverSessionSendCommand( void * inCtx )
{
	SendCommandContext *		context;
	OSStatus					err;
	HTTPMessageRef				msg = NULL;
	const void *				ptr;
	size_t						len;
	
	context = (SendCommandContext*) inCtx;
	
	require_action_quiet( context->session->eventClient, exit, err = kUnsupportedErr );
	require_action_quiet( context->session->eventReplyTimer, exit, err = kStateErr );
	
	err = HTTPMessageCreate( &msg );
	require_noerr( err, exit );
	
	err = HTTPHeader_InitRequest( &msg->header, "POST", kAirPlayCommandPath, kAirTunesHTTPVersionStr );
	require_noerr( err, exit );
	
	ptr = CFBinaryPlistV0Create( context->request, &len, NULL );
	require_action( ptr, exit, err = kUnknownErr );
	err = HTTPMessageSetBodyPtr( msg, kMIMEType_AppleBinaryPlist, ptr, len );
	require_noerr( err, exit );
	
	if( context->completion )
	{
		CFRetain( context->session );
		msg->userContext1 = context->session;
		msg->userContext2 = (void *)(uintptr_t) context->completion;
		msg->userContext3 = context->completionContext;
		msg->completion   = _AirPlayReceiverSessionSendCommandCompletion;
	}
	
	err = HTTPClientSendMessage( context->session->eventClient, msg );
	if( err && context->completion ) CFRelease( context->session );
	require_noerr( err, exit );

	if( context->completion )
	{
		if( 0 == context->session->eventPendingMessageCount )
		{
			dispatch_source_set_timer( context->session->eventReplyTimer, dispatch_time( DISPATCH_TIME_NOW, kAirPlayEventTimeoutNS ), DISPATCH_TIME_FOREVER, kNanosecondsPerSecond );
		}
		context->session->eventPendingMessageCount++;
	}

exit:
	CFReleaseNullSafe( msg );
	context->err = err;
}

EXPORT_GLOBAL
OSStatus
	AirPlayReceiverSessionSendCommand( 
		AirPlayReceiverSessionRef						inSession,
		CFDictionaryRef									inRequest,
		AirPlayReceiverSessionCommandCompletionFunc		inCompletion,
		void *											inContext )
{
	OSStatus				err;
	SendCommandContext		context;

	require_action_quiet( inSession->sessionStarted, exit, err = kStateErr );
	
	context.session = inSession;
	context.request = inRequest;
	context.completion = inCompletion;
	context.completionContext = inContext;
	
	dispatch_sync_f( inSession->eventQueue, &context, _AirPlayReceiverSessionSendCommand );
	
	err = context.err;
	
exit:
	return( err );
}

static void	_AirPlayReceiverSessionSendCommandCompletion( HTTPMessageRef inMsg )
{
	AirPlayReceiverSessionRef const							session		= (AirPlayReceiverSessionRef) inMsg->userContext1;
	AirPlayReceiverSessionCommandCompletionFunc const		completion	= (AirPlayReceiverSessionCommandCompletionFunc)(uintptr_t) inMsg->userContext2;
	OSStatus												err = kNoErr;
	CFDictionaryRef											response = NULL;
	
	require_action_quiet( session->eventReplyTimer, exit, err = kStateErr );
	
	session->eventPendingMessageCount--;
	if( 0 < session->eventPendingMessageCount )
	{
		dispatch_source_set_timer( session->eventReplyTimer, dispatch_time( DISPATCH_TIME_NOW, kAirPlayEventTimeoutNS ), DISPATCH_TIME_FOREVER, kNanosecondsPerSecond );
	}
	else
	{
		dispatch_source_set_timer( session->eventReplyTimer, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, kNanosecondsPerSecond );
	}

	if( inMsg->bodyLen > 0 ) {
		response = (CFDictionaryRef)CFBinaryPlistV0CreateWithData( inMsg->bodyPtr, inMsg->bodyLen, &err );
		require_noerr( err, exit );
		require_action( CFGetTypeID( response ) == CFDictionaryGetTypeID(), exit, err = kTypeErr );
		
		err = (OSStatus) CFDictionaryGetInt64( response, CFSTR( kAirPlayKey_Status ), NULL );
	}
	
exit:
	if( completion ) completion( err, err ? NULL : response, inMsg->userContext3 );
	CFRelease( session );
	CFReleaseNullSafe( response );
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetup
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionSetup( 
		AirPlayReceiverSessionRef	me,
		CFDictionaryRef				inRequestParams, 
		CFDictionaryRef *			outResponseParams )
{
	Boolean						general = false;
	OSStatus					err;
	CFMutableDictionaryRef		responseParams;
	CFArrayRef					requestStreams;
	CFDictionaryRef				requestStreamDesc;
	CFIndex						streamIndex, streamCount;
	char						clientOSBuildVersion[ 32 ], minClientOSBuildVersion[ 32 ];
	AirPlayStreamType			type;
	
	atr_ulog( kLogLevelTrace, "Setting up session %llu with %##a %?@\n", 
		me->clientSessionID, &me->peerAddr, log_category_enabled( atr_ucat(), kLogLevelVerbose ), inRequestParams );
	
	responseParams = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( responseParams, exit, err = kNoMemoryErr );
	
	if( !me->controlSetup )
	{
		err = _ControlSetup( me, inRequestParams, responseParams );
		require_noerr( err, exit );
	}
	
	// Perform minimum version check
	
	CFDictionaryGetCString( inRequestParams, CFSTR( kAirPlayKey_OSBuildVersion ), clientOSBuildVersion, sizeof( clientOSBuildVersion ), &err );
	if( !err )
	{
		strlcpy( me->clientOSBuildVersion, clientOSBuildVersion, sizeof( me->clientOSBuildVersion ) );
		err = AirPlayGetMinimumClientOSBuildVersion( me->server, minClientOSBuildVersion, sizeof( minClientOSBuildVersion ) );
		if( !err )
		{
			if( _CompareOSBuildVersionStrings( minClientOSBuildVersion, clientOSBuildVersion ) > 0 )
				require_noerr( err = kVersionErr, exit );
		}
	}
	
	// Set up each stream.
	
	requestStreams = CFDictionaryGetCFArray( inRequestParams, CFSTR( kAirPlayKey_Streams ), &err );
	streamCount = requestStreams ? CFArrayGetCount( requestStreams ) : 0;
	for( streamIndex = 0; streamIndex < streamCount; ++streamIndex )
	{
		requestStreamDesc = CFArrayGetCFDictionaryAtIndex( requestStreams, streamIndex, &err );
		require_noerr( err, exit );
		
		type = (AirPlayStreamType) CFDictionaryGetInt64( requestStreamDesc, CFSTR( kAirPlayKey_Type ), NULL );
		switch( type )
		{

			case kAirPlayStreamType_MainHighAudio:
				err = _GeneralAudioSetup( me, type, requestStreamDesc, responseParams );
				require_noerr( err, exit );
				general = true;
				break;

			case kAirPlayStreamType_MainAudio:
			case kAirPlayStreamType_AltAudio:
				err = _MainAltAudioSetup( me, type, requestStreamDesc, responseParams );
				require_noerr( err, exit );
				break;
			
			case kAirPlayStreamType_Screen:
				err = _ScreenSetup( me, requestStreamDesc, responseParams );
				require_noerr( err, exit );
				break;
			
			default:
				atr_ulog( kLogLevelNotice, "### Unsupported stream type: %d\n", type );
				break;
		}
	}
	
	if( streamCount > 0 || !me->sessionIdleValid )
	{
		err = _ControlIdleStateTransition( me, responseParams );
		require_noerr( err, exit );
	}

	// Set up the platform.
	
	err = AirPlayReceiverSessionPlatformControl( me, kCFObjectFlagDirect, CFSTR( kAirPlayCommand_SetUpStreams ), NULL, 
		inRequestParams, NULL );
	require_noerr( err, exit );
	
	if( general )
	{
		_GeneralAudioUpdateLatency( me );
		
	}
	
	*outResponseParams = responseParams;
	responseParams = NULL;
	gAirTunes = me;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( responseParams );
	if( err )
	{
		atr_ulog( kLogLevelNotice, "### Set up session %llu with %##a failed: %#m %@\n", 
			me->clientSessionID, &me->peerAddr, err, inRequestParams );

		if( me->server->delegate.sessionFailed_f )
			me->server->delegate.sessionFailed_f( me->server, err, me->server->delegate.context );

		AirPlayReceiverSessionTearDown( me, inRequestParams, err, NULL );
	}
	return( err );
}

//===========================================================================================================================
//	_ReplyTimerTearDown
//===========================================================================================================================

static void _ForgetReplyTimer( void * inCtx )
{
	AirPlayReceiverSessionRef session = (AirPlayReceiverSessionRef) inCtx;
	dispatch_source_forget( &session->eventReplyTimer );
}

static void _ReplyTimerTearDown( AirPlayReceiverSessionRef inSession )
{
	dispatch_sync_f( inSession->eventQueue, inSession, _ForgetReplyTimer );
}

//===========================================================================================================================
//	AirPlayReceiverSessionTearDown
//===========================================================================================================================

void
	AirPlayReceiverSessionTearDown( 
		AirPlayReceiverSessionRef	inSession, 
		CFDictionaryRef				inParams, 
		OSStatus					inReason, 
		Boolean *					outDone )
{
	OSStatus				err;
	CFArrayRef				streams;
	CFIndex					streamIndex, streamCount;
	CFDictionaryRef			streamDesc;
	AirPlayStreamType		streamType;
	
	atr_ulog( kLogLevelTrace, "Tearing down session %llu with %##a %?@\n", 
		inSession->clientSessionID, &inSession->peerAddr, log_category_enabled( atr_ucat(), kLogLevelVerbose ), inParams );
	
	AirPlayReceiverSessionPlatformControl( inSession, kCFObjectFlagDirect, CFSTR( kAirPlayCommand_TearDownStreams ), NULL, 
		inParams, NULL );
	
	streams = inParams ? CFDictionaryGetCFArray( inParams, CFSTR( kAirPlayKey_Streams ), NULL ) : NULL;
	streamCount = streams ? CFArrayGetCount( streams ) : 0;
	for( streamIndex = 0; streamIndex < streamCount; ++streamIndex )
	{
		streamDesc = CFArrayGetCFDictionaryAtIndex( streams, streamIndex, &err );
		require_noerr( err, exit );
		
		streamType = (AirPlayStreamType) CFDictionaryGetInt64( streamDesc, CFSTR( kAirPlayKey_Type ), NULL );
		
		atr_ulog( kLogLevelTrace, "Tearing down stream type %d\n", streamType );

		switch( streamType )
		{
			case kAirPlayStreamType_MainAudio:
			case kAirPlayStreamType_MainHighAudio:
				_TearDownStream( inSession, &inSession->mainAudioCtx, false );
				break;
			
			case kAirPlayStreamType_AltAudio:
				_TearDownStream( inSession, &inSession->altAudioCtx, false );
				break;
			
			case kAirPlayStreamType_Screen:
				_ScreenTearDown( inSession );
				break;
			
			default:
				atr_ulog( kLogLevelNotice, "### Unsupported stream type: %d\n", streamType );
				break;
		}
	}

	if( streamCount > 0 )
	{
		err = _ControlIdleStateTransition( inSession, NULL );
		require_noerr( err, exit );

		goto exit;
	}

	_LogEnded( inSession, inReason );
	gAirTunes = NULL;
	
	if( inSession->inputRingRef )
	{
		MirroredRingBufferFree( inSession->inputRingRef );
		inSession->inputRingRef = NULL;
	}
	dispatch_source_forget( &inSession->periodicTimer );
	_ScreenTearDown( inSession );
	_TearDownStream( inSession, &inSession->altAudioCtx, false );
	_TearDownStream( inSession, &inSession->mainAudioCtx, false );
	_ReplyTimerTearDown( inSession );
	_ControlTearDown( inSession );
	_TimingFinalize( inSession );
	AirTunesClock_Finalize( inSession->airTunesClock );
	inSession->airTunesClock = NULL;
	
exit:
	if( outDone ) *outDone = ( streamCount == 0 );
}

//===========================================================================================================================
//	AirPlayReceiverSessionStart
//===========================================================================================================================

OSStatus	
	AirPlayReceiverSessionStart( 
		AirPlayReceiverSessionRef inSession, 
		AirPlayReceiverSessionStartInfo *inInfo )
{
	OSStatus						err;
	AirPlayAudioStreamContext *		ctx;
	dispatch_source_t				source;
	uint64_t						nanos;
	
	inSession->playTicks = UpTicks();
	
	if( IsValidSocket( inSession->eventSock ) )
	{
		err = _ControlStart( inSession );
		require_noerr( err, exit );
	}
	
	ctx = &inSession->mainAudioCtx;

	if( ( ctx->type == kAirPlayStreamType_MainHighAudio ) && !ctx->threadPtr )
	{
		err = pthread_create( &ctx->thread, NULL, _GeneralAudioThread, inSession );
		require_noerr( err, exit );
		ctx->threadPtr = &ctx->thread;
	}
	
	if( ( ctx->type == kAirPlayStreamType_MainAudio ) && !ctx->threadPtr )
	{
		err = pthread_create( &ctx->thread, NULL, _MainAltAudioThread, ctx );
		require_noerr( err, exit );
		ctx->threadPtr = &ctx->thread;
	}
	
	ctx = &inSession->altAudioCtx;
	if( ( ctx->type == kAirPlayStreamType_AltAudio ) && !ctx->threadPtr )
	{
		err = pthread_create( &ctx->thread, NULL, _MainAltAudioThread, ctx );
		require_noerr( err, exit );
		ctx->threadPtr = &ctx->thread;
	}
	
	if( IsValidSocket( inSession->screenSock ) )
	{
		err = _ScreenStart( inSession );
		require_noerr( err, exit );
	}

	err = AirPlayReceiverSessionPlatformControl( inSession, kCFObjectFlagDirect, CFSTR( kAirPlayCommand_StartSession ), 
		NULL, NULL, NULL );
	require_noerr_quiet( err, exit );
	inSession->sessionStarted = true;
	
	if( inSession->delegate.started_f )
	{
		inSession->delegate.started_f( inSession, inSession->delegate.context );
	}
	
	// Start a timer to service things periodically.
	
	inSession->source.lastIdleLogTicks = inSession->playTicks;
	inSession->periodicTimer = source = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, inSession->queue );
	require_action( source, exit, err = kUnknownErr );
	dispatch_set_context( source, inSession );
	dispatch_source_set_event_handler_f( source, _PerformPeriodTasks );
	nanos = 250 * kNanosecondsPerMillisecond;
	dispatch_source_set_timer( source, dispatch_time( DISPATCH_TIME_NOW, nanos ), nanos, nanos );
	dispatch_resume( source );
	
exit:
	_LogStarted( inSession, inInfo, err );
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionFlushAudio
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionFlushAudio( 
		AirPlayReceiverSessionRef	inSession, 
		uint32_t					inFlushUntilTS, 
		uint16_t					inFlushUntilSeq, 
		uint32_t *					outLastTS )
{
	AirTunesSource * const					ats = &inSession->source;
	AirPlayAudioStreamContext * const		ctx = &inSession->mainAudioCtx;
	AirTunesBufferNode *					curr;
	AirTunesBufferNode *					stop;
	AirTunesBufferNode *					next;
	OSStatus								err;
	
	DEBUG_USE_ONLY( err );
	
	require_action_quiet( inSession->busyListSentinel, exit, atr_ulog( kLogLevelNotice, "### Not playing audio - nothing to flush\n" ) );
	
	atr_ulog( kLogLevelVerbose, "Flushing until ts %u seq %u\n", inFlushUntilTS, inFlushUntilSeq );
	
	_SessionLock( inSession );
	
	// Reset state so we don't play until we get post-flush timelines, etc.
	
	inSession->flushing				= true;
	inSession->flushTimeoutTS		= inFlushUntilTS + ( 3 * ctx->sampleRate ); // 3 seconds.
	inSession->flushUntilTS			= inFlushUntilTS;
	inSession->lastPlayedValid		= false;
	ats->rtcpRTDisable				= inSession->redundantAudio;
	ats->receiveCount				= 0; // Reset so we don't try to retransmit on the next post-flush packet.
	
	// Drop packets in the queue that are earlier than the flush timestamp and abort any pending retransmits.
	
	stop = inSession->busyListSentinel;
	for( curr = stop->next; curr != stop; curr = next )
	{
		next = curr->next;
		
		if( Mod32_LT( curr->rtp->header.ts, inFlushUntilTS ) || Mod16_LT( curr->rtp->header.seq, inFlushUntilSeq ) )
		{
			AirTunesFreeBufferNode( inSession, curr );
			continue;
		}
		break;
	}
	if( curr != stop )
	{
		atr_ulog( kLogLevelInfo, "Packets still present after flush (first ts %u)\n", curr->rtp->header.ts );
	}
	_RetransmitsAbortAll( inSession, "flush" );
	
	if( inSession->audioConverter )
	{
		err = AudioConverterReset( inSession->audioConverter );
		check_noerr( err );
	}
	
	if( outLastTS ) *outLastTS = inSession->lastPlayedTS;
	
	_SessionUnlock( inSession );

exit:
	return( kNoErr );
}

//===========================================================================================================================
//	AirPlayReceiverSessionReadAudio
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionReadAudio( 
		AirPlayReceiverSessionRef	inSession, 
		AirPlayStreamType			inType, 
		uint32_t					inSampleTime, 
		uint64_t					inHostTime, 
		void *						inBuffer, 
		size_t						inLen )
{
	OSStatus		err;
	
	switch( inType )
	{
		
		case kAirPlayStreamType_MainAudio:
			_UpdateEstimatedRate( &inSession->mainAudioCtx, inSampleTime, inHostTime );
			err = RTPJitterBufferRead( &inSession->mainAudioCtx.jitterBuffer, inBuffer, inLen );
			require_noerr( err, exit );
			break;

		case kAirPlayStreamType_MainHighAudio:
			_UpdateEstimatedRate( &inSession->mainAudioCtx, inSampleTime, inHostTime );
			_SessionLock( inSession );
			_GeneralAudioRender( inSession, inSampleTime, inBuffer, inLen );
			_SessionUnlock( inSession );
			err = kNoErr;
			break;
			
		case kAirPlayStreamType_AltAudio:
			_UpdateEstimatedRate( &inSession->altAudioCtx, inSampleTime, inHostTime );
			err = RTPJitterBufferRead( &inSession->altAudioCtx.jitterBuffer, inBuffer, inLen );
			require_noerr( err, exit );
			break;
		
		default:
			dlogassert( "Bad stream type: %u", inType );
			err = kParamErr;
			goto exit;
	}
	
exit:
	return( err );
}

static size_t _AirPlayReceiver_EncryptAudio(
    AirPlayAudioStreamContext * const ctx,
    RTPSavedPacket            *inPkt,
    size_t                     inPayloadSize)
{
    size_t	additionalPayload = 0;

    if( ctx->inputCryptor.isValid )
    {
        size_t		len			= 0;
        uint8_t *	aad			= NULL;
        size_t		aadLength	= 0;

        _MainAltAudioGetAADFromRTPHeader( ctx, &inPkt->pkt.rtp.header, &aad, &aadLength );

        // Encrypt and authenticate the payload
        // The last 24 bytes of the payload are nonce and the auth tag. Both are LE. The rest of the payload is BE.

        chacha20_poly1305_init_64x64( &ctx->inputCryptor.state, ctx->inputCryptor.key, ctx->inputCryptor.nonce );
        chacha20_poly1305_add_aad( &ctx->inputCryptor.state, aad, aadLength );
        len = chacha20_poly1305_encrypt( &ctx->inputCryptor.state, inPkt->pkt.rtp.payload, inPayloadSize, inPkt->pkt.rtp.payload );
        len += chacha20_poly1305_final( &ctx->inputCryptor.state, &inPkt->pkt.rtp.payload[ len ], &inPkt->pkt.rtp.payload[ inPayloadSize ] );

        // Add nonce and increment it

        memcpy( &inPkt->pkt.rtp.payload[ inPayloadSize + 16 ], ctx->inputCryptor.nonce, sizeof( ctx->inputCryptor.nonce ) );
        LittleEndianIntegerIncrement( ctx->inputCryptor.nonce, sizeof( ctx->inputCryptor.nonce ) );

        additionalPayload = 24;
    }
    return( additionalPayload );
}

static void  _AirPlayReceiver_SendAudio( AirPlayAudioStreamContext * const ctx )
{
    RTPSavedPacket							pkt;
    size_t									avail, len;
    size_t									maxPayloadSize;
    MirroredRingBuffer * const				ring    = ctx->inputRingRef;
    uint16_t								seq		= ctx->inputSeqNum;
    uint32_t								ts		= ctx->inputTimestamp;
    uint32_t								spp;
    ssize_t									n;
    OSStatus								err;
	
    if( ctx->inputCryptor.isValid )
    {
        maxPayloadSize = kAirTunesMaxPayloadSizeUDP - 24; // 16 bytes for auth tag and 8 - for nonce
    }
    else
    {
        maxPayloadSize = kAirTunesMaxPayloadSizeUDP;
    }

    pkt.pkt.rtp.header.v_p_x_cc	= RTPHeaderInsertVersion( 0, kRTPVersion );
    pkt.pkt.rtp.header.m_pt		= RTPHeaderInsertPayloadType( 0, ctx->type );
    pkt.pkt.rtp.header.ssrc		= 0;
    for( ;; )
    {
        avail = MirroredRingBufferGetBytesUsed( ring );
        if( avail > maxPayloadSize ) avail = maxPayloadSize;
        spp = (uint32_t)( avail / ctx->bytesPerUnit );
        if( spp == 0 ) break;
        
        pkt.pkt.rtp.header.seq	= htons( seq );
        pkt.pkt.rtp.header.ts	= htonl( ts );
        
        if( ctx->inputConverter )
        {
			const uint8_t *						src;
			UInt32								packetCount;
			AudioBufferList						outputList;
			AudioStreamPacketDescription		packetDescription;
            
            src = MirroredRingBufferGetReadPtr( ring );
            ctx->inputDataPtr							= src;
            ctx->inputDataEnd							= src + avail;
            packetCount									= 1;
            outputList.mNumberBuffers					= 1;
            outputList.mBuffers[ 0 ].mNumberChannels	= ctx->channels;
            outputList.mBuffers[ 0 ].mDataByteSize		= (UInt32)maxPayloadSize;
            outputList.mBuffers[ 0 ].mData				= pkt.pkt.rtp.payload;
			packetDescription.mStartOffset				= 0;
			packetDescription.mDataByteSize				= 0;
			packetDescription.mVariableFramesInPacket	= 0;
            
            err = AudioConverterFillComplexBuffer( ctx->inputConverter, _AudioEncoderEncodeCallback, ctx,
                                                  &packetCount, &outputList, &packetDescription );
            check( err == kNoErr || err == kUnderrunErr );
            check( packetCount == 0 || packetDescription.mStartOffset == 0 );
            MirroredRingBufferReadAdvance( ring, (size_t)( ctx->inputDataPtr - src ) );
            
            if( packetCount == 0 ) continue; // Skip if a packet wasn't produced.
            
            spp = packetCount * ctx->framesPerPacket;
            len = kRTPHeaderSize + packetDescription.mDataByteSize;

            check( len <= maxPayloadSize );
        }
        else
        {
            HostToBig16Mem( MirroredRingBufferGetReadPtr( ring ), avail, pkt.pkt.rtp.payload );
            len = kRTPHeaderSize + avail;
            MirroredRingBufferReadAdvance( ring, avail );
        }

        len += _AirPlayReceiver_EncryptAudio( ctx, &pkt, len - kRTPHeaderSize );

        n = sendto( ctx->dataSock, (const char *) &pkt, len, 0, &ctx->inputAddr.sa, ctx->inputAddrLen );
        err = map_socket_value_errno( inSession->audioInputSock, n == (ssize_t) len, n );
        if( err )
        {
            increment_saturate( ctx->sendErrors, UINT32_MAX );
            atr_stats_ulog( kLogLevelNotice, "### Audio audio send error (%u total): %#m\n", ctx->sendErrors, err );
        }

        seq += 1;
        ts  += spp;
    }
    ctx->inputSeqNum = seq;
    ctx->inputTimestamp = ts;
}

//===========================================================================================================================
//	AirPlayReceiverSessionWriteAudio
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionWriteAudio( 
		AirPlayReceiverSessionRef	inSession, 
		AirPlayStreamType			inType, 
		uint32_t					inSampleTime, 
		uint64_t					inHostTime, 
		const void *				inBuffer, 
		size_t						inLen )
{
	AirPlayAudioStreamContext * const		ctx					= &inSession->mainAudioCtx;
	size_t									len;
	MirroredRingBuffer * const				ring				= ctx->inputRingRef;
	OSStatus								err 				= kNoErr;

	(void) inType;
	(void) inSampleTime;
	(void) inHostTime;
	
	len = MirroredRingBufferGetBytesFree( ring );
	require_action_quiet( len >= inLen, exit, err = kNoSpaceErr; 
		atr_ulog( kLogLevelInfo, "### Audio input buffer full: %zu > %zu\n", inLen, len ) );
	memcpy( MirroredRingBufferGetWritePtr( ring ), inBuffer, inLen );
	MirroredRingBufferWriteAdvance( ring, inLen );

	pthread_mutex_lock( ctx->sendAudioMutexPtr );
	pthread_cond_signal( ctx->sendAudioCondPtr );
	pthread_mutex_unlock( ctx->sendAudioMutexPtr );
exit:
	return( err );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_GetTypeID
//===========================================================================================================================

static void _GetTypeID( void *inContext )
{
	(void) inContext;
	
	gAirPlayReceiverSessionTypeID = _CFRuntimeRegisterClass( &kAirPlayReceiverSessionClass );
	check( gAirPlayReceiverSessionTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	_Finalize
//===========================================================================================================================

static void	_Finalize( CFTypeRef inCF )
{
	AirPlayReceiverSessionRef const		session = (AirPlayReceiverSessionRef) inCF;
	
	atr_ulog( kLogLevelTrace, "Finalize session %llu\n", session->clientSessionID );
	
	if( session->delegate.finalize_f ) session->delegate.finalize_f( session, session->delegate.context );
	AirPlayReceiverSessionPlatformFinalize( session );
	
	dispatch_source_forget( &session->periodicTimer );
	_ScreenTearDown( session );
	_TearDownStream( session, &session->altAudioCtx, true );
	_TearDownStream( session, &session->mainAudioCtx, true );
	_ReplyTimerTearDown( session );
	_ControlTearDown( session );
	_TimingFinalize( session );
	_IdleStateKeepAliveFinalize( session );
	AirTunesClock_Finalize( session->airTunesClock );
	session->airTunesClock = NULL;
	
	AES_CBCFrame_Final( &session->decryptorStorage );
	pthread_mutex_forget( &session->mutexPtr );
	AirPlayReceiverSessionScreen_Forget( &session->screenSession );
	session->connection = NULL;
	ForgetCF( &session->server );
	dispatch_forget( &session->queue );
	dispatch_forget( &session->eventQueue );
}

//===========================================================================================================================
//	_PerformPeriodTasks
//===========================================================================================================================

static void	_PerformPeriodTasks( void *inContext )
{
	AirPlayReceiverSessionRef const		me  = (AirPlayReceiverSessionRef) inContext;
	AirTunesSource * const				ats = &me->source;
	uint64_t							ticks, idleTicks;
	
	// Check activity.
	
	ticks = UpTicks();
	if( ( ats->receiveCount == ats->lastReceiveCount ) && ( ats->activityCount == ats->lastActivityCount ) )
	{
		// If we've been idle for a while then log it.
		
		idleTicks = ticks - ats->lastActivityTicks;
		if( ( ticks - ats->lastIdleLogTicks ) > ats->idleLogIntervalTicks )
		{
			atr_ulog( kLogLevelInfo, "### Idle for %llu seconds\n", idleTicks / UpTicksPerSecond() );
			ats->lastIdleLogTicks = ticks;
		}
		
		// If there hasn't been activity in long time, fail the session.
		
		if( ( idleTicks > ats->maxIdleTicks )
				&& ( ( !_UsingIdleStateKeepAlive( me ) )
					|| ( _UsingIdleStateKeepAlive( me ) && me->sessionStarted &&  _UsingScreenOrAudio( me ) ) )
				)
		{
			atr_ulog( kLogLevelError, "Idle timeout after %d seconds with no audio\n", me->server->timeoutDataSecs );
			AirPlayReceiverServerControl( me->server, kCFObjectFlagDirect, CFSTR( kAirPlayCommand_SessionDied ), me, NULL, NULL );
			goto exit;
		}
	}
	else
	{
		ats->lastReceiveCount	= ats->receiveCount;
		ats->lastActivityCount	= ats->activityCount;
		ats->lastActivityTicks	= ticks;
		ats->lastIdleLogTicks	= ticks;
	}
	
	// Check for glitches.
	
	if( ticks >= me->glitchNextTicks )
	{
		int		glitches;
		
		++me->glitchTotalPeriods;
		glitches = me->glitchTotal - me->glitchLast;
		me->glitchLast += glitches;
		if( glitches > 0 )
		{
			++me->glitchyPeriods;
			atr_ulog( kLogLevelNotice, "### %d glitches in the last minute of %d minute(s) (%d%% glitchy)\n", 
				glitches, me->glitchTotalPeriods, ( me->glitchyPeriods * 100 ) / me->glitchTotalPeriods );
		}
		else
		{
			atr_ulog( kLogLevelVerbose, "No glitches in the last minute of %d minutes (%d%% glitchy)\n", 
				me->glitchTotalPeriods, ( me->glitchyPeriods * 100 ) / me->glitchTotalPeriods );
		}
		me->glitchNextTicks = ticks + me->glitchIntervalTicks;
	}
	
	_LogUpdate( me, ticks, false );
	
exit:
	return;
}

//===========================================================================================================================
//	_SessionLock
//===========================================================================================================================

static OSStatus	_SessionLock( AirPlayReceiverSessionRef inSession )
{
	OSStatus		err;
	
	require_action( inSession->mutexPtr, exit, err = kNotInitializedErr );
	
	err = pthread_mutex_lock( inSession->mutexPtr );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_SessionUnlock
//===========================================================================================================================

static OSStatus	_SessionUnlock( AirPlayReceiverSessionRef inSession )
{
	OSStatus		err;
	
	require_action( inSession->mutexPtr, exit, err = kNotInitializedErr );
	
	err = pthread_mutex_unlock( inSession->mutexPtr );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_UpdateFeedback
//===========================================================================================================================

static OSStatus	_UpdateFeedback( AirPlayReceiverSessionRef inSession, CFDictionaryRef inInput, CFDictionaryRef *outOutput )
{
	OSStatus					err;
	AirPlayTimestampTuple		zeroTime;
	CFMutableDictionaryRef		feedback = NULL;
	CFMutableArrayRef			streams  = NULL;
	CFMutableDictionaryRef		stream   = NULL;
	
	(void) inInput;
	
	require_action_quiet( inSession->mainAudioCtx.session || inSession->altAudioCtx.session, exit, err = kNoErr );
	require_action_quiet( outOutput, exit, err = kNoErr );
	
	feedback = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( feedback, exit, err = kNoMemoryErr );
	
	streams = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( streams, exit, err = kNoMemoryErr );
	CFDictionarySetValue( feedback, CFSTR( kAirPlayKey_Streams ), streams );
	
	if( inSession->mainAudioCtx.session )
	{
		stream = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( stream, exit, err = kNoMemoryErr );
		
		pthread_mutex_lock( inSession->mainAudioCtx.zeroTimeLockPtr );
		zeroTime = inSession->mainAudioCtx.zeroTime;
		pthread_mutex_unlock( inSession->mainAudioCtx.zeroTimeLockPtr );

		CFDictionarySetInt64( stream, CFSTR( kAirPlayKey_Type ), inSession->mainAudioCtx.type );
		CFDictionarySetDouble( stream, CFSTR( kAirPlayKey_SampleRate ), inSession->mainAudioCtx.rateAvg );
		
		// 'zeroTime' is only valid if we've had at least one rate update
		if( inSession->mainAudioCtx.rateUpdateCount > 0 )
		{
			CFDictionarySetUInt64( stream, CFSTR( kAirPlayKey_StreamConnectionID ), inSession->mainAudioCtx.connectionID );
			CFDictionarySetUInt64( stream, CFSTR( kAirPlayKey_Timestamp ), zeroTime.hostTime );
			CFDictionarySetUInt64( stream, CFSTR( kAirPlayKey_TimestampRawNs ), zeroTime.hostTimeRaw );
			CFDictionarySetInt64( stream, CFSTR( kAirPlayKey_SampleTime ), zeroTime.sampleTime );
		}
		
		CFArrayAppendValue( streams, stream );
		CFRelease( stream );
		stream = NULL;
	}
	if( inSession->altAudioCtx.session )
	{
		stream = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( stream, exit, err = kNoMemoryErr );
		
		pthread_mutex_lock( inSession->altAudioCtx.zeroTimeLockPtr );
		zeroTime = inSession->altAudioCtx.zeroTime;
		pthread_mutex_unlock( inSession->altAudioCtx.zeroTimeLockPtr );

		CFDictionarySetInt64( stream, CFSTR( kAirPlayKey_Type ), kAirPlayStreamType_AltAudio );
		CFDictionarySetDouble( stream, CFSTR( kAirPlayKey_SampleRate ), inSession->altAudioCtx.rateAvg );

		// 'zeroTime' is only valid if we've had at least one rate update
		if( inSession->altAudioCtx.rateUpdateCount > 0 )
		{
			CFDictionarySetUInt64( stream, CFSTR( kAirPlayKey_StreamConnectionID ), inSession->altAudioCtx.connectionID );
			CFDictionarySetUInt64( stream, CFSTR( kAirPlayKey_Timestamp ), zeroTime.hostTime );
			CFDictionarySetUInt64( stream, CFSTR( kAirPlayKey_TimestampRawNs ), zeroTime.hostTimeRaw );
			CFDictionarySetInt64( stream, CFSTR( kAirPlayKey_SampleTime ), zeroTime.sampleTime );
		}
		
		CFArrayAppendValue( streams, stream );
		CFRelease( stream );
		stream = NULL;
	}
	
	*outOutput = feedback;
	feedback = NULL;
	err = kNoErr;
	
exit:
	CFReleaseNullSafe( stream );
	CFReleaseNullSafe( streams );
	CFReleaseNullSafe( feedback );
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Control/Events ==
#endif

//===========================================================================================================================
//	_ControlIdleStateTransition
//===========================================================================================================================

static OSStatus _ControlIdleStateTransition( AirPlayReceiverSessionRef inSession, CFMutableDictionaryRef inResponseParams )
{
	OSStatus		err;
	Boolean			idle;

	idle = !_UsingScreenOrAudio( inSession );
	require_action_quiet( ( !inSession->sessionIdleValid || ( idle != inSession->sessionIdle ) ), exit, err = kNoErr );

	atr_ulog( kLogLevelTrace, "mainAudio %c. altAudio %c. screen %c\n",
			inSession->mainAudioCtx.type != kAirPlayStreamType_Invalid ? 'Y' : 'N',
			inSession->altAudioCtx.type != kAirPlayStreamType_Invalid ? 'Y' : 'N',
			inSession->screenInitialized ? 'Y' : 'N'
			);

	if( idle )
	{
		if ( _UsingIdleStateKeepAlive( inSession ) )
		{
			_IdleStateKeepAliveStart( inSession );
			SocketSetKeepAlive( inSession->connection->httpCnx->sock, 0, 0 );
		}

		if( NULL != inSession->timingThreadPtr ) _TimingFinalize( inSession );
	}
	else
	{
		if ( _UsingIdleStateKeepAlive( inSession ) )
		{
			_IdleStateKeepAliveStop( inSession );
			SocketSetKeepAlive( inSession->connection->httpCnx->sock, kAirPlayDataTimeoutSecs / 10, 3 );    //9 sec
		}

		check( NULL == inSession->timingThreadPtr );

		if( !IsValidSocket( inSession->timingSock ) )
		{
			err = _TimingInitialize( inSession );
			require_noerr( err, exit );
			if( inResponseParams ) CFDictionarySetInt64( inResponseParams, CFSTR( kAirPlayKey_Port_Timing ), inSession->timingPortLocal );
		}

		err = _TimingNegotiate( inSession );
		require_noerr( err, exit );
	}

	inSession->sessionIdle = idle;
	inSession->sessionIdleValid = true;
	err = kNoErr;
exit:
	if( err ) atr_ulog( kLogLevelWarning, "### Control idle state transition failed: %#m\n", err );
	return err;
}

//===========================================================================================================================
//	_ControlSetup
//===========================================================================================================================

static OSStatus
	_ControlSetup( 
		AirPlayReceiverSessionRef	inSession, 
		CFDictionaryRef				inRequestParams, 
		CFMutableDictionaryRef		inResponseParams )
{
	OSStatus		err;
	
	require_action( !inSession->controlSetup, exit2, err = kAlreadyInitializedErr );
	
	inSession->timingPortRemote = (int) CFDictionaryGetInt64( inRequestParams, CFSTR( kAirPlayKey_Port_Timing ), NULL );
	if( inSession->timingPortRemote <= 0 ) inSession->timingPortRemote = kAirPlayFixedPort_TimeSyncLegacy;
	
	err = AirTunesClock_Create( &inSession->airTunesClock );
	require_noerr( err, exit );
	
	err = _TimingInitialize( inSession );
	require_noerr( err, exit );
	CFDictionarySetInt64( inResponseParams, CFSTR( kAirPlayKey_Port_Timing ), inSession->timingPortLocal );

	if( CFDictionaryGetBoolean( inRequestParams, CFSTR( kAirPlayKey_KeepAliveLowPower ), NULL ) )
	{
		// Supports receiving UDP beacon as keep alive.

		err = _IdleStateKeepAliveInitialize( inSession );
		require_noerr( err, exit );
		CFDictionarySetInt64( inResponseParams, CFSTR( kAirPlayKey_Port_KeepAlive ), inSession->keepAlivePortLocal );
	}
	
	if( inSession->useEvents )
	{
		err = ServerSocketOpen( inSession->peerAddr.sa.sa_family, SOCK_STREAM, IPPROTO_TCP, -kAirPlayPort_RTSPEvents,
			&inSession->eventPort, kSocketBufferSize_DontSet, &inSession->eventSock );
		require_noerr( err, exit );
		
		CFDictionarySetInt64( inResponseParams,  CFSTR( kAirPlayKey_Port_Event ), inSession->eventPort );
		
		atr_ulog( kLogLevelTrace, "Events set up on port %d\n", inSession->eventPort );
	}
	inSession->controlSetup = true;
	
exit:
	if( err ) _ControlTearDown( inSession );
	
exit2:
	if( err ) atr_ulog( kLogLevelWarning, "### Control setup failed: %#m\n", err );
	return( err );
}

//===========================================================================================================================
//	_ControlTearDown
//===========================================================================================================================

static void _ForgetEventClient( void * inCtx )
{
	AirPlayReceiverSessionRef		session;
	
	session = (AirPlayReceiverSessionRef) inCtx;
	HTTPClientForget( &session->eventClient );
}

static void	_ControlTearDown( AirPlayReceiverSessionRef inSession )
{
	dispatch_sync_f( inSession->eventQueue, inSession, _ForgetEventClient );
	ForgetCF( &inSession->pairVerifySession );
	ForgetSocket( &inSession->eventSock );
	if( inSession->controlSetup )
	{
		inSession->controlSetup = false;
		atr_ulog( kLogLevelTrace, "Control torn down\n" );
	}
}

//===========================================================================================================================
//	_ControlStart
//===========================================================================================================================

static OSStatus	_ControlStart( AirPlayReceiverSessionRef inSession )
{
	OSStatus		err;
	SocketRef		newSock = kInvalidSocketRef;
	sockaddr_ip		sip;
	
	err = SocketAccept( inSession->eventSock, kAirPlayConnectTimeoutSecs, &newSock, &sip );
	require_noerr( err, exit );
	ForgetSocket( &inSession->eventSock );
	
	err = HTTPClientCreateWithSocket( &inSession->eventClient, newSock );
	require_noerr( err, exit );
	newSock = kInvalidSocketRef;
	
	HTTPClientSetDispatchQueue( inSession->eventClient, inSession->eventQueue );
	HTTPClientSetLogging( inSession->eventClient, atr_events_ucat() );
	
	// Configure HTTPClient for encryption if needed
	
	if( inSession->pairVerifySession )
	{
		NetTransportDelegate		delegate;
		uint8_t						readKey[ 32 ], writeKey[ 32 ];
	
		err = PairingSessionDeriveKey( inSession->pairVerifySession, kAirPlayPairingEventsKeySaltPtr, kAirPlayPairingEventsKeySaltLen, 
			kAirPlayPairingEventsKeyReadInfoPtr, kAirPlayPairingEventsKeyReadInfoLen, sizeof( readKey ), readKey );
		require_noerr( err, exit );
		
		err = PairingSessionDeriveKey( inSession->pairVerifySession, kAirPlayPairingEventsKeySaltPtr, kAirPlayPairingEventsKeySaltLen, 
			kAirPlayPairingEventsKeyWriteInfoPtr, kAirPlayPairingEventsKeyWriteInfoLen, sizeof( writeKey ), writeKey );
		require_noerr( err, exit );
		
		err = NetTransportChaCha20Poly1305Configure( &delegate, atr_ucat(), readKey, NULL, writeKey, NULL );
		require_noerr( err, exit );
		MemZeroSecure( readKey, sizeof( readKey ) );
		MemZeroSecure( writeKey, sizeof( writeKey ) );
		HTTPClientSetTransportDelegate( inSession->eventClient, &delegate );
	}
	
	atr_ulog( kLogLevelTrace, "Events started on port %d to port %d\n", inSession->eventPort, SockAddrGetPort( &sip ) );
	
exit:
	ForgetSocket( &newSock );
	if( err ) atr_ulog( kLogLevelWarning, "### Event start failed: %#m\n", err );
	return( err );
}

#if 0
#pragma mark -
#pragma mark == GeneralAudio ==
#endif

//===========================================================================================================================
//	_GeneralAudioSetup
//===========================================================================================================================

static OSStatus
	_GeneralAudioSetup(
		AirPlayReceiverSessionRef	me,
		AirPlayStreamType			inStreamType,
		CFDictionaryRef				inStreamDesc,
		CFMutableDictionaryRef		inResponseParams )
{
	AirTunesSource * const					ats = &me->source;
	AirPlayAudioStreamContext * const		ctx = &me->mainAudioCtx;
	OSStatus								err;
	CFMutableDictionaryRef					responseStreamDesc;
	AirPlayAudioFormat						format;
	AudioStreamBasicDescription				asbd;
	sockaddr_ip								sip;
	size_t									i, n;
	uint8_t *								ptr;
	uint32_t								latencyMs;
	uint64_t								streamConnectionID = 0;
	uint8_t									outputKey[ 32 ];
	
	ctx->sampleTimeOffset = 0;
	
	require_action( !IsValidSocket( ctx->dataSock ), exit2, err = kAlreadyInitializedErr );
	
	responseStreamDesc = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( responseStreamDesc, exit, err = kNoMemoryErr );
	
	streamConnectionID = (uint64_t) CFDictionaryGetInt64( inStreamDesc, CFSTR( kAirPlayKey_StreamConnectionID ), &err );
	if( err )
	{
		UUIDData		uuid;
		
		UUIDGet( &uuid );
		ctx->connectionID = ReadBig64( uuid.bytes );
		err = kNoErr;
	}
	else
	{
		ctx->connectionID = streamConnectionID;
	}
	
	if( me->pairVerifySession )	require_action( streamConnectionID, exit, err = kVersionErr );

	MemZeroSecure( &ctx->outputCryptor, sizeof( ctx->outputCryptor ) );
	if( streamConnectionID && me->pairVerifySession )
	{
		err = _GetStreamSecurityKeys( me, streamConnectionID, 0, NULL, 32, outputKey );
		require_noerr( err, exit );
		
		memset( ctx->outputCryptor.nonce, 0, sizeof( ctx->outputCryptor.nonce ) );
		memcpy( ctx->outputCryptor.key, outputKey, 32 );
		ctx->outputCryptor.isValid = true;
	}
	
	check( inStreamType == kAirPlayStreamType_MainHighAudio );
	ctx->type = inStreamType;
	ctx->label = "MainHigh";

	me->compressionType = (AirPlayCompressionType) CFDictionaryGetInt64( inStreamDesc, CFSTR( kAirPlayKey_CompressionType ), NULL );
	format = (AirPlayAudioFormat) CFDictionaryGetInt64( inStreamDesc, CFSTR( kAirPlayKey_AudioFormat ), NULL );
	if( format == kAirPlayAudioFormat_Invalid )
	{
		if(      me->compressionType == kAirPlayCompressionType_PCM )		format = kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo;
		else if( me->compressionType == kAirPlayCompressionType_AAC_LC )	format = kAirPlayAudioFormat_AAC_LC_44KHz_Stereo;
		else { dlogassert( "Bad compression type: 0x%X", me->compressionType ); err = kUnsupportedErr; goto exit; }
	}
	err = AirPlayAudioFormatToASBD( format, &asbd, &ctx->bitsPerSample );
	require_noerr( err, exit );
	
	if( me->compressionType == kAirPlayCompressionType_Undefined )
	{
		me->compressionType = AudioFormatIDToAirPlayCompressionType( asbd.mFormatID );
	}
	else
	{
		check( me->compressionType == AudioFormatIDToAirPlayCompressionType( asbd.mFormatID ) ); 
	}
	
	ctx->sampleRate		= (uint32_t) asbd.mSampleRate;
	ctx->channels		= asbd.mChannelsPerFrame;
	ctx->bytesPerUnit	= asbd.mBytesPerFrame;
	if( ctx->bytesPerUnit == 0 ) ctx->bytesPerUnit = ( RoundUp( ctx->bitsPerSample, 8 ) * ctx->channels ) / 8;
	
	me->rtpAudioDupsInitialized	= false;
	me->redundantAudio			= (int) CFDictionaryGetInt64( inStreamDesc, CFSTR( kAirPlayKey_RedundantAudio ), NULL );
	me->screen					= CFDictionaryGetBoolean( inStreamDesc, CFSTR( kAirPlayKey_UsingScreen ), NULL );
	me->rtcpPortRemote			= (int) CFDictionaryGetInt64( inStreamDesc, CFSTR( kAirPlayKey_Port_Control ), NULL );
	if( me->rtcpPortRemote <= 0 ) me->rtcpPortRemote = kAirPlayFixedPort_RTCPLegacy;
	latencyMs					= (uint32_t) CFDictionaryGetInt64( inStreamDesc, CFSTR( kAirPlayKey_AudioLatencyMs ), NULL );
	if( latencyMs > 0 )
	{
		me->minLatency			= ctx->sampleRate * latencyMs / 1000;
		me->maxLatency			= ctx->sampleRate * latencyMs / 1000;
	}
	else
	{
		me->minLatency				= (uint32_t) CFDictionaryGetInt64( inStreamDesc, CFSTR( kAirPlayKey_LatencyMin ), NULL );
		me->maxLatency				= (uint32_t) CFDictionaryGetInt64( inStreamDesc, CFSTR( kAirPlayKey_LatencyMax ), NULL );
	}
	me->audioQoS				= me->screen ? kSocketQoS_AirPlayScreenAudio : kSocketQoS_AirPlayAudio;
	me->framesPerPacket	= (uint32_t) CFDictionaryGetInt64( inStreamDesc, CFSTR( kAirPlayKey_FramesPerPacket ), NULL );
	i = ( me->framesPerPacket + ( ( me->framesPerPacket / kAirTunesMaxSkewAdjustRate ) + 16 ) ) * ctx->bytesPerUnit;
	me->nodeCount				= kAirTunesBufferNodeCountUDP;
	me->nodeBufferSize			= kRTPHeaderSize + Max( kAirTunesMaxPacketSizeUDP, i );
	
	ctx->rateUpdateNextTicks		= 0;
	ctx->rateUpdateIntervalTicks	= SecondsToUpTicks( 1 );
	ctx->rateUpdateCount			= 0;
	ctx->rateAvg					= (Float32) ctx->sampleRate;

	err = pthread_mutex_init( &ctx->zeroTimeLock, NULL );
	require_noerr( err, exit );
	ctx->zeroTimeLockPtr = &ctx->zeroTimeLock;
	
	// Set up RTP.
	
	err = ServerSocketOpen( me->peerAddr.sa.sa_family, SOCK_DGRAM, IPPROTO_UDP, -kAirPlayPort_RTPAudio, 
		&me->rtpAudioPort, -kAirTunesRTPSocketBufferSize, &ctx->dataSock );
	require_noerr( err, exit );
	
	SocketSetQoS( ctx->dataSock, me->audioQoS );
	
	err = OpenSelfConnectedLoopbackSocket( &ctx->cmdSock );
	require_noerr( err, exit );
	
	// Set up RTCP.
	
	SockAddrCopy( &me->peerAddr, &sip );
	err = ServerSocketOpen( sip.sa.sa_family, SOCK_DGRAM, IPPROTO_UDP, -kAirPlayPort_RTCPServer, &me->rtcpPortLocal, 
		-kAirTunesRTPSocketBufferSize, &me->rtcpSock );
	require_noerr( err, exit );
	
	SocketSetPacketTimestamps( me->rtcpSock, true );
	SocketSetQoS( me->rtcpSock, me->audioQoS );
	
	SockAddrSetPort( &sip, me->rtcpPortRemote );
	me->rtcpRemoteAddr = sip;
	me->rtcpRemoteLen  = SockAddrGetSize( &sip );
	err = connect( me->rtcpSock, &sip.sa, me->rtcpRemoteLen );
	err = map_socket_noerr_errno( me->rtcpSock, err );
	if( err ) dlog( kLogLevelNotice, "### RTCP connect UDP to %##a failed (using sendto instead): %#m\n", &sip, err );
	me->rtcpConnected = !err;
	
	// Set up retransmissions.
	
	if( me->redundantAudio )
	{
		ats->rtcpRTDisable = true;
	}
	else
	{
		ats->rtcpRTListStorage = (AirTunesRetransmitNode *) malloc( kAirTunesRetransmitCount * sizeof( AirTunesRetransmitNode ) );
		require_action( ats->rtcpRTListStorage, exit, err = kNoMemoryErr );
		
		n = kAirTunesRetransmitCount - 1;
		for( i = 0; i < n; ++i )
		{
			ats->rtcpRTListStorage[ i ].next = &ats->rtcpRTListStorage[ i + 1 ];
		}
		ats->rtcpRTListStorage[ i ].next = NULL;
		ats->rtcpRTFreeList = ats->rtcpRTListStorage;
		
		ats->rtcpRTMinRTTNanos			= INT64_MAX;
		ats->rtcpRTMaxRTTNanos			= INT64_MIN;
		ats->rtcpRTAvgRTTNanos			= 100000000; // Default to 100 ms.
		ats->rtcpRTTimeoutNanos			= 100000000; // Default to 100 ms.
		ats->rtcpRTDisable				= me->redundantAudio;
		ats->retransmitMinNanos			= UINT64_MAX;
		ats->retransmitRetryMinNanos	= UINT64_MAX;
		if( inStreamType == kAirPlayStreamType_MainHighAudio)
		{
			// Half the minimum latency
			ats->retransmitMaxLoss		= ( ( me->minLatency + ( me->framesPerPacket / 2 ) ) / me->framesPerPacket ) / 2;
		}
		else
		{
			ats->retransmitMaxLoss		= kAirTunesRetransmitMaxLoss;
		}
	}
	
	// Set up buffering. The free list is a normal head pointer, null tail list and is initially populated with all the 
	// nodes. The busy list is doubly-linked and circular with a sentinel node to simplify and speed up linked list code.
	
	me->nodeHeaderStorage = (AirTunesBufferNode *) malloc( me->nodeCount * sizeof( AirTunesBufferNode ) );
	require_action( me->nodeHeaderStorage, exit, err = kNoMemoryErr );
	
	me->nodeBufferStorage = (uint8_t *) malloc( me->nodeCount * me->nodeBufferSize );
	require_action( me->nodeBufferStorage, exit, err = kNoMemoryErr );
	
	ptr = me->nodeBufferStorage;
	n = me->nodeCount - 1;
	for( i = 0; i < n; ++i )
	{
		me->nodeHeaderStorage[ i ].next = &me->nodeHeaderStorage[ i + 1 ];
		me->nodeHeaderStorage[ i ].data = ptr;
		ptr += me->nodeBufferSize;
	}
	me->nodeHeaderStorage[ i ].next = NULL;
	me->nodeHeaderStorage[ i ].data = ptr;
	ptr += me->nodeBufferSize;
	check( ptr == ( me->nodeBufferStorage + ( me->nodeCount * me->nodeBufferSize ) ) );
	
	me->freeList						= me->nodeHeaderStorage;
	me->busyNodeCount					= 0;
	me->busyListSentinelStorage.prev	= &me->busyListSentinelStorage;
	me->busyListSentinelStorage.next	= &me->busyListSentinelStorage;
	me->busyListSentinel				= &me->busyListSentinelStorage;
	
	// Set up temporary buffers.
	
	if( 0 ) {} // Empty if to simplify conditional logic below.
	else if( me->compressionType == kAirPlayCompressionType_AAC_LC )
	{
		err = _AudioDecoderInitialize( me );
		require_noerr( err, exit );
		
		n = me->nodeBufferSize;
	}
	else if( me->compressionType == kAirPlayCompressionType_PCM )
	{
		n = me->nodeBufferSize;
	}
	else
	{
		dlogassert( "Unsupported compression type: %d", me->compressionType );
		err = kUnsupportedErr;
		goto exit;
	}
	
	me->decodeBufferSize = n;
	me->decodeBuffer = (uint8_t *) malloc( me->decodeBufferSize );
	require_action( me->decodeBuffer, exit, err = kNoMemoryErr );
	
	me->readBufferSize = n;
	me->readBuffer = (uint8_t *) malloc( me->readBufferSize );
	require_action( me->readBuffer, exit, err = kNoMemoryErr );
	
	me->skewAdjustBufferSize = n;
	me->skewAdjustBuffer = (uint8_t *) malloc( me->skewAdjustBufferSize );
	require_action( me->skewAdjustBuffer, exit, err = kNoMemoryErr );
	
	EWMA_FP_Init( &gAirPlayAudioStats.bufferAvg, 0.25, kEWMAFlags_StartWithFirstValue );
	gAirPlayAudioStats.lostPackets			= 0;
	gAirPlayAudioStats.unrecoveredPackets	= 0;
	gAirPlayAudioStats.latePackets			= 0;
	
	// Add the stream to the response.
	
	CFDictionarySetInt64( responseStreamDesc, CFSTR( kAirPlayKey_Type ), ctx->type );
	CFDictionarySetInt64( responseStreamDesc, CFSTR( kAirPlayKey_Port_Data ), me->rtpAudioPort );
	CFDictionarySetInt64( responseStreamDesc, CFSTR( kAirPlayKey_Port_Control ), me->rtcpPortLocal );
	if( ctx->type == kAirPlayStreamType_MainHighAudio )
		CFDictionarySetInt64( responseStreamDesc, CFSTR( kAirPlayKey_StreamConnectionID ), ctx->connectionID );
	
	err = _AddResponseStream( inResponseParams, responseStreamDesc );
	require_noerr( err, exit );
	
	ctx->session = me;
	atr_ulog( kLogLevelTrace, "%s audio set up for %s on port %d, RTCP on port %d\n",
		ctx->label, AirPlayAudioFormatToString( format ), me->rtpAudioPort, me->rtcpPortLocal );
	
	// If the session's already started then immediately start the thread process it.
	
	if( me->sessionStarted && !ctx->threadPtr )
	{
		err = pthread_create( &ctx->thread, NULL, _GeneralAudioThread, me );
		require_noerr( err, exit );
		ctx->threadPtr = &ctx->thread;
	}
	
exit:
	CFReleaseNullSafe( responseStreamDesc );
	if( err ) _TearDownStream( me, ctx, false );
	
exit2:
	if( err ) atr_ulog( kLogLevelWarning, "### General audio setup failed: %#m\n", err );
	return( err );
}

//===========================================================================================================================
//	_AudioSenderThread
//===========================================================================================================================
static void *	_AudioSenderThread( void *inArg )
{
	AirPlayAudioStreamContext * const	ctx					= inArg;
	
	SetThreadName( "AirPlayAudioSender" );
	SetCurrentThreadPriority( kAirPlayThreadPriority_AudioSender );
	
	pthread_mutex_lock( ctx->sendAudioMutexPtr );
	while( !ctx->sendAudioDone )
	{
		pthread_cond_wait( ctx->sendAudioCondPtr, ctx->sendAudioMutexPtr );
		pthread_mutex_unlock( ctx->sendAudioMutexPtr );
		_AirPlayReceiver_SendAudio( ctx );
		pthread_mutex_lock( ctx->sendAudioMutexPtr );
	}
	
	pthread_mutex_unlock( ctx->sendAudioMutexPtr );
	return( NULL );
}

//===========================================================================================================================
//	_GeneralAudioThread
//===========================================================================================================================

static void *	_GeneralAudioThread( void *inArg )
{
	AirPlayReceiverSessionRef const		session		= (AirPlayReceiverSessionRef) inArg;
	AirPlayAudioStreamContext * const	ctx			= &session->mainAudioCtx;
	SocketRef const						rtpSock		= ctx->dataSock;
	SocketRef const						rtcpSock	= session->rtcpSock;
	SocketRef const						cmdSock		= ctx->cmdSock;
	fd_set								readSet;
	int									maxFd;
	int									n;
	OSStatus							err;
	
	SetThreadName( "AirPlayAudioReceiver" );
	SetCurrentThreadPriority( kAirPlayThreadPriority_AudioReceiver );
	
	FD_ZERO( &readSet );
	maxFd = -1;
	if( (int) rtpSock  > maxFd ) maxFd = rtpSock;
	if( (int) rtcpSock > maxFd ) maxFd = rtcpSock;
	if( (int) cmdSock  > maxFd ) maxFd = cmdSock;
	maxFd += 1;
	for( ;; )
	{
		FD_SET( rtpSock,  &readSet );
		FD_SET( rtcpSock, &readSet );
		FD_SET( cmdSock,  &readSet );
		n = select( maxFd, &readSet, NULL, NULL, NULL );
		err = select_errno( n );
		if( err == EINTR ) continue;
		if( err ) { dlogassert( "select() error: %#m", err ); usleep( 100000 ); continue; }
		
		if( FD_ISSET( rtpSock,  &readSet ) ) _GeneralAudioReceiveRTP( session, NULL, 0 );
		if( FD_ISSET( rtcpSock, &readSet ) ) _GeneralAudioReceiveRTCP( session, rtcpSock, kRTCPTypeAny );
		if( FD_ISSET( cmdSock,  &readSet ) ) break; // The only event is quit so break if anything is pending.
	}
	atr_ulog( kLogLevelTrace, "General audio thread exit\n" );
	return( NULL );
}

//===========================================================================================================================
//	_GeneralAudioReceiveRTCP
//===========================================================================================================================

static OSStatus	_GeneralAudioReceiveRTCP( AirPlayReceiverSessionRef inSession, SocketRef inSock, RTCPType inExpectedType )
{
	OSStatus					err;
	RTCPPacket					pkt;
	size_t						len;
	uint64_t					ticks;
	int							tmp;
	
	// Read the packet and do some very simple validity checks.
	
	err = SocketRecvFrom( inSock, &pkt, sizeof( pkt ), &len, NULL, 0, NULL, &ticks, NULL, NULL );
	if( err == EWOULDBLOCK ) goto exit;
	require_noerr( err, exit );
	if( len < sizeof( pkt.header ) )
	{
		dlogassert( "Bad size: %zu < %zu", sizeof( pkt.header ), len );
		err = kSizeErr;
		goto exit;
	}
	
	tmp = RTCPHeaderExtractVersion( pkt.header.v_p_c );
	if( tmp != kRTPVersion )
	{
		dlogassert( "Bad version: %d", tmp );
		err = kVersionErr;
		goto exit;
	}
	if( ( inExpectedType != kRTCPTypeAny ) && ( pkt.header.pt != inExpectedType ) )
	{
		dlogassert( "Wrong packet type: %d vs %d", pkt.header.pt, inExpectedType );
		err = kTypeErr;
		goto exit;
	}
	
	// Dispatch the packet to the appropriate handler.
	
	switch( pkt.header.pt )
	{
		case kRTCPTypeRetransmitResponse:
			err = _RetransmitsProcessResponse( inSession, &pkt.retransmitAck, len );
			break;
		
		default:
			dlogassert( "Unsupported packet type: %d (%zu bytes)", pkt.header.pt, len );
			err = kUnsupportedErr;
			goto exit;
	}
	
exit:
	return( err );
}

//===========================================================================================================================
//	_GeneralAudioReceiveRTP
//===========================================================================================================================

static OSStatus	_GeneralAudioReceiveRTP( AirPlayReceiverSessionRef inSession, RTPPacket *inPkt, size_t inSize )
{
	AirPlayAudioStreamContext * const		ctx = &inSession->mainAudioCtx;
	OSStatus								err;
	AirTunesBufferNode *					node;
	AirTunesBufferNode *					stop;
	size_t									len;
	
	// Get a free node. If there aren't any free nodes, steal the oldest busy node.
	
	_SessionLock( inSession );
	node = inSession->freeList;
	if( node )
	{
		inSession->freeList = node->next;
	}
	else
	{
		stop = inSession->busyListSentinel;
		node = stop->next;
		if( node != stop )
		{
			node->next->prev = node->prev;
			node->prev->next = node->next;
			--inSession->busyNodeCount;
			
			atr_stats_ulog( kLogLevelVerbose, "### No free buffer nodes. Stealing oldest busy node.\n" );
		}
		else
		{
			node = NULL;
			
			dlogassert( "No buffer nodes at all? Probably a bug in the code\n" );
			usleep( 100000 ); // Sleep for a moment to avoid hogging the CPU on continual failures.
			err = kNoResourcesErr;
			goto exit;
		}
	}
	
	// Get the packet. If a packet was passed in then use it directly. Otherwise, read it from the socket.
	
	if( inPkt )
	{
		require_action( inSize <= inSession->nodeBufferSize, exit, err = kSizeErr );
		memcpy( node->data, inPkt, inSize );
		len = inSize;
	}
	else
	{
			err = SocketRecvFrom( ctx->dataSock, node->data, inSession->nodeBufferSize, &len, NULL, 0, NULL, NULL, NULL, NULL );
		if( err == EWOULDBLOCK ) goto exit;
		require_noerr( err, exit );
	}
	
	// Process the packet. Warning: this function MUST either queue the node onto the busy queue and return kNoErr or
	// it MUST return an error and NOT queue the packet. Doing anything else will lead to leaks and/or crashes.
	
	err = _GeneralAudioProcessPacket( inSession, node, len, inPkt != NULL );
	require_noerr_quiet( err, exit );
	node = NULL;
	
exit:
	if( node )
	{
		// Processing the packet failed so put the node back on the free list.
		
		node->next = inSession->freeList;
		inSession->freeList = node;
	}
	_SessionUnlock( inSession );
	return( err );
}

//===========================================================================================================================
//	_GeneralAudioProcessPacket
//
//	Warning: Assumes the AirTunes lock is held.
//
//	Warning: this function MUST either queue the node onto the busy queue and return kNoErr or it MUST return an error 
//	and NOT queue the packet. Doing anything else will lead to leaks and/or crashes.
//
//	Note: The "buf" field of "inNode" is expected to be filled in, but all other fields are filled in by this function.
//===========================================================================================================================

static OSStatus
	_GeneralAudioProcessPacket( 
		AirPlayReceiverSessionRef	inSession, 
		AirTunesBufferNode *		inNode, 
		size_t 						inSize, 
		Boolean						inIsRetransmit )
{
	OSStatus					err;
	RTPPacket *					rtp;
	uint16_t					pktSeq;
	uint32_t					pktTS;
	AirTunesBufferNode *		curr;
	AirTunesBufferNode *		stop;
	
	// Validate and parse the packet.
	
	if( inSize < kRTPHeaderSize ) { dlogassert( "Packet too small: %zu bytes", inSize ); err = kSizeErr; goto exit; }
	rtp					= (RTPPacket *) inNode->data;
	rtp->header.seq		= pktSeq = ntohs( rtp->header.seq );
	rtp->header.ts		= pktTS  = ntohl( rtp->header.ts );
	rtp->header.ssrc	= ntohl( rtp->header.ssrc );
	inNode->rtp			= rtp;
	inNode->ptr			= inNode->data + kRTPHeaderSize;
	inNode->size		= inSize - kRTPHeaderSize;
	inNode->ts			= pktTS;
	
	if( _GeneralAudioTrackDups( inSession, pktSeq ) )	{ err = kDuplicateErr; goto exit; }
	if( !inIsRetransmit )								_GeneralAudioTrackLosses( inSession, inNode );
	if( !inSession->source.rtcpRTDisable )				_RetransmitsUpdate( inSession, inNode, inIsRetransmit );
	if( !inIsRetransmit )								increment_wrap( inSession->source.receiveCount, 1 );
	
	// If we're flushing then drop packets with timestamps before the end of the flush. Because of retranmissions
	// and out-of-order packets, we may receive pre-flush packets *after* post-flush packets. We want to drop all
	// pre-flush packets, but keep post-flush packets so we keep flush mode active until we hit the flush timeout.
	
	if( inSession->flushing )
	{
		if( Mod32_LT( pktTS, inSession->flushUntilTS ) )
		{
			err = kSkipErr;
			goto exit;
		}
		if( Mod32_GE( pktTS, inSession->flushTimeoutTS ) )
		{
			inSession->flushing = false;
			atr_ulog( kLogLevelInfo, "Flush complete ts=%u until=%u\n", pktTS, inSession->flushUntilTS );
		}
	}
	
	// Insert the new node. Start at the end since it'll usually be at or near the end. Drop if time slot already taken.
	
	stop = inSession->busyListSentinel;
	for( curr = stop->prev; ( curr != stop ) && Mod32_GT( curr->rtp->header.ts, pktTS ); curr = curr->prev ) {}
	if( ( curr != stop ) && ( curr->rtp->header.ts == pktTS ) )
	{
		dlogassert( "Duplicate timestamp not caught earlier? seq %u ts %u", pktSeq, pktTS );
		err = kDuplicateErr;
		goto exit;
	}
	inNode->prev		= curr;
	inNode->next		= curr->next;
	inNode->next->prev	= inNode;
	inNode->prev->next	= inNode;
	++inSession->busyNodeCount;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_GeneralAudioGenerateAADForPacket
//===========================================================================================================================

static OSStatus
	_GeneralAudioGenerateAADForPacket( 
		const RTPPacket *			inRTPPacket,
		uint8_t **					outAAD, 
		size_t *					outAADSize )
{
	OSStatus			err;
	uint8_t *			aad					= NULL;
	size_t				aadSize				= sizeof( inRTPPacket->header.ts ) + sizeof( inRTPPacket->header.ssrc );
	uint32_t			tsNetworkOrder		= htonl( inRTPPacket->header.ts );
	uint32_t			ssrcNetworkOrder	= htonl( inRTPPacket->header.ssrc );
	
	aad = calloc( 1, aadSize );
	require_action( aad, exit, err = kNoMemoryErr );
	
	memcpy( aad, &tsNetworkOrder, sizeof( tsNetworkOrder ) );
	memcpy( &aad[sizeof( tsNetworkOrder )], &ssrcNetworkOrder, sizeof( ssrcNetworkOrder ) );
	
	*outAAD = aad;
	aad = NULL;
	*outAADSize = aadSize;
	err = kNoErr;

exit:
	ForgetMem( &aad );
	return( err );
}

//===========================================================================================================================
//	_GeneralAudioDecryptPacket
//===========================================================================================================================

static OSStatus
	_GeneralAudioDecryptPacket( 
		AirPlayReceiverSessionRef	inSession,
		uint8_t *					inAAD, 
		size_t						inAADSize, 
		uint8_t *					inSrc, 
		size_t						inSrcSize, 
		uint8_t *					inDst, 
		size_t *					outSize )
{
	OSStatus							err;
	size_t								len = 0;
	AirPlayAudioStreamContext * const	ctx = &inSession->mainAudioCtx;
		
	if( ctx->outputCryptor.isValid )
	{
		// The last 24 bytes of the payload are nonce and the auth tag. Both are LE. The rest of the payload is BE.
			
		chacha20_poly1305_init_64x64( &ctx->outputCryptor.state, ctx->outputCryptor.key, &inSrc[ inSrcSize - 8 ] );
		chacha20_poly1305_add_aad( &ctx->outputCryptor.state, inAAD, inAADSize );
		len = chacha20_poly1305_decrypt( &ctx->outputCryptor.state, inSrc, inSrcSize - 24, inDst );
		len += chacha20_poly1305_verify( &ctx->outputCryptor.state, &inDst[ len ], &inSrc[ inSrcSize - 24 ], &err );
		require_noerr( err, exit );
		require_action( len == inSrcSize - 24, exit, err = kInternalErr );

		// Increment nonce - for debugging only
		
		LittleEndianIntegerIncrement( ctx->outputCryptor.nonce, sizeof( ctx->outputCryptor.nonce ) );
	}
	else
	{
		err = AES_CBCFrame_Update( inSession->decryptor, inSrc, inSrcSize, inDst );
		require_noerr( err, exit );
		len = inSrcSize;
	}
	
	*outSize = len;
	err = kNoErr;

exit:
	return( err );
}

//===========================================================================================================================
//	_GeneralAudioDecodePacket
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static OSStatus
	_GeneralAudioDecodePacket( 
		AirPlayReceiverSessionRef	inSession, 
		uint8_t *					inAAD, 
		size_t						inAADSize,
		uint8_t *					inSrc, 
		size_t						inSrcSize, 
		uint8_t *					inDst, 
		size_t						inDstSize, 
		size_t *					outSize )
{
	OSStatus					err;
	Boolean						decrypt;
	int							decompress;
	size_t						size;
	uint32_t					percent;
	
	// inSrc may be the same as inDst, but otherwise they cannot overlap. Neither can overlap the temp buffer.
	
	if( inSrc != inDst ) check_ptr_overlap( inSrc, inSrcSize, inDst, inDstSize );
	check_ptr_overlap( inSrc, inSrcSize, inSession->decodeBuffer, inSession->decodeBufferSize );
	check_ptr_overlap( inDst, inDstSize, inSession->decodeBuffer, inSession->decodeBufferSize );

	decrypt = ( inSession->decryptor != NULL );
	decrypt = ( decrypt || inSession->mainAudioCtx.outputCryptor.isValid );
	decompress = ( inSession->compressionType != kAirPlayCompressionType_PCM );
	if( decrypt && decompress ) // Encryption and Compression (most common case)
	{
		// Decrypt into a temp buffer then decompress back into the node buffer. Note: decoder result is in host byte order.
		
		require_action( inSrcSize <= inSession->decodeBufferSize, exit, err = kSizeErr );
		
		err = _GeneralAudioDecryptPacket( 
				inSession,
				inAAD, 
				inAADSize, 
				inSrc, 
				inSrcSize, 
				inSession->decodeBuffer, 
				&size );
		require_noerr( err, exit );
		inSrcSize = size;
		
		size = 0;
		err = _AudioDecoderDecodeFrame( inSession, inSession->decodeBuffer, inSrcSize, inDst, inDstSize, &size );
		require_noerr( err, exit );
	}
	else if( decrypt ) // Encryption only
	{
		// Decrypt into a temp buffer then swap to host byte order back into the node buffer.
		
		require_action( inSrcSize <= inSession->decodeBufferSize, exit, err = kSizeErr );
		
		err = _GeneralAudioDecryptPacket( 
				inSession,
				inAAD, 
				inAADSize, 
				inSrc, 
				inSrcSize, 
				inSession->decodeBuffer, 
				&size );
		require_noerr( err, exit );
		
		BigToHost16Mem( inSession->decodeBuffer, size, inDst );
	}
	else if( decompress ) // Compression only
	{
		// Decompress into a temp buffer then copy back into the node buffer. Note: decoder result is in host byte order.
		// This does an extra memcpy due to the lack of in-place decoding, but this case is not normally used.
		
		size = 0;
		err = _AudioDecoderDecodeFrame( inSession, inSrc, inSrcSize, inSession->decodeBuffer, 
			inSession->decodeBufferSize, &size );
		require_noerr( err, exit );
		
		require_action( size <= inDstSize, exit, err = kSizeErr );
		memcpy( inDst, inSession->decodeBuffer, size );
	}
	else // No Encryption or Compression (raw data)
	{
		require_action( inSrcSize <= inDstSize, exit, err = kSizeErr );
		
		BigToHost16Mem( inSrc, inSrcSize, inDst );
		size = inSrcSize;
	}
	
	// Track the amount of compression we're getting.
	
	percent = ( size > 0 ) ? ( (uint32_t)( ( inSrcSize * ( 100 * 100 ) ) / size ) ) : 0;
	inSession->compressionPercentAvg = ( ( inSession->compressionPercentAvg * 63 ) + percent ) / 64;
	
	*outSize = size;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_GeneralAudioRender
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static void	_GeneralAudioRender( AirPlayReceiverSessionRef inSession, uint32_t inRTPTime, void *inBuffer, size_t inSize )
{
	AirPlayAudioStreamContext * const		ctx = &inSession->mainAudioCtx;
	AirTunesSource * const					src = &inSession->source;
	uint32_t const							bytesPerUnit = ctx->bytesPerUnit;
	uint32_t								nowTS, limTS, maxTS, srcTS, endTS, pktTS, delta;
	uint16_t								pktSeq, pktGap;
	Boolean									some;
	uint8_t *								dst;
	uint8_t *								lim;
	int										glitchCount;
	AirTunesBufferNode *					curr;
	AirTunesBufferNode *					stop;
	AirTunesBufferNode *					next;
	int										cap;
	size_t									size;
	OSStatus								err;
	
	if( ctx->type == kAirPlayStreamType_MainHighAudio )
	{
		// Save the first RTP time value as a zero-offset for all future timestamps
		if( ctx->sampleTimeOffset == 0 && ( 0 < inSession->busyNodeCount ) )
		{
			atr_ulog( kLogLevelInfo, "busyNodeCount %d set sampleTimeOffset to %d\n",
					inSession->busyNodeCount,
					inRTPTime );
			ctx->sampleTimeOffset = inRTPTime;
		}
	}
	
	nowTS	= inRTPTime - ctx->sampleTimeOffset;
	limTS	= nowTS + (uint32_t)( inSize / bytesPerUnit );
	maxTS	= limTS + kAirTunesRTPOffsetApplyThreshold;
	pktSeq	= 0;
	some	= false;
	dst		= (uint8_t *) inBuffer;
	lim		= dst + inSize;
	glitchCount = 0;
	
	// Process all applicable packets for this timing window.
	
	stop = inSession->busyListSentinel;
	for( curr = stop->next; curr != stop; curr = next )
	{
		next = curr->next;
		
		// Apply the new RTP offset if we have a pending reset and we've reached the time when we should apply it.
		
		pktTS = curr->ts;
		if( src->rtpOffsetApply && Mod32_GE( pktTS, src->rtpOffsetApplyTimestamp ) )
		{
			atr_ulog( kLogLevelTrace, "Applying RTP offset %u (was=%u, ts=%u, apply=%u)\n", 
				src->rtpOffset, src->rtpOffsetActive, pktTS, src->rtpOffsetApplyTimestamp );
			
			src->rtpOffsetActive			= src->rtpOffset;
			src->rtpOffsetApply				= false;
			inSession->stutterCreditPending = true;
		}
		
		// Calculate the desired playout time of this packet and exit if we haven't reached its play time yet.
		// Packets are sorted by timestamp so if this packet isn't ready yet, none after it can be either.
		// If the apply time is way in the future, apply immediately since there was probably a timeline reset.

        srcTS = pktTS + src->rtpOffsetActive + inSession->audioLatencyOffset;
		if( src->rtpOffsetApply && Mod32_GT( srcTS, maxTS ) )
		{
			atr_ulog( kLogLevelNotice, "Force apply RTP offset (srcTS=%u maxTS=%u)\n", srcTS, maxTS );
			
			src->rtpOffsetActive			= src->rtpOffset;
			src->rtpOffsetApply				= false;
			inSession->stutterCreditPending = true;
			
			srcTS = pktTS + src->rtpOffsetActive + inSession->audioLatencyOffset;
		}
		if( Mod32_GE( srcTS, limTS ) ) break;
		some = true;
		
		// Track packet losses.
		
		pktSeq = curr->rtp->header.seq;
		if( inSession->lastPlayedValid && ( ( (int16_t)( pktSeq - inSession->lastPlayedSeq ) ) > 1 ) )
		{
			pktGap = ( pktSeq - inSession->lastPlayedSeq ) - 1;
			gAirPlayAudioStats.unrecoveredPackets += pktGap;
			atr_stats_ulog( kLogLevelNotice, "### Unrecovered packets: %u-%u (%u) %u total\n", 
				inSession->lastPlayedSeq + 1, pktSeq, pktGap, gAirPlayAudioStats.unrecoveredPackets );
		}
		inSession->lastPlayedTS		= pktTS;
		inSession->lastPlayedSeq	= pktSeq;
		inSession->lastPlayedValid	= true;
		
		// Decrypt and decompress the packet (if we haven't already on a prevous pass).
		
		if( curr->ptr == curr->rtp->payload )
		{
			uint8_t *	aad		= NULL;
			size_t		aadSize	= 0;
			err = _GeneralAudioGenerateAADForPacket( curr->rtp, &aad, &aadSize );
			require_noerr( err, exit );
			
			err = _GeneralAudioDecodePacket( inSession, aad, aadSize, curr->ptr, curr->size, curr->ptr, 
				inSession->nodeBufferSize - kRTPHeaderSize, &curr->size );
			free( aad );
			if( err || ( curr->size == 0 ) )
			{
				AirTunesFreeBufferNode( inSession, curr );
				continue;
			}
		}
		
		// If the packet is too old, free it and move to the next packet.
		
		endTS = srcTS + (uint32_t)( curr->size / bytesPerUnit );
		if( Mod32_LE( endTS, nowTS ) )
		{
			gAirPlayAudioStats.latePackets += 1;
			atr_stats_ulog( kLogLevelNotice, "Discarding late packet: seq %u ts %u-%u (%u ms), %u total\n", 
				pktSeq, nowTS, srcTS, AirTunesSamplesToMs( nowTS - srcTS ), gAirPlayAudioStats.latePackets );
			
			_RetransmitsAbortOne( inSession, pktSeq, "OLD" );
			AirTunesFreeBufferNode( inSession, curr );
			continue;
		}
		
		// This packet has at least some samples within the timing window. If the packet starts after the current 
		// time then there's a gap (e.g. packet loss) so we need to conceal the gap with simulated data.
		
		if( Mod32_LT( nowTS, srcTS ) )
		{
			delta = srcTS - nowTS;
			atr_stats_ulog( kLogLevelVerbose, "Concealed %d unit gap (%u vs %u), curr seq %u\n", delta, nowTS, srcTS, pktSeq );
			
			_RetransmitsAbortOne( inSession, pktSeq, "GAP" );
			size = delta * bytesPerUnit;
			check_ptr_bounds( inBuffer, inSize, dst, size );
			_AudioDecoderConcealLoss( inSession, dst, size );
			dst   += size;
			nowTS += delta;
			++glitchCount;
		}
		
		// If the packet has samples before the timing window then skip those samples because they're too late.
		
		if( Mod32_LT( srcTS, nowTS ) )
		{
			gAirPlayAudioStats.latePackets += 1;
			atr_stats_ulog( kLogLevelVerbose, "Dropped %d late units (%u vs %u), %u total\n", 
				Mod32_Diff( nowTS, srcTS ), srcTS, nowTS, gAirPlayAudioStats.latePackets );
			
			_RetransmitsAbortOne( inSession, pktSeq, "LATE" );
			delta		= nowTS - srcTS;
			size		= delta * bytesPerUnit;
			curr->ptr	+= size;
			curr->size	-= size;
			curr->ts	+= delta;
			srcTS		= nowTS;
			++glitchCount;
		}
		
		// Copy the packet data into the playout buffer. If all of the data doesn't fit in our buffer then only
		// copy as much as we can and adjust the packet (pointers, sizes, timestamps) so we process the rest of 
		// it next time. If we processed the whole packet then move it to the free list so it can be reused.
		
		cap = Mod32_GT( endTS, limTS );
		if( cap ) endTS = limTS;
		
		delta = endTS - srcTS;
		size  = delta * bytesPerUnit;
		check_ptr_bounds( curr->data, inSession->nodeBufferSize, curr->ptr, size );
		check_ptr_bounds( inBuffer, inSize, dst, size );
		memcpy( dst, curr->ptr, size );
		dst   += size;
		nowTS += delta;
		
		if( cap )
		{
			curr->ptr	+= size;
			curr->size	-= size;
			curr->ts	+= delta;
			check_ptr_bounds( curr->data, inSession->nodeBufferSize, curr->ptr, curr->size );
			check_string( curr->size > 0, "capped packet to fit, but nothing left over?" );
			check_string( dst == lim, "capped packet, but more room in buffer?" );
			break;
		}
		else
		{
			AirTunesFreeBufferNode( inSession, curr );
		}
		if( dst >= lim ) break;
	}
	
exit:
	// If there wasn't enough data to fill the entire buffer then fill the remaining space with simulated data.
	
	if( Mod32_LT( nowTS, limTS ) )
	{
		delta = limTS - nowTS;
		size = delta * bytesPerUnit;
		check_ptr_bounds( inBuffer, inSize, dst, size );
		_AudioDecoderConcealLoss( inSession, dst, size );
		dst   += size;
		nowTS += delta;
		++glitchCount;
		
		atr_ulog( kLogLevelChatty, "Concealed %d units at end (ts=%u)\n", delta, nowTS );
		
		if( some ) _RetransmitsAbortOne( inSession, pktSeq, "UNDERRUN" );
	}
	check( dst == lim );
	check( nowTS == limTS );
	
	// Update to account for glitches. This tries to be conservative and ignore glitches due to no packets, 
	// flushing, or when we expect a glitch (RTP reset). If there's too many glitches, use silence.
	
	if( !some || inSession->flushing )
	{
		glitchCount = 0;
	}
	else if( ( glitchCount > 0 ) && inSession->stutterCreditPending )
	{
		glitchCount = 0;
		inSession->stutterCreditPending = false;
	}
	inSession->glitchTotal += glitchCount;
	if( glitchCount > 0 )
	{
		atr_ulog( kLogLevelChatty, "Glitch: %d new, %u session\n", glitchCount, inSession->glitchTotal );
	}
	
	// Update stats.
	
	if( inSession->lastPlayedValid && log_category_enabled( atr_stats_ucat(), kLogLevelNotice ) )
	{
		int samples = (int)( inSession->lastRTPTS - inSession->lastPlayedTS );
		EWMA_FP_Update( &gAirPlayAudioStats.bufferAvg, samples );
		atr_ulog( kLogLevelChatty, "RTP Buffer: %3d ms, %3.2f ms avg\n", 
			AirTunesSamplesToMs( samples ), AirTunesSamplesToMs( EWMA_FP_Get( &gAirPlayAudioStats.bufferAvg ) ) );
	}
}

//===========================================================================================================================
//	_GeneralAudioTrackDups
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static Boolean	_GeneralAudioTrackDups( AirPlayReceiverSessionRef inSession, uint16_t inSeq )
{
	size_t		i;
	int			diff;
	
	if( inSession->rtpAudioDupsInitialized )
	{
		diff = Mod16_Cmp( inSeq, inSession->rtpAudioDupsLastSeq );
		if(      diff  > 0 )						{}
		else if( diff == 0 )						goto dup;
		else if( diff <= -kAirTunesDupWindowSize )	goto dup;
		
		i = inSeq % kAirTunesDupWindowSize;
		if( inSession->rtpAudioDupsArray[ i ] == inSeq ) goto dup;
		inSession->rtpAudioDupsArray[ i ] = inSeq;
	}
	else
	{
		for( i = 0; i < kAirTunesDupWindowSize; ++i )
		{
			inSession->rtpAudioDupsArray[ i ] = inSeq;
		}
		inSession->rtpAudioDupsInitialized = true;
	}
	inSession->rtpAudioDupsLastSeq = inSeq;
	return( false );
	
dup:
	if( !inSession->redundantAudio )
	{
		atr_stats_ulog( kLogLevelInfo, "### Duplicate packet seq %u\n", inSeq );
	}
	return( true );
}

//===========================================================================================================================
//	_GeneralAudioTrackLosses
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static void	_GeneralAudioTrackLosses( AirPlayReceiverSessionRef inSession, AirTunesBufferNode *inNode )
{
	Boolean			updateLast;
	uint16_t		seqCurr;
	uint16_t		seqNext;
	uint16_t		seqLoss;
	
	updateLast = true;
	seqCurr = inNode->rtp->header.seq;
	if( inSession->source.receiveCount > 0 )
	{
		seqNext = inSession->lastRTPSeq + 1;
		if( seqCurr == seqNext )
		{
			// Expected case of the receiving the next packet in the sequence so nothing special needs to be done.
		}
		else if( Mod16_GT( seqCurr, seqNext ) )
		{
			seqLoss = seqCurr - seqNext;
			gAirPlayAudioStats.lostPackets += seqLoss;
			if( seqLoss > inSession->source.maxBurstLoss ) inSession->source.maxBurstLoss = seqLoss;
			if( seqLoss <= inSession->source.retransmitMaxLoss )
			{
				atr_stats_ulog( kLogLevelNotice, "### Lost packets %u-%u (+%u, %u total)\n", 
					seqNext, seqCurr, seqLoss, gAirPlayAudioStats.lostPackets );
				if( !inSession->source.rtcpRTDisable ) _RetransmitsSchedule( inSession, seqNext, seqLoss );
			}
			else
			{
				atr_ulog( kLogLevelNotice, "### Burst packet loss %u-%u (+%u, %u total)\n", 
					seqNext, seqCurr, seqLoss, gAirPlayAudioStats.lostPackets );
				++inSession->source.bigLossCount;
				_RetransmitsAbortAll( inSession, "BURST" );
			}
		}
		else if( seqCurr == inSession->lastRTPSeq )
		{
			dlogassert( "Duplicate sequence number not caught earlier? seq %u", seqCurr );
			updateLast = false;
		}
		else if( inSession->redundantAudio )
		{
			updateLast = false; // Don't track misorders since they are intentional in redundant mode.
		}
		else
		{
			atr_ulog( kLogLevelNotice, "### Misordered packet seq %u -> %u\n", inSession->lastRTPSeq, seqCurr );
			updateLast = false;
		}
	}
	if( updateLast )
	{
		inSession->lastRTPSeq = seqCurr;
		inSession->lastRTPTS  = inNode->rtp->header.ts;
	}
}

//===========================================================================================================================
//	_GeneralAudioUpdateLatency
//===========================================================================================================================

static void	_GeneralAudioUpdateLatency( AirPlayReceiverSessionRef inSession )
{
	inSession->audioLatencyOffset = inSession->minLatency + gAirTunesRelativeTimeOffset;
	
	atr_ulog( kLogLevelVerbose, "Audio Latency Offset %u, Sender %d, Relative %d\n",
		inSession->audioLatencyOffset, inSession->minLatency, gAirTunesRelativeTimeOffset );
}

//===========================================================================================================================
//	_RetransmitsSendRequest
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static OSStatus	_RetransmitsSendRequest( AirPlayReceiverSessionRef inSession, uint16_t inSeqStart, uint16_t inSeqCount )
{
	OSStatus						err;
	RTCPRetransmitRequestPacket		pkt;
	ssize_t							n;
	size_t							size;
	
	size = kRTCPRetransmitRequestPacketMinSize;
	pkt.v_p			= RTCPHeaderInsertVersion( 0, kRTPVersion );
	pkt.pt			= kRTCPTypeRetransmitRequest;
	pkt.length		= htons( (uint16_t)( ( size / 4 ) - 1 ) );
	pkt.seqStart	= htons( inSeqStart );
	pkt.seqCount	= htons( inSeqCount );

	if( inSession->rtcpConnected )
	{
		n = send( inSession->rtcpSock, (char *) &pkt, size, 0 );
	}
	else
	{
		n = sendto( inSession->rtcpSock, (char *) &pkt, size, 0, &inSession->rtcpRemoteAddr.sa, inSession->rtcpRemoteLen );
	}
	err = map_socket_value_errno( inSession->rtcpSock, n == (ssize_t) size, n );
	require_noerr( err, exit );
	++inSession->source.retransmitSendCount;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_RetransmitsProcessResponse
//===========================================================================================================================

static OSStatus	_RetransmitsProcessResponse( AirPlayReceiverSessionRef inSession, RTCPRetransmitResponsePacket *inPkt, size_t inSize )
{
	OSStatus		err;
	
	if( inSize == ( offsetof( RTCPRetransmitResponsePacket, payload ) + sizeof_field( RTCPRetransmitResponsePacket, payload.fail ) ) )
	{
		inPkt->payload.fail.seq = ntohs( inPkt->payload.fail.seq );
		_SessionLock( inSession );
		_RetransmitsAbortOne( inSession, inPkt->payload.fail.seq, "FUTILE" );
		_SessionUnlock( inSession );
		++inSession->source.retransmitFutileCount;
		err = kNoErr;
		goto exit;
	}
	if( inSize < ( offsetof( RTCPRetransmitResponsePacket, payload ) + offsetof( RTPPacket, payload ) ) )
	{
		dlogassert( "Retransmit packet too small (%zu bytes)", inSize );
		err = kSizeErr;
		goto exit;
	}
	
	err = _GeneralAudioReceiveRTP( inSession, &inPkt->payload.rtp, 
		inSize - offsetof( RTCPRetransmitResponsePacket, payload ) );
	++inSession->source.retransmitReceiveCount;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_RetransmitsSchedule
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static void	_RetransmitsSchedule( AirPlayReceiverSessionRef inSession, uint16_t inSeqStart, uint16_t inSeqCount )
{
	uint16_t						i;
	AirTunesRetransmitNode **		next;
	AirTunesRetransmitNode *		node;
	uint64_t						nowNanos;
	
	for( next = &inSession->source.rtcpRTBusyList; *next; next = &( *next )->next ) {}
	nowNanos = UpNanoseconds();
	for( i = 0; i < inSeqCount; ++i )
	{
		// Get a free node.
		
		node = inSession->source.rtcpRTFreeList;
		if( !node )
		{
			atr_stats_ulog( kLogLevelWarning, "### No free retransmit nodes, dropping retransmit of seq %u#%u, %u\n", 
				inSeqStart, inSeqCount, i );
			break;
		}
		inSession->source.rtcpRTFreeList = node->next;
		
		// Schedule a retransmit request.
		
		node->next			= NULL;
		node->seq			= inSeqStart + i;
		node->tries			= 0;
		node->startNanos	= nowNanos;
		node->nextNanos		= nowNanos;
		*next = node;
		 next = &node->next;
	}
}

//===========================================================================================================================
//	_RetransmitsUpdate
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static void	_RetransmitsUpdate( AirPlayReceiverSessionRef inSession, AirTunesBufferNode *inNode, Boolean inIsRetransmit )
{
	AirTunesSource * const			ats = &inSession->source;
	uint16_t						pktSeq;
	AirTunesRetransmitNode **		next;
	AirTunesRetransmitNode *		curr;
	uint64_t						nowNanos;
	uint64_t						ageNanos;
	int64_t							rttNanos;
	int								credits;
	Boolean							outlier;
	
	nowNanos = UpNanoseconds();
	pktSeq   = inNode->rtp->header.seq;
	
	// Search for the completed retransmit.
	
	for( next = &ats->rtcpRTBusyList; ( ( curr = *next ) != NULL ) && ( curr->seq != pktSeq ); next = &curr->next ) {}
	if( curr )
	{
		if( inIsRetransmit )
		{
			ageNanos = nowNanos - curr->startNanos;
			if( ageNanos < ats->retransmitMinNanos ) ats->retransmitMinNanos = ageNanos;
			if( ageNanos > ats->retransmitMaxNanos ) ats->retransmitMaxNanos = ageNanos;
			if( ( ageNanos > ats->retransmitMinNanos ) && ( ageNanos < ats->retransmitMaxNanos ) )
			{
				ats->retransmitAvgNanos = ( ( ats->retransmitAvgNanos * 63 ) + ageNanos ) / 64;
			}
			// If multiple requests have gone out, this may be a response to an earlier request and not the most
			// recent one so following Karn's algorithm, only consider the RTT if we've only sent 1 request.
			
			if( curr->tries <= 1 )
			{
				outlier = false;
				rttNanos = nowNanos - curr->sentNanos;
				if( rttNanos < ats->rtcpRTMinRTTNanos ) { ats->rtcpRTMinRTTNanos = rttNanos; outlier = true; }
				if( rttNanos > ats->rtcpRTMaxRTTNanos ) { ats->rtcpRTMaxRTTNanos = rttNanos; outlier = true; }
				if( !outlier )
				{
					int64_t		errNanos;
					int64_t		absErrNanos;
					
					errNanos				= rttNanos - ats->rtcpRTAvgRTTNanos;
					absErrNanos				= ( errNanos < 0 ) ? -errNanos : errNanos;
					ats->rtcpRTAvgRTTNanos	= ats->rtcpRTAvgRTTNanos + ( errNanos / 8 );
					ats->rtcpRTDevRTTNanos	= ats->rtcpRTDevRTTNanos + ( ( absErrNanos - ats->rtcpRTDevRTTNanos ) / 4 );
					ats->rtcpRTTimeoutNanos	= ( 2 * ats->rtcpRTAvgRTTNanos ) + ( 4 * ats->rtcpRTDevRTTNanos );
					if( ats->rtcpRTTimeoutNanos > 100000000 ) // Cap at 100 ms
					{
						ats->rtcpRTTimeoutNanos = 100000000;
					}
				}
			}
		}
		
		*next = curr->next;
		curr->next = ats->rtcpRTFreeList;
		ats->rtcpRTFreeList = curr;
	}
	else if( inIsRetransmit )
	{
		atr_stats_ulog( kLogLevelInfo, "### Retransmit seq %u not found\n", pktSeq );
		++ats->retransmitNotFoundCount;
	}
	
	// Retry retransmits that have timed out.
	
	credits = 3;
	for( curr = ats->rtcpRTBusyList; curr; curr = curr->next )
	{
		if( nowNanos < curr->nextNanos ) continue;
		ageNanos = nowNanos - curr->startNanos;
		if( curr->tries++ > 0 )
		{
			if( ageNanos < ats->retransmitRetryMinNanos ) ats->retransmitRetryMinNanos = ageNanos;
			if( ageNanos > ats->retransmitRetryMaxNanos ) ats->retransmitRetryMaxNanos = ageNanos;
		}
		curr->sentNanos = nowNanos;
		curr->nextNanos = nowNanos + ats->rtcpRTTimeoutNanos;
		_RetransmitsSendRequest( inSession, curr->seq, 1 );
		if( --credits == 0 ) break;
	}
}

//===========================================================================================================================
//	_RetransmitsAbortAll
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static void	_RetransmitsAbortAll( AirPlayReceiverSessionRef inSession, const char *inReason )
{
	AirTunesSource * const			ats = &inSession->source;
	AirTunesRetransmitNode *		curr;
	
	if( ats->rtcpRTBusyList ) atr_stats_ulog( kLogLevelInfo, "### Aborting all retransmits (%s)\n", inReason );
	
	while( ( curr = ats->rtcpRTBusyList ) != NULL )
	{
		ats->rtcpRTBusyList = curr->next;
		curr->next = ats->rtcpRTFreeList;
		ats->rtcpRTFreeList = curr;
	}
}

//===========================================================================================================================
//	_RetransmitsAbortOne
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static void	_RetransmitsAbortOne( AirPlayReceiverSessionRef inSession, uint16_t inSeq, const char *inReason )
{
	AirTunesSource * const			ats = &inSession->source;
	AirTunesRetransmitNode **		next;
	AirTunesRetransmitNode *		curr;
	uint64_t						nowNanos;
	
	if( ats->rtcpRTBusyList ) atr_ulog( kLogLevelInfo, "### Aborting retransmits <= %u (%s)\n", inSeq, inReason );
	
	nowNanos = UpNanoseconds();
	for( next = &ats->rtcpRTBusyList; ( curr = *next ) != NULL; )
	{
		if( Mod16_LE( curr->seq, inSeq ) )
		{
			atr_ulog( kLogLevelVerbose, "    ### Abort retransmit %5u  T %2u  A %10llu \n", 
				curr->seq, curr->tries, nowNanos - curr->startNanos  );
			
			*next = curr->next;
			curr->next = ats->rtcpRTFreeList;
			ats->rtcpRTFreeList = curr;
			continue;
		}
		next = &curr->next;
	}
}

#if 0
#pragma mark -
#pragma mark == MainAltAudio ==
#endif

//===========================================================================================================================
//	_MainAudioSetup
//===========================================================================================================================

static OSStatus
	_MainAltAudioSetup(
		AirPlayReceiverSessionRef	inSession, 
		AirPlayStreamType			inType, 
		CFDictionaryRef				inRequestStreamDesc, 
		CFMutableDictionaryRef		inResponseParams )
{
	OSStatus						err;
	AirPlayAudioStreamContext *		ctx;
	const char *					label;
	CFMutableDictionaryRef			responseStreamDesc = NULL;
	AirPlayAudioFormat				format;
	AudioStreamBasicDescription		encodedASBD, decodedASBD;
	int								receivePort, sendPort = 0;
	int64_t							bufferMs;
	uint64_t						streamConnectionID = 0;
	uint8_t							outputKey[ 32 ];
	uint8_t							inputKey[ 32 ];
	
	switch( inType )
	{
		case kAirPlayStreamType_MainAudio:
			ctx = &inSession->mainAudioCtx;
			label = "Main";
			break;
		
		case kAirPlayStreamType_AltAudio:
			ctx = &inSession->altAudioCtx;
			label = "Alt";
			break;
		
		default:
			dlogassert( "Bad stream type: %u", inType );
			label = "<< Bad >>";
			err = kParamErr;
			goto exit2;
	}
	require_action( !ctx->session, exit2, err = kAlreadyInitializedErr );
	ctx->type  = inType;
	ctx->label = label;
	
	streamConnectionID = (uint64_t) CFDictionaryGetInt64( inRequestStreamDesc, CFSTR( kAirPlayKey_StreamConnectionID ), &err );
	if( err )
	{
		UUIDData		uuid;
		
		UUIDGet( &uuid );
		ctx->connectionID = ReadBig64( uuid.bytes );
		err = kNoErr;
	}
	else
	{
		ctx->connectionID = streamConnectionID;
	}
	
	if( inSession->pairVerifySession )	require_action( streamConnectionID, exit, err = kVersionErr );
	
	responseStreamDesc = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( responseStreamDesc, exit, err = kNoMemoryErr );
	
	err = OpenSelfConnectedLoopbackSocket( &ctx->cmdSock );
	require_noerr( err, exit );
	
	// Set up to receive audio from the sender.
	
	format = (AirPlayAudioFormat) CFDictionaryGetInt64( inRequestStreamDesc, CFSTR( kAirPlayKey_AudioFormat ), NULL );
	if( format == kAirPlayAudioFormat_Invalid ) format = kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo;
	err = AirPlayAudioFormatToASBD( format, &encodedASBD, &ctx->bitsPerSample );
	require_noerr( err, exit );
	AirPlayAudioFormatToPCM( format, &decodedASBD );
	
	ctx->sampleRate		= (uint32_t) decodedASBD.mSampleRate;
	ctx->channels		= decodedASBD.mChannelsPerFrame;
	ctx->bytesPerUnit	= decodedASBD.mBytesPerFrame;
	if( ctx->bytesPerUnit == 0 ) ctx->bytesPerUnit = ( RoundUp( ctx->bitsPerSample, 8 ) * ctx->channels ) / 8;
	ctx->framesPerPacket= encodedASBD.mFramesPerPacket;
	
	ctx->rateUpdateNextTicks		= 0;
	ctx->rateUpdateIntervalTicks	= SecondsToUpTicks( 1 );
	ctx->rateUpdateCount			= 0;
	ctx->rateAvg					= (Float32) ctx->sampleRate;
	
	err = pthread_mutex_init( &ctx->zeroTimeLock, NULL );
	require_noerr( err, exit );
	ctx->zeroTimeLockPtr = &ctx->zeroTimeLock;
	
	ctx->inputTimestamp		        = 0;
	
	bufferMs = CFDictionaryGetInt64( inRequestStreamDesc, CFSTR( kAirPlayKey_AudioLatencyMs ), &err );
	if( err || ( bufferMs < 0 ) )
	{
		if( inSession->transportType == kNetTransportType_DirectLink )
		{
			bufferMs = kAirPlayAudioBufferMainAltWiredMs;
		}
		else
		{
			bufferMs = kAirPlayAudioBufferMainAltWiFiMs;
		}
	}
	
	if( bufferMs < kAirPlayAudioBufferMinMs )
		bufferMs = kAirPlayAudioBufferMinMs;
	
	ctx->compressionType = AudioFormatIDToAirPlayCompressionType( encodedASBD.mFormatID );
	require_action( ctx->compressionType != kAirPlayCompressionType_Undefined, exit, err = kUnknownErr );

	MemZeroSecure( &ctx->outputCryptor, sizeof( ctx->outputCryptor ) );
	if( streamConnectionID )
	{
		err = _GetStreamSecurityKeys( inSession, streamConnectionID, 32, inputKey, 32, outputKey );
		require_noerr( err, exit );
		
		memset( ctx->outputCryptor.nonce, 0, sizeof( ctx->outputCryptor.nonce ) );
		memcpy( ctx->outputCryptor.key, outputKey, 32 );
		ctx->outputCryptor.isValid = true;
	}
	
	err = RTPJitterBufferInit( &ctx->jitterBuffer, &encodedASBD, &decodedASBD, (uint32_t) bufferMs );
	require_noerr( err, exit );
	ctx->jitterBuffer.label = label;
	
	err = ServerSocketOpen( inSession->peerAddr.sa.sa_family, SOCK_DGRAM, IPPROTO_UDP,
			inType == kAirPlayStreamType_MainAudio ? -kAirPlayPort_RTPAudio : -kAirPlayPort_RTPAltAudio,
			&receivePort, kSocketBufferSize_DontSet, &ctx->dataSock );
	require_noerr( err, exit );
	
	SocketSetQoS( ctx->dataSock, kSocketQoS_Voice );
	
	CFDictionarySetInt64( responseStreamDesc, CFSTR( kAirPlayKey_Type ), inType );
	CFDictionarySetInt64( responseStreamDesc, CFSTR( kAirPlayKey_StreamConnectionID ), ctx->connectionID );
	CFDictionarySetInt64( responseStreamDesc, CFSTR( kAirPlayKey_Port_Data ), receivePort );
	
	// If the sender provided its own port number then set up to send input audio to it.
	
	if( inType == kAirPlayStreamType_MainAudio )
	{
		sendPort = (int) CFDictionaryGetInt64( inRequestStreamDesc, CFSTR( kAirPlayKey_Port_Data ), NULL );
		if( sendPort > 0 )
		{
			if( ctx->compressionType != kAirPlayCompressionType_PCM )
			{
				uint32_t u32;

				err = AudioConverterNew( &decodedASBD, &encodedASBD, &ctx->inputConverter );
				require_noerr( err, exit );

				// Configure converter packet size limit
				check( encodedASBD.mSampleRate <= 48000 );
				if( encodedASBD.mSampleRate <= 24000 )
					u32 = kAirPlayAudioBitrateLowLatencyUpTo24KHz / (uint32_t)(encodedASBD.mSampleRate / encodedASBD.mFramesPerPacket);
				else if( encodedASBD.mSampleRate <= 32000 )
					u32 = kAirPlayAudioBitrateLowLatencyUpTo32KHz / (uint32_t)(encodedASBD.mSampleRate / encodedASBD.mFramesPerPacket);
				else
					u32 = kAirPlayAudioBitrateLowLatencyUpTo48KHz / (uint32_t)(encodedASBD.mSampleRate / encodedASBD.mFramesPerPacket);

				err = AudioConverterSetProperty( ctx->inputConverter, kAudioCodecPropertyPacketSizeLimitForVBR, sizeof( u32 ), &u32 );
				require_noerr( err, exit );
			}

			MemZeroSecure( &ctx->inputCryptor, sizeof( ctx->inputCryptor ) );
			if( streamConnectionID )
			{
				memset( ctx->inputCryptor.nonce, 0, sizeof( ctx->inputCryptor.nonce ) );
				memcpy( ctx->inputCryptor.key, inputKey, 32 );
				ctx->inputCryptor.isValid = true;
			}
			if( !inSession->inputRingRef )
			{
				err = MirroredRingBufferInit( &inSession->inputRing, kAirPlayInputRingSize, true );
				require_noerr( err, exit );
				inSession->inputRingRef = &inSession->inputRing;
			}
			else
			{
				MirroredRingBufferReset( inSession->inputRingRef );
			}
			ctx->inputRingRef = inSession->inputRingRef;
			ctx->sendAudioDone = 0;
			
			err = pthread_mutex_init( &ctx->sendAudioMutex, NULL );
			require_noerr( err, exit );
			ctx->sendAudioMutexPtr = &ctx->sendAudioMutex;
			
			err = pthread_cond_init( &ctx->sendAudioCond, NULL );
			require_noerr( err, exit );
			ctx->sendAudioCondPtr = &ctx->sendAudioCond;
			
			err = pthread_create( &ctx->sendAudioThread, NULL, _AudioSenderThread, ctx );
			require_noerr( err, exit );
			ctx->sendAudioThreadPtr = &ctx->sendAudioThread;
			
			RandomBytes( &ctx->inputSeqNum, sizeof( ctx->inputSeqNum ) );
			
			SockAddrCopy( &inSession->peerAddr, &ctx->inputAddr );
			SockAddrSetPort( &ctx->inputAddr, sendPort );
			ctx->inputAddrLen = (socklen_t) SockAddrGetSize( &ctx->inputAddr );
		}
	}
	
	err = _AddResponseStream( inResponseParams, responseStreamDesc );
	require_noerr( err, exit );
	
	ctx->session = inSession;
	atr_ulog( kLogLevelTrace, "%s audio set up for %s on receive port %d, send port %d\n", 
		ctx->label, AirPlayAudioFormatToString( format ), receivePort, sendPort );
	
	// If the session's already started then immediately start the thread process it.
	
	if( inSession->sessionStarted && !ctx->threadPtr )
	{
		err = pthread_create( &ctx->thread, NULL, _MainAltAudioThread, ctx );
		require_noerr( err, exit );
		ctx->threadPtr = &ctx->thread;
	}
	
exit:
	CFReleaseNullSafe( responseStreamDesc );
	if( err ) _TearDownStream( inSession, ctx, false );
	
exit2:
	MemZeroSecure( inputKey, sizeof( inputKey ) );
	MemZeroSecure( outputKey, sizeof( outputKey ) );
	if( err ) atr_ulog( kLogLevelWarning, "### %s audio setup failed: %#m\n", label, err );
	return( err );
}

//===========================================================================================================================
//	_MainAltAudioThread
//===========================================================================================================================

static void *	_MainAltAudioThread( void *inArg )
{
	AirPlayAudioStreamContext * const		ctx			= (AirPlayAudioStreamContext *) inArg;
	SocketRef const							dataSock	= ctx->dataSock;
	SocketRef const							cmdSock		= ctx->cmdSock;
	fd_set									readSet;
	int										maxFd;
	int										n;
	OSStatus								err;
	
	SetThreadName( "AirPlayAudioReceiver" );
	SetCurrentThreadPriority( kAirPlayThreadPriority_AudioReceiver );
    atr_ulog( kLogLevelTrace, "MainAltAudio thread starting\n" );
	
	FD_ZERO( &readSet );
	maxFd = -1;
	if( (int) dataSock > maxFd ) maxFd = dataSock;
	if( (int) cmdSock  > maxFd ) maxFd = cmdSock;
	maxFd += 1;
	for( ;; )
	{
		FD_SET( dataSock, &readSet );
		FD_SET( cmdSock,  &readSet );
		n = select( maxFd, &readSet, NULL, NULL, NULL );
		err = select_errno( n );
		if( err == EINTR ) continue;
		if( err ) { dlogassert( "select() error: %#m", err ); usleep( 100000 ); continue; }
		
		if( FD_ISSET( dataSock, &readSet ) ) _MainAltAudioProcessPacket( ctx );
		if( FD_ISSET( cmdSock,  &readSet ) ) break; // The only event is quit so break if anything is pending.
	}
	atr_ulog( kLogLevelTrace, "%s audio thread exit\n", ctx->label );
	return( NULL );
}

//===========================================================================================================================
//	_MainAltAudioProcessPacket
//===========================================================================================================================

static void	_MainAltAudioProcessPacket( AirPlayAudioStreamContext * const ctx )
{
	OSStatus			err;
	RTPPacketNode *		node = NULL;
	size_t				len;
	
	err = RTPJitterBufferGetFreeNode( &ctx->jitterBuffer, &node );
	require_noerr( err, exit );
	
	err = SocketRecvFrom( ctx->dataSock, node->pkt.pkt.bytes, sizeof( node->pkt.pkt.bytes ), &len, 
		NULL, 0, NULL, NULL, NULL, NULL );
	require_noerr( err, exit );
	require_action( len >= kRTPHeaderSize, exit, err = kSizeErr );
	
	node->pkt.len					= len - kRTPHeaderSize;
	node->ptr						= node->pkt.pkt.rtp.payload;
	
	if( ctx->outputCryptor.isValid )
	{
		uint8_t *	aad			= NULL;
		size_t		aadLength	= 0;

		_MainAltAudioGetAADFromRTPHeader( ctx, &node->pkt.pkt.rtp.header, &aad, &aadLength );
		// The last 24 bytes of the payload are nonce and the auth tag. Both are LE. The rest of the payload is BE.
			
		require_action( node->pkt.len >= 24, exit, err = kUnknownErr );
		chacha20_poly1305_init_64x64( &ctx->outputCryptor.state, ctx->outputCryptor.key, &node->ptr[ node->pkt.len - 8 ] );
		chacha20_poly1305_add_aad( &ctx->outputCryptor.state, aad, aadLength );
		len = chacha20_poly1305_decrypt( &ctx->outputCryptor.state, node->ptr, node->pkt.len - 24, node->ptr );
		len += chacha20_poly1305_verify( &ctx->outputCryptor.state, &node->ptr[ len ], &node->ptr[ node->pkt.len - 24 ], &err );
		require_noerr( err, exit );
		require_action( len == node->pkt.len - 24, exit, err = kInternalErr );
		node->pkt.len -= 24;
		
		// Increment nonce - for debugging only
		
		LittleEndianIntegerIncrement( ctx->outputCryptor.nonce, sizeof( ctx->outputCryptor.nonce ) );
	}
	node->pkt.pkt.rtp.header.seq	= ntohs( node->pkt.pkt.rtp.header.seq );
	node->pkt.pkt.rtp.header.ts		= ntohl( node->pkt.pkt.rtp.header.ts );
	node->pkt.pkt.rtp.header.ssrc	= ntohl( node->pkt.pkt.rtp.header.ssrc );
	
	if( ctx->compressionType == kAirPlayCompressionType_PCM )
	{
		BigToHost16Mem( node->ptr, node->pkt.len, node->ptr );
	}

	err = RTPJitterBufferPutBusyNode( &ctx->jitterBuffer, node );
	require_noerr( err, exit );
	node = NULL;
	
exit:
	if( node )	RTPJitterBufferPutFreeNode( &ctx->jitterBuffer, node );
	if( err )	atr_ulog( kLogLevelNotice, "### Process main audio error: %#m\n", err );
}

//===========================================================================================================================
//	_MainAltAudioGetAADFromRTPHeader
//===========================================================================================================================

static OSStatus	_MainAltAudioGetAADFromRTPHeader( AirPlayAudioStreamContext * const ctx, RTPHeader *inRTPHeaderPtr, uint8_t **outAAD, size_t *outAADLength )
{
	if( _CompareOSBuildVersionStrings( "13A1", ctx->session->clientOSBuildVersion ) < 0 )
	{
		*outAAD = &((uint8_t *)inRTPHeaderPtr)[offsetof( RTPHeader, ts )];
		*outAADLength = sizeof( inRTPHeaderPtr->ts ) + sizeof( inRTPHeaderPtr->ssrc );
	}
	else
	{
		*outAAD = (uint8_t *)inRTPHeaderPtr;
		*outAADLength = sizeof( RTPHeader );
	}
	
	return( kNoErr );
}

#if 0
#pragma mark -
#pragma mark == Idle state keep alive ==
#endif

//===========================================================================================================================
//	_IdleStateKeepAliveInitialize
//===========================================================================================================================

static OSStatus	_IdleStateKeepAliveInitialize( AirPlayReceiverSessionRef inSession )
{
	OSStatus		err;
	sockaddr_ip		sip;

	check( !IsValidSocket( inSession->keepAliveSock ) );

	// Set up a socket to receive udp keep alive beacon.

	SockAddrCopy( &inSession->peerAddr, &sip );
	err = ServerSocketOpen( sip.sa.sa_family, SOCK_DGRAM, IPPROTO_UDP, -kAirPlayPort_KeepAlive,
			&inSession->keepAlivePortLocal, kSocketBufferSize_DontSet, &inSession->keepAliveSock );
	require_noerr( err, exit );

	SocketSetQoS( inSession->keepAliveSock, kSocketQoS_Background );

	atr_ulog( kLogLevelTrace, "KeepAlive set up on port %d\n", inSession->keepAlivePortLocal );

exit:
	if( err )
	{
		atr_ulog( kLogLevelWarning, "### Keep alive setup failed: %#m\n", err );
		_IdleStateKeepAliveFinalize( inSession );
	}

	return( err );
}

//===========================================================================================================================
//	_IdleStateKeepAliveStart
//===========================================================================================================================

static OSStatus	_IdleStateKeepAliveStart( AirPlayReceiverSessionRef inSession )
{
	OSStatus		err;

	require_action( IsValidSocket( inSession->keepAliveSock ), exit, err = kNotInitializedErr );

	require_action( NULL == inSession->keepAliveThreadPtr, exit, err = kAlreadyInitializedErr );


	// Set up a socket for sending commands to the thread.

	ForgetSocket( &inSession->keepAliveCmdSock );
	err = OpenSelfConnectedLoopbackSocket( &inSession->keepAliveCmdSock );
	require_noerr( err, exit );

	// Start the keep alive thread to monitor client.

	err = pthread_create( &inSession->keepAliveThread, NULL, _IdleStateKeepAliveThread, inSession );
	require_noerr( err, exit );

	inSession->keepAliveThreadPtr = &inSession->keepAliveThread;

exit:
	if( err ) atr_ulog( kLogLevelWarning, "### Keep alive start failed: %#m\n", err );
	return( err );
}

//===========================================================================================================================
//	_IdleStateKeepAliveStop
//===========================================================================================================================

static OSStatus	_IdleStateKeepAliveStop( AirPlayReceiverSessionRef inSession )
{
	OSStatus		err = kNoErr;

	require_action( IsValidSocket( inSession->keepAliveSock ), exit, err = kNotInitializedErr );

	// Signal the thread to quit and wait for it to signal back that it exited.

	if( inSession->keepAliveThreadPtr )
	{
		err = SendSelfConnectedLoopbackMessage( inSession->keepAliveCmdSock, "q", 1 );
		check_noerr( err );

		err = pthread_join( inSession->keepAliveThread, NULL );
		check_noerr( err );

		inSession->keepAliveThreadPtr = NULL;
	}

exit:
	ForgetSocket( &inSession->keepAliveCmdSock );
	if( err ) atr_ulog( kLogLevelWarning, "### Keep alive stop failed: %#m\n", err );
	return( err );
}

//===========================================================================================================================
//	_IdleStateKeepAliveFinalize
//===========================================================================================================================

static OSStatus	_IdleStateKeepAliveFinalize( AirPlayReceiverSessionRef inSession )
{
	Boolean			wasStarted;

	wasStarted = IsValidSocket( inSession->keepAliveSock );

	if( wasStarted ) _IdleStateKeepAliveStop( inSession );

	// Clean up resources.

	ForgetSocket( &inSession->keepAliveCmdSock );
	ForgetSocket( &inSession->keepAliveSock );
	if( wasStarted ) atr_ulog( kLogLevelTrace, "Keep alive finalized\n" );
	return( kNoErr );
}


static void _IdleStateSessionDied( void *inCtx )
{
	AirPlayReceiverSessionRef const		session	= (AirPlayReceiverSessionRef) inCtx;
	AirPlayReceiverServerControl( session->server, kCFObjectFlagDirect, CFSTR( kAirPlayCommand_SessionDied ), session, NULL, NULL );
	CFRelease( session );
}

//===========================================================================================================================
//	_IdleStateKeepAliveThread
//===========================================================================================================================

static void *	_IdleStateKeepAliveThread( void *inArg )
{
	AirPlayReceiverSessionRef const		session	= (AirPlayReceiverSessionRef) inArg;
	SocketRef const						sock	= session->keepAliveSock;
	SocketRef const						cmdSock	= session->keepAliveCmdSock;
	fd_set								readSet;
	int									maxFd;
	struct timeval						timeout;
	int									n;
	OSStatus							err;

	SetThreadName( "AirPlayKeepAliveReceiver" );
	SetCurrentThreadPriority( kAirPlayThreadPriority_KeepAliveReceiver );
    atr_ulog( kLogLevelTrace, "Keep alive thread starting\n" );

	FD_ZERO( &readSet );
	maxFd = -1;
	if( (int) sock    > maxFd ) maxFd = sock;
	if( (int) cmdSock > maxFd ) maxFd = cmdSock;
	maxFd += 1;
	for( ;; )
	{
		timeout.tv_sec  = kAirPlayDataTimeoutSecs;
		timeout.tv_usec = 0;

		FD_SET( sock, &readSet );
		FD_SET( cmdSock, &readSet );
		n = select( maxFd, &readSet, NULL, NULL, &timeout );
		err = select_errno( n );
		if( err == EINTR )					continue;
		if( err == kTimeoutErr )
		{
			atr_ulog( kLogLevelError, "Keep alive thread timeout\n" );
			CFRetain( session );
			dispatch_async_f( session->queue, session, _IdleStateSessionDied );
			break;
		}
		if( err )							{ dlogassert( "select() error: %#m", err ); usleep( 100000 ); continue; }
		if( FD_ISSET( sock,    &readSet ) ) _IdleStateKeepAliveReceiveBeacon( session, sock );
		if( FD_ISSET( cmdSock, &readSet ) ) break; // The only event is quit so break if anything is pending.
	}
	atr_ulog( kLogLevelTrace, "Keep alive thread exit\n" );
	return( NULL );
}

//===========================================================================================================================
//	_IdleStateKeepAliveReceiveBeacon
//===========================================================================================================================

#define	kLowPowerKeepAliveVersion								0
#define	LowPowerKeepAliveHeaderExtractVersion( FIELDS )			( ( ( FIELDS ) >> 6 ) & 0x03 )
#define	LowPowerKeepAliveHeaderExtractSleep( FIELDS )			( ( ( FIELDS ) >> 5 ) & 0x01 )

static OSStatus	_IdleStateKeepAliveReceiveBeacon( AirPlayReceiverSessionRef inSession, SocketRef inSock )
{
	OSStatus			err;
	char				pkt[32];
	size_t				len;

	err = SocketRecvFrom( inSock, &pkt, sizeof( pkt ), &len, NULL, 0, NULL, NULL, NULL, NULL );
	if( err == EWOULDBLOCK ) goto exit;
	require_noerr( err, exit );

	if( ( len > 0 )
		&& ( kLowPowerKeepAliveVersion == LowPowerKeepAliveHeaderExtractVersion( pkt[0] ) )
		&& LowPowerKeepAliveHeaderExtractSleep( pkt[0] )
		&& inSession->eventQueue )
	{
		dispatch_async_f( inSession->eventQueue, inSession, _EventReplyTimeoutCallback );
	}

exit:
	atr_ulog( kLogLevelInfo, "err %#m RCV UDP len %d. %02x%02x %02x%02x\n", err, len, (unsigned char)pkt[0], (unsigned char)pkt[1], (unsigned char)pkt[2], (unsigned char)pkt[3] );
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Timing ==
#endif

//===========================================================================================================================
//	_TimingInitialize
//===========================================================================================================================

static OSStatus	_TimingInitialize( AirPlayReceiverSessionRef inSession )
{
	OSStatus		err;
	sockaddr_ip		sip;
	
	// Set up a socket to send and receive timing-related info.
	
	SockAddrCopy( &inSession->peerAddr, &sip );
	err = ServerSocketOpen( sip.sa.sa_family, SOCK_DGRAM, IPPROTO_UDP, -kAirPlayPort_TimeSyncClient, 
		&inSession->timingPortLocal, kSocketBufferSize_DontSet, &inSession->timingSock );
	require_noerr( err, exit );
	
	SocketSetPacketTimestamps( inSession->timingSock, true );
	SocketSetQoS( inSession->timingSock, kSocketQoS_NTP );
	
	// Connect to the server address to avoid the IP stack doing a temporary connect on each send. 
	// Using connect also allows us to receive ICMP errors if the server goes away.
	
	SockAddrSetPort( &sip, inSession->timingPortRemote );
	inSession->timingRemoteAddr = sip;
	inSession->timingRemoteLen  = SockAddrGetSize( &sip );
	err = connect( inSession->timingSock, &sip.sa, inSession->timingRemoteLen );
	err = map_socket_noerr_errno( inSession->timingSock, err );
	if( err ) dlog( kLogLevelNotice, "### Timing connect UDP to %##a failed (using sendto instead): %#m\n", &sip, err );
	inSession->timingConnected = !err;
	
	// Set up a socket for sending commands to the thread.
	
	err = OpenSelfConnectedLoopbackSocket( &inSession->timingCmdSock );
	require_noerr( err, exit );
	
	atr_ulog( kLogLevelTrace, "Timing set up on port %d to port %d\n", inSession->timingPortLocal, inSession->timingPortRemote );
	
exit:
	if( err )
	{
		atr_ulog( kLogLevelWarning, "### Timing setup failed: %#m\n", err );
		_TimingFinalize( inSession );
	}
	return( err );
}

//===========================================================================================================================
//	_TimingFinalize
//===========================================================================================================================

static OSStatus	_TimingFinalize( AirPlayReceiverSessionRef inSession )
{
	OSStatus		err;
	Boolean			wasStarted;
	
	DEBUG_USE_ONLY( err );
	wasStarted = IsValidSocket( inSession->timingSock );
	
	// Signal the thread to quit and wait for it to signal back that it exited.
	
	if( inSession->timingThreadPtr )
	{
		err = SendSelfConnectedLoopbackMessage( inSession->timingCmdSock, "q", 1 );
		check_noerr( err );
		
		err = pthread_join( inSession->timingThread, NULL );
		check_noerr( err );
		inSession->timingThreadPtr = NULL;
	}
	
	// Clean up resources.
	
	ForgetSocket( &inSession->timingCmdSock );
	ForgetSocket( &inSession->timingSock );
	if( wasStarted ) atr_ulog( kLogLevelTrace, "Timing finalized\n" );
	return( kNoErr );
}

//===========================================================================================================================
//	_TimingNegotiate
//===========================================================================================================================

static OSStatus	_TimingNegotiate( AirPlayReceiverSessionRef inSession )
{
	SocketRef const		timingSock = inSession->timingSock;
	OSStatus			err;
	int					nFailure;
	int					nSuccess;
	int					nTimeouts;
	int					nSendErrors;
	int					nRecvErrors;
	OSStatus			lastSendError;
	OSStatus			lastRecvError;
	fd_set				readSet;
	int					n;
	struct timeval		timeout;
	
	require_action( inSession->timingThreadPtr == NULL, exit, err = kAlreadyInitializedErr );
	require_action( inSession->airTunesClock, exit, err = kStateErr );
	
	inSession->source.rtcpTIResponseCount = 0;
	inSession->source.rtcpTIForceStep = true;
	nFailure		= 0;
	nSuccess		= 0;
	nTimeouts		= 0;
	nRecvErrors		= 0;
	lastSendError	= kNoErr;
	lastRecvError	= kNoErr;
	FD_ZERO( &readSet );
	for( ;; )
	{
		// Send a request.
		
		nSendErrors = 0;
		for( ;; )
		{
			err = _TimingSendRequest( inSession );
			if( !err ) break;
			atr_ulog( kLogLevelWarning, "### Time sync send error: %#m\n", err );
			usleep( 100000 );
			if( err != lastSendError )
			{
				atr_ulog( kLogLevelNotice, "Time negotiate send error: %d\n", (int) err );
				lastSendError = err;
			}
			if( ++nSendErrors >= 64 )
			{
				atr_ulog( kLogLevelError, "Too many time negotiate send failures: %d\n", (int) err );
				goto exit;
			}
		}
		
		// Receive and process the response.
		
		for( ;; )
		{
			FD_SET( timingSock, &readSet );
			timeout.tv_sec  = 0;
			timeout.tv_usec = 100 * 1000;
			n = select( timingSock + 1, &readSet, NULL, NULL, &timeout );
			err = select_errno( n );
			if( err )
			{
				atr_ulog( kLogLevelWarning, "### Time sync select() error: %#m\n", err );
				#if( TARGET_OS_POSIX )
				if( err == EINTR ) continue;
				#endif
				++nTimeouts;
				++nFailure;
				if( err != lastRecvError )
				{
					atr_ulog( kLogLevelNotice, "Time negotiate receive error: %d\n", (int) err );
					lastRecvError = err;
				}
				break;
			}
			
			err = _TimingReceiveResponse( inSession, timingSock );
			if( err )
			{
				if( err == kDuplicateErr ) {
					continue;
				}
				if( err == ECONNREFUSED )
				{
					goto exit;
				}
				++nRecvErrors;
				++nFailure;
				atr_ulog( kLogLevelWarning, "### Time sync receive error: %#m\n", err );
				if( err != lastRecvError )
				{
					atr_ulog( kLogLevelNotice, "Time negotiate receive error: %d\n", (int) err );
					lastRecvError = err;
				}
				if( err == kDuplicateErr )
				{
					DrainUDPSocket( timingSock, 500, NULL );
				}
			}
			else
			{
				++nSuccess;
			}
			break;
		}
		if( nSuccess >= 5 ) break;
		if( nFailure >= 64 )
		{
			atr_ulog( kLogLevelError, "Too many time negotiate failures: G=%d B=%d R=%d T=%d\n", 
				nSuccess, nFailure, nRecvErrors, nTimeouts );
			err = kTimeoutErr;
			goto exit;
		}
	}
	inSession->source.rtcpTIForceStep = false;
	// Because these were all done back to back during negotiate, there is no need to keep anything other than the best.
	// The rest can just cause the best measurement to be prematurely evicted from the shift register.
	if (inSession->source.rctpTIClockUsedIndex > 0) {
		inSession->source.rtcpTIClockDelayArray[ 0 ] = inSession->source.rtcpTIClockDelayArray[ inSession->source.rctpTIClockUsedIndex ];
		inSession->source.rtcpTIClockOffsetArray[ 0 ] = inSession->source.rtcpTIClockOffsetArray[ inSession->source.rctpTIClockUsedIndex ];
		inSession->source.rctpTIClockUsedIndex = 0;
	}
	inSession->source.rtcpTIClockIndex = 1;
	
	// Start the timing thread to keep our clock sync'd.
	
	err = pthread_create( &inSession->timingThread, NULL, _TimingThread, inSession );
	require_noerr( err, exit );
	inSession->timingThreadPtr = &inSession->timingThread;
	
	atr_ulog( kLogLevelTrace, "Timing started\n" );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_TimingThread
//===========================================================================================================================

static void *	_TimingThread( void *inArg )
{
	AirPlayReceiverSessionRef const		session	= (AirPlayReceiverSessionRef) inArg;
	SocketRef const						sock	= session->timingSock;
	SocketRef const						cmdSock	= session->timingCmdSock;
	fd_set								readSet;
	int									maxFd;
	struct timeval						timeout;
	int									n;
	OSStatus							err;
	
	SetThreadName( "AirPlayTimeSyncClient" );
	SetCurrentThreadPriority( kAirPlayThreadPriority_TimeSyncClient );
    atr_ulog( kLogLevelTrace, "Timing thread starting\n" );
		
	FD_ZERO( &readSet );
	maxFd = -1;
	if( (int) sock    > maxFd ) maxFd = sock;
	if( (int) cmdSock > maxFd ) maxFd = cmdSock;
	maxFd += 1;
	for( ;; )
	{
		FD_SET( sock, &readSet );
		FD_SET( cmdSock, &readSet );
		timeout.tv_sec  = 2;
		timeout.tv_usec = Random32() % 1000000;
		n = select( maxFd, &readSet, NULL, NULL, &timeout );
		err = select_errno( n );
		if( err == EINTR )					continue;
		if( err == kTimeoutErr )			{ _TimingSendRequest( session ); continue; }
		if( err )							{ dlogassert( "select() error: %#m", err ); usleep( 100000 ); continue; }
		if( FD_ISSET( sock,    &readSet ) ) _TimingReceiveResponse( session, sock );
		if( FD_ISSET( cmdSock, &readSet ) ) break; // The only event is quit so break if anything is pending.
	}
	atr_ulog( kLogLevelTrace, "Timing thread exit\n" );
	return( NULL );
}

//===========================================================================================================================
//	_TimingSendRequest
//
//	Note: This function does not need the AirTunes lock because it only accesses variables from a single thread at a time.
//		  These variables are only accessed once by the RTSP thread during init and then only by the timing thread.
//===========================================================================================================================

static OSStatus	_TimingSendRequest( AirPlayReceiverSessionRef inSession )
{
	OSStatus				err;
	AirTunesSource *		src;
	RTCPTimeSyncPacket		pkt;
	AirTunesTime			now;
	ssize_t					n;
	
	src = &inSession->source;
	
	// Build and send the request. The response is received asynchronously.
	
	pkt.v_p_m			= RTCPHeaderInsertVersion( 0, kRTPVersion );
	pkt.pt				= kRTCPTypeTimeSyncRequest;
	pkt.length			= htons( ( sizeof( pkt ) / 4 ) - 1 );
	pkt.rtpTimestamp	= 0;
	pkt.ntpOriginateHi	= 0;
	pkt.ntpOriginateLo	= 0;
	pkt.ntpReceiveHi	= 0;
	pkt.ntpReceiveLo	= 0;
	
	AirTunesClock_GetSynchronizedTime( inSession->airTunesClock, &now );
	src->rtcpTILastTransmitTimeHi = now.secs + kNTPvsUnixSeconds;
	src->rtcpTILastTransmitTimeLo = (uint32_t)( now.frac >> 32 );
	pkt.ntpTransmitHi	= htonl( src->rtcpTILastTransmitTimeHi );
	pkt.ntpTransmitLo	= htonl( src->rtcpTILastTransmitTimeLo );
	
	if( inSession->timingConnected )
	{
		n = send( inSession->timingSock, (char *) &pkt, sizeof( pkt ), 0 );
	}
	else
	{
		n = sendto( inSession->timingSock, (char *) &pkt, sizeof( pkt ), 0, &inSession->timingRemoteAddr.sa, 
			inSession->timingRemoteLen );
	}
	err = map_socket_value_errno( inSession->timingSock, n == (ssize_t) sizeof( pkt ), n );
	require_noerr_quiet( err, exit );
	increment_wrap( src->rtcpTISendCount, 1 );
	
exit:
	if( err ) atr_ulog( kLogLevelNotice, "### NTP send request failed: %#m\n", err );
	return( err );
}

//===========================================================================================================================
//	_TimingReceiveResponse
//===========================================================================================================================

static OSStatus	_TimingReceiveResponse( AirPlayReceiverSessionRef inSession, SocketRef inSock )
{
	OSStatus			err;
	RTCPPacket			pkt;
	size_t				len;
	uint64_t			ticks;
	int					tmp;
	AirTunesTime		recvTime;
	
	err = SocketRecvFrom( inSock, &pkt, sizeof( pkt ), &len, NULL, 0, NULL, &ticks, NULL, NULL );
	if( err == EWOULDBLOCK ) goto exit;
	require_noerr( err, exit );
	if( len < sizeof( pkt.header ) )
	{
		dlogassert( "Bad size: %zu < %zu", sizeof( pkt.header ), len );
		err = kSizeErr;
		goto exit;
	}
	
	tmp = RTCPHeaderExtractVersion( pkt.header.v_p_c );
	if( tmp != kRTPVersion )
	{
		dlogassert( "Bad version: %d", tmp );
		err = kVersionErr;
		goto exit;
	}
	if( pkt.header.pt != kRTCPTypeTimeSyncResponse )
	{
		dlogassert( "Wrong packet type: %d", pkt.header.pt );
		err = kTypeErr;
		goto exit;
	}
	
	require_action( len >= sizeof( pkt.timeSync ), exit, err = kSizeErr );
	AirTunesClock_GetSynchronizedTimeNearUpTicks( inSession->airTunesClock, &recvTime, ticks );
	err = _TimingProcessResponse( inSession, &pkt.timeSync, &recvTime );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_TimingProcessResponse
//
//	Note: This function does not need the AirTunes lock because it only accesses variables from a single thread at a time.
//		  These variables are only accessed once by the RTSP thread during init and then only by the timing thread.
//===========================================================================================================================

static OSStatus	_TimingProcessResponse( AirPlayReceiverSessionRef inSession, RTCPTimeSyncPacket *inPkt, const AirTunesTime *inTime )
{
	AirTunesSource * const	src = &inSession->source;
	OSStatus				err;
	uint64_t				t1;
	uint64_t				t2;
	uint64_t				t3;
	uint64_t				t4;
	double					offset;
	double					rtt;
	unsigned int			i;
	Boolean					useMeasurement;
	Boolean					clockStepped;
	
	inPkt->rtpTimestamp		= ntohl( inPkt->rtpTimestamp );
	inPkt->ntpOriginateHi	= ntohl( inPkt->ntpOriginateHi );
	inPkt->ntpOriginateLo	= ntohl( inPkt->ntpOriginateLo );
	inPkt->ntpReceiveHi		= ntohl( inPkt->ntpReceiveHi );
	inPkt->ntpReceiveLo		= ntohl( inPkt->ntpReceiveLo );
	inPkt->ntpTransmitHi	= ntohl( inPkt->ntpTransmitHi );
	inPkt->ntpTransmitLo	= ntohl( inPkt->ntpTransmitLo );
	
	// Make sure this response is for the last request we made and is not a duplicate response.
	
	if( ( inPkt->ntpOriginateHi != src->rtcpTILastTransmitTimeHi ) || 
		( inPkt->ntpOriginateLo != src->rtcpTILastTransmitTimeLo ) )
	{
		err = kDuplicateErr;
		goto exit;
	}
	src->rtcpTILastTransmitTimeHi = 0; // Zero so we don't try to process a duplicate.
	src->rtcpTILastTransmitTimeLo = 0;
	
	// Calculate the relative offset between clocks.
	//
	// Client:  T1           T4
	// ----------------------------->
	//           \           ^
	//            \         /
	//             v       /
	// ----------------------------->
	// Server:     T2     T3
	// 
	// Clock offset in NTP units     = ((T2 - T1) + (T3 - T4)) / 2.
	// Round-trip delay in NTP units =  (T4 - T1) - (T3 - T2)
	
	t1 = ( ( (uint64_t) inPkt->ntpOriginateHi )	<< 32 ) | inPkt->ntpOriginateLo;
	t2 = ( ( (uint64_t) inPkt->ntpReceiveHi )	<< 32 ) | inPkt->ntpReceiveLo;
	t3 = ( ( (uint64_t) inPkt->ntpTransmitHi )	<< 32 ) | inPkt->ntpTransmitLo;
	t4 = ( ( (uint64_t)( inTime->secs + kNTPvsUnixSeconds ) ) << 32 ) + ( inTime->frac >> 32 );
	
	offset = 0.5 * ( ( ( (double)( (int64_t)( t2 - t1 ) ) ) * kNTPFraction ) + 
					 ( ( (double)( (int64_t)( t3 - t4 ) ) ) * kNTPFraction ) );
	rtt = ( ( (double)( (int64_t)( t4 - t1 ) ) ) * kNTPFraction ) - 
		  ( ( (double)( (int64_t)( t3 - t2 ) ) ) * kNTPFraction );
	
	// Update round trip time stats.
	
	if( rtt < src->rtcpTIClockRTTMin ) src->rtcpTIClockRTTMin = rtt;
	if( rtt > src->rtcpTIClockRTTMax ) src->rtcpTIClockRTTMax = rtt;
	if( src->rtcpTIResponseCount == 0 ) src->rtcpTIClockRTTAvg = rtt;
	src->rtcpTIClockRTTAvg			= ( ( 15.0 * src->rtcpTIClockRTTAvg ) + rtt ) * ( 1.0 / 16.0 );
	
	// Update clock offset stats. If this is first time ever or the first time after a clock step, reset the stats.
	
	if( src->rtcpTIResponseCount == 0 )
	{
		for( i = 0; i < countof( src->rtcpTIClockOffsetArray ); ++i )
		{
			src->rtcpTIClockDelayArray[ i ] = 1000.0;
			src->rtcpTIClockOffsetArray[ i ] = 0.0;
		}
		src->rtcpTIClockIndex = 0;
		src->rctpTIClockUsedIndex = 0;
		src->rtcpTIClockOffsetAvg = 0.0;
		src->rtcpTIClockOffsetMin = offset;
		src->rtcpTIClockOffsetMax = offset;
	}
	
	//Only use measurements with a short RTT (delay)
	//Typical NTP 8 stage shift register as per https://www.eecis.udel.edu/~mills/ntp/html/filter.html

	useMeasurement = true;
	for( i = 0; i < countof( src->rtcpTIClockDelayArray ); ++i )
	{
		if( rtt > src->rtcpTIClockDelayArray[ i ])
		{
			useMeasurement = false;
			break;
		}
	}

	src->rtcpTIClockDelayArray[ src->rtcpTIClockIndex ] = rtt;
	src->rtcpTIClockOffsetArray[ src->rtcpTIClockIndex ] = offset;
	if (useMeasurement) {
		src->rctpTIClockUsedIndex = src->rtcpTIClockIndex;
	}
	src->rtcpTIClockIndex++;
	if( src->rtcpTIClockIndex >= countof( src->rtcpTIClockOffsetArray ) )
	{
		src->rtcpTIClockIndex = 0;
	}
	
	src->rtcpTIClockOffsetAvg = ( ( 15.0 * src->rtcpTIClockOffsetAvg ) + offset ) * ( 1.0 / 16.0 );
	if( offset < src->rtcpTIClockOffsetMin ) src->rtcpTIClockOffsetMin = offset;
	if( offset > src->rtcpTIClockOffsetMax ) src->rtcpTIClockOffsetMax = offset;

	err = kNoErr;
	if (useMeasurement)
	{
		// Sync our local clock to the server's clock. If this is the first sync, always step.
		
		clockStepped = AirTunesClock_Adjust( inSession->airTunesClock, (int64_t)( offset * 1E9 ), src->rtcpTIForceStep );
		if( clockStepped && !src->rtcpTIForceStep ) {
			++src->rtcpTIStepCount;
			err = kRangeErr;
		}
		++src->rtcpTIResponseCount;
	}
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Screen ==
#endif

//===========================================================================================================================
//	Screen
//===========================================================================================================================

#define kAirPlayScreenReceiver_MaxFrameCount		8
#define kAirPlayScreenReceiver_MaxFrameSize			( 160 * 1024 )
#define kAirPlayScreenReceiver_SocketBufferSize		( ( 110 * kAirPlayScreenReceiver_MaxFrameSize ) / 100 ) // 10% extra

typedef struct
{
	AirPlayReceiverServerRef	server;
	CFArrayRef					timestampInfo;
	
}	ScreenTimestampInfoParams;

//===========================================================================================================================
//	_ScreenSetup
//===========================================================================================================================

static OSStatus
	_ScreenSetup(
		AirPlayReceiverSessionRef	inSession, 
		CFDictionaryRef				inStreamDesc,
		CFMutableDictionaryRef		inResponseParams )
{
	OSStatus						err;
	CFMutableDictionaryRef			responseStreamDesc = NULL;
	CFArrayRef						timestampInfo = NULL;
	int								receivePort;
	uint64_t						streamConnectionID = 0;
	uint8_t							aesScreenKey[ 16 ];
	uint8_t							aesScreenIV[ 16 ];
	uint8_t							outputKey[ 32 ];
				
	require_action( !inSession->screenInitialized, exit2, err = kAlreadyInitializedErr );
	
	responseStreamDesc = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( responseStreamDesc, exit, err = kNoMemoryErr );
	
	err = AirPlayReceiverSessionScreen_Setup( inSession->screenSession, inStreamDesc, (uint32_t) inSession->clientSessionID );
	require_noerr( err, exit );
	
	streamConnectionID = (uint64_t) CFDictionaryGetInt64( inStreamDesc, CFSTR( kAirPlayKey_StreamConnectionID ), NULL );
	require_action( streamConnectionID, exit, err = kVersionErr );
	
	if( inSession->pairVerifySession )
	{
		err = _GetStreamSecurityKeys( inSession, streamConnectionID, 0, NULL, 32, outputKey );
		require_noerr( err, exit );
		err = AirPlayReceiverSessionScreen_SetChaChaSecurityInfo( inSession->screenSession, outputKey, 32 );
		require_noerr( err, exit );
	}
	else
	{
		AirPlay_DeriveAESKeySHA512ForScreen( inSession->aesSessionKey, kAirTunesAESKeyLen, streamConnectionID, aesScreenKey, aesScreenIV );
		err = AirPlayReceiverSessionScreen_SetSecurityInfo( inSession->screenSession, aesScreenKey, aesScreenIV );
		MemZeroSecure( aesScreenKey, sizeof( aesScreenKey ) );
		MemZeroSecure( aesScreenIV, sizeof( aesScreenIV ) );
		require_noerr( err, exit );
	}

	err = ServerSocketOpen( inSession->peerAddr.sa.sa_family, SOCK_STREAM, IPPROTO_TCP, -kAirPlayPort_RTPScreen, 
		&receivePort, -kAirPlayScreenReceiver_SocketBufferSize, &inSession->screenSock );
	require_noerr( err, exit );
	
	SocketSetQoS( inSession->screenSock, kSocketQoS_AirPlayScreenVideo );
	
	CFDictionarySetInt64( responseStreamDesc, CFSTR( kAirPlayKey_Type ), kAirPlayStreamType_Screen );
	CFDictionarySetInt64( responseStreamDesc, CFSTR( kAirPlayKey_Port_Data ), receivePort );
	
	err = _AddResponseStream( inResponseParams, responseStreamDesc );
	require_noerr( err, exit );
	
	inSession->screenInitialized = true;
	atr_ulog( kLogLevelTrace, "screen receiver set up on port %d\n", receivePort );

	// If the session's already started then immediately start the thread process it.
	
	if( inSession->sessionStarted )
	{
		err = _ScreenStart( inSession );
		require_noerr( err, exit );
	}
	
exit:
	CFReleaseNullSafe( responseStreamDesc );
	CFReleaseNullSafe( timestampInfo );
	if( err ) _ScreenTearDown( inSession );
	
exit2:
	MemZeroSecure( outputKey, sizeof( outputKey ) );
	if( err ) atr_ulog( kLogLevelWarning, "### screen receiver setup failed: %#m\n", err );
	return( err );
}

//===========================================================================================================================
//	_ScreenTearDown
//===========================================================================================================================

static void	_ScreenTearDown( AirPlayReceiverSessionRef inSession )
{
	OSStatus		err;
	
	DEBUG_USE_ONLY( err );
	
	if( inSession->screenThreadPtr )
	{
		err = AirPlayReceiverSessionScreen_SendCommand( inSession->screenSession, kAirPlayReceiverSessionScreenCommand_Quit, NULL, 0 );
		check_noerr( err );

		err = pthread_join( inSession->screenThread, NULL );
		check_noerr( err );
		inSession->screenThreadPtr = NULL;
	}
	
	ForgetSocket( &inSession->screenSock );
	
	if( inSession->screenInitialized ) atr_ulog( kLogLevelTrace, "screen receiver torn down\n" );
	inSession->screenInitialized = false;
}

//===========================================================================================================================
//	_ScreenStart
//===========================================================================================================================

static OSStatus	_ScreenStart( AirPlayReceiverSessionRef inSession )
{
	OSStatus			err;
	
	require_action_quiet( !inSession->screenThreadPtr, exit, err = kNoErr );

	err = pthread_create( &inSession->screenThread, NULL, _ScreenThread, inSession );
	require_noerr( err, exit );
	inSession->screenThreadPtr = &inSession->screenThread;

exit:
	if( err ) atr_ulog( kLogLevelWarning, "### screen start failed: %#m\n", err );
	return( err );
}

//===========================================================================================================================
//	_ScreenThread
//===========================================================================================================================

static void *	_ScreenThread( void *inArg )
{
	AirPlayReceiverSessionRef const					session = (AirPlayReceiverSessionRef) inArg;
	OSStatus										err;
	NetSocketRef									netSock = NULL;
	AirPlayReceiverSessionScreenTimeSynchronizer	timeSynchronizer;
	CFMutableDictionaryRef							params = NULL;
	SocketRef										newSock = kInvalidSocketRef;
	
	SetThreadName( "AirPlayScreenReceiver" );
	SetCurrentThreadPriority( kAirPlayThreadPriority_ScreenReceiver );
	
	// Wait for the client to connect and then replace the acceptor socket with the new data socket.
	
	err = SocketAccept( session->screenSock, kAirPlayConnectTimeoutSecs, &newSock, NULL );
	require_noerr( err, exit );
	
	ForgetSocket( &session->screenSock ); // don't need listening socket anymore.
	
	atr_ulog( kLogLevelTrace, "screen receiver started\n" );

	err = NetSocket_CreateWithNative( &netSock, newSock );
	require_noerr( err, exit );
	
	newSock = kInvalidSocketRef; // netSock now owns newSock.
	
	timeSynchronizer.context								= session;
	timeSynchronizer.getSynchronizedNTPTimeFunc				= _ScreenGetSynchronizedNTPTime;
	timeSynchronizer.getUpTicksNearSynchronizedNTPTimeFunc	= _ScreenGetUpTicksNearSynchronizedNTPTime;

	AirPlayReceiverSessionScreen_SetTimeSynchronizer( session->screenSession, &timeSynchronizer );
	
	err = AirPlayReceiverSessionScreen_StartSession( session->screenSession, session->delegate.context );
	require_noerr( err, exit );
	
	params = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( params, exit, err = kNoMemoryErr );
	
	err = AirPlayReceiverSessionScreen_ProcessFrames( session->screenSession, netSock, session->server->timeoutDataSecs );
	require_noerr( err, exit );
	
exit:
	AirPlayReceiverSessionScreen_StopSession( session->screenSession );
	NetSocket_Forget( &netSock );
	ForgetSocket( &newSock );
	CFReleaseNullSafe( params );
    atr_ulog( kLogLevelTrace, "Screen thread exit\n" );
	return( NULL );
}

//===========================================================================================================================
//	_ScreenGetSynchronizedNTPTime
//===========================================================================================================================

static uint64_t _ScreenGetSynchronizedNTPTime( void *inContext )
{
	AirPlayReceiverSessionRef me =  (AirPlayReceiverSessionRef) inContext;
	
	return AirTunesClock_GetSynchronizedNTPTime( me->airTunesClock );
}

//===========================================================================================================================
//	_ScreenGetUpTicksNearSynchronizedNTPTime
//===========================================================================================================================

static uint64_t _ScreenGetUpTicksNearSynchronizedNTPTime( void *inContext, uint64_t inNTPTime )
{
	AirPlayReceiverSessionRef me =  (AirPlayReceiverSessionRef) inContext;

	return AirTunesClock_GetUpTicksNearSynchronizedNTPTime( me->airTunesClock, inNTPTime );
}

#if 0
#pragma mark -
#pragma mark == Utils ==
#endif

//===========================================================================================================================
//	_AddResponseStream
//===========================================================================================================================

static OSStatus	_AddResponseStream( CFMutableDictionaryRef inResponseParams, CFDictionaryRef inStreamDesc )
{
	OSStatus				err;
	CFMutableArrayRef		responseStreams;
	
	responseStreams = (CFMutableArrayRef) CFDictionaryGetCFArray( inResponseParams, CFSTR( kAirPlayKey_Streams ), NULL );
	if( !responseStreams )
	{
		responseStreams = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		require_action( responseStreams, exit, err = kNoMemoryErr );
		CFArrayAppendValue( responseStreams, inStreamDesc );
		CFDictionarySetValue( inResponseParams, CFSTR( kAirPlayKey_Streams ), responseStreams );
		CFRelease( responseStreams );
	}
	else
	{
		CFArrayAppendValue( responseStreams, inStreamDesc );
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_AudioDecoderInitialize
//===========================================================================================================================

static OSStatus	_AudioDecoderInitialize( AirPlayReceiverSessionRef inSession )
{
	AirPlayAudioStreamContext * const		ctx = &inSession->mainAudioCtx;
	OSStatus								err;
	AudioStreamBasicDescription				inputFormat;
	AudioStreamBasicDescription				outputFormat;
	
	if( 0 ) {} // Empty if to simplify conditional logic below.
	else if( inSession->compressionType == kAirPlayCompressionType_AAC_LC )
	{
		ASBD_FillAAC_LC( &inputFormat, ctx->sampleRate, ctx->channels );
	}
	else { dlogassert( "Bad compression type: %d", inSession->compressionType ); err = kUnsupportedErr; goto exit; }
	
	ASBD_FillPCM( &outputFormat, ctx->sampleRate, ctx->bitsPerSample, ctx->bitsPerSample, ctx->channels );
	err = AudioConverterNew( &inputFormat, &outputFormat, &inSession->audioConverter );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	_AudioDecoderDecodeFrame
//===========================================================================================================================

static OSStatus
	_AudioDecoderDecodeFrame( 
		AirPlayReceiverSessionRef	inSession, 
		const uint8_t *				inSrcPtr, 
		size_t						inSrcLen, 
		uint8_t *					inDstPtr, 
		size_t						inDstMaxLen, 
		size_t *					outDstLen )
{
	AirPlayAudioStreamContext * const		ctx = &inSession->mainAudioCtx;
	OSStatus								err;
	UInt32									frameCount;
	AudioBufferList							bufferList;
	
	inSession->encodedDataPtr = inSrcPtr;
	inSession->encodedDataEnd = inSrcPtr + inSrcLen;
	
	frameCount									= inSession->framesPerPacket;
	bufferList.mNumberBuffers					= 1;
	bufferList.mBuffers[ 0 ].mNumberChannels	= ctx->channels;
	bufferList.mBuffers[ 0 ].mDataByteSize		= (uint32_t) inDstMaxLen;
	bufferList.mBuffers[ 0 ].mData				= inDstPtr;
	
	err = AudioConverterFillComplexBuffer( inSession->audioConverter, _AudioDecoderDecodeCallback, inSession, 
		&frameCount, &bufferList, NULL );
	if( err == kUnderrunErr ) err = kNoErr;
	require_noerr( err, exit );
	
	*outDstLen = frameCount * ctx->bytesPerUnit;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_AudioDecoderDecodeCallback
//
//	See <http://developer.apple.com/library/mac/#qa/qa2001/qa1317.html> for AudioConverterFillComplexBuffer callback details.
//===========================================================================================================================

static OSStatus
	_AudioDecoderDecodeCallback(
		AudioConverterRef				inAudioConverter,
		UInt32 *						ioNumberDataPackets,
		AudioBufferList *				ioData,
		AudioStreamPacketDescription **	outDataPacketDescription,
		void *							inUserData )
{
	AirPlayReceiverSessionRef const			session = (AirPlayReceiverSessionRef) inUserData;
	AirPlayAudioStreamContext * const		ctx = &session->mainAudioCtx;
	
	(void) inAudioConverter;
	
	if( session->encodedDataPtr != session->encodedDataEnd )
	{
		check( *ioNumberDataPackets > 0 );
		*ioNumberDataPackets = 1;
		
		ioData->mNumberBuffers					= 1;
		ioData->mBuffers[ 0 ].mNumberChannels	= ctx->channels;
		ioData->mBuffers[ 0 ].mDataByteSize		= (UInt32)( session->encodedDataEnd - session->encodedDataPtr );
		ioData->mBuffers[ 0 ].mData				= (void *) session->encodedDataPtr;
		session->encodedDataPtr					= session->encodedDataEnd;
		
		session->encodedPacketDesc.mStartOffset				= 0;
		session->encodedPacketDesc.mVariableFramesInPacket	= 0;
		session->encodedPacketDesc.mDataByteSize			= ioData->mBuffers[ 0 ].mDataByteSize;
		*outDataPacketDescription							= &session->encodedPacketDesc;
		
		return( kNoErr );
	}
	
	*ioNumberDataPackets = 0;
	return( kUnderrunErr );
}

//===========================================================================================================================
//	_AudioEncoderEncodeCallback
//===========================================================================================================================

static OSStatus
	_AudioEncoderEncodeCallback(
		AudioConverterRef				inAudioConverter,
		UInt32 *						ioNumberDataPackets,
		AudioBufferList *				ioData,
		AudioStreamPacketDescription **	outDataPacketDescription,
		void *							inUserData )
{
	AirPlayAudioStreamContext * const		ctx = (AirPlayAudioStreamContext *) inUserData;
	UInt32									bytes, avail;
	
	(void) inAudioConverter;
	(void) outDataPacketDescription;
	
	if( ctx->inputDataPtr != ctx->inputDataEnd )
	{
		bytes = *ioNumberDataPackets * ctx->bytesPerUnit;
		avail = (UInt32)( ctx->inputDataEnd - ctx->inputDataPtr );
		bytes = Min( bytes, avail );
		
		ioData->mBuffers[ 0 ].mNumberChannels	= ctx->channels;
		ioData->mBuffers[ 0 ].mDataByteSize		= bytes;
		ioData->mBuffers[ 0 ].mData				= (void *) ctx->inputDataPtr;
		ctx->inputDataPtr += bytes;
		*ioNumberDataPackets = bytes / ctx->bytesPerUnit;
		
		return( kNoErr );
	}

	*ioNumberDataPackets = 0;
	return( kUnderrunErr );
}

//===========================================================================================================================
//	_CompareOSBuildVersionStrings
//===========================================================================================================================

int _CompareOSBuildVersionStrings( const char *inVersion1, const char *inVersion2 )
{
	int result;
	int major1, major2, build1, build2;
	char minor1, minor2;
		
	result = sscanf( inVersion1, "%d%c%d", &major1, &minor1, &build1 );
	require_action( result == 3, exit, result = -1 );
	minor1 = (char) toupper_safe( minor1 );

	result = sscanf( inVersion2, "%d%c%d", &major2, &minor2, &build2 );
	require_action( result == 3, exit, result = 1 );
	minor2 = (char) toupper_safe( minor2 );

	result = ( major1 != major2 ) ? ( major1 - major2 ) : ( ( minor1 != minor2 ) ? ( minor1 - minor2 ) : ( build1 - build2 ) );
	
exit:
	return( result );
}

//===========================================================================================================================
//	_GetStreamSecurityKeys
//===========================================================================================================================

static OSStatus
	_GetStreamSecurityKeys( 
		AirPlayReceiverSessionRef	inSession,
		uint64_t					streamConnectionID, 
		size_t						inInputKeyLen, 
		uint8_t *					outInputKey,
		size_t						inOutputKeyLen,
		uint8_t *					outOutputKey )
{
	OSStatus				err;
	char *					streamKeySaltPtr = NULL;
	size_t					streamKeySaltLen = 0;
	
	require_action( inSession->pairVerifySession, exit, err = kStateErr );

	streamKeySaltLen = asprintf( &streamKeySaltPtr, "%s%llu", kAirPlayPairingDataStreamKeySaltPtr, streamConnectionID );
 
	if( outOutputKey )
	{
		err = PairingSessionDeriveKey( inSession->pairVerifySession, streamKeySaltPtr, streamKeySaltLen, 
			kAirPlayPairingDataStreamKeyOutputInfoPtr, kAirPlayPairingDataStreamKeyOutputInfoLen, inOutputKeyLen, outOutputKey );
		require_noerr( err, exit );
	}
	
	if( outInputKey ) 
	{
		err = PairingSessionDeriveKey( inSession->pairVerifySession, streamKeySaltPtr, streamKeySaltLen, 
			kAirPlayPairingDataStreamKeyInputInfoPtr, kAirPlayPairingDataStreamKeyInputInfoLen, inInputKeyLen, outInputKey );
		require_noerr( err, exit );
	}
	err = kNoErr;
	
exit:
	MemZeroSecure( streamKeySaltPtr, streamKeySaltLen );
	FreeNullSafe( streamKeySaltPtr );
	return( err );
}

//===========================================================================================================================
//	_LogStarted
//===========================================================================================================================

static void	_LogStarted( AirPlayReceiverSessionRef inSession, AirPlayReceiverSessionStartInfo *inInfo, OSStatus inStatus )
{
	inSession->startStatus = inStatus;
	inInfo->recordMs = (uint32_t)( UpTicksToMilliseconds( UpTicks() - inSession->playTicks ) );
	
	atr_ulog( kLogLevelNotice, 
		"AirPlay session started: From=%s D=0x%012llx A=%##a "
		"T=%s C=%s L=%u ms Bonjour=%u ms Conn=%u ms Auth=%u ms Ann=%u ms Setup=%u ms %s%?u%sRec=%u ms: %#m\n",
		inInfo->clientName, inSession->clientDeviceID, &inSession->peerAddr, 
		NetTransportTypeToString( inInfo->transportType ), AirPlayCompressionTypeToString( inSession->compressionType ), 
		AirTunesSamplesToMs( inSession->maxLatency ), 
		inInfo->bonjourMs, inInfo->connectMs, inInfo->authMs, inInfo->announceMs, inInfo->setupAudioMs, 
		inSession->screen ? "Scr=" : "", inSession->screen, inInfo->setupScreenMs, inSession->screen ? " ms " : "", 
		inInfo->recordMs, inStatus );
	
}

//===========================================================================================================================
//	_LogEnded
//===========================================================================================================================

static void	_LogEnded( AirPlayReceiverSessionRef inSession, OSStatus inReason )
{
	DataBuffer							db;
#if( TARGET_OS_POSIX )
	char								buf[ 2048 ];
#else
	char								buf[ 512 ];
#endif
	uint32_t const						durationSecs			= (uint32_t) UpTicksToSeconds( UpTicks() - inSession->sessionTicks );
	const AirTunesSource * const		ats						= &inSession->source;
	uint32_t const						retransmitMinMs			= NanosecondsToMilliseconds32( ats->retransmitMinNanos );
	uint32_t const						retransmitMaxMs			= NanosecondsToMilliseconds32( ats->retransmitMaxNanos );
	uint32_t const						retransmitAvgMs			= NanosecondsToMilliseconds32( ats->retransmitAvgNanos );
	uint32_t const						retransmitRetryMinMs	= NanosecondsToMilliseconds32( ats->retransmitRetryMinNanos );
	uint32_t const						retransmitRetryMaxMs	= NanosecondsToMilliseconds32( ats->retransmitRetryMaxNanos );
	uint32_t const						ntpRTTMin				= (uint32_t)( 1000 * ats->rtcpTIClockRTTMin );
	uint32_t const						ntpRTTMax				= (uint32_t)( 1000 * ats->rtcpTIClockRTTMax );
	uint32_t const						ntpRTTAvg				= (uint32_t)( 1000 * ats->rtcpTIClockRTTAvg );
	
	DataBuffer_Init( &db, buf, sizeof( buf ), 10000 );
	DataBuffer_AppendF( &db, "AirPlay session ended: Dur=%u seconds Reason=%#m\n", durationSecs, inReason );
	DataBuffer_AppendF( &db, "Glitches:    %d%%, %d total, %d glitchy minute(s)\n", 
		( inSession->glitchTotalPeriods > 0 ) ? ( ( inSession->glitchyPeriods * 100 ) / inSession->glitchTotalPeriods ) : 0,
		inSession->glitchTotal, inSession->glitchyPeriods );
	DataBuffer_AppendF( &db, "Retransmits: "
		"%u sent, %u received, %u futile, %u not found, %u/%u/%u ms min/max/avg, %u/%u ms retry min/max\n", 
		ats->retransmitSendCount, ats->retransmitReceiveCount, ats->retransmitFutileCount, ats->retransmitNotFoundCount, 
		retransmitMinMs, retransmitMaxMs, retransmitAvgMs, retransmitRetryMinMs, retransmitRetryMaxMs );
	DataBuffer_AppendF( &db, "Packets:     %u lost, %u unrecovered, %u late, %u max burst, %u big losses, %d%% compression\n", 
		gAirPlayAudioStats.lostPackets, gAirPlayAudioStats.unrecoveredPackets, gAirPlayAudioStats.latePackets, 
		ats->maxBurstLoss, ats->bigLossCount, inSession->compressionPercentAvg / 100 );
	DataBuffer_AppendF( &db, "Time Sync:   "
		"%u/%u/%u ms min/max/avg RTT, %d/%d/%d µS min/max/avg offset, %u step(s)\n",
		ntpRTTMin, ntpRTTMax, ntpRTTAvg, 
		(int32_t)(  1000000 * ats->rtcpTIClockOffsetMin ), 
		(int32_t)(  1000000 * ats->rtcpTIClockOffsetMax ), 
		(int32_t)(  1000000 * ats->rtcpTIClockOffsetAvg ), ats->rtcpTIStepCount );
	atr_ulog( kLogLevelNotice, "%.*s\n", (int) DataBuffer_GetLen( &db ), DataBuffer_GetPtr( &db ) );
	DataBuffer_Free( &db );
	
}

//===========================================================================================================================
//	_LogUpdate
//===========================================================================================================================

static void	_LogUpdate( AirPlayReceiverSessionRef inSession, uint64_t inTicks, Boolean inForce )
{
	(void) inSession;
	(void) inTicks;
	(void) inForce;
}

//===========================================================================================================================
//	_TearDownStream
//===========================================================================================================================

static void	_TearDownStream( AirPlayReceiverSessionRef inSession, AirPlayAudioStreamContext * const ctx, Boolean inIsFinalizing )
{
	OSStatus		err;
	
	DEBUG_USE_ONLY( err );
	
	if( ctx->threadPtr )
	{
		err = SendSelfConnectedLoopbackMessage( ctx->cmdSock, "q", 1 );
		check_noerr( err );
		
		err = pthread_join( ctx->thread, NULL );
		check_noerr( err );
		ctx->threadPtr = NULL;
	}
	if( ctx->sendAudioThreadPtr )
    {
        pthread_mutex_lock( ctx->sendAudioMutexPtr );
        ctx->sendAudioDone = 1;
        pthread_cond_signal( ctx->sendAudioCondPtr );
        pthread_mutex_unlock( ctx->sendAudioMutexPtr );
        pthread_join( ctx->sendAudioThread, NULL );
        ctx->sendAudioThreadPtr = NULL;
        pthread_mutex_forget( &ctx->sendAudioMutexPtr );
        pthread_cond_forget( &ctx->sendAudioCondPtr );
    }
	if( ctx->zeroTimeLockPtr )
	{
		pthread_mutex_forget( &ctx->zeroTimeLockPtr );
	}
	ForgetSocket( &ctx->cmdSock );
	ForgetSocket( &ctx->dataSock );
	RTPJitterBufferFree( &ctx->jitterBuffer );
	AudioConverterForget( &ctx->inputConverter );
	ctx->inputRingRef = NULL;
	
	if( ctx == &inSession->mainAudioCtx )
	{
		inSession->flushing	= false;
		inSession->rtpAudioPort = 0;
		ForgetSocket( &inSession->rtcpSock );
		ForgetMem( &inSession->source.rtcpRTListStorage );
		ForgetMem( &inSession->nodeBufferStorage );
		ForgetMem( &inSession->nodeHeaderStorage );
		ForgetMem( &inSession->decodeBuffer );
		ForgetMem( &inSession->readBuffer );
		ForgetMem( &inSession->skewAdjustBuffer );
		AudioConverterForget( &inSession->audioConverter );
		inSession->source.receiveCount = 0;
	}
	(void) inIsFinalizing;
	ctx->session = NULL;
	ctx->connectionID = 0;
	if( ctx->type != kAirPlayStreamType_Invalid )
	{
		ctx->type = kAirPlayStreamType_Invalid;
		atr_ulog( kLogLevelTrace, "%s audio torn down\n", ctx->label );
	}
}

//===========================================================================================================================
//	_UpdateEstimatedRate
//===========================================================================================================================

static void	_UpdateEstimatedRate( AirPlayAudioStreamContext *ctx, uint32_t inSampleTime, uint64_t inHostTime )
{
	uint32_t					oldCount, newCount, oldNdx;
	AirPlayTimestampTuple *		newSample;
	AirPlayTimestampTuple *		oldSample;
	AirTunesTime				atTime;
	double						scale, rate;
	AirTunesClockRef			airTunesClock = ctx->session ? ctx->session->airTunesClock : NULL;
	
	if( inHostTime >= ctx->rateUpdateNextTicks )
	{
		oldCount = ctx->rateUpdateCount;
		oldNdx = oldCount % countof( ctx->rateUpdateSamples );
		newCount = oldCount + 1;
		AirTunesClock_GetSynchronizedTimeNearUpTicks( airTunesClock, &atTime, inHostTime );
		newSample				= &ctx->rateUpdateSamples[ oldNdx ];
		newSample->hostTime		= AirTunesTime_ToNTP( &atTime );
		newSample->hostTimeRaw	= UpTicksToNanoseconds( inHostTime );
		newSample->sampleTime	= inSampleTime;
		
		pthread_mutex_lock( ctx->zeroTimeLockPtr );
		ctx->zeroTime = *newSample;
		pthread_mutex_unlock( ctx->zeroTimeLockPtr );
		
		if( newCount >= 8 )
		{
			oldSample = &ctx->rateUpdateSamples[ newCount % Min( newCount, countof( ctx->rateUpdateSamples ) ) ];
			scale = ( newSample->hostTime - oldSample->hostTime ) * kNTPFraction;
			if( scale > 0 )
			{
				rate = ( newSample->sampleTime - oldSample->sampleTime ) / scale;
				ctx->rateAvg = (Float32) MovingAverageF( ctx->rateAvg, rate, 0.125 );
				atr_stats_ulog( ( kLogLevelVerbose + 1 ) | kLogLevelFlagDontRateLimit, "%s: Estimated rate: %.3f\n", 
					ctx->label, ctx->rateAvg );
			}
		}
		ctx->rateUpdateCount = newCount;
		ctx->rateUpdateNextTicks = inHostTime + ctx->rateUpdateIntervalTicks;
	}
}

#if 0
#pragma mark -
#pragma mark == Helpers ==
#endif

//===========================================================================================================================
//	AirPlayReceiverSessionChangeModes
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
	AirPlayReceiverSessionChangeModes( 
		AirPlayReceiverSessionRef					inSession, 
		const AirPlayModeChanges *					inChanges, 
		CFStringRef									inReason, 
		AirPlayReceiverSessionCommandCompletionFunc	inCompletion, 
		void *										inContext )
{
	OSStatus					err;
	CFMutableDictionaryRef		request;
	CFDictionaryRef				params;
	
	request = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( request, exit, err = kNoMemoryErr );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_Type ), CFSTR( kAirPlayCommand_ChangeModes ) );
	
	params = AirPlayCreateModesDictionary( inChanges, inReason, &err );
	require_noerr( err, exit );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_Params ), params );
	CFRelease( params );
	
	err = AirPlayReceiverSessionSendCommand( inSession, request, inCompletion, inContext );
	require_noerr( err, exit );
	
exit:
	CFReleaseNullSafe( request );
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionChangeAppState
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
	AirPlayReceiverSessionChangeAppState( 
		AirPlayReceiverSessionRef					inSession, 
		AirPlaySpeechMode							inSpeechMode, 
		AirPlayTriState								inPhoneCall, 
		AirPlayTriState								inTurnByTurn, 	
		CFStringRef									inReason, 
		AirPlayReceiverSessionCommandCompletionFunc	inCompletion, 
		void *										inContext )
{
	OSStatus				err;
	AirPlayModeChanges		changes;
	
	AirPlayModeChangesInit( &changes );
	if( inSpeechMode != kAirPlaySpeechMode_NotApplicable )	changes.speech		= inSpeechMode;
	if( inPhoneCall  != kAirPlayTriState_NotApplicable )	changes.phoneCall	= inPhoneCall;
	if( inTurnByTurn != kAirPlayTriState_NotApplicable )	changes.turnByTurn	= inTurnByTurn;
	
	err = AirPlayReceiverSessionChangeModes( inSession, &changes, inReason, inCompletion, inContext );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionChangeResourceMode
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
	AirPlayReceiverSessionChangeResourceMode( 
		AirPlayReceiverSessionRef					inSession, 
		AirPlayResourceID							inResourceID, 
		AirPlayTransferType							inType, 
		AirPlayTransferPriority						inPriority, 
		AirPlayConstraint							inTakeConstraint, 
		AirPlayConstraint							inBorrowOrUnborrowConstraint, 
		CFStringRef									inReason, 
		AirPlayReceiverSessionCommandCompletionFunc	inCompletion, 
		void *										inContext )
{
	OSStatus				err;
	AirPlayModeChanges		changes;
	
	AirPlayModeChangesInit( &changes );
	if( inResourceID == kAirPlayResourceID_MainScreen )
	{
		changes.screen.type								= inType;
		changes.screen.priority							= inPriority;
		changes.screen.takeConstraint					= inTakeConstraint;
		changes.screen.borrowOrUnborrowConstraint		= inBorrowOrUnborrowConstraint;
	}
	else if( inResourceID == kAirPlayResourceID_MainAudio )
	{
		changes.mainAudio.type							= inType;
		changes.mainAudio.priority						= inPriority;
		changes.mainAudio.takeConstraint				= inTakeConstraint;
		changes.mainAudio.borrowOrUnborrowConstraint	= inBorrowOrUnborrowConstraint;
	}
	else
	{
		dlogassert( "Bad resource ID: %d\n", inResourceID );
		err = kParamErr;
		goto exit;
	}
	
	err = AirPlayReceiverSessionChangeModes( inSession, &changes, inReason, inCompletion, inContext );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionMakeModeStateFromDictionary
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
	AirPlayReceiverSessionMakeModeStateFromDictionary( 
		AirPlayReceiverSessionRef	inSession, 
		CFDictionaryRef				inDict, 
		AirPlayModeState *			outModes )
{
	OSStatus			err;
	CFArrayRef			array;
	CFIndex				i, n;
	CFDictionaryRef		dict;
	int					x;
	
	(void) inSession;
	
	AirPlayModeStateInit( outModes );
	
	// AppStates
	
	array = CFDictionaryGetCFArray( inDict, CFSTR( kAirPlayKey_AppStates ), NULL );
	n = array ? CFArrayGetCount( array ) : 0;
	for( i = 0; i < n; ++i )
	{
		dict = CFArrayGetCFDictionaryAtIndex( array, i, &err );
		require_noerr( err, exit );
		
		x = (int) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_AppStateID ), NULL );
		switch( x )
		{
			case kAirPlayAppStateID_PhoneCall:
				x = (int) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_Entity ), &err );
				require_noerr( err, exit );
				outModes->phoneCall = x;
				break;
			
			case kAirPlayAppStateID_Speech:
				x = (int) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_Entity ), &err );
				require_noerr( err, exit );
				outModes->speech.entity = x;
				
				x = (int) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_SpeechMode ), &err );
				require_noerr( err, exit );
				outModes->speech.mode = x;
				break;
			
			case kAirPlayAppStateID_TurnByTurn:
				x = (int) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_Entity ), &err );
				require_noerr( err, exit );
				outModes->turnByTurn = x;
				break;
			
			case kAirPlayAppStateID_NotApplicable:
				break;
			
			default:
				atr_ulog( kLogLevelNotice, "### Ignoring unknown app state %@\n", dict );
				break;
		}
	}
	
	// Resources
	
	array = CFDictionaryGetCFArray( inDict, CFSTR( kAirPlayKey_Resources ), NULL );
	n = array ? CFArrayGetCount( array ) : 0;
	for( i = 0; i < n; ++i )
	{
		dict = CFArrayGetCFDictionaryAtIndex( array, i, &err );
		require_noerr( err, exit );
		
		x = (AirPlayAppStateID) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_ResourceID ), NULL );
		switch( x )
		{
			case kAirPlayResourceID_MainScreen:
				x = (int) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_Entity ), &err );
				require_noerr( err, exit );
				outModes->screen = x;
				break;
			
			case kAirPlayResourceID_MainAudio:
				x = (int) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_Entity ), &err );
				require_noerr( err, exit );
				outModes->mainAudio = x;
				break;
			
			case kAirPlayResourceID_NotApplicable:
				break;
			
			default:
				atr_ulog( kLogLevelNotice, "### Ignoring unknown resource state %@\n", dict );
				break;
		}
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionForceKeyFrame
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
	AirPlayReceiverSessionForceKeyFrame(
		AirPlayReceiverSessionRef					inSession,
		AirPlayReceiverSessionCommandCompletionFunc	inCompletion,
		void *										inContext )
{
	OSStatus					err;
	CFMutableDictionaryRef		request;
	
	request = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( request, exit, err = kNoMemoryErr );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_Type ), CFSTR( kAirPlayCommand_ForceKeyFrame ) );
	err = AirPlayReceiverSessionSendCommand( inSession, request, inCompletion, inContext );
	require_noerr( err, exit );
	
exit:
	CFReleaseNullSafe( request );
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionRequestSiriAction
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
	AirPlayReceiverSessionRequestSiriAction(
		AirPlayReceiverSessionRef					inSession,
		AirPlaySiriAction							inAction,
		AirPlayReceiverSessionCommandCompletionFunc	inCompletion,
		void *										inContext )
{
	OSStatus					err;
	CFMutableDictionaryRef		request;
	CFMutableDictionaryRef		params;
	
	request = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( request, exit, err = kNoMemoryErr );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_Type ), CFSTR( kAirPlayCommand_RequestSiri ) );
	
	params = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( params, exit, err = kNoMemoryErr );
	CFDictionarySetInt64( params, CFSTR( kAirPlayKey_SiriAction ), (int64_t) inAction );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_Params ), params );
	CFRelease( params );
	
	err = AirPlayReceiverSessionSendCommand( inSession, request, inCompletion, inContext );
	require_noerr( err, exit );
	
exit:
	CFReleaseNullSafe( request );
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionRequestUI
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
	AirPlayReceiverSessionRequestUI( 
		AirPlayReceiverSessionRef					inSession, 
		CFStringRef									inURL, 
		AirPlayReceiverSessionCommandCompletionFunc	inCompletion, 
		void *										inContext )
{
	OSStatus					err;
	CFMutableDictionaryRef		request;
	CFMutableDictionaryRef		params;
	
	request = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( request, exit, err = kNoMemoryErr );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_Type ), CFSTR( kAirPlayCommand_RequestUI ) );
	
	if( inURL )
	{
		params = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( params, exit, err = kNoMemoryErr );
		CFDictionarySetValue( params, CFSTR( kAirPlayKey_URL ), inURL );
		CFDictionarySetValue( request, CFSTR( kAirPlayKey_Params ), params );
		CFRelease( params );
	}
	
	err = AirPlayReceiverSessionSendCommand( inSession, request, inCompletion, inContext );
	require_noerr( err, exit );
	
exit:
	CFReleaseNullSafe( request );
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetNightMode
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionSetNightMode(
	AirPlayReceiverSessionRef					inSession,
	Boolean										inNightMode,
	AirPlayReceiverSessionCommandCompletionFunc	inCompletion,
	void *										inContext )
{
	OSStatus					err;
	CFMutableDictionaryRef		request;
	CFMutableDictionaryRef		params;
	
	request = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( request, exit, err = kNoMemoryErr );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_Type ), CFSTR( kAirPlayCommand_SetNightMode ) );
	
	params = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( params, exit, err = kNoMemoryErr );
	CFDictionarySetValue( params, CFSTR( kAirPlayKey_NightMode ), inNightMode ? kCFBooleanTrue : kCFBooleanFalse );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_Params ), params );
	CFRelease( params );
	
	err = AirPlayReceiverSessionSendCommand( inSession, request, inCompletion, inContext );
	require_noerr( err, exit );
	
exit:
	CFReleaseNullSafe( request );
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetLimitedUI
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionSetLimitedUI(
	AirPlayReceiverSessionRef						inSession,
	Boolean											inLimitUI,
	AirPlayReceiverSessionCommandCompletionFunc		inCompletion,
	void *											inContext )
{
	OSStatus					err;
	CFMutableDictionaryRef		request;
	CFMutableDictionaryRef		params;
	
	request = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( request, exit, err = kNoMemoryErr );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_Type ), CFSTR( kAirPlayCommand_SetLimitedUI ) );
	
	params = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( params, exit, err = kNoMemoryErr );
	CFDictionarySetValue( params, CFSTR( kAirPlayKey_LimitedUI ), inLimitUI ? kCFBooleanTrue : kCFBooleanFalse );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_Params ), params );
	CFRelease( params );
	
	err = AirPlayReceiverSessionSendCommand( inSession, request, inCompletion, inContext );
	require_noerr( err, exit );
	
exit:
	CFReleaseNullSafe( request );
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionSendiAPMessage
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionSendiAPMessage(
	AirPlayReceiverSessionRef					inSession, 
	CFDataRef									inMessageData,
	AirPlayReceiverSessionCommandCompletionFunc	inCompletion, 
	void *										inContext )
{
	OSStatus					err;
	CFMutableDictionaryRef		request;
	CFMutableDictionaryRef		params;
	
	request = NULL;
	
	// iAP Over AirPlay is a Wireless only feature
	
	require_action( NetTransportTypeIsWireless( inSession->transportType ), exit, err = kUnsupportedErr );
	
	request = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( request, exit, err = kNoMemoryErr );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_Type ), CFSTR( kAirPlayCommand_iAPSendMessage ) );
	
	params = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( params, exit, err = kNoMemoryErr );
	CFDictionarySetValue( params, CFSTR( kAirPlayKey_Data ), inMessageData );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_Params ), params );
	CFRelease( params );
	
	err = AirPlayReceiverSessionSendCommand( inSession, request, inCompletion, inContext );
	require_noerr( err, exit );
	
exit:
	CFReleaseNullSafe( request );
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionUpdateVehicleInformation
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionUpdateVehicleInformation(
	AirPlayReceiverSessionRef					inSession, 
	CFDictionaryRef								inVehicleInformation,
	AirPlayReceiverSessionCommandCompletionFunc	inCompletion, 
	void *										inContext )
{
	OSStatus					err;
	CFMutableDictionaryRef		request;
	CFMutableDictionaryRef		params;
	
	request = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( request, exit, err = kNoMemoryErr );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_Type ), CFSTR( kAirPlayCommand_UpdateVehicleInformation ) );

	params = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( params, exit, err = kNoMemoryErr );
	CFDictionarySetValue( params, CFSTR( kAirPlayKey_VehicleInformation ), inVehicleInformation );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_Params ), params );
	CFRelease( params );
	
	err = AirPlayReceiverSessionSendCommand( inSession, request, inCompletion, inContext );
	require_noerr( err, exit );
	
exit:
	CFReleaseNullSafe( request );
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionSendHIDReport
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionSendHIDReport(
	AirPlayReceiverSessionRef					inSession,
	uint32_t									inDeviceUID,
	const uint8_t *								inPtr,
	size_t										inLen )
{
	OSStatus					err;
	CFMutableDictionaryRef		request;
	CFStringRef					uid;
	
	uid = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%X" ), inDeviceUID );
	require_action( uid, exit, err = kNoMemoryErr );
	
	request = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( request, exit, err = kNoMemoryErr );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_Type ), CFSTR( kAirPlayCommand_HIDSendReport ) );
	CFDictionarySetValue( request, CFSTR( kAirPlayKey_UUID ), uid );
	CFRelease(uid);
	CFDictionarySetData( request, CFSTR( kAirPlayKey_HIDReport ), inPtr, inLen );
	
	err = AirPlayReceiverSessionSendCommand( inSession, request, NULL, NULL );
	CFRelease( request );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
// AirPlayInfoArrayAddHIDDevice
//===========================================================================================================================

OSStatus
AirPlayInfoArrayAddHIDDevice(
	CFArrayRef *			inArray,
	uint32_t				inDeviceUID,
	const char *			inName,
	uint16_t				inVendorID,
	uint16_t				inProductID,
	uint16_t				inCountryCode,
	const uint8_t *			inDescPtr,
	size_t					inDescLen,
	CFStringRef				inDisplayUUID )
{
	OSStatus					err;
	CFMutableArrayRef			dictArray;
	CFMutableDictionaryRef		dict = NULL;
	CFStringRef					uidStr;
	CFDataRef					descData;
	CFStringRef					nameStr = NULL;

	if( *inArray == NULL )
	{
		*inArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		require_action( *inArray, exit, err = kNoMemoryErr );
	}
	dictArray = (CFMutableArrayRef)*inArray;

	dict = CFDictionaryCreateMutable( NULL, 0,  &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( dict, exit, err = kNoMemoryErr );

	uidStr = CFStringCreateF( &err, "%X", inDeviceUID );
	require_action( uidStr, exit, err = kNoMemoryErr );
	CFDictionarySetValue( dict, CFSTR( kAirPlayKey_UUID ), uidStr );
	CFRelease( uidStr );

	nameStr = CFStringCreateWithCString( kCFAllocatorDefault, inName, kCFStringEncodingUTF8 );
	require_action( dict, exit, err = kNoMemoryErr );
	CFDictionarySetValue( dict, CFSTR( kAirPlayKey_Name ), nameStr );
	CFRelease( nameStr );

	CFDictionarySetValue( dict, CFSTR( kAirPlayKey_DisplayUUID ), inDisplayUUID );
	CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_HIDProductID ), inProductID );
	CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_HIDVendorID ), inVendorID );
	CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_HIDCountryCode ), inCountryCode );
	
	descData = CFDataCreate( NULL, inDescPtr, inDescLen );
	require_action( descData, exit, err = kNoMemoryErr );
	CFDictionarySetValue( dict, CFSTR( kAirPlayKey_HIDDescriptor ), descData );
	CFRelease( descData );
	
	CFArrayAppendValue( dictArray, dict );
	err = kNoErr;

exit:
	CFReleaseNullSafe( dict );
	return( err );
}

//===========================================================================================================================
//	AirPlayInfoArrayAddAudioLatency
//===========================================================================================================================

OSStatus
AirPlayInfoArrayAddAudioLatency(
	CFArrayRef *			inArray,
	AudioStreamType			inStreamType,
	CFStringRef				inAudioType,
	uint32_t				inSampleRate,
	uint32_t				inSampleSize,
	uint32_t				inChannels,
	uint32_t				inInputLatency,
	uint32_t				inOutputLatency )
{
	OSStatus				err;
	CFMutableArrayRef		dictArray;
	CFMutableDictionaryRef	dict = NULL;

	if( *inArray == NULL )
	{
		*inArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		require_action( *inArray, exit, err = kNoMemoryErr );
	}
	dictArray = (CFMutableArrayRef)*inArray;
	
	dict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( dict, exit, err = kNoMemoryErr );

	if( inStreamType != kAudioStreamType_Invalid )
		CFDictionarySetInt64( dict, kAudioSessionKey_Type, inStreamType );
	if( inAudioType )
		CFDictionarySetValue( dict, kAudioSessionKey_AudioType, inAudioType );
	if( inSampleRate )
		CFDictionarySetInt64( dict, kAudioSessionKey_SampleRate, inSampleRate );
	if( inSampleSize )
		CFDictionarySetInt64( dict, kAudioSessionKey_SampleSize, inSampleSize );
	if( inChannels )
		CFDictionarySetInt64( dict, kAudioSessionKey_Channels, inChannels );
	CFDictionarySetInt64( dict, kAudioSessionKey_InputLatencyMicros, inInputLatency );
	CFDictionarySetInt64( dict, kAudioSessionKey_OutputLatencyMicros, inOutputLatency );

	CFArrayAppendValue( dictArray, dict );
	err = kNoErr;

exit:
	CFReleaseNullSafe( dict );
	return( err );
}

//===========================================================================================================================
//	AirPlayInfoArrayAddAudioFormat
//===========================================================================================================================

OSStatus
AirPlayInfoArrayAddAudioFormat(
	CFArrayRef *			inArray,
	AudioStreamType			inStreamType,
	CFStringRef				inAudioType,
	AirPlayAudioFormat		inInputFormats,
	AirPlayAudioFormat		inOutputFormats )
{
	OSStatus				err;
	CFMutableArrayRef		dictArray;
	CFMutableDictionaryRef	dict = NULL;

	if( *inArray == NULL )
	{
		*inArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		require_action( *inArray, exit, err = kNoMemoryErr );
	}
	dictArray = (CFMutableArrayRef)*inArray;
	
	dict = CFDictionaryCreateMutable( NULL, 0,  &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( dict, exit, err = kNoMemoryErr );

	if( inStreamType != kAudioStreamType_Invalid )
		CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_Type ), inStreamType );
	if( inAudioType )
		CFDictionarySetValue( dict, CFSTR( kAirPlayKey_AudioType ), inAudioType );
	if( inInputFormats )
		CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_AudioInputFormats ), inInputFormats );
	if( inOutputFormats )
		CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_AudioOutputFormats ), inOutputFormats );

	CFArrayAppendValue( dictArray, dict );
	err = kNoErr;

exit:
	CFReleaseNullSafe( dict );
	return( err );
}

//===========================================================================================================================
//	AirPlayInfoArrayAddScreenDisplay
//===========================================================================================================================

OSStatus
AirPlayInfoArrayAddScreenDisplay(
	CFArrayRef *						inArray,
	CFStringRef							inUUID,
	AirPlayDisplayFeatures				inFeatures,
	AirPlayDisplayPrimaryInputDevice	inPrimaryDevice,
	uint32_t							inMaxFPS,
	uint32_t							inWidthPixels,
	uint32_t							inHeightPixels,
	uint32_t							inWidthPhysical,
	uint32_t							inHeightPhysical )
{
	OSStatus				err;
	CFMutableArrayRef		dictArray;
	CFMutableDictionaryRef	dict = NULL;

	if( *inArray == NULL )
	{
		*inArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		require_action( *inArray, exit, err = kNoMemoryErr );
	}
	dictArray = (CFMutableArrayRef)*inArray;
	
	dict = CFDictionaryCreateMutable( NULL, 0,  &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( dict, exit, err = kNoMemoryErr );

	CFDictionarySetValue( dict, CFSTR( kAirPlayKey_UUID ), inUUID );
	CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_DisplayFeatures ), inFeatures );
	CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_PrimaryInputDevice ), inPrimaryDevice );
	if( inMaxFPS )
		CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_MaxFPS ), inMaxFPS );
	CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_WidthPixels ), inWidthPixels );
	CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_HeightPixels ), inHeightPixels );
	CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_WidthPhysical ), inWidthPhysical );
	CFDictionarySetInt64( dict, CFSTR( kAirPlayKey_HeightPhysical ), inHeightPhysical );

	CFArrayAppendValue( dictArray, dict );
	err = kNoErr;

exit:
	CFReleaseNullSafe( dict );
	return( err );
}
