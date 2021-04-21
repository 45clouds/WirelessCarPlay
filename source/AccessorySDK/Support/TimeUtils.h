/*
	File:    	TimeUtils.h
	Package: 	Apple CarPlay Communication Plug-in.
	Abstract: 	n/a 
	Version: 	410.12
	
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
	
	Copyright (C) 2001-2016 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef __TimeUtils_h__
#define	__TimeUtils_h__

#include "CommonServices.h"	// Include early for TARGET_* conditionals.
#include "DebugServices.h"	// Include early for DEBUG_* conditionals.

#if( TARGET_HAS_STD_C_LIB )
	#include <time.h>
#endif

#if( TARGET_OS_POSIX )
	#include <sys/time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == Conversions ==
#endif

//===========================================================================================================================
//	Conversions
//===========================================================================================================================

#define	kNTPvsUnixSeconds			2208988800U // Seconds between 1900-01-01 00:00:00 UTC (NTP) and  1970-01-01 00:00:00 UTC (Unix).

#if 0
#pragma mark == Misc ==
#endif

#define kNTPFraction ( 1.0 / 4294967296.0 )

		// Converts a 24-hour value to AM/PM strings.
#define Hour24ToAMPM( HOUR )	( ( (HOUR) < 12 ) ? "AM" : "PM" )

// Converts a 24-hour value (0-23) to a 12-hour value (0-12).
STATIC_INLINE int	Hour24ToHour12( int inHour )
{
	if(      inHour ==  0 ) return( 12 );
	else if( inHour <= 12 ) return( inHour );
	return( inHour - 12 );
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	clock_gettime
	@abstract	POSIX clock_gettime. See <http://www.opengroup.org/onlinepubs/007908799/xsh/clock_gettime.html>.
*/
#if( TARGET_OS_WINDOWS )
	
	typedef int		clockid_t;
	
	#define CLOCK_REALTIME		1
	
	int	clock_gettime( clockid_t inClockID, struct timespec *outTS );
#endif

#if( TARGET_OS_VXWORKS || TARGET_PLATFORM_WICED || TARGET_OS_WINDOWS )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	gettimeofday
	@abstract	Gets the current time. WARNING: this timeline may change via NTP so don't rely on it being consistent.
	
	@param		outTV		Receives current time.
	@param		inUnused	Unused timezone parameter. Pass NULL.
	
	@returns	Error code or 0 if successful.
*/
int	gettimeofday( struct timeval *outTV, void *inUnused );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	YMD_HMStoSeconds
	@abstract	Converts a date/time as year, month, day, hour, minute, and second to a cumulative number of seconds.
	
	@param		inYear		Year starting from 0 (e.g. the year 2007 would be just be 2007).
	@param		inMonth		Month where 1=January and 12=December.
	@param		inDay		Day between 1-31 where 1 is the first day of the month.
	@param		inHour		Hour between 0-23 where 0 is 12:00 AM (beginning of the day).
	@param		inMinute	Minute between 0-59.
	@param		inSecond	Second between 0-62. 62 is allowed for double leap seconds (no longer allowed by POSIX).
	@param		inEpochDays	The number of cumulative days from 0 to act as the epoch (e.g. 719162 for 1970-01-01).
	
	@result		int64_t		Number of seconds between the specified date/time and the epoch.
*/

#define kDaysTo1900_01_01		693596 //! Number of days since 1900-01-01 (NTP epoch).
#define kDaysTo1904_01_01		695056 //! Number of days since 1904-01-01 (Mac epoch).
#define kDaysTo1970_01_01		719163 //! Number of days since 1970-01-01 (Unix epoch).
#define kDaysTo2001_01_01		730486 //! Number of days since 2001-01-01 (CoreFoundation epoch).

#define kDaysToNTPEpoch					kDaysSince1900_01_01
#define kDaysToMacEpoch					kDaysTo1904_01_01
#define kDaysToUnixEpoch				kDaysTo1970_01_01
#define kDaysToCoreFoundationEpoch		kDaysTo2001_01_01

int64_t	YMD_HMStoSeconds( int inYear, int inMonth, int inDay, int inHour, int inMinute, int inSecond, int inEpochDays );
void
	SecondsToYMD_HMS( 
		int64_t	inSeconds, 
		int *	outYear, 
		int *	outMonth, 
		int *	outDay, 
		int *	outHour, 
		int *	outMinute, 
		int *	outSecond );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TimeUtilsTest
	@abstract	Unit test.
*/
OSStatus	TimeUtilsTest( void );

#ifdef __cplusplus
}
#endif

#endif 	// __TimeUtils_h__
