/*
	File:    	NTPUtils.h
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
	
	Copyright (C) 2011-2014 Apple Inc. All Rights Reserved.
*/

#ifndef __NTPUtils_h__
#define __NTPUtils_h__

#include "CommonServices.h"
#include "DebugServices.h"

#include CF_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == NTPPacket ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		NTPPacket
	@abstract	Packet for NTP exchanges.
	@discussion
	                    1                    2                   3
	 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +0/0x00
	|LI | VN  |Mode |    Stratum    |     Poll      |   Precision   |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +4/0x04
	|                          Root Delay                           |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +8/0x08
	|                       Root Dispersion                         |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +12/0x0C
	|                     Reference Identifier                      |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +16/0x10
	|                                                               |
	|                   Reference Timestamp (64)                    |
	|                                                               |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +24/0x18
	|                                                               |
	|                   Originate Timestamp (64)                    |
	|                                                               |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +32/0x20
	|                                                               |
	|                    Receive Timestamp (64)                     |
	|                                                               |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +40/0x28
	|                                                               |
	|                    Transmit Timestamp (64)                    |
	|                                                               |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +48/0x30
*/    
typedef struct
{
	uint8_t			li_vn_mode;			// Leap Indicator (LI), Version (VN), and Mode.
	uint8_t			stratum;			// Stratum of local clock.
	int8_t			poll;				// 2^n poll interval in seconds.
	int8_t			precision;			// 2^n precision in seconds.
	int32_t			rootDelay;			// Q16.16 RTT to primary reference source.
	uint32_t		rootDispersion;		// Q16.16 nominal error relative to primary reference source.
	uint32_t		referenceID;		// Identifier of the reference source.
	
	uint32_t		referenceTimeHi;	// Time when the local clock was last set/corrected.
	uint32_t		referenceTimeLo;
	uint32_t		originateTimeHi;	// [T1] Time when request was sent from client to server.
	uint32_t		originateTimeLo;
	uint32_t		receiveTimeHi;		// [T2] Time when request arrived at the server.
	uint32_t		receiveTimeLo;
	uint32_t		transmitTimeHi;		// [T3] Time when reply was sent from server to the client.
	uint32_t		transmitTimeLo;
	
}	NTPPacket;

check_compile_time( offsetof( NTPPacket, li_vn_mode )		==  0 );
check_compile_time( offsetof( NTPPacket, stratum )			==  1 );
check_compile_time( offsetof( NTPPacket, poll )				==  2 );
check_compile_time( offsetof( NTPPacket, precision )		==  3 );
check_compile_time( offsetof( NTPPacket, rootDelay )		==  4 );
check_compile_time( offsetof( NTPPacket, rootDispersion )	==  8 );
check_compile_time( offsetof( NTPPacket, referenceID )		== 12 );
check_compile_time( offsetof( NTPPacket, referenceTimeHi )	== 16 );
check_compile_time( offsetof( NTPPacket, referenceTimeLo )	== 20 );
check_compile_time( offsetof( NTPPacket, originateTimeHi )	== 24 );
check_compile_time( offsetof( NTPPacket, originateTimeLo )	== 28 );
check_compile_time( offsetof( NTPPacket, receiveTimeHi )	== 32 );
check_compile_time( offsetof( NTPPacket, receiveTimeLo )	== 36 );
check_compile_time( offsetof( NTPPacket, transmitTimeHi )	== 40 );
check_compile_time( offsetof( NTPPacket, transmitTimeLo )	== 44 );
check_compile_time( sizeof( NTPPacket ) 					== 48 );

#define kNTPLeap_NoWarning					0
#define kNTPLeap_61Seconds					1
#define kNTPLeap_59Seconds					2
#define kNTPLeap_Alarm						3
#define NTPPacket_GetLeap( PKT )			( ( (PKT)->li_vn_mode >> 6 ) & 3 )
#define NTPPacket_SetLeap( PKT, LI )		(PKT)->li_vn_mode = (uint8_t)( ( (PKT)->li_vn_mode & 0x3F ) | ( (LI) << 6 ) )

#define kNTPVersion3						3
#define kNTPVersion4						4
#define NTPPacket_GetVersion( PKT )			( ( (PKT)->li_vn_mode >> 3 ) & 7 )
#define NTPPacket_SetVersion( PKT, VN )		(PKT)->li_vn_mode = (uint8_t)( ( (PKT)->li_vn_mode & 0xC7 ) | ( (VN) << 3 ) )

#define kNTPMode_SymmetricActive			1
#define kNTPMode_SymmetricPassive			2
#define kNTPMode_Client						3
#define kNTPMode_Server						4
#define kNTPMode_Broadcast					5
#define NTPPacket_GetMode( PKT )			( (PKT)->li_vn_mode & 7 )
#define NTPPacket_SetMode( PKT, MODE )		(PKT)->li_vn_mode = (uint8_t)( ( (PKT)->li_vn_mode & 0xF8 ) | (MODE) )

#define NTPPacket_SetLeap_Version_Mode( PKT, LI, VN, MODE )	\
	(PKT)->li_vn_mode = (uint8_t)( ( (LI) << 6 ) | ( (VN) << 3 ) | (MODE) )

#define kNTPReferenceID_NTPUtilsV1			0x584E5531 // 'XNU1'
#define kNTPReferenceID_Ignore				0x5849474E // 'XIGN'

#if 0
#pragma mark -
#pragma mark == NTPoverRTCPPacket ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		NTPoverRTCPPacket
	@abstract	Packet for NTP exchanges encapulated in RTCP.
	@discussion
	
			0                   1                   2                   3
			0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +0/0x00
	header |V=2|P|M|   0   |      PT       |           length              |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +4/0x04
	timing |           RTP timestamp at NTP Transit (T3) time              |
	info   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +8/0x08
		   |      NTP Originate (T1) timestamp, most  significant word     |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +12/0x0C
		   |      NTP Originate (T1) timestamp, least significant word     |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +16/0x10
		   |      NTP Receive   (T2) timestamp, most  significant word     |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +20/0x14
		   |      NTP Receive   (T2) timestamp, least significant word     |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +24/0x18
		   |      NTP Transmit  (T3) timestamp, most  significant word     |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +28/0x1C
		   |      NTP Transmit  (T3) timestamp, least significant word     |
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ +32/0x20
*/
typedef struct
{
	uint8_t			v_p_m;			// Version (V), Padding (P), and Marker (M) fields.	
	uint8_t			pt;				// RTCP packet type.
	uint16_t		length;			// Packet length in 32-bit words - 1.
	
	uint32_t		rtpTimestamp;	// RTP timestamp at the same instant as ntpTransmitHi/Lo (clients should use 0).
	uint32_t		ntpOriginateHi;	// Transmit time from the original client request (clients should use 0).
	uint32_t		ntpOriginateLo;
	uint32_t		ntpReceiveHi;	// Time request received by the server (clients should use 0).
	uint32_t		ntpReceiveLo;
	uint32_t		ntpTransmitHi;	// Time client request or server response sent.
	uint32_t		ntpTransmitLo;
	
}	NTPoverRTCPPacket;

check_compile_time( offsetof( NTPoverRTCPPacket, pt )				==  1 );
check_compile_time( offsetof( NTPoverRTCPPacket, length ) 			==  2 );
check_compile_time( offsetof( NTPoverRTCPPacket, rtpTimestamp )		==  4 );
check_compile_time( offsetof( NTPoverRTCPPacket, ntpOriginateHi )	==  8 );
check_compile_time( offsetof( NTPoverRTCPPacket, ntpOriginateLo )	== 12 );
check_compile_time( offsetof( NTPoverRTCPPacket, ntpReceiveHi )		== 16 );
check_compile_time( offsetof( NTPoverRTCPPacket, ntpReceiveLo )		== 20 );
check_compile_time( offsetof( NTPoverRTCPPacket, ntpTransmitHi )	== 24 );
check_compile_time( offsetof( NTPoverRTCPPacket, ntpTransmitLo )	== 28 );
check_compile_time( sizeof( NTPoverRTCPPacket ) 					== 32 );

// Union of all NTP-ish packets.

typedef union
{
	NTPPacket				ntp;
	NTPoverRTCPPacket		rtcp;
	
}	NTPPacketUnion;

#if 0
#pragma mark -
#pragma mark == Conversions ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		NTP Conversions
	@abstract	Constants and macros to convert between NTP and other time units.
*/
#define kNTPUnits_FP					( 1.0 / 4294967296.0 )
#define kNTPFractionalUnit_FP			( 1.0 / 4294967296.0 )

#define NTPtoSeconds( NTP )				(   (uint32_t)( (NTP)    >> 32 ) )
#define	NTPtoSeconds_FP( NTP )			( ( (double)(NTP) ) * kNTPUnits_FP )
#define SecondsToNTP( SECS )			( ( (uint64_t)(SECS) ) << 32 )
#define SecondsToNTP_FP( SECS )			( (uint64_t)( (SECS) * 4294967296 ) )

#define NTPtoMilliseconds( NTP )		( ( 1000 * (NTP) ) >> 32 )
#define MillisecondsToNTP( MS )			( ( (MS) * UINT64_C( 0x100000000 ) ) / 1000 )

#define NTPtoMicroseconds( NTP )		( ( 1000000 * (NTP) ) >> 32 )
#define MicrosecondsToNTP( MS )			( ( (MS) * UINT64_C( 0x100000000 ) ) / 1000000 )

#define NTPFractionToMicroseconds( NTP )	\
	( (uint32_t)( ( 1000000 * ( (NTP) & UINT32_C( 0xFFFFFFFF ) ) ) / UINT32_C( 0xFFFFFFFF ) ) )

#define NTPFractionToMilliseconds( NTP )	\
	( (uint32_t)( ( 1000 * ( (NTP) & UINT32_C( 0xFFFFFFFF ) ) ) / UINT32_C( 0xFFFFFFFF ) ) )

#define NTPCalculateOffsetSeconds_FP( T1, T2, T3, T4 ) ( \
	( ( ( (double)( (int64_t)( (T2) - (T1) ) ) ) * kNTPUnits_FP ) + \
	  ( ( (double)( (int64_t)( (T3) - (T4) ) ) ) * kNTPUnits_FP ) ) / 2 )

#define NTPCalculateRTTSeconds_FP( T1, T2, T3, T4 ) ( \
	( ( (double)( (int64_t)( (T4) - (T1) ) ) ) * kNTPUnits_FP ) - \
	( ( (double)( (int64_t)( (T3) - (T2) ) ) ) * kNTPUnits_FP ) )

#if 0
#pragma mark -
#pragma mark == NTPClock ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPClockCreate
	@abstract	Creates an object to manage an NTP clock.
*/
typedef struct NTPClockPrivate *		NTPClockRef;

CFTypeID	NTPClockGetTypeID( void );
OSStatus	NTPClockCreate( NTPClockRef *outListener );
#define		NTPClockForget( X )	do { if( *(X) ) { NTPClockStop( *(X) ); CFRelease( *(X) ); *(X) = NULL; } } while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPClockStartClient / NTPClockStartServer / NTPClockStop
	@abstract	Starts/stops NTP client/server.
*/
OSStatus	NTPClockStartClient( NTPClockRef inClock );
OSStatus	NTPClockStartServer( NTPClockRef inClock );
void		NTPClockStop( NTPClockRef inClock );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPClockSetEpoch
	@abstract	Sets the epoch offset in seconds from the NTP epoch (1900-01-01).
	@discussion	Some uses, such as older AirPlay, use the NTP -> Unix epoch (1900-1970) so this adjusts for that.
*/
void	NTPClockSetEpoch( NTPClockRef inClock, uint32_t inEpochSecs );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPClockSetLogging
	@abstract	Sets the log category to use for all logging instead of the default one.
*/
void	NTPClockSetLogging( NTPClockRef inClock, LogCategory *inCategory );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPClockSetP2P
	@abstract	Enables operation over P2P interfaces.
*/
void	NTPClockSetP2P( NTPClockRef inClock, Boolean inP2PAllowed );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPClockSetPeer
	@abstract	Sets the peer address/port (i.e. remote NTP server).
*/
void	NTPClockSetPeer( NTPClockRef inClock, const void *inSockAddr, int inDefaultPort );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPClockGetListenPort / NTPClockSetListenPort
	@abstract	Gets the port it's listening on for requests (in server mode) or responses (in client mode).
				Sets the preferred port to listen on for requests (in server mode) or responses (in client mode).
*/
int		NTPClockGetListenPort( NTPClockRef inClock );
void	NTPClockSetListenPort( NTPClockRef inClock, int inPort );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPClockSetQoSDisabled
	@abstract	Disables use of QoS.
*/
void	NTPClockSetQoSDisabled( NTPClockRef inClock, Boolean inQoSDisabled );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPClockSetRTCP
	@abstract	Set whether to send NTP-over-RTCP request packets (only needed on client...server will auto-detect).
*/
void	NTPClockSetRTCP( NTPClockRef inClock, Boolean inUseRTCP );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPClockGetStats
	@abstract	Gets statistics about the NTP session.
*/
typedef struct
{
	uint32_t		sendErrors;		// Errors sending an NTP request (client) or response (server).
	uint32_t		receiveErrors;	// Errors receiving an NTP response.
	uint32_t		timeouts;		// Timeouts waiting for an NTP response.
	uint32_t		unexpected;		// Unexpected packets received (e.g. duplicate, too old, or a misdirected packet).
	
}	NTPStats;

void	NTPClockGetStats( NTPClockRef inClock, NTPStats *outStats );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPClockSetThreadName / NTPClockSetThreadPriority
	@abstract	Sets the name/priority of threads created by the clock.
*/
OSStatus	NTPClockSetThreadName( NTPClockRef inClock, const char *inName );
void		NTPClockSetThreadPriority( NTPClockRef inClock, int inPriority );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPClockGetSynchronizedNTPTime
	@abstract	Gets the current time, synchronized to the remote clock, in NTP units.
*/
uint64_t	NTPClockGetSynchronizedNTPTime( NTPClockRef inClock );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPClockGetSynchronizedNTPTimeNearUpTicks
	@abstract	Gets an estimate of the NTP time, synchronized to the remote clock, near the specified UpTicks.
*/
uint64_t	NTPClockGetSynchronizedNTPTimeNearUpTicks( NTPClockRef inClock, uint64_t inTicks );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPClockGetUpTicksNearSynchronizedNTPTime
	@abstract	Gets an estimate of the UpTicks near the specified synchronized NTP time.
*/
uint64_t	NTPClockGetUpTicksNearSynchronizedNTPTime( NTPClockRef inClock, uint64_t inNTP );

#ifdef __cplusplus
}
#endif

#endif // __NTPUtils_h__
