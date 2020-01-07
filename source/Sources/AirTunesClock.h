/*
	File:    	AirTunesClock.h
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
	
	Copyright (C) 2007-2014 Apple Inc. All Rights Reserved.
*/

#ifndef	__AirTunesClock_h__
#define	__AirTunesClock_h__

#include <CoreUtils/CommonServices.h>
#include <CoreUtils/DebugServices.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == API ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		AirTunesClockRef
	@abstract	Private structure representing internals for AirTunesClock.
*/
typedef struct AirTunesClockPrivate *		AirTunesClockRef;
	
//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		AirTunesTime
	@abstract	Structure representing time for AirTunes.
*/

typedef struct
{
	int32_t		secs; //! Number of seconds since 1970-01-01 00:00:00 (Unix time).
	uint64_t	frac; //! Fraction of a second in units of 1/2^64.

}	AirTunesTime;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_Create
	@abstract	Create an new instance of the clock/timing engine.
*/

OSStatus	AirTunesClock_Create( AirTunesClockRef *outRef );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_Finalize
	@abstract	Finalizes the clock/timing engine.
*/

OSStatus	AirTunesClock_Finalize( AirTunesClockRef inClock );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_Adjust
	@abstract	Starts the clock adjustment and discipline process based on the specified clock offset.
	
	@result		true if the clock was stepped. false if slewing.
*/

Boolean	AirTunesClock_Adjust( AirTunesClockRef inClock, int64_t inOffsetNanoseconds, Boolean inReset );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_GetSynchronizedNTPTime
	@abstract	Gets the current time, synchronized to the source clock as an NTP timestamp.
*/

uint64_t	AirTunesClock_GetSynchronizedNTPTime( AirTunesClockRef inClock );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_GetSynchronizedTime
	@abstract	Gets the current time, synchronized to the source clock.
*/

void	AirTunesClock_GetSynchronizedTime( AirTunesClockRef inClock, AirTunesTime *outTime );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_GetSynchronizedTimeNearUpTicks
	@abstract	Gets an estimate of the synchronized time near the specified UpTicks.
*/

void	AirTunesClock_GetSynchronizedTimeNearUpTicks( AirTunesClockRef inClock, AirTunesTime *outTime, uint64_t inTicks );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_GetUpTicksNearSynchronizedNTPTime
	@abstract	Gets an estimate of the local UpTicks() near an NTP timestamp on the synchronized timeline.
*/

uint64_t	AirTunesClock_GetUpTicksNearSynchronizedNTPTime( AirTunesClockRef inClock, uint64_t inNTPTime );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_GetUpTicksNearSynchronizedNTPTimeMid32
	@abstract	Gets an estimate of the local UpTicks() near the middle 32 bits of an NTP timestamp on the synchronized timeline.
*/

uint64_t	AirTunesClock_GetUpTicksNearSynchronizedNTPTimeMid32( AirTunesClockRef inClock, uint32_t inNTPMid32 );

#if 0
#pragma mark == Utils ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesTime_AddFrac
	@abstract	Adds a fractional seconds (1/2^64 units) value to a time.
*/

STATIC_INLINE void	AirTunesTime_AddFrac( AirTunesTime *inTime, uint64_t inFrac )
{
	uint64_t		frac;
	
	frac = inTime->frac;
	inTime->frac = frac + inFrac;
	if( frac > inTime->frac ) inTime->secs += 1; // Increment seconds on fraction wrap.
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesTime_Add
	@abstract	Adds one time to another time.
*/

STATIC_INLINE void	AirTunesTime_Add( AirTunesTime *inTime, const AirTunesTime *inTimeToAdd )
{
	uint64_t		frac;
	
	frac = inTime->frac;
	inTime->frac = frac + inTimeToAdd->frac;
	if( frac > inTime->frac ) inTime->secs += 1; // Increment seconds on fraction wrap.
	inTime->secs += inTimeToAdd->secs;
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesTime_Sub
	@abstract	Subtracts one time from another time.
*/

STATIC_INLINE void	AirTunesTime_Sub( AirTunesTime *inTime, const AirTunesTime *inTimeToSub )
{
	uint64_t	frac;
	
	frac = inTime->frac;
	inTime->frac = frac - inTimeToSub->frac;
	if( frac < inTime->frac ) inTime->secs -= 1; // Decrement seconds on fraction wrap.
	inTime->secs -= inTimeToSub->secs;
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesTime_ToFP
	@abstract	Converts an AirTunesTime to a floating-point seconds value.
*/

STATIC_INLINE double	AirTunesTime_ToFP( const AirTunesTime *inTime )
{
	return( ( (double) inTime->secs ) + ( ( (double) inTime->frac ) * ( 1.0 / 18446744073709551615.0 ) ) );
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesTime_FromFP
	@abstract	Converts a floating-point seconds value to an AirTunesTime.
*/

STATIC_INLINE void	AirTunesTime_FromFP( AirTunesTime *inTime, double inFP )
{
	double		secs;
	double		frac;
	
	secs = floor( inFP );
	frac = inFP - secs;
	frac *= 18446744073709551615.0;
	
	inTime->secs = (int32_t) secs;
	inTime->frac = (uint64_t) frac;
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesTime_ToNanoseconds
	@abstract	Converts an AirTunesTime to nanoseconds.
*/

STATIC_INLINE uint64_t	AirTunesTime_ToNanoseconds( const AirTunesTime *inTime )
{
	uint64_t		ns;
	
	ns  = ( (uint64_t) inTime->secs ) * 1000000000;
	ns += ( UINT64_C( 1000000000 ) * ( (uint32_t)( inTime->frac >> 32 ) ) ) >> 32;
	return( ns );
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesTime_ToNTP
	@abstract	Converts an AirTunesTime to a 64-bit NTP timestamp.
*/

#define AirTunesTime_ToNTP( AT )		( ( ( (uint64_t) (AT)->secs ) << 32 ) | ( (AT)->frac >> 32 ) )

#ifdef __cplusplus
}
#endif

#endif	// __AirTunesClock_h__
