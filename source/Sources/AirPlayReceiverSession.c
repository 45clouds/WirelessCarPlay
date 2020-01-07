/*
	File:    	AirPlayReceiverSession.c
	Package: 	CarPlay Communications Plug-in.
	Abstract: 	n/a 
	Version: 	280.33.8
	
	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
	Apple software is governed by and subject to the terms and conditions of your MFi License,
	including, but not limited to, the restrictions specified in the provision entitled ”Public 
	Software�? and is further subject to your agreement to the following additional terms, and your 
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
	fixes or enhancements to Apple in connection with this software (“Feedback�?, you hereby grant to
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
	
	Copyright (C) 2005-2015 Apple Inc. All Rights Reserved.
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

#include <CoreUtils/AESUtils.h>
#include <CoreUtils/CFUtils.h>
#include <CoreUtils/CommonServices.h>
#include <CoreUtils/DebugServices.h>
#include <CoreUtils/HTTPClient.h>
#include <CoreUtils/HTTPUtils.h>
#include <CoreUtils/MathUtils.h>
#include <CoreUtils/NetTransportChaCha20Poly1305.h>
#include <CoreUtils/NetUtils.h>
#include <CoreUtils/NTPUtils.h>
#include <CoreUtils/PrintFUtils.h>
#include <CoreUtils/RandomNumberUtils.h>
#include <CoreUtils/StringUtils.h>
#include <CoreUtils/SystemUtils.h>
#include <CoreUtils/TickUtils.h>
#include <CoreUtils/TimeUtils.h>
#include <CoreUtils/UUIDUtils.h>

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
#include "AirPlaySettings.h"
#include "AirPlayUtils.h"
#include "AirTunesClock.h"

#include CF_HEADER

	#include "APSAudioConverter.h"
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
#include <glib.h>
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
		debug_sub( gAirTunesDebugBusyNodeCount, 1 );		\
															\
	}	while( 0 )

#define NanosecondsToMilliseconds32( NANOS )	\
	( (uint32_t)( ( (NANOS) == UINT64_MAX ) ? 0 : ( (NANOS) / kNanosecondsPerMillisecond ) ) )

// General

static void		_GetTypeID( void *inContext );
static void		_Finalize( CFTypeRef inCF );
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
#define 		_RetransmitsDisabled( ME )	( (ME)->redundantAudio || debug_false_conditional( gAirTunesDebugNoRetransmits ) )

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
static void
	_ScreenHandleEvent(
		AirPlayReceiverSessionScreenRef		inSession,
		CFStringRef							inEventName,
		CFDictionaryRef						inEventData,
		void *								inUserData );
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

#if( DEBUG )
	#define	airtunes_record_clock_offset( X )																			\
		do																												\
		{																												\
			gAirTunesClockOffsetHistory[ gAirTunesClockOffsetIndex++ ] = ( X );											\
			if( gAirTunesClockOffsetIndex >= countof( gAirTunesClockOffsetHistory ) ) gAirTunesClockOffsetIndex = 0;	\
			if( gAirTunesClockOffsetCount < countof( gAirTunesClockOffsetHistory ) ) ++gAirTunesClockOffsetCount;		\
																														\
		}	while( 0 )
#else
	#define	airtunes_record_clock_offset( X )
#endif

#if( DEBUG )
	#define	airtunes_record_clock_rtt( X )																		\
		do																										\
		{																										\
			gAirTunesClockRTTHistory[ gAirTunesClockRTTIndex++ ] = ( X );										\
			if( gAirTunesClockRTTIndex >= countof( gAirTunesClockRTTHistory ) ) gAirTunesClockRTTIndex = 0;		\
			if( gAirTunesClockRTTCount < countof( gAirTunesClockRTTHistory ) ) ++gAirTunesClockRTTCount;		\
																												\
		}	while( 0 )
#else
	#define	airtunes_record_clock_rtt( X )
#endif

#if( DEBUG )
	#define	airtunes_record_rtp_offset( X )																			\
		do																											\
		{																											\
			gAirTunesRTPOffsetHistory[ gAirTunesRTPOffsetIndex++ ] = ( X );											\
			if( gAirTunesRTPOffsetIndex >= countof( gAirTunesRTPOffsetHistory ) ) gAirTunesRTPOffsetIndex = 0;		\
			if( gAirTunesRTPOffsetCount < countof( gAirTunesRTPOffsetHistory ) ) ++gAirTunesRTPOffsetCount;			\
																													\
		}	while( 0 )
#else
	#define	airtunes_record_rtp_offset( X )
#endif

#if( DEBUG )
	#define	airtunes_record_retransmit( RT_NODE, REASON, FINAL_NANOS )												\
		do																											\
		{																											\
			AirTunesRetransmitHistoryNode *		rthNode;															\
			size_t								rthI;																\
																													\
			rthNode = &gAirTunesRetransmitHistory[ gAirTunesRetransmitIndex++ ];									\
			rthNode->reason 	= ( REASON );																		\
			rthNode->seq		= ( RT_NODE )->seq;																	\
			rthNode->tries		= ( RT_NODE )->tries;																\
			rthNode->finalNanos	= ( FINAL_NANOS );																	\
			for( rthI = 0; rthI < countof( rthNode->tryNanos ); ++rthI )											\
			{																										\
				rthNode->tryNanos[ rthI ] = ( RT_NODE )->tryNanos[ rthI ];											\
			}																										\
			if( gAirTunesRetransmitIndex >= countof( gAirTunesRetransmitHistory ) ) gAirTunesRetransmitIndex = 0;	\
			if( gAirTunesRetransmitCount < countof( gAirTunesRetransmitHistory ) ) ++gAirTunesRetransmitCount;		\
																													\
		}	while( 0 )
#else
	#define	airtunes_record_retransmit( RT_NODE, REASON, FINAL_NANOS )
#endif

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

// Debugging

#if( DEBUG )
	FILE *					gAirTunesFile									= NULL;
	
	// Control Variables
	
	int						gAirTunesDropMinRate							= 0;
	int						gAirTunesDropMaxRate							= 0;
	int						gAirTunesDropRemaining							= 0;
	int						gAirTunesSkipMinRate							= 0;
	int						gAirTunesSkipMaxRate							= 0;
	int						gAirTunesSkipRemaining							= 0;
	int						gAirTunesLateDrop								= 0;
	int						gAirTunesDebugNoSkewSlew						= 0;
	int						gAirTunesDebugLogAllSkew						= 0;
	int						gAirTunesDebugNoRetransmits						= 0;
	int						gAirTunesDebugPrintPerf							= 0;
	int						gAirTunesDebugPerfMode							= 0;
	int						gAirTunesDebugRetransmitTiming					= 1;
	
	// Stats
	
	unsigned int			gAirTunesDebugBusyNodeCount						= 0;
	unsigned int			gAirTunesDebugBusyNodeCountLast					= 0;
	unsigned int			gAirTunesDebugBusyNodeCountMax					= 0;
	
	uint64_t				gAirTunesDebugSentByteCount						= 0;
	uint64_t				gAirTunesDebugRecvByteCount						= 0;
	uint64_t				gAirTunesDebugRecvRTPOriginalByteCount			= 0;
	uint64_t				gAirTunesDebugRecvRTPOriginalByteCountLast		= 0;
	uint64_t				gAirTunesDebugRecvRTPOriginalBytesPerSecAvg		= 0;
	uint64_t				gAirTunesDebugRecvRTPRetransmitByteCount		= 0;
	uint64_t				gAirTunesDebugRecvRTPRetransmitByteCountLast	= 0;
	uint64_t				gAirTunesDebugRecvRTPRetransmitBytesPerSecAvg	= 0;
	unsigned int			gAirTunesDebugIdleTimeoutCount					= 0;
	unsigned int			gAirTunesDebugStolenNodeCount					= 0;
	unsigned int			gAirTunesDebugOldDiscardCount					= 0;
	unsigned int			gAirTunesDebugConcealedGapCount					= 0;
	unsigned int			gAirTunesDebugConcealedEndCount					= 0;
	unsigned int			gAirTunesDebugLateDropCount						= 0;
	unsigned int			gAirTunesDebugSameTimestampCount				= 0;
	unsigned int			gAirTunesDebugLossCounts[ 10 ]					= { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	unsigned int			gAirTunesDebugTotalLossCount					= 0;
	unsigned int			gAirTunesDebugMaxBurstLoss						= 0;
	unsigned int			gAirTunesDebugDupCount							= 0;
	unsigned int			gAirTunesDebugMisorderedCount					= 0;
	unsigned int			gAirTunesDebugUnrecoveredPacketCount			= 0;
	unsigned int			gAirTunesDebugUnexpectedRTPOffsetResetCount 	= 0;
	unsigned int			gAirTunesDebugHugeSkewResetCount				= 0;
	unsigned int			gAirTunesDebugGlitchCount			 			= 0;
	unsigned int			gAirTunesDebugTimeSyncHugeRTTCount				= 0;
	uint64_t				gAirTunesDebugTimeAnnounceMinNanos				= UINT64_C( 0xFFFFFFFFFFFFFFFF );
	uint64_t				gAirTunesDebugTimeAnnounceMaxNanos				= 0;
	unsigned int			gAirTunesDebugRetransmitActiveCount				= 0;
	unsigned int			gAirTunesDebugRetransmitActiveMax				= 0;
	unsigned int			gAirTunesDebugRetransmitSendCount				= 0;
	unsigned int			gAirTunesDebugRetransmitSendLastCount			= 0;
	unsigned int			gAirTunesDebugRetransmitSendPerSecAvg			= 0;
	unsigned int			gAirTunesDebugRetransmitRecvCount				= 0;
	unsigned int			gAirTunesDebugRetransmitBigLossCount			= 0;
	unsigned int			gAirTunesDebugRetransmitAbortCount				= 0;
	unsigned int			gAirTunesDebugRetransmitFutileAbortCount		= 0;
	unsigned int			gAirTunesDebugRetransmitNoFreeNodesCount		= 0;
	unsigned int			gAirTunesDebugRetransmitNotFoundCount			= 0;
	unsigned int			gAirTunesDebugRetransmitPrematureCount			= 0;
	unsigned int			gAirTunesDebugRetransmitMaxTries				= 0;
	uint64_t				gAirTunesDebugRetransmitMinNanos				= UINT64_C( 0xFFFFFFFFFFFFFFFF );
	uint64_t				gAirTunesDebugRetransmitMaxNanos				= 0;
	uint64_t				gAirTunesDebugRetransmitAvgNanos				= 0;
	uint64_t				gAirTunesDebugRetransmitRetryMinNanos			= UINT64_C( 0xFFFFFFFFFFFFFFFF );
	uint64_t				gAirTunesDebugRetransmitRetryMaxNanos			= 0;
	
	double					gAirTunesClockOffsetHistory[ 512 ];
	unsigned int			gAirTunesClockOffsetIndex 						= 0;
	unsigned int			gAirTunesClockOffsetCount 						= 0;
	
	double					gAirTunesClockRTTHistory[ 512 ];
	unsigned int			gAirTunesClockRTTIndex 							= 0;
	unsigned int			gAirTunesClockRTTCount 							= 0;
	
	uint32_t				gAirTunesRTPOffsetHistory[ 1024 ];
	unsigned int			gAirTunesRTPOffsetIndex 						= 0;
	unsigned int			gAirTunesRTPOffsetCount 						= 0;
	
	AirTunesRetransmitHistoryNode	gAirTunesRetransmitHistory[ 1024 ];
	size_t							gAirTunesRetransmitIndex 				= 0;
	size_t							gAirTunesRetransmitCount 				= 0;
	
	// Transients
	
	uint64_t				gAirTunesDebugLastPollTicks						= 0;
	uint64_t				gAirTunesDebugPollIntervalTicks					= 0;
	uint64_t				gAirTunesDebugSentByteCountLast					= 0;
	uint64_t				gAirTunesDebugRecvByteCountLast					= 0;
	uint16_t				gAirTunesDebugHighestSeqLast					= 0;
	uint32_t				gAirTunesDebugTotalLossCountLast				= 0;
	uint32_t				gAirTunesDebugRecvCountLast						= 0;
	uint32_t				gAirTunesDebugGlitchCountLast					= 0;
#endif

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
	
	err = AirPlayReceiverSessionScreen_Create( &me->screenSession );
	require_noerr( err, exit );
	
	AirPlayReceiverSessionScreen_SetEventHandler( me->screenSession, _ScreenHandleEvent, me, NULL );
	
	AirPlayReceiverSessionScreen_SetOverscanOverride( me->screenSession, me->server->overscanOverride );
	
	AirPlayReceiverSessionScreen_SetClientIfMACAddr( me->screenSession, (uint8_t *) inParams->clientIfMACAddr, sizeof( inParams->clientIfMACAddr ) );
	
	AirPlayReceiverSessionScreen_SetIFName( me->screenSession, (char *) inParams->ifName );
	
	AirPlayReceiverSessionScreen_SetTransportType( me->screenSession, me->transportType );
	
	// Finish initialization.
	
	err = pthread_mutex_init( &me->mutex, NULL );
	require_noerr( err, exit );
	me->mutexPtr = &me->mutex;
	
	me->flushRecentTicks			= SecondsToUpTicks( 5 );
	me->flushLastTicks				= ticks;
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
	ats->rtcpTIClockRTTLeastBad		= +1000000.0;
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
		atr_ulog( kLogLevelNotice, "iAP Send Message\n" );
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
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_DeviceID ) ) )//Get wifi mac addr
	{
		value = CFStringCreateWithBytes( NULL, session->clientIfMACAddr, 6, kCFStringEncodingUTF8, false );
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
	CFDataRef					data;
	
	context = (SendCommandContext*) inCtx;
	
	require_action_quiet( context->session->eventClient, exit, err = kUnsupportedErr );
	
	err = HTTPMessageCreate( &msg );
	require_noerr( err, exit );
	
	err = HTTPHeader_InitRequest( &msg->header, "POST", kAirPlayCommandPath, kAirTunesHTTPVersionStr );
	require_noerr( err, exit );
	
	data = CFPropertyListCreateData( NULL, context->request, kCFPropertyListBinaryFormat_v1_0, 0, NULL );
	require_action( data, exit, err = kUnknownErr );
	err = HTTPMessageSetBody( msg, kMIMEType_AppleBinaryPlist, CFDataGetBytePtr( data ), (size_t) CFDataGetLength( data ) );
	CFRelease( data );
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
	OSStatus												err;
	CFDictionaryRef											response;
	
	response = CFDictionaryCreateWithBytes( inMsg->bodyPtr, inMsg->bodyLen, &err );
	require_noerr( err, exit );
	
	err = (OSStatus) CFDictionaryGetInt64( response, CFSTR( kAirPlayKey_Status ), NULL );
	require_noerr_quiet( err, exit );
	
exit:
	if( completion ) completion( err, err ? NULL : response, inMsg->userContext3 );
	CFRelease( session );
	CFReleaseNullSafe( response );
}

#if( COMPILER_HAS_BLOCKS )
//===========================================================================================================================
//	AirPlayReceiverSessionSendCommand_b
//===========================================================================================================================

static void	_AirPlayReceiverSessionSendCommandCompletion_b( OSStatus inStatus, CFDictionaryRef inResponse, void *inContext );

EXPORT_GLOBAL
OSStatus
	AirPlayReceiverSessionSendCommand_b( 
		AirPlayReceiverSessionRef						inSession, 
		CFDictionaryRef									inRequest, 
		AirPlayReceiverSessionCommandCompletionBlock	inCompletion )
{
	OSStatus											err;
	AirPlayReceiverSessionCommandCompletionBlock		completion;
	
	if( inCompletion )
	{
		completion = Block_copy( inCompletion );
		require_action( completion, exit, err = kNoMemoryErr );
		
		err = AirPlayReceiverSessionSendCommand( inSession, inRequest, 
			_AirPlayReceiverSessionSendCommandCompletion_b, (void *) completion );
		if( err ) Block_release( completion );
		require_noerr( err, exit );
	}
	else
	{
		err = AirPlayReceiverSessionSendCommand( inSession, inRequest, NULL, NULL );
		require_noerr( err, exit );
	}
	
exit:
	return( err );
}

static void	_AirPlayReceiverSessionSendCommandCompletion_b( OSStatus inStatus, CFDictionaryRef inResponse, void *inContext )
{
	AirPlayReceiverSessionCommandCompletionBlock	completion = (AirPlayReceiverSessionCommandCompletionBlock) inContext;
	
	completion( inStatus, inResponse );
	Block_release( completion );
}
#endif // COMPILER_HAS_BLOCKS

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
	CFStringRef					tempCFStr;
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
		strcpy( me->clientOSBuildVersion, clientOSBuildVersion );
		err = AirPlayGetMinimumClientOSBuildVersion( minClientOSBuildVersion, sizeof( minClientOSBuildVersion ) );
		if( !err )
		{
			if( _CompareOSBuildVersionStrings( minClientOSBuildVersion, clientOSBuildVersion ) > 0 )
				require_noerr( err = kVersionErr, exit );
		}
	}
		
	// Save off client info.
	
		tempCFStr = CFDictionaryGetCFString( inRequestParams, CFSTR( kAirPlayKey_ModelCode ), &err );
		if( tempCFStr ) AirPlayReceiverSessionScreen_SetClientModelCode( me->screenSession, tempCFStr );
		
		tempCFStr = CFDictionaryGetCFString( inRequestParams, CFSTR( kAirPlayKey_UDID ), &err );
		if( tempCFStr ) AirPlayReceiverSessionScreen_SetClientDeviceUDID( me->screenSession, tempCFStr );
		
		tempCFStr = CFDictionaryGetCFString( inRequestParams, CFSTR( kAirPlayKey_OSBuildVersion ), &err );
		if( tempCFStr ) AirPlayReceiverSessionScreen_SetClientOSBuildVersion( me->screenSession, tempCFStr );
	
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
	
	if( streamCount > 0 )
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
//	AirPlayReceiverSessionTearDown
//===========================================================================================================================

static void
	_AirPlayReceiverSessionReplaceEventHandlerSynchronouslyOnSessionScreen( void *inArg )
{
	AirPlayReceiverSessionRef inSession = (AirPlayReceiverSessionRef) inArg;
	AirPlayReceiverSessionScreen_ReplaceEventHandlerSynchronously( inSession->screenSession, NULL, NULL, NULL );
	CFRelease( inSession );
}

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
				if( inSession->screenSession != NULL ) {
					CFRetain( inSession );
                    dispatch_async_f( dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_DEFAULT, 0 ), inSession, _AirPlayReceiverSessionReplaceEventHandlerSynchronouslyOnSessionScreen );
                }
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
	
	dispatch_source_forget( &inSession->periodicTimer );
	_ScreenTearDown( inSession );
	_TearDownStream( inSession, &inSession->altAudioCtx, false );
	_TearDownStream( inSession, &inSession->mainAudioCtx, false );
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
	
	// Tell the platform to flush first so it can quiet the audio immediately.
	
	AirPlayReceiverSessionPlatformControl( inSession, kCFObjectFlagDirect, CFSTR( kAirPlayCommand_FlushAudio ), 
		NULL, NULL, NULL );
	
	// Reset state so we don't play until we get post-flush timelines, etc.
	
	inSession->flushing				= true;
	inSession->flushLastTicks		= UpTicks();
	inSession->flushTimeoutTS		= inFlushUntilTS + ( 3 * ctx->sampleRate ); // 3 seconds.
	inSession->flushUntilTS			= inFlushUntilTS;
	inSession->lastPlayedValid		= false;
	ats->rtcpRTDisable				= _RetransmitsDisabled( inSession );
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
static void  _AirPlayReceiver_SendAudio(
        AirPlayAudioStreamContext * const ctx,
        AirPlayStreamType		   inType,
        uint32_t                   inSampleTime )
{
    RTPSavedPacket							pkt;
    size_t									avail, len;
    size_t									maxPayloadSize;
    MirroredRingBuffer * const				ring    = &ctx->inputRing;
    uint16_t								seq		= ctx->inputSeqNum;
    uint32_t								ts		= ctx->inputTimestamp;
    uint32_t								spp;
    ssize_t									n;
    OSStatus								err;
	
	(void) inSampleTime;
	
    if( ctx->inputCryptor.isValid )
    {
        maxPayloadSize = kAirTunesMaxPayloadSizeUDP - 24; // 16 bytes for auth tag and 8 - for nonce
    }
    else
    {
        maxPayloadSize = kAirTunesMaxPayloadSizeUDP;
    }

    pkt.pkt.rtp.header.v_p_x_cc	= RTPHeaderInsertVersion( 0, kRTPVersion );
    pkt.pkt.rtp.header.m_pt		= RTPHeaderInsertPayloadType( 0, inType );
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
            const uint8_t *		src;
            UInt32				packetCount;
            AudioBufferList		outputList;
            
            src = MirroredRingBufferGetReadPtr( ring );
            ctx->inputDataPtr							= src;
            ctx->inputDataEnd							= src + avail;
            packetCount									= 1;
            outputList.mNumberBuffers					= 1;
            outputList.mBuffers[ 0 ].mNumberChannels	= ctx->channels;
            outputList.mBuffers[ 0 ].mDataByteSize		= (UInt32)maxPayloadSize;
            outputList.mBuffers[ 0 ].mData				= pkt.pkt.rtp.payload;
            
            err = AudioConverterFillComplexBuffer( ctx->inputConverter, _AudioEncoderEncodeCallback, ctx,
                                                  &packetCount, &outputList, NULL );
            check( err == kNoErr || err == kUnderrunErr );
            MirroredRingBufferReadAdvance( ring, (size_t)( ctx->inputDataPtr - src ) );
            
            if( packetCount == 0 ) continue; // Skip if a packet wasn't produced.
            
            spp = packetCount * ctx->framesPerPacket;
            len = kRTPHeaderSize + outputList.mBuffers[ 0 ].mDataByteSize;

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

typedef struct SendAudioContext
{
	TAILQ_ENTRY( SendAudioContext )	list;
	AirPlayAudioStreamContext * 	ctx;
	AirPlayStreamType		   		type;
	uint32_t                   		sampleTime;
}
SendAudioContext;

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
	MirroredRingBuffer * const				ring				= &ctx->inputRing;
	OSStatus								err 				= kNoErr;
	SendAudioContext *						sendAudioContext;

	(void) inHostTime;
	
	len = MirroredRingBufferGetBytesFree( ring );
	require_action_quiet( len >= inLen, exit, err = kNoSpaceErr; 
		atr_ulog( kLogLevelInfo, "### Audio input buffer full: %zu > %zu\n", inLen, len ) );
	memcpy( MirroredRingBufferGetWritePtr( ring ), inBuffer, inLen );
	MirroredRingBufferWriteAdvance( ring, inLen );
    
	sendAudioContext 				= malloc( sizeof( *sendAudioContext ) );
	sendAudioContext->ctx 			= ctx;
	sendAudioContext->type 			= inType;
	sendAudioContext->sampleTime	= inSampleTime;

	pthread_mutex_lock( ctx->sendAudioMutexPtr );
	TAILQ_INSERT_TAIL( &ctx->sendAudioList, sendAudioContext, list );
	pthread_cond_signal( ctx->sendAudioCondPtr );
	pthread_mutex_unlock( ctx->sendAudioMutexPtr );
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetUserVersion
//===========================================================================================================================

void AirPlayReceiverSessionSetUserVersion( AirPlayReceiverSessionRef inSession, uint32_t userVersion )
{
	AirPlayReceiverSessionScreen_SetUserVersion( inSession->screenSession, userVersion );
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
	
	gAirTunes = NULL;
	if( session->delegate.finalize_f ) session->delegate.finalize_f( session, session->delegate.context );
	AirPlayReceiverSessionPlatformFinalize( session );
	
	dispatch_source_forget( &session->periodicTimer );
	_ScreenTearDown( session );
	_TearDownStream( session, &session->altAudioCtx, true );
	_TearDownStream( session, &session->mainAudioCtx, true );
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
			debug_increment_saturate( gAirTunesDebugIdleTimeoutCount, UINT_MAX );
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
	
	// Update stats.
	
#if( 1 && DEBUG )
	if( ( ticks - ats->perSecLastTicks ) >= ats->perSecTicks )
	{
		uint64_t		u64;
		uint32_t		u32;
	
		ats->perSecLastTicks = ticks;
		
		u32 = gAirTunesDebugRetransmitSendCount - gAirTunesDebugRetransmitSendLastCount;
		gAirTunesDebugRetransmitSendLastCount += u32;
		gAirTunesDebugRetransmitSendPerSecAvg = ( ( gAirTunesDebugRetransmitSendPerSecAvg * 7 ) + u32 ) / 8;
		
		u64 = gAirTunesDebugRecvRTPOriginalByteCount - gAirTunesDebugRecvRTPOriginalByteCountLast;
		gAirTunesDebugRecvRTPOriginalByteCountLast += u64;
		gAirTunesDebugRecvRTPOriginalBytesPerSecAvg = ( ( gAirTunesDebugRecvRTPOriginalBytesPerSecAvg * 15 ) + u64 ) / 16;
		
		u64 = gAirTunesDebugRecvRTPRetransmitByteCount - gAirTunesDebugRecvRTPRetransmitByteCountLast;
		gAirTunesDebugRecvRTPRetransmitByteCountLast += u64;
		gAirTunesDebugRecvRTPRetransmitBytesPerSecAvg = ( ( gAirTunesDebugRecvRTPRetransmitBytesPerSecAvg * 15 ) + u64 ) / 16;
	}
	if( gAirTunesDebugPrintPerf )
	{
		AirTunesDebugPerf( -1, me );
	}
#endif
	
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
	require_action_quiet( idle != inSession->sessionIdle, exit, err = kNoErr );

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
	
	check( inStreamType == kAirPlayStreamType_GeneralAudio || inStreamType == kAirPlayStreamType_MainHighAudio );
	ctx->type = inStreamType;
	if(		 inStreamType == kAirPlayStreamType_GeneralAudio )		ctx->label = "General";
	else if( inStreamType == kAirPlayStreamType_MainHighAudio )		ctx->label = "MainHigh";

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
	
	if( !me->server->qosDisabled ) SocketSetQoS( ctx->dataSock, me->audioQoS );
	
	err = OpenSelfConnectedLoopbackSocket( &ctx->cmdSock );
	require_noerr( err, exit );
	
	// Set up RTCP.
	
	SockAddrCopy( &me->peerAddr, &sip );
	err = ServerSocketOpen( sip.sa.sa_family, SOCK_DGRAM, IPPROTO_UDP, -kAirPlayPort_RTCPServer, &me->rtcpPortLocal, 
		-kAirTunesRTPSocketBufferSize, &me->rtcpSock );
	require_noerr( err, exit );
	
	SocketSetPacketTimestamps( me->rtcpSock, true );
	if( !me->server->qosDisabled ) SocketSetQoS( me->rtcpSock, me->audioQoS );
	
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
		ats->rtcpRTDisable				= _RetransmitsDisabled( me );
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
	SendAudioContext *					sendAudioContext;
	
	SetThreadName( "AirPlayAudioSender" );
	SetCurrentThreadPriority( kAirPlayThreadPriority_AudioSender );
	
	pthread_mutex_lock( ctx->sendAudioMutexPtr );
	while( !ctx->sendAudioDone )
	{
		pthread_cond_wait( ctx->sendAudioCondPtr, ctx->sendAudioMutexPtr );
		
		while( !TAILQ_EMPTY( &ctx->sendAudioList) )
		{
			if( ctx->sendAudioDone )    goto exit;
			
			sendAudioContext = TAILQ_FIRST( &ctx->sendAudioList );
			TAILQ_REMOVE( &ctx->sendAudioList, sendAudioContext, list );
			pthread_mutex_unlock( ctx->sendAudioMutexPtr );
			
			_AirPlayReceiver_SendAudio( sendAudioContext->ctx, sendAudioContext->type, sendAudioContext->sampleTime );
			free( sendAudioContext );
			
			pthread_mutex_lock( ctx->sendAudioMutexPtr );
		}
	}
	
exit:
	TAILQ_FOREACH( sendAudioContext, &ctx->sendAudioList, list)
	{
		free( sendAudioContext );
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
	debug_add( gAirTunesDebugRecvByteCount, len );
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
			
			debug_sub( gAirTunesDebugBusyNodeCount, 1 );
			debug_increment_saturate( gAirTunesDebugStolenNodeCount, UINT_MAX );
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
		debug_add( gAirTunesDebugRecvByteCount, len );
		debug_add( gAirTunesDebugRecvRTPOriginalByteCount, len );
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
	
	// Simulate packet loss.
	
#if( DEBUG )
	if( !inIsRetransmit && ( gAirTunesDropMinRate > 0 ) )
	{
		Boolean		drop;
		
		drop = false;
		if( gAirTunesDropRemaining > 0 )
		{
			--gAirTunesDropRemaining;
			drop = true;
		}
		if( gAirTunesDropRemaining == 0 )
		{
			if( gAirTunesSkipRemaining  > 0 ) --gAirTunesSkipRemaining;
			if( gAirTunesSkipRemaining == 0 )
			{
				gAirTunesSkipRemaining = RandomRange( gAirTunesSkipMinRate, gAirTunesSkipMaxRate );
				gAirTunesDropRemaining = RandomRange( gAirTunesDropMinRate, gAirTunesDropMaxRate );
			}
		}
		if( drop )
		{
			err = kCanceledErr;
			goto exit;
		}
	}
#endif
	
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
	
	debug_add( gAirTunesDebugBusyNodeCount, 1 );
	debug_track_max( gAirTunesDebugBusyNodeCountMax, gAirTunesDebugBusyNodeCount );
	
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
	
	debug_perform( gAirTunesDebugBusyNodeCountLast = gAirTunesDebugBusyNodeCount );
	
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
		
		#if( DEBUG )
			if( ( gAirTunesLateDrop > 0 ) && ( RandomRange( 0, gAirTunesLateDrop ) == 0 ) )
			{
				atr_ulog( kLogLevelNotice, "Random late drop seq %u\n", curr->rtp->header.seq );
				AirTunesFreeBufferNode( inSession, curr );
				continue;
			}
		#endif
		
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
			debug_add_saturate( gAirTunesDebugUnrecoveredPacketCount, pktGap, UINT_MAX );
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
			debug_increment_saturate( gAirTunesDebugOldDiscardCount, UINT_MAX );
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
			debug_increment_saturate( gAirTunesDebugConcealedGapCount, UINT_MAX );
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
			debug_increment_saturate( gAirTunesDebugLateDropCount, UINT_MAX );
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
		
		debug_increment_saturate( gAirTunesDebugConcealedEndCount, UINT_MAX );
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
		debug_increment_saturate( gAirTunesDebugGlitchCount, UINT_MAX );
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
		debug_increment_saturate( gAirTunesDebugDupCount, 1 );
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
			if(      seqLoss ==   1 ) debug_increment_saturate( gAirTunesDebugLossCounts[ 0 ], UINT_MAX );
			else if( seqLoss  <   5 ) debug_increment_saturate( gAirTunesDebugLossCounts[ 1 ], UINT_MAX );
			else if( seqLoss  <  10 ) debug_increment_saturate( gAirTunesDebugLossCounts[ 2 ], UINT_MAX );
			else if( seqLoss  <  20 ) debug_increment_saturate( gAirTunesDebugLossCounts[ 3 ], UINT_MAX );
			else if( seqLoss  <  30 ) debug_increment_saturate( gAirTunesDebugLossCounts[ 4 ], UINT_MAX );
			else if( seqLoss  <  40 ) debug_increment_saturate( gAirTunesDebugLossCounts[ 5 ], UINT_MAX );
			else if( seqLoss  <  50 ) debug_increment_saturate( gAirTunesDebugLossCounts[ 6 ], UINT_MAX );
			else if( seqLoss  < 100 ) debug_increment_saturate( gAirTunesDebugLossCounts[ 7 ], UINT_MAX );
			else if( seqLoss  < 500 ) debug_increment_saturate( gAirTunesDebugLossCounts[ 8 ], UINT_MAX );
			else                      debug_increment_saturate( gAirTunesDebugLossCounts[ 9 ], UINT_MAX );
			debug_add_saturate( gAirTunesDebugTotalLossCount, seqLoss, UINT_MAX );
			debug_track_max( gAirTunesDebugMaxBurstLoss, seqLoss );
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
				debug_increment_saturate( gAirTunesDebugRetransmitBigLossCount, UINT_MAX );
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
			debug_increment_saturate( gAirTunesDebugMisorderedCount, 1 );
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
	if( inSession->minLatency >= inSession->platformAudioLatency )
	{
		inSession->audioLatencyOffset = inSession->minLatency - inSession->platformAudioLatency;
	}
	else
	{
		if( inSession->minLatency > 0 )
		{
			dlogassert( "Min latency (%u) < our latency (%u)\n", inSession->minLatency, inSession->platformAudioLatency );
		}
		inSession->audioLatencyOffset = inSession->platformAudioLatency;
	}
	inSession->audioLatencyOffset += gAirTunesRelativeTimeOffset;
	
	atr_ulog( kLogLevelVerbose, "Audio Latency Offset %u, Platform %u, Sender %d, Relative %d\n", 
		inSession->audioLatencyOffset, inSession->platformAudioLatency, inSession->minLatency, gAirTunesRelativeTimeOffset );
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
	
	if( debug_false_conditional( gAirTunesDebugRetransmitTiming ) )
	{
		AirTunesTime		nowAT;
		uint64_t			nowNS;
		
		size = kRTCPRetransmitRequestPacketNTPSize;
		pkt.v_p			= RTCPHeaderInsertVersion( 0, kRTPVersion );
		pkt.pt			= kRTCPTypeRetransmitRequest;
		pkt.length		= htons( (uint16_t)( ( size / 4 ) - 1 ) );
		pkt.seqStart	= htons( inSeqStart );
		pkt.seqCount	= htons( inSeqCount );
		
		AirTunesClock_GetSynchronizedTime( inSession->airTunesClock, &nowAT );
		nowNS = AirTunesTime_ToNanoseconds( &nowAT );
		pkt.length	= htons( (uint16_t)( ( size / 4 ) - 1 ) );
		pkt.ntpHi	= (uint32_t)( nowNS >> 32 );
		pkt.ntpLo	= (uint32_t)( nowNS & UINT64_C( 0xFFFFFFFF ) );
		pkt.ntpHi	= htonl( pkt.ntpHi );
		pkt.ntpLo	= htonl( pkt.ntpLo );
	}
	else
	{
		size = kRTCPRetransmitRequestPacketMinSize;
		pkt.v_p			= RTCPHeaderInsertVersion( 0, kRTPVersion );
		pkt.pt			= kRTCPTypeRetransmitRequest;
		pkt.length		= htons( (uint16_t)( ( size / 4 ) - 1 ) );
		pkt.seqStart	= htons( inSeqStart );
		pkt.seqCount	= htons( inSeqCount );
	}
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
	debug_add( gAirTunesDebugSentByteCount, n );
	debug_increment_saturate( gAirTunesDebugRetransmitSendCount, UINT_MAX );
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
	debug_add( gAirTunesDebugRecvRTPRetransmitByteCount, inSize );
	
	err = _GeneralAudioReceiveRTP( inSession, &inPkt->payload.rtp, 
		inSize - offsetof( RTCPRetransmitResponsePacket, payload ) );
	debug_increment_saturate( gAirTunesDebugRetransmitRecvCount, UINT_MAX );
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
			debug_increment_saturate( gAirTunesDebugRetransmitNoFreeNodesCount, UINT_MAX );
			break;
		}
		inSession->source.rtcpRTFreeList = node->next;
		
		// Schedule a retransmit request.
		
		node->next			= NULL;
		node->seq			= inSeqStart + i;
		node->tries			= 0;
		node->startNanos	= nowNanos;
		node->nextNanos		= nowNanos;
		#if( DEBUG )
		{
			size_t		j;
			
			for( j = 0; j < countof( node->tryNanos ); ++j )
			{
				node->tryNanos[ j ] = 0;
			}
		}
		#endif
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
			#if( DEBUG )
				debug_track_min( gAirTunesDebugRetransmitMinNanos, ageNanos );
				debug_track_max( gAirTunesDebugRetransmitMaxNanos, ageNanos );
				if( ( ageNanos > gAirTunesDebugRetransmitMinNanos ) && ( ageNanos < gAirTunesDebugRetransmitMaxNanos ) )
				{
					gAirTunesDebugRetransmitAvgNanos = ( ( gAirTunesDebugRetransmitAvgNanos * 63 ) + ageNanos ) / 64;
				}
				if( curr->tries > 8 ) airtunes_record_retransmit( curr, "found", ageNanos );
				if( curr->tries > 0 ) --gAirTunesDebugRetransmitActiveCount;
			#endif
			
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
		else
		{
			debug_increment_saturate( gAirTunesDebugRetransmitPrematureCount, UINT_MAX );
		}
		
		*next = curr->next;
		curr->next = ats->rtcpRTFreeList;
		ats->rtcpRTFreeList = curr;
	}
	else if( inIsRetransmit )
	{
		atr_stats_ulog( kLogLevelInfo, "### Retransmit seq %u not found\n", pktSeq );
		debug_increment_saturate( gAirTunesDebugRetransmitNotFoundCount, UINT_MAX );
		++ats->retransmitNotFoundCount;
	}
	
	// Retry retransmits that have timed out.
	
	credits = 3;
	for( curr = ats->rtcpRTBusyList; curr; curr = curr->next )
	{
		if( nowNanos < curr->nextNanos ) continue;
		ageNanos = nowNanos - curr->startNanos;
		#if( DEBUG )
			curr->tryNanos[ Min( curr->tries, countof( curr->tryNanos ) - 1 ) ] = (uint32_t) ageNanos;
			if( curr->tries == 0 ) ++gAirTunesDebugRetransmitActiveCount;
			debug_track_max( gAirTunesDebugRetransmitActiveMax, gAirTunesDebugRetransmitActiveCount );
			if( curr->tries++ > 0 )
			{
				if( ageNanos < ats->retransmitRetryMinNanos ) ats->retransmitRetryMinNanos = ageNanos;
				if( ageNanos > ats->retransmitRetryMaxNanos ) ats->retransmitRetryMaxNanos = ageNanos;
				debug_track_min( gAirTunesDebugRetransmitRetryMinNanos, ageNanos );
				debug_track_max( gAirTunesDebugRetransmitRetryMaxNanos, ageNanos );
				debug_track_max( gAirTunesDebugRetransmitMaxTries, curr->tries );
			}
		#else
			if( curr->tries++ > 0 )
			{
				if( ageNanos < ats->retransmitRetryMinNanos ) ats->retransmitRetryMinNanos = ageNanos;
				if( ageNanos > ats->retransmitRetryMaxNanos ) ats->retransmitRetryMaxNanos = ageNanos;
			}
		#endif
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
		debug_perform( if( curr->tries > 0 ) --gAirTunesDebugRetransmitActiveCount );
		
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
			debug_increment_saturate( gAirTunesDebugRetransmitAbortCount, UINT_MAX );
			airtunes_record_retransmit( curr, inReason, nowNanos - curr->startNanos );
			atr_ulog( kLogLevelVerbose, "    ### Abort retransmit %5u  T %2u  A %10llu \n", 
				curr->seq, curr->tries, nowNanos - curr->startNanos  );
			debug_perform( if( curr->tries > 0 ) --gAirTunesDebugRetransmitActiveCount );
			
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
			bufferMs = CFDictionaryGetInt64( inRequestStreamDesc, CFSTR( kAirPlayPrefKey_AudioBufferMainAltWiredMs ), &err );
			if( err || ( bufferMs < 0 ) ) bufferMs = kAirPlayAudioBufferMainAltWiredMs;
		}
		else
		{
			bufferMs = CFDictionaryGetInt64( inRequestStreamDesc, CFSTR( kAirPlayPrefKey_AudioBufferMainAltWiFiMs ), &err );
			if( err || ( bufferMs < 0 ) ) bufferMs = kAirPlayAudioBufferMainAltWiFiMs;
		}
	}
	
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
	
	if( !inSession->server->qosDisabled )	SocketSetQoS( ctx->dataSock, kSocketQoS_Voice );
	
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
			err = MirroredRingBufferInit( &ctx->inputRing, kAirPlayInputRingSize, true );
			require_noerr( err, exit );
			
			TAILQ_INIT( &ctx->sendAudioList );
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

	if( !inSession->server->qosDisabled ) SocketSetQoS( inSession->keepAliveSock, kSocketQoS_Background );

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
	atr_ulog( kLogLevelTrace, "Timing thread exit\n" );
	return( NULL );
}

//===========================================================================================================================
//	_IdleStateKeepAliveReceiveBeacon
//===========================================================================================================================

static OSStatus	_IdleStateKeepAliveReceiveBeacon( AirPlayReceiverSessionRef inSession, SocketRef inSock )
{
	OSStatus			err;
	char				pkt[32];
	size_t				len;

	(void) inSession;	// Unused

	err = SocketRecvFrom( inSock, &pkt, sizeof( pkt ), &len, NULL, 0, NULL, NULL, NULL, NULL );
	if( err == EWOULDBLOCK ) goto exit;
	require_noerr( err, exit );


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
	if( !inSession->server->qosDisabled ) SocketSetQoS( inSession->timingSock, kSocketQoS_NTP );
	
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
			timeout.tv_usec = 500 * 1000;
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
		if( nSuccess >= 3 ) break;
		if( nFailure >= 64 )
		{
			atr_ulog( kLogLevelError, "Too many time negotiate failures: G=%d B=%d R=%d T=%d\n", 
				nSuccess, nFailure, nRecvErrors, nTimeouts );
			err = kTimeoutErr;
			goto exit;
		}
	}
	
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
	debug_add( gAirTunesDebugSentByteCount, n );
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
	debug_add( gAirTunesDebugRecvByteCount, len );
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
	double					rttAbs;
	unsigned int			i;
	double					median;
	Boolean					forceStep;
	
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
	airtunes_record_clock_offset( offset );
	airtunes_record_clock_rtt( rtt );
	
	// Update round trip time stats. If the RTT is > 70 ms, it's probably a spurious error.
	// If we're not getting any good RTT's, use the best one we've received in the last 5 tries.
	// This avoids the initial time sync negotiate from failing entirely if RTT's are really high.
	// It also avoids time sync from drifting too much if RTT's become really high later.
	
	if( rtt < src->rtcpTIClockRTTMin ) src->rtcpTIClockRTTMin = rtt;
	if( rtt > src->rtcpTIClockRTTMax ) src->rtcpTIClockRTTMax = rtt;
	rttAbs = fabs( rtt );
	if( rttAbs > 0.070 )
	{
		++src->rtcpTIClockRTTOutliers;
		debug_increment_saturate( gAirTunesDebugTimeSyncHugeRTTCount, UINT_MAX );
		atr_ulog( kLogLevelVerbose, "### Large clock sync RTT %f, Avg %f, Min %f, Max %f, Offset %f\n", 
			rtt, src->rtcpTIClockRTTAvg, src->rtcpTIClockRTTMin, src->rtcpTIClockRTTMax, offset );
		
		if( rttAbs < fabs( src->rtcpTIClockRTTLeastBad ) )
		{
			src->rtcpTIClockRTTLeastBad		= rtt;
			src->rtcpTIClockOffsetLeastBad	= offset;
		}
		if( ++src->rtcpTIClockRTTBadCount < 5 )
		{
			err = kRangeErr;
			goto exit;
		}
		rtt		= src->rtcpTIClockRTTLeastBad;
		offset	= src->rtcpTIClockOffsetLeastBad;
		
		atr_ulog( kLogLevelInfo, "Replaced large clock sync RTT with %f, offset %f\n", rtt, offset );
	}
	if( src->rtcpTIResponseCount == 0 ) src->rtcpTIClockRTTAvg = rtt;
	src->rtcpTIClockRTTAvg			= ( ( 15.0 * src->rtcpTIClockRTTAvg ) + rtt ) * ( 1.0 / 16.0 );
	src->rtcpTIClockRTTBadCount		= 0;
	src->rtcpTIClockRTTLeastBad		= +1000000.0;
	src->rtcpTIClockOffsetLeastBad	= 0.0;
	
	// Update clock offset stats. If this is first time ever or the first time after a clock step, reset the stats.
	
	if( src->rtcpTIResetStats || ( src->rtcpTIResponseCount == 0 ) )
	{
		for( i = 0; i < countof( src->rtcpTIClockOffsetArray ); ++i )
		{
			src->rtcpTIClockOffsetArray[ i ] = offset;
		}
		src->rtcpTIClockOffsetIndex = 0;
		src->rtcpTIClockOffsetAvg = offset;
		src->rtcpTIClockOffsetMin = offset;
		src->rtcpTIClockOffsetMax = offset;
		median = offset;
	}
	else
	{
		src->rtcpTIClockOffsetArray[ src->rtcpTIClockOffsetIndex++ ] = offset;
		if( src->rtcpTIClockOffsetIndex >= countof( src->rtcpTIClockOffsetArray ) )
		{
			src->rtcpTIClockOffsetIndex = 0;
		}
		
		check_compile_time_code( countof( src->rtcpTIClockOffsetArray ) == 3 );
		median = median_of_3( src->rtcpTIClockOffsetArray[ 0 ], 
							  src->rtcpTIClockOffsetArray[ 1 ], 
							  src->rtcpTIClockOffsetArray[ 2 ] );
		
		src->rtcpTIClockOffsetAvg = ( ( 15.0 * src->rtcpTIClockOffsetAvg ) + offset ) * ( 1.0 / 16.0 );
		if( offset < src->rtcpTIClockOffsetMin ) src->rtcpTIClockOffsetMin = offset;
		if( offset > src->rtcpTIClockOffsetMax ) src->rtcpTIClockOffsetMax = offset;
	}
	
	// Sync our local clock to the server's clock. Use median to reject outliers. If this is the first sync, always step.
	
	forceStep = ( src->rtcpTIResponseCount == 0 );
	src->rtcpTIResetStats = AirTunesClock_Adjust( inSession->airTunesClock, (int64_t)( median * 1E9 ), forceStep );
	if( src->rtcpTIResetStats && !forceStep ) ++src->rtcpTIStepCount;
	++src->rtcpTIResponseCount;
	err = kNoErr;
	
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
	
	if( !inSession->server->qosDisabled )	SocketSetQoS( inSession->screenSock, kSocketQoS_AirPlayScreenVideo );
	
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

	AirPlayReceiverSessionScreen_SetSessionUUID( session->screenSession, session->sessionUUID );
	AirPlayReceiverSessionScreen_SetClientDeviceID( session->screenSession, session->clientDeviceID );

	err = NetSocket_CreateWithNative( &netSock, newSock );
	require_noerr( err, exit );
	
	newSock = kInvalidSocketRef; // netSock now owns newSock.
	
	timeSynchronizer.context								= session;
	timeSynchronizer.getSynchronizedNTPTimeFunc				= _ScreenGetSynchronizedNTPTime;
	timeSynchronizer.getUpTicksNearSynchronizedNTPTimeFunc	= _ScreenGetUpTicksNearSynchronizedNTPTime;

	AirPlayReceiverSessionScreen_SetTimeSynchronizer( session->screenSession, &timeSynchronizer );
	
	AirPlayReceiverSessionScreen_SetReceiveEndTime( session->screenSession, CFAbsoluteTimeGetCurrent() );
	AirPlayReceiverSessionScreen_SetAuthEndTime( session->screenSession, CFAbsoluteTimeGetCurrent() );
	AirPlayReceiverSessionScreen_SetNTPEndTime( session->screenSession, CFAbsoluteTimeGetCurrent() );

	err = AirPlayReceiverSessionScreen_StartSession( session->screenSession, session->server->screenStreamOptions );
	require_noerr( err, exit );
	
	params = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( params, exit, err = kNoMemoryErr );
	
	AirPlayReceiverSessionScreen_LogStarted( session->screenSession, params, session->transportType );
	
	err = AirPlayReceiverSessionScreen_ProcessFrames( session->screenSession, netSock, session->server->timeoutDataSecs );
	require_noerr( err, exit );
	
exit:
	AirPlayReceiverSessionScreen_LogEnded( session->screenSession, err );
	AirPlayReceiverSessionScreen_StopSession( session->screenSession );
	NetSocket_Forget( &netSock );
	ForgetSocket( &newSock );
	CFReleaseNullSafe( params );
    atr_ulog( kLogLevelTrace, "Screen thread exit\n" );
	return( NULL );
}

//===========================================================================================================================
//	_ScreenHandleEvent
//===========================================================================================================================
static void
	_ScreenHandleEvent(
		AirPlayReceiverSessionScreenRef		inSession,
		CFStringRef							inEventName,
		CFDictionaryRef						inEventData,
		void *								inUserData )
{
	AirPlayReceiverSessionRef const		me = (AirPlayReceiverSessionRef) inUserData;
	OSStatus							err;
	uint32_t							frameCount;
	CFArrayRef							metrics;
	CFMutableDictionaryRef				request;
	
	(void) inSession;
	
	if( 0 ) {} // Empty if to simplify else if's below.
	
	// ForceKeyFrame
	
	else if( CFEqual( inEventName, CFSTR( kAirPlayReceiverSessionScreenEvent_ForceKeyFrameNeeded ) ) )
	{
		request = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require( request, exit );
		CFDictionarySetValue( request, CFSTR( kAirPlayKey_Type ), CFSTR( kAirPlayCommand_ForceKeyFrame ) );
		err = AirPlayReceiverSessionSendCommand( me, request, NULL, NULL );
		CFRelease( request );
		require_noerr( err, exit );
	}
	
	// TimestampsUpdated
	
	else if( CFEqual( inEventName, CFSTR( kAirPlayReceiverSessionScreenEvent_TimestampsUpdated ) ) )
	{
		frameCount = (uint32_t) CFDictionaryGetInt64( inEventData, CFSTR( "frameCount" ), NULL );
		metrics = CFDictionaryGetCFArray( inEventData, CFSTR( "metrics" ), NULL );
		if( metrics )
		{
			AirPlayReceiverServerControlAsyncF( me->server, CFSTR( kAirPlayCommand_UpdateTimestamps ), NULL,
				NULL, NULL, NULL, 
				"{"
					"%kO=%i"
					"%kO=%O"
				"}", 
				CFSTR( "frameCount" ),	frameCount, 
				CFSTR( "metrics" ),		metrics );
		}
	}
	
exit:
	return;
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

	streamKeySaltLen = ASPrintF( &streamKeySaltPtr, "%s%llu", kAirPlayPairingDataStreamKeySaltPtr, streamConnectionID );
 
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
	DataBuffer_AppendF( &db, "AirPlay session ended: Dur=%{dur} Reason=%#m\n", durationSecs, inReason );
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
		"%u/%u/%u ms min/max/avg RTT, %d/%d/%d µS min/max/avg offset, %u outlier(s), %u step(s), %d max skew, %u skew reset(s)\n", 
		ntpRTTMin, ntpRTTMax, ntpRTTAvg, 
		(int32_t)(  1000000 * ats->rtcpTIClockOffsetMin ), 
		(int32_t)(  1000000 * ats->rtcpTIClockOffsetMax ), 
		(int32_t)(  1000000 * ats->rtcpTIClockOffsetAvg ), 
		ats->rtcpTIClockRTTOutliers, ats->rtcpTIStepCount, ats->rtpOffsetMaxSkew, ats->rtpOffsetSkewResetCount );
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
	MirroredRingBufferFree( &ctx->inputRing );
	
	if( ctx == &inSession->mainAudioCtx )
	{
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
		newSample->sampleTime	= inSampleTime;
		
		pthread_mutex_lock( ctx->zeroTimeLockPtr );
		ctx->zeroTime = *newSample;
		pthread_mutex_unlock( ctx->zeroTimeLockPtr );
		
		if( newCount >= 8 )
		{
			oldSample = &ctx->rateUpdateSamples[ newCount % Min( newCount, countof( ctx->rateUpdateSamples ) ) ];
			scale = ( newSample->hostTime - oldSample->hostTime ) * kNTPUnits_FP;
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
	if( inPhoneCall  != kAirPlaySpeechMode_NotApplicable )	changes.phoneCall	= inPhoneCall;
	if( inTurnByTurn != kAirPlaySpeechMode_NotApplicable )	changes.turnByTurn	= inTurnByTurn;
	
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
	CFStringRef			cfstr;
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
		
		cfstr = CFDictionaryGetCFString( dict, CFSTR( kAirPlayKey_AppStateID ), NULL );
		if( cfstr )	x = AirPlayAppStateIDFromCFString( cfstr );
		else		x = (int) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_AppStateID ), NULL );
		switch( x )
		{
			case kAirPlayAppStateID_PhoneCall:
				cfstr = CFDictionaryGetCFString( dict, CFSTR( kAirPlayKey_Entity ), &err );
				if( cfstr )	x = AirPlayEntityFromCFString( cfstr );
				else		x = (int) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_Entity ), &err );
				require_noerr( err, exit );
				outModes->phoneCall = x;
				break;
			
			case kAirPlayAppStateID_Speech:
				cfstr = CFDictionaryGetCFString( dict, CFSTR( kAirPlayKey_Entity ), &err );
				if( cfstr )	x = AirPlayEntityFromCFString( cfstr );
				else		x = (int) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_Entity ), &err );
				require_noerr( err, exit );
				outModes->speech.entity = x;
				
				cfstr = CFDictionaryGetCFString( dict, CFSTR( kAirPlayKey_SpeechMode ), &err );
				if( cfstr )	x = AirPlaySpeechModeFromCFString( cfstr );
				else		x = (int) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_SpeechMode ), &err );
				require_noerr( err, exit );
				outModes->speech.mode = x;
				break;
			
			case kAirPlayAppStateID_TurnByTurn:
				cfstr = CFDictionaryGetCFString( dict, CFSTR( kAirPlayKey_Entity ), &err );
				if( cfstr )	x = AirPlayEntityFromCFString( cfstr );
				else		x = (int) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_Entity ), &err );
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
		
		cfstr = CFDictionaryGetCFString( dict, CFSTR( kAirPlayKey_ResourceID ), NULL );
		if( cfstr )	x = AirPlayResourceIDFromCFString( cfstr );
		else		x = (AirPlayAppStateID) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_ResourceID ), NULL );
		switch( x )
		{
			case kAirPlayResourceID_MainScreen:
				cfstr = CFDictionaryGetCFString( dict, CFSTR( kAirPlayKey_Entity ), &err );
				if( cfstr )	x = AirPlayEntityFromCFString( cfstr );
				else		x = (int) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_Entity ), &err );
				require_noerr( err, exit );
				outModes->screen = x;
				break;
			
			case kAirPlayResourceID_MainAudio:
				cfstr = CFDictionaryGetCFString( dict, CFSTR( kAirPlayKey_Entity ), &err );
				if( cfstr )	x = AirPlayEntityFromCFString( cfstr );
				else		x = (int) CFDictionaryGetInt64( dict, CFSTR( kAirPlayKey_Entity ), &err );
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

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

//===========================================================================================================================
//	AirTunesDebugControl
//===========================================================================================================================

#if( DEBUG )

void	AirTunesDebugControl_Tweak( DataBuffer *inDB, const char *inCmd );

OSStatus	AirTunesDebugControl( const char *inCmd, CFStringRef *outOutput )
{
	OSStatus			err;
	DataBuffer			db;
	const char *		key;
	size_t				keyLen;
	const char *		value;
	int					n;
	
	DataBuffer_Init( &db, NULL, 0, SIZE_MAX );
	
	if( *inCmd == '\0' ) inCmd = "help";
	key = inCmd;
	for( value = inCmd; ( *value != '\0' ) && ( *value != '=' ); ++value ) {}
	keyLen = (size_t)( value - key );
	if( *value != '\0' ) ++value;
	
	// aro -- Adjust the relative RTP offset.
	
	if( strncmpx( key, keyLen, "aro" ) == 0 )
	{
		int32_t		oldValue, newValue;
		
		newValue = atoi( value );
		oldValue = gAirTunesRelativeTimeOffset;
		gAirTunesRelativeTimeOffset = newValue;
		
		if( gAirTunes ) _GeneralAudioUpdateLatency( gAirTunes );
		else			DataBuffer_AppendF( &db, "Changed relative latency: %d -> %d\n", oldValue, newValue );
	}
	
	// drop -- simulate dropped packets.
	
	else if( strncmpx( key, keyLen, "drop" ) == 0 )
	{
		int		minDropRate, maxDropRate, minSkipRate, maxSkipRate;
		
		n = sscanf( value, "%d,%d,%d,%d", &minDropRate, &maxDropRate, &minSkipRate, &maxSkipRate );
		if( n == 4 )
		{
			gAirTunesDropMinRate = minDropRate;
			gAirTunesDropMaxRate = maxDropRate;
			gAirTunesSkipMinRate = minSkipRate;
			gAirTunesSkipMaxRate = maxSkipRate;
			gAirTunesDropRemaining = 0;
			gAirTunesSkipRemaining = 0;
			
			DataBuffer_AppendF( &db, "Starting simulated packet loss: drop %d-%d packets every %d-%d packets\n", 
				minDropRate, maxDropRate, minSkipRate, maxSkipRate );
		}
		else
		{
			gAirTunesDropMinRate = 0;
			DataBuffer_AppendF( &db, "Stopped simulated packet loss\n" );
		}
	}
	else if( strncmpx( key, keyLen, "latedrop" ) == 0 )
	{
		gAirTunesLateDrop = 0;
		sscanf( value, "%d", &gAirTunesLateDrop );
		if( gAirTunesLateDrop > 0 ) DataBuffer_AppendF( &db, "Late dropping every %d packets\n", gAirTunesLateDrop );
		else						DataBuffer_AppendF( &db, "Disabling late drop\n" );
	}
	
	// rs -- Resets all debug stats.
	
	else if( strncmpx( key, keyLen, "rs" ) == 0 )
	{
		AirTunesDebugControl_ResetDebugStats();
		DataBuffer_AppendF( &db, "Reset debug stats\n" );
	}
	
	// file -- Save audio to a file.
	
#if( TARGET_HAS_C_LIB_IO )
	else if( strncmpx( key, keyLen, "file" ) == 0 )
	{
		char		path[ PATH_MAX + 1 ];
		
		if( gAirTunesFile )
		{
			fclose( gAirTunesFile );
			gAirTunesFile = NULL;
		}
		
		if( *value != '\0' )
		{
			snprintf( path, sizeof( path ), "%s", value );
			gAirTunesFile = fopen( path, "wb" );
			err = map_global_value_errno( gAirTunesFile, gAirTunesFile );
			require_noerr( err, exit );
			
			DataBuffer_AppendF( &db, "starting record of audio at file \"%s\"\n", value );
		}
	}
#endif
	
	// perf -- Start performance monitoring
	
	else if( strncmpx( key, keyLen, "perf" ) == 0 )
	{
		int		interval;
		
		interval = atoi( value );
		if( interval > 0 )
		{
			DataBuffer_AppendF( &db, "started performance monitoring every %d seconds\n", interval );
			AirTunesDebugPerf( interval, NULL );
		}
		else
		{
			AirTunesDebugPerf( interval, NULL );
			DataBuffer_AppendF( &db, "stopped performance monitoring\n" );
		}
	}
	
	// perfMode -- Set performance monitoring mode
	
	else if( strncmpx( key, keyLen, "perfMode" ) == 0 )
	{
		gAirTunesDebugPerfMode = atoi( value );
		DataBuffer_AppendF( &db, "set performance monitoring mode to %d\n", gAirTunesDebugPerfMode );
	}
	
	// tweak -- Tweak variables.
	
	else if( strncmpx( key, keyLen, "tweak" ) == 0 )
	{
		AirTunesDebugControl_Tweak( &db, value );
	}
	
	// Help
	
	else if( strncmpx( key, keyLen, "help" ) == 0 )
	{
		DataBuffer_AppendF( &db, "-- AirTunes Debug Control Commands --\n" );
		DataBuffer_AppendF( &db, "    aro                    -- Adjust relative RTP offset.\n" );
		DataBuffer_AppendF( &db, "    rs                     -- Reset all debug stats.\n" );
		DataBuffer_AppendF( &db, "    perf                   -- Start/stop performance monitoring.\n" );
		DataBuffer_AppendF( &db, "    perfMode               -- Set performance monitoring mode.\n" );
		DataBuffer_AppendF( &db, "    hwskew <force>,<skew>  -- Set performance monitoring mode.\n" );
		DataBuffer_AppendF( &db, "\n" );
	}
	
	// Unknown
	
	else
	{
		DataBuffer_AppendF( &db, "### unknown command: '%s'\n", inCmd );
		goto exitOutput;
	}
	
exitOutput:
		
	// Return as a CFString.
	
	if( outOutput )
	{
		CFStringRef		output;
		
		output = CFStringCreateWithBytes( NULL, db.bufferPtr, (CFIndex) db.bufferLen, kCFStringEncodingUTF8, false );
		require_action( output, exit, err = kNoMemoryErr );
		*outOutput = output;
	}
	else
	{
		dlog( kLogLevelMax, "%.*s", (int) db.bufferLen, db.bufferPtr );
	}
	err = kNoErr;
	
exit:
	DataBuffer_Free( &db );
	return( err );
}
#endif

//===========================================================================================================================
//	AirTunesDebugControl_ResetDebugStats
//===========================================================================================================================

#if( DEBUG )
void	AirTunesDebugControl_ResetDebugStats( void )
{
	gAirTunesDebugBusyNodeCountMax					= 0;
	
	gAirTunesDebugSentByteCount						= 0;
	gAirTunesDebugRecvByteCount						= 0;
	gAirTunesDebugRecvRTPOriginalByteCount			= 0;
	gAirTunesDebugRecvRTPOriginalByteCountLast		= 0;
	gAirTunesDebugRecvRTPOriginalBytesPerSecAvg		= 0;
	gAirTunesDebugRecvRTPRetransmitByteCount		= 0;
	gAirTunesDebugRecvRTPRetransmitByteCountLast	= 0;
	gAirTunesDebugRecvRTPRetransmitBytesPerSecAvg	= 0;
	gAirTunesDebugIdleTimeoutCount					= 0;
	gAirTunesDebugStolenNodeCount					= 0;
	gAirTunesDebugOldDiscardCount					= 0;
	gAirTunesDebugConcealedGapCount					= 0;
	gAirTunesDebugConcealedEndCount					= 0;
	gAirTunesDebugLateDropCount						= 0;
	gAirTunesDebugSameTimestampCount				= 0;
	memset( gAirTunesDebugLossCounts, 0, sizeof( gAirTunesDebugLossCounts ) );
	gAirTunesDebugTotalLossCount					= 0;
	gAirTunesDebugMaxBurstLoss						= 0;
	gAirTunesDebugDupCount							= 0;
	gAirTunesDebugMisorderedCount					= 0;
	gAirTunesDebugUnrecoveredPacketCount			= 0;
	gAirTunesDebugUnexpectedRTPOffsetResetCount 	= 0;
	gAirTunesDebugHugeSkewResetCount				= 0;
	gAirTunesDebugGlitchCount						= 0;
	gAirTunesDebugTimeSyncHugeRTTCount				= 0;
	gAirTunesDebugTimeAnnounceMinNanos				= UINT64_C( 0xFFFFFFFFFFFFFFFF );
	gAirTunesDebugTimeAnnounceMaxNanos				= 0;
	gAirTunesDebugRetransmitActiveCount				= 0;
	gAirTunesDebugRetransmitActiveMax				= 0;
	gAirTunesDebugRetransmitSendCount				= 0;
	gAirTunesDebugRetransmitSendLastCount			= 0;
	gAirTunesDebugRetransmitSendPerSecAvg			= 0;
	gAirTunesDebugRetransmitRecvCount				= 0;
	gAirTunesDebugRetransmitBigLossCount			= 0;
	gAirTunesDebugRetransmitAbortCount				= 0;
	gAirTunesDebugRetransmitFutileAbortCount		= 0;
	gAirTunesDebugRetransmitNoFreeNodesCount		= 0;
	gAirTunesDebugRetransmitNotFoundCount			= 0;
	gAirTunesDebugRetransmitPrematureCount			= 0;
	gAirTunesDebugRetransmitMaxTries				= 0;
	gAirTunesDebugRetransmitMinNanos				= UINT64_C( 0xFFFFFFFFFFFFFFFF );
	gAirTunesDebugRetransmitMaxNanos				= 0;
	gAirTunesDebugRetransmitAvgNanos				= 0;
	gAirTunesDebugRetransmitRetryMinNanos			= UINT64_C( 0xFFFFFFFFFFFFFFFF );
	gAirTunesDebugRetransmitRetryMaxNanos			= 0;
	
	gAirTunesClockOffsetIndex 						= 0;
	gAirTunesClockOffsetCount 						= 0;
	
	gAirTunesClockRTTIndex 							= 0;
	gAirTunesClockRTTCount 							= 0;
	
	gAirTunesRTPOffsetIndex 						= 0;
	gAirTunesRTPOffsetCount 						= 0;
	
	dlog( kLogLevelMax, "AirPlay debugging stats reset\n" );
}
#endif

//===========================================================================================================================
//	AirTunesDebugControl_Tweak
//===========================================================================================================================

#if( DEBUG )
void	AirTunesDebugControl_Tweak( DataBuffer *inDB, const char *inCmd )
{
	const char *		name;
	size_t				nameLen;
	const char *		value;
	
	name = inCmd;
	for( value = inCmd; ( *value != '\0' ) && ( *value != '=' ); ++value ) {}
	if( *value != '=' )
	{
		DataBuffer_AppendF( inDB, "### malformed tweak: '%s'\n", inCmd );
		goto exit;
	}
	nameLen = (size_t)( value - name );
	++value;
	
	if( 0 ) {}
	else if( strncmpx( name, nameLen, "pid" ) == 0 )
	{
		double		pGain, iGain, dGain, dPole, iMin, iMax;
		int			n;
		
		if( !gAirTunes )
		{
			DataBuffer_AppendF( inDB, "error: AirTunes not running\n", value );
			goto exit;
		}
		
		n = sscanf( value, "%lf,%lf,%lf,%lf,%lf,%lf", &pGain, &iGain, &dGain, &dPole, &iMin, &iMax );
		if( n != 6 )
		{
			DataBuffer_AppendF( inDB, "error: bad PID value '%s'. Must be <pGain>,<iGain>,<dGain>,<dPole>,<iMin>,<iMax>\n", value );
			goto exit;
		}
	}
	else
	{
		(void) nameLen;
		DataBuffer_AppendF( inDB, "### unknown tweak: '%s'\n", inCmd );
	}
	
exit:
	return;
}
#endif

//===========================================================================================================================
//	AirTunesDebugShow - Console show routine.
//===========================================================================================================================

OSStatus	AirTunesDebugShow( const char *inCmd, CFStringRef *outOutput )
{
	OSStatus		err;
	DataBuffer		dataBuf;
	CFStringRef		output;
	
	DataBuffer_Init( &dataBuf, NULL, 0, SIZE_MAX );
	
	err = AirTunesDebugAppendShowData( inCmd, &dataBuf );
	require_noerr( err, exit );
		
	if( outOutput )
	{
		output = CFStringCreateWithBytes( NULL, dataBuf.bufferPtr, (CFIndex) dataBuf.bufferLen, kCFStringEncodingUTF8, false );
		require_action( output, exit, err = kNoMemoryErr );
		*outOutput = output;
	}
	else
	{
		dlog( kLogLevelMax, "%.*s", (int) dataBuf.bufferLen, dataBuf.bufferPtr );
	}
	
exit:
	DataBuffer_Free( &dataBuf );
	return( err );
}

//===========================================================================================================================
//	AirTunesDebugAppendShowData - Console show routine.
//===========================================================================================================================

OSStatus	AirTunesDebugAppendShowData( const char *inCmd, DataBuffer *inDB )
{
	OSStatus				err;
	uint32_t				runningSecs;
	AirTunesSource *		src;
#if( DEBUG )
	unsigned int			i;
#endif
	Boolean					b;
	
	// globals -- Show globals.
	
	if( !inCmd || ( *inCmd == '\0' ) || ( strcmp( inCmd, "globals" ) == 0 ) )
	{
		DataBuffer_AppendF( inDB, "\n" );
		DataBuffer_AppendF( inDB, "+-+ AirPlay Audio Statistics +-+\n" );
		if( gAirTunes )
		{
			b = (Boolean)( gAirTunes->compressionType != kAirPlayCompressionType_PCM );
			DataBuffer_AppendF( inDB, "    Encoding Mode            = %s\n", 
				( gAirTunes->decryptor && b )	? "EC"	:
				  gAirTunes->decryptor			? "Ec"	:
				  b								? "eC"	:
												  "ec" );
			runningSecs = (uint32_t) UpTicksToSeconds( UpTicks() - gAirTunes->sessionTicks );
			DataBuffer_AppendF( inDB, "    Running Time             = %u seconds (%{dur})\n", runningSecs, runningSecs );
			DataBuffer_AppendF( inDB, "    rtp                      = sock %d, local port %d\n", 
				gAirTunes->mainAudioCtx.dataSock, gAirTunes->rtpAudioPort );
			DataBuffer_AppendF( inDB, "    rtcp                     = sock %d, local port %d, remote port %d\n", 
				gAirTunes->rtcpSock, gAirTunes->rtcpPortLocal, gAirTunes->rtcpPortRemote );
			DataBuffer_AppendF( inDB, "    timing                   = sock %d, local port %d, remote port %d\n", 
				gAirTunes->timingSock, gAirTunes->timingPortLocal, gAirTunes->timingPortRemote );
			src = &gAirTunes->source;
			DataBuffer_AppendF( inDB, "    receiveCount             = %u\n", src->receiveCount );
			DataBuffer_AppendF( inDB, "    lastRTPSeq/lastRTPTS     = %hu/%u\n", gAirTunes->lastRTPSeq, (int) gAirTunes->lastRTPTS );
			DataBuffer_AppendF( inDB, "    glitches                 = %d total, %d glitchy period(s) of %d total period(s) (%d%%)\n", 
				gAirTunes->glitchTotal, gAirTunes->glitchyPeriods, gAirTunes->glitchTotalPeriods, 
				( gAirTunes->glitchTotalPeriods > 0 ) ? ( ( gAirTunes->glitchyPeriods * 100 ) / gAirTunes->glitchTotalPeriods ) : 0 );
			DataBuffer_AppendF( inDB, "    rtcpTIStepCount          = %u\n", src->rtcpTIStepCount );
			DataBuffer_AppendF( inDB, "    rtcpTIClockRTTOutliers   = %u\n", src->rtcpTIClockRTTOutliers );
			DataBuffer_AppendF( inDB, "    rtcpTIClockRTTAvg        = %f\n", src->rtcpTIClockRTTAvg );
			DataBuffer_AppendF( inDB, "    rtcpTIClockRTTMin        = %f\n", src->rtcpTIClockRTTMin );
			DataBuffer_AppendF( inDB, "    rtcpTIClockRTTMax        = %f\n", src->rtcpTIClockRTTMax );
			DataBuffer_AppendF( inDB, "    rtcpTIClockOffsetArray   = [%f, %f, %f]\n", 
				src->rtcpTIClockOffsetArray[ 0 ], src->rtcpTIClockOffsetArray[ 1 ], src->rtcpTIClockOffsetArray[ 2 ] );
			DataBuffer_AppendF( inDB, "    rtcpTIClockOffsetAvg     = %f\n", src->rtcpTIClockOffsetAvg );
			DataBuffer_AppendF( inDB, "    rtcpTIClockOffsetMin     = %f\n", src->rtcpTIClockOffsetMin );
			DataBuffer_AppendF( inDB, "    rtcpTIClockOffsetMax     = %f\n", src->rtcpTIClockOffsetMax );
			DataBuffer_AppendF( inDB, "    rtpOffsetActive          = %u (%+d avg)\n", 
				(int) src->rtpOffsetActive, (int)( src->rtpOffsetActive - src->rtpOffsetAvg ) );
			DataBuffer_AppendF( inDB, "    rtpOffset                = %u (%+d avg)\n", 
				(int) src->rtpOffset, (int)( src->rtpOffset - src->rtpOffsetAvg ) );
			DataBuffer_AppendF( inDB, "    rtpOffsetApplyTimestamp  = %u (%s)\n", 
				(int) src->rtpOffsetApplyTimestamp, src->rtpOffsetApply ? "YES" : "NO" );
			DataBuffer_AppendF( inDB, "    rtpOffsetLast            = %u\n", src->rtpOffsetLast );
			DataBuffer_AppendF( inDB, "    rtpOffsetAvg             = %u\n", src->rtpOffsetAvg );
			DataBuffer_AppendF( inDB, "    rtpOffsetMaxDelta        = %u\n", src->rtpOffsetMaxDelta );
			DataBuffer_AppendF( inDB, "    rtpOffsetMaxSkew         = %u\n", src->rtpOffsetMaxSkew );
			DataBuffer_AppendF( inDB, "    rtpOffsetSkewResetCount  = %u\n", src->rtpOffsetSkewResetCount );
			DataBuffer_AppendF( inDB, "    rtcpRTDisable            = %d\n", src->rtcpRTDisable );
			DataBuffer_AppendF( inDB, "    rtcpRTMinRTTNanos        = %lld\n", src->rtcpRTMinRTTNanos );
			DataBuffer_AppendF( inDB, "    rtcpRTMaxRTTNanos        = %lld\n", src->rtcpRTMaxRTTNanos );
			DataBuffer_AppendF( inDB, "    rtcpRTAvgRTTNanos        = %lld\n", src->rtcpRTAvgRTTNanos );
			DataBuffer_AppendF( inDB, "    rtcpRTDevRTTNanos        = %lld\n", src->rtcpRTDevRTTNanos );
			DataBuffer_AppendF( inDB, "    rtcpRTTimeoutNanos       = %lld\n", src->rtcpRTTimeoutNanos );
			DataBuffer_AppendF( inDB, "    retransmitSendCount      = %u\n", src->retransmitSendCount );
			DataBuffer_AppendF( inDB, "    retransmitReceiveCount   = %u\n", src->retransmitReceiveCount );
			DataBuffer_AppendF( inDB, "    retransmitFutileCount    = %u\n", src->retransmitFutileCount );
			DataBuffer_AppendF( inDB, "    retransmitNotFoundCount  = %u\n", src->retransmitNotFoundCount );
			DataBuffer_AppendF( inDB, "    retransmitMinNanos       = %llu\n", src->retransmitMinNanos );
			DataBuffer_AppendF( inDB, "    retransmitMaxNanos       = %llu\n", src->retransmitMaxNanos );
			DataBuffer_AppendF( inDB, "    retransmitAvgNanos       = %llu\n", src->retransmitAvgNanos );
			DataBuffer_AppendF( inDB, "    retransmitRetryMinNanos  = %llu\n", src->retransmitRetryMinNanos );
			DataBuffer_AppendF( inDB, "    retransmitRetryMaxNanos  = %llu\n", src->retransmitRetryMaxNanos );
			DataBuffer_AppendF( inDB, "    maxBurstLoss             = %u\n", src->maxBurstLoss );
			DataBuffer_AppendF( inDB, "    bigLossCount             = %u\n", src->bigLossCount );
			DataBuffer_AppendF( inDB, "\n" );
		}
	}
	
	// stats -- Show persistent stats.
	
	else if( strcmp( inCmd, "stats" ) == 0 )
	{
	#if( DEBUG )
		DataBuffer_AppendF( inDB, "+-+ AirTunes Persistent Stats +-+\n" );
		
		if( gAirTunes )
		{
			DataBuffer_AppendF( inDB, "    Busy Nodes                                    = current %u free, last %u free, min %u free of %u (%u%% busy)\n", 
				gAirTunes->nodeCount - gAirTunesDebugBusyNodeCount, 
				gAirTunes->nodeCount - gAirTunesDebugBusyNodeCountLast, 
				gAirTunes->nodeCount - gAirTunesDebugBusyNodeCountMax, gAirTunes->nodeCount, 
				( gAirTunesDebugBusyNodeCount * 100 ) / gAirTunes->nodeCount );
		}
		DataBuffer_AppendF( inDB, "    gAirTunesRelativeTimeOffset                   = %d\n", gAirTunesRelativeTimeOffset );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugSentByteCount                   = %llu\n", gAirTunesDebugSentByteCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRecvByteCount                   = %llu\n", gAirTunesDebugRecvByteCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRecvRTPOriginalByteCount        = %llu\n", gAirTunesDebugRecvRTPOriginalByteCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRecvRTPOriginalBytesPerSecAvg   = %llu\n", gAirTunesDebugRecvRTPOriginalBytesPerSecAvg );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRecvRTPRetransmitByteCount      = %llu\n", gAirTunesDebugRecvRTPRetransmitByteCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRecvRTPRetransmitBytesPerSecAvg = %llu (%llu%%)\n", 
			gAirTunesDebugRecvRTPRetransmitBytesPerSecAvg, 
			( gAirTunesDebugRecvRTPOriginalBytesPerSecAvg > 0 ) ? 
				( gAirTunesDebugRecvRTPRetransmitBytesPerSecAvg * 100 ) / gAirTunesDebugRecvRTPOriginalBytesPerSecAvg : 0 );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugIdleTimeoutCount                = %u\n", gAirTunesDebugIdleTimeoutCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugStolenNodeCount                 = %u\n", gAirTunesDebugStolenNodeCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugOldDiscardCount                 = %u\n", gAirTunesDebugOldDiscardCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugConcealedGapCount               = %u\n", gAirTunesDebugConcealedGapCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugConcealedEndCount               = %u\n", gAirTunesDebugConcealedEndCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugLateDropCount                   = %u\n", gAirTunesDebugLateDropCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugSameTimestampCount              = %u\n", gAirTunesDebugSameTimestampCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugLossCounts ==   1               = %u\n", gAirTunesDebugLossCounts[ 0 ] );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugLossCounts  <   5               = %u\n", gAirTunesDebugLossCounts[ 1 ] );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugLossCounts  <  10               = %u\n", gAirTunesDebugLossCounts[ 2 ] );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugLossCounts  <  20               = %u\n", gAirTunesDebugLossCounts[ 3 ] );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugLossCounts  <  30               = %u\n", gAirTunesDebugLossCounts[ 4 ] );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugLossCounts  <  40               = %u\n", gAirTunesDebugLossCounts[ 5 ] );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugLossCounts  <  50               = %u\n", gAirTunesDebugLossCounts[ 6 ] );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugLossCounts  < 100               = %u\n", gAirTunesDebugLossCounts[ 7 ] );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugLossCounts  < 500               = %u\n", gAirTunesDebugLossCounts[ 8 ] );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugLossCounts >= 500               = %u\n", gAirTunesDebugLossCounts[ 9 ] );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugTotalLossCount                  = %u\n", gAirTunesDebugTotalLossCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugMaxBurstLoss                    = %u\n", gAirTunesDebugMaxBurstLoss );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugDupCount                        = %u\n", gAirTunesDebugDupCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugMisorderedCount                 = %u\n", gAirTunesDebugMisorderedCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugUnrecoveredPacketCount          = %u\n", gAirTunesDebugUnrecoveredPacketCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugUnexpectedRTPOffsetResetCount   = %u\n", gAirTunesDebugUnexpectedRTPOffsetResetCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugHugeSkewResetCount              = %u\n", gAirTunesDebugHugeSkewResetCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugGlitchCount                     = %u\n", gAirTunesDebugGlitchCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugTimeSyncHugeRTTCount            = %u\n", gAirTunesDebugTimeSyncHugeRTTCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugTimeAnnounceMinNanos            = %llu\n", gAirTunesDebugTimeAnnounceMinNanos );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugTimeAnnounceMaxNanos            = %llu\n", gAirTunesDebugTimeAnnounceMaxNanos );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitActiveCount           = %u\n", gAirTunesDebugRetransmitActiveCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitActiveMax             = %u\n", gAirTunesDebugRetransmitActiveMax );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitSendCount             = %u\n", gAirTunesDebugRetransmitSendCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitSendPerSecAvg         = %u\n", gAirTunesDebugRetransmitSendPerSecAvg );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitRecvCount             = %u\n", gAirTunesDebugRetransmitRecvCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitBigLossCount          = %u\n", gAirTunesDebugRetransmitBigLossCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitAbortCount            = %u\n", gAirTunesDebugRetransmitAbortCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitFutileAbortCount      = %u\n", gAirTunesDebugRetransmitFutileAbortCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitNoFreeNodesCount      = %u\n", gAirTunesDebugRetransmitNoFreeNodesCount);	
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitNotFoundCount         = %u\n", gAirTunesDebugRetransmitNotFoundCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitPrematureCount        = %u\n", gAirTunesDebugRetransmitPrematureCount );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitMaxTries              = %u\n", gAirTunesDebugRetransmitMaxTries );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitMinNanos              = %llu\n", gAirTunesDebugRetransmitMinNanos );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitMaxNanos              = %llu\n", gAirTunesDebugRetransmitMaxNanos );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitAvgNanos              = %llu\n", gAirTunesDebugRetransmitAvgNanos );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitRetryMinNanos         = %llu\n", gAirTunesDebugRetransmitRetryMinNanos );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitRetryMaxNanos         = %llu\n", gAirTunesDebugRetransmitRetryMaxNanos );
		DataBuffer_AppendF( inDB, "\n" );
	#endif
	}
	
	// tweak -- Show tweakable values.
	
	else if( strcmp( inCmd, "tweak" ) == 0 )
	{
	#if( DEBUG )
		DataBuffer_AppendF( inDB, "+-+ AirTunes Tweakables Values +-+\n" );
		DataBuffer_AppendF( inDB, "    gAirTunesDropMinRate                 = %d\n", gAirTunesDropMinRate );
		DataBuffer_AppendF( inDB, "    gAirTunesDropMaxRate                 = %d\n", gAirTunesDropMaxRate );
		DataBuffer_AppendF( inDB, "    gAirTunesSkipMinRate                 = %d\n", gAirTunesSkipMinRate );
		DataBuffer_AppendF( inDB, "    gAirTunesSkipMaxRate                 = %d\n", gAirTunesSkipMaxRate );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugNoSkewSlew             = %s\n", gAirTunesDebugNoSkewSlew ? "PREVENT SLEW" : "ALLOW SLEW" );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugLogAllSkew             = %s\n", gAirTunesDebugLogAllSkew ? "YES" : "NO" );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugNoRetransmits          = %d\n", gAirTunesDebugNoRetransmits );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugPrintPerf              = %d\n", gAirTunesDebugPrintPerf );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugPerfMode               = %d\n", gAirTunesDebugPerfMode );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugRetransmitTiming       = %d\n", gAirTunesDebugRetransmitTiming );
		DataBuffer_AppendF( inDB, "    gAirTunesDebugPollIntervalTicks      = %llu\n", gAirTunesDebugPollIntervalTicks );
		DataBuffer_AppendF( inDB, "    kAirTunesBufferNodeCountUDP          = %u\n", kAirTunesBufferNodeCountUDP );
		DataBuffer_AppendF( inDB, "    kAirTunesRetransmitMaxLoss           = %u\n", kAirTunesRetransmitMaxLoss );
		DataBuffer_AppendF( inDB, "    kAirTunesRetransmitCount             = %u\n", kAirTunesRetransmitCount );
		DataBuffer_AppendF( inDB, "\n" );
	#endif
	}
	
	// mem -- Show memory usage.
	
	else if( strcmp( inCmd, "mem" ) == 0 )
	{
	#if( DEBUG )
		DataBuffer_AppendF( inDB, "+-+ AirTunes Memory Usage +-+\n" );
		DataBuffer_AppendF( inDB, "sizeof( AirPlayReceiverSession ) = %zu\n", sizeof( struct AirPlayReceiverSessionPrivate ) );
		DataBuffer_AppendF( inDB, "Packet Buffer Nodes (UDP)        = %6zu bytes per node * %3d nodes = %7zu bytes total\n", 
			sizeof( AirTunesBufferNode ) + kAirTunesMaxPacketSizeUDP, kAirTunesBufferNodeCountUDP, 
			( sizeof( AirTunesBufferNode ) + kAirTunesMaxPacketSizeUDP ) * kAirTunesBufferNodeCountUDP );
		DataBuffer_AppendF( inDB, "RTP socket buffer size            = %d\n", kAirTunesRTPSocketBufferSize );
		if( gAirTunes ) 
		DataBuffer_AppendF( inDB, "Decode Buffer Size                = %zu\n", gAirTunes->decodeBufferSize );
		DataBuffer_AppendF( inDB, "\n" );
	#endif
	}
	
	// clockOffsets -- Show clock offset history.
	
	else if( strcmp( inCmd, "clockOffsets" ) == 0 )
	{
	#if( DEBUG )
		DataBuffer_AppendF( inDB, "+-+ AirTunes Clock Offset History (%u total) +-+\n", gAirTunesClockOffsetCount );
		for( i = 0; i < gAirTunesClockOffsetCount; ++i )
		{
			if( i == 0 )
			{
				DataBuffer_AppendF( inDB, "%+f\n", gAirTunesClockOffsetHistory[ i ] );
			}
			else
			{
				DataBuffer_AppendF( inDB, "%+f\t%+f\n", gAirTunesClockOffsetHistory[ i ], 
					gAirTunesClockOffsetHistory[ i ] - gAirTunesClockOffsetHistory[ i - 1 ] );
			}
		}
		DataBuffer_AppendF( inDB, "\n" );
	#endif
	}
	
	// rtt -- Show RTT history.
	
	else if( strcmp( inCmd, "rtt" ) == 0 )
	{
	#if( DEBUG )
		DataBuffer_AppendF( inDB, "+-+ AirTunes Clock RTT History (%u total) +-+\n", gAirTunesClockRTTCount );
		for( i = 0; i < gAirTunesClockRTTCount; ++i )
		{
			if( i == 0 )
			{
				DataBuffer_AppendF( inDB, "%+f\n", gAirTunesClockRTTHistory[ i ] );
			}
			else
			{
				DataBuffer_AppendF( inDB, "%+f\t%+f\n", gAirTunesClockRTTHistory[ i ], 
					gAirTunesClockRTTHistory[ i ] - gAirTunesClockRTTHistory[ i - 1 ] );
			}
		}
		DataBuffer_AppendF( inDB, "\n" );
	#endif
	}
	
	// rtpOffsets -- Show RTP offset history.
	
	else if( strcmp( inCmd, "rtpOffsets" ) == 0 )
	{
	#if( DEBUG )
		uint32_t		prev;
		uint32_t		curr;
		
		DataBuffer_AppendF( inDB, "+-+ AirTunes RTP Offset History (%u total) +-+\n", gAirTunesRTPOffsetCount );
		prev = gAirTunesRTPOffsetHistory[ 0 ];
		for( i = 0; i < gAirTunesRTPOffsetCount; ++i )
		{
			curr = gAirTunesRTPOffsetHistory[ i ];
			DataBuffer_AppendF( inDB, "%u %+d\n", curr, (int)( curr - prev ) );
			prev = curr;
		}
		DataBuffer_AppendF( inDB, "\n" );
	#endif
	}
	
	// retrans -- Show retransmit history.
	
	else if( strcmp( inCmd, "retrans" ) == 0 )
	{
	#if( DEBUG )
		AirTunesRetransmitNode *		node;
		int								n;
		uint64_t						nowNanos;
		
		DataBuffer_AppendF( inDB, "+-+ AirTunes Retransmit History (%u total) +-+\n", gAirTunesRetransmitCount );
		
		if( !gAirTunes )
		{
			DataBuffer_AppendF( inDB, "### ERROR: AirTunes not running\n" );
			goto exitWithOutput;
		}
		
		nowNanos = UpNanoseconds();
		_SessionLock( gAirTunes );
		n = 0;
		for( node = gAirTunes->source.rtcpRTBusyList; node; node = node->next )
		{
			DataBuffer_AppendF( inDB, "%3d: retransmit for seq %5hu  T %2hu  start %10llu  next %10lld\n", 
				n, node->seq, node->tries, nowNanos - node->startNanos, 
				( node->nextNanos > 0 ) ? node->nextNanos - nowNanos : UINT64_C( 0 ) );
			++n;
		}
		_SessionUnlock( gAirTunes );
		if( n == 0 ) DataBuffer_AppendF( inDB, "no outstanding retransmits\n" );
		else		 DataBuffer_AppendF( inDB, "\n" );
		DataBuffer_AppendF( inDB, "\n" );
	#endif
	}
	
	// retransDone -- Show retransmit done history.
	
	else if( strcmp( inCmd, "retransDone" ) == 0 )
	{
	#if( DEBUG )
		AirTunesRetransmitHistoryNode *		node;
		uint64_t							minNanos, maxNanos, sumNanos;
		unsigned int						j;
		
		minNanos = ULLONG_MAX;
		maxNanos = 0;
		sumNanos = 0;
		DataBuffer_AppendF( inDB, "+-+ AirTunes Retransmit Done History (%u total) +-+\n", gAirTunesRetransmitCount );
		for( i = 0; i < gAirTunesRetransmitCount; ++i )
		{
			node = &gAirTunesRetransmitHistory[ i ];
			if( node->finalNanos < minNanos ) minNanos = node->finalNanos;
			if( node->finalNanos > maxNanos ) maxNanos = node->finalNanos;
			sumNanos += node->finalNanos;
			
			DataBuffer_AppendF( inDB, "%5u: T %2u  F %10llu  %-9s  ", node->seq, node->tries, node->finalNanos, node->reason );
			for( j = 0; j < countof( node->tryNanos ); ++j )
			{
				DataBuffer_AppendF( inDB, "%10u ", node->tryNanos[ j ] );
			}
			DataBuffer_AppendF( inDB, "\n" );
		}
		if( gAirTunesRetransmitCount > 0 )
		{
			DataBuffer_AppendF( inDB, "Min, Max, Avg Final Nanos: %10llu, %10llu, %10llu\n", minNanos, maxNanos, 
				sumNanos / gAirTunesRetransmitCount );
		}
		else
		{
			DataBuffer_AppendF( inDB, "no retransmits\n" );
		}
		DataBuffer_AppendF( inDB, "\n" );
	#endif
	}
	
	// Help
	
	else if( strcmp( inCmd, "help" ) == 0 )
	{
	#if( DEBUG )
		DataBuffer_AppendF( inDB, "+-+ AirTunes Debug Show Commands +-+\n" );
		DataBuffer_AppendF( inDB, "    globals       -- Show globals.\n" );
		DataBuffer_AppendF( inDB, "    stats         -- Show persistent stats.\n" );
		DataBuffer_AppendF( inDB, "    tweak         -- Show tweakable values.\n" );
		DataBuffer_AppendF( inDB, "    mem           -- Show memory usage.\n" );
		DataBuffer_AppendF( inDB, "    clockOffsets  -- Show clock offset history.\n" );
		DataBuffer_AppendF( inDB, "    rtt           -- Show round-trip-time (RTT) history.\n" );
		DataBuffer_AppendF( inDB, "    rtpOffsets    -- Show RTP offset history.\n" );
		DataBuffer_AppendF( inDB, "    retrans       -- Show retransmit history.\n" );
		DataBuffer_AppendF( inDB, "    retransDone   -- Show retransmit done history.\n" );
		DataBuffer_AppendF( inDB, "\n" );
	#endif
	}
	
	// Unknown
	
	else
	{
		dlog( kLogLevelError, "### Unknown command: \"%s\"\n", inCmd );
		err = kUnsupportedErr;
		goto exit;
	}
	goto exitWithOutput;
	
exitWithOutput:
	err = kNoErr;
	
exit:
	return( err );
}

#if( 1 && DEBUG )
//===========================================================================================================================
//	AirTunesDebugPerf
//===========================================================================================================================

OSStatus	AirTunesDebugPerf( int inPollIntervalSeconds, AirPlayReceiverSessionRef inSession )
{
	uint64_t		ticks;
	
	ticks = UpTicks();
	if( !inSession )
	{
		inSession = gAirTunes;
		if( !inSession ) goto exit;
	}
	else if( ( ticks - gAirTunesDebugLastPollTicks ) < gAirTunesDebugPollIntervalTicks )
	{
		goto exit;
	}
	gAirTunesDebugLastPollTicks = ticks;
	
	if( gAirTunesDebugPerfMode == 0 ) // Packet Info
	{
		uint64_t		sentDiff;
		uint64_t		recvDiff;
		uint16_t		seqDiff;
		uint32_t		lossDiff;
		uint32_t		recvCountDiff;
		uint32_t		glitchDiff;
		double			lossPercent;
		double			deltaTime;
		AirTunesTime	nowAT;
		double			nowFP;
		
		sentDiff = gAirTunesDebugSentByteCount - gAirTunesDebugSentByteCountLast;
		gAirTunesDebugSentByteCountLast += sentDiff;
		
		recvDiff = gAirTunesDebugRecvByteCount - gAirTunesDebugRecvByteCountLast;
		gAirTunesDebugRecvByteCountLast += recvDiff;
		
		if( gAirTunesDebugHighestSeqLast == 0 ) gAirTunesDebugHighestSeqLast = inSession->lastRTPSeq;
		seqDiff = inSession->lastRTPSeq - gAirTunesDebugHighestSeqLast;
		gAirTunesDebugHighestSeqLast += seqDiff;
		
		lossDiff = gAirTunesDebugTotalLossCount - gAirTunesDebugTotalLossCountLast;
		gAirTunesDebugTotalLossCountLast += lossDiff;
		if( seqDiff != 0 )	lossPercent = ( ( (double) lossDiff ) / ( (double) seqDiff ) ) * 100.0;
		else				lossPercent = 0;
		
		recvCountDiff = inSession->source.receiveCount - gAirTunesDebugRecvCountLast;
		gAirTunesDebugRecvCountLast += recvCountDiff;
					
		glitchDiff = gAirTunesDebugGlitchCount - gAirTunesDebugGlitchCountLast;
		gAirTunesDebugGlitchCountLast += glitchDiff;
		
		deltaTime = ( (double)( ticks - gAirTunesDebugLastPollTicks ) ) / ( (double) UpTicksPerSecond() );
		AirTunesClock_GetSynchronizedTime( inSession->airTunesClock, &nowAT );
		nowFP = AirTunesTime_ToFP( &nowAT );
		
		dlog( kLogLevelMax, "S %3llu bytes, R %6llu bytes, R %4u pkts, E %4hu pkts, L %2u pkts, L %5.2f%%, G %3u, D %.0f secs, T %.0f secs\n", 
			sentDiff, recvDiff, (int) recvCountDiff, seqDiff, (int) lossDiff, lossPercent, (int) glitchDiff, deltaTime, nowFP );
	}
	
exit:
	if( inPollIntervalSeconds >= 0 )
	{
		gAirTunesDebugPollIntervalTicks = inPollIntervalSeconds * UpTicksPerSecond();
		gAirTunesDebugPrintPerf			= ( inPollIntervalSeconds > 0 );
	}
	return( kNoErr );
}
#endif // 1 && DEBUG

