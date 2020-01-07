/*
	File:    	TickUtils.h
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
	
	Copyright (C) 2001-2015 Apple Inc. All Rights Reserved.
*/
/*!
    @header		Tick API
    @discussion APIs for providing a high-resolution, low-latency tick counter and conversions.
*/

#ifndef __TickUtils_h__
#define	__TickUtils_h__

#if( defined( TickUtils_PLATFORM_HEADER ) )
	#include  TickUtils_PLATFORM_HEADER
#endif

#include "CommonServices.h"	// Include early for TARGET_* conditionals.
#include "DebugServices.h"	// Include early for DEBUG_* conditionals.

#if( TARGET_HAS_STD_C_LIB )
	#include <time.h>
#endif

#if( TARGET_MACH )
	#include <mach/mach_time.h>
#endif

#if( TARGET_OS_POSIX )
	#include <sys/time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Tick counter access.
	@abstract	Provides access to the raw tick counter, ticks per second, and convenience functions for common units.
	@discussion
	
	Primitive functions:
	
	uint64_t	UpTicks( void );			// Monotonically increasing number of ticks since the system started.
	uint64_t	UpTicksPerSecond( void );	// Number of ticks per second.
	
	If your platform is not already supported then you can implement UpTicks() and UpTicksPerSecond() in your own file
	and link it in. Alternatively, if you have an existing API and want to avoid the overhead of wrapping your function 
	then you can define TickUtils_PLATFORM_HEADER to point to your custom header file and inside that file you can 
	define UpTicks() and UpTicksPerSecond() to point to your existing functions. This assumes they are API compatible.
	
	Convenience functions if a platform has a more efficient way of provide time in standard units:
	
	uint64_t	UpSeconds( void );			// Monotonically increasing number of seconds since the system started.
	uint64_t	UpMilliseconds( void );		// Monotonically increasing number of milliseconds since the system started.
	uint64_t	UpMicroseconds( void );		// Monotonically increasing number of microseconds since the system started.
	uint64_t	UpNanoseconds( void );		// Monotonically increasing number of nanoseconds since the system started.
	
	If the platform provides versions of the convenience functions then TickUtils_CONVERSION_OVERRIDES is defined to 1.
	If the platform doesn't provide these functions, built-in conversions are provided using the primitive functions.
	
	Implementors of these APIs must be careful to avoid temporary integer overflows. Even with 64-bit values, it's 
	easy to exceed the range of a 64-bit value when conversion to/from very small units or very large counts.
*/
#if  ( TARGET_OS_FREEBSD || TARGET_OS_LINUX || TARGET_OS_QNX )
	uint64_t	UpTicks( void );
	#define 	UpTicksPerSecond()					UINT64_C_safe( kNanosecondsPerSecond )
	
	#define		UpSeconds()							( UpTicks() / kNanosecondsPerSecond )
	#define		UpMilliseconds()					( UpTicks() / kNanosecondsPerMillisecond )
	#define		UpMicroseconds()					( UpTicks() / kNanosecondsPerMicrosecond )
	#define		UpNanoseconds()						UpTicks()
	#define		TickUtils_CONVERSION_OVERRIDES		1
#elif( TARGET_MACH )
	#define		UpTicks()							mach_absolute_time()
	uint64_t	UpTicksPerSecond( void );
#elif( TARGET_OS_NETBSD )
	#define		UpTicks()							upticks()
	#if( TARGET_KERNEL )
		#define	UpTicksPerSecond()					upticksrate
	#else
		uint64_t	UpTicksPerSecond( void );
	#endif
#elif( TARGET_OS_THREADX )
	#define		UpTicks()							( (uint64_t) tx_time_get() )
	#define 	UpTicksPerSecond()					UINT64_C_safe( kMillisecondsPerSecond )
	
	#define		UpSeconds()							( UpTicks() / kMillisecondsPerSecond )
	#define		UpMilliseconds()					UpTicks()
	#define		UpMicroseconds()					( UpTicks() * kMicrosecondsPerMillisecond )
	#define		UpNanoseconds()						( UpTicks() * kNanosecondsPerMillisecond )
	#define		TickUtils_CONVERSION_OVERRIDES		1
#elif( TARGET_OS_WINDOWS )
	uint64_t	UpTicks( void );
	uint64_t	UpTicksPerSecond( void );
#else
	#if( !defined( UpTicks ) )
		uint64_t	UpTicks( void );
	#endif
	#if( !defined( UpTicksPerSecond ) )
		uint64_t	UpTicksPerSecond( void );
	#endif
#endif

#define UpTicks32()					( (uint32_t)( UpTicks() & UINT32_C( 0xFFFFFFFF ) ) )

#if( !defined( TickUtils_CONVERSION_OVERRIDES ) )
	#define TickUtils_CONVERSION_OVERRIDES		0
#endif
#if( !TickUtils_CONVERSION_OVERRIDES )
	#define UpSeconds()				UpTicksToSeconds( UpTicks() )
	#define UpMilliseconds()		UpTicksToMilliseconds( UpTicks() )
	#define UpMicroseconds()		UpTicksToMicroseconds( UpTicks() )
	#define UpNanoseconds()			UpTicksToNanoseconds( UpTicks() )
#endif

#define UpNTP()		UpTicksToNTP( UpTicks() )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Sleep APIs
	@abstract	APIs for blocking the current thread with minimal CPU usage while sleeping.
	@discussion
	
	SleepForUpTicks( uint32_t inTicks );	// Sleep for the specified number of ticks.
	SleepUntilUpTicks( uint32_t inTicks );	// Sleep until the specified tick deadline has been reached.
	
	If the platform provides the sleep functions then TickUtils_HAS_SLEEP_PRIMITIVE is defined to 1.
	
	Implementors should be careful to avoid premature returns on signals or for other interruptions.
	For example, the POSIX nanosleep returns prematurely with EINT on signals and must be repeated for the remaining time.
*/
#if( TARGET_OS_POSIX || TARGET_OS_WINDOWS )
	void	SleepForUpTicks( uint64_t inTicks );
	void	SleepUntilUpTicks( uint64_t inTicks );
	#define TickUtils_HAS_SLEEP_PRIMITIVE		1
#elif( TARGET_OS_THREADX )
	#define SleepForUpTicks( TICKS )	tx_thread_sleep( (ULONG)(TICKS) )
	void	SleepUntilUpTicks( uint64_t inTicks );
	#define TickUtils_HAS_SLEEP_PRIMITIVE		1
#endif

#define kUpTicksNow			UINT64_C( 0 )
#define kUpTicksForever		UINT64_C( 0xFFFFFFFFFFFFFFFF )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		UpTicks Conversions
	@abstract	Conversions between UpTicks and other units.
*/
uint64_t	UpTicksToSeconds( uint64_t inTicks );
double		UpTicksToSecondsF( uint64_t inTicks );
uint64_t	UpTicksToMilliseconds( uint64_t inTicks );
uint64_t	UpTicksToMicroseconds( uint64_t inTicks );
uint64_t	UpTicksToNanoseconds( uint64_t inTicks );
uint64_t	UpTicksToNTP( uint64_t inTicks );

uint64_t	SecondsToUpTicks( uint64_t x );
uint64_t	SecondsToUpTicksF( double x );
uint64_t	MillisecondsToUpTicks( uint64_t x );
uint64_t	MicrosecondsToUpTicks( uint64_t x );
uint64_t	NanosecondsToUpTicks( uint64_t x );
uint64_t	NTPtoUpTicks( uint64_t inNTP );

#if( !TARGET_OS_THREADX )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	UpTicksToTimeValTimeout
	@abstract	Converts an absolute, UpTicks deadline to a timeval timeout, suitable for passing to APIs like select.
	@discussion	This handles deadlines that have already expired (immediate timeout) and kUpTicksForever (no timeout).
*/
struct timeval *	UpTicksToTimeValTimeout( uint64_t inDeadline, struct timeval *inTimeVal );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TickUtilsTest
	@abstract	Unit test.
*/
void	TickUtilsTest( void );

#ifdef __cplusplus
}
#endif

#endif 	// __TickUtils_h__
