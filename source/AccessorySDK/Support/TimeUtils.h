/*
	File:    	TimeUtils.h
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

#define kMacVsUnixSeconds			2082844800  // Seconds between 1904-01-01 00:00:00 UTC (Mac) and  1970-01-01 00:00:00 UTC (Unix).
#define kMacVsNTPSeconds			 126144000  // Seconds between 1900-01-01 00:00:00 UTC (NTP) and  1904-01-01 00:00:00 UTC (Mac).
#define	kMacVsAppleSingleSeconds	3029529600U // Seconds between 1904-01-01 00:00:00 UTC (Mac) and  2000-01-01 00:00:00 UTC (AppleSingle).
#define	kAFPVsUnixSeconds			 946684800	// Seconds between 1970-01-01 00:00:00 UTC (Unix) and 2000-01-01 00:00:00 UTC (AFP). 
#define	kNTPvsUnixSeconds			2208988800U // Seconds between 1900-01-01 00:00:00 UTC (NTP) and  1970-01-01 00:00:00 UTC (Unix).
#define	kNTPvsUnixSeconds_FP	  2208988800.0  // NTP vs Unix seconds as a floating-point value.

#define	MacToUnixSeconds( X )			( ( X ) - kMacVsUnixSeconds )
#define	UnixToMacSeconds( X )			( ( X ) + kMacVsUnixSeconds )

#define	MacToNTPSeconds( X )			( ( X ) + kMacVsNTPSeconds )
#define	NTPtoMacSeconds( X )			( ( X ) - kMacVsNTPSeconds )

#define	MacToAppleSingleSeconds( X )	( ( X ) - kMacVsAppleSingleSeconds )
#define	AppleSingleToMacSeconds( X )	( ( X ) + kMacVsAppleSingleSeconds )

#define	AFPToUnixSeconds( X )			( ( X ) + kAFPVsUnixSeconds )
#define	UnixToAFPSeconds( X )			( ( X ) - kAFPVsUnixSeconds )

#define	NTPtoUnixSeconds( X )			( ( X ) - kNTPvsUnixSeconds )
#define	UnixToNTPSeconds( X )			( ( X ) + kNTPvsUnixSeconds )

#define	NTPtoUnixSeconds_FP( X )		( ( X ) - kNTPvsUnixSeconds_FP )
#define	UnixToNTPSeconds_FP( X )		( ( X ) + kNTPvsUnixSeconds_FP )

#if 0
#pragma mark == NanoTime ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	NanoTime64
	@brief		Nanoseconds since an epoch of 1970-01-01 00:00:00 UTC (Unix time).
*/
typedef uint64_t		NanoTime64;
#define kNanoTime_Forever		UINT64_C( 0xFFFFFFFFFFFFFFFF )	//! Forever in the future.
#define kNanoTime_External		UINT64_C( 0xFFFFFFFFFFFFFFFE )	//! Indicates NanoTime is provided via external means.
#define kNanoTime_Invalid		UINT64_C( 0 )					//! Invalid time.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			NanoTimeFromTimeVal
	@brief		Converts a timeval to a NanoTime.
*/
STATIC_INLINE NanoTime64	NanoTimeFromTimeVal( const struct timeval *inTV )
{
	return( ( ( (uint64_t) inTV->tv_sec ) * kNanosecondsPerSecond ) + ( ( (uint32_t) inTV->tv_usec ) * kNanosecondsPerMicrosecond ) );
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			NanoTimeFromTimeSpec
	@brief		Converts a timespec to a NanoTime.
*/	
STATIC_INLINE NanoTime64	NanoTimeFromTimeSpec( const struct timespec *inTS )
{
	return( ( ( (uint64_t) inTS->tv_sec ) * kNanosecondsPerSecond ) + ( (uint32_t) inTS->tv_nsec ) );
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			NanoTimeToTimeVal
	@brief		Converts a NanoTime to a timeval.
*/
STATIC_INLINE void	NanoTimeToTimeVal( NanoTime64 inNanoTime, struct timeval *outTV )
{
#if( TARGET_OS_WINDOWS )
	outTV->tv_sec  = (long)( inNanoTime / kNanosecondsPerSecond );
	outTV->tv_usec = (long)( ( inNanoTime % kNanosecondsPerSecond ) / kNanosecondsPerMicrosecond );
#else
	outTV->tv_sec  = (time_t)( inNanoTime / kNanosecondsPerSecond );
	outTV->tv_usec = ( inNanoTime % kNanosecondsPerSecond ) / kNanosecondsPerMicrosecond;
#endif
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			NanoTimeToTimeSpec
	@brief		Converts a NanoTime to a timespec.
*/
STATIC_INLINE void	NanoTimeToTimeSpec( NanoTime64 inNanoTime, struct timespec *outTS )
{
#if( TARGET_OS_WINDOWS )
	outTS->tv_sec  = (long)( inNanoTime / kNanosecondsPerSecond );
	outTS->tv_nsec = (long)( inNanoTime % kNanosecondsPerSecond );
#else
	outTS->tv_sec  = (time_t)( inNanoTime / kNanosecondsPerSecond );
	outTS->tv_nsec = (int)( inNanoTime % kNanosecondsPerSecond );
#endif
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@fn			NanoTimeGetCurrent
	@brief		Gets the current time.
*/
NanoTime64	NanoTimeGetCurrent( void );

#if 0
#pragma mark == NTP ==
#endif

//===========================================================================================================================
//	NTP
//===========================================================================================================================

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTP32fromNTP64
	@abstract	Converts a 64-bit NTP timestamp format to a 32-bit NTP format specified by the RTP spec (middle 32 bits).
*/
#define	NTP32fromNTP64( NTP_SECS, NTP_FRAC )	\
	( ( ( ( NTP_SECS ) & 0x0000FFFF ) << 16 ) |	\
	  ( ( ( NTP_FRAC ) & 0xFFFF0000 ) >> 16 ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPFractionFromMicroseconds
	@abstract	Converts microseconds to NTP fractional seconds (1/2^32 second units).
	@discussion
	
	This relies on the factorization of 2^32 / 10^6 being 2^12 + 2^8 - 1825/2^5, which results in a maximum conversion 
	error of 3 * 10^-7 and an average error of half that.
*/
#define	NTPFractionFromMicroseconds( X )	\
	( ( ( ( (uint32_t)( X ) ) << 12 ) + ( ( (uint32_t)( X ) ) << 8 ) ) - ( ( ( (uint32_t)( X ) ) * 1825 ) >> 5 ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTP64fromRelativeTimeVal
	@abstract	Converts a timeval structure containing a relative time (not 1970's based) to NTP time.
*/
#define	NTP64fromRelativeTimeVal( TV_PTR, NTP_SECS_PTR, NTP_FRAC_PTR )	\
	do																	\
	{																	\
		uint32_t		tmp;											\
																		\
		*( NTP_SECS_PTR ) = ( TV_PTR )->tv_sec;							\
		tmp = (uint32_t)( ( TV_PTR )->tv_usec );						\
		*( NTP_FRAC_PTR ) = NTPFractionFromMicroseconds( tmp );			\
																		\
	}	while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTP64fromTimeVal
	@abstract	Converts a timeval structure containing absolute time (Unix-style 1970's based) to NTP time.
*/
#define	NTP64fromTimeVal( TV_PTR, NTP_SECS_PTR, NTP_FRAC_PTR )			\
	do																	\
	{																	\
		uint32_t		tmp;											\
																		\
		*( NTP_SECS_PTR ) = ( TV_PTR )->tv_sec + kNTPvsUnixSeconds;		\
		tmp = (uint32_t)( ( TV_PTR )->tv_usec );						\
		*( NTP_FRAC_PTR ) = NTPFractionFromMicroseconds( tmp );			\
																		\
	}	while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TimeValToNTP64
	@abstract	Converts a timeval structure containing absolute time (Unix-style 1970's based) to 64-bit NTP time.
*/
#define	TimeValToNTP64( TV_PTR )												\
	( ( ( ( (uint64_t)( (TV_PTR)->tv_sec ) ) + kNTPvsUnixSeconds ) << 32 ) |	\
	  NTPFractionFromMicroseconds( (TV_PTR)->tv_usec ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPSecondsFP
	@abstract	Converts a 64-bit NTP timestamp to a single, floating point seconds value.
*/
#define kNTPFraction							( 1.0 / 4294967296.0 )
#define	NTPSecondsFP( NTP_SECS, NTP_FRAC )		( ( (double)( NTP_SECS ) ) + ( ( (double)( NTP_FRAC ) ) * kNTPFraction ) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	NTPSecondsFP
	@abstract	Converts a 64-bit NTP timestamp to a single, floating point seconds value relative to Unix-style 1970 time.
	@discussion	Warning: Result will be negative if the time is before 1970-01-01 00:00:00.
*/
#define	NTPSeconds1970FP( NTP_SECS, NTP_FRAC )	( NTPSecondsFP( NTP_SECS, NTP_FRAC ) - kNTPvsUnixSeconds_FP )

#if 0
#pragma mark == Misc ==
#endif

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
/*!	@function	IsEvenDay / IsOddDay
	@abstract	Return true if the current day is even/odd.
*/
#define IsEvenDay()		( ( ( time( NULL ) / kSecondsPerDay ) % 2 ) == 0 )
#define IsOddDay()		( !IsEvenDay() )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@enum		ISOWeekDay
	@abstract	ISO 8601 weekday number (1 = Monday).
*/
typedef enum
{
	kISOMon = 1,
	kISOTue = 2, 
	kISOWed = 3, 
	kISOThu = 4, 
	kISOFri = 5, 
	kISOSat = 6,
	kISOSun = 7, 
	
	kISOFirstWeekDay = 1, 
	kISOLastWeekDay  = 7

}	ISOWeekDay;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@enum		ISOMonth
	@abstract	ISO 8601 month number (1 = January).
*/
typedef enum
{
	kISOJan = 1, 
	kISOFeb	= 2, 
	kISOMar = 3, 
	kISOApr = 4, 
	kISOMay = 5, 
	kISOJun = 6, 
	kISOJul = 7, 
	kISOAug = 8, 
	kISOSep = 9, 
	kISOOct = 10, 
	kISONov = 11, 
	kISODec = 12,
	
	kISOFirstMonth = 1, 
	kISOLastMonth  = 12
	
}	ISOMonth;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ANSIWeekDayToISOWeekDay
	@abstract	Maps an ANSI C weekday (0 = Sunday) to an ISO 8601 weekday (1=Monday).
	@param		DAY		ANSI C weekday number (0 = Sunday).
	@result		ISO 8601 weekday (1 = Monday).
*/
#define	ANSIWeekDayToISOWeekDay( DAY )		( ( ( ( DAY ) + 6 ) % 7 ) + 1 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	clock_gettime
	@abstract	POSIX clock_gettime. See <http://www.opengroup.org/onlinepubs/007908799/xsh/clock_gettime.html>.
*/
#if( TARGET_OS_WINDOWS )
	
	typedef int		clockid_t;
	
	#define CLOCK_REALTIME		1
	
	int	clock_gettime( clockid_t inClockID, struct timespec *outTS );
#endif

#if( TARGET_OS_DARWIN )
	#define CLOCK_REALTIME		1
	#define CLOCK_MONOTONIC		2
	
	typedef int		clockid_t;
	
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

#if( TARGET_OS_WINDOWS )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	settimeofday
	@abstract	Sets the current time in UTC.
	
	@param		inTV		New time to set.
	@param		inUnused	Unused timezone parameter. Pass NULL.
	
	@returns	Error code or 0 if successful.
*/
int	settimeofday( const struct timeval *inTV, const void *inUnused );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	ISODayOfWeek
	@abstract	Calculates the ISO 8601 weekday (1 = Monday) from a year, month, and day of month.
	
	@param		y	Year. Must be > 1752.
	@param		m	ISO Month. 1 = January.
	@param		d	ISO Day of month. 1 for the first day of the month.
	
	@result		ISO 8601 weekday (1 = Monday).
*/
int	ISODayOfWeek( int y, int m, int d );

#if( TARGET_HAS_STD_C_LIB )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	GMTtoLocalTime
	@abstract	Converts GMT/UTC time to local time.
	
	@param		inGMT					Ptr to tm filled out with GMT time. The contents are copied before any functions are called.
	@param		outLocalBrokenDownTime	Receives tm filled out with GMT as local time. May be NULL if not needed.
	@param		outLocalCalendarTime	Receives time_t for GMT as local time. May be NULL if not needed.
	
	@returns	Error code or 0 if successful.
*/
int	GMTtoLocalTime( const struct tm *inGMT, struct tm *outLocalBrokenDownTime, time_t *outLocalCalendarTime );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	UTCOffset
	@abstract	Calculates the system's current offset from UTC in seconds.
*/

OSStatus	UTCOffset( int *outUTCOffset );

#if( TARGET_HAS_STD_C_LIB )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	UTCSeconds
	@abstract	Calculates the number of seconds since 2001-01-01 UTC until the specified time.
	
	@param		inTM		Time to measure. May be NULL to use the current time.
	@param		outSeconds	Receives the number of seconds since 2001-01-01 UTC until the specified time.
*/
OSStatus	UTCSeconds( const struct tm *inTM, uint32_t *outSeconds );
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	SecondsToTimeComponentsString
	@abstract	Converts seconds into a days, hours, minutes, and seconds string. For example: 930232 -> "10d 18h 23m 52s".
	
	@param		inSeconds	Seconds value to convert.
	@param		inBuffer	Buffer to hold resulting string. Must be at least 32 bytes.
	
	@result		Ptr to beginning of string (i.e. ptr to input buffer).
*/
char *	SecondsToTimeComponentsString( unsigned int inSeconds, char *inBuffer );


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
/*!	@function	MakeFractionalDateString
	@abstract	Makes a date/time string with fractional seconds.
*/
char *		MakeFractionalDateString( const struct timeval *inTime, char *inBuffer, size_t inMaxLen );
OSStatus	ParseFractionalDateString( const char *inStr, size_t inLen, struct timeval *outTime, const char **outSrc );

#if 0
#pragma mark == TimeDescriptor ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@typedef	DaySet
	@abstract	Describes 1 or more days of a week.
*/
typedef uint32_t		DaySet;

#define	kDaySetMon		( 1U << 0 )
#define	kDaySetTue		( 1U << 1 )
#define	kDaySetWed		( 1U << 2 )
#define	kDaySetThr		( 1U << 3 )
#define	kDaySetFri		( 1U << 4 )
#define	kDaySetSat		( 1U << 5 )
#define	kDaySetSun		( 1U << 6 )

#define kDaySetNoDays		0
#define	kDaySetWeekdays		( kDaySetMon | kDaySetTue | kDaySetWed | kDaySetThr | kDaySetFri )
#define	kDaySetWeekends		( kDaySetSat | kDaySetSun )
#define	kDaySetAllDays		( kDaySetMon | kDaySetTue | kDaySetWed | kDaySetThr | kDaySetFri | kDaySetSat | kDaySetSun )

#define	DaySetContainsISOWeekDay( SET, DAY )		( ( ( SET ) & ( 1 << ( ( DAY ) - 1 ) ) ) != 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TimeDescriptorMakeWithDaysAndTimes
	@abstract	Makes a TimeDescriptor string from a DaySet and start/end times.
*/
OSStatus
	TimeDescriptorMakeWithDaysAndTimes( 
		DaySet	inDays, 
		int		inStartHour, 
		int		inStartMinute, 
		int		inStartSecond, 
		int		inEndHour, 
		int		inEndMinute, 
		int		inEndSecond, 
		char	outDesc[ 64 ] );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TimeDescriptorGetDays
	@abstract	Gets a DaySet from a TimeDescriptor string.
*/
OSStatus	TimeDescriptorGetDays( const char *inDesc, DaySet *outDays );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TimeDescriptorGetStartEndTimes
	@abstract	Gets start/end times from a TimeDescriptor string.
*/
OSStatus
	TimeDescriptorGetStartEndTimes( 
		const char *inDesc, 
		int *		outStartHour, 
		int *		outStartMinute, 
		int *		outStartSecond, 
		int *		outEndHour, 
		int *		outEndMinute, 
		int *		outEndSecond );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TimeDescriptorMatch
	@abstract	Determines if the time specified falls within the specified TimeDescriptor.
	@discussion	Note: If "inTM" is NULL, the current local time is used.
*/
#if( TARGET_HAS_STD_C_LIB )
	OSStatus	TimeDescriptorMatch( const char *inDesc, const struct tm *inTM, Boolean *outMatch );
#endif

OSStatus
	ParseDate( 
		const char *	inSrc, 
		const char *	inEnd, 
		int *			outYear, 
		int *			outMonth, 
		int *			outDay, 
		const char **	outSrc );

OSStatus
	ParseTime( 
		const char *	inSrc, 
		const char *	inEnd, 
		int *			outYear, 
		int *			outMonth, 
		int *			outDay, 
		const char **	outSrc );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	TimeUtilsTest
	@abstract	Unit test.
*/
void		TimeUtilsTest( void );
OSStatus	TimeUtilsTestOld( void );

#ifdef __cplusplus
}
#endif

#endif 	// __TimeUtils_h__
