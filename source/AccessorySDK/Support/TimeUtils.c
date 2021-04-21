/*
	File:    	TimeUtils.c
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

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if( !defined( _CRT_SECURE_NO_DEPRECATE ) )
	#define _CRT_SECURE_NO_DEPRECATE		1
#endif

#include "CommonServices.h" // Include early for TARGET_* conditionals.
#include "DebugServices.h"  // Include early for DEBUG* conditionals.

#if( TARGET_HAS_STD_C_LIB )
	#include <ctype.h>
	#include <stdio.h>
	#include <string.h>
	#include <time.h>
#endif

#include "TickUtils.h"

#if( TARGET_OS_POSIX )
	#include <sys/time.h>
#endif

#include "TimeUtils.h"

//===========================================================================================================================
//	clock_gettime
//===========================================================================================================================

#if( TARGET_OS_WINDOWS )
int	clock_gettime( clockid_t inClockID, struct timespec *outTS )
{
	int				err;
	FILETIME		ft;
	uint64_t		nsec;
	
	require_action( inClockID == CLOCK_REALTIME, exit, err = EINVAL );
	
	GetSystemTimeAsFileTime( &ft );			// Get 100ns units since 1/1/1601.
	nsec  = ( (uint64_t) ft.dwHighDateTime << 32 ) | ft.dwLowDateTime;
	nsec -= UINT64_C( 0x019db1ded53e8000 );	// Convert to Unix 1970 time: subtract 100ns units from 1/1/1601 to 1/1/1970.
	nsec *= 100;							// Convert 100ns units to nanoseconds.
	outTS->tv_sec  = (int32_t)( nsec / 1000000000 );
	outTS->tv_nsec = (int32_t)( nsec % 1000000000 );
	err = 0;

exit:
	return( err );
}
#endif

//===========================================================================================================================
//	gettimeofday
//===========================================================================================================================


#if( TARGET_OS_WINDOWS )
int	gettimeofday( struct timeval *outTV, void *inUnused )
{
	FILETIME		ft;
	uint64_t		usec;
	
	(void) inUnused; // Unused
	
	GetSystemTimeAsFileTime( &ft );			// Get 100ns units since 1/1/1601.
	usec =  ( (uint64_t) ft.dwHighDateTime << 32 ) | ft.dwLowDateTime;
	usec -= UINT64_C( 0x019db1ded53e8000 );	// Convert to Unix 1970 time: subtract 100ns units from 1/1/1601 to 1/1/1970.
	usec /= 10;								// Convert 100ns units to microseconds.
	outTV->tv_sec  = (int32_t)( usec / 1000000 );
	outTV->tv_usec = (int32_t)( usec % 1000000 );
	return( 0 );
}	
#endif

#if( TARGET_OS_VXWORKS )
int	gettimeofday( struct timeval *outTV, void *inUnused )
{
	struct timespec		ts;
	
	(void) inUnused; // Unused
	
	clock_gettime( CLOCK_REALTIME, &ts );
	outTV->tv_sec  = (time_t) ts.tv_sec;
	outTV->tv_usec = (int32_t)( ts.tv_nsec / 1000 );
	return( 0 );
}
#endif

#if( TARGET_PLATFORM_WICED )
int	gettimeofday( struct timeval *outTV, void *inUnused )
{
	(void) inUnused; // Unused
	
	outTV->tv_sec  = time( NULL );
	outTV->tv_usec = 0;
	return( 0 );
}
#endif

#if 0
#pragma mark -
#endif


//===========================================================================================================================
//	YMD_HMStoSeconds
//===========================================================================================================================

int64_t	YMD_HMStoSeconds( int inYear, int inMonth, int inDay, int inHour, int inMinute, int inSecond, int inEpochDays )
{
	int64_t		seconds;
	int			year;
	int			days;
	
	check(   inYear   >   0 );
	check( ( inMonth  >=  1 ) && ( inMonth  <= 12 ) );
	check( ( inDay    >=  1 ) && ( inDay    <= 31 ) );
	check( ( inHour   >=  0 ) && ( inHour   <= 23 ) );
	check( ( inMinute >=  0 ) && ( inMinute <= 59 ) );
	check( ( inSecond >=  0 ) && ( inSecond <= 62 ) ); // Note: 62 to handle double leap seconds.
	
	// Convert the year to days.
	
	year  = ( inYear - 1 );	// Only include days that occurred before this year.
	days  = ( year * 365 );	// Add all the days assuming all are non-leap years.
	days += ( year / 4 );	// Add a day for each 4 year cycle (possible leap years).
	days -= ( year / 100 );	// Subtract a day for each non-leap century year.
	days += ( year / 400 );	// Add a day for each leap century year.
	
	// Convert the month to days. This is some magic formula to calculate the cumulative number of non-leap days 
	// before the given month. Months after February are off by 1 day (leap year) or 2 days (non-leap year). This
	// could also be done using a const array of cumulative days, but I didn't want to require static storage.
	
	days += ( ( inMonth * 3057 ) - 3007 ) / 100;
	if( inMonth > 2 )
	{
		days -= ( IsLeapYear( inYear ) ? 1 : 2 );
	}
	days += inDay;
	days -= inEpochDays;
	
	// Convert the total full days and the partial day hours, minutes, and seconds to seconds.
	
	seconds = days;
	seconds *= ( 60 * 60 * 24 );		// Convert days to seconds.
	seconds += ( inHour * 60 * 60 );	// Add hour as seconds.
	seconds += ( inMinute * 60 );		// Add minute as seconds.
	seconds += inSecond;
	return( seconds );
}

//===========================================================================================================================
//	SecondsToYMD_HMS
//===========================================================================================================================

void
	SecondsToYMD_HMS( 
		int64_t	inSeconds, 
		int *	outYear, 
		int *	outMonth, 
		int *	outDay, 
		int *	outHour, 
		int *	outMinute, 
		int *	outSecond )
{
	int		days, seconds, x;
	int		year, month, day, hour, minute, second;
	
	days	= (int)( inSeconds / kSecondsPerDay );
	seconds = (int)( inSeconds % kSecondsPerDay );
	for( x = ( days * 400 ) / 146097; YearToDays( x ) < days; ++x ) {}
	year = x;
	
	x = days - YearToDays( x - 1 );
	if( x > 59 )
	{
		x += 2;
		if( IsLeapYear( year ) )
		{
			x -= ( x > 64 ) ? 1 : 2;
		}
	}
	month	= ( ( x * 100 ) + 3007 ) / 3057;
	day		= x - MonthToDays( month );
	hour	= seconds / kSecondsPerHour;
	seconds	= seconds % kSecondsPerHour;
	minute	= seconds / kSecondsPerMinute;
	second	= seconds % kSecondsPerMinute;
	
	if( outYear )	*outYear	= year;
	if( outMonth )	*outMonth	= month;
	if( outDay )	*outDay		= day;
	if( outHour )	*outHour	= hour;
	if( outMinute )	*outMinute	= minute;
	if( outSecond )	*outSecond	= second;
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )
//===========================================================================================================================
//	TimeUtilsTest
//===========================================================================================================================

OSStatus	TimeUtilsTest( void )
{
	OSStatus			err;
	struct timeval		tv;
	uint64_t			mics;
	int					year, month, day, hour, minute, second;
	
	require_action( !IsLeapYear( 1800 ), exit, err = kResponseErr );
	require_action(  IsLeapYear( 2000 ), exit, err = kResponseErr );
	require_action( !IsLeapYear( 2006 ), exit, err = kResponseErr );
	require_action(  IsLeapYear( 2008 ), exit, err = kResponseErr );
	
#if( TARGET_OS_WINDOWS )
{
	struct timespec		ts;
	
	err = clock_gettime( CLOCK_REALTIME, &ts );
	require_noerr( err, exit );
	
	err = clock_gettime( CLOCK_REALTIME, &ts );
	require_noerr( err, exit );
}
#endif

	err = gettimeofday( &tv, NULL );
	require_noerr( err, exit );
	
	mics = UpMicroseconds();
	sleep( 1 );
	mics = UpMicroseconds() - mics;
	require_action( ( mics >= 900000 ) && ( mics <= 2000000 ), exit, err = kResponseErr );
	
	// Hours
	
	require_action( strcmp( Hour24ToAMPM(  0 ), "AM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM(  1 ), "AM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM(  2 ), "AM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM(  3 ), "AM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM(  4 ), "AM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM(  5 ), "AM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM(  6 ), "AM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM(  7 ), "AM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM(  8 ), "AM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM(  9 ), "AM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM( 10 ), "AM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM( 11 ), "AM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM( 12 ), "PM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM( 13 ), "PM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM( 14 ), "PM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM( 15 ), "PM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM( 16 ), "PM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM( 17 ), "PM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM( 18 ), "PM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM( 19 ), "PM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM( 20 ), "PM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM( 21 ), "PM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM( 22 ), "PM" ) == 0, exit, err = -1 );
	require_action( strcmp( Hour24ToAMPM( 23 ), "PM" ) == 0, exit, err = -1 );
	
	require_action( Hour24ToHour12(  0 ) == 12, exit, err = -1 );
	require_action( Hour24ToHour12(  1 ) ==  1, exit, err = -1 );
	require_action( Hour24ToHour12(  2 ) ==  2, exit, err = -1 );
	require_action( Hour24ToHour12(  3 ) ==  3, exit, err = -1 );
	require_action( Hour24ToHour12(  4 ) ==  4, exit, err = -1 );
	require_action( Hour24ToHour12(  5 ) ==  5, exit, err = -1 );
	require_action( Hour24ToHour12(  6 ) ==  6, exit, err = -1 );
	require_action( Hour24ToHour12(  7 ) ==  7, exit, err = -1 );
	require_action( Hour24ToHour12(  8 ) ==  8, exit, err = -1 );
	require_action( Hour24ToHour12(  9 ) ==  9, exit, err = -1 );
	require_action( Hour24ToHour12( 10 ) == 10, exit, err = -1 );
	require_action( Hour24ToHour12( 11 ) == 11, exit, err = -1 );
	require_action( Hour24ToHour12( 12 ) == 12, exit, err = -1 );
	require_action( Hour24ToHour12( 13 ) ==  1, exit, err = -1 );
	require_action( Hour24ToHour12( 14 ) ==  2, exit, err = -1 );
	require_action( Hour24ToHour12( 15 ) ==  3, exit, err = -1 );
	require_action( Hour24ToHour12( 16 ) ==  4, exit, err = -1 );
	require_action( Hour24ToHour12( 17 ) ==  5, exit, err = -1 );
	require_action( Hour24ToHour12( 18 ) ==  6, exit, err = -1 );
	require_action( Hour24ToHour12( 19 ) ==  7, exit, err = -1 );
	require_action( Hour24ToHour12( 20 ) ==  8, exit, err = -1 );
	require_action( Hour24ToHour12( 21 ) ==  9, exit, err = -1 );
	require_action( Hour24ToHour12( 22 ) == 10, exit, err = -1 );
	require_action( Hour24ToHour12( 23 ) == 11, exit, err = -1 );
	
	// YMD_HMStoSeconds
	
	require_action( YMD_HMStoSeconds( 1900, 1, 1, 0, 0, 0, kDaysTo1900_01_01 ) == 0, exit, err = kResponseErr );
	require_action( YMD_HMStoSeconds( 1904, 1, 1, 0, 0, 0, kDaysTo1904_01_01 ) == 0, exit, err = kResponseErr );
	require_action( YMD_HMStoSeconds( 1970, 1, 1, 0, 0, 0, kDaysTo1970_01_01 ) == 0, exit, err = kResponseErr );
	require_action( YMD_HMStoSeconds( 2001, 1, 1, 0, 0, 0, kDaysTo2001_01_01 ) == 0, exit, err = kResponseErr );
	
	SecondsToYMD_HMS( INT64_C_safe( kDaysTo1900_01_01 ) * kSecondsPerDay, &year, &month, &day, &hour, &minute, &second );
	require_action( ( year == 1900 ) && ( month == 1 ) && ( day == 1 ), exit, err = kResponseErr );
	
	SecondsToYMD_HMS( INT64_C_safe( kDaysTo1904_01_01 ) * kSecondsPerDay, &year, &month, &day, &hour, &minute, &second );
	require_action( ( year == 1904 ) && ( month == 1 ) && ( day == 1 ), exit, err = kResponseErr );
	
	SecondsToYMD_HMS( INT64_C_safe( kDaysTo1970_01_01 ) * kSecondsPerDay, &year, &month, &day, &hour, &minute, &second );
	require_action( ( year == 1970 ) && ( month == 1 ) && ( day == 1 ), exit, err = kResponseErr );
	
	SecondsToYMD_HMS( INT64_C_safe( kDaysTo2001_01_01 ) * kSecondsPerDay, &year, &month, &day, &hour, &minute, &second );
	require_action( ( year == 2001 ) && ( month == 1 ) && ( day == 1 ), exit, err = kResponseErr );
	
exit:
	printf( "TimeUtilsTestOld: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

#endif // !EXCLUDE_UNIT_TESTS
