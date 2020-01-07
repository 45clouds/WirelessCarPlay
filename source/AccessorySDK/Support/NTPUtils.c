/*
	File:    	NTPUtils.c
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
	
	Copyright (C) 2012-2014 Apple Inc. All Rights Reserved.
*/

#include "NTPUtils.h"

#include <float.h>
#include <math.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include "MathUtils.h"
#include "NetUtils.h"
#include "ThreadUtils.h"
#include "TickUtils.h"

#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

//===========================================================================================================================
//	Internals
//===========================================================================================================================

typedef struct
{
	uint64_t		t1, t2, t3, t4;
	double			rtt;
	
}	NTPSample;

struct NTPClockPrivate
{
	CFRuntimeBase		base;						// CF type info. Must be first.
	
	// Settings
	
	uint32_t			epochSecs;					// Base seconds for timestamps (some clients add NTP->Unix offset).
	sockaddr_ip			peerAddr;					// Address of the peer (i.e. an NTP server).
	int					preferredListenPort;		// UDP port number we'd prefer to listen on.
	int					listenPort;					// UDP port number we're actually listening on.
	LogCategory *		ucat;						// Log category to use for logging.
	Boolean				p2pAllowed;					// True if receiving requests on P2P interfaces is allowed.
	uint32_t			pollMinMs;					// Min milliseconds between requests to the server.
	uint32_t			pollMaxMs;					// Max milliseconds between requests to the server.
	int8_t				precision;					// 2^n precision in seconds (e.g. 2^-19 ~= closest to microseconds (1/524288)).
	Boolean				qosDisabled;				// If true, don't use QoS.
	Boolean				useRTCP;					// If true, send NTP-over-RTCP request packets.
	char *				threadName;					// Name to use when creating threads.
	int					threadPriority;				// Priority to run threads.
	Boolean				hasThreadPriority;			// True if a thread priority has been set.
	uint64_t			ticksPerSec;				// Ticks per second of our local clock.
	
	// Common state.
	
	pthread_mutex_t		lock;						// Protects access to clock state.
	pthread_mutex_t *	lockPtr;					// Ptr to lock for NULL testing.
	pthread_t			thread;						// Thread for sending/receiving requests/responses.
	pthread_t *			threadPtr;					// Ptr to thread for NULL testing.
	SocketRef			cmdSock;					// Socket for communicating with the thread.
	SocketRef			clientSock;					// Socket for sending requests and receiving responses.
	SocketRef			serverSockV4;				// IPv4 socket for receiving requests.
	SocketRef			serverSockV6;				// IPv6 socket for receiving requests.
	
	// Client state
	
	NTPPacketUnion		transmitPkt;				// Packet data used for sending.
	uint32_t *			transmitHiPtr;				// Ptr to the upper 32-bit's of the transmit time.
	uint32_t *			transmitLoPtr;				// Ptr to the lower 32-bit's of the transmit time.
	size_t				transmitLen;				// Number of bytes in the transmit packet.
	uint32_t			transmitTimeHiArray[ 8 ];	// Upper 32 bits of NTP transmit time of last N requests we sent.
	uint32_t			transmitTimeLoArray[ 8 ];	// Lower 32 bits of NTP transmit time of last N requests we sent.
	uint32_t			transmitRequestCount;		// Total number of transmited requests.
	
	NTPSample			sampleArray[ 16 ];			// Most recent responses.
	size_t				sampleCount;				// Total number of responses we've procesed.
	NTPSample			bestArray[ 64 ];			// Best responses at the end of each minimum filter window.
	size_t				bestCount;					// Total number of best responses we've processed.
	
	uint32_t			offsetCount;				// Number of clock offset updates we've processed.
	uint32_t			resetCount;					// Number of times we've reset the clock because it's too far off to slew.
	uint64_t			bigOffset;					// NTP units to add/subtract from our NTP time to bring it "close".
	Boolean				bigNegative;				// True if the bigOffset should be subtracted from our NTP time.
	double				offsetAvg;					// Offset from bigOffset in seconds.
	double				offsetMin;					// Minimum offset from "bigOffset" so far in seconds.
	double				offsetMax;					// Maximum offset from "bigOffset" so far in seconds.
	double				offsetJitter;				// Moving average of absolute differences from the average in seconds.
	
	double				rttAvg;						// Moving average of RTTs.
	double				rttMin;						// Minimum RTT we've measured.
	double				rttMax;						// Maximum RTT we've measured.
	double				rttJitter;					// Moving average of absolute differences from the average.
	
	uint32_t			rateCount;					// Number of rate updates we've performed.
	double				rateAvg;					// Moving average estimate of the remote clock rate.
	double				rateMin;					// Minimum rate we've measured.
	double				rateMax;					// Maximum rate we've measured.
	double				rateJitter;					// Moving average of absolute differences from the average.
	
	int128_compat		scaledUpTime;				// Rate sync'd time since the local clock started in Q64.64 seconds.
	uint64_t			scaledLastTicks;			// UpTicks when "scaledUpTime" was last updated.
	uint64_t			scaledMultiplier;			// Multiplier to scale UpTicks to 1/2^64 units at the sync'd rate.
	uint64_t			scaledTicksPerSec;			// Ticks per second to run the local clock to match the remote clock.
	int128_compat		unscaledUpTime;				// Uncorrected time since the local clocks started in Q64.64 seconds.
	uint64_t			unscaledMultiplier;			// Multiplier to scale UpTicks to 1/2^64 units at the un-sync'd rate.
	
	NTPStats			stats;						// Statistics about the NTP session.
};

static void 	_NTPClockGetTypeID( void *inArg );
static void		_NTPClockFinalize( CFTypeRef inCF );

static OSStatus	_NTPClockClientNegotiate( NTPClockRef inClock );
static OSStatus	_NTPClockClientSendRequest( NTPClockRef inClock );
static OSStatus	_NTPClockClientProcessResponse( NTPClockRef inClock );
static void *	_NTPClockClientThread( void *inArg );
static void		_NTPClockClientTick( NTPClockRef inClock );

static void *	_NTPClockServerThread( void *inArg );
static void		_NTPClockServerProcessPacket( NTPClockRef inClock, SocketRef inSock );

static dispatch_once_t			gNTPClockInitOnce = 0;
static CFTypeID					gNTPClockTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kNTPClockClass = 
{
	0,					// version
	"NTPClock",			// className
	NULL,				// init
	NULL,				// copy
	_NTPClockFinalize,	// finalize
	NULL,				// equal -- NULL means pointer equality.
	NULL,				// hash  -- NULL means pointer hash.
	NULL,				// copyFormattingDesc
	NULL,				// copyDebugDesc
	NULL,				// reclaim
	NULL				// refcount
};

ulog_define( NTPClockCore, kLogLevelVerbose,	kLogFlags_Default, "NTPClock", NULL );
#define ntp_ulog( CLOCK, LEVEL, ... )		ulog( (CLOCK)->ucat, (LEVEL), __VA_ARGS__ )
#define ntp_dlog( CLOCK, LEVEL, ... )		dlogc( (CLOCK)->ucat, (LEVEL), __VA_ARGS__ )

ulog_define( NTPClockRaw, kLogLevelOff,		kLogFlags_Default, "NTPClock", NULL );
#define ntp_raw_ulog( LEVEL, ... )			ulog( &log_category_from_name( NTPClockRaw ), (LEVEL), __VA_ARGS__ )

//===========================================================================================================================
//	NTPClockGetTypeID
//===========================================================================================================================

CFTypeID	NTPClockGetTypeID( void )
{
	dispatch_once_f( &gNTPClockInitOnce, NULL, _NTPClockGetTypeID );
	return( gNTPClockTypeID );
}

static void _NTPClockGetTypeID( void *inArg )
{
	(void) inArg;
	
	gNTPClockTypeID = _CFRuntimeRegisterClass( &kNTPClockClass );
	check( gNTPClockTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	NTPClockCreate
//===========================================================================================================================

OSStatus	NTPClockCreate( NTPClockRef *outListener )
{
	OSStatus		err;
	NTPClockRef		me;
	size_t			extraLen;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (NTPClockRef) _CFRuntimeCreateInstance( NULL, NTPClockGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->ucat			= &log_category_from_name( NTPClockCore );
	me->ticksPerSec		= UpTicksPerSecond();
	me->precision		= -( (int8_t) ilog2_64( me->ticksPerSec ) ); // Precision can't be better than one unit.
	me->cmdSock			= kInvalidSocketRef;
	me->clientSock		= kInvalidSocketRef;
	me->serverSockV4	= kInvalidSocketRef;
	me->serverSockV6	= kInvalidSocketRef;
	
	*outListener = me;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_NTPClockFinalize
//===========================================================================================================================

static void	_NTPClockFinalize( CFTypeRef inCF )
{
	NTPClockRef const		me = (NTPClockRef) inCF;
	
	ForgetMem( &me->threadName );
	check( !me->lockPtr );
	check( !me->threadPtr );
	check( !IsValidSocket( me->cmdSock ) );
	check( !IsValidSocket( me->clientSock ) );
	check( !IsValidSocket( me->serverSockV4 ) );
	check( !IsValidSocket( me->serverSockV6 ) );
}

//===========================================================================================================================
//	NTPClockStop
//===========================================================================================================================

void	NTPClockStop( NTPClockRef inClock )
{
	OSStatus		err;
	Boolean			wasStarted, wasClient;
	
	DEBUG_USE_ONLY( err );
	
	wasStarted = inClock->threadPtr ? true : false;
	wasClient  = IsValidSocket( inClock->clientSock );
	if( inClock->threadPtr )
	{
		err = SendSelfConnectedLoopbackMessage( inClock->cmdSock, "q", 1 );
		check_noerr( err );
		
		err = pthread_join( inClock->thread, NULL );
		check_noerr( err );
		inClock->threadPtr = NULL;
	}
	ForgetSocket( &inClock->cmdSock );
	ForgetSocket( &inClock->clientSock );
	ForgetSocket( &inClock->serverSockV4 );
	ForgetSocket( &inClock->serverSockV6 );
	pthread_mutex_forget( &inClock->lockPtr );
	inClock->listenPort = 0;
	
	if( wasStarted ) ntp_ulog( inClock, kLogLevelTrace, "NTP %s stopped\n", wasClient ? "client" : "server" );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	NTPClockSetEpoch
//===========================================================================================================================

void	NTPClockSetEpoch( NTPClockRef inClock, uint32_t inEpochSecs )
{
	inClock->epochSecs = inEpochSecs;
}

//===========================================================================================================================
//	NTPClockSetLogging
//===========================================================================================================================

void	NTPClockSetLogging( NTPClockRef inClock, LogCategory *inCategory )
{
	inClock->ucat = inCategory;
}

//===========================================================================================================================
//	NTPClockSetP2P
//===========================================================================================================================

void	NTPClockSetP2P( NTPClockRef inClock, Boolean inP2PAllowed )
{
	inClock->p2pAllowed = inP2PAllowed;
}

//===========================================================================================================================
//	NTPClockSetPeer
//===========================================================================================================================

void	NTPClockSetPeer( NTPClockRef inClock, const void *inSockAddr, int inDefaultPort )
{
	SockAddrCopy( inSockAddr, &inClock->peerAddr );
	if( ( inDefaultPort > 0 ) && ( SockAddrGetPort( inSockAddr ) == 0 ) )
	{
		SockAddrSetPort( &inClock->peerAddr, inDefaultPort );
	}
}

//===========================================================================================================================
//	NTPClockGetListenPort / NTPClockSetListenPort
//===========================================================================================================================

int	NTPClockGetListenPort( NTPClockRef inClock )
{
	return( inClock->listenPort );
}

void	NTPClockSetListenPort( NTPClockRef inClock, int inPort )
{
	inClock->preferredListenPort = inPort;
}

//===========================================================================================================================
//	NTPClockSetQoSDisabled
//===========================================================================================================================

void	NTPClockSetQoSDisabled( NTPClockRef inClock, Boolean inQoSDisabled )
{
	inClock->qosDisabled = inQoSDisabled;
}

//===========================================================================================================================
//	NTPClockSetRTCP
//===========================================================================================================================

void	NTPClockSetRTCP( NTPClockRef inClock, Boolean inUseRTCP )
{
	inClock->useRTCP = inUseRTCP;
}

//===========================================================================================================================
//	NTPClockGetStats
//===========================================================================================================================

void	NTPClockGetStats( NTPClockRef inClock, NTPStats *outStats )
{
	*outStats = inClock->stats;
}

//===========================================================================================================================
//	NTPClockSetThreadName
//===========================================================================================================================

OSStatus	NTPClockSetThreadName( NTPClockRef inClock, const char *inName )
{
	OSStatus		err;
	char *			name;
	
	if( inName )
	{
		name = strdup( inName );
		require_action( name, exit, err = kNoMemoryErr );
	}
	else
	{
		name = NULL;
	}
	if( inClock->threadName ) free( inClock->threadName );
	inClock->threadName = name;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	NTPClockSetThreadPriority
//===========================================================================================================================

void	NTPClockSetThreadPriority( NTPClockRef inClock, int inPriority )
{
	inClock->threadPriority = inPriority;
	inClock->hasThreadPriority = true;
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	NTPClockStartClient
//===========================================================================================================================

OSStatus	NTPClockStartClient( NTPClockRef inClock )
{
	OSStatus		err;
	SocketRef		sock;
	
	err = pthread_mutex_init( &inClock->lock, NULL );
	require_noerr( err, exit );
	inClock->lockPtr = &inClock->lock;
	
	// Set fixed fields of the request packet once up front and init stats.
	
	if( inClock->useRTCP )
	{
		inClock->transmitPkt.rtcp.v_p_m		= 2 << 6;
		inClock->transmitPkt.rtcp.pt		= 210;
		inClock->transmitPkt.rtcp.length	= htons( ( sizeof( inClock->transmitPkt.rtcp ) / 4 ) - 1 );
		inClock->transmitHiPtr				= &inClock->transmitPkt.rtcp.ntpTransmitHi;
		inClock->transmitLoPtr				= &inClock->transmitPkt.rtcp.ntpTransmitLo;
		inClock->transmitLen				= sizeof( inClock->transmitPkt.rtcp );
	}
	else
	{
		NTPPacket_SetLeap_Version_Mode( &inClock->transmitPkt.ntp, kNTPLeap_NoWarning, kNTPVersion4, kNTPMode_Client );
		inClock->transmitHiPtr	= &inClock->transmitPkt.ntp.transmitTimeHi;
		inClock->transmitLoPtr	= &inClock->transmitPkt.ntp.transmitTimeLo;
		inClock->transmitLen	= sizeof( inClock->transmitPkt.ntp );
	}
	inClock->transmitRequestCount	= 0;
	inClock->sampleCount			= 0;
	inClock->bestCount				= 0;
	inClock->pollMinMs				= 1000;
	inClock->pollMaxMs				= 1100;
	inClock->offsetCount			= 0;
	inClock->resetCount				= 0;
	inClock->bigOffset				= 0;
	inClock->bigNegative			= false;
	inClock->offsetAvg				= 0;
	inClock->offsetMin				= DBL_MAX;
	inClock->offsetMax				= DBL_MIN;
	inClock->offsetJitter			= 0;
	
	inClock->rttAvg					= 0;
	inClock->rttMin					= DBL_MAX;
	inClock->rttMax					= DBL_MIN;
	inClock->rttJitter				= 0;
	
	inClock->rateCount				= 0;
	inClock->rateAvg				= 1.0;
	inClock->rateMin				= DBL_MAX;
	inClock->rateMax				= DBL_MIN;
	inClock->rateJitter				= 0;
	
	inClock->scaledUpTime.hi		= 0;
	inClock->scaledUpTime.lo		= 0;
	inClock->scaledLastTicks		= UpTicks();
	inClock->scaledMultiplier		= UINT64_C( 0xFFFFFFFFFFFFFFFF ) / inClock->ticksPerSec;
	inClock->scaledTicksPerSec		= inClock->ticksPerSec;
	inClock->unscaledUpTime.hi		= 0;
	inClock->unscaledUpTime.lo		= 0;
	inClock->unscaledMultiplier		= inClock->scaledMultiplier;
	
	memset( &inClock->stats, 0, sizeof( inClock->stats ) );
	
	// Set up networking and get an initial estimate of sync by negotiation with the server.
	
	if( SockAddrGetPort( &inClock->peerAddr ) == 0 ) SockAddrSetPort( &inClock->peerAddr, 7010 );
	err = UDPClientSocketOpen( AF_UNSPEC, &inClock->peerAddr, 0, inClock->preferredListenPort, &inClock->listenPort, &sock );
	require_noerr( err, exit );
	inClock->clientSock = sock;
	
	if( inClock->p2pAllowed ) SocketSetP2P( sock, true );
	if( !inClock->qosDisabled ) SocketSetQoS( sock, kSocketQoS_NTP );
	SocketSetPacketTimestamps( sock, true );
	
	err = _NTPClockClientNegotiate( inClock );
	require_noerr_quiet( err, exit );
	
	// Start a thread to periodically poll the NTP server to sync to its offset/rate.
	
	err = OpenSelfConnectedLoopbackSocket( &inClock->cmdSock );
	require_noerr( err, exit );
	
	err = pthread_create( &inClock->thread, NULL, _NTPClockClientThread, inClock );
	require_noerr( err, exit );
	inClock->threadPtr = &inClock->thread;
	
	ntp_ulog( inClock, kLogLevelTrace, "NTP client started with %##a on port %d\n", &inClock->peerAddr, inClock->listenPort );
	
exit:
	if( err )
	{
		ntp_ulog( inClock, kLogLevelWarning, "### NTP client start failed: %#m\n", err );
		NTPClockStop( inClock );
	}
	return( err );
}

//===========================================================================================================================
//	_NTPClockClientNegotiate
//===========================================================================================================================

static OSStatus	_NTPClockClientNegotiate( NTPClockRef inClock )
{
	OSStatus			err = kNoErr;
	int					nGood = 0;
	fd_set				readSet;
	uint64_t			startTicks, deadlineTicks;
	int					n;
	struct timeval		timeout;
	
	ntp_ulog( inClock, kLogLevelTrace, "NTP client negotiating with %##a\n", &inClock->peerAddr );
	startTicks = UpTicks();
	FD_ZERO( &readSet );
	deadlineTicks = startTicks + SecondsToUpTicks( 10 );
	while( nGood < 4 )
	{
		require_action_quiet( UpTicks() < deadlineTicks, exit, err = kTimeoutErr );
		
		if( err != kSkipErr )
		{
			err = _NTPClockClientSendRequest( inClock );
			if( err ) { usleep( 100000 ); continue; }
		}
		do
		{
			FD_SET( inClock->clientSock, &readSet );
			timeout.tv_sec  = 0;
			timeout.tv_usec = 200 * 1000;
			n = select( inClock->clientSock + 1, &readSet, NULL, NULL, &timeout );
			err = select_errno( n );
			
		}	while( err == EINTR );
		if( err )
		{
			++inClock->stats.timeouts;
			ntp_ulog( inClock, kLogLevelNotice, "### NTP client negotiate wait for %##a failed (%u total): %#m\n", 
				&inClock->peerAddr, inClock->stats.timeouts, err );
			continue;
		}
		
		err = _NTPClockClientProcessResponse( inClock );
		if( err ) continue;
		++nGood;
	}
	
exit:
	ntp_ulog( inClock, err ? kLogLevelWarning : kLogLevelTrace, 
		"%sNTP client negotiation with %##a %s: Good=%d, Serr=%u, Tout=%u, Rerr=%u Dur=%llu ms%s%?#m\n", 
		err ? "### " : "", &inClock->peerAddr, err ? "failed" : "succeeded", 
		nGood, inClock->stats.sendErrors, inClock->stats.timeouts, inClock->stats.receiveErrors, 
		UpTicksToMilliseconds( UpTicks() - startTicks ), err ? ", " : "", err, err );
	return( err );
}

//===========================================================================================================================
//	_NTPClockClientSendRequest
//===========================================================================================================================

static OSStatus	_NTPClockClientSendRequest( NTPClockRef inClock )
{
	OSStatus		err;
	ssize_t			n;
	uint64_t		ticks;
	uint32_t		transmitHi, transmitLo;
	size_t			i;
	
	transmitHi = ( (uint32_t)( ( ticks = UpTicks() ) / inClock->ticksPerSec ) ) + inClock->epochSecs;
	transmitLo = (uint32_t)( ( ( ticks % inClock->ticksPerSec ) << 32 ) / inClock->ticksPerSec );
	*inClock->transmitHiPtr	= htonl( transmitHi );
	*inClock->transmitLoPtr	= htonl( transmitLo );
	
	n = send( inClock->clientSock, (const char *) &inClock->transmitPkt, inClock->transmitLen, 0 );
	err = map_socket_value_errno( inClock->clientSock, n == (ssize_t) inClock->transmitLen, n );
	require_noerr_quiet( err, exit );
	
	i = inClock->transmitRequestCount++ % countof( inClock->transmitTimeHiArray );
	inClock->transmitTimeHiArray[ i ] = transmitHi;
	inClock->transmitTimeLoArray[ i ] = transmitLo;
	ntp_ulog( inClock, kLogLevelChatty, "NTP client request: 0x%08X%08X\n", transmitHi, transmitLo );
	
exit:
	if( err )
	{
		inClock->stats.sendErrors += 1;
		ntp_ulog( inClock, kLogLevelNotice, "### NTP client send to %##a failed (%u total): %#m\n", 
			&inClock->peerAddr, inClock->stats.sendErrors, err );
	}
	return( err );
}

//===========================================================================================================================
//	_NTPClockClientProcessResponse
//===========================================================================================================================

static OSStatus	_NTPClockClientProcessResponse( NTPClockRef inClock )
{
	OSStatus				err;
	NTPPacketUnion			pkt;
	size_t					len;
	uint64_t				receiveTicks;
	uint32_t				originateTimeHi, originateTimeLo, receiveTimeHi, receiveTimeLo, transmitTimeHi, transmitTimeLo;
	uint64_t				t1, t2, t3, t4, t1a, t4a, u64;
	double					offset, rtt, bestRTT, forwardRate, backwardRate, rate, d;
	size_t					oldCount, newCount, newIndex, bestIndex, windowSize, i, n, j;
	NTPSample *				newSample;
	const NTPSample *		oldestBest;
	const NTPSample *		newestBest;
	Boolean					isBest, isMin, isMax, b;
	int128_compat			adj;
	
	err = SocketRecvFrom( inClock->clientSock, &pkt, sizeof( pkt ), &len, NULL, 0, NULL, &receiveTicks, NULL, NULL );
	require_noerr_quiet( err, exit );
	if( len == sizeof( pkt.ntp ) )
	{
		// Ignore responses used for establishing UDP flows to avoid blocking by firewalls.
		
		if( ( pkt.ntp.stratum == 0 ) && ( ntohl( pkt.ntp.referenceID ) == kNTPReferenceID_Ignore ) )
		{
			err = kSkipErr;
			goto exit2;
		}
		
		originateTimeHi	= ntohl( pkt.ntp.originateTimeHi );
		originateTimeLo	= ntohl( pkt.ntp.originateTimeLo );
		receiveTimeHi	= ntohl( pkt.ntp.receiveTimeHi ) + inClock->epochSecs;
		receiveTimeLo	= ntohl( pkt.ntp.receiveTimeLo );
		transmitTimeHi	= ntohl( pkt.ntp.transmitTimeHi );
		transmitTimeLo	= ntohl( pkt.ntp.transmitTimeLo );
	}
	else if( len == sizeof( pkt.rtcp  ) )
	{
		originateTimeHi	= ntohl( pkt.rtcp.ntpOriginateHi );
		originateTimeLo	= ntohl( pkt.rtcp.ntpOriginateLo );
		receiveTimeHi	= ntohl( pkt.rtcp.ntpReceiveHi ) + inClock->epochSecs;
		receiveTimeLo	= ntohl( pkt.rtcp.ntpReceiveLo );
		transmitTimeHi	= ntohl( pkt.rtcp.ntpTransmitHi );
		transmitTimeLo	= ntohl( pkt.rtcp.ntpTransmitLo );
	}
	else
	{
		dlogassert( "Bad packet length: %zu", len );
		err = kSizeErr;
		goto exit;
	}
	t1 = ( ( (uint64_t) originateTimeHi ) << 32 ) | originateTimeLo;
	t2 = ( ( (uint64_t) receiveTimeHi )   << 32 ) | receiveTimeLo;
	t3 = ( ( (uint64_t) transmitTimeHi )  << 32 ) | transmitTimeLo;
	t4 = UpTicksToNTP( receiveTicks );
	
	// Reject the packet if it's not in our request list (too old, dup, etc.).
	
	b = ( ( originateTimeHi != 0 ) || ( originateTimeLo != 0 ) );
	if( b )	n = Min( inClock->transmitRequestCount, countof( inClock->transmitTimeHiArray ) );
	else	n = 0; // Don't let a zero originate timestamp match one of our zero'd slots.
	for( i = 0; i < n; ++i )
	{
		if( ( inClock->transmitTimeHiArray[ i ] == originateTimeHi ) &&
			( inClock->transmitTimeLoArray[ i ] == originateTimeLo ) )
		{
			break;
		}
	}
	if( i >= n )
	{
		++inClock->stats.unexpected;
		ntp_ulog( inClock, kLogLevelNotice, 
			"### NTP client received unexpected response: originated 0x%016llX, received 0x%016llX (%u total)\n", 
			t1, t4, inClock->stats.unexpected );
		err = kUnexpectedErr;
		goto exit2;
	}
	inClock->transmitTimeHiArray[ i ] = 0; // Zero so we don't try to process a duplicate in the future.
	inClock->transmitTimeLoArray[ i ] = 0;
	
	// Client:  T1           T4
	// ----------------------------->
	//           \           ^
	//            \         /
	//             v       /
	// ----------------------------->
	// Server:     T2     T3
	// 
	// Clock offset		= ((T2 - T1) + (T3 - T4)) / 2
	// RTT				=  (T4 - T1) - (T3 - T2)
	// Forward rate		= (T2i - T2i-1) / (T1i - T1i-1) -- Client -> Server path
	// Backward rate	= (T3i - T3i-1) / (T4i - T4i-1) -- Server -> Client path
	// Rate				= (Forward rate + Backward rate) / 2
	
	oldCount		= inClock->sampleCount++;
	newCount		= oldCount + 1;
	newIndex		= oldCount % countof( inClock->sampleArray );
	newSample		= &inClock->sampleArray[ newIndex ];
	newSample->t1	= t1;
	newSample->t2	= t2;
	newSample->t3	= t3;
	newSample->t4	= t4;
	newSample->rtt	= rtt = NTPCalculateRTTSeconds_FP( t1, t2, t3, t4 );
	
	// Update RTT stats and if the new sample has the best RTT in recent history, use it to update the offset.
	// This uses a smaller window size while warming up to react more quickly.
	
	bestRTT		= rtt;
	bestIndex	= newIndex;
	isBest		= true;
	if(      oldCount >= 28 )	windowSize = 16;
	else if( oldCount >= 16 )	windowSize = oldCount - 12;
	else						windowSize = 4;
	for( i = ( oldCount >= windowSize ) ? ( oldCount - ( windowSize - 1 ) ) : 0; i < oldCount; ++i )
	{
		j = i % countof( inClock->sampleArray );
		d = inClock->sampleArray[ j ].rtt;
		if( d < bestRTT )
		{
			bestRTT   = d;
			bestIndex = j;
			isBest    = false;
		}
	}
	if( oldCount == 0 )
	{
		inClock->rttAvg = rtt;
	}
	else
	{
		inClock->rttJitter = MovingAverageF( inClock->rttJitter, fabs( rtt - inClock->rttAvg ), 0.125 );
		inClock->rttAvg    = MovingAverageF( inClock->rttAvg, rtt, 0.125 );
	}
	if( rtt < inClock->rttMin ) inClock->rttMin = rtt;
	if( rtt > inClock->rttMax ) inClock->rttMax = rtt;
	
	// Update the clock offset. If the clock offset is large, it could overflow the 64-bit timestamp format.
	// For example, if the NTP server is set to 2013, but the client is starting from 1900 (NTP 0), it would 
	// exceed a signed 64-bit value. So this first calculates an approximate offset and if it was negative.
	// That is used to adjust the timestamps to be close. The full resolution offset is calculated from that.
	
	if( ( newCount >= 4 ) && ( isBest || ( inClock->offsetCount == 0 ) ) )
	{
		newestBest = &inClock->sampleArray[ bestIndex ];
		t1a = inClock->bigNegative ? ( newestBest->t1 - inClock->bigOffset ) : ( newestBest->t1 + inClock->bigOffset );
		t4a = inClock->bigNegative ? ( newestBest->t4 - inClock->bigOffset ) : ( newestBest->t4 + inClock->bigOffset );
		if( int128_gt( &inClock->scaledUpTime, &inClock->unscaledUpTime ) )
		{
			int128_sub( &adj, &inClock->scaledUpTime, &inClock->unscaledUpTime );
			u64 = ( adj.hi << 32 ) | ( adj.lo >> 32 );
			t1a -= u64;
			t4a -= u64;
		}
		else
		{
			int128_sub( &adj, &inClock->unscaledUpTime, &inClock->scaledUpTime );
			u64 = ( adj.hi << 32 ) | ( adj.lo >> 32 );
			t1a += u64;
			t4a += u64;
		}
		if( AbsoluteDiff( t1a, newestBest->t2 ) > SecondsToNTP( 1 ) )
		{
			t1a = newestBest->t1;
			inClock->bigNegative = ( t1a > newestBest->t2 );
			inClock->bigOffset = inClock->bigNegative ? ( t1a - newestBest->t2 ) : ( newestBest->t2 - t1a );
			ntp_ulog( inClock, ( inClock->resetCount > 0 ) ? kLogLevelNotice : kLogLevelInfo, 
				"%sNTP client clock reset: %s%{dur}\n", ( inClock->resetCount > 0 ) ? "### " : "", 
				inClock->bigNegative ? "-" : "+", NTPtoSeconds( inClock->bigOffset ) );
			++inClock->resetCount;
			t1a = newestBest->t2;
			t4a = newestBest->t3;
		}
		d = NTPCalculateOffsetSeconds_FP( t1a, newestBest->t2, newestBest->t3, t4a );
		if( inClock->offsetCount++ == 0 )
		{
			inClock->offsetAvg = 0;
		}
		else
		{
			inClock->offsetJitter = MovingAverageF( inClock->offsetJitter, fabs( d - inClock->offsetAvg ), 0.125 );
			inClock->offsetAvg    = MovingAverageF( inClock->offsetAvg, d, 0.125 );
		}
		if( d < inClock->offsetMin ) inClock->offsetMin = d;
		if( d > inClock->offsetMax ) inClock->offsetMax = d;
		offset = d;
	}
	else
	{
		offset = 0;
	}
	
	if( oldCount == 0 )
	{
		ntp_raw_ulog( kLogLevelInfo, 
			"NTP Raw: #\tT1\tT2\tT3\tT4\t"
			"Offset ms\tOffsetAvg\tOffsetMin\tOffsetMax\tOffsetJitter\t"
			"RTT ms\tRTTavg\tRTTmin\tRTTmax\tRTTjitter\n" );
	}
	ntp_raw_ulog( kLogLevelInfo, 
		"NTP Raw: %zu\t%llu\t%llu\t%llu\t%llu\t"
		"%.9f\t%.9f\t%.9f\t%.9f\t%.9f\t"
		"%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t"
		"%s\n",
		newCount, t1, t2, t3, t4, 
		1000 * offset, 1000 * inClock->offsetAvg, 1000 * inClock->offsetMin, 1000 * inClock->offsetMax, 1000 * inClock->offsetJitter, 
		1000 * rtt, 1000 * inClock->rttAvg, 1000 * inClock->rttMin, 1000 * inClock->rttMax, 1000 * inClock->rttJitter, 
		isBest ? "\tBest" : "" );
	
	// Update our history of "best" samples and our estimate of the rate at the end of each minimum filter window.
	
	if( ( newCount % windowSize ) == 0 )
	{
		i = inClock->bestCount;
		inClock->bestArray[ i++ % countof( inClock->bestArray ) ] = inClock->sampleArray[ bestIndex ];
		inClock->bestCount = i;
		
		oldestBest = &inClock->bestArray[ i % Min( i, countof( inClock->bestArray ) ) ];
		newestBest = &inClock->sampleArray[ bestIndex ];
		if( ( newestBest->t1 > oldestBest->t1 ) && 
			( newestBest->t2 > oldestBest->t2 ) && 
			( newestBest->t3 > oldestBest->t3 ) && 
			( newestBest->t4 > oldestBest->t4 ) )
		{
			forwardRate  = ( (double)( (int64_t)( newestBest->t2 - oldestBest->t2 ) ) ) /
						   ( (double)( (int64_t)( newestBest->t1 - oldestBest->t1 ) ) );
			backwardRate = ( (double)( (int64_t)( newestBest->t3 - oldestBest->t3 ) ) ) /
						   ( (double)( (int64_t)( newestBest->t4 - oldestBest->t4 ) ) );
			rate = ( forwardRate + backwardRate ) / 2;
			inClock->rateJitter = MovingAverageF( inClock->rateJitter, fabs( rate - inClock->rateAvg ), 0.125 );
			inClock->rateAvg = MovingAverageF( inClock->rateAvg, rate, 0.25 );
			if( ( isMin = ( rate < inClock->rateMin ) ) ) inClock->rateMin = rate;
			if( ( isMax = ( rate > inClock->rateMax ) ) ) inClock->rateMax = rate;
			inClock->scaledTicksPerSec = (uint64_t)( inClock->ticksPerSec * inClock->rateAvg );
			inClock->scaledMultiplier = UINT64_C( 0xFFFFFFFFFFFFFFFF ) / inClock->scaledTicksPerSec;
			
			if( inClock->rateCount == 0 )
			{
				ntp_ulog( inClock, kLogLevelVerbose, 
				"NTP client update: #\tRTT ms\tRTT ! \tRateNew    \tRateAvg    \tRateHz    \tJitHz\tOffset ms\tOffset Avg\tFlags\n" );
			}
			ntp_ulog( inClock, kLogLevelVerbose, 
				"NTP client update: %zu\t%.3f\t%.3f\t%.9f\t%.9f\t%llu\t%llu\t%+.9f\t%+.9f%s%s%s%s\n", 
				newCount, 1000 * newSample->rtt, 1000 * bestRTT, rate, inClock->rateAvg, 
				inClock->scaledTicksPerSec, (uint64_t)( inClock->rateJitter * inClock->ticksPerSec ), 
				1000 * offset, 1000 * inClock->offsetAvg, ( isBest || isMin || isMax ) ? "\t" : "", 
				isBest ? "Best " : "", isMin ? "Min " : "", isMax ? "Max " : "" );
			++inClock->rateCount;
		}
	}
	b = false;
	if( newCount == 1 )
	{
		b = true;
	}
	if( newCount == 32 )
	{
		inClock->pollMinMs = 2000;
		inClock->pollMaxMs = 2100;
		b = true;
	}
	if( b ) ntp_ulog( inClock, kLogLevelVerbose, "NTP client poll updated: %u-%u ms\n", inClock->pollMinMs, inClock->pollMaxMs );
	
exit:
	if( err )
	{
		++inClock->stats.receiveErrors;
		ntp_ulog( inClock, inClock->threadPtr ? kLogLevelInfo : kLogLevelNotice, 
			"### NTP client received bad response from %##a (%u total): %#m\n", 
			&inClock->peerAddr, inClock->stats.receiveErrors, err );
	}
	
exit2:
	return( err );
}

//===========================================================================================================================
//	_NTPClockClientThread
//===========================================================================================================================

static void *	_NTPClockClientThread( void *inArg )
{
	NTPClockRef const		ntpClock			= (NTPClockRef) inArg;
	SocketRef const			cmdSock				= ntpClock->cmdSock;
	SocketRef const			sock				= ntpClock->clientSock;
	uint64_t const			updateIntervelTicks	= ntpClock->ticksPerSec / 10;
	fd_set					readSet;
	int						maxFd;
	int						n;
	OSStatus				err;
	uint64_t				nextRequestTicks, nextUpdateTicks, ticks;
	struct timeval			timeout;
	
	SetThreadName( ntpClock->threadName ? ntpClock->threadName : "NTPClockClient" );
	if( ntpClock->hasThreadPriority ) SetCurrentThreadPriority( ntpClock->threadPriority );
	
	maxFd = -1;
	if( (int) cmdSock > maxFd ) maxFd = cmdSock;
	if( (int) sock    > maxFd ) maxFd = sock;
	maxFd += 1;
	FD_ZERO( &readSet );
	
	ticks				= UpTicks();
	nextRequestTicks	= ticks + MillisecondsToUpTicks( RandomRange( ntpClock->pollMinMs, ntpClock->pollMaxMs ) );
	nextUpdateTicks		= ticks + updateIntervelTicks;
	for( ;; )
	{
		FD_SET( cmdSock, &readSet );
		FD_SET( sock, &readSet );
		ticks = ( nextUpdateTicks < nextRequestTicks ) ? nextUpdateTicks : nextRequestTicks;
		n = select( maxFd, &readSet, NULL, NULL, UpTicksToTimeValTimeout( ticks, &timeout ) );
		err = select_errno( n );
		if( err == EINTR ) continue;
		if( err == kTimeoutErr )
		{
			ticks = UpTicks();
			if( ticks >= nextUpdateTicks )
			{
				_NTPClockClientTick( ntpClock );
				nextUpdateTicks = ticks + updateIntervelTicks;
			}
			if( ticks >= nextRequestTicks )
			{
				_NTPClockClientSendRequest( ntpClock );
				nextRequestTicks = ticks + MillisecondsToUpTicks( RandomRange( ntpClock->pollMinMs, ntpClock->pollMaxMs ) );
			}
			continue;
		}
		if( err ) { dlogassert( "select() error: %#m", err ); usleep( 100000 ); continue; }
		
		if( FD_ISSET( sock,    &readSet ) ) _NTPClockClientProcessResponse( ntpClock );
		if( FD_ISSET( cmdSock, &readSet ) ) break; // The only event is quit so break if anything is pending.
	}
	return( NULL );
}

//===========================================================================================================================
//	_NTPClockClientTick
//===========================================================================================================================

static void	_NTPClockClientTick( NTPClockRef inClock )
{
	uint64_t		ticks;
	uint32_t		delta;
	
	ticks = UpTicks();
	pthread_mutex_lock( inClock->lockPtr );
		delta = (uint32_t)( ticks - inClock->scaledLastTicks );
		int128_addlo( &inClock->scaledUpTime,   &inClock->scaledUpTime,   delta * inClock->scaledMultiplier );
		int128_addlo( &inClock->unscaledUpTime, &inClock->unscaledUpTime, delta * inClock->unscaledMultiplier );
		inClock->scaledLastTicks = ticks;
	pthread_mutex_unlock( inClock->lockPtr );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	NTPClockGetSynchronizedNTPTime
//===========================================================================================================================

uint64_t	NTPClockGetSynchronizedNTPTime( NTPClockRef inClock )
{
	int128_compat		t;
	
	pthread_mutex_lock( inClock->lockPtr );
		int128_addlo( &t, &inClock->scaledUpTime, ( UpTicks() - inClock->scaledLastTicks ) * inClock->scaledMultiplier );
	pthread_mutex_unlock( inClock->lockPtr );
	return( ( t.hi << 32 ) | ( t.lo >> 32 ) ); // Middle 64 bits of 128-bit value.
}

//===========================================================================================================================
//	NTPClockGetSynchronizedNTPTimeNearUpTicks
//===========================================================================================================================

uint64_t	NTPClockGetSynchronizedNTPTimeNearUpTicks( NTPClockRef inClock, uint64_t inTicks )
{
	uint64_t			ticks, delta, scale, scaledTicksPerSec;
	int128_compat		t, adj;
	Boolean				future;
	
	ticks  = UpTicks();
	future = ( inTicks > ticks );
	delta  = future ? ( inTicks - ticks ) : ( ticks - inTicks );
	
	pthread_mutex_lock( inClock->lockPtr );
		scale = inClock->scaledMultiplier;
		scaledTicksPerSec = inClock->scaledTicksPerSec;
		int128_addlo( &t, &inClock->scaledUpTime, ( ticks - inClock->scaledLastTicks ) * scale );
	pthread_mutex_unlock( inClock->lockPtr );
	
	adj.hi =   delta / scaledTicksPerSec;
	adj.lo = ( delta % scaledTicksPerSec ) * scale;
	if( future )	int128_add( &t, &t, &adj );
	else			int128_sub( &t, &t, &adj );
	return( ( t.hi << 32 ) | ( t.lo >> 32 ) ); // Middle 64 bits of 128-bit value.
}

//===========================================================================================================================
//	NTPClockGetUpTicksNearSynchronizedNTPTime
//===========================================================================================================================

uint64_t	NTPClockGetUpTicksNearSynchronizedNTPTime( NTPClockRef inClock, uint64_t inNTP )
{
	uint64_t		nowNTP, ticks;
	
	nowNTP = NTPClockGetSynchronizedNTPTime( inClock );
	if( inNTP > nowNTP )	ticks = UpTicks() + NTPtoUpTicks( inNTP  - nowNTP );
	else					ticks = UpTicks() - NTPtoUpTicks( nowNTP - inNTP );
	return( ticks );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	NTPClockStartServer
//===========================================================================================================================

OSStatus	NTPClockStartServer( NTPClockRef inClock )
{
	OSStatus		err;
	SocketRef		sockV4, sockV6;
	
	err = UDPServerSocketPairOpen( inClock->preferredListenPort, &inClock->listenPort, &sockV4, &sockV6 );
	require_noerr( err, exit );
	inClock->serverSockV4 = sockV4;
	inClock->serverSockV6 = sockV6;
	if( IsValidSocket( sockV4 ) )
	{
		if( inClock->p2pAllowed )	SocketSetP2P( sockV4, true );
		if( !inClock->qosDisabled )	SocketSetQoS( sockV4, kSocketQoS_NTP );
		SocketSetPacketTimestamps( sockV4, true );
	}
	if( IsValidSocket( sockV6 ) )
	{
		if( inClock->p2pAllowed )	SocketSetP2P( sockV6, true );
		if( !inClock->qosDisabled )	SocketSetQoS( sockV6, kSocketQoS_NTP );
		SocketSetPacketTimestamps( sockV6, true );
	}
	
	err = OpenSelfConnectedLoopbackSocket( &inClock->cmdSock );
	require_noerr( err, exit );
	
	err = pthread_create( &inClock->thread, NULL, _NTPClockServerThread, inClock );
	require_noerr( err, exit );
	inClock->threadPtr = &inClock->thread;
	
	ntp_ulog( inClock, kLogLevelTrace, "NTP server started on port %d\n", inClock->listenPort );
	
exit:
	if( err )
	{
		ntp_ulog( inClock, kLogLevelWarning, "### NTP server start failed: %#m\n", err );
		NTPClockStop( inClock );
	}
	return( err );
}

//===========================================================================================================================
//	_NTPClockServerThread
//===========================================================================================================================

static void *	_NTPClockServerThread( void *inArg )
{
	NTPClockRef const		ntpClock	= (NTPClockRef) inArg;
	SocketRef const			cmdSock		= ntpClock->cmdSock;
	SocketRef const			sockV4		= ntpClock->serverSockV4;
	SocketRef const			sockV6		= ntpClock->serverSockV6;
	fd_set					readSet;
	int						maxFd;
	int						n;
	OSStatus				err;
	
	SetThreadName( ntpClock->threadName ? ntpClock->threadName : "NTPClockServer" );
	if( ntpClock->hasThreadPriority ) SetCurrentThreadPriority( ntpClock->threadPriority );
	
	FD_ZERO( &readSet );
	maxFd = -1;
	if( (int) cmdSock > maxFd ) maxFd = cmdSock;
	if( (int) sockV4  > maxFd ) maxFd = sockV4;
	if( (int) sockV6  > maxFd ) maxFd = sockV6;
	maxFd += 1;
	for( ;; )
	{
		FD_SET( cmdSock, &readSet );
		if( IsValidSocket( sockV4 ) ) FD_SET( sockV4, &readSet );
		if( IsValidSocket( sockV6 ) ) FD_SET( sockV6, &readSet );
		
		n = select( maxFd, &readSet, NULL, NULL, NULL );
		err = select_errno( n );
		if( err == EINTR ) continue;
		if( err ) { dlogassert( "select() error: %#m", err ); usleep( 100000 ); continue; }
		
		if( IsValidSocket( sockV4 ) && FD_ISSET( sockV4, &readSet ) ) _NTPClockServerProcessPacket( ntpClock, sockV4 );
		if( IsValidSocket( sockV6 ) && FD_ISSET( sockV6, &readSet ) ) _NTPClockServerProcessPacket( ntpClock, sockV6 );
		if( FD_ISSET( cmdSock, &readSet ) ) break; // The only event is quit so break if anything is pending.
	}
	return( NULL );
}

//===========================================================================================================================
//	_NTPClockServerProcessPacket
//===========================================================================================================================

static void	_NTPClockServerProcessPacket( NTPClockRef inClock, SocketRef inSock )
{
	uint64_t const		ticksPerSec = inClock->ticksPerSec;
	OSStatus			err;
	NTPPacketUnion		pkt;
	size_t				len;
	sockaddr_ip			sip;
	size_t				sipLen;
	Boolean				rtcp = false;
	uint64_t			receiveTicks, sendTicks;
	ssize_t				n;
	uint32_t *			receiveTimeHiPtr;
	uint32_t *			receiveTimeLoPtr;
	uint32_t *			transmitTimeHiPtr;
	uint32_t *			transmitTimeLoPtr;
	uint32_t			hi, lo;
	
	sip.sa.sa_family = AF_UNSPEC;
	err = SocketRecvFrom( inSock, &pkt, sizeof( pkt ), &len, &sip, sizeof( sip ), &sipLen, &receiveTicks, NULL, NULL );
	require_noerr( err, exit );
	
	if( len == sizeof( pkt.ntp ) )
	{
		NTPPacket_SetLeap_Version_Mode( &pkt.ntp, kNTPLeap_NoWarning, NTPPacket_GetVersion( &pkt.ntp ), kNTPMode_Server );
		pkt.ntp.stratum			= 1; // We're the primary reference for this timeline.
		pkt.ntp.precision		= inClock->precision;
		pkt.ntp.rootDelay		= 0; // No delay because we're a primary reference.
		pkt.ntp.rootDispersion	= 0; // No error because we're a primary reference.
		pkt.ntp.referenceID		= htonl( kNTPReferenceID_NTPUtilsV1 );
		pkt.ntp.referenceTimeHi	= 0; // Our clock is only set at boot and it starts at 0.
		pkt.ntp.referenceTimeLo	= 0;
		pkt.ntp.originateTimeHi	= pkt.ntp.transmitTimeHi;
		pkt.ntp.originateTimeLo	= pkt.ntp.transmitTimeLo;
		receiveTimeHiPtr		= &pkt.ntp.receiveTimeHi;
		receiveTimeLoPtr		= &pkt.ntp.receiveTimeLo;
		transmitTimeHiPtr		= &pkt.ntp.transmitTimeHi;
		transmitTimeLoPtr		= &pkt.ntp.transmitTimeLo;
	}
	else if( len == sizeof( pkt.rtcp ) )
	{
		rtcp = true;
		require_action( ( pkt.rtcp.v_p_m >> 6 ) == 2, exit, err = kVersionErr );
		require_action( pkt.rtcp.pt == 210, exit, err = kTypeErr );
		// Note: this doesn't check the length field of the packet because AirFoil sends packets with an incorrect length.
		
		pkt.rtcp.v_p_m			= 2 << 6;
		pkt.rtcp.pt				= 211;
		pkt.rtcp.length			= htons( 7 );
		pkt.rtcp.rtpTimestamp	= 0;
		pkt.rtcp.ntpOriginateHi	= pkt.rtcp.ntpTransmitHi;
		pkt.rtcp.ntpOriginateLo	= pkt.rtcp.ntpTransmitLo;
		receiveTimeHiPtr		= &pkt.rtcp.ntpReceiveHi;
		receiveTimeLoPtr		= &pkt.rtcp.ntpReceiveLo;
		transmitTimeHiPtr		= &pkt.rtcp.ntpTransmitHi;
		transmitTimeLoPtr		= &pkt.rtcp.ntpTransmitLo;
	}
	else
	{
		dlogassert( "Bad packet length: %zu", len );
		err = kSizeErr;
		goto exit;
	}
	hi					= ( (uint32_t)(   receiveTicks / ticksPerSec ) ) + inClock->epochSecs;
	lo					= (uint32_t)( ( ( receiveTicks % ticksPerSec ) << 32 ) / ticksPerSec );
	*receiveTimeHiPtr	= htonl( hi );
	*receiveTimeLoPtr	= htonl( lo );
	hi					= ( (uint32_t)( ( sendTicks = UpTicks() ) / ticksPerSec ) ) + inClock->epochSecs;
	lo					= (uint32_t)( ( ( sendTicks % ticksPerSec ) << 32 ) / ticksPerSec );
	*transmitTimeHiPtr	= htonl( hi );
	*transmitTimeLoPtr	= htonl( lo );
	
	n = sendto( inSock, (const char *) &pkt, len, 0, &sip.sa, (socklen_t) sipLen );
	err = map_socket_value_errno( inSock, n == (ssize_t) len, n );
	require_noerr( err, exit );
	ntp_ulog( inClock, kLogLevelChatty, "NTP server sent %sresponse to %##a\n", rtcp ? "RTCP " : "", &sip );
	
exit:
	if( err ) ntp_ulog( inClock, kLogLevelNotice, "### NTP server send%s to %##a failed: %#m\n", rtcp ? " RTCP" : "", &sip, err );
}
