/*
	File:    	AirTunesClock.c
	Package: 	CarPlay Communications Plug-in.
	Abstract: 	n/a 
	Version: 	280.33.8
	
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
	
	Copyright (C) 2007-2015 Apple Inc. All Rights Reserved.
*/

#include "AirTunesClock.h"

#include <CoreUtils/CommonServices.h>
#include <CoreUtils/DebugServices.h>
#include <CoreUtils/ThreadUtils.h>
#include <CoreUtils/TickUtils.h>

//===========================================================================================================================
//	Private
//===========================================================================================================================

// 64-bit fixed-pointed math (32.32).

typedef int64_t		Fixed64;

#define Fixed64_Add( X, Y )				( ( X ) += ( Y ) )
#define Fixed64_Sub( X, Y )				( ( X ) -= ( Y ) )
#define Fixed64_RightShift( X, N )						\
	do													\
	{													\
		if( ( X ) < 0)	( X ) = -( -( X ) >> ( N ) );	\
		else			( X ) =     ( X ) >> ( N ); 	\
														\
	}	while( 0 )

#define Fixed64_Multiply( X, Y )		( ( X ) *= ( Y ) )
#define Fixed64_Clear( X )				( ( X ) = 0 )
#define Fixed64_GetInteger( X )			( ( ( X ) < 0 ) ? ( -( -( X ) >> 32 ) ) : ( ( X ) >> 32 ) )
#define Fixed64_SetInteger( X, Y )		( ( X ) = ( (int64_t)( Y ) ) << 32 )

// Phase Lock Loop (PLL) constants.

#define kAirTunesClock_MaxPhase			500000000 // Max phase error (nanoseconds).
#define kAirTunesClock_MaxFrequency		   500000 // Max frequence error (nanoseconds per second).
#define kAirTunesClock_PLLShift			        4 // PLL loop gain (bit shift value).

// Prototypes

DEBUG_STATIC void	_AirTunesClock_Tick( AirTunesClockRef inClock );
DEBUG_STATIC void *	_AirTunesClock_Thread( void *inArg );

// Globals

struct AirTunesClockPrivate
{
	AirTunesTime		epochTime;
	AirTunesTime		upTime;
	AirTunesTime		lastTime;
	uint64_t			frequency;
	uint64_t			scale;
	uint32_t			lastCount;
	int64_t				adjustment;
	int32_t				lastOffset;						// Last time offset (nanoseconds).
	int32_t				lastAdjustTime;					// Time at last adjustment (seconds).
	Fixed64				offset;							// Time offset (nanoseconds).
	Fixed64				frequencyOffset;				// Frequency offset (nanoseconds per second).
	Fixed64				tickAdjust;						// Amount to adjust at each tick (nanoseconds per second).
	int32_t				second;							// Current second.
	pthread_t			thread;
	pthread_t *			threadPtr;
	pthread_mutex_t		lock;
	pthread_mutex_t *	lockPtr;
	Boolean				running;
};


//===========================================================================================================================
//	AirTunesClock_Create
//===========================================================================================================================

OSStatus	AirTunesClock_Create( AirTunesClockRef *outRef )
{
	OSStatus			err;
	AirTunesClockRef	obj;
	
	obj = (AirTunesClockRef) calloc( 1, sizeof( *obj ) );
	require_action( obj, exit, err = kNoMemoryErr );
	
	obj->epochTime.secs = 0;
	obj->epochTime.frac = 0;
	
	obj->upTime.secs = 0;
	obj->upTime.frac = 0;
	
	obj->lastTime.secs = 0;
	obj->lastTime.frac = 0;
	
	obj->frequency = UpTicksPerSecond();
	obj->scale = UINT64_C( 0xFFFFFFFFFFFFFFFF ) / obj->frequency;
	
	obj->lastCount		= 0;
	obj->adjustment		= 0;
	obj->lastOffset		= 0;
	obj->lastAdjustTime	= 0;
	
	Fixed64_Clear( obj->offset );
	Fixed64_Clear( obj->frequencyOffset );
	Fixed64_Clear( obj->tickAdjust );
	
	obj->second = 1;
	
	err = pthread_mutex_init( &obj->lock, NULL );
	require_noerr( err, exit );
	obj->lockPtr = &obj->lock;
	
	// Signal to Terminate the thread.
	obj->running = true;
	err = pthread_create( &obj->thread, NULL, _AirTunesClock_Thread, obj );
	require_noerr( err, exit );
	obj->threadPtr = &obj->thread;

	*outRef = obj;
	obj = NULL;
	err = kNoErr;
	
exit:
	if ( obj )
		AirTunesClock_Finalize( obj );
	return( err );
}

//===========================================================================================================================
//	AirTunesClock_Finalize
//===========================================================================================================================

OSStatus	AirTunesClock_Finalize( AirTunesClockRef inClock )
{
	OSStatus		err;
	
	DEBUG_USE_ONLY( err );

	if( inClock )
	{
		inClock->running = false;
		if( inClock->threadPtr )
		{
			err = pthread_join( inClock->thread, NULL );
			check_noerr( err );
			inClock->threadPtr = NULL;
		}
		
		if( inClock->lockPtr )
		{
			err = pthread_mutex_destroy( inClock->lockPtr );
			check_noerr( err );
			inClock->lockPtr = NULL;
		}
		free( inClock );
	}
	return( kNoErr );
}

//===========================================================================================================================
//	AirTunesClock_Adjust
//===========================================================================================================================

Boolean	AirTunesClock_Adjust( AirTunesClockRef inClock, int64_t inOffsetNanoseconds, Boolean inReset )
{
	if( inReset || ( ( inOffsetNanoseconds < -100000000 ) || ( inOffsetNanoseconds > 100000000 ) ) )
	{
		AirTunesTime		at;
		uint64_t			offset;
		
		pthread_mutex_lock( inClock->lockPtr );
		if( inOffsetNanoseconds < 0 )
		{
			offset  = (uint64_t) -inOffsetNanoseconds;
			at.secs = (int32_t)( offset / 1000000000 );
			at.frac = ( offset % 1000000000 ) * ( UINT64_C( 0xFFFFFFFFFFFFFFFF ) / 1000000000 );
			AirTunesTime_Sub( &inClock->epochTime, &at );
		}
		else
		{
			offset  = (uint64_t) inOffsetNanoseconds;
			at.secs = (int32_t)( offset / 1000000000 );
			at.frac = ( offset % 1000000000 ) * ( UINT64_C( 0xFFFFFFFFFFFFFFFF ) / 1000000000 );
			AirTunesTime_Add( &inClock->epochTime, &at );
		}
		pthread_mutex_unlock( inClock->lockPtr );
		
		_AirTunesClock_Tick( inClock );
		inReset = true;
	}
	else
	{
		int32_t		offset;
		int32_t		mtemp;
		Fixed64		ftemp;
		
		pthread_mutex_lock( inClock->lockPtr );
		
		// Use a phase-lock loop (PLL) to update the time and frequency offset estimates.
		
		offset = (int32_t) inOffsetNanoseconds;
		if(      offset >  kAirTunesClock_MaxPhase )	inClock->lastOffset = kAirTunesClock_MaxPhase;
		else if( offset < -kAirTunesClock_MaxPhase )	inClock->lastOffset = -kAirTunesClock_MaxPhase;
		else											inClock->lastOffset = offset;
		Fixed64_SetInteger( inClock->offset, inClock->lastOffset );
		
		if( inClock->lastAdjustTime == 0 )
		{
			inClock->lastAdjustTime = inClock->second;
		}
		mtemp = inClock->second - inClock->lastAdjustTime;
		Fixed64_SetInteger( ftemp, inClock->lastOffset );
		Fixed64_RightShift( ftemp, ( kAirTunesClock_PLLShift + 2 ) << 1 );
		Fixed64_Multiply( ftemp, mtemp );
		Fixed64_Add( inClock->frequencyOffset, ftemp );
		inClock->lastAdjustTime = inClock->second;
		if( Fixed64_GetInteger( inClock->frequencyOffset ) > kAirTunesClock_MaxFrequency )
		{
			Fixed64_SetInteger( inClock->frequencyOffset, kAirTunesClock_MaxFrequency );
		}
		else if( Fixed64_GetInteger( inClock->frequencyOffset ) < -kAirTunesClock_MaxFrequency )
		{
			Fixed64_SetInteger( inClock->frequencyOffset, -kAirTunesClock_MaxFrequency );
		}
		
		pthread_mutex_unlock( inClock->lockPtr );
	}
	return( inReset );
}

//===========================================================================================================================
//	AirTunesClock_GetSynchronizedNTPTime
//===========================================================================================================================

uint64_t	AirTunesClock_GetSynchronizedNTPTime( AirTunesClockRef inClock )
{
	AirTunesTime		t;
	
	AirTunesClock_GetSynchronizedTime( inClock, &t );
	return( AirTunesTime_ToNTP( &t ) );
}

//===========================================================================================================================
//	AirTunesClock_GetSynchronizedTime
//===========================================================================================================================

void	AirTunesClock_GetSynchronizedTime( AirTunesClockRef inClock, AirTunesTime *outTime )
{
	pthread_mutex_lock( inClock->lockPtr );
		*outTime = inClock->upTime;
		AirTunesTime_AddFrac( outTime, inClock->scale * ( UpTicks32() - inClock->lastCount ) );
		AirTunesTime_Add( outTime, &inClock->epochTime );
	pthread_mutex_unlock( inClock->lockPtr );
}

//===========================================================================================================================
//	AirTunesClock_GetSynchronizedTimeNearUpTicks
//===========================================================================================================================

void	AirTunesClock_GetSynchronizedTimeNearUpTicks( AirTunesClockRef inClock, AirTunesTime *outTime, uint64_t inTicks )
{
	uint64_t			nowTicks;
	uint32_t			nowTicks32;
	uint64_t			deltaTicks;
	AirTunesTime		deltaTime;
	uint64_t			scale;
	Boolean				future;
	
	pthread_mutex_lock( inClock->lockPtr );
		nowTicks	= UpTicks();
		nowTicks32	= ( (uint32_t)( nowTicks & UINT32_C( 0xFFFFFFFF ) ) );
		future		= ( inTicks > nowTicks );
		deltaTicks	= future ? ( inTicks - nowTicks ) : ( nowTicks - inTicks );
		
		*outTime = inClock->upTime;
		AirTunesTime_AddFrac( outTime, inClock->scale * ( nowTicks32 - inClock->lastCount ) );
		AirTunesTime_Add( outTime, &inClock->epochTime );
		scale = inClock->scale;
	pthread_mutex_unlock( inClock->lockPtr );
	
	deltaTime.secs = (int32_t)( deltaTicks / inClock->frequency ); // Note: unscaled, but delta expected to be < 1 sec.
	deltaTime.frac = ( deltaTicks % inClock->frequency ) * scale;
	if( future )	AirTunesTime_Add( outTime, &deltaTime );
	else			AirTunesTime_Sub( outTime, &deltaTime );
}

//===========================================================================================================================
//	AirTunesClock_GetUpTicksNearSynchronizedNTPTime
//===========================================================================================================================

uint64_t	AirTunesClock_GetUpTicksNearSynchronizedNTPTime( AirTunesClockRef inClock, uint64_t inNTPTime )
{
	uint64_t		nowNTP;
	uint64_t		ticks;
	
	nowNTP = AirTunesClock_GetSynchronizedNTPTime( inClock );
	if( inNTPTime >= nowNTP )	ticks = UpTicks() + NTPtoUpTicks( inNTPTime - nowNTP );
	else						ticks = UpTicks() - NTPtoUpTicks( nowNTP    - inNTPTime );
	return( ticks );
}

//===========================================================================================================================
//	AirTunesClock_GetUpTicksNearSynchronizedNTPTimeMid32
//===========================================================================================================================

uint64_t	AirTunesClock_GetUpTicksNearSynchronizedNTPTimeMid32( AirTunesClockRef inClock, uint32_t inNTPMid32 )
{
	uint64_t		ntpA, ntpB;
	uint64_t		ticks;
	
	ntpA = AirTunesClock_GetSynchronizedNTPTime( inClock );
	ntpB = ( ntpA & UINT64_C( 0xFFFF000000000000 ) ) | ( ( (uint64_t) inNTPMid32 ) << 16 );
	if( ntpB >= ntpA )	ticks = UpTicks() + NTPtoUpTicks( ntpB - ntpA );
	else				ticks = UpTicks() - NTPtoUpTicks( ntpA - ntpB );
	return( ticks );
}

//===========================================================================================================================
//	_AirTunesClock_Tick
//===========================================================================================================================

DEBUG_STATIC void	_AirTunesClock_Tick( AirTunesClockRef inClock )
{
	uint32_t			count;
	uint32_t			delta;
	AirTunesTime		at;
	int					recalc;
	
	pthread_mutex_lock( inClock->lockPtr );
	
	// Update the current uptime from the delta between now and the last update.
	
	count = UpTicks32();
	delta = count - inClock->lastCount;
	inClock->lastCount = count;
	AirTunesTime_AddFrac( &inClock->upTime, inClock->scale * delta );
	
	// Perform NTP adjustments each second.
	
	at = inClock->upTime;
	AirTunesTime_Add( &at, &inClock->epochTime );
	recalc = 0;
	if( at.secs > inClock->lastTime.secs )
	{
		Fixed64		ftemp;
		
		ftemp = inClock->offset;
		Fixed64_RightShift( ftemp, kAirTunesClock_PLLShift );
		inClock->tickAdjust = ftemp;
		Fixed64_Sub( inClock->offset, ftemp );
		Fixed64_Add( inClock->tickAdjust, inClock->frequencyOffset );
		
		inClock->adjustment = inClock->tickAdjust;
		recalc = 1;
	}
	
	// Recalculate the scaling factor. We want the number of 1/2^64 fractions of a second per period of 
	// the hardware counter, taking into account the adjustment factor which the NTP PLL processing 
	// provides us with. The adjustment is nanoseconds per second with 32 bit binary fraction and we want 
	// 64 bit binary fraction of second:
	//
	//		x = a * 2^32 / 10^9 = a * 4.294967296
	//
	// The range of adjustment is +/- 5000PPM so inside a 64 bit integer we can only multiply by about 
	// 850 without overflowing, that leaves no suitably precise fractions for multiply before divide.
	// Divide before multiply with a fraction of 2199/512 results in a systematic undercompensation of 
	// 10PPM. On a 5000 PPM adjustment this is a 0.05PPM error. This is acceptable.
	
	if( recalc )
	{
		uint64_t		scale;
		
		scale  = UINT64_C( 1 ) << 63;
		scale += ( inClock->adjustment / 1024 ) * 2199;
		scale /= inClock->frequency;
		inClock->scale = scale * 2;
	}
	
	inClock->second = at.secs;
	inClock->lastTime = at;
	
	pthread_mutex_unlock( inClock->lockPtr );
}

//===========================================================================================================================
//	_AirTunesClock_Thread
//===========================================================================================================================

DEBUG_STATIC void *	_AirTunesClock_Thread( void *inArg )
{
	AirTunesClockRef const me = (AirTunesClockRef) inArg;
	
	pthread_setname_np_compat( "AirPlayClock" );
	
	while( me->running )
	{
		_AirTunesClock_Tick( me );
		usleep( 10000 );
	}
	return( NULL );
}
