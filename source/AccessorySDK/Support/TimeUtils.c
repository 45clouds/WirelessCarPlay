/*
	File:    	TimeUtils.c
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

#include "StringUtils.h"
#include "TickUtils.h"

#if( TARGET_OS_POSIX )
	#include <sys/time.h>
#endif

#include "TimeUtils.h"

//===========================================================================================================================
//	clock_gettime
//===========================================================================================================================

#if( TARGET_OS_DARWIN )
int	clock_gettime( clockid_t inClockID, struct timespec *outTS )
{
	int		err;
	
	if( inClockID == CLOCK_REALTIME )
	{
		struct timeval		tv;
		
		gettimeofday( &tv, NULL );
		outTS->tv_sec  = tv.tv_sec;
		outTS->tv_nsec = tv.tv_usec * 1000;
	}
	else if( inClockID == CLOCK_MONOTONIC )
	{
		uint64_t		mics;
		
		mics = UpMicroseconds();
		outTS->tv_sec  = (int32_t)(   mics / 1000000 );
		outTS->tv_nsec = (int32_t)( ( mics % 1000000 ) * 1000 );
	}
	else
	{
		dlogassert( "unknown clock id: %d", inClockID );
		err = kUnsupportedErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}
#endif

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

#if( TARGET_OS_WINDOWS )
//===========================================================================================================================
//	settimeofday
//===========================================================================================================================

int	settimeofday( const struct timeval *inTV, const void *inUnused )
{
	OSStatus				err;
	OSStatus				tempErr;
	FILETIME				ft;
	SYSTEMTIME				st;
	uint64_t				usec;
	BOOL					good;
	HANDLE					processToken;
	TOKEN_PRIVILEGES		newPrivs;
	TOKEN_PRIVILEGES		oldPrivs;
	DWORD					size;
	
	(void) inUnused; // Unused
	
	processToken = NULL;
	
	usec  = ( ( (uint64_t) inTV->tv_sec ) * 1000000 ) + inTV->tv_usec;
	usec *= 10;								// Convert microseconds to 100ns units.
	usec += UINT64_C( 0x019db1ded53e8000 );	// Convert from Unix 1970 time: add 100ns units from 1/1/1601 to 1/1/1970.
	
	ft.dwHighDateTime	= (DWORD)( ( usec >> 32 ) & UINT64_C( 0xFFFFFFFF ) );
	ft.dwLowDateTime	= (DWORD)(   usec         & UINT64_C( 0xFFFFFFFF ) );
	
	good = FileTimeToSystemTime( &ft, &st );
	err = map_global_value_errno( good, good );
	require_noerr( err, exit );
	
	// Enable the SetSystemTime privilege for the current process to allow us to set the time when not admin.
	
	good = OpenProcessToken( GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &processToken );
	err = map_global_value_errno( good, good );
	require_noerr( err, exit );
	
	memset( &newPrivs, 0, sizeof( newPrivs ) );
	newPrivs.PrivilegeCount				= 1;
	newPrivs.Privileges[ 0 ].Attributes	= SE_PRIVILEGE_ENABLED;
	good = LookupPrivilegeValue( NULL, SE_SYSTEMTIME_NAME, &newPrivs.Privileges[ 0 ].Luid );
	err = map_global_value_errno( good, good );
	require_noerr( err, exit );
	
	size = sizeof( oldPrivs );
	good = AdjustTokenPrivileges( processToken, FALSE, &newPrivs, sizeof( newPrivs ), &oldPrivs, &size );
	err = map_global_value_errno( good, good );
	require_noerr( err, exit );
 	
 	// Set the new time.
 	
	good = SetSystemTime( &st );
	err = map_global_value_errno( good, good );
	check_noerr( err );
	
	// Restore the SetSystemTime privilege.
	
	good = AdjustTokenPrivileges( processToken, FALSE, &oldPrivs, size, NULL, NULL );
	tempErr = map_global_value_errno( good, good );
	check_noerr( tempErr );
	if( !err ) err = tempErr;
	
exit:
	if( processToken ) CloseHandle( processToken );
	return( err );
}
#endif

//===========================================================================================================================
//	NanoTimeGetCurrent
//===========================================================================================================================

NanoTime64	NanoTimeGetCurrent( void )
{
#if( TARGET_OS_DARWIN ) // No clock_gettime on Darwin.
	int					err;
	struct timeval		tv;
	
	DEBUG_USE_ONLY( err );
	
	err = gettimeofday( &tv, NULL );
	check_noerr( err );
	return( NanoTimeFromTimeVal( &tv ) );
#else
	OSStatus			err;
	struct timespec		ts;
	
	DEBUG_USE_ONLY( err );
	
	err = clock_gettime( CLOCK_REALTIME, &ts );
	check_noerr( err );
	return( NanoTimeFromTimeSpec( &ts ) );
#endif
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	ISODayOfWeek
//
//	Derived from code by Tomohiko Sakamoto in the C FAQ 17.28. 1 = Monday, 1 = January, y > 1752.
//===========================================================================================================================

int	ISODayOfWeek( int y, int m, int d )
{
	static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
	int dow;
	
	check( y > 1752 );
	check( ( m >= 1 ) && ( m <= 12 ) );
	check( ( d >= 1 ) && ( d <= 31 ) );
	
	y -= m < 3;
	dow = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
	dow = ANSIWeekDayToISOWeekDay( dow );
	return( dow );
}

//===========================================================================================================================
//	GMTtoLocalTime
//===========================================================================================================================

#if( TARGET_HAS_STD_C_LIB )
int	GMTtoLocalTime( const struct tm *inGMT, struct tm *outLocalBrokenDownTime, time_t *outLocalCalendarTime )
{
	int				err;
	time_t			currentCalendarTime;
	struct tm *		tmPtr;
	struct tm		localBrokenDownTime;
	struct tm		gmtBrokenDownTime;
	time_t			localCalendarTime;
	time_t			gmtCalendarTime;
	double			gmtOffset;
	struct tm		tempBrokenDownTime;
		
	// Make a copy of the input GMT time because some systems (e.g. Windows) reuse the same global storage for all
	// ANSI C time operations and may be overwritten when we call the time functions if passing one of those ptrs.
	
	tempBrokenDownTime = *inGMT;
	
	// Calculate the GMT offset.
	
	currentCalendarTime = time( NULL );
	require_action( currentCalendarTime != ( (time_t) -1 ), exit, err = -1 );
	
	tmPtr = localtime( &currentCalendarTime );
	require_action( tmPtr, exit, err = -2 );
	localBrokenDownTime = *tmPtr;
	
	tmPtr = gmtime( &currentCalendarTime );
	require_action( tmPtr, exit, err = -3 );
	gmtBrokenDownTime = *tmPtr;
	
	localCalendarTime = mktime( &localBrokenDownTime );
	require_action( localCalendarTime != ( (time_t) -1 ), exit, err = -4 );
	
	gmtBrokenDownTime.tm_isdst = localBrokenDownTime.tm_isdst;
	gmtCalendarTime = mktime( &gmtBrokenDownTime );
	require_action( gmtCalendarTime != ( (time_t) -1 ), exit, err = -5 );
	
	gmtOffset = difftime( localCalendarTime, gmtCalendarTime );
	
	// Convert the GMT time to local time by adjusting for the GMT offset and normalizing.
	
	tempBrokenDownTime.tm_isdst	= localBrokenDownTime.tm_isdst;
	tempBrokenDownTime.tm_sec  += (int) gmtOffset;
	
	localCalendarTime = mktime( &tempBrokenDownTime );
	require_action( localCalendarTime != ( (time_t) -1 ), exit, err = -6 );
	
	// Success!
	
	if( outLocalBrokenDownTime ) *outLocalBrokenDownTime = tempBrokenDownTime;
	if( outLocalCalendarTime )   *outLocalCalendarTime   = localCalendarTime;
	err = 0;
	
exit:
	return( err );
}
#endif

//===========================================================================================================================
//	UTCOffset
//===========================================================================================================================

#if( TARGET_HAS_STD_C_LIB )
OSStatus	UTCOffset( int *outUTCOffset )
{
	OSStatus		err;
	time_t			currentCalendarTime;
	struct tm *		tmPtr;
	struct tm		localBrokenDownTime;
	struct tm		gmtBrokenDownTime;
	time_t			localCalendarTime;
	time_t			gmtCalendarTime;
	
	currentCalendarTime = time( NULL );
	require_action( currentCalendarTime != ( (time_t) -1 ), exit, err = kUnknownErr );
	
	tmPtr = localtime( &currentCalendarTime );
	require_action( tmPtr, exit, err = kUnknownErr );
	localBrokenDownTime = *tmPtr;
	
	tmPtr = gmtime( &currentCalendarTime );
	require_action( tmPtr, exit, err = kUnknownErr );
	gmtBrokenDownTime = *tmPtr;
	
	localCalendarTime = mktime( &localBrokenDownTime );
	require_action( localCalendarTime != ( (time_t) -1 ), exit, err = kUnknownErr );
	
	gmtBrokenDownTime.tm_isdst = localBrokenDownTime.tm_isdst;
	gmtCalendarTime = mktime( &gmtBrokenDownTime );
	require_action( gmtCalendarTime != ( (time_t) -1 ), exit, err = kUnknownErr );
	
	*outUTCOffset = (int) difftime( localCalendarTime, gmtCalendarTime ); // GMT is basically the same as UTC.
	err = kNoErr;
	
exit:
	return( err );
}
#endif

//===========================================================================================================================
//	UTCSeconds
//===========================================================================================================================

#if( TARGET_HAS_STD_C_LIB )
OSStatus	UTCSeconds( const struct tm *inTM, uint32_t *outSeconds )
{
	OSStatus		err;
	struct tm		tm1;
	struct tm		tm2;
	struct tm *		tp;
	time_t			tt1;
	time_t			tt2;
	double			diff;
	
	require_action_expect( outSeconds, exit, err = kParamErr );
	
	// Get the time to measure. If no time is passed in, use the current time.
	
	if( inTM )
	{
		tm1 = *inTM;
	}
	else
	{
		tt1 = time( NULL );
		require_action( tt1 != (time_t) -1, exit, err = kUnknownErr );
		
		tp = gmtime( &tt1 );
		require_action( tp, exit, err = kUnsupportedErr );
		tm1 = *tp;
	}
	
	tt1 = mktime( &tm1 );
	require_action( tt1 != (time_t) -1, exit, err = kUnknownErr );
	
	// Get the reference time of 2001-01-1 UTC.
	
	tm2.tm_year		= 2001 - 1900;
	tm2.tm_mon 		= 1 - 1;
	tm2.tm_mday 	= 1;
	tm2.tm_hour 	= 0;
	tm2.tm_min		= 0;
	tm2.tm_sec		= 0;
	tm2.tm_isdst	= -1;
	tt2 = mktime( &tm2 );
	require_action( tt2 != ( (time_t) -1 ), exit, err = kUnknownErr );
	
	// Calculate the difference in seconds and return it.
	
	diff = difftime( tt1, tt2 );
	require_action( diff >= 0, exit, err = kRangeErr );
	
	*outSeconds = (uint32_t) diff;
	err = kNoErr;
	
exit:
	return( err );
}
#endif

//===========================================================================================================================
//	SecondsToTimeComponentsString
//
//	Converts seconds into a days, hours, minutes, and seconds string. For example: 930232 -> "10d 18h 23m 52s".
//===========================================================================================================================

char *	SecondsToTimeComponentsString( unsigned int inSeconds, char *inBuffer )
{
	unsigned int		days;
	unsigned int		hours;
	unsigned int		minutes;
	unsigned int		seconds;
	char *				dst;
	char *				lim;
	
	days		= inSeconds / kSecondsPerDay;
	inSeconds	= inSeconds % kSecondsPerDay;
	hours		= inSeconds / kSecondsPerHour;
	inSeconds	= inSeconds % kSecondsPerHour;
	minutes		= inSeconds / kSecondsPerMinute;
	seconds		= inSeconds % kSecondsPerMinute;
	
	dst = inBuffer;
	lim = dst + 32;
	if( days != 0 )
	{
		snprintf_add( &dst, lim, "%ud", days );
	}
	if( hours != 0 )
	{
		if( ( dst != inBuffer ) && ( dst < lim ) ) *dst++ = ' ';
		snprintf_add( &dst, lim, "%uh", hours );
	}
	if( minutes != 0 )
	{
		if( ( dst != inBuffer ) && ( dst < lim ) ) *dst++ = ' ';
		snprintf_add( &dst, lim, "%um", minutes );
	}
	if( ( seconds != 0 ) || ( dst == inBuffer ) )
	{
		if( ( dst != inBuffer ) && ( dst < lim ) ) *dst++ = ' ';
		snprintf_add( &dst, lim, "%us", seconds );
	}
	if( dst < lim ) *dst = '\0';
	
	return( inBuffer );
}


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

//===========================================================================================================================
//	MakeFractionalDateString
//===========================================================================================================================

char *	MakeFractionalDateString( const struct timeval *inTime, char *inBuffer, size_t inMaxLen )
{
	struct timeval		now;
	time_t				nowTT;
	struct tm *			nowTM;
	size_t				len;
	
	if( !inTime )
	{
		gettimeofday( &now, NULL );
		inTime = &now;
	}
	nowTT = inTime->tv_sec;
	nowTM = gmtime( &nowTT );
	require_action_quiet( nowTM, exit, inBuffer = (char *) "" );
	
	len = strftime( inBuffer, inMaxLen, "%Y-%m-%dT%H:%M:%S", nowTM );
	snprintf( &inBuffer[ len ], inMaxLen - len, ".%06uZ", (unsigned int) inTime->tv_usec );
	
exit:
	return( inBuffer );
}

//===========================================================================================================================
//	ParseFractionalDateString
//
//	Parses an ISO 8601 date/time with fractional seconds: "YYYY-MM-DDThh:mm:ss.sTZD". See <http://www.w3.org/TR/NOTE-datetime>
//===========================================================================================================================

OSStatus	ParseFractionalDateString( const char *inStr, size_t inLen, struct timeval *outTime, const char **outSrc )
{
	OSStatus			err;
	const char *		src;
	const char *		end;
	const char *		ptr;
	char				c;
	int					year;
	int					month;
	int					day;
	int					hour;
	int					minute;
	int					second;
	int64_t				frac;
	int					power, denominator;
	
	src		= inStr;
	end		= inStr + ( ( inLen == kSizeCString ) ? strlen( inStr ) : inLen );
	c		= 0;
	year	= 0;
	month	= 1;
	day		= 1;
	hour	= 0;
	minute	= 0;
	second	= 0;
	frac	= 0;
	
	// Year
	
	for( ptr = src; ( src < end ) && isdigit_safe( ( c = *src ) ); ++src ) year = ( year * 10 ) + ( c - '0' );
	require_action( ( src - ptr ) == 4, exit, err = kMalformedErr );
	if( src == end ) goto done;
	require_action( ( c == '-' ) || ( c == 'Z' ), exit, err = kMalformedErr );
	++src;
	
	// Month
	
	month = 0;
	for( ptr = src; ( src < end ) && isdigit_safe( ( c = *src ) ); ++src ) month = ( month * 10 ) + ( c - '0' );
	require_action( ( src - ptr ) == 2, exit, err = kMalformedErr );
	require_action( ( month >= 1 ) && ( month <= 12 ), exit, err = kRangeErr );
	if( src == end ) goto done;
	require_action( ( c == '-' ) || ( c == 'Z' ), exit, err = kMalformedErr );
	++src;
	if( c == 'Z' ) goto done;
	
	// Day
	
	day = 0;
	for( ptr = src; ( src < end ) && isdigit_safe( ( c = *src ) ); ++src ) day = ( day * 10 ) + ( c - '0' );
	require_action( ( src - ptr ) == 2, exit, err = kMalformedErr );
	require_action( ( day >= 0 ) && ( day <= 31 ), exit, err = kRangeErr );
	if( src == end ) goto done;
	require_action( ( c == 'T' ) || ( c == 'Z' ), exit, err = kMalformedErr );
	++src;
	if( c == 'Z' ) goto done;
	
	// Hour
	
	for( ptr = src; ( src < end ) && isdigit_safe( ( c = *src ) ); ++src ) hour = ( hour * 10 ) + ( c - '0' );
	require_action( ( src - ptr ) == 2, exit, err = kMalformedErr );
	require_action( ( hour >= 0 ) && ( hour <= 23 ), exit, err = kRangeErr );
	if( src == end ) goto done;
	require_action( ( c == ':' ) || ( c == 'Z' ), exit, err = kMalformedErr );
	++src;
	if( c == 'Z' ) goto done;
	
	// Minute
	
	for( ptr = src; ( src < end ) && isdigit_safe( ( c = *src ) ); ++src ) minute = ( minute * 10 ) + ( c - '0' );
	require_action( ( src - ptr ) == 2, exit, err = kMalformedErr );
	require_action( ( minute >= 0 ) && ( minute <= 59 ), exit, err = kRangeErr );
	if( src == end ) goto done;
	require_action( ( c == ':' ) || ( c == 'Z' ), exit, err = kMalformedErr );
	++src;
	if( c == 'Z' ) goto done;
	
	// Second
	
	for( ptr = src; ( src < end ) && isdigit_safe( ( c = *src ) ); ++src ) second = ( second * 10 ) + ( c - '0' );
	require_action( ( src - ptr ) == 2, exit, err = kMalformedErr );
	require_action( ( second >= 0 ) && ( second <= 61 ), exit, err = kRangeErr ); // Handle double-leap seconds.
	if( src == end ) goto done;
	require_action( ( c == '.' ) || ( c == 'Z' ), exit, err = kMalformedErr );
	++src;
	if( c == 'Z' ) goto done;
	
	// Fraction of a second.
	
	for( ptr = src; ( src < end ) && isdigit_safe( ( c = *src ) ); ++src ) frac = ( frac * 10 ) + ( c - '0' );
	power = (int)( src - ptr );
	require_action( power <= 9, exit, err = kRangeErr ); // 10^9 is the largest denominator a 32-bit int can handle.
	for( denominator = 1; power-- > 0; denominator *= 10 ) {}
	frac = ( frac * kMicrosecondsPerSecond ) / denominator;
	if( src == end ) goto done;
	require_action( c == 'Z', exit, err = kMalformedErr );
	++src;
	
done:
	if( outTime )
	{
		outTime->tv_sec  = (int32_t) YMD_HMStoSeconds( year, month, day, hour, minute, second, kDaysTo1970_01_01 );
		outTime->tv_usec = (int32_t) frac;
	}
	if( outSrc ) *outSrc = src;
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == TimeDescriptor ==
#endif

//===========================================================================================================================
//	TimeDescriptor
//===========================================================================================================================

static OSStatus
	TimeDescriptorFindComponent( 
		const char *	inDesc, 
		const char *	inName, 
		const char **	outValue, 
		size_t *		outValueSize, 
		const char **	outNext );

static OSStatus
	TimeDescriptorParseComponent( 
		const char *	inDesc, 
		const char **	outName, 
		size_t *		outNameSize, 
		const char **	outValue, 
		size_t *		outValueSize, 
		const char **	outNext );

//===========================================================================================================================
//	TimeDescriptorMakeWithDaysAndTimes
//===========================================================================================================================

OSStatus
	TimeDescriptorMakeWithDaysAndTimes( 
		DaySet	inDays, 
		int		inStartHour, 
		int		inStartMinute, 
		int		inStartSecond, 
		int		inEndHour, 
		int		inEndMinute, 
		int		inEndSecond, 
		char	outDesc[ 64 ] )
{
	OSStatus		err;
	char *			p;
	char *			q;
	
	require_action( ( inDays & ~kDaySetAllDays ) == 0, exit, err = kParamErr );
	require_action( ( inStartHour   >= 0 ) && ( inStartHour   <= 23 ), exit, err = kRangeErr );
	require_action( ( inStartMinute >= 0 ) && ( inStartMinute <= 59 ), exit, err = kRangeErr );
	require_action( ( inStartSecond >= 0 ) && ( inStartSecond <= 61 ), exit, err = kRangeErr ); // 0-61 for double leap seconds.
	require_action( ( inEndHour     >= 0 ) && ( inEndHour     <= 24 ), exit, err = kRangeErr ); // See time notes below for end ranges.
	require_action( ( inEndMinute   >= 0 ) && ( inEndMinute   <= 60 ), exit, err = kRangeErr ); // See time notes below for end ranges.
	require_action( ( inEndSecond   >= 0 ) && ( inEndSecond   <= 62 ), exit, err = kRangeErr ); // See time notes below for end ranges.
	require_action( inStartHour <= inEndHour, exit, err = kParamErr );
	if( inStartHour == inEndHour )
	{
		require_action( inStartMinute <= inEndMinute, exit, err = kParamErr );
		if( inStartMinute == inEndMinute )
		{
			require_action( inStartSecond <= inEndSecond, exit, err = kParamErr );
		}
	}
	
	p = outDesc;
	q = p + 64;
	
	// Generate the day set. A day set is specified using a component name of "days" with the value being 7 characters. 
	// Each character maps to a specific day with the lower-cased first letter of that day meaning enabled and a dash (-) 
	// meaning disabled. If no "days" component is included in the date descriptor then all days are considered enabled 
	// (i.e. any day matches). If all days are valid, don't include it since no days component means any day.
	//
	// Examples:
	//
	// "days=m-w-f--"	Only Monday, Wednesday, and Friday.
	// "days=-----ss"	Saturday and Sunday.
	
	if( ( inDays & kDaySetAllDays ) != kDaySetAllDays )
	{
		snprintf_add( &p, q, "days=%c%c%c%c%c%c%c;", 
			( inDays & kDaySetMon ) ? 'm' : '-', 
			( inDays & kDaySetTue ) ? 't' : '-', 
			( inDays & kDaySetWed ) ? 'w' : '-', 
			( inDays & kDaySetThr ) ? 't' : '-', 
			( inDays & kDaySetFri ) ? 'f' : '-', 
			( inDays & kDaySetSat ) ? 's' : '-', 
			( inDays & kDaySetSun ) ? 's' : '-' );
	}
	
	// Sparse times are specified using an unlimited number of time components. Each time component is specified 
	// using a name of "t" and a value consisting of a start time, a dash (-), and an end time. Start/end times are 
	// specified in 24-hour format and they only need to use the most significant segments needed to represent the time 
	// (e.g. "8" for 8 AM"). Insignificant digits may still be used if desired, but they are not required (e.g. "8:00" 
	// and "8:00:00" are also valid for 8 AM). Leading zeros are permitted (e.g. "08" is valid for 8 AM). When 
	// multiple time components are included in a TimeDescriptor, they act as a logical OR for matches; if the time 
	// falls within any of the time components then it is considered a match. If no time components are included in the 
	// TimeDescriptor then any time is considered a match. Start times are inclusive, but end times are exclusive so 
	// natural-looking intervals can be specified (e.g. 8-10) without incorrectly triggering a match on the second 
	// immediately after the intended range (e.g. 8-10 means 9:59:59 is a match, but not 10:00:00).
	//
	// Examples:
	//
	// "t=8-10"				8 AM to 10 AM
	// "t=8:30-10:25:30"	8:30 AM to 10:25:30 AM
	// "t=8-12;t=13-17"		8 AM to 12 AM or 1 PM to 5 PM
	
	*p++ = 't';
	*p++ = '=';
	
	// Add start time with only enough precision to represent the time (e.g. "8" for 8 AM, but "8:00:10" for 8:00:10 AM).
	
	snprintf_add( &p, q, "%d", inStartHour );
	if( ( inStartMinute != 0 ) || ( inStartSecond != 0 ) )	snprintf_add( &p, q, ":%02d", inStartMinute );
	if( inStartSecond != 0 )								snprintf_add( &p, q, ":%02d", inStartSecond );
	if( p < q ) *p++ = '-'; // Add start/end separator.
	
	// Add end time with only enough precision to represent the time (e.g. "8" for 8 AM, but "8:00:10" for 8:00:10 AM).
	
	snprintf_add( &p, q, "%d", inEndHour );
	if( ( inEndMinute != 0 ) || ( inEndSecond != 0 ) )	snprintf_add( &p, q, ":%02d", inEndMinute );
	if( inEndSecond != 0 )								snprintf_add( &p, q, ":%02d", inEndSecond );
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	TimeDescriptorGetDays
//===========================================================================================================================

OSStatus	TimeDescriptorGetDays( const char *inDesc, DaySet *outDays )
{
	OSStatus			err;
	const char *		p;
	size_t				n;
	DaySet				days;
	
	// Parse a day set. It is an array of 7 characters, one for each day of the week. If the character matches the 
	// first letter of that day of the week in lower case, it means that day is included. If the character is a '-' 
	// then it means that day is not included. Any other character is invalid. Here are some examples:
	//
	// "days=mtwtfss"	Every day
	// "days=-------"	No days
	// "days=m-w-f--"	Monday, Wednesday, and Friday
	// "days=-----ss"	Saturday and Sunday
	
	err = TimeDescriptorFindComponent( inDesc, "days", &p, &n, NULL );
	require( ( err == kNoErr ) || ( err == kNotFoundErr ), exit );
	if( err == kNoErr )
	{
		// Make sure the day set is valid.
		
		require_action( n == 7, exit, err = kMalformedErr );
		require_action( ( p[ 0 ] == 'm' ) || ( p[ 0 ] == '-' ), exit, err = kMalformedErr );
		require_action( ( p[ 1 ] == 't' ) || ( p[ 1 ] == '-' ), exit, err = kMalformedErr );
		require_action( ( p[ 2 ] == 'w' ) || ( p[ 2 ] == '-' ), exit, err = kMalformedErr );
		require_action( ( p[ 3 ] == 't' ) || ( p[ 3 ] == '-' ), exit, err = kMalformedErr );
		require_action( ( p[ 4 ] == 'f' ) || ( p[ 4 ] == '-' ), exit, err = kMalformedErr );
		require_action( ( p[ 5 ] == 's' ) || ( p[ 5 ] == '-' ), exit, err = kMalformedErr );
		require_action( ( p[ 6 ] == 's' ) || ( p[ 6 ] == '-' ), exit, err = kMalformedErr );
		
		// Set each enabled day.
		
		days = 0;
		if( p[ 0 ] != '-' ) days |= kDaySetMon;
		if( p[ 1 ] != '-' ) days |= kDaySetTue;
		if( p[ 2 ] != '-' ) days |= kDaySetWed;
		if( p[ 3 ] != '-' ) days |= kDaySetThr;
		if( p[ 4 ] != '-' ) days |= kDaySetFri;
		if( p[ 5 ] != '-' ) days |= kDaySetSat;
		if( p[ 6 ] != '-' ) days |= kDaySetSun;
	}
	else
	{
		// No day set component was found so include all days (i.e. no days excluded).
		
		days = kDaySetAllDays;
		err = kNoErr;
	}
	*outDays = days;
	
exit:
	return( err );
}

//===========================================================================================================================
//	TimeDescriptorGetStartEndTime
//===========================================================================================================================

OSStatus
	TimeDescriptorGetStartEndTimes( 
		const char *inDesc, 
		int *		outStartHour, 
		int *		outStartMinute, 
		int *		outStartSecond, 
		int *		outEndHour, 
		int *		outEndMinute, 
		int *		outEndSecond )
{
	OSStatus			err;
	const char *		value;
	size_t				valueSize;
	const char *		p;
	const char *		q;
	int					sHH;
	int					sMM;
	int					sSS;
	int					eHH;
	int					eMM;
	int					eSS;
	
	// Search for a time component that includes the time. There can be multiple time components in a TimeDescriptor and
	// they are logically OR'd together so if any time component includes the time, it is a match. If no time components
	// are present then it is also consider a match (i.e. no time restrictions). Only if there are time components and 
	// none of them match is it considered a mismatch. A time component is a start/end time in the following format:
	//
	// "t=HH[[:MM]:SS]-HH[[:MM]:SS]"
	// 
	// Leading zeros and insignificant trailing zeros are optional. Time is in 24-hour format. Here are some examples:
	// 
	// "t=8:30-10:30"			 8:30:00 AM to 10:30:00 AM.
	// "t=12:05:00-17"			12:05:00 PM to  5:00:00 PM.
	// "t=12:05:00-17:05:00"	12:05:00 PM to  5:05:00 PM.
	
	err = TimeDescriptorFindComponent( inDesc, "t", &value, &valueSize, &inDesc );
	if( err == kNotFoundErr ) goto exit;
	require_noerr( err, exit );
	
	p = value;
	q = p + valueSize;
	
	// Parse the start time. Allow 0-61 for leap and double leap seconds.
	
	err = ParseTime( p, q, &sHH, &sMM, &sSS, &p );
	require_noerr( err, exit );
	require_action( ( p < q ) && ( *p == '-' ), exit, err = kMalformedErr );
	require_action( ( sHH >= 0 ) && ( sHH <= 23 ), exit, err = kRangeErr );
	require_action( ( sMM >= 0 ) && ( sMM <= 59 ), exit, err = kRangeErr );
	require_action( ( sSS >= 0 ) && ( sSS <= 61 ), exit, err = kRangeErr );
	++p;
	
	// Parse the end time. Allow 0-62 for leap and double leap seconds and the end-of-range.
	
	err = ParseTime( p, q, &eHH, &eMM, &eSS, &p );
	require_noerr( err, exit );
	require_action( p == q, exit, err = kMalformedErr );
	require_action( ( eHH >= 0 ) && ( eHH <= 24 ), exit, err = kRangeErr );
	require_action( ( eMM >= 0 ) && ( eMM <= 60 ), exit, err = kRangeErr );
	require_action( ( eSS >= 0 ) && ( eSS <= 62 ), exit, err = kRangeErr );
	
	// Fill in the results.
	
	*outStartHour	= sHH;
	*outStartMinute	= sMM;
	*outStartSecond	= sSS;
	*outEndHour		= eHH;
	*outEndMinute	= eMM;
	*outEndSecond	= eSS;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	TimeDescriptorMatch
//===========================================================================================================================

#if( TARGET_HAS_STD_C_LIB )

static OSStatus	TimeDescriptorDayMatch( const char *inDesc, int inISOWeekDay, Boolean *outMatch );
static OSStatus	TimeDescriptorTimeMatch( const char *inDesc, int inHour, int inMinute, int inSecond, Boolean *outMatch );

OSStatus	TimeDescriptorMatch( const char *inDesc, const struct tm *inTM, Boolean *outMatch )
{
	OSStatus		err;
	time_t			tt;
	Boolean			match;
	
	// If no time is passed in, use the local time.
	
	if( !inTM )
	{
		tt = time( NULL );
		require_action( tt != ( (time_t) -1 ), exit, err = kUnknownErr );
		
		inTM = localtime( &tt );
		require_action( inTM, exit, err = kUnknownErr );
	}
	
	// Check if the days and times match.
	
	match = false;
	err = TimeDescriptorDayMatch( inDesc, ANSIWeekDayToISOWeekDay( inTM->tm_wday ), &match );
	require_noerr( err, exit );
	
	if( match )
	{
		err = TimeDescriptorTimeMatch( inDesc, inTM->tm_hour, inTM->tm_min, inTM->tm_sec, &match );
		require_noerr( err, exit );
	}
	
	if( outMatch ) *outMatch = match;
	
exit:
	return( err );
}

//
//	TimeDescriptorDayMatch
//
static OSStatus	TimeDescriptorDayMatch( const char *inDesc, int inISOWeekDay, Boolean *outMatch )
{
	OSStatus			err;
	DaySet				days;
	
	require_action( ( inISOWeekDay >= 1 ) && ( inISOWeekDay <= 7 ), exit, err = kParamErr );
	
	err = TimeDescriptorGetDays( inDesc, &days );
	require_noerr( err, exit );
	
	if( outMatch ) *outMatch = (Boolean) DaySetContainsISOWeekDay( days, inISOWeekDay );
	
exit:
	return( err );
}

//
//	TimeDescriptorTimeMatch
//
static OSStatus	TimeDescriptorTimeMatch( const char *inDesc, int inHour, int inMinute, int inSecond, Boolean *outMatch )
{
	OSStatus			err;
	Boolean				match;
	const char *		value;
	size_t				valueSize;
	const char *		p;
	const char *		q;
	int					sHH;
	int					sMM;
	int					sSS;
	int					eHH;
	int					eMM;
	int					eSS;
	int					s;
	int					e;
	int					t;
	
	require_action( ( inHour   >= 0 ) && ( inHour   <= 23 ), exit, err = kRangeErr );
	require_action( ( inMinute >= 0 ) && ( inMinute <= 59 ), exit, err = kRangeErr );
	require_action( ( inSecond >= 0 ) && ( inSecond <= 61 ), exit, err = kRangeErr ); // 0-61 for leap and double leap seconds.
	
	// Search for a time component that includes the time. There can be multiple time components in a TimeDescriptor and
	// they are logically OR'd together so if any time component includes the time, it is a match. If no time components
	// are present then it is also consider a match (i.e. no time restrictions). Only if there are time components and 
	// none of them match is it considered a mismatch. A time component is a start/end time in the following format:
	//
	// "t=HH[[:MM]:SS]-HH[[:MM]:SS]"
	// 
	// Leading zeros and insignificant trailing zeros are optional. Time is in 24-hour format. Here are some examples:
	// 
	// "t=8:30-10:30"			 8:30:00 AM to 10:30:00 AM.
	// "t=12:05:00-17"			12:05:00 PM to  5:00:00 PM.
	// "t=12:05:00-17:05:00"	12:05:00 PM to  5:05:00 PM.
	
	t = ( inHour * ( 60 * 60 ) ) + ( inMinute * 60 ) + inSecond;
	match = true;
	for( ;; )
	{
		err = TimeDescriptorFindComponent( inDesc, "t", &value, &valueSize, &inDesc );
		if( err == kNotFoundErr ) break;
		require_noerr( err, exit );
		
		p = value;
		q = p + valueSize;
		
		// Parse the start time. Allow 0-61 for leap and double leap seconds.
		
		err = ParseTime( p, q, &sHH, &sMM, &sSS, &p );
		require_noerr( err, exit );
		require_action( ( p < q ) && ( *p == '-' ), exit, err = kMalformedErr );
		require_action( ( sHH >= 0 ) && ( sHH <= 23 ), exit, err = kRangeErr );
		require_action( ( sMM >= 0 ) && ( sMM <= 59 ), exit, err = kRangeErr );
		require_action( ( sSS >= 0 ) && ( sSS <= 61 ), exit, err = kRangeErr );
		++p;
		
		// Parse the end time. Allow 0-62 for leap and double leap seconds and the end-of-range.
		
		err = ParseTime( p, q, &eHH, &eMM, &eSS, &p );
		require_noerr( err, exit );
		require_action( p == q, exit, err = kMalformedErr );
		require_action( ( eHH >= 0 ) && ( eHH <= 24 ), exit, err = kRangeErr );
		require_action( ( eMM >= 0 ) && ( eMM <= 60 ), exit, err = kRangeErr );
		require_action( ( eSS >= 0 ) && ( eSS <= 62 ), exit, err = kRangeErr );
		
		// If the time is within the start/end times then it's a match. Note: the start time is inclusive, but the end 
		// time is exclusive. This is needed so times like 8 PM to 10 PM can be used to mean that any time from the first 
		// second of 8 PM (8:00:00) until the clock turns to 10 PM (10:00:00 is when the clock is at 10 PM so 9:59:59
		// is the last second. Otherwise, you'd have to use 8:00:00/09:59:59, which is confusing and unnatural.
		
		s = ( sHH * ( 60 * 60 ) ) + ( sMM * 60 ) + sSS;
		e = ( eHH * ( 60 * 60 ) ) + ( eMM * 60 ) + eSS;
		match = (Boolean)( ( t >= s ) && ( t < e ) );
		if( match ) break;
	}
	
	if( outMatch ) *outMatch = match;
	err = kNoErr;
	
exit:
	return( err );
}
#endif // TARGET_HAS_STD_C_LIB

//===========================================================================================================================
//	TimeDescriptorFindComponent
//===========================================================================================================================

static OSStatus
	TimeDescriptorFindComponent( 
		const char *	inDesc, 
		const char *	inName, 
		const char **	outValue, 
		size_t *		outValueSize, 
		const char **	outNext )
{
	OSStatus			err;
	const char *		name;
	size_t				nameSize;
	const char *		value;
	size_t				valueSize;
	
	name		= NULL; // Work around GCC warning bug by initializing variables.
	nameSize	= 0;
	value		= NULL;
	valueSize	= 0;
	
	for( ;; )
	{
		err = TimeDescriptorParseComponent( inDesc, &name, &nameSize, &value, &valueSize, &inDesc );
		if( err == kNotFoundErr ) goto exit;
		require_noerr( err, exit );
		if( strncmpx( name, nameSize, inName ) == 0 ) break;
	}
	
	if( outValue )		*outValue		= value;
	if( outValueSize )	*outValueSize	= valueSize;
	if( outNext )		*outNext		= inDesc;
	
exit:
	return( err );
}

//===========================================================================================================================
//	TimeDescriptorParseComponent
//===========================================================================================================================

static OSStatus
	TimeDescriptorParseComponent( 
		const char *	inDesc, 
		const char **	outName, 
		size_t *		outNameSize, 
		const char **	outValue, 
		size_t *		outValueSize, 
		const char **	outNext )
{
	OSStatus			err;
	const char *		p;
	const char *		name;
	const char *		nameEnd;
	const char *		value;
	const char *		valueEnd;
	
	p = inDesc;
	
	// Parse a component in <name>=<value>[;] format. Skip leading white space.
	
	while( isspace( *( (const unsigned char *) p ) ) ) ++p;	
	require_action_quiet( *p != '\0', exit, err = kNotFoundErr );
	
	name = p;
	while( ( *p != '\0' ) && ( *p != '=' ) ) ++p;
	require_action( *p == '=', exit, err = kMalformedErr );
	nameEnd	= p;
	++p;
	
	value = p;
	while( ( *p != '\0' ) && ( *p != ';' ) ) ++p;
	require_action( ( *p == '\0' ) || ( *p == ';' ) , exit, err = kMalformedErr );
	valueEnd = p;
	if( *p == ';' ) ++p;
	
	if( outName )		*outName		= name;
	if( outNameSize )	*outNameSize	= (size_t)( nameEnd - name );
	if( outValue )		*outValue		= value;
	if( outValueSize )	*outValueSize	= (size_t)( valueEnd - value );
	if( outNext )		*outNext 		= p;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	ParseDate
//===========================================================================================================================

OSStatus
	ParseDate( 
		const char *	inSrc, 
		const char *	inEnd, 
		int *			outYear, 
		int *			outMonth, 
		int *			outDay, 
		const char **	outSrc )
{
	OSStatus			err;
	const char *		p;
	const char *		q;
	const char *		r;
	int					y;
	int					m;
	int					d;
	
	p = inSrc;
	q = inEnd;
	
	// Parse the year.
	
	y = 0;
	r = p;
	for( ; ( p < q ) && isdigit_safe( *p ); ++p )
	{
		y = ( y * 10 ) + ( *p - '0' );
	}
	require_action_quiet( p != r, exit, err = kMalformedErr );
	
	// Parse the month (if present).
	
	m = 0;
	if( ( p < q ) && ( *p == '-' ) )
	{
		r = ++p;
		for( ; ( p < q ) && isdigit_safe( *p ); ++p )
		{
			m = ( m * 10 ) + ( *p - '0' );
		}
		require_action_quiet( p != r, exit, err = kMalformedErr );
	}
	
	// Parse the day (if present).
	
	d = 0;
	if( ( p < q ) && ( *p == '-' ) )
	{
		r = ++p;
		for( ; ( p < q ) && isdigit_safe( *p ); ++p )
		{
			d = ( d * 10 ) + ( *p - '0' );
		}
		require_action_quiet( p != r, exit, err = kMalformedErr );
	}
	
	if( outYear )	*outYear	= y;
	if( outMonth )	*outMonth	= m;
	if( outDay )	*outDay		= d;
	if( outSrc )	*outSrc		= p;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	ParseTime
//===========================================================================================================================

OSStatus
	ParseTime( 
		const char *	inSrc, 
		const char *	inEnd, 
		int *			outHour, 
		int *			outMinute, 
		int *			outSecond, 
		const char **	outSrc )
{
	OSStatus			err;
	const char *		p;
	const char *		q;
	const char *		r;
	int					h;
	int					m;
	int					s;
	
	p = inSrc;
	q = inEnd;
	
	// Parse the hour.
	
	h = 0;
	r = p;
	for( ; ( p < q ) && isdigit_safe( *p ); ++p )
	{
		h = ( h * 10 ) + ( *p - '0' );
	}
	require_action_quiet( p != r, exit, err = kMalformedErr );
	
	// Parse the minute (if present).
	
	m = 0;
	if( ( p < q ) && ( *p == ':' ) )
	{
		r = ++p;
		for( ; ( p < q ) && isdigit_safe( *p ); ++p )
		{
			m = ( m * 10 ) + ( *p - '0' );
		}
		require_action_quiet( p != r, exit, err = kMalformedErr );
	}
	
	// Parse the second (if present).
	
	s = 0;
	if( ( p < q ) && ( *p == ':' ) )
	{
		r = ++p;
		for( ; ( p < q ) && isdigit_safe( *p ); ++p )
		{
			s = ( s * 10 ) + ( *p - '0' );
		}
		require_action_quiet( p != r, exit, err = kMalformedErr );
	}
	
	if( outHour )	*outHour	= h;
	if( outMinute )	*outMinute	= m;
	if( outSecond )	*outSecond	= s;
	if( outSrc )	*outSrc		= p;
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if( !EXCLUDE_UNIT_TESTS )

#include "TestUtils.h"

static void	NanoTimeTest( TUTestContext *inTestCtx );

//===========================================================================================================================
//	TestTimeDescriptorMatch
//===========================================================================================================================

#if( TARGET_HAS_STD_C_LIB )
OSStatus	TestTimeDescriptorMatch( const char *inDesc, const char *inDateTime, Boolean inMatch );
OSStatus	TestTimeDescriptorMatch( const char *inDesc, const char *inDateTime, Boolean inMatch )
{
	OSStatus		err;
	int				n;
	struct tm *		tp;
	struct tm		tm1;
	time_t			tt1;
	Boolean			match;
	
	// Date/Time
	
	tp = &tm1;
#if( TARGET_HAS_STD_C_LIB )
	n = sscanf( inDateTime, "%d-%d-%d %d:%d:%d", &tp->tm_year, &tp->tm_mon, &tp->tm_mday, &tp->tm_hour, &tp->tm_min, &tp->tm_sec );
#else
	n = SNScanF( inDateTime, kSizeCString, "%d-%d-%d %d:%d:%d", &tp->tm_year, &tp->tm_mon, &tp->tm_mday, &tp->tm_hour, &tp->tm_min, &tp->tm_sec );
#endif
	require_action( n == 6, exit, err = kFormatErr );
	tp->tm_year -= 1900;
	tp->tm_mon  -= 1;
	tp->tm_isdst = -1;
	tt1 = mktime( &tm1 );
	require_action( tt1 != ( (time_t) -1 ), exit, err = kUnsupportedErr );
	
	err = TimeDescriptorMatch( inDesc, &tm1, &match );
	require_noerr( err, exit );
	require_action( match == inMatch, exit, err = kResponseErr );
	
exit:
	return( err );
}
#endif

//===========================================================================================================================
//	TimeUtilsTestOld
//===========================================================================================================================

OSStatus	TimeUtilsTestOld( void )
{
	OSStatus			err;
	struct timeval		tv, tv2;
	uint64_t			mics;
	int					tmp;
	char				str[ 128 ];
	char				desc[ 64 ];
	DaySet				days;
	int					sHH;
	int					sMM;
	int					sSS;
	int					eHH;
	int					eMM;
	int					eSS;
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

#if( TARGET_OS_DARWIN )
{
	struct timespec		ts;
	
	err = clock_gettime( CLOCK_REALTIME, &ts );
	require_noerr( err, exit );
	
	err = clock_gettime( CLOCK_REALTIME, &ts );
	require_noerr( err, exit );
	
	err = clock_gettime( CLOCK_MONOTONIC, &ts );
	require_noerr( err, exit );
	
	err = clock_gettime( CLOCK_MONOTONIC, &ts );
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
	
	// ISODayOfWeek
	
	tmp = ISODayOfWeek( 2004, 1, 1 ); require_action( tmp == 4, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 1, 2 ); require_action( tmp == 5, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 1, 3 ); require_action( tmp == 6, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 1, 4 ); require_action( tmp == 7, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 1, 5 ); require_action( tmp == 1, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 1, 6 ); require_action( tmp == 2, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 1, 7 ); require_action( tmp == 3, exit, err = kResponseErr );
	
	tmp = ISODayOfWeek( 2004, 5, 1 ); require_action( tmp == 6, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 5, 2 ); require_action( tmp == 7, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 5, 3 ); require_action( tmp == 1, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 5, 4 ); require_action( tmp == 2, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 5, 5 ); require_action( tmp == 3, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 5, 6 ); require_action( tmp == 4, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 5, 7 ); require_action( tmp == 5, exit, err = kResponseErr );
	
	tmp = ISODayOfWeek( 2004, 12, 1 ); require_action( tmp == 3, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 12, 2 ); require_action( tmp == 4, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 12, 3 ); require_action( tmp == 5, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 12, 4 ); require_action( tmp == 6, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 12, 5 ); require_action( tmp == 7, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 12, 6 ); require_action( tmp == 1, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 12, 7 ); require_action( tmp == 2, exit, err = kResponseErr );
	
	tmp = ISODayOfWeek( 2004, 2, 29 ); require_action( tmp == 7, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2003, 2, 28 ); require_action( tmp == 5, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 1, 19 ); require_action( tmp == 1, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 12, 31 ); require_action( tmp == 5, exit, err = kResponseErr );
	tmp = ISODayOfWeek( 2004, 8, 12 ); require_action( tmp == 4, exit, err = kResponseErr );
	
	//
	// UTCSeconds
	//

#if( TARGET_HAS_STD_C_LIB )	
{
	uint32_t		u32;
	struct tm		tm1;
	time_t			tt1;
	
	// 0 time test
	
	tm1.tm_year		= 2001 - 1900;
	tm1.tm_mon 		= 1 - 1;
	tm1.tm_mday 	= 1;
	tm1.tm_hour 	= 0;
	tm1.tm_min		= 0;
	tm1.tm_sec		= 0;
	tm1.tm_isdst	= -1;
	tt1 = mktime( &tm1 );
	require_action( tt1 != ( (time_t) -1 ), exit, err = kUnknownErr );
	
	err = UTCSeconds( &tm1, &u32 );
	require_noerr( err, exit );
	require_action( u32 == 0, exit, err = kResponseErr );
	
	// Short time test
	
	tm1.tm_year		= 2001 - 1900;
	tm1.tm_mon 		= 1 - 1;
	tm1.tm_mday 	= 1;
	tm1.tm_hour 	= 0;
	tm1.tm_min		= 0;
	tm1.tm_sec		= 50;
	tm1.tm_isdst	= -1;
	tt1 = mktime( &tm1 );
	require_action( tt1 != ( (time_t) -1 ), exit, err = kUnknownErr );
	
	err = UTCSeconds( &tm1, &u32 );
	require_noerr( err, exit );
	require_action( u32 == 50, exit, err = kResponseErr );
	
	// Current time test
	
	err = UTCSeconds( NULL, &u32 );
	require_noerr( err, exit );
	require_action( u32 > 0, exit, err = kResponseErr );
}
#endif
	
	// SecondsToTimeComponentsString
	
	require_action( strcmp( SecondsToTimeComponentsString(      0, str ), "0s" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString(     60, str ), "1m" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString(     60, str ), "1m" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString(     61, str ), "1m 1s" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString(    123, str ), "2m 3s" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString(   3600, str ), "1h" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString(   3601, str ), "1h 1s" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString(   3660, str ), "1h 1m" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString(   3661, str ), "1h 1m 1s" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString(  86400, str ), "1d" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString(  86401, str ), "1d 1s" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString(  86460, str ), "1d 1m" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString(  86461, str ), "1d 1m 1s" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString(  90000, str ), "1d 1h" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString(  90060, str ), "1d 1h 1m" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString(  90061, str ), "1d 1h 1m 1s" ) == 0, exit, err = kResponseErr );
	require_action( strcmp( SecondsToTimeComponentsString( 930232, str ), "10d 18h 23m 52s" ) == 0, exit, err = kResponseErr );
	
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
	
	// Fractional Date/Time Strings
	
	gettimeofday( &tv, NULL );
	err = ParseFractionalDateString( MakeFractionalDateString( &tv, str, sizeof( str ) ), kSizeCString, &tv2, NULL );
	require_noerr( err, exit );
	require_action( ( tv.tv_sec == tv2.tv_sec ) && ( tv.tv_usec == tv2.tv_usec ), exit, err = -1 );
	
	err = ParseFractionalDateString( "2010-12-17T19:04:54.123Z", kSizeCString, &tv, NULL );
	require_noerr( err, exit );
	
	err = ParseFractionalDateString( "2010-12-17T19:04:54.001000Z", kSizeCString, &tv, NULL );
	require_noerr( err, exit );
	
	err = ParseFractionalDateString( "2010-12-17T19:04:54Z", kSizeCString, &tv, NULL );
	require_noerr( err, exit );
	
	err = ParseFractionalDateString( "2010-12-17T19:04Z", kSizeCString, &tv, NULL );
	require_noerr( err, exit );
	
	err = ParseFractionalDateString( "2010-12-17T19:04Z", kSizeCString, &tv, NULL );
	require_noerr( err, exit );
	
#if( TARGET_HAS_STD_C_LIB )
{
	int			utcOffset;
	
	err = UTCOffset( &utcOffset );
	require_noerr( err, exit );
}
#endif
	
	// TimeDescriptorMakeWithDaysAndTimes
	
	days = kDaySetMon | kDaySetWed | kDaySetFri;
	err = TimeDescriptorMakeWithDaysAndTimes( days, 8, 30, 5, 17, 3, 7, desc );
	require_noerr( err, exit );
	require_action( strcmp( desc, "days=m-w-f--;t=8:30:05-17:03:07" ) == 0, exit, err = kResponseErr );
	
	days = kDaySetTue | kDaySetThr | kDaySetSat | kDaySetSun;
	err = TimeDescriptorMakeWithDaysAndTimes( days, 8, 0, 0, 17, 0, 0, desc );
	require_noerr( err, exit );
	require_action( strcmp( desc, "days=-t-t-ss;t=8-17" ) == 0, exit, err = kResponseErr );
	
	// TimeDescriptorMatch
	
#if( TARGET_HAS_STD_C_LIB )
	err = TestTimeDescriptorMatch( "days=mtwtfss", "2004-06-21 08:30:00", 1 );
	require_noerr( err, exit );
	
	err = TestTimeDescriptorMatch( "days=mtwtfss", "2004-06-21 08:30:00", 1 );
	require_noerr( err, exit );
	
	err = TestTimeDescriptorMatch( "t=08:30:00-10:30:20", "2004-06-21 9:30:00", 1 );
	require_noerr( err, exit );
	
	err = TestTimeDescriptorMatch( "days=-----s-;t=8-13", "2006-06-24 08:30:00", 1 );
	require_noerr( err, exit );
	
	err = TestTimeDescriptorMatch( "days=----f--;t=8:00:00-13", "2006-06-23 08:01:00", 1 );
	require_noerr( err, exit );
	
	err = TestTimeDescriptorMatch( "days=mtwt-ss;t=8:00:00-13", "2006-06-23 08:00:00", 0 );
	require_noerr( err, exit );
#endif
	
	// TimeDescriptorGetDays
	
	err = TimeDescriptorGetDays( "days=mtwtfss", &days );
	require_noerr( err, exit );
	require_action( ( days & kDaySetAllDays ) == kDaySetAllDays, exit, err = kResponseErr );
	
	err = TimeDescriptorGetDays( "days=------s", &days );
	require_noerr( err, exit );
	require_action( days & kDaySetSun, exit, err = kResponseErr );
	
	// TimeDescriptorGetStartEndTimes
	
	err = TimeDescriptorGetStartEndTimes( "t=08:30:05-10:30:20", &sHH, &sMM, &sSS, &eHH, &eMM, &eSS );
	require_noerr( err, exit );
	require_action( sHH == 8, exit, err = kResponseErr );
	require_action( sMM == 30, exit, err = kResponseErr );
	require_action( sSS == 5, exit, err = kResponseErr );
	require_action( eHH == 10, exit, err = kResponseErr );
	require_action( eMM == 30, exit, err = kResponseErr );
	require_action( eSS == 20, exit, err = kResponseErr );
	
	err = TimeDescriptorGetStartEndTimes( "t=14:10-14:15", &sHH, &sMM, &sSS, &eHH, &eMM, &eSS );
	require_noerr( err, exit );
	require_action( sHH == 14, exit, err = kResponseErr );
	require_action( sMM == 10, exit, err = kResponseErr );
	require_action( sSS == 0, exit, err = kResponseErr );
	require_action( eHH == 14, exit, err = kResponseErr );
	require_action( eMM == 15, exit, err = kResponseErr );
	require_action( eSS == 0, exit, err = kResponseErr );
	
	err = TimeDescriptorGetStartEndTimes( "t=12-13", &sHH, &sMM, &sSS, &eHH, &eMM, &eSS );
	require_noerr( err, exit );
	require_action( sHH == 12, exit, err = kResponseErr );
	require_action( sMM == 0, exit, err = kResponseErr );
	require_action( sSS == 0, exit, err = kResponseErr );
	require_action( eHH == 13, exit, err = kResponseErr );
	require_action( eMM == 0, exit, err = kResponseErr );
	require_action( eSS == 0, exit, err = kResponseErr );
	
#if( TARGET_HAS_STD_C_LIB )
{
	Boolean		match;
	
	// TimeDescriptorDayMatch
	
	err = TimeDescriptorDayMatch( "days=mtwtfss", 1, &match );
	require_noerr( err, exit );
	require_action( match, exit, err = kResponseErr );
	
	err = TimeDescriptorDayMatch( "days=-twtfss", 1, &match );
	require_noerr( err, exit );
	require_action( !match, exit, err = kResponseErr );
	
	err = TimeDescriptorDayMatch( "", 1, &match );
	require_noerr( err, exit );
	require_action( match, exit, err = kResponseErr );
	
	// TimeDescriptorTimeMatch
	
	err = TimeDescriptorTimeMatch( "", 5, 12, 34, &match );
	require_noerr( err, exit );
	require_action( match, exit, err = kResponseErr );
	
	err = TimeDescriptorTimeMatch( "t=8-10", 9, 0, 0, &match );
	require_noerr( err, exit );
	require_action( match, exit, err = kResponseErr );
	
	err = TimeDescriptorTimeMatch( "t=8:30-10:30", 8, 25, 0, &match );
	require_noerr( err, exit );
	require_action( !match, exit, err = kResponseErr );
	
	err = TimeDescriptorTimeMatch( "t=8:30-10:30", 8, 30, 0, &match );
	require_noerr( err, exit );
	require_action( match, exit, err = kResponseErr );
	
	err = TimeDescriptorTimeMatch( "t=8:30:20-10:30", 8, 30, 15, &match );
	require_noerr( err, exit );
	require_action( !match, exit, err = kResponseErr );
	
	err = TimeDescriptorTimeMatch( "t=8:30:20-10:30", 8, 30, 20, &match );
	require_noerr( err, exit );
	require_action( match, exit, err = kResponseErr );
	
	err = TimeDescriptorTimeMatch( "t=8-10; t=12-13", 9, 0, 0, &match );
	require_noerr( err, exit );
	require_action( match, exit, err = kResponseErr );
	
	err = TimeDescriptorTimeMatch( "t=8-10; t=12-13", 12, 0, 0, &match );
	require_noerr( err, exit );
	require_action( match, exit, err = kResponseErr );
	
	err = TimeDescriptorTimeMatch( "t=8-10;t=12-13", 13, 0, 0, &match );
	require_noerr( err, exit );
	require_action( !match, exit, err = kResponseErr );
	
	err = TimeDescriptorTimeMatch( "t=8-10;t=12-13", 14, 0, 0, &match );
	require_noerr( err, exit );
	require_action( !match, exit, err = kResponseErr );
	
	err = TimeDescriptorTimeMatch( "t=08:30:00-10:30:20", 10, 30, 19, &match );
	require_noerr( err, exit );
	require_action( match, exit, err = kResponseErr );
	
	err = TimeDescriptorTimeMatch( "t=08:30:00-10:30:20", 10, 30, 20, &match );
	require_noerr( err, exit );
	require_action( !match, exit, err = kResponseErr );
	
	err = TimeDescriptorTimeMatch( "t=0-24", 0, 0, 0, &match );
	require_noerr( err, exit );
	require_action( match, exit, err = kResponseErr );
	
	err = TimeDescriptorTimeMatch( "t=0-24", 23, 59, 59, &match );
	require_noerr( err, exit );
	require_action( match, exit, err = kResponseErr );
	
	err = TimeDescriptorTimeMatch( "t=08:30:00-10:30:00; t=08:30:00-10:30:00", 8, 30, 0, &match );
	require_noerr( err, exit );
	require_action( match, exit, err = kResponseErr );
}
#endif // TARGET_HAS_STD_C_LIB
	
exit:
	printf( "TimeUtilsTestOld: %s\n", !err ? "PASSED" : "FAILED" );
	return( err );
}

//===========================================================================================================================
//	TimeUtilsTest
//===========================================================================================================================

void	TimeUtilsTest( void )
{
	TUPerformTest( NanoTimeTest );
}

//===========================================================================================================================
//	NanoTimeTest
//===========================================================================================================================

static void	NanoTimeTest( TUTestContext *inTestCtx )
{
	NanoTime64			t1, t2;
	int64_t				delta;
	struct timeval		tv, tv2;
	struct timespec		ts, ts2;
	
	t1 = NanoTimeGetCurrent();
	usleep( 100000 );
	t2 = NanoTimeGetCurrent();
	tu_require( t2 > t1, exit );
	delta = (int64_t)( t2 - t1 );
	tu_require( ( delta > 99000000 ) && ( delta < 120000000 ), exit );
	
	tv.tv_sec  = 100000;
	tv.tv_usec = 500000;
	t1 = NanoTimeFromTimeVal( &tv );
	tu_require( t1 == UINT64_C( 100000500000000 ), exit );
	
	NanoTimeToTimeVal( t1, &tv2 );
	tu_require( tv.tv_sec == tv2.tv_sec, exit );
	tu_require( tv.tv_usec == tv2.tv_usec, exit );
	
	ts.tv_sec  = 100000;
	ts.tv_nsec = 500000000;
	t1 = NanoTimeFromTimeSpec( &ts );
	tu_require( t1 == UINT64_C( 100000500000000 ), exit );
	
	NanoTimeToTimeSpec( t1, &ts2 );
	tu_require( ts.tv_sec == ts2.tv_sec, exit );
	tu_require( ts.tv_nsec == ts2.tv_nsec, exit );
	
exit:
	return;
}

#endif // !EXCLUDE_UNIT_TESTS
